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
static void read_input(YaepParseState *ps);
static void set_add_dotted_rule_with_matched_length(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, const char *why);
static void set_add_dotted_rule_no_match_yet(YaepParseState *ps, YaepDottedRule *dotted_rule, const char *why);
static void set_add_dotted_rule_with_parent(YaepParseState *ps, YaepDottedRule *dotted_rule, int parent_dotted_rule_id, const char *why);
static bool convert_leading_dotted_rules_into_new_set(YaepParseState *ps);
static void prepare_for_leading_dotted_rules(YaepParseState *ps);
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

static unsigned stateset_core_term_lookahead_hash(YaepStateSetCoreTermLookAhead *s)
{
    YaepStateSet *set = s->set;
    YaepSymbol *term = s->term;
    int lookahead = s->lookahead;

    return ((stateset_core_matched_lengths_hash(set)* hash_shift + term->u.terminal.term_id)* hash_shift + lookahead);
}

static bool stateset_core_term_lookahead_eq(YaepStateSetCoreTermLookAhead *s1, YaepStateSetCoreTermLookAhead *s2)
{
    YaepStateSet *set1 = s1->set;
    YaepStateSet *set2 = s2->set;
    YaepSymbol *term1 = s1->term;
    YaepSymbol *term2 = s2->term;
    int lookahead1 = s1->lookahead;
    int lookahead2 = s2->lookahead;

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
    OS_CREATE(ps->triplet_core_term_lookahead_os, ps->run.grammar->alloc, 0);

    ps->cache_stateset_cores = create_hash_table(ps->run.grammar->alloc, 2000,
                                                 (hash_table_hash_function)stateset_core_hash,
                                                 (hash_table_eq_function)stateset_core_eq);

    ps->cache_stateset_matched_lengths = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                                           (hash_table_hash_function)matched_lengths_hash,
                                                           (hash_table_eq_function)matched_lengths_eq);

    ps->cache_stateset_core_matched_lengths = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                                                (hash_table_hash_function)stateset_core_matched_lengths_hash,
                                                                (hash_table_eq_function)stateset_core_matched_lengths_eq);

    ps->cache_stateset_core_term_lookahead = create_hash_table(ps->run.grammar->alloc, n < 30000 ? 30000 : n,
                                                               (hash_table_hash_function)stateset_core_term_lookahead_hash,
                                                               (hash_table_eq_function)stateset_core_term_lookahead_eq);

    ps->num_set_cores = ps->num_set_core_start_dotted_rules= 0;
    ps->num_set_matched_lengths = ps->num_set_matched_lengths_len = ps->num_parent_dotted_rule_ids = 0;
    ps->num_sets_total = ps->num_dotted_rules_total= 0;
    ps->num_triplets_core_term_lookahead = 0;
    dotted_rule_matched_length_set_init(ps);
}

static void debug_step(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length, int parent_id)
{
    if (!ps->run.debug) return;

    /*
    if (blocked)
    {
        MemBuffer *mb = new_membuffer();
        membuffer_printf(mb, "step @%d %s", ps->tok_i, why);
        debug_mb("ixml.pa=", mb);
        free_membuffer_and_free_content(mb);
        return;
        }*/

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "step @%d ", ps->tok_i);

    // We add one to the state_set_k because the index is updated at the end of the parse loop.
    int k = 1+ps->state_set_k;
    if (k < 0) k = 0;

    print_dotted_rule(mb, ps, k, dotted_rule, matched_length, parent_id);
    debug_mb("ixml.pa=", mb);
    free_membuffer_and_free_content(mb);

}

static void debug_info(YaepParseState *ps, const char *format, ...)
{
    if (!ps->run.debug) return;

    va_list ap;
    va_start(ap, format);

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "info @%d ", ps->tok_i);

    char *buf = buf_vsnprintf(format, ap);
    membuffer_append(mb, buf);
    membuffer_append_null(mb);
    free(buf);

    debug_mb("ixml.pa=", mb);
    free_membuffer_and_free_content(mb);
    return;
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

    debug_info(ps, "add leading d%d len %d (%s)", dotted_rule->id, matched_length, why);
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
            //debug_info(ps, "skip d%", dotted_rule->id);
            return;
        }
    }
    /* Remember we do not store matched_length for not-yet-started dotted_rules. */
    append_dotted_rule_to_core(ps, dotted_rule);

    debug_info(ps, "add d%d to c%d", dotted_rule->id, ps->new_core->id);
    debug_step(ps, dotted_rule, 0, -1);
}

/* Add nonstart, noninitial SIT with distance DIST at the end of the
   situation array of the set being formed.  If this is situation and
   there is already the same pair (situation, the corresponding
   distance), we do not add it.
   set_add_new_nonstart_sit (struct sit *sit, int parent)
*/
static void set_add_dotted_rule_with_parent(YaepParseState *ps, YaepDottedRule *dotted_rule, int parent_dotted_rule_id, const char *why)
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
            ps->new_core->parent_dotted_rule_ids[rule_index_in_core] == parent_dotted_rule_id)
        {
            // The dotted_rule + parent dotted rule already exists.
            debug_info(ps, "reusing d%d with parent d%d", dotted_rule->id, parent_dotted_rule_id);
            return;
        }
    }

    // Increase the object stack storing dotted_rules, with the size of a new dotted_rule.
    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_dotted_rules = ps->new_core->dotted_rules = (YaepDottedRule**)OS_TOP_BEGIN(ps->set_dotted_rules_os);

    // Increase the parent index vector with another int.
    // This integer points to ...?
    OS_TOP_EXPAND(ps->set_parent_dotted_rule_ids_os, sizeof(int));
    ps->new_core->parent_dotted_rule_ids = (int*)OS_TOP_BEGIN(ps->set_parent_dotted_rule_ids_os) - ps->new_num_leading_dotted_rules;

    // Store dotted_rule into new dotted_rules.
    ps->new_dotted_rules[ps->new_core->num_dotted_rules++] = dotted_rule;
    // Store parent index. Meanst what...?
    ps->new_core->parent_dotted_rule_ids[ps->new_core->num_all_matched_lengths++] = parent_dotted_rule_id;
    ps->num_parent_dotted_rule_ids++;

    debug_info(ps, "add d%d with parent d%d to c%d", dotted_rule->id, parent_dotted_rule_id, ps->new_core->id);

    debug_step(ps, dotted_rule, 0, parent_dotted_rule_id);
}

/* Set up hash of matched_lengths of set S. */
static void setup_set_matched_lengths_hash(hash_table_entry_t s)
{
    YaepStateSet *set = (YaepStateSet*)s;

    int num_matched_lengths = set->core->num_started_dotted_rules;
    unsigned result = jauquet_prime_mod32;

    int *i = set->matched_lengths;
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

    debug_info(ps, "start collecting leading rules");
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

    debug_info(ps, "convert leading rules into s%d", ps->new_set->id);

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
            debug_info(ps, "re-using matched lengths %s", mb->buffer_);
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
            debug_info(ps, "new matched lengths (%s)", mb->buffer_);
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
            debug_info(ps, "re-using %s", mb->buffer_);
            free_membuffer_and_free_content(mb);
        }
    }
    else
    {
        OS_TOP_FINISH(ps->set_cores_os);
        ps->new_core->id = ps->num_set_cores++;
        ps->new_core->num_dotted_rules = ps->new_num_leading_dotted_rules;
        ps->new_core->num_all_matched_lengths = ps->new_num_leading_dotted_rules;
        ps->new_core->parent_dotted_rule_ids = NULL;
        *sc = ps->new_set;
        ps->num_set_core_start_dotted_rules += ps->new_num_leading_dotted_rules;
        added = true;

        if (xmq_trace_enabled_)
        {
            MemBuffer *mb = new_membuffer();
            print_core(mb, ps->new_set->core);
            membuffer_append_null(mb);
            debug_info(ps, "new %s", mb->buffer_);
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
        debug_info(ps, "new s%d", ps->new_set->id);
    }
    else
    {
        ps->new_set = *scm;
        OS_TOP_NULLIFY(ps->sets_os);
        debug_info(ps, "re-using s%d", ps->new_set->id);
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
    delete_hash_table(ps->cache_stateset_core_term_lookahead);
    delete_hash_table(ps->cache_stateset_core_matched_lengths);
    delete_hash_table(ps->cache_stateset_matched_lengths);
    delete_hash_table(ps->cache_stateset_cores);
    OS_DELETE(ps->triplet_core_term_lookahead_os);
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

/* Initialize work with array of vlos.*/
static void vlo_array_init(YaepParseState *ps)
{
    VLO_CREATE(ps->vlo_array, ps->run.grammar->alloc, 4096);
    ps->vlo_array_len = 0;
}

/* The function forms new empty vlo at the end of the array of vlos.*/
static int vlo_array_expand(YaepParseState *ps)
{
    vlo_t*vlo_ptr;

    if ((unsigned) ps->vlo_array_len >= VLO_LENGTH(ps->vlo_array) / sizeof(vlo_t))
    {
        VLO_EXPAND(ps->vlo_array, sizeof(vlo_t));
        vlo_ptr = &((vlo_t*) VLO_BEGIN(ps->vlo_array))[ps->vlo_array_len];
        VLO_CREATE(*vlo_ptr, ps->run.grammar->alloc, 64);
    }
    else
    {
        vlo_ptr = &((vlo_t*) VLO_BEGIN(ps->vlo_array))[ps->vlo_array_len];
        VLO_NULLIFY(*vlo_ptr);
    }
    return ps->vlo_array_len++;
}

/* The function purges the array of vlos.*/
static void vlo_array_nullify(YaepParseState *ps)
{
    ps->vlo_array_len = 0;
}

/* The following function returns pointer to vlo with INDEX.*/
static vlo_t *vlo_array_el(YaepParseState *ps, int index)
{
    assert(index >= 0 && ps->vlo_array_len > index);
    return &((vlo_t*) VLO_BEGIN(ps->vlo_array))[index];
}

static void free_vlo_array(YaepParseState *ps)
{
    vlo_t *vlo_ptr;

    for (vlo_ptr = (vlo_t*)VLO_BEGIN(ps->vlo_array); (char*) vlo_ptr < (char*) VLO_BOUND(ps->vlo_array); vlo_ptr++)
    {
        VLO_DELETE(*vlo_ptr);
    }
    VLO_DELETE(ps->vlo_array);
}

#ifdef USE_CORE_SYMB_HASH_TABLE
/* Hash of core_symb_to_predcomps.*/
static unsigned core_symb_to_predcomps_hash(YaepCoreSymbToPredComps *core_symb_to_predcomps)
{
    return (jauquet_prime_mod32* hash_shift+(size_t)/* was unsigned */core_symb_to_predcomps->core) * hash_shift
        +(size_t)/* was unsigned */core_symb_to_predcomps->symb;
}

/* Equality of core_symb_to_predcomps.*/
static bool core_symb_to_predcomps_eq(YaepCoreSymbToPredComps *core_symb_to_predcomps1, YaepCoreSymbToPredComps *core_symb_to_predcomps2)
{
    return core_symb_to_predcomps1->core == core_symb_to_predcomps2->core && core_symb_to_predcomps1->symb == core_symb_to_predcomps2->symb;
}
#endif

static unsigned vect_ids_hash(YaepVect*v)
{
    unsigned result = jauquet_prime_mod32;

    for (int i = 0; i < v->len; i++)
    {
        result = result* hash_shift + v->ids[i];
    }
    return result;
}

static bool vect_ids_eq(YaepVect *v1, YaepVect *v2)
{
    if (v1->len != v2->len) return false;

    for (int i = 0; i < v1->len; i++)
    {
        if (v1->ids[i] != v2->ids[i]) return false;
    }
    return true;
}

static unsigned prediction_ids_hash(hash_table_entry_t t)
{
    return vect_ids_hash(&((YaepCoreSymbToPredComps*)t)->predictions);
}

static bool prediction_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbToPredComps*)t1)->predictions,
                       &((YaepCoreSymbToPredComps*)t2)->predictions);
}

static unsigned completion_ids_hash(hash_table_entry_t t)
{
    return vect_ids_hash(&((YaepCoreSymbToPredComps*)t)->completions);
}

static bool completion_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbToPredComps*) t1)->completions,
                       &((YaepCoreSymbToPredComps*) t2)->completions);
}

/* Initialize work with the triples(set core, symbol, vector).*/
static void core_symb_to_predcomps_init(YaepParseState *ps)
{
    OS_CREATE(ps->core_symb_to_predcomps_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->new_core_symb_to_predcomps_vlo, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->vect_ids_os, ps->run.grammar->alloc, 0);

    vlo_array_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    ps->map_core_symb_to_predcomps = create_hash_table(ps->run.grammar->alloc, 3000,
                                                       (hash_table_hash_function)core_symb_to_predcomps_hash,
                                                       (hash_table_eq_function)core_symb_to_predcomps_eq);
#else
    VLO_CREATE(ps->core_symb_table_vlo, ps->run.grammar->alloc, 4096);
    ps->core_symb_table = (YaepCoreSymbToPredComps***)VLO_BEGIN(ps->core_symb_table_vlo);
    OS_CREATE(ps->core_symb_tab_rows, ps->run.grammar->alloc, 8192);
#endif

    ps->map_transition_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000,
                                                           (hash_table_hash_function)prediction_ids_hash,
                                                           (hash_table_eq_function)prediction_ids_eq);

    ps->map_reduce_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000,
                                                       (hash_table_hash_function)completion_ids_hash,
                                                       (hash_table_eq_function)completion_ids_eq);

    ps->n_core_symb_pairs = ps->n_core_symb_to_predcomps_len = 0;
    ps->n_transition_vects = ps->n_transition_vect_len = 0;
    ps->n_reduce_vects = ps->n_reduce_vect_len = 0;
}

#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed.*/
static YaepCoreSymbToPredComps **core_symb_to_pred_comps_addr_get(YaepParseState *ps, YaepCoreSymbToPredComps *triple, int reserv_p)
{
    YaepCoreSymbToPredComps **result;

    if (triple->symb->cached_core_symb_to_predcomps != NULL
        && triple->symb->cached_core_symb_to_predcomps->core == triple->core)
    {
        return &triple->symb->cached_core_symb_to_predcomps;
    }

    result = ((YaepCoreSymbToPredComps**)find_hash_table_entry(ps->map_core_symb_to_predcomps, triple, reserv_p));

    triple->symb->cached_core_symb_to_predcomps = *result;

    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed. */
static YaepCoreSymbToPredComps **core_symb_to_predcomps_addr_get(YaepParseState *ps, YaepStateSetCore *set_core, YaepSymbol *symb)
{
    YaepCoreSymbToPredComps***core_symb_to_predcomps_ptr;

    core_symb_to_predcomps_ptr = ps->core_symb_table + set_core->id;

    if ((char*) core_symb_to_predcomps_ptr >=(char*) VLO_BOUND(ps->core_symb_table_vlo))
    {
        YaepCoreSymbToPredComps***ptr,***bound;
        int diff, i;

        diff =((char*) core_symb_to_predcomps_ptr
                -(char*) VLO_BOUND(ps->core_symb_table_vlo));
        diff += sizeof(YaepCoreSymbToPredComps**);
        if (diff == sizeof(YaepCoreSymbToPredComps**))
            diff*= 10;

        VLO_EXPAND(ps->core_symb_table_vlo, diff);
        ps->core_symb_table = (YaepCoreSymbToPredComps***) VLO_BEGIN(ps->core_symb_table_vlo);
        core_symb_to_predcomps_ptr = ps->core_symb_table + set_core->id;
        bound =(YaepCoreSymbToPredComps***) VLO_BOUND(ps->core_symb_table_vlo);

        ptr = bound - diff / sizeof(YaepCoreSymbToPredComps**);
        while(ptr < bound)
        {
            OS_TOP_EXPAND(ps->core_symb_tab_rows,
                          (ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals)
                          * sizeof(YaepCoreSymbToPredComps*));
           *ptr =(YaepCoreSymbToPredComps**) OS_TOP_BEGIN(ps->core_symb_tab_rows);
            OS_TOP_FINISH(ps->core_symb_tab_rows);
            for(i = 0; i < ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals; i++)
               (*ptr)[i] = NULL;
            ptr++;
        }
    }
    return &(*core_symb_to_predcomps_ptr)[symb->id];
}
#endif

/* The following function returns the triple(if any) for given SET_CORE and SYMB. */
static YaepCoreSymbToPredComps *core_symb_to_predcomps_find(YaepParseState *ps, YaepStateSetCore *core, YaepSymbol *symb)
{
    YaepCoreSymbToPredComps *r;

#ifdef USE_CORE_SYMB_HASH_TABLE
    YaepCoreSymbToPredComps core_symb_to_predcomps;

    core_symb_to_predcomps.core = core;
    core_symb_to_predcomps.symb = symb;
    r = *core_symb_to_pred_comps_addr_get(ps, &core_symb_to_predcomps, false);
#else
    r = *core_symb_to_predcomps_addr_get(ps, core, symb);
#endif

    return r;
}

static YaepCoreSymbToPredComps *core_symb_to_predcomps_new(YaepParseState *ps, YaepStateSetCore*core, YaepSymbol*symb)
{
    YaepCoreSymbToPredComps*core_symb_to;
    YaepCoreSymbToPredComps**addr;
    vlo_t*vlo_ptr;

    /* Create table element.*/
    OS_TOP_EXPAND(ps->core_symb_to_predcomps_os, sizeof(YaepCoreSymbToPredComps));
    core_symb_to = ((YaepCoreSymbToPredComps*) OS_TOP_BEGIN(ps->core_symb_to_predcomps_os));
    core_symb_to->id = ps->core_symb_to_pred_comps_counter++;
    core_symb_to->core = core;
    core_symb_to->symb = symb;
    OS_TOP_FINISH(ps->core_symb_to_predcomps_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_to_pred_comps_addr_get(ps, core_symb_to, true);
#else
    addr = core_symb_to_pred_comps_addr_get(ps, core, symb);
#endif
    assert(*addr == NULL);
   *addr = core_symb_to;

    core_symb_to->predictions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, core_symb_to->predictions.intern);
    core_symb_to->predictions.len = 0;
    core_symb_to->predictions.ids =(int*) VLO_BEGIN(*vlo_ptr);

    core_symb_to->completions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, core_symb_to->completions.intern);
    core_symb_to->completions.len = 0;
    core_symb_to->completions.ids =(int*) VLO_BEGIN(*vlo_ptr);
    VLO_ADD_MEMORY(ps->new_core_symb_to_predcomps_vlo, &core_symb_to,
                    sizeof(YaepCoreSymbToPredComps*));
    ps->n_core_symb_pairs++;

    return core_symb_to;
}

static void vect_add_id(YaepParseState *ps, YaepVect *vec, int id)
{
    vec->len++;
    vlo_t *vlo_ptr = vlo_array_el(ps, vec->intern);
    VLO_ADD_MEMORY(*vlo_ptr, &id, sizeof(int));
    vec->ids =(int*) VLO_BEGIN(*vlo_ptr);
    ps->n_core_symb_to_predcomps_len++;
}

static void log_dotted_rule(YaepParseState *ps, int lookahead_term_id, YaepDottedRule *new_dotted_rule)
{
    /*
    MemBuffer *mb = new_membuffer();
    YaepSymbol *symb = symb_find_by_term_id(ps, lookahead_term_id);
    const char *hr = symb->hr;
    if (!symb) hr = "?";
    membuffer_printf(mb, "lookahead (%d) %s blocked by ", lookahead_term_id, hr);
    print_dotted_rule(mb, ps, ps->tok_i-1, new_dotted_rule, 0, 0, "woot1");
    membuffer_append(mb, "\n");
    debug_mb("ixml.pa.c=", mb);*/
}

static void core_symb_to_predcomps_add_predict(YaepParseState *ps,
                                      YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                      int rule_index_in_core)
{
    vect_add_id(ps, &core_symb_to_predcomps->predictions, rule_index_in_core);

    YaepDottedRule *dotted_rule = core_symb_to_predcomps->core->dotted_rules[rule_index_in_core];
    debug_info(ps, "add prediction cspc%d[c%d %s] -> d%d",
               core_symb_to_predcomps->id,
               core_symb_to_predcomps->core->id,
               core_symb_to_predcomps->symb->hr,
               dotted_rule->id);
}

static void core_symb_to_predcomps_add_complete(YaepParseState *ps,
                                       YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                       int rule_index_in_core)
{
    vect_add_id(ps, &core_symb_to_predcomps->completions, rule_index_in_core);
    YaepDottedRule *dotted_rule = core_symb_to_predcomps->core->dotted_rules[rule_index_in_core];
    debug_info(ps, "completed d%d store in cspc%d[c%d %s]",
               dotted_rule->id,
               core_symb_to_predcomps->id,
               core_symb_to_predcomps->core->id,
               core_symb_to_predcomps->symb->hr
               );

}

/* Insert vector VEC from CORE_SYMB_TO_PREDCOMPS into table TAB.  Update
   *N_VECTS and INT*N_VECT_LEN if it is a new vector in the table. */
static void process_core_symb_to_predcomps_el(YaepParseState *ps,
                                      YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                      YaepVect *vec,
                                      hash_table_t *tab,
                                      int *n_vects,
                                      int *n_vect_len)
{
    hash_table_entry_t*entry;

    if (vec->len == 0)
    {
        vec->ids = NULL;
    }
    else
    {
        entry = find_hash_table_entry(*tab, core_symb_to_predcomps, true);
        if (*entry != NULL)
        {
            vec->ids = (&core_symb_to_predcomps->predictions == vec
                        ?((YaepCoreSymbToPredComps*)*entry)->predictions.ids
                        :((YaepCoreSymbToPredComps*)*entry)->completions.ids);
        }
        else
        {
            *entry = (hash_table_entry_t)core_symb_to_predcomps;
            OS_TOP_ADD_MEMORY(ps->vect_ids_os, vec->ids, vec->len* sizeof(int));
            vec->ids =(int*) OS_TOP_BEGIN(ps->vect_ids_os);
            OS_TOP_FINISH(ps->vect_ids_os);
            (*n_vects)++;
            *n_vect_len += vec->len;
        }
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_to_predcomps.*/
static void core_symb_to_predcomps_new_all_stop(YaepParseState *ps)
{
    YaepCoreSymbToPredComps**triple_ptr;

    for(triple_ptr =(YaepCoreSymbToPredComps**) VLO_BEGIN(ps->new_core_symb_to_predcomps_vlo);
        (char*) triple_ptr <(char*) VLO_BOUND(ps->new_core_symb_to_predcomps_vlo);
         triple_ptr++)
    {
        process_core_symb_to_predcomps_el(ps, *triple_ptr, &(*triple_ptr)->predictions,
                                  &ps->map_transition_to_coresymbvect, &ps->n_transition_vects,
                                  &ps->n_transition_vect_len);
        process_core_symb_to_predcomps_el(ps, *triple_ptr, &(*triple_ptr)->completions,
                                  &ps->map_reduce_to_coresymbvect, &ps->n_reduce_vects,
                                  &ps->n_reduce_vect_len);
    }
    vlo_array_nullify(ps);
    VLO_NULLIFY(ps->new_core_symb_to_predcomps_vlo);
}

/* Finalize work with all triples(set core, symbol, vector).*/
static void free_core_symb_to_vect_lookup(YaepParseState *ps)
{
    delete_hash_table(ps->map_transition_to_coresymbvect);
    delete_hash_table(ps->map_reduce_to_coresymbvect);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table(ps->map_core_symb_to_predcomps);
#else
    OS_DELETE(ps->core_symb_tab_rows);
    VLO_DELETE(ps->core_symb_table_vlo);
#endif
    free_vlo_array(ps);
    OS_DELETE(ps->vect_ids_os);
    VLO_DELETE(ps->new_core_symb_to_predcomps_vlo);
    OS_DELETE(ps->core_symb_to_predcomps_os);
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
        membuffer_printf(mb, "(s%d,-) %s →  ε not[%d-%d/%d] ",
                         1+ps->state_set_k,
                         symb->repr, ps->tok_i, ps->tok_i+1, ps->input_len);
        membuffer_printf(mb, "{%s", info);
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
        membuffer_printf(mb, "}");
        membuffer_append_null(mb);

        debug_info(ps, mb->buffer_);

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
    YaepRule *rule;

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

            debug_info(ps, "lookup cspc? [c%d %s]", ps->new_core->id, symb->hr);

            if (core_symb_to_predcomps)
            {
                debug_info(ps, "found cspc%d[c%d %s]", core_symb_to_predcomps->id, ps->new_core->id, symb->hr);
            }
            else
            {
                // No vector found for this core+symb combo.
                // Add a new vector.
                core_symb_to_predcomps = core_symb_to_predcomps_new(ps, ps->new_core, symb);
                debug_info(ps, "new cspc%d [c%d %s]", core_symb_to_predcomps->id, ps->new_core->id, symb->hr);

                if (!symb->is_terminal)
                {
                    for (YaepRule *r = symb->u.nonterminal.rules; r != NULL; r = r->lhs_next)
                    {
                        YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, r, 0, 0);
                        debug_info(ps, "d%d.%d predicts %s", dotted_rule->id, dotted_rule->dot_j, r->lhs->hr);
                        set_add_dotted_rule_no_match_yet(ps, new_dotted_rule, "predict");
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
                    debug_info(ps, "complete empty rule %s", dotted_rule->rule->lhs->hr);
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
                    debug_info(ps, "complete lookahead ok");
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
        }
        core_symb_to_predcomps_add_complete(ps, core_symb_to_predcomps, rule_index_in_core);
    }

    if (ps->run.grammar->lookahead_level > 1)
    {
        YaepDottedRule *new_dotted_rule, *shifted_dotted_rule;
        terminal_bitset_t *dyn_lookahead_context_set;
        int dotted_rule_id, dyn_lookahead_context, j;
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

static int lookup_matched_length(YaepParseState *ps, YaepStateSet *set, int dotted_rule_id)
{
    if (dotted_rule_id >= set->core->num_all_matched_lengths)
    {
        return 0;
    }
    if (dotted_rule_id < set->core->num_started_dotted_rules)
    {
        return set->matched_lengths[dotted_rule_id];
    }
    return set->matched_lengths[set->core->parent_dotted_rule_ids[dotted_rule_id]];
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
    if (local_lookahead_level != 0 &&
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
static bool check_cached_transition_set(YaepParseState *ps, YaepStateSet*set, int place)
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

static YaepStateSetCoreTermLookAhead *lookup_cached_core_term_lookahead(YaepParseState *ps,
                                                                        YaepSymbol *THE_TERMINAL,
                                                                        YaepSymbol *NEXT_TERMINAL,
                                                                        YaepStateSet *set)
{
    int i;
    hash_table_entry_t *entry;
    YaepStateSetCoreTermLookAhead *new_core_term_lookahead;

    OS_TOP_EXPAND(ps->triplet_core_term_lookahead_os, sizeof(YaepStateSetCoreTermLookAhead));

    new_core_term_lookahead = (YaepStateSetCoreTermLookAhead*) OS_TOP_BEGIN(ps->triplet_core_term_lookahead_os);
    new_core_term_lookahead->set = set;
    new_core_term_lookahead->term = THE_TERMINAL;
    new_core_term_lookahead->lookahead = NEXT_TERMINAL?NEXT_TERMINAL->u.terminal.term_id:-1;

    for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
    {
        new_core_term_lookahead->result[i] = NULL;
    }
    new_core_term_lookahead->curr = 0;
    // We write into the hashtable using the entry point! Yay!
    // I.e. there is no write hash table entry function.....
    entry = find_hash_table_entry(ps->cache_stateset_core_term_lookahead, new_core_term_lookahead, true);

    if (*entry != NULL)
    {
        YaepStateSet *s;

        OS_TOP_NULLIFY(ps->triplet_core_term_lookahead_os);
        for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
        {
            if ((s = ((YaepStateSetCoreTermLookAhead*)*entry)->result[i]) == NULL)
            {
                break;
            }
            else if (check_cached_transition_set(ps,
                                                 s,
                                                 ((YaepStateSetCoreTermLookAhead*)*entry)->place[i]))
            {
                ps->new_set = s;
                ps->n_goto_successes++;
                break;
            }
        }
    }
    else
    {
        OS_TOP_FINISH(ps->triplet_core_term_lookahead_os);
        // Write the new_core_term_lookahead triplet into the hash table.
        *entry = (hash_table_entry_t)new_core_term_lookahead;
        ps->num_triplets_core_term_lookahead++;
        debug_info(ps, "store lookahead [s%dc%d %s]",
                   new_core_term_lookahead->set->id,
                   new_core_term_lookahead->set->core->id,
                   new_core_term_lookahead->term->hr);
    }

    return (YaepStateSetCoreTermLookAhead*)*entry;
}

/* Save(set, term, lookahead) -> new_set in the table. */
static void save_cached_set(YaepParseState *ps, YaepStateSetCoreTermLookAhead *entry, YaepSymbol *NEXT_TERMINAL)
{
    int i = entry->curr;
    entry->result[i] = ps->new_set;
    entry->place[i] = ps->state_set_k;
    entry->lookahead = NEXT_TERMINAL ? NEXT_TERMINAL->u.terminal.term_id : -1;
    entry->curr = (i + 1) % MAX_CACHED_GOTO_RESULTS;
}

static void perform_parse(YaepParseState *ps)
{
    error_recovery_init(ps);
    build_start_set(ps);

    if (ps->run.trace)
    {
        MemBuffer *mb = new_membuffer();
        print_state_set(mb, ps, ps->new_set, 0);
        debug_mb("ixml.pa.st=", mb);
        free_membuffer_and_free_content(mb);
    }

    ps->tok_i = 0;
    ps->state_set_k = 0;

    for(; ps->tok_i < ps->input_len; ps->tok_i++)
    {
        // This assert is TODO! Theoretically the state_set_k could be less than tok_i
        // assuming a state set has been reused. So far I have not seen that, so assume state_set_k == tok_i == index of input char.
        assert(ps->tok_i == ps->state_set_k);

        YaepSymbol *THE_TERMINAL = ps->input[ps->tok_i].symb;
        YaepSymbol *NEXT_TERMINAL = NULL;

        if (ps->run.grammar->lookahead_level != 0 && ps->tok_i < ps->input_len-1)
        {
            NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
        }

        assert(ps->tok_i == ps->state_set_k);

        debug_info(ps, "LOOP token %s next %s", THE_TERMINAL->hr, NEXT_TERMINAL?NEXT_TERMINAL->hr:"?");

        YaepStateSet *set = ps->state_sets[ps->state_set_k];
        ps->new_set = NULL;

#ifdef USE_SET_HASH_TABLE
        // This command also adds the set to the hashtable if it does not already exist.
        // As a side effect it writes into ps->new_set
        YaepStateSetCoreTermLookAhead *entry = lookup_cached_core_term_lookahead(ps, THE_TERMINAL, NEXT_TERMINAL, set);
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

            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "input[%d]=", ps->tok_i);
                print_symbol(mb, THE_TERMINAL, true);
                membuffer_printf(mb, " s%d core%d -> csl%d", set->id, set->core->id, core_symb_to_predcomps->id);
                debug_mb("ixml.pa=", mb);
                free_membuffer_and_free_content(mb);
            }

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
            debug_mb("ixml.pa.st=", mb);
            free_membuffer_and_free_content(mb);
        }
    }
    free_error_recovery(ps);
}

static unsigned parse_state_hash(hash_table_entry_t s)
{
    YaepParseTreeBuildState*state =((YaepParseTreeBuildState*) s);

    /* The table contains only states with dot at the end of rule. */
    assert(state->dot_j == state->rule->rhs_len);
    return(((jauquet_prime_mod32* hash_shift +
             (unsigned)(size_t) state->rule)* hash_shift +
             state->from_i)* hash_shift + state->state_set_k);
}

static bool parse_state_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepParseTreeBuildState*state1 =((YaepParseTreeBuildState*) s1);
    YaepParseTreeBuildState*state2 =((YaepParseTreeBuildState*) s2);

    /* The table contains only states with dot at the end of rule.*/
    assert(state1->dot_j == state1->rule->rhs_len
            && state2->dot_j == state2->rule->rhs_len);
    return(state1->rule == state2->rule && state1->from_i == state2->from_i
            && state1->state_set_k == state2->state_set_k);
}

/* The following function initializes work with parser states.*/
static void parse_state_init(YaepParseState *ps)
{
    ps->free_parse_state = NULL;
    OS_CREATE(ps->parse_state_os, ps->run.grammar->alloc, 0);
    if (!ps->run.grammar->one_parse_p)
    {
        ps->map_rule_orig_statesetind_to_internalstate =
            create_hash_table(ps->run.grammar->alloc, ps->input_len* 2,
                              (hash_table_hash_function)parse_state_hash,
                              (hash_table_eq_function)parse_state_eq);
    }
}

/* The following function returns new parser state.*/
static YaepParseTreeBuildState *parse_state_alloc(YaepParseState *ps)
{
    YaepParseTreeBuildState*result;

    if (ps->free_parse_state == NULL)
    {
        OS_TOP_EXPAND(ps->parse_state_os, sizeof(YaepParseTreeBuildState));
        result =(YaepParseTreeBuildState*) OS_TOP_BEGIN(ps->parse_state_os);
        OS_TOP_FINISH(ps->parse_state_os);
    }
    else
    {
        result = ps->free_parse_state;
        ps->free_parse_state =(YaepParseTreeBuildState*) ps->free_parse_state->rule;
    }
    return result;
}

/* The following function frees STATE.*/
static void parse_state_free(YaepParseState *ps, YaepParseTreeBuildState*state)
{
    state->rule = (YaepRule*)ps->free_parse_state;
    ps->free_parse_state = state;
}

/* The following function searches for state in the table with the
   same characteristics as "state".  If found, then it returns a pointer
   to the state in the table.  Otherwise the function makes copy of
  *STATE, inserts into the table and returns pointer to copied state.
   In the last case, the function also sets up*NEW_P.*/
static YaepParseTreeBuildState *parse_state_insert(YaepParseState *ps, YaepParseTreeBuildState *state, bool *new_p)
{
    YaepParseTreeBuildState **entry = (YaepParseTreeBuildState**)find_hash_table_entry(
        ps->map_rule_orig_statesetind_to_internalstate,
        state,
        true);

    *new_p = false;

    if (*entry != NULL)
    {
        return *entry;
    }

    *new_p = true;

    /* We make copy because state_set_k can be changed in further processing state. */
    *entry = parse_state_alloc(ps);
    **entry = *state;

    return *entry;
}

static void free_parse_state(YaepParseState *ps)
{
    if (!ps->run.grammar->one_parse_p)
    {
        delete_hash_table(ps->map_rule_orig_statesetind_to_internalstate);
    }
    OS_DELETE(ps->parse_state_os);
}

/* The following function places translation NODE into *PLACE and
   creates alternative nodes if it is necessary. */
static void place_translation(YaepParseState *ps, YaepTreeNode **place, YaepTreeNode *node)
{
    YaepTreeNode *alt, *next_alt;

    assert(place != NULL);
    if (*place == NULL)
    {
        *place = node;
        return;
    }
    /* We need an alternative.*/

    ps->n_parse_alt_nodes++;

    alt =(YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode));
    alt->type = YAEP_ALT;
    alt->val.alt.node = node;
    if ((*place)->type == YAEP_ALT)
    {
        alt->val.alt.next =*place;
    }
    else
    {
        /* We need alternative node for the 1st
           alternative too.*/
        ps->n_parse_alt_nodes++;
        next_alt = alt->val.alt.next
            =((YaepTreeNode*)
              (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
        next_alt->type = YAEP_ALT;
        next_alt->val.alt.node =*place;
        next_alt->val.alt.next = NULL;
    }
   *place = alt;
}

static YaepTreeNode *copy_anode(YaepParseState *ps,
                                YaepTreeNode **place,
                                YaepTreeNode *anode,
                                YaepRule *rule,
                                int rhs_offset)
{
    YaepTreeNode*node;
    int i;

    node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                              + sizeof(YaepTreeNode*)
                                              *(rule->trans_len + 1)));
   *node =*anode;
    node->val.anode.children = ((YaepTreeNode**)((char*) node + sizeof(YaepTreeNode)));
    for(i = 0; i <= rule->trans_len; i++)
    {
        node->val.anode.children[i] = anode->val.anode.children[i];
    }
    node->val.anode.children[rhs_offset] = NULL;
    place_translation(ps, place, node);

    return node;
}

/* The hash of the memory reference. */
static unsigned reserv_mem_hash(hash_table_entry_t m)
{
    return (size_t)m;
}

/* The equity of the memory reference. */
static bool reserv_mem_eq(hash_table_entry_t m1, hash_table_entry_t m2)
{
    return m1 == m2;
}

/* The following function sets up minimal cost for each abstract node.
   The function returns minimal translation corresponding to NODE.
   The function also collects references to memory which can be
   freed. Remeber that the translation is DAG, altenatives form lists
   (alt node may not refer for another alternative). */
static YaepTreeNode *prune_to_minimal(YaepParseState *ps, YaepTreeNode *node, int *cost)
{
    YaepTreeNode*child,*alt,*next_alt,*result = NULL;
    int i, min_cost = INT_MAX;

    assert(node != NULL);
    switch(node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        if (ps->run.parse_free != NULL)
        {
            VLO_ADD_MEMORY(ps->tnodes_vlo, &node, sizeof(node));
        }
       *cost = 0;
        return node;
    case YAEP_ANODE:
        if (node->val.anode.cost >= 0)
        {
            if (ps->run.parse_free != NULL)
            {
                VLO_ADD_MEMORY(ps->tnodes_vlo, &node, sizeof(node));
            }
            for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
            {
                node->val.anode.children[i] = prune_to_minimal(ps, child, cost);
                if (node->val.anode.children[i] != child)
                {
                    //fprintf(stderr, "PRUNEDDDDDDD\n");
                }
                node->val.anode.cost += *cost;
            }
           *cost = node->val.anode.cost;
            node->val.anode.cost = -node->val.anode.cost - 1;        /* flag of visit*/
        }
        return node;
    case YAEP_ALT:
        for(alt = node; alt != NULL; alt = next_alt)
        {
            if (ps->run.parse_free != NULL)
            {
                VLO_ADD_MEMORY(ps->tnodes_vlo, &alt, sizeof(alt));
            }
            next_alt = alt->val.alt.next;
            alt->val.alt.node = prune_to_minimal(ps, alt->val.alt.node, cost);
            if (alt == node || min_cost > *cost)
            {
                if (ps->run.debug)
                {
                    fprintf(stderr, "FOUND smaller cost %d %s\n", *cost, alt->val.alt.node->val.anode.name);
                }
                min_cost = *cost;
                alt->val.alt.next = NULL;
                result = alt;
            }
            else if (min_cost ==*cost && !ps->run.grammar->one_parse_p)
            {
                alt->val.alt.next = result;
                result = alt;
            }
        }
       *cost = min_cost;
        return(result->val.alt.next == NULL ? result->val.alt.node : result);
    default:
        assert(false);
    }
   *cost = 0;
    return NULL;
}

/* The following function traverses the translation collecting
   reference to memory which may not be freed.*/
static void traverse_pruned_translation(YaepParseState *ps, YaepTreeNode *node)
{
    YaepTreeNode*child;
    hash_table_entry_t*entry;
    int i;

next:
    assert(node != NULL);
    if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->set_of_reserved_memory, node, true)) == NULL)
    {
       *entry = (hash_table_entry_t)node;
    }
    switch(node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;
    case YAEP_ANODE:
        if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->set_of_reserved_memory,
                                                                      node->val.anode.name,
                                                                      true)) == NULL)
        {
            *entry =(hash_table_entry_t) node->val.anode.name;
        }
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            traverse_pruned_translation(ps, child);
        }
        // FIXME Is this assert needed? What is its purpose?
        // assert(node->val.anode.cost < 0);
        node->val.anode.cost = -node->val.anode.cost - 1;
        break;
    case YAEP_ALT:
        traverse_pruned_translation(ps, node->val.alt.node);
        if ((node = node->val.alt.next) != NULL)
            goto next;
        break;
    default:
        assert(false);
    }

    return;
}

/* The function finds and returns a minimal cost parse(s). */
static YaepTreeNode *find_minimal_translation(YaepParseState *ps, YaepTreeNode *root)
{
    YaepTreeNode**node_ptr;
    int cost;

    if (ps->run.parse_free != NULL)
    {
        ps->set_of_reserved_memory = create_hash_table(ps->run.grammar->alloc, ps->input_len* 4,
                                                       (hash_table_hash_function)reserv_mem_hash,
                                                       (hash_table_eq_function)reserv_mem_eq);

        VLO_CREATE(ps->tnodes_vlo, ps->run.grammar->alloc, ps->input_len* 4* sizeof(void*));
    }
    root = prune_to_minimal(ps, root, &cost);

    traverse_pruned_translation(ps, root);

    if (ps->run.parse_free != NULL)
    {
        for(node_ptr = (YaepTreeNode**)VLO_BEGIN(ps->tnodes_vlo);
            node_ptr <(YaepTreeNode**)VLO_BOUND(ps->tnodes_vlo);
            node_ptr++)
        {
            if (*find_hash_table_entry(ps->set_of_reserved_memory,*node_ptr, true) == NULL)
            {
                if ((*node_ptr)->type == YAEP_ANODE
                    &&*find_hash_table_entry(ps->set_of_reserved_memory,
                                             (*node_ptr)->val.anode.name,
                                             true) == NULL)
                {
                    // (*ps->run.parse_free)((void*)(*node_ptr)->val.anode.name);
                }
                //(*ps->run.parse_free)(*node_ptr);
            }
        }
        VLO_DELETE(ps->tnodes_vlo);
        delete_hash_table(ps->set_of_reserved_memory);
    }

    return root;
}

static void loop_stack(YaepTreeNode **result,
                       int n_candidates,
                       YaepParseState *ps,
                       YaepTreeNode *empty_node,
                       YaepTreeNode *error_node,
                       YaepStateSet *set,
                       YaepDottedRule *dotted_rule,
                       bool *ambiguous_p)
{
    YaepParseTreeBuildState root_state;
    YaepTreeNode root_anode;

    vlo_t stack, orig_states;

    YaepTreeNode **term_node_array = NULL;

    VLO_CREATE(stack, ps->run.grammar->alloc, 10000);

    if (!ps->run.grammar->one_parse_p)
    {
        /* We need this array to reuse terminal nodes when building a parse tree with ALT nodes.
           I.e. a tree with multiple possible parses. */
        size_t term_node_array_size = ps->input_len * sizeof(YaepTreeNode*);
        term_node_array = (YaepTreeNode**)yaep_malloc(ps->run.grammar->alloc, term_node_array_size);
        memset(term_node_array, 0, term_node_array_size);
        /* The following is used to check necessity to create current state with different state_set_k. */
        VLO_CREATE(orig_states, ps->run.grammar->alloc, 0);
    }

    // The root abstract node points to the result tree pointer.
    root_anode.val.anode.children = result;
    // The root state pointsto the root abstract node.
    root_state.anode = &root_anode;

    // Push a new state on the stack
    YaepParseTreeBuildState *state = parse_state_alloc(ps);
    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;

    YaepRule *rule = state->rule = dotted_rule->rule;
    state->dotted_rule = dotted_rule;
    state->dot_j = dotted_rule->dot_j;
    state->from_i = 0;
    state->state_set_k = ps->state_set_k;

    // Again is state_set_k always == tok_i ????
    assert(ps->state_set_k == ps->tok_i);

    state->parent_anode_state = &root_state;
    state->parent_rhs_offset = 0;
    state->anode = NULL;

    if (ps->run.debug)
    {
        // Log the starting node.
        MemBuffer *mb = new_membuffer();
	membuffer_printf(mb, "adding (i%d,d%d) [%d-%d]    ",
                         state->from_i,
                         state->dotted_rule->id,
                         state->from_i,
                         state->state_set_k);
	print_rule(mb, ps, state->rule);
        debug_mb("ixml.tr=", mb);
        free_membuffer_and_free_content(mb);
    }

    while (VLO_LENGTH(stack) != 0)
    {
        if (ps->run.debug && state->dot_j == state->rule->rhs_len)
        {
            // top = (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1
            MemBuffer *mb = new_membuffer();
            membuffer_printf(mb, "processing (s%d,dri%d) [%d-%d]    ", state->state_set_k, state->dotted_rule->id, state->from_i, state->state_set_k);
            print_rule(mb, ps, state->rule);
            debug_mb("ixml.tr.c=", mb);
            free_membuffer_and_free_content(mb);
        }
        int pos_j = --state->dot_j;
        rule = state->rule;
        YaepParseTreeBuildState *parent_anode_state = state->parent_anode_state;
        YaepTreeNode *parent_anode = parent_anode_state->anode;
        int parent_rhs_offset = state->parent_rhs_offset;
        YaepTreeNode *anode = state->anode;
        int rhs_offset = rule->order[pos_j];
        int state_set_k = state->state_set_k;
        int from_i = state->from_i;
        if (pos_j < 0)
        {
            /* We've processed all rhs of the rule.*/
            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "popping    ");
                print_rule(mb, ps, state->rule);
                debug_mb("ixml.tr.c=", mb);
                free_membuffer_and_free_content(mb);
            }
            parse_state_free(ps, state);
            VLO_SHORTEN(stack, sizeof(YaepParseTreeBuildState*));
            if (VLO_LENGTH(stack) != 0)
            {
                state = ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
            }
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
            {
                /* We do dotted_ruleuce nothing but we should. So write empty node.*/
                place_translation(ps, parent_anode->val.anode.children + parent_rhs_offset, empty_node);
                empty_node->val.nil.used = 1;
            } else
            if (anode != NULL)
            {
                /* Change NULLs into empty nodes.  We can not make it
                 the first time because when building several parses
                 the NULL means flag of absence of translations(see
                 function `place_translation'). */
                for (int i = 0; i < rule->trans_len; i++)
                {
                    if (anode->val.anode.children[i] == NULL)
                    {
                        anode->val.anode.children[i] = empty_node;
                        empty_node->val.nil.used = 1;
                    }
                }
            }

            continue;
        }
        assert(pos_j >= 0);
        YaepTreeNode *node;
        YaepSymbol *symb = rule->rhs[pos_j];
        if (symb->is_terminal)
        {
            /* Terminal before dot:*/
            state_set_k--; /* l*/
            /* Because of error recovery input [state_set_k].symb may be not equal to symb.*/
            //assert(ps->input[state_set_k].symb == symb);
            if (parent_anode != NULL && rhs_offset >= 0)
            {
                /* We should generate and use the translation of the terminal.  Add reference to the current node.*/
                if (symb == ps->run.grammar->term_error)
                {
                    // Oups error node.
                    node = error_node;
                    error_node->val.error.used = 1;
                } else
                if (!ps->run.grammar->one_parse_p && (node = term_node_array[state_set_k]) != NULL)
                {
                    // Reuse existing terminal node.
                } else
                {
                    // Allocate terminal node.
                    ps->n_parse_term_nodes++;
                    node = ((YaepTreeNode*) (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
                    node->type = YAEP_TERM;
                    node->val.terminal.code = symb->u.terminal.code;
                    if (rule->marks && rule->marks[pos_j])
                    {
                        // Copy the ixml mark from the rhs position on to the terminal.
                        node->val.terminal.mark = rule->marks[pos_j];
                    }
                    // Copy any attr from input token to parsed terminal.
                    node->val.terminal.attr = ps->input[state_set_k].attr;
                    if (!ps->run.grammar->one_parse_p)
                    {
                        term_node_array[state_set_k] = node;
                    }
                }

                YaepTreeNode **placement = NULL;
                if (anode)
                {
                    placement = anode->val.anode.children + rhs_offset;
                } else
                {
                    placement = parent_anode->val.anode.children + parent_rhs_offset;
                }
                place_translation(ps, placement, node);
            }
            if (pos_j != 0)
            {
                state->state_set_k = state_set_k;
            }
            continue;
        }
        /* Nonterminal before dot: */
        set = ps->state_sets[state_set_k];
        YaepStateSetCore *set_core = set->core;
        YaepCoreSymbToPredComps *core_symb_to_predcomps = core_symb_to_predcomps_find(ps, set_core, symb);
        debug("ixml.pa.c=", "core core%d symb %s -> %p", set_core->id, symb->hr, core_symb_to_predcomps);
        if (!core_symb_to_predcomps)
            continue;

        assert(core_symb_to_predcomps->completions.len != 0);
        n_candidates = 0;
        YaepParseTreeBuildState *orig_state = state;
        if (!ps->run.grammar->one_parse_p)
        {
            VLO_NULLIFY(orig_states);
        }
        for (int i = 0; i < core_symb_to_predcomps->completions.len; i++)
        {
            int rule_index_in_core = core_symb_to_predcomps->completions.ids[i];
            dotted_rule = set_core->dotted_rules[rule_index_in_core];
            int dotted_rule_from_i;
            if (rule_index_in_core < set_core->num_started_dotted_rules)
            {
                // The state_set_k is the tok_i for which the state set was created.
                // Ie, it is the to_i inside the Earley item.
                // Now subtract the matched length from this to_i to get the from_i
                // which is the origin.
                dotted_rule_from_i = state_set_k - set->matched_lengths[rule_index_in_core];
            } else
            if (rule_index_in_core < set_core->num_all_matched_lengths)
            {
                // Parent??
                dotted_rule_from_i = state_set_k - set->matched_lengths[set_core->parent_dotted_rule_ids[rule_index_in_core]];
            } else
            {
                dotted_rule_from_i = state_set_k;
            }

            YaepStateSet *check_set = ps->state_sets[dotted_rule_from_i];
            YaepStateSetCore *check_set_core = check_set->core;
            YaepCoreSymbToPredComps *check_core_symb_to_predcomps = core_symb_to_predcomps_find(ps, check_set_core, symb);
            assert(check_core_symb_to_predcomps != NULL);
            bool found = false;
            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "trying (s%d,d%d) [%d-%d]  csl%d check_csl%d  ", state_set_k, dotted_rule->id, dotted_rule_from_i, state_set_k,
                                core_symb_to_predcomps->id, check_core_symb_to_predcomps->id);
                print_rule(mb, ps, dotted_rule->rule);
                debug_mb("ixml.tr.c=", mb);
                free_membuffer_and_free_content(mb);
            }
            for (int j = 0; j < check_core_symb_to_predcomps->predictions.len; j++)
            {
                int rule_index_in_check_core = check_core_symb_to_predcomps->predictions.ids[j];
                YaepDottedRule *check_dotted_rule = check_set->core->dotted_rules[rule_index_in_check_core];
                if (check_dotted_rule->rule != rule || check_dotted_rule->dot_j != pos_j)
                {
                    continue;
                }
                int check_dotted_rule_from_i = dotted_rule_from_i;
                if (rule_index_in_check_core < check_set_core->num_all_matched_lengths)
                {
                    if (rule_index_in_check_core < check_set_core->num_started_dotted_rules)
                    {
                        check_dotted_rule_from_i = dotted_rule_from_i - check_set->matched_lengths[rule_index_in_check_core];
                    } else
                    {
                        check_dotted_rule_from_i = (dotted_rule_from_i
                                        - check_set->matched_lengths[check_set_core->parent_dotted_rule_ids[rule_index_in_check_core]]);
                    }
                }
                if (check_dotted_rule_from_i == from_i)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                continue;
            }
            if (n_candidates != 0)
            {
                // Oups, n_candidates is > 0 already, this means that a previous completion matched.
                // We have more than one parse.
                *ambiguous_p = true;
                if (ps->run.grammar->one_parse_p)
                {
                    break;
                }
            }
            YaepRule *dotted_rule_rule = dotted_rule->rule;
            if (n_candidates == 0)
            {
                orig_state->state_set_k = dotted_rule_from_i;
            }
            if (parent_anode != NULL && rhs_offset >= 0)
            {
                /* We should generate and use the translation of the nonterminal. */
                YaepParseTreeBuildState *curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule. */
                if (n_candidates != 0)
                {
                    assert(!ps->run.grammar->one_parse_p);
                    if (n_candidates == 1)
                    {
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1] = orig_state;
                    }
                    int j = VLO_LENGTH(orig_states) / sizeof(YaepParseTreeBuildState*) - 1;
                    while (j >= 0)
                    {
                        if (((YaepParseTreeBuildState**) VLO_BEGIN(orig_states))[j]->state_set_k == dotted_rule_from_i)
                        {
                            break;
                        }
                        j--;
                    }
                    if (j >= 0)
                    {
                        /* [A -> x., n] & [A -> y., n]*/
                        curr_state = ((YaepParseTreeBuildState**) VLO_BEGIN(orig_states))[j];
                        anode = curr_state->anode;
                    } else
                    {
                        /* [A -> x., n] & [A -> y., m] where n != m.*/
                        /* It is different from the previous ones so add
                         it to process.*/
                        // Push a new state on the stack.
                        state = parse_state_alloc(ps);
                        VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                        *state = *orig_state;
                        state->state_set_k = dotted_rule_from_i;
                        if (anode != NULL)
                        {
                            state->anode = copy_anode(ps, parent_anode->val.anode.children + parent_rhs_offset, anode, rule, rhs_offset);
                        }
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1] = state;
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "* (f%d,d%d) add1 modified dotted_rule=", dotted_rule_from_i, state->dotted_rule->id);
                            print_rule_with_dot(mb, ps, state->rule, state->dot_j);
                            membuffer_printf(mb, " state->from_i=%d", state->from_i);
                            debug_mb("ixml.tr.c=", mb);
                            free_membuffer_and_free_content(mb);
                            assert(false);
                        }
                        curr_state = state;
                        anode = state->anode;
                    }
                }
                /* WOOT? if (n_candidates != 0)*/
                if (dotted_rule_rule->anode != NULL)
                {
                    /* This rule creates abstract node. */
                    // Push a new state on the stack.
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = dotted_rule_rule;
                    state->dotted_rule = dotted_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    YaepParseTreeBuildState *table_state = NULL;
                    _Bool new_p;
                    if (!ps->run.grammar->one_parse_p)
                    {
                        table_state = parse_state_insert(ps, state, &new_p);
                    }
                    if (table_state == NULL || new_p)
                    {
                        /* Allocate abtract node. */
                        ps->n_parse_abstract_nodes++;
                        node = ((YaepTreeNode*) (*ps->run.parse_alloc)(sizeof(YaepTreeNode) + sizeof(YaepTreeNode*) * (dotted_rule_rule->trans_len + 1)));
                        node->type = YAEP_ANODE;
                        state->anode = node;
                        if (table_state != NULL)
                        {
                            table_state->anode = node;
                        }
                        if (dotted_rule_rule->caller_anode == NULL)
                        {
                            dotted_rule_rule->caller_anode = ((char*) (*ps->run.parse_alloc)(strlen(dotted_rule_rule->anode) + 1));
                            strcpy(dotted_rule_rule->caller_anode, dotted_rule_rule->anode);
                        }
                        node->val.anode.name = dotted_rule_rule->caller_anode;
                        node->val.anode.cost = dotted_rule_rule->anode_cost;
                        // IXML Copy the rule name -to the generated abstract node.
                        node->val.anode.mark = dotted_rule_rule->mark;
                        if (rule->marks && rule->marks[pos_j])
                        {
                            // But override the mark with the rhs mark!
                            node->val.anode.mark = rule->marks[pos_j];
                        }
                        /////////
                        node->val.anode.children = ((YaepTreeNode**) ((char*) node + sizeof(YaepTreeNode)));
                        for (int k = 0; k <= dotted_rule_rule->trans_len; k++)
                        {
                            node->val.anode.children[k] = NULL;
                        }
                        if (anode == NULL)
                        {
                            state->parent_anode_state = curr_state->parent_anode_state;
                            state->parent_rhs_offset = parent_rhs_offset;
                        } else
                        {
                            state->parent_anode_state = curr_state;
                            state->parent_rhs_offset = rhs_offset;
                        }
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "adding (s%d,d%d) [%d-%d] ", state->state_set_k, state->dotted_rule->id, state->from_i, state->state_set_k);
                            print_rule(mb, ps, dotted_rule->rule);
                            debug_mb("ixml.tr=", mb);
                            free_membuffer_and_free_content(mb);
                        }
                    } else
                    {
                        /* We allready have the translation.*/
                        assert(!ps->run.grammar->one_parse_p);
                        parse_state_free(ps, state);
                        state = ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
                        node = table_state->anode;
                        assert(node != NULL);
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "* found prev. translation: state_set_k = %d, dotted_rule = ", state_set_k);
                            print_dotted_rule(mb, ps, -1, dotted_rule, -1, -1);
                            membuffer_printf(mb, ", %d\n", dotted_rule_from_i);
                            debug_mb("ixml.tr.c=", mb);
                            free_membuffer_and_free_content(mb);
                            // assert(false);
                        }
                    }
                    YaepTreeNode **placement = NULL;
                    if (anode)
                    {
                        placement = anode->val.anode.children + rhs_offset;
                    } else
                    {
                        placement = parent_anode->val.anode.children + parent_rhs_offset;
                    }
                    place_translation(ps, placement, node);
                } /* if (dotted_rule_rule->anode != NULL)*/else
                if (dotted_rule->dot_j != 0)
                {
                    /* We should generate and use the translation of the
                     nonterminal.  Add state to get a translation.*/
                    // Push a new state on the stack
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = dotted_rule_rule;
                    state->dotted_rule = dotted_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    state->parent_anode_state = (anode == NULL ? curr_state->parent_anode_state : curr_state);
                    assert(state->parent_anode_state);
                    state->parent_rhs_offset = anode == NULL ? parent_rhs_offset : rhs_offset;
                    state->anode = NULL;
                    if (ps->run.debug)
                    {
                        MemBuffer *mb = new_membuffer();
                        membuffer_printf(mb, "* add3   state_set_k=%d   dotted_rule_from_i=%d    ", state_set_k, dotted_rule_from_i);
                        print_rule(mb, ps, dotted_rule->rule);
                        debug_mb("ixml.tr.c=", mb);
                        free_membuffer_and_free_content(mb);
                        //assert(false);
                    }
                } else
                {
                    /* Empty rule should dotted_ruleuce something not abtract
                     node.  So place empty node.*/
                    place_translation(ps, anode == NULL ? parent_anode->val.anode.children + parent_rhs_offset : anode->val.anode.children + rhs_offset,
                                    empty_node);
                    empty_node->val.nil.used = 1;
                }
            }
            /* if (parent_anode != NULL && rhs_offset >= 0)*/                // Continue with next completion.
            n_candidates++;
        }
        /* For all completions of the nonterminal.*//* We should have a parse.*/
        assert(n_candidates != 0 && (!ps->run.grammar->one_parse_p || n_candidates == 1));
    }
    /* For all parser states.*/

    VLO_DELETE(stack);

    if (!ps->run.grammar->one_parse_p)
    {
        VLO_DELETE(orig_states);
        yaep_free(ps->run.grammar->alloc, term_node_array);
    }
}

static YaepTreeNode *build_parse_tree(YaepParseState *ps, bool *ambiguous_p)
{
    // Number of candiate trees found.
    int n_candidates = 0;

    // The result pointer points to the final parse tree.
    YaepTreeNode *result;

    ps->n_parse_term_nodes = ps->n_parse_abstract_nodes = ps->n_parse_alt_nodes = 0;

    // Pick the final state set, where we completed the axiom $.
    YaepStateSet *set = ps->state_sets[ps->state_set_k];
    assert(ps->run.grammar->axiom != NULL);

    /* We have only one start dotted_rule: "$ : <start symb> eof .". */
    YaepDottedRule *dotted_rule = (set->core->dotted_rules != NULL ? set->core->dotted_rules[0] : NULL);

    if (dotted_rule == NULL
        || set->matched_lengths[0] != ps->state_set_k
        || dotted_rule->rule->lhs != ps->run.grammar->axiom || dotted_rule->dot_j != dotted_rule->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'.*/
        assert(!ps->run.grammar->error_recovery_p);
        return NULL;
    }
    bool saved_one_parse_p = ps->run.grammar->one_parse_p;
    if (ps->run.grammar->cost_p)
    {
        /* We need all parses to choose the minimal one. */
        ps->run.grammar->one_parse_p = false;
    }

    parse_state_init(ps);

    // The result tree starts empty.
    result = NULL;

    /* Create empty and error node:*/
    YaepTreeNode *empty_node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    empty_node->type = YAEP_NIL;
    empty_node->val.nil.used = 0;

    YaepTreeNode *error_node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    error_node->type = YAEP_ERROR;
    error_node->val.error.used = 0;

    verbose("ixml=", "building tree");

    loop_stack(&result, n_candidates, ps, empty_node, error_node, set, dotted_rule, ambiguous_p);

    free_parse_state(ps);
    ps->run.grammar->one_parse_p = saved_one_parse_p;
    if (ps->run.grammar->cost_p && *ambiguous_p)
    {
        /* We can not build minimal tree during building parsing list
           because we have not the translation yet. We can not make it
           during parsing because the abstract nodes are created before
           their children. */
        result = find_minimal_translation(ps, result);
    }
    if (false)
    {
        fprintf(stderr, "(ixml) yaep parse tree: %p\n", result);
        print_parse(ps, stderr, result);
        fprintf(stderr, "\n");
    }
    if (false)
    {
        // Graphviz
        fprintf(stderr, "digraph CFG {\n");
        fprintf(stderr, "  node [shape=ellipse, fontsize=200];\n");
        fprintf(stderr, "  ratio=fill;\n");
        fprintf(stderr, "  ordering=out;\n");
        fprintf(stderr, "  page = \"8.5, 11\"; // inches\n");
        fprintf(stderr, "  size = \"7.5, 10\"; // inches\n\n");
        print_parse(ps, stderr, result);
        fprintf(stderr, "}\n");
    }


    /* Free empty and error node if they have not been used*/
    if (ps->run.parse_free != NULL)
    {
        if (!empty_node->val.nil.used)
        {
            ps->run.parse_free(empty_node);
        }
        if (!error_node->val.error.used)
        {
            ps->run.parse_free(error_node);
        }
    }

    assert(result != NULL && (!ps->run.grammar->one_parse_p || ps->n_parse_alt_nodes == 0));

    return result;
}

static void *parse_alloc_default(int nmemb)
{
    void *result;

    assert(nmemb > 0);

    result = malloc(nmemb);
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
    int table_collisions, table_searches;

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
    table_collisions = get_all_collisions();
    table_searches = get_all_searches();

    // Perform a parse.
    perform_parse(ps);

    // Reconstruct a parse tree from the state sets.
    *root = build_parse_tree(ps, ambiguous_p);

    table_collisions = get_all_collisions() - table_collisions;
    table_searches = get_all_searches() - table_searches;

    if (ps->run.debug)
    {
        MemBuffer *mb = new_membuffer();
        membuffer_printf(mb, "%sparse statistics\n#terminals = %d, #nonterms = %d\n",
                *ambiguous_p ? "AMBIGUOUS " : "",
                 ps->run.grammar->symbs_ptr->num_terminals, ps->run.grammar->symbs_ptr->num_nonterminals);
        membuffer_printf(mb, "#rules = %d, rules size = %d\n",
                 ps->run.grammar->rulestorage_ptr->num_rules,
                 ps->run.grammar->rulestorage_ptr->n_rhs_lens + ps->run.grammar->rulestorage_ptr->num_rules);
        membuffer_printf(mb, "#tokens = %d, #unique dotted_rules = %d\n",
                 ps->input_len, ps->num_all_dotted_rules);
        membuffer_printf(mb, "#terminal sets = %d, their size = %d\n",
                 ps->run.grammar->term_sets_ptr->n_term_sets, ps->run.grammar->term_sets_ptr->n_term_sets_size);
        membuffer_printf(mb, "#unique set cores = %d, #their start dotted_rules = %d\n",
                 ps->num_set_cores, ps->num_set_core_start_dotted_rules);
        membuffer_printf(mb, "#parent indexes for some non start dotted_rules = %d\n",
                 ps->num_parent_dotted_rule_ids);
        membuffer_printf(mb, "#unique set dist. vects = %d, their length = %d\n",
                 ps->num_set_matched_lengths, ps->num_set_matched_lengths_len);
        membuffer_printf(mb, "#unique sets = %d, #their start dotted_rules = %d\n",
                 ps->num_sets_total, ps->num_dotted_rules_total);
        membuffer_printf(mb, "#unique triples(set, term, lookahead) = %d, goto successes=%d\n",
                ps->num_triplets_core_term_lookahead, ps->n_goto_successes);
        membuffer_printf(mb, "#pairs(set core, symb) = %d, their trans+reduce vects length = %d\n",
                 ps->n_core_symb_pairs, ps->n_core_symb_to_predcomps_len);
        membuffer_printf(mb, "#unique transition vectors = %d, their length = %d\n",
                ps->n_transition_vects, ps->n_transition_vect_len);
        membuffer_printf(mb, "#unique reduce vectors = %d, their length = %d\n",
                 ps->n_reduce_vects, ps->n_reduce_vect_len);
        membuffer_printf(mb, "#term nodes = %d, #abstract nodes = %d\n",
                 ps->n_parse_term_nodes, ps->n_parse_abstract_nodes);
        membuffer_printf(mb, "#alternative nodes = %d, #all nodes = %d\n",
                 ps->n_parse_alt_nodes,
                 ps->n_parse_term_nodes + ps->n_parse_abstract_nodes
                 + ps->n_parse_alt_nodes);
        if (table_searches == 0) table_searches++;
        membuffer_printf(mb, "#table collisions = %.2g%%(%d out of %d)",
                 table_collisions* 100.0 / table_searches,
                 table_collisions, table_searches);
        debug_mb("ixml.st=", mb);
        free_membuffer_and_free_content(mb);
    }

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
