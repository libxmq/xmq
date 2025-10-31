/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018 Vladimir Makarov <vmakarov@gcc.gnu.org>
   Copyright (c) 2024-2025 Fredrik Öhrström <oehrstroem@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files(the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* 1997-2018 Vladimir Makarov
   This file implements parsing of any context free grammar with minimal
   error recovery and syntax directed translation.  The parser is based
   on Earley's algorithm from 1968. The implementation is sufficiently
   fast to be used in serious language processors.

   2024-2025 Fredrik Öhrström
   Heavily refactored to fit ixml use case, removed global variables, restructured
   code, commented and renamed variables and structures, added ixml charset
   matching and the not operator.

   Terminology:

   Input tokens: The content to be parsed stored as an array of symbols
                 (with user supplied attributes attached that can be user fetched later).
                 The tokens can be lexer symbols or unicode characters (ixml).
                 An offset into the input tokens array is always denoted with the suffix _i.
                 E.g. input[tok_i] (current input token being scanned), from_i, to_i, state_set_i etc.
                 An offset inside the rhs of a rule is denoted with the suffix _j.

   Rule: A grammar rule: S →  NP VP

   Dotted Rule: A rule with a dot: S →  NP 🞄 VP
                The dot symbolizes how far the rule has been matched against input.
                The dot_j starts at zero which means nothing has been matched.
                A dotted rule is started if the dot_j > 0, ie it has matched something.

   Earley Item: Every input token input[tok_i] gets a state set that stores Early items (aka chart entries).
                An early item: [from_i, to_i, S →  NP 🞄 VP]
                The item maps a token range with a partial (or fully completed) dotted rule.
                Since to_i == tok_i we do not need to actually store to_i, its implicit from the state set.
                Instead we store the match_length (== to_i - from_i).

                The matched lengths are stored in a separate array and are not needed for
                parsing/recognition but are required when building the parse tree.

                Because of the separate array, there is no need not have an Earley Item structure
                in this implementation. Instead we store dotted rules and match_lengths arrays.

   StateSetCore: The part of a state set that can be shared between StateSets.
                 This is where we store the dotted rules, the dotted_rule_lenghts,
                 and the scanned terminal that created this core.
                 Again, the dotted_rule_lengths are only used to build the final parse tree
                 after at least one valid parse has been found.
                 The StateSetCores can be reused a lot.

   StateSet: For each input token, we build a state set with all possible Earley items.
             started (some match) and not-yet-started (no match yet). Theses items
             come from the scan/complete/predict algorithm.

             A started dotted_rule stores the matched length in number of tokens as matched_length.

             We compress the StateSet with an immutable StateSetCore and a separate
             array of matched_lengths corresponding to the dotted rules inside the state set core.
*/

#ifndef BUILDING_DIST_XMQ

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "yaep_allocate.h"
#include "yaep_cspc.h"
#include "yaep_hashtab.h"
#include "yaep_vlobject.h"
#include "yaep_objstack.h"
#include "yaep.h"
#include "yaep_structs.h"
#include "yaep_util.h"
#include "yaep_tree.h"
#include "yaep_print.h"
#include "yaep_symbols.h"
#include "yaep_terminal_bitset.h"
#include "xmq.h"
#include "always.h"
#include "text.h"
#include "membuffer.h"

#endif

#ifdef YAEP_MODULE

// Declarations ///////////////////////////////////////////////////

static bool blocked_by_lookahead(YaepParseState *ps, YaepDottedRule *dotted_rule, YaepSymbol *symb, int n, const char *info);
static bool core_has_not_rules(YaepStateSetCore *core);
static void check_predicted_dotted_rules(YaepParseState *ps, YaepStateSet *set, YaepVect *predictions, int lookahead_term_id, int local_lookahead_level);
static void check_leading_dotted_rules(YaepParseState *ps, YaepStateSet *set, int lookahead_term_id, int local_lookahead_level);
static bool has_lookahead(YaepParseState *ps, YaepSymbol *symb, int n);
static int default_read_token(YaepParseRun *ps, void **attr);
static void error_recovery(YaepParseState *ps, int *start, int *stop);
static void error_recovery_init(YaepParseState *ps);
static void free_error_recovery(YaepParseState *ps);
static size_t memusage(YaepParseState *ps);
static void read_input(YaepParseState *ps);
static void set_add_dotted_rule_with_matched_length(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, const char *why);
static void set_add_dotted_rule_no_match_yet(YaepParseState *ps, YaepDottedRule *dotted_rule, const char *why);
static void set_add_dotted_rule_with_parent(YaepParseState *ps, YaepDottedRule *dotted_rule, int parent_dotted_rule_id, const char *why);
static bool convert_leading_dotted_rules_into_new_set(YaepParseState *ps);
static void prepare_for_leading_dotted_rules(YaepParseState *ps);
static void verbose_stats(YaepParseState *ps);
static void yaep_error(YaepParseState *ps, int code, const char*format, ...);

// Implementations ////////////////////////////////////////////////////////////////////

/* Initialize work with rules and returns pointer to rules storage. */
static YaepRuleStorage *rulestorage_create(YaepGrammar *grammar)
{
    void *mem;
    YaepRuleStorage *result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepRuleStorage));
    result = (YaepRuleStorage*)mem;
    OS_CREATE(result->rules_os, grammar->alloc, 0);
    result->first_rule = result->current_rule = NULL;
    result->num_rules = result->n_rhs_lens = 0;

    return result;
}

/* Create new rule with LHS empty rhs. */
static YaepRule *rule_new_start(YaepParseState *ps, YaepSymbol *lhs, const char *anode, int anode_cost)
{
    YaepRule *rule;
    YaepSymbol *empty;

    assert(!lhs->is_terminal);

    OS_TOP_EXPAND(ps->run.grammar->rulestorage_ptr->rules_os, sizeof(YaepRule));
    rule =(YaepRule*) OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);
    rule->lhs = lhs;
    rule->mark = 0;
    rule->contains_not_operator = false;
    if (anode == NULL)
    {
        rule->anode = NULL;
        rule->anode_cost = 0;
    }
    else
    {
        OS_TOP_ADD_STRING(ps->run.grammar->rulestorage_ptr->rules_os, anode);
        rule->anode =(char*) OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
        OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);
        rule->anode_cost = anode_cost;
    }
    rule->trans_len = 0;
    rule->mark = 0;
    rule->marks = NULL;
    rule->order = NULL;
    rule->next = NULL;
    if (ps->run.grammar->rulestorage_ptr->current_rule != NULL)
    {
        ps->run.grammar->rulestorage_ptr->current_rule->next = rule;
    }
    rule->lhs_next = lhs->u.nonterminal.rules;
    lhs->u.nonterminal.rules = rule;
    rule->rhs_len = 0;
    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rulestorage_ptr->rules_os, &empty, sizeof(YaepSymbol*));
    rule->rhs =(YaepSymbol**) OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    ps->run.grammar->rulestorage_ptr->current_rule = rule;
    if (ps->run.grammar->rulestorage_ptr->first_rule == NULL)
    {
        ps->run.grammar->rulestorage_ptr->first_rule = rule;
    }
    rule->rule_start_offset = ps->run.grammar->rulestorage_ptr->n_rhs_lens + ps->run.grammar->rulestorage_ptr->num_rules;
    rule->num = ps->run.grammar->rulestorage_ptr->num_rules++;

    return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void rule_new_symb_add(YaepParseState *ps, YaepSymbol *symb)
{
    YaepSymbol *ignore = NULL;

    OS_TOP_ADD_MEMORY(ps->run.grammar->rulestorage_ptr->rules_os, &ignore, sizeof(YaepSymbol*));

    YaepRule *r = ps->run.grammar->rulestorage_ptr->current_rule;
    r->rhs = (YaepSymbol**)OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    r->rhs[ps->run.grammar->rulestorage_ptr->current_rule->rhs_len] = symb;
    r->rhs_len++;
    r->contains_not_operator |= symb->is_not_operator;
    ps->run.grammar->rulestorage_ptr->n_rhs_lens++;
}

/* The function should be called at end of forming each rule.  It
   creates and initializes dotted_rule cache.*/
static void rule_new_stop(YaepParseState *ps)
{
    int i;

    OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);
    OS_TOP_EXPAND(ps->run.grammar->rulestorage_ptr->rules_os, ps->run.grammar->rulestorage_ptr->current_rule->rhs_len* sizeof(int));
    ps->run.grammar->rulestorage_ptr->current_rule->order = (int*)OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);
    for(i = 0; i < ps->run.grammar->rulestorage_ptr->current_rule->rhs_len; i++)
    {
        ps->run.grammar->rulestorage_ptr->current_rule->order[i] = -1;
    }

    OS_TOP_EXPAND(ps->run.grammar->rulestorage_ptr->rules_os, ps->run.grammar->rulestorage_ptr->current_rule->rhs_len* sizeof(char));
    ps->run.grammar->rulestorage_ptr->current_rule->marks = (char*)OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    memset(ps->run.grammar->rulestorage_ptr->current_rule->marks, 0, ps->run.grammar->rulestorage_ptr->current_rule->rhs_len* sizeof(char));
    OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);

    /*
    if (ps->run.grammar->rulestorage_ptr->current_rule->contains_not_operator)
        fprintf(stderr, "NOT inside %s\n", ps->run.grammar->rulestorage_ptr->current_rule->lhs->hr);
    */
}

/* The following function frees memory for rules.*/
static void rulestorage_clear(YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_EMPTY(rules->rules_os);
    rules->first_rule = rules->current_rule = NULL;
    rules->num_rules = rules->n_rhs_lens = 0;
}

static void rulestorage_free(YaepGrammar *grammar, YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_DELETE(rules->rules_os);
    yaep_free(grammar->alloc, rules);
    rules = NULL;
}

static void create_input(YaepParseState *ps)
{
    VLO_CREATE(ps->input_vlo, ps->run.grammar->alloc, NUM_INITIAL_YAEP_TOKENS * sizeof(YaepInputToken));
    ps->input_len = 0;
}

/* Add input token with CODE and attribute at the end of input tokens array.*/
static void tok_add(YaepParseState *ps, int code, void *attr)
{
    YaepInputToken tok;

    tok.attr = attr;
    tok.symb = symb_find_by_code(ps, code);
    if (tok.symb == NULL)
    {
        yaep_error(ps, YAEP_INVALID_TOKEN_CODE, "syntax error at offset %d '%c'", ps->input_len, code);
    }
    VLO_ADD_MEMORY(ps->input_vlo, &tok, sizeof(YaepInputToken));
    ps->input = (YaepInputToken*)VLO_BEGIN(ps->input_vlo);
    ps->input_len++;
}

static void free_input(YaepParseState *ps)
{
    VLO_DELETE(ps->input_vlo);
}

static void init_dotted_rules(YaepParseState *ps)
{
    ps->num_all_dotted_rules = 0;
    OS_CREATE(ps->dotted_rules_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->dotted_rules_table_vlo, ps->run.grammar->alloc, 4096);
    ps->dotted_rules_table = (YaepDottedRule***)VLO_BEGIN(ps->dotted_rules_table_vlo);
}

/* The following function sets up lookahead of dotted_rule SIT.  The
   function returns true if the dotted_rule tail may derive empty
   string.*/
static bool dotted_rule_calculate_lookahead(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    YaepSymbol *symb, **symb_ptr;
    bool found_not = false;

    if (ps->run.grammar->lookahead_level == 0)
    {
        dotted_rule->lookahead = NULL;
    }
    else
    {
        dotted_rule->lookahead = terminal_bitset_create(ps);
        terminal_bitset_clear(ps, dotted_rule->lookahead);
    }

    if (dotted_rule->rule->lhs->is_not_operator) return false;

    // Point to the first symbol after the dot.
    symb_ptr = &dotted_rule->rule->rhs[dotted_rule->dot_j];
    while ((symb = *symb_ptr) != NULL)
    {
        if (ps->run.grammar->lookahead_level != 0)
        {
            if (symb->is_terminal)
            {
                terminal_bitset_up(ps, dotted_rule->lookahead, symb->u.terminal.term_id);
            }
            else
            {
                terminal_bitset_or(ps, dotted_rule->lookahead, symb->u.nonterminal.first);
            }
        }
        // Stop collecting lookahead if non-empty rule and its not a not-rule.
        if (!symb->empty_p && !symb->is_not_operator) break;
        if (symb->is_not_operator) found_not = true;
        symb_ptr++;
    }

    if (symb == NULL)
    {
        // We reached the end of the tail and all were potentially empty.
        if (ps->run.grammar->lookahead_level == 1)
        {
            terminal_bitset_or(ps, dotted_rule->lookahead, dotted_rule->rule->lhs->u.nonterminal.follow);
        }
        else if (ps->run.grammar->lookahead_level != 0)
        {
            terminal_bitset_or(ps, dotted_rule->lookahead, terminal_bitset_from_table(ps, dotted_rule->dyn_lookahead_context));
        }
        if (found_not) return false;
        return true;
    }
    return false;
}

/* The following function returns dotted_rules with given
   characteristics.  Remember that dotted_rules are stored in one
   exemplar. */
static YaepDottedRule *create_dotted_rule(YaepParseState *ps, YaepRule *rule, int dot_j, int dyn_lookahead_context)
{
    YaepDottedRule *dotted_rule;
    YaepDottedRule ***dyn_lookahead_context_dotted_rules_table_ptr;

    assert(dyn_lookahead_context >= 0);
    dyn_lookahead_context_dotted_rules_table_ptr = ps->dotted_rules_table + dyn_lookahead_context;

    if ((char*) dyn_lookahead_context_dotted_rules_table_ptr >= (char*) VLO_BOUND(ps->dotted_rules_table_vlo))
    {
        YaepDottedRule***bound,***ptr;
        int i, diff;

        assert((ps->run.grammar->lookahead_level <= 1 && dyn_lookahead_context == 0) || (ps->run.grammar->lookahead_level > 1 && dyn_lookahead_context >= 0));
        diff = (char*) dyn_lookahead_context_dotted_rules_table_ptr -(char*) VLO_BOUND(ps->dotted_rules_table_vlo);
        diff += sizeof(YaepDottedRule**);
        if (ps->run.grammar->lookahead_level > 1 && diff == sizeof(YaepDottedRule**))
        {
            diff *= 10;
        }
        VLO_EXPAND(ps->dotted_rules_table_vlo, diff);
        ps->dotted_rules_table =(YaepDottedRule***) VLO_BEGIN(ps->dotted_rules_table_vlo);
        bound =(YaepDottedRule***) VLO_BOUND(ps->dotted_rules_table_vlo);
        dyn_lookahead_context_dotted_rules_table_ptr = ps->dotted_rules_table + dyn_lookahead_context;
        ptr = bound - diff / sizeof(YaepDottedRule**);

        while (ptr < bound)
        {
            OS_TOP_EXPAND(ps->dotted_rules_os,
                          (ps->run.grammar->rulestorage_ptr->n_rhs_lens + ps->run.grammar->rulestorage_ptr->num_rules)
                          *sizeof(YaepDottedRule*));

            *ptr = (YaepDottedRule**)OS_TOP_BEGIN(ps->dotted_rules_os);
            OS_TOP_FINISH(ps->dotted_rules_os);

            for(i = 0; i < ps->run.grammar->rulestorage_ptr->n_rhs_lens + ps->run.grammar->rulestorage_ptr->num_rules; i++)
            {
                (*ptr)[i] = NULL;
            }
            ptr++;
        }
    }

    if ((dotted_rule = (*dyn_lookahead_context_dotted_rules_table_ptr)[rule->rule_start_offset + dot_j]) != NULL)
    {
        return dotted_rule;
    }
    OS_TOP_EXPAND(ps->dotted_rules_os, sizeof(YaepDottedRule));
    dotted_rule =(YaepDottedRule*) OS_TOP_BEGIN(ps->dotted_rules_os);
    OS_TOP_FINISH(ps->dotted_rules_os);
    ps->num_all_dotted_rules++;
    dotted_rule->rule = rule;
    dotted_rule->dot_j = dot_j;
    dotted_rule->id = ps->num_all_dotted_rules;
    dotted_rule->dyn_lookahead_context = dyn_lookahead_context;
    dotted_rule->empty_tail_p = dotted_rule_calculate_lookahead(ps, dotted_rule);

    (*dyn_lookahead_context_dotted_rules_table_ptr)[rule->rule_start_offset + dot_j] = dotted_rule;

    assert(dotted_rule->lookahead);
    return dotted_rule;
}


/* Return hash of sequence of NUM_DOTTED_RULES dotted_rules in array DOTTED_RULES. */
static unsigned dotted_rules_hash(int num_dotted_rules, YaepDottedRule **dotted_rules)
{
    int n, i;
    unsigned result;

    result = jauquet_prime_mod32;
    for(i = 0; i < num_dotted_rules; i++)
    {
        n = dotted_rules[i]->id;
        result = result* hash_shift + n;
    }
    return result;
}

static void free_dotted_rules(YaepParseState *ps)
{
    VLO_DELETE(ps->dotted_rules_table_vlo);
    OS_DELETE(ps->dotted_rules_os);
}

static unsigned stateset_core_hash(YaepStateSet* s)
{
    return s->core->hash;
}

static bool stateset_core_eq(YaepStateSet *s1, YaepStateSet *s2)
{
    YaepDottedRule **dotted_rule_ptr1, **dotted_rule_ptr2, **dotted_rule_bound1;
    YaepStateSetCore*set_core1 = s1->core;
    YaepStateSetCore*set_core2 = s2->core;

    if (set_core1->num_started_dotted_rules != set_core2->num_started_dotted_rules)
    {
        return false;
    }
    dotted_rule_ptr1 = set_core1->dotted_rules;
    dotted_rule_bound1 = dotted_rule_ptr1 + set_core1->num_started_dotted_rules;
    dotted_rule_ptr2 = set_core2->dotted_rules;
    while(dotted_rule_ptr1 < dotted_rule_bound1)
    {
        if (*dotted_rule_ptr1++ !=*dotted_rule_ptr2++)
        {
            return false;
        }
    }
    return true;
}

static unsigned matched_lengths_hash(YaepStateSet *s)
{
    return s->matched_lengths_hash;
}

/* Compare all the matched_lengths stored in the two state sets. */
static bool matched_lengths_eq(YaepStateSet *s1, YaepStateSet *s2)
{
    int *i = s1->matched_lengths;
    int *j = s2->matched_lengths;
    int num_matched_lengths = s1->core->num_started_dotted_rules;

    if (num_matched_lengths != s2->core->num_started_dotted_rules)
    {
        return false;
    }

    int *bound = i + num_matched_lengths;
    while (i < bound)
    {
        if (*i++ != *j++)
        {
            return false;
        }
    }
    return true;
}

static unsigned stateset_core_matched_lengths_hash(YaepStateSet *s)
{
    return stateset_core_hash(s) * hash_shift + matched_lengths_hash(s);
}

static bool stateset_core_matched_lengths_eq(YaepStateSet *s1, YaepStateSet *s2)
{
    YaepStateSetCore *set_core1 = s1->core;
    YaepStateSetCore *set_core2 = s2->core;
    int*matched_lengths1 = s1->matched_lengths;
    int*matched_lengths2 = s2->matched_lengths;

    return set_core1 == set_core2 && matched_lengths1 == matched_lengths2;
}

static unsigned stateset_term_lookahead_hash(YaepStateSetTermLookAhead *s)
{
    YaepStateSet *set = s->set;
    YaepSymbol *term = s->term;
    int lookahead_term = s->lookahead_term;

    return ((stateset_core_matched_lengths_hash(set)* hash_shift + term->u.terminal.term_id)* hash_shift + lookahead_term);
}

static bool stateset_term_lookahead_eq(YaepStateSetTermLookAhead *s1, YaepStateSetTermLookAhead *s2)
{
    YaepStateSet *set1 = s1->set;
    YaepStateSet *set2 = s2->set;
    YaepSymbol *term1 = s1->term;
    YaepSymbol *term2 = s2->term;
    int lookahead1 = s1->lookahead_term;
    int lookahead2 = s2->lookahead_term;

    return set1 == set2 && term1 == term2 && lookahead1 == lookahead2;
}

/* Initiate the set of pairs(sit, dist). */
static void dotted_rule_matched_length_set_init(YaepParseState *ps)
{
    VLO_CREATE(ps->dotted_rule_matched_length_vec_vlo, ps->run.grammar->alloc, 8192);
    ps->dotted_rule_matched_length_vec_generation = 0;
}

/* The clear the set we only need to increment the generation.
   The test for set membership compares with the active generation.
   Thus all previously stored memberships are immediatly invalidated
   through the increment below. Thus clearing the set! */
static void clear_dotted_rule_matched_length_set(YaepParseState *ps)
{
    ps->dotted_rule_matched_length_vec_generation++;
}

/* Insert pair(DOTTED_RULE, DIST) into the ps->dotted_rule_matched_length_vec_vlo.
   Each dotted_rule has a unique id incrementally counted from 0 to the most recent dotted_rule added.
   This id is used as in index into the vector, the vector storing vlo objects.
   Each vlo object maintains a memory region used for an integer array of matched_lengths.

   If such pair exists return true (was false), otherwise return false. (was true). */
static bool dotted_rule_matched_length_test_and_set(YaepParseState *ps, YaepDottedRule *dotted_rule, int dist)
{
    int i, len, id;
    vlo_t *dist_vlo;

    id = dotted_rule->id;

    // Expand the vector to accommodate a new dotted_rule.
    len = VLO_LENGTH(ps->dotted_rule_matched_length_vec_vlo)/sizeof(vlo_t);
    if (len <= id)
    {
        VLO_EXPAND(ps->dotted_rule_matched_length_vec_vlo,(id + 1 - len)* sizeof(vlo_t));
        for(i = len; i <= id; i++)
        {
            // For each new slot in the vector, initialize a new vlo, to be used for matched_lengths.
            VLO_CREATE(((vlo_t*) VLO_BEGIN(ps->dotted_rule_matched_length_vec_vlo))[i], ps->run.grammar->alloc, 64);
        }
    }

    // Now fetch the vlo for this id, which is either an existing vlo or a freshly initialized vlo.
    // The vlo stores an array of integersCheck if the vlo is big enough for this matched_length?
    dist_vlo = &((vlo_t*)VLO_BEGIN(ps->dotted_rule_matched_length_vec_vlo))[id];
    len = VLO_LENGTH(*dist_vlo) / sizeof(int);
    if (len <= dist)
    {
        VLO_EXPAND(*dist_vlo,(dist + 1 - len)* sizeof(int));
        for(i = len; i <= dist; i++)
        {
           ((int*) VLO_BEGIN(*dist_vlo))[i] = 0;
        }
    }
    int *generation = (int*)VLO_BEGIN(*dist_vlo) + dist;
    if (*generation == ps->dotted_rule_matched_length_vec_generation)
    {
        // The pair was already inserted! We know this since we found the current generation in this slot.
        // Remember that we clear the set by incrementing the current generation.
        return true;
    }
    // The pair did not exist in the set. (Since the generation number did not match.)
    // Insert this pair my marking the vec[id][dist] with the current generation.
    *generation = ps->dotted_rule_matched_length_vec_generation;
    return false;
}

static void free_dotted_rule_matched_length_sets(YaepParseState *ps)
{
    int i, len = VLO_LENGTH(ps->dotted_rule_matched_length_vec_vlo) / sizeof(vlo_t);

    for(i = 0; i < len; i++)
    {
        VLO_DELETE(((vlo_t*) VLO_BEGIN(ps->dotted_rule_matched_length_vec_vlo))[i]);
    }
    VLO_DELETE(ps->dotted_rule_matched_length_vec_vlo);
}

/* Initialize work with sets for parsing input with N_INPUT tokens.*/
static void set_init(YaepParseState *ps, int n_input)
{
    int n = n_input >> 3;

    OS_CREATE(ps->set_cores_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->set_dotted_rules_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_parent_dotted_rule_ids_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_matched_lengths_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->sets_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->set_term_lookahead_os, ps->run.grammar->alloc, 0);

    ps->cache_stateset_cores = create_hash_table(ps->run.grammar->alloc, 2000,
                                                 (hash_table_hash_function)stateset_core_hash,
                                                 (hash_table_eq_function)stateset_core_eq);

    ps->cache_stateset_matched_lengths = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                                           (hash_table_hash_function)matched_lengths_hash,
                                                           (hash_table_eq_function)matched_lengths_eq);

    ps->cache_stateset_core_matched_lengths = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                                                (hash_table_hash_function)stateset_core_matched_lengths_hash,
                                                                (hash_table_eq_function)stateset_core_matched_lengths_eq);

    ps->cache_stateset_term_lookahead = create_hash_table(ps->run.grammar->alloc, n < 30000 ? 30000 : n,
                                                          (hash_table_hash_function)stateset_term_lookahead_hash,
                                                          (hash_table_eq_function)stateset_term_lookahead_eq);

    ps->num_set_cores = ps->num_set_core_start_dotted_rules= 0;
    ps->num_set_matched_lengths = ps->num_set_matched_lengths_len = ps->num_parent_dotted_rule_ids = 0;
    ps->num_sets_total = ps->num_dotted_rules_total= 0;
    ps->num_set_term_lookahead = 0;
    dotted_rule_matched_length_set_init(ps);
}

static void debug_step(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, int parent_id)
{
    if (!ps->run.debug) return;

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "@%d ", ps->tok_i);

    print_dotted_rule(mb, ps, ps->tok_i, dotted_rule, matched_length, parent_id);

    debug_mb("ixml.pa.step=", mb);
    free_membuffer_and_free_content(mb);

}

static void debug_step_blocked(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, int parent_id)
{
    if (!ps->run.debug) return;

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "@%d ", ps->tok_i);

    print_dotted_rule_blocked(mb, ps, ps->tok_i, dotted_rule, matched_length, parent_id);

    debug_mb("ixml.pa.step=", mb);
    free_membuffer_and_free_content(mb);
}

static void append_dotted_rule_no_core_yet(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    assert(!ps->new_core);

    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_dotted_rules = (YaepDottedRule**) OS_TOP_BEGIN(ps->set_dotted_rules_os);
    ps->new_dotted_rules[ps->new_num_leading_dotted_rules] = dotted_rule;
}

static void append_dotted_rule_to_core(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    assert(ps->new_core);

    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_core->dotted_rules = (YaepDottedRule**)OS_TOP_BEGIN(ps->set_dotted_rules_os);
    ps->new_core->dotted_rules[ps->new_core->num_dotted_rules++] = dotted_rule;

    // cache ptr
    ps->new_dotted_rules = ps->new_core->dotted_rules;
}

static void append_matched_length_no_core_yet(YaepParseState *ps, int matched_length)
{
    assert(!ps->new_core);

    OS_TOP_EXPAND(ps->set_matched_lengths_os, sizeof(int));
    ps->new_matched_lengths = (int*)OS_TOP_BEGIN(ps->set_matched_lengths_os);
    ps->new_matched_lengths[ps->new_num_leading_dotted_rules] = matched_length;
}

/*
  start SIT = the leading dotted rules added first in a cycle.
     no core yet, no set yet.
     duplicates are not tested, do not add those!
     set_add_dotted_rule_with_matched_length

  non-start initial = new dotted rule with zero matched length (no parent)
     core yes, set yes
     duplicates are ignored
     set_add_dotted_rule_no_match_yet

  non-start non-initial = new dotted rule with parent pointer.
     core yes, set yes
     duplicates are ignored
     set_add_dotted_rule_with_parent

 */

/* Add start SIT with distance DIST at the end of the situation array
   of the set being formed.
   set_new_add_start_sit (struct sit *sit, int dist)
*/
static void set_add_dotted_rule_with_matched_length(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, const char *why)
{
    assert(!ps->new_set_ready_p);
    assert(!ps->new_set);
    assert(!ps->new_core);

    append_dotted_rule_no_core_yet(ps, dotted_rule);
    append_matched_length_no_core_yet(ps, matched_length);

    ps->new_num_leading_dotted_rules++;

    yaep_trace(ps, "%s add leading d%d len %d", why, dotted_rule->id, matched_length);
    debug_step(ps, dotted_rule, matched_length, -1);
}

/* Add non-start (initial) SIT with zero distance at the end of the
   situation array of the set being formed.  If this is non-start
   situation and there is already the same pair (situation, zero
   distance), we do not add it.
   set_new_add_initial_sit (struct sit *sit)
*/
static void set_add_dotted_rule_no_match_yet(YaepParseState *ps, YaepDottedRule *dotted_rule, const char *why)
{
    assert(ps->new_set_ready_p);
    assert(ps->new_set);
    assert(ps->new_core);

    /* When we add not-yet-started dotted_rules we need to have pairs
       (dotted_rule, the corresponding matched_length) without duplicates
       because we also form core_symb_to_predcomps at that time. */
    for (int i = ps->new_num_leading_dotted_rules; i < ps->new_core->num_dotted_rules; i++)
    {
        // Check if already added.
        if (ps->new_dotted_rules[i] == dotted_rule) {
            //yaep_trace(ps, "skip d%", dotted_rule->id);
            return;
        }
    }
    /* Remember we do not store matched_length for not-yet-started dotted_rules. */
    append_dotted_rule_to_core(ps, dotted_rule);

    yaep_trace(ps, "%s add d%d to c%d", why, dotted_rule->id, ps->new_core->id);
    debug_step(ps, dotted_rule, 0, -1);
}

/* Add nonstart, noninitial SIT with distance DIST at the end of the
   situation array of the set being formed.  If this is situation and
   there is already the same pair (situation, the corresponding
   distance), we do not add it.
   set_add_new_nonstart_sit (struct sit *sit, int parent)
*/
static void set_add_dotted_rule_with_parent(YaepParseState *ps,
                                            YaepDottedRule *dotted_rule,
                                            int parent_rule_index,
                                            const char *why)
{
    assert(ps->new_set_ready_p);
    assert(ps->new_set);
    assert(ps->new_core);

    /* When we add predicted dotted_rules we need to have pairs
       (dotted_rule + parent_dotted_rule_id) without duplicates
       because we also form core_symb_to_predcomps at that time. */
    for (int rule_index_in_core = ps->new_num_leading_dotted_rules;
         rule_index_in_core < ps->new_core->num_dotted_rules;
         rule_index_in_core++)
    {
        if (ps->new_dotted_rules[rule_index_in_core] == dotted_rule &&
            ps->new_core->to_parent_rule_index[rule_index_in_core] == parent_rule_index)
        {
            // The dotted_rule + parent dotted rule already exists.
            yaep_trace(ps, "reusing d%d with parent rule index %d", dotted_rule->id, parent_rule_index);
            return;
        }
    }

    // Increase the object stack storing dotted_rules, with the size of a new dotted_rule.
    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_dotted_rules = ps->new_core->dotted_rules = (YaepDottedRule**)OS_TOP_BEGIN(ps->set_dotted_rules_os);

    // Increase the parent index vector with another int.
    // This integer points to ...?
    OS_TOP_EXPAND(ps->set_parent_dotted_rule_ids_os, sizeof(int));
    ps->new_core->to_parent_rule_index = (int*)OS_TOP_BEGIN(ps->set_parent_dotted_rule_ids_os) - ps->new_num_leading_dotted_rules;

    // Store dotted_rule into new dotted_rules.
    ps->new_dotted_rules[ps->new_core->num_dotted_rules++] = dotted_rule;
    // Store parent index. Meanst what...?
    ps->new_core->to_parent_rule_index[ps->new_core->num_all_matched_lengths++] = parent_rule_index;
    ps->num_parent_dotted_rule_ids++;

    int matched_length = ps->new_set->matched_lengths[parent_rule_index];

    yaep_trace(ps, "%s add d%d with parent index %d to c%d", why, dotted_rule->id, parent_rule_index, ps->new_core->id);
    debug_step(ps, dotted_rule, matched_length, parent_rule_index);
}

/* Set up hash of matched_lengths of set S. */
static void setup_set_matched_lengths_hash(hash_table_entry_t s)
{
    YaepStateSet *set = (YaepStateSet*)s;

    int num_matched_lengths = set->core->num_started_dotted_rules;
    unsigned result = jauquet_prime_mod32;

    int *i = set->matched_lengths;
    if (num_matched_lengths == 0 || i == NULL)
    {
        set->matched_lengths_hash = 0;
        return;
    }

    int *stop = i + num_matched_lengths;

    while (i < stop)
    {
        result = result*hash_shift + *i++;
    }
    set->matched_lengths_hash = result;
}

/* Set up hash of core of set S. */
static void setup_stateset_core_hash(YaepStateSet *s)
{
    s->core->hash = dotted_rules_hash(s->core->num_started_dotted_rules, s->core->dotted_rules);
}

static void prepare_for_leading_dotted_rules(YaepParseState *ps)
{
    ps->new_set = NULL;
    ps->new_core = NULL;
    ps->new_set_ready_p = false;
    ps->new_dotted_rules = NULL;
    ps->new_matched_lengths = NULL;
    ps->new_num_leading_dotted_rules = 0;

    yaep_trace(ps, "start collecting leading rules");
}

/* The new set should contain only start dotted_rules.
   Sort dotted_rules, remove duplicates and insert set into the set table.
   Returns true if a new core was allocated. False if an old core was reused. */
static bool convert_leading_dotted_rules_into_new_set(YaepParseState *ps)
{
    bool added;

    assert(!ps->new_set_ready_p);

    OS_TOP_EXPAND(ps->sets_os, sizeof(YaepStateSet));
    ps->new_set = (YaepStateSet*)OS_TOP_BEGIN(ps->sets_os);
    ps->new_set->matched_lengths = ps->new_matched_lengths;
    ps->new_set->id = ps->num_sets_total;

    yaep_trace(ps, "convert leading rules into s%d", ps->new_set->id);

    OS_TOP_EXPAND(ps->set_cores_os, sizeof(YaepStateSetCore));
    ps->new_set->core = ps->new_core = (YaepStateSetCore*) OS_TOP_BEGIN(ps->set_cores_os);
    ps->new_core->num_started_dotted_rules = ps->new_num_leading_dotted_rules;
    ps->new_core->dotted_rules = ps->new_dotted_rules;

#ifdef USE_SET_HASH_TABLE
    // Lookup matched_lengths from cache table.
    setup_set_matched_lengths_hash(ps->new_set);
    YaepStateSet **sm = (YaepStateSet**)find_hash_table_entry(ps->cache_stateset_matched_lengths, ps->new_set, true);
    if (*sm != NULL)
    {
        // The matched lengths already existed use the cache.
        ps->new_matched_lengths = ps->new_set->matched_lengths = (*sm)->matched_lengths;
        OS_TOP_NULLIFY(ps->set_matched_lengths_os);

        if (xmq_trace_enabled_)
        {
            MemBuffer *mb = new_membuffer();
            print_matched_lenghts(mb, ps->new_set);
            membuffer_append_null(mb);
            yaep_trace(ps, "re-using matched lengths %s", mb->buffer_);
            free_membuffer_and_free_content(mb);
        }
    }
    else
    {
        // This is a new set of matched lengths.
        OS_TOP_FINISH(ps->set_matched_lengths_os);
        *sm = ps->new_set;
        ps->num_set_matched_lengths++;
        ps->num_set_matched_lengths_len += ps->new_num_leading_dotted_rules;

        if (xmq_trace_enabled_)
        {
            MemBuffer *mb = new_membuffer();
            print_matched_lenghts(mb, ps->new_set);
            membuffer_append_null(mb);
            yaep_trace(ps, "new matched lengths (%s)", mb->buffer_);
            free_membuffer_and_free_content(mb);
        }
    }
#else
    OS_TOP_FINISH(ps->set_matched_lengths_os);
    ps->num_set_matched_lengths++;
    ps->num_set_matched_lengths_len += ps->new_num_leading_dotted_rules;
#endif

    /* Insert set core into table.*/
    setup_stateset_core_hash(ps->new_set);
    /* We look for a core with an identical list of started dotted rules (pointer equivalence). */
    YaepStateSet **sc = (YaepStateSet**)find_hash_table_entry(ps->cache_stateset_cores, ps->new_set, true);
    bool reuse_core = *sc != NULL;
    if (reuse_core)
    {
        // We can potentially re-use this core, but lets check if there are not-operators in any of the dotted rules.
        // If so, then we cannot re-use the core. Later on we can improve this by checking if the not rules
        // apply to this position as well.
        if (core_has_not_rules((*sc)->core)) reuse_core = false;
    }
    if (reuse_core)
    {
        // The core already existed, drop the core allocation.
        // Point to the old core instead.
        OS_TOP_NULLIFY(ps->set_cores_os);
        ps->new_set->core = ps->new_core = (*sc)->core;
        ps->new_dotted_rules = ps->new_core->dotted_rules;

        OS_TOP_NULLIFY(ps->set_dotted_rules_os);
        added = false;

        if (xmq_trace_enabled_)
        {
            MemBuffer *mb = new_membuffer();
            print_core(mb, (*sc)->core);
            membuffer_append_null(mb);
            yaep_trace(ps, "re-using %s", mb->buffer_);
            free_membuffer_and_free_content(mb);
        }
    }
    else
    {
        OS_TOP_FINISH(ps->set_cores_os);
        ps->new_core->id = ps->num_set_cores++;
        ps->new_core->num_dotted_rules = ps->new_num_leading_dotted_rules;
        ps->new_core->num_all_matched_lengths = ps->new_num_leading_dotted_rules;
        ps->new_core->to_parent_rule_index = NULL;
        *sc = ps->new_set;
        ps->num_set_core_start_dotted_rules += ps->new_num_leading_dotted_rules;
        added = true;

        if (xmq_trace_enabled_)
        {
            MemBuffer *mb = new_membuffer();
            print_core(mb, ps->new_set->core);
            membuffer_append_null(mb);
            yaep_trace(ps, "new %s", mb->buffer_);
            free_membuffer_and_free_content(mb);
        }
    }

#ifdef USE_SET_HASH_TABLE
    /* Insert set into table.*/
    YaepStateSet **scm = (YaepStateSet**)find_hash_table_entry(ps->cache_stateset_core_matched_lengths, ps->new_set, true);
    if (*scm == NULL)
    {
       *scm = ps->new_set;
        ps->num_sets_total++;
        ps->num_dotted_rules_total += ps->new_num_leading_dotted_rules;
        OS_TOP_FINISH(ps->sets_os);
        yaep_trace(ps, "new s%d", ps->new_set->id);
    }
    else
    {
        ps->new_set = *scm;
        OS_TOP_NULLIFY(ps->sets_os);
        yaep_trace(ps, "re-using s%d", ps->new_set->id);
    }
#else
    OS_TOP_FINISH(ps->sets_os);
#endif

    ps->new_set_ready_p = true;
    return added;
}

/* The following function finishes work with set being formed.*/
static void set_new_core_stop(YaepParseState *ps)
{
    OS_TOP_FINISH(ps->set_dotted_rules_os);
    OS_TOP_FINISH(ps->set_parent_dotted_rule_ids_os);
}

static void free_sets(YaepParseState *ps)
{
    free_dotted_rule_matched_length_sets(ps);
    delete_hash_table(ps->cache_stateset_term_lookahead);
    delete_hash_table(ps->cache_stateset_core_matched_lengths);
    delete_hash_table(ps->cache_stateset_matched_lengths);
    delete_hash_table(ps->cache_stateset_cores);
    OS_DELETE(ps->set_term_lookahead_os);
    OS_DELETE(ps->sets_os);
    OS_DELETE(ps->set_parent_dotted_rule_ids_os);
    OS_DELETE(ps->set_dotted_rules_os);
    OS_DELETE(ps->set_matched_lengths_os);
    OS_DELETE(ps->set_cores_os);
}

/* Initialize work with the parser list.*/
static void pl_init(YaepParseState *ps)
{
    ps->state_sets = NULL;
}

/* The following function creates Earley's parser list.*/
static void allocate_state_sets(YaepParseState *ps)
{
    /* Because of error recovery we may have sets 2 times more than tokens.*/
    void *mem = yaep_malloc(ps->run.grammar->alloc, sizeof(YaepStateSet*)*(ps->input_len + 1)* 2);
    ps->state_sets = (YaepStateSet**)mem;
    ps->state_set_k = -1;
}

static void free_state_sets(YaepParseState *ps)
{
    if (ps->state_sets != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->state_sets);
        ps->state_sets = NULL;
    }
}

static void verbose_stats(YaepParseState *ps)
{
    size_t size = memusage(ps);
    char *siz = humanReadableTwoDecimals(size);
    verbose("ixml=", "@%d/%d #sets=%d #cores=%d #dotted_rules=%d #matched_lengths=%d mem=%s",
            ps->tok_i,
            ps->input_len,
            ps->num_sets_total,
            ps->num_set_cores,
            ps->num_dotted_rules_total,
            ps->num_set_matched_lengths,
            siz
        );
    free(siz);
}
/* The following function stores error CODE and MESSAGE.  The function
   makes long jump after that.  So the function is designed to process
   only one error.*/
static void yaep_error(YaepParseState *ps, int code, const char*format, ...)
{
    va_list arguments;

    ps->run.grammar->error_code = code;
    va_start(arguments, format);
    vsprintf(ps->run.grammar->error_message, format, arguments);
    va_end(arguments);
    assert(strlen(ps->run.grammar->error_message) < YAEP_MAX_ERROR_MESSAGE_LENGTH);
    longjmp(ps->error_longjump_buff, code);
}

/* The following function processes allocation errors. */
static void error_func_for_allocate(void *ps)
{
   yaep_error((YaepParseState*)ps, YAEP_NO_MEMORY, "no memory");
}

YaepGrammar *yaepNewGrammar()
{
    YaepAllocator *allocator;

    allocator = yaep_alloc_new(NULL, NULL, NULL, NULL);
    if (allocator == NULL)
    {
        return NULL;
    }
    YaepGrammar *grammar = (YaepGrammar*)yaep_malloc(allocator, sizeof(*grammar));

    if (grammar == NULL)
    {
        yaep_alloc_del(allocator);
        return NULL;
    }
    grammar->alloc = allocator;
    yaep_alloc_seterr(allocator, error_func_for_allocate,
                      yaep_alloc_getuserptr(allocator));

    grammar->undefined_p = true;
    grammar->error_code = 0;
   *grammar->error_message = '\0';
    grammar->lookahead_level = 1;
    grammar->one_parse_p = true;
    grammar->cost_p = false;
    grammar->error_recovery_p = false;
    grammar->recovery_token_matches = DEFAULT_RECOVERY_TOKEN_MATCHES;
    grammar->symbs_ptr = symbolstorage_create(grammar);
    grammar->term_sets_ptr = termsetstorage_create(grammar);
    grammar->rulestorage_ptr = rulestorage_create(grammar);
    return grammar;
}

YaepParseRun *yaepNewParseRun(YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)calloc(1, sizeof(YaepParseState));
    INSTALL_PARSE_STATE_MAGIC(ps);

    ps->run.grammar = g;

    return (YaepParseRun*)ps;
}

void yaepFreeParseRun(YaepParseRun *pr)
{
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));
    free(ps);
}

void yaepSetUserData(YaepGrammar *g, void *data)
{
    g->user_data = data;
}

void *yaepGetUserData(YaepGrammar *g)
{
    return g->user_data;
}

/* The following function makes grammar empty.*/
static void yaep_empty_grammar(YaepParseState *ps,YaepGrammar *grammar)
{
    if (grammar != NULL)
    {
        rulestorage_clear(grammar->rulestorage_ptr);
        terminal_bitset_empty(grammar->term_sets_ptr);
        symb_empty(ps, grammar->symbs_ptr);
    }
}

/* The function returns the last occurred error code for given grammar. */
int
yaep_error_code(YaepGrammar*g)
{
    assert(g != NULL);
    return g->error_code;
}

/* The function returns message are always contains error message
   corresponding to the last occurred error code.*/
const char*
yaep_error_message(YaepGrammar*g)
{
    assert(g != NULL);
    return g->error_message;
}

/* The following function creates sets FIRST and FOLLOW for all
   grammar nonterminals.*/
static void create_first_follow_sets(YaepParseState *ps)
{
    YaepSymbol *symb, **rhs, *rhs_symb, *next_rhs_symb;
    YaepRule *rule;
    int i, j, k, rhs_len;
    bool changed_p, first_continue_p;

    for (i = 0; (symb = nonterm_get(ps, i)) != NULL; i++)
    {
        symb->u.nonterminal.first = terminal_bitset_create(ps);
        terminal_bitset_clear(ps, symb->u.nonterminal.first);

        symb->u.nonterminal.follow = terminal_bitset_create(ps);
        terminal_bitset_clear(ps, symb->u.nonterminal.follow);
    }

    do
    {
        changed_p = false;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
        {
            for (rule = symb->u.nonterminal.rules;
                 rule != NULL;
                 rule = rule->lhs_next)
            {
                first_continue_p = true;
                rhs = rule->rhs;
                rhs_len = rule->rhs_len;
                for(j = 0; j < rhs_len; j++)
                {
                    rhs_symb = rhs[j];
                    if (rhs_symb->is_terminal)
                    {
                        if (first_continue_p)
                        {
                            changed_p |= terminal_bitset_up(ps, symb->u.nonterminal.first,
                                                            rhs_symb->u.terminal.term_id);
                        }
                    }
                    else
                    {
                        if (first_continue_p)
                        {
                            changed_p |= terminal_bitset_or(ps,
                                                            symb->u.nonterminal.first,
                                                            rhs_symb->u.nonterminal.first);
                        }
                        for(k = j + 1; k < rhs_len; k++)
                        {
                            next_rhs_symb = rhs[k];
                            if (next_rhs_symb->is_terminal)
                            {
                                changed_p
                                    |= terminal_bitset_up(ps, rhs_symb->u.nonterminal.follow,
                                                   next_rhs_symb->u.terminal.term_id);
                            }
                            else
                            {
                                changed_p
                                    |= terminal_bitset_or(ps, rhs_symb->u.nonterminal.follow,
                                                   next_rhs_symb->u.nonterminal.first);
                            }
                            if (!next_rhs_symb->empty_p && !next_rhs_symb->is_not_operator)
                            {
                                break;
                            }
                        }
                        if (k == rhs_len)
                        {
                            changed_p |= terminal_bitset_or(ps, rhs_symb->u.nonterminal.follow,
                                                     symb->u.nonterminal.follow);
                        }
                    }
                    if (!rhs_symb->empty_p && !rhs_symb->is_not_operator)
                    {
                        first_continue_p = false;
                    }
                }
            }
        }
    }
    while (changed_p);
}

/* The following function sets up flags empty_p, access_p and
   derivation_p for all grammar symbols.*/
static void set_empty_access_derives(YaepParseState *ps)
{
    bool empty_changed_p, derivation_changed_p, accessibility_change_p;

    for (int i = 0; ; i++)
    {
        YaepSymbol *symb = symb_get(ps, i);
        if (!symb) break;
        symb->empty_p = false;
        symb->derivation_p = (symb->is_terminal ? true : false);
        symb->access_p = false;
    }

    ps->run.grammar->axiom->access_p = 1;
    do
    {
        empty_changed_p = derivation_changed_p = accessibility_change_p = false;
        for (int i = 0; ; i++)
        {
            YaepSymbol *symb = nonterm_get(ps, i);
            if (!symb) break;

            for (YaepRule *rule = symb->u.nonterminal.rules; rule != NULL; rule = rule->lhs_next)
            {
                bool empty_p = true;
                bool derivation_p = true;

                if (rule->lhs->is_not_operator)
                {
                    empty_p = 0;
                }

                for (int j = 0; j < rule->rhs_len; j++)
                {
                    YaepSymbol *rhs_symb = rule->rhs[j];
                    if (symb->access_p)
                    {
                        accessibility_change_p |= rhs_symb->access_p ^ 1;
                        rhs_symb->access_p = 1;
                    }
                    // A not rule forbids emptiness since it must always be checked against the input.
                    if (rhs_symb->is_not_operator)
                    {
                        empty_p = 0;
                    }
                    empty_p &= rhs_symb->empty_p;
                    derivation_p &= rhs_symb->derivation_p;
                }
                if (empty_p)
                {
                    empty_changed_p |= symb->empty_p ^ empty_p;
                    symb->empty_p = empty_p;
                }
                if (derivation_p)
                {
                    derivation_changed_p |= symb->derivation_p ^ derivation_p;
                    symb->derivation_p = derivation_p;
                }
            }
        }
    }
    while(empty_changed_p || derivation_changed_p || accessibility_change_p);

}

/* The following function sets up flags loop_p for nonterminals. */
static void set_loop_p(YaepParseState *ps)
{
    YaepSymbol*symb,*lhs;
    YaepRule*rule;
    int i, j, k;
    bool loop_p, changed_p;
    /* Initialize accoding to minimal criteria: There is a rule in which
       the nonterminal stands and all the rest symbols can derive empty
       strings.*/
    for(rule = ps->run.grammar->rulestorage_ptr->first_rule; rule != NULL; rule = rule->next)
        for(i = 0; i < rule->rhs_len; i++)
            if (!(symb = rule->rhs[i])->is_terminal)
            {
                for(j = 0; j < rule->rhs_len; j++)
                    if (i == j)
                        continue;
                    else if (!rule->rhs[j]->empty_p)
                        break;
                if (j >= rule->rhs_len)
                    symb->u.nonterminal.loop_p = true;
            }
    /* Major cycle: Check looped nonterminal that there is a rule with
       the nonterminal in lhs with a looped nonterminal in rhs and all
       the rest rhs symbols deriving empty string.*/
    do
    {
        changed_p = false;
        for(i = 0;(lhs = nonterm_get(ps, i)) != NULL; i++)
            if (lhs->u.nonterminal.loop_p)
            {
                loop_p = false;
                for(rule = lhs->u.nonterminal.rules;
                     rule != NULL; rule = rule->lhs_next)
                    for(j = 0; j < rule->rhs_len; j++)
                        if (!(symb = rule->rhs[j])->is_terminal && symb->u.nonterminal.loop_p)
                        {
                            for(k = 0; k < rule->rhs_len; k++)
                                if (j == k)
                                    continue;
                                else if (!rule->rhs[k]->empty_p)
                                    break;
                            if (k >= rule->rhs_len)
                                loop_p = true;
                        }
                if (!loop_p)
                    changed_p = true;
                lhs->u.nonterminal.loop_p = loop_p;
            }
    }
    while(changed_p);
}

/* The following function evaluates different sets and flags for
   grammar and checks the grammar on correctness.*/
static void check_grammar(YaepParseState *ps, int strict_p)
{
    YaepSymbol*symb;
    int i;

    set_empty_access_derives(ps);
    set_loop_p(ps);
    if (strict_p)
    {
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
        {
            if (!symb->derivation_p)
            {
                yaep_error(ps, YAEP_NONTERM_DERIVATION,
                     "nonterm `%s' does not derive any term string", symb->repr);
            }
            else if (!symb->access_p)
            {
                yaep_error(ps,
                           YAEP_UNACCESSIBLE_NONTERM,
                           "nonterm `%s' is not accessible from axiom",
                           symb->repr);
            }
        }
    }
    else if (!ps->run.grammar->axiom->derivation_p)
    {
        yaep_error(ps, YAEP_NONTERM_DERIVATION,
                   "nonterm `%s' does not derive any term string",
                   ps->run.grammar->axiom->repr);
    }
    for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
    {
        if (symb->u.nonterminal.loop_p)
        {
            yaep_error(ps, YAEP_LOOP_NONTERM,
                 "nonterm `%s' can derive only itself(grammar with loops)",
                 symb->repr);
        }
    }
    /* We should have correct flags empty_p here.*/
    create_first_follow_sets(ps);
}

/* The following are names of additional symbols.  Don't use them in
   grammars.*/
#define AXIOM_NAME "$"
#define END_MARKER_NAME "ω"
#define TERM_ERROR_NAME "error"

/* It should be negative.*/
#define END_MARKER_CODE -1
#define TERM_ERROR_CODE -2

int yaep_read_grammar(YaepParseRun *pr,
                      YaepGrammar *g,
                      int strict_p,
                      const char*(*read_terminal)(YaepParseRun*pr,YaepGrammar*g,int*code),
                      const char*(*read_rule)(YaepParseRun*pr,YaepGrammar*g,const char***rhs,
                                              const char**abs_node,
                                              int*anode_cost, int**transl, char*mark, char**marks))
{
    const char*name,*lhs,**rhs,*anode;
    YaepSymbol*symb, *start;
    YaepRule*rule;
    int anode_cost;
    int*transl;
    char mark;
    char*marks;
    int i, el, code;

    assert(g != NULL);
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));

    if ((code = setjmp(ps->error_longjump_buff)) != 0)
    {
        return code;
    }
    if (!ps->run.grammar->undefined_p)
    {
        yaep_empty_grammar(ps, ps->run.grammar);
    }
    while((name =(*read_terminal)(pr, pr->grammar, &code)) != NULL)
    {
        if (code < 0)
        {
            yaep_error(ps, YAEP_NEGATIVE_TERM_CODE,
                        "term `%s' has negative code", name);
        }
        symb = symb_find_by_repr(ps, name);
        if (symb != NULL)
        {
            yaep_error(ps, YAEP_REPEATED_TERM_DECL,
                        "repeated declaration of term `%s'", name);
        }
        if (symb_find_by_code(ps, code) != NULL)
        {
            yaep_error(ps, YAEP_REPEATED_TERM_CODE,
                        "repeated code %d in term `%s'", code, name);
        }
        symb_add_terminal(ps, name, code);
    }

    /* Adding error symbol.*/
    if (symb_find_by_repr(ps, TERM_ERROR_NAME) != NULL)
    {
        yaep_error(ps, YAEP_FIXED_NAME_USAGE, "do not use fixed name `%s'", TERM_ERROR_NAME);
    }

    if (symb_find_by_code(ps, TERM_ERROR_CODE) != NULL) abort();

    ps->run.grammar->term_error = symb_add_terminal(ps, TERM_ERROR_NAME, TERM_ERROR_CODE);
    ps->run.grammar->term_error_id = ps->run.grammar->term_error->u.terminal.term_id;
    ps->run.grammar->axiom = ps->run.grammar->end_marker = NULL;

    for (;;)
    {
        lhs = (*read_rule)(pr, pr->grammar, &rhs, &anode, &anode_cost, &transl, &mark, &marks);
        if (lhs == NULL) break;

        symb = symb_find_by_repr(ps, lhs);
        if (symb == NULL)
        {
            symb = symb_add_nonterm(ps, lhs);
        }
        else if (symb->is_terminal)
        {
            yaep_error(ps, YAEP_TERM_IN_RULE_LHS,
                        "term `%s' in the left hand side of rule", lhs);
        }
        if (anode == NULL && transl != NULL &&*transl >= 0 && transl[1] >= 0)
        {
            yaep_error(ps, YAEP_INCORRECT_TRANSLATION,
                        "rule for `%s' has incorrect translation", lhs);
        }
        if (anode != NULL && anode_cost < 0)
        {
            yaep_error(ps, YAEP_NEGATIVE_COST,
                        "translation for `%s' has negative cost", lhs);
        }
        if (ps->run.grammar->axiom == NULL)
        {
            /* We made this here becuase we want that the start rule has number 0.*/
            /* Add axiom and end marker.*/
            start = symb;
            ps->run.grammar->axiom = symb_find_by_repr(ps, AXIOM_NAME);
            if (ps->run.grammar->axiom != NULL)
            {
                yaep_error(ps, YAEP_FIXED_NAME_USAGE, "do not use fixed name `%s'", AXIOM_NAME);
            }
            ps->run.grammar->axiom = symb_add_nonterm(ps, AXIOM_NAME);
            ps->run.grammar->end_marker = symb_find_by_repr(ps, END_MARKER_NAME);
            if (ps->run.grammar->end_marker != NULL)
            {
                yaep_error(ps, YAEP_FIXED_NAME_USAGE, "do not use fixed name `%s'", END_MARKER_NAME);
            }
            if (symb_find_by_code(ps, END_MARKER_CODE) != NULL) abort();
            ps->run.grammar->end_marker = symb_add_terminal(ps, END_MARKER_NAME, END_MARKER_CODE);

            /* Add rules for start*/
            rule = rule_new_start(ps, ps->run.grammar->axiom, NULL, 0);
            rule_new_symb_add(ps, symb);
            rule_new_symb_add(ps, ps->run.grammar->end_marker);
            rule_new_stop(ps);
            rule->order[0] = 0;
            rule->trans_len = 1;
        }
        rule = rule_new_start(ps, symb, anode,(anode != NULL ? anode_cost : 0));
        size_t rhs_len = 0;
        while(*rhs != NULL)
        {
            rhs_len++;
            symb = symb_find_by_repr(ps, *rhs);
            if (symb == NULL)
            {
                symb = symb_add_nonterm(ps, *rhs);
            }
            rule_new_symb_add(ps, symb);
            rhs++;
        }
        rule_new_stop(ps);
        // IXML
        rule->mark = mark;
        memcpy(rule->marks, marks, rhs_len);

        if (transl != NULL)
        {
            for(i = 0;(el = transl[i]) >= 0; i++)
            {
                if (el >= rule->rhs_len)
                {
                    if (el != YAEP_NIL_TRANSLATION_NUMBER)
                    {
                        yaep_error(ps, YAEP_INCORRECT_SYMBOL_NUMBER,
                                   "translation symbol number %d in rule for `%s' is out of range",
                                   el, lhs);
                    }
                    else
                    {
                        rule->trans_len++;
                    }
                }
                else if (rule->order[el] >= 0)
                {
                    yaep_error(ps, YAEP_REPEATED_SYMBOL_NUMBER,
                               "repeated translation symbol number %d in rule for `%s'",
                               el, lhs);
                }
                else
                {
                    rule->order[el] = i;
                    rule->trans_len++;
                }
                assert(i < rule->rhs_len || transl[i] < 0);
            }
        }
    }

    if (ps->run.grammar->axiom == NULL)
    {
        yaep_error(ps, YAEP_NO_RULES, "grammar does not contains rules");
    }

    assert(start != NULL);

    /* Adding axiom : error $eof if it is neccessary.*/
    for(rule = start->u.nonterminal.rules; rule != NULL; rule = rule->lhs_next)
    {
        if (rule->rhs[0] == ps->run.grammar->term_error) break;
    }

    if (rule == NULL)
    {
        rule = rule_new_start(ps, ps->run.grammar->axiom, NULL, 0);
        rule_new_symb_add(ps, ps->run.grammar->term_error);
        rule_new_symb_add(ps, ps->run.grammar->end_marker);
        rule_new_stop(ps);
        rule->trans_len = 0;
        rule->mark = 0;
    }

    check_grammar(ps, strict_p);

    symb_finish_adding_terms(ps);

    if (ps->run.verbose)
    {
        /* Print rules.*/
        for(rule = ps->run.grammar->rulestorage_ptr->first_rule; rule != NULL; rule = rule->next)
        {
            if (rule->lhs->repr[0] != '$')
            {
                MemBuffer *mb = new_membuffer();
                rule_print(mb, ps, rule, true);
                debug_mb("ixml.gr=", mb);
                free_membuffer_and_free_content(mb);
            }
        }
        /* Print symbol sets with lookahead.*/
        if (ps->run.debug)
        {
            for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "%s%s%s%s%s\n",
                                 symb->repr,
                                 (symb->empty_p ? " CAN_BECOME_EMPTY" : ""),
                                 (symb->is_not_operator ? " NOT_OP" : ""),
                                 (symb->access_p ? "" : " OUPS_NOT_REACHABLE"),
                                 (symb->derivation_p ? "" : " OUPS_NO_TEXT"));
                membuffer_append(mb, "  1st: ");
                print_terminal_bitset(mb, ps, symb->u.nonterminal.first);
                membuffer_append(mb, "\n  2nd: ");
                print_terminal_bitset(mb, ps, symb->u.nonterminal.follow);
                debug_mb("ixml.nt=", mb);
                free_membuffer_and_free_content(mb);
            }
        }
    }

    ps->run.grammar->undefined_p = false;
    return 0;
}

/* The following functions set up parameter which affect parser work
   and return the previous parameter value.*/
int yaep_set_lookahead_level(YaepGrammar *grammar, int level)
{
    int old;

    assert(grammar != NULL);
    old = grammar->lookahead_level;
    grammar->lookahead_level =(level < 0 ? 0 : level > 2 ? 2 : level);
    return old;
}

bool yaep_set_one_parse_flag(YaepGrammar *grammar, bool flag)
{
    assert(grammar != NULL);
    bool old = grammar->one_parse_p;
    grammar->one_parse_p = flag;
    return old;
}

bool yaep_set_cost_flag(YaepGrammar *grammar, bool flag)
{
    assert(grammar != NULL);
    bool old = grammar->cost_p;
    grammar->cost_p = flag;
    return old;
}

bool yaep_set_error_recovery_flag(YaepGrammar *grammar, bool flag)
{
    assert(grammar != NULL);
    bool old = grammar->error_recovery_p;
    grammar->error_recovery_p = flag;
    return old;
}

int yaep_set_recovery_match(YaepGrammar *grammar, int n_input)
{
    int old;

    assert(grammar != NULL);
    old = grammar->recovery_token_matches;
    grammar->recovery_token_matches = n_input;
    return old;
}

static void yaep_parse_init(YaepParseState *ps, int n_input)
{
    YaepRule*rule;

    init_dotted_rules(ps);
    set_init(ps, n_input);
    core_symb_to_predcomps_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    {
        int i;
        YaepSymbol*symb;

        for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
        {
            symb->cached_core_symb_to_predcomps = NULL;
        }
    }
#endif
    for(rule = ps->run.grammar->rulestorage_ptr->first_rule; rule != NULL; rule = rule->next)
    {
        rule->caller_anode = NULL;
    }
}

static void free_inside_parse_state(YaepParseState *ps)
{
    free_core_symb_to_vect_lookup(ps);
    free_sets(ps);
    free_dotted_rules(ps);
}

static size_t memusage(YaepParseState *ps)
{
    size_t sum = 0;

    // Grammar...

    // Symbols, the memory usage for the symbols is static during the parse.
    sum += objstack_memusage(&ps->run.grammar->symbs_ptr->symbs_os);
    sum += vlo_memusage(&ps->run.grammar->symbs_ptr->symbs_vlo);
    sum += vlo_memusage(&ps->run.grammar->symbs_ptr->terminals_vlo);
    sum += vlo_memusage(&ps->run.grammar->symbs_ptr->nonterminals_vlo);
    sum += hash_table_memusage(ps->run.grammar->symbs_ptr->map_repr_to_symb);
    sum += hash_table_memusage(ps->run.grammar->symbs_ptr->map_code_to_symb);

    // Rules, the memory usage is static during the parse.
    sum += objstack_memusage(&ps->run.grammar->rulestorage_ptr->rules_os);

    // Terminal bitsets static.
    sum += objstack_memusage(&ps->run.grammar->term_sets_ptr->terminal_bitset_os);
    sum += vlo_memusage(&ps->run.grammar->term_sets_ptr->terminal_bitset_vlo);
    sum += hash_table_memusage(ps->run.grammar->term_sets_ptr->map_terminal_bitset_to_id);

    // Parse state, increasing during parse.

    sum += objstack_memusage(&ps->set_cores_os);
    sum += objstack_memusage(&ps->set_dotted_rules_os);
    sum += objstack_memusage(&ps->set_parent_dotted_rule_ids_os);
    sum += objstack_memusage(&ps->set_matched_lengths_os);
    sum += objstack_memusage(&ps->sets_os);
    sum += objstack_memusage(&ps->set_term_lookahead_os);

    sum += hash_table_memusage(ps->cache_stateset_cores);
    sum += hash_table_memusage(ps->cache_stateset_matched_lengths);
    sum += hash_table_memusage(ps->cache_stateset_core_matched_lengths);
    sum += hash_table_memusage(ps->cache_stateset_term_lookahead);

    sum += vlo_memusage(&ps->dotted_rules_table_vlo);
    sum += objstack_memusage(&ps->dotted_rules_os);
    sum += vlo_memusage(&ps->dotted_rule_matched_length_vec_vlo);
    sum += objstack_memusage(&ps->core_symb_to_predcomps_os);
    sum += vlo_memusage(&ps->new_core_symb_to_predcomps_vlo);
    sum += objstack_memusage(&ps->vect_ids_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    sum += hash_table_memusage(ps->map_core_symb_to_predcomps);
#else
    sum += vlo_memusage(&ps->core_symb_table_vlo);
    sum += objstack_memusage(&ps->core_symb_tab_rows);
#endif

    sum += hash_table_memusage(ps->map_transition_to_coresymbvect);
    sum += hash_table_memusage(ps->map_reduce_to_coresymbvect);
    sum += objstack_memusage(&ps->recovery_state_tail_sets);
    sum += vlo_memusage(&ps->original_state_set_tail_stack);
    sum += vlo_memusage(&ps->vlo_array);
    sum += hash_table_memusage(ps->set_of_reserved_memory);
    sum += vlo_memusage(&ps->tnodes_vlo);
    sum += hash_table_memusage(ps->map_node_to_visit);
    sum += objstack_memusage(&ps->node_visits_os);
    sum += vlo_memusage(&ps->recovery_state_stack);
    sum += objstack_memusage(&ps->parse_state_os);
    sum += hash_table_memusage(ps->map_rule_orig_statesetind_to_internalstate);

    return sum;
}

/* The following function reads all input tokens.*/
static void read_input(YaepParseState *ps)
{
    int code;
    void *attr;

    while((code = ps->run.read_token((YaepParseRun*)ps, &attr)) >= 0)
    {
        tok_add(ps, code, attr);
    }

    tok_add(ps, END_MARKER_CODE, NULL);
}

/* Add predicted (derived) not yet started dotted_rules which is formed from
   given start dotted_rule DOTTED_RULE with matched_length DIST by reducing symbol
   which can derivate empty string and which is placed after dot in
   given dotted_rule. */
static void complete_empty_nonterminals_in_rule(YaepParseState *ps,
                                                YaepDottedRule *dotted_rule,
                                                int dotted_rule_parent_id,
                                                bool only_nots)
{
    YaepRule *rule = dotted_rule->rule;
    int dyn_lookahead_context = dotted_rule->dyn_lookahead_context;

    for(int j = dotted_rule->dot_j; ; ++j)
    {
        if (!rule->rhs[j]) break;
        if (rule->rhs[j]->empty_p)
        {
            if (!only_nots)
            {
                YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, rule, j+1, dyn_lookahead_context);
                set_add_dotted_rule_with_parent(ps, new_dotted_rule, dotted_rule_parent_id, "complete empty");
            }
        }
        else if (rule->rhs[j]->is_not_operator)
        {
            if (blocked_by_lookahead(ps, dotted_rule, rule->rhs[j], 1-only_nots, "lookahead1"))
            {
                break;
            }
            else
            {
                YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, rule, j+1, dyn_lookahead_context);
                set_add_dotted_rule_with_parent(ps, new_dotted_rule, dotted_rule_parent_id, "complete lookahead ok pre");
            }
        }
        else
        {
            break;
        }
    }
}

static int mmin(int a, int b)
{
    if (a <= b) return a;
    return b;
}

static bool blocked_by_lookahead(YaepParseState *ps, YaepDottedRule *dotted_rule, YaepSymbol *symb, int n, const char *info)
{
    // Some empty rules encode a purpose in their names.
    // We have +"howdy" which becomes a rule |+howdy for insertions and
    // we have !"chapter" which becaomes a rule |!Schapter for not lookups if strings.
    // we have !!"chapter" which becomes a rule |?Schapter for required lookups if strings.
    // and ![L] which becomes |!CL for charsets
    // and ![Ls;'_-'] which becomes |![Ls;'_-']
    // and !#41 which becomes |!SA

    if (!symb) return false;
    if (!symb->is_not_operator) return false;

    // The rule is a NOT lookup rule that can potentially block the completion of this empty rule if the matching lookahead exists.
    bool is_blocked = has_lookahead(ps, symb, n);

    if (ps->run.debug)
    {
        MemBuffer *mb = new_membuffer();
        int tok_i = ps->tok_i+n;
        membuffer_printf(mb, "@%d NOT operator %s ", ps->tok_i, symb->repr);
        membuffer_printf(mb, "%s", info);
        if (is_blocked)
        {
            membuffer_printf(mb, " blocked: ");
        }
        else
        {
            membuffer_printf(mb, " ok ");
        }
        int to = mmin(ps->input_len, tok_i+strlen(symb->repr));
        for (int i = tok_i; i < to; ++i)
        {
            membuffer_printf(mb, "%s ", ps->input[i].symb->hr);
        }
        membuffer_append_null(mb);

        yaep_trace(ps, mb->buffer_);
        debug_step_blocked(ps, dotted_rule, 0, 0);

        free_membuffer_and_free_content(mb);
    }


    return is_blocked;
}

static bool has_lookahead(YaepParseState *ps, YaepSymbol *symb, int n)
{
    assert(symb->repr[0]== '|');
    assert(symb->repr[1]== '!');
    assert(symb->repr[2]== 'S' || symb->repr[2]== '[');

    int p = ps->tok_i+n;

    // End of buffer immediately returns no lookahead match.
    if (p >= ps->input_len) return false;
    // The last token is $eof returs no lookahead match.
    if (!strcmp(ps->input[p].symb->hr, "$eof")) return false;

    char type = symb->repr[2];
    if (type == 'S')
    {
        // Start scanning utf8 characters after the S: |!S...
        const char *u = symb->repr+3;
        const char *stop = symb->repr+strlen(symb->repr);
        for (int i = p; i < ps->input_len && u < stop; ++i)
        {
            YaepSymbol *next = ps->input[i].symb;
            if (!strcmp(next->hr, "$eof")) return false;
            int unc = 0;
            size_t len = 0;
            bool ok = decode_utf8(u, stop, &unc, &len);
            if (!ok)
            {
                fprintf(stderr, "Illegal utf8 encoding in not lookahead >%s<\n", symb->repr);
                exit(1);
            }
            if (next->u.terminal.code != unc)
            {
                // There is a mismatch between the lookahead and the actual content.
                // We can stop early and report failed lookahead.
                return false;
            }
            // Jump to the next character in the lookahead string.
            u += len;
        }
        // All characters matched the lookahead!
        return true;
    }

    // Charset lookahead.
    YaepSymbol *ys = symb_find_by_repr(ps, symb->repr+2);
    if (!ys)
    {
        // No charset exists, because no input characters matched this charset.
        // Easy, no possible lookahead match.
        return false;
    }
    YaepSymbol *next = ps->input[p].symb;
    bool match = terminal_bitset_test(ps, ys->u.nonterminal.first, next->u.terminal.term_id);

    return match;
}

static bool core_has_not_rules(YaepStateSetCore *c)
{
    for (int i = 0; i < c->num_started_dotted_rules; ++i)
    {
        YaepDottedRule *dotted_rule = c->dotted_rules[i];
        if (dotted_rule->rule->contains_not_operator) return true;
    }
    return false;
}

/* The following function adds the rest(predicted not-yet-started) dotted_rules to the
   new set and and forms triples(set_core, symbol, indexes) for
   further fast search of start dotted_rules from given core by
   transition on given symbol(see comment for abstract data `core_symb_to_predcomps'). */
static void expand_new_set(YaepParseState *ps)
{
    YaepSymbol *symb;
    YaepCoreSymbToPredComps *core_symb_to_predcomps;

    /* Look for dotted rules that can be progressed because the next non-terminal
       can be empty. I.e. we can complete the E immediately.
       S = E, 'a'.
       E = .
    */
    for (int leading_rule_index = 0;
         leading_rule_index < ps->new_num_leading_dotted_rules;
         leading_rule_index++)
    {
        YaepDottedRule *dotted_rule = ps->new_dotted_rules[leading_rule_index];
        complete_empty_nonterminals_in_rule(ps,
                                            dotted_rule,
                                            leading_rule_index,
                                            false);
    }

    for (int rule_index_in_core = 0;
         rule_index_in_core < ps->new_core->num_dotted_rules;
         rule_index_in_core++)
    {
        YaepDottedRule *dotted_rule = ps->new_dotted_rules[rule_index_in_core];

        // Check that there is a symbol after the dot!
        if (dotted_rule->dot_j < dotted_rule->rule->rhs_len)
        {
            // Yes.
            symb = dotted_rule->rule->rhs[dotted_rule->dot_j];
            core_symb_to_predcomps = core_symb_to_predcomps_find(ps, ps->new_core, symb);

            if (core_symb_to_predcomps)
            {
                //yaep_trace(ps, "re-using cspc%d[c%d %s]", core_symb_to_predcomps->id, ps->new_core->id, symb->hr);
            }
            else
            {
                // No vector found for this core+symb combo.
                // Add a new vector.
                core_symb_to_predcomps = core_symb_to_predcomps_new(ps, ps->new_core, symb);
                yaep_trace(ps, "new cspc%d [c%d %s]", core_symb_to_predcomps->id, ps->new_core->id, symb->hr);

                if (!symb->is_terminal)
                {
                    for (YaepRule *r = symb->u.nonterminal.rules; r != NULL; r = r->lhs_next)
                    {
                        YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, r, 0, 0);
                        char buf[128];
                        snprintf(buf, 128, "d%d@%d predicts %s", dotted_rule->id, dotted_rule->dot_j, r->lhs->hr);
                        set_add_dotted_rule_no_match_yet(ps, new_dotted_rule, buf);
                    }
                }
            }
            // Add a prediction to the core+symb lookup that points to this dotted rule.
            // I.e. when we reach a certain symbol within this core, the we just find
            // a vector using the core+symb lookup. This vector stores all predicted dotted_rules
            // that should be added for further parsing.
            core_symb_to_predcomps_add_predict(ps, core_symb_to_predcomps, rule_index_in_core);

            // The non-terminal can be empty and this is a not-yet added dotted_rule.
            if (symb->empty_p && rule_index_in_core >= ps->new_core->num_all_matched_lengths)
            {
                int first = 1; //ps->new_set->id == 0 ? 0 : 1;
                if (!blocked_by_lookahead(ps, dotted_rule, dotted_rule->rule->rhs[dotted_rule->dot_j], first, "lookahead2a"))
                {
                    YaepDottedRule *nnnew_dotted_rule = create_dotted_rule(ps,
                                                                           dotted_rule->rule,
                                                                           dotted_rule->dot_j+1,
                                                                           0);
                    //  "complete_empty_rule_gurka");
                    // Add new rule to be handled in next loop iteration.
                    yaep_trace(ps, "complete empty rule %s", dotted_rule->rule->lhs->hr);
                    set_add_dotted_rule_no_match_yet(ps, nnnew_dotted_rule, "complete empty rule");
                }
            }
            if (symb->is_not_operator && rule_index_in_core >= ps->new_core->num_all_matched_lengths)
            {
                int first = ps->new_set->id == 0 ? 0 : 1;
                if (!blocked_by_lookahead(ps, dotted_rule, dotted_rule->rule->rhs[dotted_rule->dot_j], first, "lookahead2b"))
                {
                    YaepDottedRule *new_dotted_rule = create_dotted_rule(ps,
                                                                         dotted_rule->rule,
                                                                         dotted_rule->dot_j+1,
                                                                         0);
                    //"complete_lookahead_ok");
                    yaep_trace(ps, "complete lookahead ok");
                    set_add_dotted_rule_no_match_yet(ps, new_dotted_rule, "complete lookahead ok");
                }
            }
        }
    }

    for (int rule_index_in_core = 0;
         rule_index_in_core < ps->new_core->num_dotted_rules;
         rule_index_in_core++)
    {
        YaepDottedRule *dotted_rule = ps->new_dotted_rules[rule_index_in_core];

        // Is this dotted_rule complete? I.e. the dot is at its rightmost position?
        if (dotted_rule->dot_j != dotted_rule->rule->rhs_len) continue;

        // Yes, all rhs elements have been completed/scanned.
        symb = dotted_rule->rule->lhs;

        core_symb_to_predcomps = core_symb_to_predcomps_find(ps, ps->new_core, symb);
        if (core_symb_to_predcomps == NULL)
        {
            core_symb_to_predcomps = core_symb_to_predcomps_new(ps, ps->new_core, symb);
            yaep_trace(ps, "new cspc%d [c%d %s]", core_symb_to_predcomps->id, ps->new_core->id, symb->hr);
        }
        core_symb_to_predcomps_add_complete(ps, core_symb_to_predcomps, rule_index_in_core);
    }

    if (ps->run.grammar->lookahead_level > 1)
    {
        YaepDottedRule *new_dotted_rule, *shifted_dotted_rule;
        terminal_bitset_t *dyn_lookahead_context_set;
        int dyn_lookahead_context, j;
        bool changed_p;

        /* Now we have incorrect initial dotted_rules because their dyn_lookahead_context is not correct. */
        dyn_lookahead_context_set = terminal_bitset_create(ps);
        do
        {
            changed_p = false;
            for(int new_rule_index_in_core = ps->new_core->num_all_matched_lengths;
                new_rule_index_in_core < ps->new_core->num_dotted_rules;
                new_rule_index_in_core++)
            {
                terminal_bitset_clear(ps, dyn_lookahead_context_set);
                new_dotted_rule = ps->new_dotted_rules[new_rule_index_in_core];

                core_symb_to_predcomps = core_symb_to_predcomps_find(ps, ps->new_core, new_dotted_rule->rule->lhs);

                for (j = 0; j < core_symb_to_predcomps->predictions.len; j++)
                {
                    int rule_index_in_core = core_symb_to_predcomps->predictions.ids[j];
                    YaepDottedRule *dotted_rule = ps->new_dotted_rules[rule_index_in_core];
                    shifted_dotted_rule = create_dotted_rule(ps,
                                                             dotted_rule->rule,
                                                             dotted_rule->dot_j+1,
                                                             dotted_rule->dyn_lookahead_context);
                    //                                                             "expand_enss3");
                    terminal_bitset_or(ps, dyn_lookahead_context_set, shifted_dotted_rule->lookahead);
                }
                dyn_lookahead_context = terminal_bitset_insert(ps, dyn_lookahead_context_set);
                if (dyn_lookahead_context >= 0)
                {
                    dyn_lookahead_context_set = terminal_bitset_create(ps);
                }
                else
                {
                    dyn_lookahead_context = -dyn_lookahead_context - 1;
                }
                YaepDottedRule *dotted_rule = create_dotted_rule(ps,
                                                                 new_dotted_rule->rule,
                                                                 new_dotted_rule->dot_j,
                                                                 dyn_lookahead_context);
                // "expand_enss4");

                if (dotted_rule != new_dotted_rule)
                {
                    ps->new_dotted_rules[new_rule_index_in_core] = dotted_rule;
                    changed_p = true;
                }
            }
        }
        while(changed_p);
    }

    set_new_core_stop(ps);
    core_symb_to_predcomps_new_all_stop(ps);
}

static void build_start_set(YaepParseState *ps)
{
    int dyn_lookahead_context = 0;

    prepare_for_leading_dotted_rules(ps);

    if (ps->run.grammar->lookahead_level > 1)
    {
        terminal_bitset_t *empty_dyn_lookahead_context_set = terminal_bitset_create(ps);
        terminal_bitset_clear(ps, empty_dyn_lookahead_context_set);
        dyn_lookahead_context = terminal_bitset_insert(ps, empty_dyn_lookahead_context_set);

        /* Empty dyn_lookahead_context in the table has always number zero. */
        assert(dyn_lookahead_context == 0);
    }

    for (YaepRule *rule = ps->run.grammar->axiom->u.nonterminal.rules; rule != NULL; rule = rule->lhs_next)
    {
        YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, rule, 0, dyn_lookahead_context);
        set_add_dotted_rule_with_matched_length(ps, new_dotted_rule, 0, "axiom");
    }

    bool core_added = convert_leading_dotted_rules_into_new_set(ps);
    assert(core_added);

    expand_new_set(ps);
    ps->state_sets[0] = ps->new_set;
}

static int lookup_matched_length(YaepParseState *ps, YaepStateSet *set, int rule_index_in_core)
{
    if (rule_index_in_core >= set->core->num_all_matched_lengths)
    {
        return 0;
    }
    if (rule_index_in_core < set->core->num_started_dotted_rules)
    {
        return set->matched_lengths[rule_index_in_core];
    }
    return set->matched_lengths[set->core->to_parent_rule_index[rule_index_in_core]];
}

static void trace_lookahead_predicts_no_match(YaepParseState *ps, int lookahead_term_id, YaepDottedRule *new_dotted_rule, const  char *info)
{
    MemBuffer *mb = new_membuffer();
    YaepSymbol *symb = symb_find_by_term_id(ps, lookahead_term_id);
    const char *hr = symb->hr;
    if (!symb) hr = "?";
    membuffer_printf(mb, "look bitset %s (%d) %s blocked by ", info, lookahead_term_id, hr);
    print_dotted_rule(mb, ps, ps->tok_i-1, new_dotted_rule, 0, 0);
    membuffer_append(mb, "\n");
}

void try_eat_token(const char *why, YaepParseState *ps, YaepStateSet *set,
                   YaepDottedRule *dotted_rule, int rule_index_in_core,
                   int lookahead_term_id, int local_lookahead_level,
                   int add_matched_length);

void try_eat_token(const char *why, YaepParseState *ps, YaepStateSet *set,
                   YaepDottedRule *dotted_rule, int rule_index_in_core,
                   int lookahead_term_id, int local_lookahead_level,
                   int add_matched_length)
{
    YaepDottedRule *new_dotted_rule = create_dotted_rule(ps,
                                                         dotted_rule->rule, dotted_rule->dot_j + 1,
                                                         dotted_rule->dyn_lookahead_context);
    if (false && local_lookahead_level != 0 &&
        !terminal_bitset_test(ps, new_dotted_rule->lookahead, lookahead_term_id) &&
        !terminal_bitset_test(ps, new_dotted_rule->lookahead, ps->run.grammar->term_error_id))
    {
        // Lookahead predicted no-match. Stop here.
        return;
    }

    int matched_length = lookup_matched_length(ps, set, rule_index_in_core);
    matched_length += add_matched_length;

    // This combo dotted_rule + matched_length did not already exist, lets add it.
    // But first test if there is a not lookahead that blocks....
    if (!blocked_by_lookahead(ps, new_dotted_rule, new_dotted_rule->rule->rhs[new_dotted_rule->dot_j], 1, why))
    {
        if (!dotted_rule_matched_length_test_and_set(ps, new_dotted_rule, matched_length))
        {
            set_add_dotted_rule_with_matched_length(ps, new_dotted_rule, matched_length, why);
        }
    }
}

void check_predicted_dotted_rules(YaepParseState *ps,
                                  YaepStateSet *set,
                                  YaepVect *predictions,
                                  int lookahead_term_id,
                                  int local_lookahead_level)
{
    for (int i = 0; i < predictions->len; i++)
    {
        int rule_index_in_core = predictions->ids[i];
        YaepDottedRule *dotted_rule = set->core->dotted_rules[rule_index_in_core];
        try_eat_token("scan", ps, set, dotted_rule, rule_index_in_core, lookahead_term_id, local_lookahead_level, 1);
    }
}

void check_leading_dotted_rules(YaepParseState *ps, YaepStateSet *set, int lookahead_term_id, int local_lookahead_level)
{
    for (int i = 0; i < ps->new_num_leading_dotted_rules; i++)
    {
        YaepDottedRule *new_dotted_rule = ps->new_dotted_rules[i];
        bool completed = new_dotted_rule->empty_tail_p;

        YaepSymbol *sym = new_dotted_rule->rule->rhs[new_dotted_rule->dot_j];
        if (!completed && sym && sym->is_not_operator
            && !blocked_by_lookahead(ps, new_dotted_rule, new_dotted_rule->rule->rhs[new_dotted_rule->dot_j], 1, "lookaheadbanan"))
        {
            completed = true;
        }

        // Note that empty_tail_p is both true if you reached the end of the rule
        // and if the rule can derive the empty string from the dot.
        if (completed)
        {
            /* All tail in new sitiation may derivate empty string so
               make reduce and add new dotted_rules. */
            int new_matched_length = ps->new_matched_lengths[i];
            int place = ps->state_set_k + 1 - new_matched_length;
            YaepStateSet *prev_set = ps->state_sets[place];
            YaepCoreSymbToPredComps *prev_core_symb_to_predcomps = core_symb_to_predcomps_find(ps, prev_set->core, new_dotted_rule->rule->lhs);
            if (prev_core_symb_to_predcomps == NULL)
            {
                assert(new_dotted_rule->rule->lhs == ps->run.grammar->axiom);
                continue;
            }
            for (int j = 0; j < prev_core_symb_to_predcomps->predictions.len; j++)
            {
                int rule_index_in_core = prev_core_symb_to_predcomps->predictions.ids[j];
                YaepDottedRule *dotted_rule = prev_set->core->dotted_rules[rule_index_in_core];
                try_eat_token("complete", ps, prev_set, dotted_rule, rule_index_in_core, lookahead_term_id, local_lookahead_level, new_matched_length);
            }
        }
    }
}

/* The following function predicts a new state set by shifting dotted_rules
   of SET given in CORE_SYMB_TO_PREDCOMPS with given lookahead terminal number.
   If the number is negative, we ignore lookahead at all. */
static void complete_and_predict_new_state_set(YaepParseState *ps,
                                               YaepStateSet *set,
                                               YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                               YaepSymbol *THE_TERMINAL,
                                               YaepSymbol *NEXT_TERMINAL)
{
    int lookahead_term_id = NEXT_TERMINAL?NEXT_TERMINAL->u.terminal.term_id:-1;
    int local_lookahead_level = (lookahead_term_id < 0 ? 0 : ps->run.grammar->lookahead_level);

    prepare_for_leading_dotted_rules(ps);

    YaepVect *predictions = &core_symb_to_predcomps->predictions;

    clear_dotted_rule_matched_length_set(ps);

    check_predicted_dotted_rules(ps, set, predictions, lookahead_term_id, local_lookahead_level);
    check_leading_dotted_rules(ps, set, lookahead_term_id, local_lookahead_level);

    bool core_added = convert_leading_dotted_rules_into_new_set(ps);

    if (core_added)
    {
        expand_new_set(ps);
        ps->new_core->term = core_symb_to_predcomps->symb;
    }
}

/* This page contains error recovery code.  This code finds minimal
   cost error recovery.  The cost of error recovery is number of
   tokens ignored by error recovery.  The error recovery is successful
   when we match at least RECOVERY_TOKEN_MATCHES tokens.*/

/* The following function may be called if you know that state set has
   original sets upto LAST element(including it).  Such call can
   decrease number of restored sets.*/
static void set_original_set_bound(YaepParseState *ps, int last)
{
    assert(last >= 0 && last <= ps->recovery_start_set_k
            && ps->original_last_state_set_el <= ps->recovery_start_set_k);
    ps->original_last_state_set_el = last;
}

/* The following function guarantees that original state set tail sets
   starting with state_set_k(including the state) is saved.  The function
   should be called after any decreasing state_set_k with subsequent
   writing to state set [state_set_k]. */
static void save_original_sets(YaepParseState *ps)
{
    int length, curr_pl;

    assert(ps->state_set_k >= 0 && ps->original_last_state_set_el <= ps->recovery_start_set_k);
    length = VLO_LENGTH(ps->original_state_set_tail_stack) / sizeof(YaepStateSet*);

    for(curr_pl = ps->recovery_start_set_k - length; curr_pl >= ps->state_set_k; curr_pl--)
    {
        VLO_ADD_MEMORY(ps->original_state_set_tail_stack, &ps->state_sets[curr_pl],
                        sizeof(YaepStateSet*));

        if (ps->run.debug)
        {
            fprintf(stderr, "++++Save original set=%d\n", curr_pl);
            /*print_state_set(ps,
                            stderr,
                            ps->state_sets[curr_pl],
                            curr_pl);*/
            fprintf(stderr, "\n");
        }

    }
    ps->original_last_state_set_el = ps->state_set_k - 1;
}

/* If it is necessary, the following function restores original pl
   part with states in range [0, last_state_set_el]. */
static void restore_original_sets(YaepParseState *ps, int last_state_set_el)
{
    assert(last_state_set_el <= ps->recovery_start_set_k
            && ps->original_last_state_set_el <= ps->recovery_start_set_k);
    if (ps->original_last_state_set_el >= last_state_set_el)
    {
        ps->original_last_state_set_el = last_state_set_el;
        return;
    }
    for(;;)
    {
        ps->original_last_state_set_el++;
        ps->state_sets[ps->original_last_state_set_el]
            =((YaepStateSet**) VLO_BEGIN(ps->original_state_set_tail_stack))
            [ps->recovery_start_set_k - ps->original_last_state_set_el];

        if (ps->run.debug)
        {
            fprintf(stderr, "++++++Restore original set=%d\n", ps->original_last_state_set_el);
            /*print_state_set(ps, stderr, ps->state_sets[ps->original_last_state_set_el], ps->original_last_state_set_el);*/
            fprintf(stderr, "\n");
        }

        if (ps->original_last_state_set_el >= last_state_set_el)
            break;
    }
}

/* The following function looking backward in state set starting with element
   START_STATE_SET_EL and returns state set element which refers set with dotted_rule
   containing `. error'.  START_STATE_SET_EL should be non negative.
   Remember that zero state set set contains `.error' because we added such
   rule if it is necessary.  The function returns number of terminals
  (not taking error into account) on path(result, start_state_set_set].*/
static int find_error_state_set_set(YaepParseState *ps, int start_state_set_set, int*cost)
{
    int curr_pl;

    assert(start_state_set_set >= 0);
   *cost = 0;
    for(curr_pl = start_state_set_set; curr_pl >= 0; curr_pl--)
        if (core_symb_to_predcomps_find(ps, ps->state_sets[curr_pl]->core, ps->run.grammar->term_error) != NULL)
            break;
        else if (ps->state_sets[curr_pl]->core->term != ps->run.grammar->term_error)
           (*cost)++;
    assert(curr_pl >= 0);
    return curr_pl;
}

/* The following function creates and returns new error recovery state
   with charcteristics(LAST_ORIGINAL_STATE_SET_EL, BACKWARD_MOVE_COST,
   state_set_k, tok_i). */
static YaepRecoveryState new_recovery_state(YaepParseState *ps, int last_original_state_set_el, int backward_move_cost)
{
    YaepRecoveryState state;
    int i;

    assert(backward_move_cost >= 0);

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Creating recovery state: original set=%d, tok=%d, ",
                last_original_state_set_el, ps->tok_i);
        // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, "\n");
    }

    state.last_original_state_set_el = last_original_state_set_el;
    state.state_set_tail_length = ps->state_set_k - last_original_state_set_el;
    assert(state.state_set_tail_length >= 0);
    for(i = last_original_state_set_el + 1; i <= ps->state_set_k; i++)
    {
        OS_TOP_ADD_MEMORY(ps->recovery_state_tail_sets, &ps->state_sets[i], sizeof(ps->state_sets[i]));

        if (ps->run.debug)
        {
            fprintf(stderr, "++++++Saving set=%d\n", i);
            /*print_state_set(ps, stderr, ps->state_sets[i], i);*/
            fprintf(stderr, "\n");
        }

    }
    state.state_set_tail =(YaepStateSet**) OS_TOP_BEGIN(ps->recovery_state_tail_sets);
    OS_TOP_FINISH(ps->recovery_state_tail_sets);
    state.start_tok = ps->tok_i;
    state.backward_move_cost = backward_move_cost;
    return state;
}

/* The following function creates new error recovery state and pushes
   it on the states stack top. */
static void push_recovery_state(YaepParseState *ps, int last_original_state_set_el, int backward_move_cost)
{
    YaepRecoveryState state;

    state = new_recovery_state(ps, last_original_state_set_el, backward_move_cost);

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Push recovery state: original set=%d, tok=%d, ",
                 last_original_state_set_el, ps->tok_i);
        // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, "\n");
    }

    VLO_ADD_MEMORY(ps->recovery_state_stack, &state, sizeof(state));
}

/* The following function sets up parser state(pl, state_set_k, ps->tok_i)
   according to error recovery STATE. */
static void set_recovery_state(YaepParseState *ps, YaepRecoveryState*state)
{
    int i;

    ps->tok_i = state->start_tok;
    restore_original_sets(ps, state->last_original_state_set_el);
    ps->state_set_k = state->last_original_state_set_el;

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Set recovery state: set=%d, tok=%d, ",
                 ps->state_set_k, ps->tok_i);
        // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, "\n");
    }

    for(i = 0; i < state->state_set_tail_length; i++)
    {
        ps->state_sets[++ps->state_set_k] = state->state_set_tail[i];

        if (ps->run.debug)
        {
            fprintf(stderr, "++++++Add saved set=%d\n", ps->state_set_k);
            /*print_state_set(ps, stderr, ps->state_sets[ps->state_set_k], ps->state_set_k);*/
            fprintf(stderr, "\n");
        }

    }
}

/* The following function pops the top error recovery state from
   states stack.  The current parser state will be setup according to
   the state. */
static YaepRecoveryState pop_recovery_state(YaepParseState *ps)
{
    YaepRecoveryState *state;

    state = &((YaepRecoveryState*) VLO_BOUND(ps->recovery_state_stack))[-1];
    VLO_SHORTEN(ps->recovery_state_stack, sizeof(YaepRecoveryState));

    if (ps->run.debug)
        fprintf(stderr, "++++Pop error recovery state\n");

    set_recovery_state(ps, state);
    return*state;
}

/* Return true if goto set SET from parsing list PLACE can be used as
   the next set.  The criterium is that all origin sets of start
   dotted_rules are the same as from PLACE. */
static bool can_transition_to_set(YaepParseState *ps, YaepStateSet*set, int place)
{
    int i, dist;
    int*matched_lengths = set->matched_lengths;

    for(i = set->core->num_started_dotted_rules - 1; i >= 0; i--)
    {
        if ((dist = matched_lengths[i]) <= 1)
            continue;
        /* Sets at origins of dotted_rules with matched_length one are supposed
           to be the same. */
        if (ps->state_sets[ps->state_set_k + 1 - dist] != ps->state_sets[place + 1 - dist])
            return false;
    }
    return true;
}

static int try_to_recover(YaepParseState *ps)
{
    int saved_tok_i, start, stop;

    /* Error recovery.  We do not check transition vector
       because for terminal transition vector is never NULL
       and reduce is always NULL. */

    saved_tok_i = ps->tok_i;
    if (ps->run.grammar->error_recovery_p)
    {
        fprintf(stderr, "Attempting error recovery...\n");
        error_recovery(ps, &start, &stop);
        ps->run.syntax_error(
            (YaepParseRun*)ps,
            saved_tok_i,
            ps->input[saved_tok_i].attr,
            start,
            ps->input[start].attr,
            stop,
            ps->input[stop].attr);
        return 1;
    }
    else
    {
        ps->run.failed_p = true;
        ps->run.syntax_error(
            (YaepParseRun*)ps,
            saved_tok_i,
            ps->input[saved_tok_i].attr,
            -1,
            NULL,
            -1,
            NULL);
        return 2;
    }

    return 0;
}

static YaepStateSetTermLookAhead *lookup_cached_set_term_lookahead(YaepParseState *ps,
                                                                   YaepSymbol *THE_TERMINAL,
                                                                   YaepSymbol *NEXT_TERMINAL,
                                                                   YaepStateSet *set)
{
    OS_TOP_EXPAND(ps->set_term_lookahead_os, sizeof(YaepStateSetTermLookAhead));

    YaepStateSetTermLookAhead *new_set_term_lookahead = (YaepStateSetTermLookAhead*)OS_TOP_BEGIN(ps->set_term_lookahead_os);
    new_set_term_lookahead->set = set;
    new_set_term_lookahead->term = THE_TERMINAL;
    new_set_term_lookahead->lookahead_term = NEXT_TERMINAL?NEXT_TERMINAL->u.terminal.term_id:-1;

    memset(new_set_term_lookahead->result, 0, MAX_CACHED_GOTO_RESULTS*sizeof(new_set_term_lookahead->result[0]));

    new_set_term_lookahead->curr = 0;
    // We write into the hashtable using the entry point! Yay!
    // I.e. there is no write hash table entry function.....
    YaepStateSetTermLookAhead **stlg = (YaepStateSetTermLookAhead**)find_hash_table_entry(ps->cache_stateset_term_lookahead, new_set_term_lookahead, true);

    if (*stlg != NULL)
    {
        YaepStateSet *s;

        OS_TOP_NULLIFY(ps->set_term_lookahead_os);
        for(int i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
        {
            s = (*stlg)->result[i];
            if (s == NULL) break;
            if (can_transition_to_set(ps, s, (*stlg)->place[i]))
            {
                ps->new_set = s;
                ps->n_goto_successes++;
                if (xmq_trace_enabled_)
                {
                    YaepSymbol *lookahead_symb = symb_find_by_term_id(ps, new_set_term_lookahead->lookahead_term);
                    const char *losymb = "";
                    if (lookahead_symb) losymb = lookahead_symb->hr;

                    yaep_trace(ps, "found stlg [s%d %s %s] -> s%d",
                               new_set_term_lookahead->set->id,
                               new_set_term_lookahead->term->hr,
                               losymb,
                               ps->new_set->id);
                }
                break;
            }
        }
    }
    else
    {
        OS_TOP_FINISH(ps->set_term_lookahead_os);
        // Write the new_set_term_lookahead triplet into the hash table.
        *stlg = new_set_term_lookahead;
        ps->num_set_term_lookahead++;

        /*
        if (xmq_trace_enabled_)
        {
            YaepSymbol *lookahead_symb = symb_find_by_term_id(ps, new_set_term_lookahead->lookahead_term);
            const char *losymb = "";
            if (lookahead_symb && lookahead_symb->hr) losymb = lookahead_symb->hr;
            yaep_trace(ps, "create stlg [s%d %s %s]",
                       new_set_term_lookahead->set->id,
                       new_set_term_lookahead->term->hr,
                       losymb);
                       }*/
    }

    return *stlg;
}

/* Save(set, term, lookahead) -> new_set in the table. */
static void save_cached_set(YaepParseState *ps, YaepStateSetTermLookAhead *entry, YaepSymbol *NEXT_TERMINAL)
{
    int i = entry->curr;
    entry->result[i] = ps->new_set;
    entry->place[i] = ps->state_set_k;
    entry->lookahead_term = NEXT_TERMINAL ? NEXT_TERMINAL->u.terminal.term_id : -1;
    entry->curr = (i + 1) % MAX_CACHED_GOTO_RESULTS;

    if (xmq_trace_enabled_)
    {
        YaepSymbol *lookahead_symb = symb_find_by_term_id(ps, entry->lookahead_term);
        const char *losymb = "";
        if (lookahead_symb) losymb = lookahead_symb->hr;
        yaep_trace(ps, "store stlg [s%d %s %s] -> s%d",
                   entry->set->id,
                   entry->term->hr,
                   losymb,
                   ps->new_set->id);
    }
}

static void perform_parse(YaepParseState *ps)
{
    yaep_debug(ps, "perform_parse()");
    error_recovery_init(ps);
    build_start_set(ps);

    if (ps->run.trace)
    {
        MemBuffer *mb = new_membuffer();
        print_state_set(mb, ps, ps->new_set, 0);
        debug_mb("ixml.pa.state=", mb);
        free_membuffer_and_free_content(mb);
    }

    ps->tok_i = 0;
    ps->state_set_k = 0;

    for(; ps->tok_i < ps->input_len; ps->tok_i++)
    {
        // This assert is TODO! Theoretically the state_set_k could be less than tok_i
        // assuming a state set has been reused.
        // So far I have not seen that, so assume state_set_k == tok_i == index of input char.
        assert(ps->tok_i == ps->state_set_k);

        YaepSymbol *THE_TERMINAL = ps->input[ps->tok_i].symb;
        YaepSymbol *NEXT_TERMINAL = NULL;

        if (ps->run.grammar->lookahead_level != 0 && ps->tok_i < ps->input_len-1)
        {
            NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
        }

        assert(ps->tok_i == ps->state_set_k);

        if (xmq_verbose_enabled_ && ps->tok_i % 100000 == 0)
        {
            verbose_stats(ps);
        }
        debug("ixml.pa.token=", "@%d %s", ps->tok_i, THE_TERMINAL->hr);
        if (NEXT_TERMINAL && xmq_trace_enabled_)
        {
            yaep_view(ps, "READ %s next %s", THE_TERMINAL->hr, NEXT_TERMINAL->hr);
        }
        else
        {
            yaep_view(ps, "READ %s", THE_TERMINAL->hr);
        }

        YaepStateSet *set = ps->state_sets[ps->state_set_k];
        ps->new_set = NULL;

#ifdef USE_SET_HASH_TABLE
        // This command also adds the set to the hashtable if it does not already exist.
        // As a side effect it writes into ps->new_set
        YaepStateSetTermLookAhead *entry = lookup_cached_set_term_lookahead(ps, THE_TERMINAL, NEXT_TERMINAL, set);
#endif

        if (ps->new_set == NULL)
        {
            YaepCoreSymbToPredComps *core_symb_to_predcomps = core_symb_to_predcomps_find(ps, set->core, THE_TERMINAL);

            if (core_symb_to_predcomps == NULL)
            {
                int c = try_to_recover(ps);
                if (c == 1)
                {
                    continue;
                }
                else if (c == 2)
                {
                    break;
                }
            }

            /*
            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "input[%d]=", ps->tok_i);
                print_symbol(mb, THE_TERMINAL, true);
                membuffer_printf(mb, " s%d core%d -> csl%d", set->id, set->core->id, core_symb_to_predcomps->id);
                debug_mb("ixml.pa.scan=", mb);
                free_membuffer_and_free_content(mb);
                }*/

            // Do the actual predict/complete cycle.
            complete_and_predict_new_state_set(ps, set, core_symb_to_predcomps, THE_TERMINAL, NEXT_TERMINAL);

#ifdef USE_SET_HASH_TABLE
            save_cached_set(ps, entry, NEXT_TERMINAL);
#endif
        }

        ps->state_set_k++;
        ps->state_sets[ps->state_set_k] = ps->new_set;

        if (ps->run.trace)
        {
            MemBuffer *mb = new_membuffer();
            print_state_set(mb, ps, ps->new_set, ps->state_set_k);
            debug_mb("ixml.pa.state=", mb);
            free_membuffer_and_free_content(mb);
        }
    }
    free_error_recovery(ps);

    verbose_stats(ps);
}



static void *parse_alloc_default(int nmemb)
{
    void *result;

    assert(nmemb > 0);

    result = calloc(1, nmemb);
    if (result == NULL)
    {
        exit(1);
    }

    return result;
}

static void parse_free_default(void *mem)
{
    free(mem);
}

static void print_statistics(YaepParseState *ps, bool *ambiguous_p, int table_searches, int table_collisions)
{
    if (ps->run.debug)
    {
        yaep_debug(ps, "print_statistics()");
        yaep_trace(ps, "symbs_os=%zu", objstack_memusage(&ps->run.grammar->symbs_ptr->symbs_os));

        yaep_trace(ps, "input_len=%d #s=%d #dotted_rules=%d",
                   ps->input_len,
                   ps->num_sets_total,
                   ps->num_dotted_rules_total);

        yaep_trace(ps, "#terminals=%d #nonterms=%d %s",
                   ps->run.grammar->symbs_ptr->num_terminals,
                   ps->run.grammar->symbs_ptr->num_nonterminals,
                   *ambiguous_p ? "AMBIGUOUS " : "");

        yaep_trace(ps, "#terminals=%d #nonterms=%d %s",
                   ps->run.grammar->symbs_ptr->num_terminals,
                   ps->run.grammar->symbs_ptr->num_nonterminals,
                   *ambiguous_p ? "AMBIGUOUS " : "");
        yaep_trace(ps, "#rules=%d lengths=%d",
                 ps->run.grammar->rulestorage_ptr->num_rules,
                 ps->run.grammar->rulestorage_ptr->n_rhs_lens);
        yaep_trace(ps,  "#tokens=%d  #unique_dotted_rules=%d",
                   ps->input_len, ps->num_all_dotted_rules);
        yaep_trace(ps, "#terminal_sets=%d size=%d",
                 ps->run.grammar->term_sets_ptr->n_term_sets, ps->run.grammar->term_sets_ptr->n_term_sets_size);
        yaep_trace(ps, "#cores=%d #their start dotted_rules=%d",
                 ps->num_set_cores, ps->num_set_core_start_dotted_rules);
        yaep_trace(ps, "#parent indexes for some non start dotted_rules = %d",
                 ps->num_parent_dotted_rule_ids);
        yaep_trace(ps, "#unique set dist. vects = %d, their length = %d",
                 ps->num_set_matched_lengths, ps->num_set_matched_lengths_len);
        yaep_trace(ps, "#stl=%d goto_successes=%d",
                ps->num_set_term_lookahead, ps->n_goto_successes);
        yaep_trace(ps, "#cspc=%d cspc_vector_lengths=%d",
                 ps->n_core_symb_pairs, ps->n_core_symb_to_predcomps_len);
        yaep_trace(ps, "#unique transition vectors = %d, their length = %d",
                ps->n_transition_vects, ps->n_transition_vect_len);
        yaep_trace(ps, "#unique reduce vectors = %d, their length = %d",
                 ps->n_reduce_vects, ps->n_reduce_vect_len);
        yaep_trace(ps, "#term nodes = %d, #abstract nodes = %d",
                 ps->n_parse_term_nodes, ps->n_parse_abstract_nodes);
        yaep_trace(ps, "#alternative nodes = %d, #all nodes = %d",
                 ps->n_parse_alt_nodes,
                 ps->n_parse_term_nodes + ps->n_parse_abstract_nodes
                 + ps->n_parse_alt_nodes);
        if (table_searches == 0) table_searches++;
        yaep_trace(ps, "#table collisions = %.2g%%(%d out of %d)",
                 table_collisions* 100.0 / table_searches,
                 table_collisions, table_searches);
    }
}
/* The following function parses input according read grammar.
   ONE_PARSE_FLAG means build only one parse tree.  For unambiguous
   grammar the flag does not affect the result.  LA_LEVEL means usage
   of statik(if 1) or dynamic(2) lookahead to decrease size of sets.
   Static lookaheads gives the best results with the point of space
   and speed, dynamic ones does sligthly worse, and no usage of
   lookaheds does the worst.  D_LEVEL says what debugging information
   to output(it works only if we compiled without defined macro
   NO_YAEP_DEBUG_PRINT).  The function returns the error code(which
   will be also in error_code).  The function sets up
   *AMBIGUOUS_P if we found that the grammer is ambigous.
   (It works even we asked only one parse tree without alternatives.) */
int yaepParse(YaepParseRun *pr, YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)pr;

    assert(CHECK_PARSE_STATE_MAGIC(ps));

    ps->run.grammar = g;
    YaepTreeNode **root = &ps->run.root;
    bool *ambiguous_p = &ps->run.ambiguous_p;

    int code;
    bool tok_init_p, parse_init_p;

    /* Set up parse allocation*/
    if (ps->run.parse_alloc == NULL)
    {
        if (ps->run.parse_free != NULL)
        {
            /* Cannot allocate memory with a null function*/
            return YAEP_NO_MEMORY;
        }
        /* Set up defaults*/
        ps->run.parse_alloc = parse_alloc_default;
        ps->run.parse_free = parse_free_default;
    }

    assert(ps->run.grammar != NULL);
    *root = NULL;
    *ambiguous_p = false;
    pl_init(ps);
    tok_init_p = parse_init_p = false;

    if (!ps->run.read_token) ps->run.read_token = default_read_token;

    if ((code = setjmp(ps->error_longjump_buff)) != 0)
    {
        free_state_sets(ps);
        if (parse_init_p)
        {
            free_inside_parse_state(ps);
        }
        if (tok_init_p)
        {
            free_input(ps);
        }
        return code;
    }
    if (g->undefined_p)
    {
        yaep_error(ps, YAEP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
    }
    ps->n_goto_successes = 0;
    create_input(ps);
    tok_init_p = true;
    read_input(ps);
    yaep_parse_init(ps, ps->input_len);
    parse_init_p = true;
    allocate_state_sets(ps);

    int table_collisions_init = get_all_collisions();
    int table_searches_init = get_all_searches();

    // Perform a parse.
    perform_parse(ps);

    // Reconstruct a parse tree from the state sets.
    *root = build_parse_tree(ps, ambiguous_p);

    int table_collisions = get_all_collisions() - table_collisions_init;
    int table_searches = get_all_searches() - table_searches_init;

    print_statistics(ps, ambiguous_p, table_searches, table_collisions);

    free_state_sets(ps);
    free_inside_parse_state(ps);
    free_input(ps);
    verbose("ixml=", "done parse");
    return pr->failed_p;
}

/* The following function frees memory allocated for the grammar.*/
void yaepFreeGrammar(YaepParseRun *pr, YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));

    YaepAllocator *allocator;

    if (g != NULL)
    {
        allocator = g->alloc;
        free_state_sets(ps);
        rulestorage_free(g, g->rulestorage_ptr);
        termsetstorage_free(g, g->term_sets_ptr);
        symbolstorage_free(ps, g->symbs_ptr);
        yaep_free(allocator, g);
        yaep_alloc_del(allocator);
    }
}

static void free_tree_reduce(YaepTreeNode *node)
{
    YaepTreeNodeType type;
    YaepTreeNode **childp;
    size_t numChildren, pos, freePos;

    assert(node != NULL);
    assert((node->type & _yaep_VISITED) == 0);

    type = node->type;
    node->type =(YaepTreeNodeType)(node->type | _yaep_VISITED);

    switch(type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;

    case YAEP_ANODE:
        if (node->val.anode.name[0] == '\0')
        {
            /* We have already seen the node name*/
            node->val.anode.name = NULL;
        }
        else
        {
            /* Mark the node name as seen*/
            node->val._anode_name.name[0] = '\0';
        }
        for(numChildren = 0, childp = node->val.anode.children;
            *childp != NULL; ++numChildren, ++childp)
        {
            if ((*childp)->type & _yaep_VISITED)
            {
               *childp = NULL;
            }
            else
            {
                free_tree_reduce(*childp);
            }
        }
        /* Compactify children array*/
        for(freePos = 0, pos = 0; pos != numChildren; ++pos)
        {
            if (node->val.anode.children[pos] != NULL)
            {
                if (freePos < pos)
                {
                    node->val.anode.children[freePos] =
                        node->val.anode.children[pos];
                    node->val.anode.children[pos] = NULL;
                }
                ++freePos;
            }
        }
        break;

    case YAEP_ALT:
        if (node->val.alt.node->type & _yaep_VISITED)
        {
            node->val.alt.node = NULL;
        }
        else
        {
            free_tree_reduce(node->val.alt.node);
        }
        while((node->val.alt.next != NULL)
               &&(node->val.alt.next->type & _yaep_VISITED))
        {
            assert(node->val.alt.next->type ==(YAEP_ALT | _yaep_VISITED));
            node->val.alt.next = node->val.alt.next->val.alt.next;
        }
        if (node->val.alt.next != NULL)
        {
            assert((node->val.alt.next->type & _yaep_VISITED) == 0);
            free_tree_reduce(node->val.alt.next);
        }
        break;

    default:
        assert("This should not happen" == NULL);
    }
}

static void free_tree_sweep(YaepTreeNode *node,
                            void(*parse_free)(void*),
                            void(*termcb)(YaepTerminalNode*))
{
    YaepTreeNodeType type;
    YaepTreeNode *next;
    YaepTreeNode **childp;

    if (node == NULL)
    {
        return;
    }

    assert(node->type & _yaep_VISITED);
    type =(YaepTreeNodeType)(node->type & ~_yaep_VISITED);

    switch(type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
        break;

    case YAEP_TERM:
        if (termcb != NULL)
        {
            termcb(&node->val.terminal);
        }
        break;

    case YAEP_ANODE:
        parse_free(node->val._anode_name.name);
        for(childp = node->val.anode.children;*childp != NULL; ++childp)
        {
            free_tree_sweep(*childp, parse_free, termcb);
        }
        break;

    case YAEP_ALT:
        free_tree_sweep(node->val.alt.node, parse_free, termcb);
        next = node->val.alt.next;
        parse_free(node);
        free_tree_sweep(next, parse_free, termcb);
        return;                        /* Tail recursion*/

    default:
        assert("This should not happen" == NULL);
    }

    parse_free(node);
}

void yaepFreeTree(YaepTreeNode *root, void (*parse_free)(void*), void (*termcb)(YaepTerminalNode*))
{
    if (root == NULL)
    {
        return;
    }
    if (parse_free == NULL)
    {
        parse_free = parse_free_default;
    }

    /* Since the parse tree is actually a DAG, we must carefully avoid
     double free errors. Therefore, we walk the parse tree twice.
     On the first walk, we reduce the DAG to an actual tree.
     On the second walk, we recursively free the tree nodes. */
    free_tree_reduce(root);
    free_tree_sweep(root, parse_free, termcb);
}

static int default_read_token(YaepParseRun *ps, void **attr)
{
    *attr = NULL;
    if (ps->buffer_i >= ps->buffer_stop) return -1;

    int uc = 0;
    size_t len = 0;
    bool ok = decode_utf8(ps->buffer_i, ps->buffer_stop, &uc, &len);
    if (!ok)
    {
        fprintf(stderr, "xmq: broken utf8\n");
        exit(1);
    }
    ps->buffer_i += len;

    return uc;
}

/* The following function is major function of syntax error recovery.
   It searches for minimal cost error recovery.  The function returns
   in the parameter number of start token which is ignored and number
   of the first token which is not ignored.  If the number of ignored
   tokens is zero,*START will be equal to*STOP and number of token
   on which the error occurred.*/
static void error_recovery(YaepParseState *ps, int *start, int *stop)
{
    YaepStateSet*set;
    YaepCoreSymbToPredComps*core_symb_to_predcomps;
    YaepRecoveryState best_state, state;
    int best_cost, cost, num_matched_input;
    int back_to_frontier_move_cost, backward_move_cost;


    if (ps->run.verbose)
        fprintf(stderr, "\n++Error recovery start\n");

   *stop =*start = -1;
    OS_CREATE(ps->recovery_state_tail_sets, ps->run.grammar->alloc, 0);
    VLO_NULLIFY(ps->original_state_set_tail_stack);
    VLO_NULLIFY(ps->recovery_state_stack);
    ps->recovery_start_set_k = ps->state_set_k;
    ps->recovery_start_tok_i = ps->tok_i;
    /* Initialize error recovery state stack.*/
    ps->state_set_k = ps->back_state_set_frontier = find_error_state_set_set(ps, ps->state_set_k, &backward_move_cost);
    back_to_frontier_move_cost = backward_move_cost;
    save_original_sets(ps);
    push_recovery_state(ps, ps->back_state_set_frontier, backward_move_cost);
    best_cost = 2* ps->input_len;
    while(VLO_LENGTH(ps->recovery_state_stack) > 0)
    {
        state = pop_recovery_state(ps);
        cost = state.backward_move_cost;
        assert(cost >= 0);
        /* Advance back frontier.*/
        if (ps->back_state_set_frontier > 0)
        {
            int saved_state_set_k = ps->state_set_k;
            int saved_tok_i = ps->tok_i;

            /* Advance back frontier.*/
            ps->state_set_k = find_error_state_set_set(ps, ps->back_state_set_frontier - 1,
                                         &backward_move_cost);

            if (ps->run.debug)
                fprintf(stderr, "++++Advance back frontier: old=%d, new=%d\n",
                         ps->back_state_set_frontier, ps->state_set_k);

            if (best_cost >= back_to_frontier_move_cost + backward_move_cost)
            {
                ps->back_state_set_frontier = ps->state_set_k;
                ps->tok_i = ps->recovery_start_tok_i;
                save_original_sets(ps);
                back_to_frontier_move_cost += backward_move_cost;
                push_recovery_state(ps, ps->back_state_set_frontier,
                                    back_to_frontier_move_cost);
                set_original_set_bound(ps, state.last_original_state_set_el);
                ps->tok_i = saved_tok_i;
            }
            ps->state_set_k = saved_state_set_k;
        }
        /* Advance head frontier.*/
        if (best_cost >= cost + 1)
        {
            ps->tok_i++;
            if (ps->tok_i < ps->input_len)
            {

                if (ps->run.debug)
                {
                    fprintf(stderr,
                             "++++Advance head frontier(one pos): tok=%d, ",
                             ps->tok_i);
                    // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
                    fprintf(stderr, "\n");

                }
                push_recovery_state(ps, state.last_original_state_set_el, cost + 1);
            }
            ps->tok_i--;
        }
        set = ps->state_sets[ps->state_set_k];

        if (ps->run.debug)
        {
            fprintf(stderr, "++++Trying set=%d, tok=%d, ", ps->state_set_k, ps->tok_i);
            // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
            fprintf(stderr, "\n");
        }

        /* Shift error:*/
        core_symb_to_predcomps = core_symb_to_predcomps_find(ps, set->core, ps->run.grammar->term_error);
        assert(core_symb_to_predcomps != NULL);

        if (ps->run.debug)
            fprintf(stderr, "++++Making error shift in set=%d\n", ps->state_set_k);

        complete_and_predict_new_state_set(ps, set, core_symb_to_predcomps, NULL, NULL);
        ps->state_sets[++ps->state_set_k] = ps->new_set;

        if (ps->run.debug)
        {
            fprintf(stderr, "++Trying new set=%d\n", ps->state_set_k);
            // print_state_set(ps, stderr, ps->new_set, ps->state_set_k, ps->run.debug, ps->run.debug);
            fprintf(stderr, "\n");
        }

        /* Search the first right token.*/
        while(ps->tok_i < ps->input_len)
        {
            core_symb_to_predcomps = core_symb_to_predcomps_find(ps, ps->new_core, ps->input[ps->tok_i].symb);
            if (core_symb_to_predcomps != NULL)
                break;

            if (ps->run.debug)
            {
                fprintf(stderr, "++++++Skipping=%d ", ps->tok_i);
                // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
                fprintf(stderr, "\n");
            }

            cost++;
            ps->tok_i++;
            if (cost >= best_cost)
            {
                /* This state is worse.  Reject it.*/
                break;
            }
        }
        if (cost >= best_cost)
        {

            if (ps->run.debug)
            {
                fprintf(stderr, "++++Too many ignored tokens %d(already worse recovery)\n", cost);
            }

            /* This state is worse.  Reject it.*/
            continue;
        }
        if (ps->tok_i >= ps->input_len)
        {

            if (ps->run.debug)
            {
                fprintf(stderr, "++++We achieved EOF without matching -- reject this state\n");
            }

            /* Go to the next recovery state.  To guarantee that state set does
               not grows to much we don't push secondary error recovery
               states without matching in primary error recovery state.
               So we can say that state set length at most twice length of
               tokens array.*/
            continue;
        }

        /* Shift the found token.*/
        YaepSymbol *NEXT_TERMINAL = NULL;
        if (ps->tok_i + 1 < ps->input_len)
        {
            NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
        }
        complete_and_predict_new_state_set(ps, ps->new_set, core_symb_to_predcomps, NULL, NEXT_TERMINAL);
        ps->state_sets[++ps->state_set_k] = ps->new_set;

        if (ps->run.debug)
        {
            fprintf(stderr, "++++++++Building new set=%d\n", ps->state_set_k);
            if (ps->run.debug)
            {
                // print_state_set(ps, stderr, ps->new_set, ps->state_set_k, ps->run.debug, ps->run.debug);
            }
        }

        num_matched_input = 0;
        for(;;)
        {

            if (ps->run.debug)
            {
                fprintf(stderr, "++++++Matching=%d ", ps->tok_i);
                // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
                fprintf(stderr, "\n");
            }

            num_matched_input++;
            if (num_matched_input >= ps->run.grammar->recovery_token_matches)
            {
                break;
            }
            ps->tok_i++;
            if (ps->tok_i >= ps->input_len)
            {
                break;
            }
            /* Push secondary recovery state(with error in set).*/
            if (core_symb_to_predcomps_find(ps, ps->new_core, ps->run.grammar->term_error) != NULL)
            {
                if (ps->run.debug)
                {
                    fprintf(stderr, "++++Found secondary state: original set=%d, tok=%d, ",
                            state.last_original_state_set_el, ps->tok_i);
                    // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
                    fprintf(stderr, "\n");
                }

                push_recovery_state(ps, state.last_original_state_set_el, cost);
            }
            core_symb_to_predcomps = core_symb_to_predcomps_find(ps, ps->new_core, ps->input[ps->tok_i].symb);
            if (core_symb_to_predcomps == NULL)
            {
                break;
            }
            YaepSymbol *NEXT_TERMINAL = NULL;
            if (ps->tok_i + 1 < ps->input_len)
            {
                NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
            }
            complete_and_predict_new_state_set(ps, ps->new_set, core_symb_to_predcomps, NULL, NEXT_TERMINAL);
            ps->state_sets[++ps->state_set_k] = ps->new_set;
        }
        if (num_matched_input >= ps->run.grammar->recovery_token_matches || ps->tok_i >= ps->input_len)
        {
            /* We found an error recovery.  Compare costs.*/
            if (best_cost > cost)
            {

                if (ps->run.debug)
                {
                    fprintf(stderr, "++++Ignore %d tokens(the best recovery now): Save it:\n", cost);
                }
                best_cost = cost;
                if (ps->tok_i == ps->input_len)
                {
                    ps->tok_i--;
                }
                best_state = new_recovery_state(ps, state.last_original_state_set_el,
                                                 /* It may be any constant here
                                                    because it is not used.*/
                                                 0);
               *start = ps->recovery_start_tok_i - state.backward_move_cost;
               *stop = *start + cost;
            }
            else if (ps->run.debug)
            {
                fprintf(stderr, "++++Ignore %d tokens(worse recovery)\n", cost);
            }
        }
        else if (cost < best_cost && ps->run.debug)
            fprintf(stderr, "++++No %d matched tokens  -- reject this state\n",
                     ps->run.grammar->recovery_token_matches);

    }

    if (ps->run.debug)
        fprintf(stderr, "\n++Finishing error recovery: Restore best state\n");

    set_recovery_state(ps, &best_state);

    if (ps->run.debug)
    {
        fprintf(stderr, "\n++Error recovery end: curr token %d=", ps->tok_i);
        // TODO symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, ", Current set=%d:\n", ps->state_set_k);
        if (ps->run.debug)
        {
            /*print_state_set(ps, stderr, ps->state_sets[ps->state_set_k],
              ps->state_set_k, ps->run.debug, ps->run.debug);*/
        }
    }

    OS_DELETE(ps->recovery_state_tail_sets);
}

/* Initialize work with error recovery.*/
static void error_recovery_init(YaepParseState *ps)
{
    VLO_CREATE(ps->original_state_set_tail_stack, ps->run.grammar->alloc, 4096);
    VLO_CREATE(ps->recovery_state_stack, ps->run.grammar->alloc, 4096);
}

/* Finalize work with error recovery.*/
static void free_error_recovery(YaepParseState *ps)
{
    VLO_DELETE(ps->recovery_state_stack);
    VLO_DELETE(ps->original_state_set_tail_stack);
}

#endif // YAEP_MODULE
