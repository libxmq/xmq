/*
  YAEP(Yet Another Earley Parser)

  Copyright(c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>
  Copyright(c) 2024-2025 Fredrik Öhrström <oehrstroem@gmail.com>

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

   2024 Fredrik Öhrström
   Refactored to fit ixml use case, removed global variables, restructured
   code, commented and renamed variables and structures, added ixml charset
   matching. Added comments.

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
                The dot_pos starts at zero which means nothing has been matched.
                A dotted rule is started if the dot_pos > 0, ie it has matched something.

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

#include <assert.h>

#define TRACE_F(ps) { \
        if (false && ps->run.trace) fprintf(stderr, "TRACE %s\n", __func__); \
    }

#define TRACEE_F(ps) { \
        if (false && ps->run.trace) fprintf(stderr, "TRACE %-30s", __func__); \
    }

#define TRACE_FA(ps, cformat, ...) { \
    if (false && ps->run.trace) fprintf(stderr, "TRACE %-30s" cformat "\n", __func__, __VA_ARGS__); \
}

#define TRACEE_FA(ps, cformat, ...) { \
    if (false && ps->run.trace) fprintf(stderr, "TRACE %-30s" cformat, __func__, __VA_ARGS__); \
}

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "allocate.h"
#include "hashtab.h"
#include "vlobject.h"
#include "objstack.h"
#include "yaep.h"
#include "xmq.h"
#include "parts/always.h"
#include "parts/text.h"

#ifndef BUILDING_XMQ
bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len);
#endif

/* Terminals are stored a in term set using bits in a bit array.
   The array consists of long ints, typedefed as terminal_bitset_el_t.
   A long int is 8 bytes, ie 64 bits. */
typedef long int terminal_bitset_t;

/* Calculate the number of required term set elements from the number of bits we want to store. */
#define CALC_NUM_ELEMENTS(num_bits) ((num_bits+CHAR_BIT*sizeof(terminal_bitset_t)-1)/(CHAR_BIT*sizeof(terminal_bitset_t)))
// This calculation translates for example into ((num_bits+63)/64)
// We round up to have enough long ints to store num_bits.

#ifndef YAEP_MAX_ERROR_MESSAGE_LENGTH
#define YAEP_MAX_ERROR_MESSAGE_LENGTH 200
#endif

/* As of Unicode 16 there are 155_063 allocated unicode code points.
   Lets pick 200_000 as the max, it shrinks to max-min code point anyway.*/
#define MAX_SYMB_CODE_TRANS_VECT_SIZE 200000

/* The initial length of array(in tokens) in which input tokens are placed.*/
#ifndef NUM_INITIAL_YAEP_INIT_TOKENS
#define NUM_INITIAL_YAEP_TOKENS 10000
#endif

/* The default number of tokens sucessfully matched to stop error recovery alternative(state). */
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* Define this if you want to reuse already calculated state sets
   when the matches lengths are identical. This considerably speeds up the parser. */
#define USE_SET_HASH_TABLE

/* This does not seem to be enabled by default? */
#define USE_CORE_SYMB_HASH_TABLE

/* Maximal goto sets saved for triple(set, terminal, lookahead). */
#define MAX_CACHED_GOTO_RESULTS 3

/* Prime number(79087987342985798987987) mod 32 used for hash calculations. */
static const unsigned jauquet_prime_mod32 = 2053222611;

/* Shift used for hash calculations. */
static const unsigned hash_shift = 611;

struct YaepSymbol;
typedef struct YaepSymbol YaepSymbol;

struct YaepSymbolStorage;
typedef struct YaepSymbolStorage YaepSymbolStorage;

struct YaepTerminalSet;
typedef struct YaepTerminalSet YaepTerminalSet;

struct YaepTerminalSetStorage;
typedef struct YaepTerminalSetStorage YaepTerminalSetStorage;

struct YaepRule;
typedef struct YaepRule YaepRule;

struct YaepRuleStorage;
typedef struct YaepRuleStorage YaepRuleStorage;

struct YaepVect;
typedef struct YaepVect YaepVect;

struct YaepCoreSymbVect;
typedef struct YaepCoreSymbVect YaepCoreSymbVect;

struct YaepStateSetCore;
typedef struct YaepStateSetCore YaepStateSetCore;

struct YaepStateSet;
typedef struct YaepStateSet YaepStateSet;

struct YaepDottedRule;
typedef struct YaepDottedRule YaepDottedRule;

struct YaepInputToken;
typedef struct YaepInputToken YaepInputToken;

struct YaepStateSetTermLookAhead;
typedef struct YaepStateSetTermLookAhead YaepStateSetTermLookAhead;

struct YaepParseTreeBuildState;
typedef struct YaepParseTreeBuildState YaepParseTreeBuildState;

struct YaepTreeNodeVisit;
typedef struct YaepTreeNodeVisit YaepTreeNodeVisit;

struct YaepRecoveryState;
typedef struct YaepRecoveryState YaepRecoveryState;

// Structure definitions ////////////////////////////////////////////////////

struct YaepGrammar
{
    /* Set to true if the grammar is undefined(you should set up the grammar
       by yaep_read_grammar or yaep_parse_grammar) or bad(error was occured
       in setting up the grammar). */
    bool undefined_p;

    /* This member always contains the last occurred error code for given grammar. */
    int error_code;

    /* This member contains message are always contains error message
       corresponding to the last occurred error code.*/
    char error_message[YAEP_MAX_ERROR_MESSAGE_LENGTH + 1];

    /* The grammar axiom is named $S. */
    YaepSymbol *axiom;

    /* The end marker denotes EOF in the input token sequence. */
    YaepSymbol *end_marker;

    /* The term error is used for create error recovery nodes. */
    YaepSymbol *term_error;

    /* And its internal id.*/
    int term_error_id;

    /* The level of usage of lookaheads:
       0    - no usage
       1    - statik lookaheads
       >= 2 - dynamic lookaheads*/
    int lookahead_level;

    /* How many subsequent tokens should be successfuly shifted to finish error recovery. */
    int recovery_token_matches;

    /* If true then stop at first successful parse.
       If false then follow all possible parses. */
    bool one_parse_p;

    /* If true then find parse with minimal cost. */
    bool cost_p;

    /* If true them try to recover from errors. */
    bool error_recovery_p;

    /* These are all the symbols used in this grammar. */
    YaepSymbolStorage *symbs_ptr;

    /* All rules used in this grammar are stored here. */
    YaepRuleStorage *rulestorage_ptr;

    /* The following terminal sets used for this grammar. */
    YaepTerminalSetStorage *term_sets_ptr;

    /* Allocator. */
    YaepAllocator *alloc;

    /* A user supplied pointer that is available to user callbacks through the grammar pointer. */
    void *user_data;
};

struct YaepSymbol
{
    /* A unique number 0,1,2... (num_terminals + num_non_terminals -1) */
    int id;
    /* The following is external representation of the symbol.  It
       should be allocated by parse_alloc because the string will be
       referred from parse tree. */
    const char *repr;
    char hr[7];    // #1ffff or ' ' or #78
    union
    {
        struct
        {
            /* The code is a unique number per terminal type and is specified when
               read_grammar fetches the terminals. For grammars with a lexer preprocessing
               step, the code means 1 = "BEGIN", 2 = "END, 3 = "IDENTIFIER" etc.
               For ixml grammars, each code is instead a unicode codepoint.
               I.e. 65 = A, 0x1f600 = 😀  etc. */
            int code;
            /* Each term is given a unique integer starting from 0. If the code range
               starts with 100 and ends with 129,then the term_ids goes from 0 to 29.
               The term_ids are used for picking the bit in the bit arrays.*/
            int term_id;
        } terminal;
        struct
        {
            /* The following refers for all rules with the nonterminal
               symbol is in the left hand side of the rules. */
            YaepRule *rules;
            /* Each nonterm is given a unique integer starting from 0. */
            int nonterm_id;
            /* The following value is nonzero if nonterminal may derivate
               itself.  In other words there is a grammar loop for this
               nonterminal.*/
            bool loop_p;
            /* The following members are FIRST and FOLLOW sets of the nonterminal. */
            terminal_bitset_t *first, *follow;
        } nonterminal;
    } u;
    /* If true, the use terminal in union. */
    bool terminal_p;
    /* The following member value(if defined) is true if the symbol is
       accessible(derivated) from the axiom. */
    bool access_p;
    /* The following member is true if it is a termainal/nonterminal which derives a terminal string. */
    bool derivation_p;
    /* The following is true if it is a nonterminal which may derive the empty string. */
    bool empty_p;
#ifdef USE_CORE_SYMB_HASH_TABLE
    /* The following is used as cache for subsequent search for
       core_symb_ids with given symb. */
    YaepCoreSymbVect *cached_core_symb_ids;
#endif
};

struct YaepSymbolStorage
{
    int num_terminals, num_nonterminals;

    /* All symbols are placed in the following object.*/
    os_t symbs_os;

    /* All references to the symbols, terminals, nonterminals are stored
       in the following vlos.  The indexes in the arrays are the same as
       corresponding symbol, terminal, and nonterminal numbers.*/
    vlo_t symbs_vlo;
    vlo_t terminals_vlo;
    vlo_t nonterminals_vlo;

    /* The following are tables to find terminal by its code and symbol by
       its representation.*/
    hash_table_t map_repr_to_symb;	/* key is `repr'*/
    hash_table_t map_code_to_symb;	/* key is `code'*/

    /* If terminal symbol codes are not spared(in this case the member
       value is not NULL, we use translation vector instead of hash table. */
    YaepSymbol **symb_code_trans_vect;
    int symb_code_trans_vect_start;
    int symb_code_trans_vect_end;
};

/* A set of terminals represented as a bit array. */
struct YaepTerminalSet
{
    // Set identity.
    int id;

    // Number of long ints (terminal_bitset_t) used to store the bit array.
    int num_elements;

    // The bit array itself.
    terminal_bitset_t *set;
};

/* The following container for the abstract data.*/
struct YaepTerminalSetStorage
{
    /* All terminal sets are stored in the following os. */
    os_t terminal_bitset_os;

    /* Their values are number of terminal sets and their overall size.*/
    int n_term_sets, n_term_sets_size;

    /* The YaepTerminalSet objects are stored in this vlo. */
    vlo_t terminal_bitset_vlo;

    /* Hashmap from key set (a bit array) to the YaepTerminalSet object from which we use the id. */
    hash_table_t map_terminal_bitset_to_id;
};

/* This page contains table for fast search for vector of indexes of
   dotted_rules with symbol after dot in given set core. */
struct YaepVect
{
    /* The following member is used internally.  The value is
       nonnegative for core_symb_ids being formed.  It is index of vlo
       in vlos array which contains the vector elements. */
    int intern;

    /* The following memebers defines array of ids of dotted_rules in a state set core. */
    int len;
    int *ids;
};

struct YaepCoreSymbVect
{
    /* The set core.*/
    YaepStateSetCore *core;

    /* The symbol.*/
    YaepSymbol *symb;

    /* The following vector contains ids of dotted_rules with given symb in dotted_rule after dot.
       We use this to predict the next set of dotted rules to add after symb has been reach in state core. */
    YaepVect predictions;

    /* The following vector contains id of reduce dotted_rule with given symb in lhs. */
    YaepVect completions;
};

/* A StateSetCore is a state set in Earley's algorithm but without matched lengths for the dotted rules.
   The state set cores can be reused between state sets and thus save memory. */
struct YaepStateSetCore
{
    /* The following is unique number of the set core. It is defined only after forming all set.*/
    int id;

    /* The state set core hash.  We save it as it is used several times. */
    unsigned int hash;

    /* The following is term shifting which resulted into this core.  It
       is defined only after forming all set.*/
    YaepSymbol *term;

    /* The variable num_dotted_rulesare all dotted_rules in the set. Both starting and predicted. */
    int num_dotted_rules;
    int num_started_dotted_rules;
    // num_not_yet_started_dotted_rules== num_dotted_rules-num_started_dotted_rules

    /* Array of dotted_rules.  Start dotted_rules are always placed the
       first in the order of their creation(with subsequent duplicates
       are removed), then not_yet_started noninitial(dotted_rule with at least
       one symbol before the dot) dotted_rules are placed and then initial
       dotted_rules are placed.  You should access to a set dotted_rule only
       through this member or variable `new_dotted_rules'(in other words don't
       save the member value in another variable).*/
    YaepDottedRule **dotted_rules;

    /* The following member is number of started dotted_rules and not-yet-started
       (noninitial) dotted_rules whose matched_length is defined from a start
       dotted_rule matched_length.  All not-yet-started initial dotted_rules have zero
       matched_lengths.  This matched_lengths are not stored. */
    int num_all_matched_lengths;

    /* The following is array containing number of start dotted_rule from
       which matched_length of(not_yet_started noninitial) dotted_rule with given
       index(between n_start_dotted_rules -> num_all_matched_lengths) is taken. */
    int *parent_dotted_rule_ids;
};

/* A YaepStateSet (aka parse list) stores chart entries (aka items) [from, to, S →  VP 🞄 NP ]
   Scanning an input token triggers the creation of a state set. If we have n input tokens,
   then we have n+2  state sets (we add the final eof token and a final state after eof
   has been scanned.) */
struct YaepStateSet
{
    /* The following is set core of the set.  You should access to set
       core only through this member or variable `new_core'(in other
       words don't save the member value in another variable).*/
    YaepStateSetCore *core;

    /* Hash of the array of matched_lengths. We save it as it is used several times. */
    unsigned int matched_lengths_hash;

    /* The following is matched_lengths only for started dotted_rules.  Not-yet-started
       dotted_rules have their matched_lengths set to 0 implicitly.  A started dotted_rule
       in the set core and its corresponding matched_length matched_length have the same index.
       You should access matched_lengths only through this variable or the variable
       new_matched_lengths, the location of the arrays can move. */
    int *matched_lengths;
};

/* A dotted_rule stores:
       reference to a rule,
       current dot position in the rule,
       lookup bitset for a quick terminal lookahead check, to see if this dotted_rule should be applied.

   This is stored separately and this means that we can reuse dotted_rule objects to save memory,
   since rule,dot,lookup is recurring often. */
struct YaepDottedRule
{
    /* Unique dotted_rule identifier. Starts at 0 and increments for each new dotted_rule. */
    int id;

    /* The following is the rule being dotted. */
    YaepRule *rule;

    /* The following is position of dot in rhs of the dotted rule.
       Starts at 0 (left of all rhs terms) and ends at rhs.len (right of all rhs terms). */
    short dot_j;

    /* The following member is true if the tail can derive empty string. */
    bool empty_tail_p;

    /* The following is number of dotted_rule context which is number of
       the corresponding terminal set in the table.  It is really used
       only for dynamic lookahead. */
    int context;

    /* The following member is the dotted_rule lookahead it is equal to
       FIRST(the dotted_rule tail || FOLLOW(lhs)) for statik lookaheads
       and FIRST(the dotted_rule tail || context) for dynamic ones. */
    terminal_bitset_t *lookahead;
};

struct YaepInputToken
{
    /* A symbol has a name "BEGIN",code 17, or for ixml "A",code 65. */
    YaepSymbol *symb;

    /* The token can have a provided attribute attached. This does not affect
       the parse, but can be extracted from the final parse tree. */
    void *attr;
};

/* The triple and possible goto sets for it. */
struct YaepStateSetTermLookAhead
{
    YaepStateSet*set;
    YaepSymbol*term;
    int lookahead;
    /* Saved goto sets form a queue.  The last goto is saved at the
       following array elements whose index is given by CURR. */
    int curr;
    /* Saved goto sets to which we can go from SET by the terminal with
       subsequent terminal LOOKAHEAD given by its code. */
    YaepStateSet*result[MAX_CACHED_GOTO_RESULTS];
    /* Corresponding places of the goto sets in the parsing list. */
    int place[MAX_CACHED_GOTO_RESULTS];
};

struct YaepRule
{
    /* The following is order number of rule. */
    int num;

    /* The following is length of rhs. */
    int rhs_len;

    /* The following is the next grammar rule. */
    YaepRule *next;

    /* The following is the next grammar rule with the same nonterminal
       in lhs of the rule.*/
    YaepRule *lhs_next;

    /* The following is nonterminal in the left hand side of the rule. */
    YaepSymbol *lhs;

    /* The ixml default mark of the rule. -@^ */
    char mark;

    /* The following is symbols in the right hand side of the rule. */
    YaepSymbol **rhs;

    /* The ixml marks for all the terms in the right hand side of the rule. */
    char *marks;
    /* The following three members define rule translation. */

    const char *anode;		/* abstract node name if any. */
    int anode_cost;		/* the cost of the abstract node if any, otherwise 0. */
    int trans_len;		/* number of symbol translations in the rule translation. */

    /* The following array elements correspond to element of rhs with
       the same index.  The element value is order number of the
       corresponding symbol translation in the rule translation.  If the
       symbol translation is rejected, the corresponding element value is
       negative. */
    int *order;

    /* The following member value is equal to size of all previous rule
       lengths + number of the previous rules.  Imagine that all left
       hand symbol and right hand size symbols of the rules are stored
       in array.  Then the following member is index of the rule lhs in
       the array. */
    int rule_start_offset;

    /* The following is the same string as anode but memory allocated in parse_alloc. */
    char *caller_anode;
};

/* The following container for the abstract data.*/
struct YaepRuleStorage
{
    /* The following is number of all rules and their summary rhs length. */
    int num_rules, n_rhs_lens;

    /* The following is the first rule.*/
    YaepRule *first_rule;

    /* The following is rule being formed. */
    YaepRule *current_rule;

    /* All rules are placed in the following object. */
    os_t rules_os;
};

/* This state is used when reconstricting the parse tree from the dotted_rules. */
struct YaepParseTreeBuildState
{
    /* The rule which we are processing. */
    YaepRule *rule;

    /* Current position in rule->rhs[]. */
    int dot_j;

    /* An index into input[] and is the starting point of the matched tokens for the rule. */
    int from_i;

    /* The current state set index into YaepParseState->state_sets. */
    int state_set_k;

    /* If the following value is NULL, then we do not need to create
       translation for this rule.  If we should create abstract node
       for this rule, the value refers for the abstract node and the
       displacement is undefined.  Otherwise, the two members is
       place into which we should place the translation of the rule.
       The following member is used only for states in the stack. */
    YaepParseTreeBuildState *parent_anode_state;
    int parent_disp;

    /* The following is used only for states in the table. */
    YaepTreeNode *anode;
};

/* To make better traversing and don't waist tree parse memory,
   we use the following structures to enumerate the tree node. */
struct YaepTreeNodeVisit
{
    /* The following member is order number of the node.  This value is
       negative if we did not visit the node yet. */
    int num;

    /* The tree node itself. */
    YaepTreeNode*node;
};

/* The following strucrture describes an error recovery state(an
   error recovery alternative.*/
struct YaepRecoveryState
{
    /* The following three members define start state set used to given error
       recovery state(alternative).*/
    /* The following members define what part of original(at the error
       recovery start) state set will be head of error recovery state.  The
       head will be all states from original state set with indexes in range
       [0, last_original_state_set_el].*/
    int last_original_state_set_el;
    /* The following two members define tail of state set for this error recovery state.*/
    int state_set_tail_length;
    YaepStateSet **state_set_tail;
    /* The following member is index of start token for given error recovery state.*/
    int start_tok;
    /* The following member value is number of tokens already ignored in
       order to achieved given error recovery state.*/
    int backward_move_cost;
};

struct YaepParseState
{
    YaepParseRun run;
    int magic_cookie; // Must be set to 736268273 when the state is created.

    /* The input token array to be parsed. */
    YaepInputToken *input;
    int input_len;
    vlo_t input_vlo;

    /* When parsing, the current input token is incremented from 0 to len. */
    int tok_i;

    /* The following says that new_set, new_core and their members are
       defined. Before this the access to data of the set being formed
       are possible only through the following variables. */
    int new_set_ready_p;

    /* The following variable is set being created. It is defined only when new_set_ready_p is true. */
    YaepStateSet *new_set;

   /* The following variable is always set core of set being created.
      Member core of new_set has always the
      following value.  It is defined only when new_set_ready_p is true. */
    YaepStateSetCore *new_core;

   /* To optimize code we use the following variables to access to data
      of new set.  They are always defined and correspondingly
      dotted_rules, matched_lengths, and the current number of start dotted_rules
      of the set being formed.*/
    YaepDottedRule **new_dotted_rules;
    int *new_matched_lengths;
    int new_num_started_dotted_rules;

    /* The following are number of unique set cores and their start
       dotted_rules, unique matched_length vectors and their summary length, and
       number of parent indexes. */
    int num_set_cores, num_set_core_start_dotted_rules;
    int num_set_matched_lengths, num_set_matched_lengths_len;
    int num_parent_dotted_rule_ids;

    /* Number of state sets and their started dotted_rules. */
    int n_sets, n_sets_start_dotted_rules;

    /* Number unique triples(core, term, lookahead). */
    int num_triplets_core_term_lookahead;

    /* The set cores of formed sets are placed in the following os.*/
    os_t set_cores_os;

    /* The dotted_rules of formed sets are placed in the following os.*/
    os_t set_dotted_rules_os;

    /* The indexes of the parent start dotted_rules whose matched_lengths are used
       to get matched_lengths of some not_yet_started dotted_rules are placed in the
       following os.*/
    os_t set_parent_dotted_rule_ids_os;

    /* The matched_lengths of formed sets are placed in the following os.*/
    os_t set_matched_lengths_os;

    /* The sets themself are placed in the following os.*/
    os_t sets_os;

    /* Container for triples(set, term, lookahead. */
    os_t triplet_core_term_lookahead_os;

    /* The following 3 tables contain references for sets which refers
       for set cores or matched_lengths or both which are in the tables.*/
    hash_table_t set_of_cores;	/* key is only start dotted_rules.*/
    hash_table_t set_of_matched_lengthses;	/* key is matched_lengths we have a set of matched_lengthses.*/
    hash_table_t set_of_tuples_core_matched_lengths;	/* key is(core, matched_lengths).*/

    /* Table for triplets (core, term, lookahead). */
    hash_table_t set_of_triplets_core_term_lookahead;	/* key is (core, term, lookeahed). */

    /* The following contains current number of unique dotted_rules. */
    int num_all_dotted_rules;

    /* The following two dimensional array(the first dimension is context
       number, the second one is dotted_rule number) contains references to
       all possible dotted_rules.*/
    YaepDottedRule ***dotted_rules_table;

    /* The following vlo is indexed by dotted_rule context number and gives
       array which is indexed by dotted_rule number
      (dotted_rule->rule->rule_start_offset + dotted_rule->dot_j).*/
    vlo_t dotted_rules_table_vlo;

    /* All dotted_rules are placed in the following object.*/
    os_t dotted_rules_os;

    /* The set of pairs (dotted_rule,matched_length) used for test-setting such pairs
       is implemented using a vec[dotted_rule id] -> vec[matched_length] -> generation since id
       is unique and incrementing, we use a vector[max_id] to find another vector[max_matched_length]
       each matched_length entry storing a generation number. To clear the set of pairs
       we only need to increment the current generation below. Yay! No need to free, alloc, memset.*/
    vlo_t dotted_rule_matched_length_vec_vlo;

    /* The value used to check the validity of elements of check_dist structures. */
    int dotted_rule_matched_length_vec_generation;

    /* The following are number of unique(set core, symbol) pairs and
       their summary(transitive) prediction and reduce vectors length,
       unique(transitive) prediction vectors and their summary length,
       and unique reduce vectors and their summary length. */
    int n_core_symb_pairs, n_core_symb_ids_len;
    int n_transition_vects, n_transition_vect_len;
    int n_reduce_vects, n_reduce_vect_len;

    /* All triples(set core, symbol, vect) are placed in the following object. */
    os_t core_symb_ids_os;

    /* Pointers to triples(set core, symbol, vect) being formed are
       placed in the following object. */
    vlo_t new_core_symb_ids_vlo;

    /* All elements of vectors (transitive_)predictions and completions, are placed in the following os. */
    os_t vect_ids_os;

#ifdef USE_CORE_SYMB_HASH_TABLE
    hash_table_t map_core_symb_to_vect;	/* key is set_core and symb.*/
#else
    /* The following two variables contains table(set core,
       symbol)->core_symb_ids implemented as two dimensional array.*/
    /* The following object contains pointers to the table rows for each
       set core.*/
    vlo_t core_symb_table_vlo;

    /* The following is always start of the previous object.*/
    YaepCoreSymbVect ***core_symb_table;

    /* The following contains rows of the table.  The element in the rows
       are indexed by symbol number.*/
    os_t core_symb_tab_rows;
#endif

    /* The following tables contains references for core_symb_ids which
       (through(transitive) predictions and completions correspondingly)
       refers for elements which are in the tables.  Sequence elements are
       stored in one exemplar to save memory.*/
    hash_table_t map_transition_to_coresymbvect;	/* key is elements.*/
    hash_table_t map_reduce_to_coresymbvect;	/* key is elements.*/

    /* Store state sets in a growing array. Even though early parser
       specifies a new state set per token, we can reuse a state set if
       the matched lengths are the same. This means that the
       state_set_k can increment fewer times than tok_i. */
    YaepStateSet **state_sets;
    int state_set_k;

    /* The following is number of created terminal, abstract, and
       alternative nodes.*/
    int n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

    /* All tail sets of error recovery are saved in the following os.*/
    os_t recovery_state_tail_sets;

    /* The following variable values is state_set_k and tok_i at error
       recovery start(when the original syntax error has been fixed).*/
    int recovery_start_set_k, recovery_start_tok_i;

    /* The following variable value means that all error sets in pl with
       indexes [back_state_set_frontier, recovery_start_set_k] are being processed or
       have been processed.*/
    int back_state_set_frontier;

    /* The following variable stores original state set tail in reversed order.
       This object only grows.  The last object sets may be used to
       restore original state set in order to try another error recovery state
       (alternative).*/
    vlo_t original_state_set_tail_stack;

    /* The following variable value is last state set element which is original
       set(set before the error_recovery start).*/
    int original_last_state_set_el;

    /// GURKA

    /* This page contains code for work with array of vlos.  It is used
       only to implement abstract data `core_symb_ids'.*/

    /* All vlos being formed are placed in the following object.*/
    vlo_t vlo_array;

    /* The following is current number of elements in vlo_array.*/
    int vlo_array_len;

    /* The following table is used to find allocated memory which should not be freed.*/
    hash_table_t set_of_reserved_memory; // (key is memory pointer)

    /* The following vlo will contain references to memory which should be
       freed.  The same reference can be represented more on time.*/
    vlo_t tnodes_vlo;

    /* The key of the following table is node itself.*/
    hash_table_t map_node_to_visit;

    /* All translation visit nodes are placed in the following stack.  All
       the nodes are in the table.*/
    os_t node_visits_os;

    /* The following value is number of translation visit nodes.*/
    int num_nodes_visits;

    /* How many times we reuse Earley's sets without their recalculation. */
    int n_goto_successes;

    /* The following vlo is error recovery states stack.  The stack
       contains error recovery state which should be investigated to find
       the best error recovery.*/
    vlo_t recovery_state_stack;

    /* The following os contains all allocated parser states.*/
    os_t parse_state_os;

    /* The following variable refers to head of chain of already allocated
       and then freed parser states.*/
    YaepParseTreeBuildState *free_parse_state;

    /* The following table is used to make translation for ambiguous
       grammar more compact.  It is used only when we want all
       translations.*/
    hash_table_t map_rule_orig_statesetind_to_internalstate;	/* Key is rule, origin, state_set_k.*/
};
typedef struct YaepParseState YaepParseState;

#define CHECK_PARSE_STATE_MAGIC(ps) (ps->magic_cookie == 736268273)
#define INSTALL_PARSE_STATE_MAGIC(ps) ps->magic_cookie=736268273

// Declarations ///////////////////////////////////////////////////

static void error_recovery(YaepParseState *ps, int *start, int *stop);
static void error_recovery_init(YaepParseState *ps);
static void free_error_recovery(YaepParseState *ps);
static void read_input(YaepParseState *ps);
static void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node);
static void print_rule_with_dot(YaepParseState *ps, FILE *f, YaepRule *rule, int pos);
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, bool trans_p);
static void print_state_set(YaepParseState *ps,
                            FILE* f,
                            YaepStateSet*set,
                            int set_dist,
                            int print_all_dotted_rules,
                            int lookahead_p);
static void print_dotted_rule(YaepParseState *ps, FILE *f, const char *prefix, const char *postfix,
                              YaepDottedRule *dotted_rule, bool lookahead_p, int origin);
static YaepSymbolStorage *symbolstorage_create(YaepGrammar *g);
static void symb_empty(YaepParseState *ps, YaepSymbolStorage *symbs);
static void symb_finish_adding_terms(YaepParseState *ps);
static void symbol_print(FILE* f, YaepSymbol *symb, bool code_p);
static void yaep_error(YaepParseState *ps, int code, const char*format, ...);
static int default_read_token(YaepParseRun *ps, void **attr);

// Global variables /////////////////////////////////////////////////////

/* Jump buffer for processing errors.*/
jmp_buf error_longjump_buff;

// Implementations ////////////////////////////////////////////////////////////////////

static unsigned symb_repr_hash(hash_table_entry_t s)
{
    YaepSymbol *sym = (YaepSymbol*)s;
    unsigned result = jauquet_prime_mod32;

    for (const char *i = sym->repr; *i; i++)
    {
        result = result * hash_shift +(unsigned)*i;
    }

     return result;
}

static bool symb_repr_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymbol *sym1 = (YaepSymbol*)s1;
    YaepSymbol *sym2 = (YaepSymbol*)s2;

    return !strcmp(sym1->repr, sym2->repr);
}

static unsigned symb_code_hash(hash_table_entry_t s)
{
    YaepSymbol *sym = (YaepSymbol*)s;

    assert(sym->terminal_p);

    return sym->u.terminal.code;
}

static bool symb_code_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymbol *sym1 = (YaepSymbol*)s1;
    YaepSymbol *sym2 = (YaepSymbol*)s2;

    assert(sym1->terminal_p && sym2->terminal_p);

    return sym1->u.terminal.code == sym2->u.terminal.code;
}

static YaepSymbolStorage *symbolstorage_create(YaepGrammar *grammar)
{
    void*mem;
    YaepSymbolStorage*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepSymbolStorage));
    result =(YaepSymbolStorage*) mem;
    OS_CREATE(result->symbs_os, grammar->alloc, 0);
    VLO_CREATE(result->symbs_vlo, grammar->alloc, 1024);
    VLO_CREATE(result->terminals_vlo, grammar->alloc, 512);
    VLO_CREATE(result->nonterminals_vlo, grammar->alloc, 512);
    result->map_repr_to_symb = create_hash_table(grammar->alloc, 300, symb_repr_hash, symb_repr_eq);
    result->map_code_to_symb = create_hash_table(grammar->alloc, 200, symb_code_hash, symb_code_eq);
    result->symb_code_trans_vect = NULL;
    result->num_nonterminals = 0;
    result->num_terminals = 0;

    return result;
}

/* Return symbol(or NULL if it does not exist) whose representation is REPR.*/
static YaepSymbol *symb_find_by_repr(YaepParseState *ps, const char*repr)
{
    YaepSymbol symb;
    symb.repr = repr;
    YaepSymbol*r = (YaepSymbol*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, false);

    TRACE_FA(ps, "%s -> %p", repr, r);

    return r;
}

/* Return symbol(or NULL if it does not exist) which is terminal with CODE. */
static YaepSymbol *symb_find_by_code(YaepParseState *ps, int code)
{
    YaepSymbol symb;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        if ((code < ps->run.grammar->symbs_ptr->symb_code_trans_vect_start) ||(code >= ps->run.grammar->symbs_ptr->symb_code_trans_vect_end))
        {
            TRACE_FA(ps, "vec '%c' -> NULL", code);
            return NULL;
        }
        else
        {
            YaepSymbol *r = ps->run.grammar->symbs_ptr->symb_code_trans_vect[code - ps->run.grammar->symbs_ptr->symb_code_trans_vect_start];
            TRACE_FA(ps, "vec '%c' -> %p", code, r);
            return r;
        }
    }

    symb.terminal_p = true;
    symb.u.terminal.code = code;
    YaepSymbol*r =(YaepSymbol*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_code_to_symb, &symb, false);

    TRACE_FA(ps, "hash '%c' -> %p", code, r);

    return r;
}

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
static YaepSymbol *symb_add_terminal(YaepParseState *ps, const char*name, int code)
{
    YaepSymbol symb, *result;
    hash_table_entry_t *repr_entry, *code_entry;

    symb.repr = name;
    if (code >= 32 && code <= 126)
    {
        snprintf(symb.hr, 6, "'%c'", code);
    }
    else
    {
        strncpy(symb.hr, name, 6);
    }
    symb.terminal_p = true;
    // Assign the next available id.
    symb.id = ps->run.grammar->symbs_ptr->num_nonterminals + ps->run.grammar->symbs_ptr->num_terminals;
    symb.u.terminal.code = code;
    symb.u.terminal.term_id = ps->run.grammar->symbs_ptr->num_terminals++;
    symb.empty_p = false;
    repr_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, true);
    assert(*repr_entry == NULL);
    code_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_code_to_symb, &symb, true);
    assert(*code_entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymbol));
    result =(YaepSymbol*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *repr_entry =(hash_table_entry_t) result;
   *code_entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymbol*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->terminals_vlo, &result, sizeof(YaepSymbol*));

    TRACE_FA(ps, "%s %d -> %p", name, code, result);

    return result;
}

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table. The function
   should create own copy of name for the new symbol. */
static YaepSymbol *symb_add_nonterm(YaepParseState *ps, const char *name)
{
    YaepSymbol symb,*result;
    hash_table_entry_t*entry;

    symb.repr = name;
    strncpy(symb.hr, name, 6);

    symb.terminal_p = false;
    // Assign the next available id.
    symb.id = ps->run.grammar->symbs_ptr->num_nonterminals + ps->run.grammar->symbs_ptr->num_terminals;
    symb.u.nonterminal.rules = NULL;
    symb.u.nonterminal.loop_p = false;
    symb.u.nonterminal.nonterm_id = ps->run.grammar->symbs_ptr->num_nonterminals++;
    entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, true);
    assert(*entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymbol));
    result =(YaepSymbol*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymbol*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->nonterminals_vlo, &result, sizeof(YaepSymbol*));

    TRACE_FA(ps, "%s -> %p", name, result);

    return result;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymbol *symb_get(YaepParseState *ps, int id)
{
    if (id < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->symbs_vlo) / sizeof(YaepSymbol*) <=(size_t) id))
    {
        return NULL;
    }
    YaepSymbol **vect = (YaepSymbol**)VLO_BEGIN(ps->run.grammar->symbs_ptr->symbs_vlo);
    YaepSymbol *symb = vect[id];
    assert(symb->id == id);

    TRACE_FA(ps, "%d -> %s", id, symb->hr);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymbol *term_get(YaepParseState *ps, int n)
{
    if (n < 0 || (VLO_LENGTH(ps->run.grammar->symbs_ptr->terminals_vlo) / sizeof(YaepSymbol*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymbol*symb =((YaepSymbol**) VLO_BEGIN(ps->run.grammar->symbs_ptr->terminals_vlo))[n];
    assert(symb->terminal_p && symb->u.terminal.term_id == n);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymbol *nonterm_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->nonterminals_vlo) / sizeof(YaepSymbol*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymbol*symb =((YaepSymbol**) VLO_BEGIN(ps->run.grammar->symbs_ptr->nonterminals_vlo))[n];
    assert(!symb->terminal_p && symb->u.nonterminal.nonterm_id == n);

    return symb;
}

static void symb_finish_adding_terms(YaepParseState *ps)
{
    int i, max_code, min_code;
    YaepSymbol *symb;
    void *mem;

    for (min_code = max_code = i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        if (i == 0 || min_code > symb->u.terminal.code) min_code = symb->u.terminal.code;
        if (i == 0 || max_code < symb->u.terminal.code) max_code = symb->u.terminal.code;
    }
    assert(i != 0);
    assert((max_code - min_code) < MAX_SYMB_CODE_TRANS_VECT_SIZE);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect_start = min_code;
    ps->run.grammar->symbs_ptr->symb_code_trans_vect_end = max_code + 1;

    size_t num_codes = max_code - min_code + 1;
    size_t vec_size = num_codes * sizeof(YaepSymbol*);
    mem = yaep_malloc(ps->run.grammar->alloc, vec_size);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect =(YaepSymbol**)mem;

    for(i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        ps->run.grammar->symbs_ptr->symb_code_trans_vect[symb->u.terminal.code - min_code] = symb;
    }

    TRACE_FA(ps, "num_codes=%zu size=%zu", num_codes, vec_size);
}

/* Free memory for symbols. */
static void symb_empty(YaepParseState *ps, YaepSymbolStorage *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
        ps->run.grammar->symbs_ptr->symb_code_trans_vect = NULL;
    }

    empty_hash_table(symbs->map_repr_to_symb);
    empty_hash_table(symbs->map_code_to_symb);
    VLO_NULLIFY(symbs->nonterminals_vlo);
    VLO_NULLIFY(symbs->terminals_vlo);
    VLO_NULLIFY(symbs->symbs_vlo);
    OS_EMPTY(symbs->symbs_os);
    symbs->num_nonterminals = symbs->num_terminals = 0;

    TRACE_FA(ps, "%p\n" , symbs);
}

static void symbolstorage_free(YaepParseState *ps, YaepSymbolStorage *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
    }

    delete_hash_table(ps->run.grammar->symbs_ptr->map_repr_to_symb);
    delete_hash_table(ps->run.grammar->symbs_ptr->map_code_to_symb);
    VLO_DELETE(ps->run.grammar->symbs_ptr->nonterminals_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->terminals_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->symbs_vlo);
    OS_DELETE(ps->run.grammar->symbs_ptr->symbs_os);
    yaep_free(ps->run.grammar->alloc, symbs);

    TRACE_FA(ps, "%p\n" , symbs);
}

static unsigned terminal_bitset_hash(hash_table_entry_t s)
{
    YaepTerminalSet *ts = (YaepTerminalSet*)s;
    terminal_bitset_t *set = ts->set;
    int num_elements = ts->num_elements;
    terminal_bitset_t *bound = set + num_elements;
    unsigned result = jauquet_prime_mod32;

    while (set < bound)
    {
        result = result * hash_shift + *set++;
    }
    return result;
}

/* Equality of terminal sets. */
static bool terminal_bitset_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepTerminalSet *ts1 = (YaepTerminalSet*)s1;
    YaepTerminalSet *ts2 = (YaepTerminalSet*)s2;
    terminal_bitset_t *i = ts1->set;
    terminal_bitset_t *j = ts2->set;

    assert(ts1->num_elements == ts2->num_elements);

    int num_elements = ts1->num_elements;
    terminal_bitset_t *i_bound = i + num_elements;

    while (i < i_bound)
    {
        if (*i++ != *j++)
        {
            return false;
        }
    }
    return true;
}

static YaepTerminalSetStorage *termsetstorage_create(YaepGrammar *grammar)
{
    void *mem;
    YaepTerminalSetStorage *result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepTerminalSetStorage));
    result =(YaepTerminalSetStorage*) mem;
    OS_CREATE(result->terminal_bitset_os, grammar->alloc, 0);
    result->map_terminal_bitset_to_id = create_hash_table(grammar->alloc, 1000, terminal_bitset_hash, terminal_bitset_eq);
    VLO_CREATE(result->terminal_bitset_vlo, grammar->alloc, 4096);
    result->n_term_sets = result->n_term_sets_size = 0;

    return result;
}

static terminal_bitset_t *terminal_bitset_create(YaepParseState *ps, int num_terminals)
{
    int size_bytes;
    terminal_bitset_t *result;

    assert(sizeof(terminal_bitset_t) <= 8);

    size_bytes = sizeof(terminal_bitset_t) * CALC_NUM_ELEMENTS(num_terminals);

    OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->terminal_bitset_os, size_bytes);
    result =(terminal_bitset_t*) OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
    OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
    ps->run.grammar->term_sets_ptr->n_term_sets++;
    ps->run.grammar->term_sets_ptr->n_term_sets_size += size_bytes;

    return result;
}

static void terminal_bitset_clear(terminal_bitset_t* set, int num_terminals)
{
    terminal_bitset_t*bound;
    int size;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = set + size;
    while(set < bound)
    {
       *set++ = 0;
    }
}

static void terminal_bitset_fill(terminal_bitset_t* set, int num_terminals)
{
    terminal_bitset_t*bound;
    int size;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = set + size;
    while(set < bound)
    {
        *set++ = (terminal_bitset_t)-1;
    }
}

/* Copy SRC into DEST. */
static void terminal_bitset_copy(terminal_bitset_t *dest, terminal_bitset_t *src, int num_terminals)
{
    terminal_bitset_t *bound;
    int size;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = dest + size;

    while (dest < bound)
    {
       *dest++ = *src++;
    }
}

/* Add all terminals from set OP with to SET.  Return true if SET has been changed.*/
static bool terminal_bitset_or(terminal_bitset_t *set, terminal_bitset_t *op, int num_terminals)
{
    terminal_bitset_t *bound;
    int size;
    bool changed_p;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = set + size;
    changed_p = false;
    while (set < bound)
    {
        if ((*set |*op) !=*set)
        {
            changed_p = true;
        }
       *set++ |= *op++;
    }
    return changed_p;
}

/* Add terminal with number NUM to SET.  Return true if SET has been changed.*/
static bool terminal_bitset_up(terminal_bitset_t *set, int num, int num_terminals)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;
    bool changed_p;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);
    changed_p = (set[word_offset] & bit_in_word ? false : true);
    set[word_offset] |= bit_in_word;

    return changed_p;
}

/* Remove terminal with number NUM from SET.  Return true if SET has been changed.*/
static bool terminal_bitset_down(terminal_bitset_t *set, int num, int num_terminals)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;
    bool changed_p;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);
    changed_p = (set[word_offset] & bit_in_word ? true : false);
    set[word_offset] &= ~bit_in_word;

    return changed_p;
}

/* Return true if terminal with number NUM is in SET. */
static int terminal_bitset_test(terminal_bitset_t *set, int num, int num_terminals)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);

    return (set[word_offset] & bit_in_word) != 0;
}

/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1(which is always negative).  Don't set after
   insertion!!! */
static int terminal_bitset_insert(YaepParseState *ps, terminal_bitset_t *set)
{
    hash_table_entry_t *entry;
    YaepTerminalSet term_set,*terminal_bitset_ptr;

    term_set.set = set;
    entry = find_hash_table_entry(ps->run.grammar->term_sets_ptr->map_terminal_bitset_to_id, &term_set, true);

    if (*entry != NULL)
    {
        return -((YaepTerminalSet*)*entry)->id - 1;
    }
    else
    {
        OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->terminal_bitset_os, sizeof(YaepTerminalSet));
        terminal_bitset_ptr = (YaepTerminalSet*)OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
        OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
       *entry =(hash_table_entry_t) terminal_bitset_ptr;
        terminal_bitset_ptr->set = set;
        terminal_bitset_ptr->id = (VLO_LENGTH(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo) / sizeof(YaepTerminalSet*));
        terminal_bitset_ptr->num_elements = CALC_NUM_ELEMENTS(ps->run.grammar->symbs_ptr->num_terminals);

        VLO_ADD_MEMORY(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo, &terminal_bitset_ptr, sizeof(YaepTerminalSet*));

        return((YaepTerminalSet*)*entry)->id;
    }
}

/* The following function returns set which is in the table with number NUM. */
static terminal_bitset_t *terminal_bitset_from_table(YaepParseState *ps, int num)
{
    assert(num >= 0);
    assert((long unsigned int)num < VLO_LENGTH(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo) / sizeof(YaepTerminalSet*));

    return ((YaepTerminalSet**)VLO_BEGIN(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo))[num]->set;
}

/* Print terminal SET into file F. */
static void terminal_bitset_print(YaepParseState *ps, FILE *f, terminal_bitset_t *set, int num_terminals)
{
    bool first = true;
    int num_set = 0;
    for (int i = 0; i < num_terminals; i++) num_set += terminal_bitset_test(set, i, num_terminals);

    if (num_set > num_terminals/2)
    {
        // Print the negation
        fprintf(f, "~[");
        for (int i = 0; i < num_terminals; i++)
        {
            if (!terminal_bitset_test(set, i, num_terminals))
            {
                if (!first) fprintf(f, " "); else first = false;
                symbol_print(f, term_get(ps, i), false);
            }
        }
        fprintf(f, "]");
    }

    fprintf(f, "[");
    for (int i = 0; i < num_terminals; i++)
    {
        if (terminal_bitset_test(set, i, num_terminals))
        {
            if (!first) fprintf(f, " "); else first = false;
            symbol_print(f, term_get(ps, i), false);
        }
    }
    fprintf(f, "]");
}

/* Free memory for terminal sets. */
static void terminal_bitset_empty(YaepTerminalSetStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_NULLIFY(term_sets->terminal_bitset_vlo);
    empty_hash_table(term_sets->map_terminal_bitset_to_id);
    OS_EMPTY(term_sets->terminal_bitset_os);
    term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

static void termsetstorage_free(YaepGrammar *grammar, YaepTerminalSetStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_DELETE(term_sets->terminal_bitset_vlo);
    delete_hash_table(term_sets->map_terminal_bitset_to_id);
    OS_DELETE(term_sets->terminal_bitset_os);
    yaep_free(grammar->alloc, term_sets);
    term_sets = NULL;
}


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

    assert(!lhs->terminal_p);

    OS_TOP_EXPAND(ps->run.grammar->rulestorage_ptr->rules_os, sizeof(YaepRule));
    rule =(YaepRule*) OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rulestorage_ptr->rules_os);
    rule->lhs = lhs;
    rule->mark = 0;
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
    YaepSymbol *empty;

    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rulestorage_ptr->rules_os, &empty, sizeof(YaepSymbol*));
    ps->run.grammar->rulestorage_ptr->current_rule->rhs = (YaepSymbol**)OS_TOP_BEGIN(ps->run.grammar->rulestorage_ptr->rules_os);
    ps->run.grammar->rulestorage_ptr->current_rule->rhs[ps->run.grammar->rulestorage_ptr->current_rule->rhs_len] = symb;
    ps->run.grammar->rulestorage_ptr->current_rule->rhs_len++;
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

static void create_dotted_rules(YaepParseState *ps)
{
    ps->num_all_dotted_rules= 0;
    OS_CREATE(ps->dotted_rules_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->dotted_rules_table_vlo, ps->run.grammar->alloc, 4096);
    ps->dotted_rules_table = (YaepDottedRule***)VLO_BEGIN(ps->dotted_rules_table_vlo);
}

/* The following function sets up lookahead of dotted_rule SIT.  The
   function returns true if the dotted_rule tail may derive empty
   string.*/
static bool dotted_rule_set_lookahead(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    YaepSymbol *symb, **symb_ptr;

    if (ps->run.grammar->lookahead_level == 0)
    {
        dotted_rule->lookahead = NULL;
    }
    else
    {
        dotted_rule->lookahead = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
        terminal_bitset_clear(dotted_rule->lookahead, ps->run.grammar->symbs_ptr->num_terminals);
    }
    symb_ptr = &dotted_rule->rule->rhs[dotted_rule->dot_j];
    while ((symb =*symb_ptr) != NULL)
    {
        if (ps->run.grammar->lookahead_level != 0)
	{
            if (symb->terminal_p)
            {
                terminal_bitset_up(dotted_rule->lookahead, symb->u.terminal.term_id, ps->run.grammar->symbs_ptr->num_terminals);
            }
            else
            {
                terminal_bitset_or(dotted_rule->lookahead, symb->u.nonterminal.first, ps->run.grammar->symbs_ptr->num_terminals);
            }
	}
        if (!symb->empty_p) break;
        symb_ptr++;
    }
    if (symb == NULL)
    {
        if (ps->run.grammar->lookahead_level == 1)
        {
            terminal_bitset_or(dotted_rule->lookahead, dotted_rule->rule->lhs->u.nonterminal.follow, ps->run.grammar->symbs_ptr->num_terminals);
        }
        else if (ps->run.grammar->lookahead_level != 0)
        {
            terminal_bitset_or(dotted_rule->lookahead, terminal_bitset_from_table(ps, dotted_rule->context), ps->run.grammar->symbs_ptr->num_terminals);
        }
        return true;
    }
    return false;
}

/* The following function returns dotted_rules with given
   characteristics.  Remember that dotted_rules are stored in one
   exemplar. */
static YaepDottedRule *create_dotted_rule(YaepParseState *ps, YaepRule *rule, int dot_j, int context)
{
    YaepDottedRule *dotted_rule;
    YaepDottedRule ***context_dotted_rules_table_ptr;

    assert(context >= 0);
    context_dotted_rules_table_ptr = ps->dotted_rules_table + context;

    if ((char*) context_dotted_rules_table_ptr >= (char*) VLO_BOUND(ps->dotted_rules_table_vlo))
    {
        YaepDottedRule***bound,***ptr;
        int i, diff;

        assert((ps->run.grammar->lookahead_level <= 1 && context == 0) || (ps->run.grammar->lookahead_level > 1 && context >= 0));
        diff = (char*) context_dotted_rules_table_ptr -(char*) VLO_BOUND(ps->dotted_rules_table_vlo);
        diff += sizeof(YaepDottedRule**);
        if (ps->run.grammar->lookahead_level > 1 && diff == sizeof(YaepDottedRule**))
        {
            diff *= 10;
        }
        VLO_EXPAND(ps->dotted_rules_table_vlo, diff);
        ps->dotted_rules_table =(YaepDottedRule***) VLO_BEGIN(ps->dotted_rules_table_vlo);
        bound =(YaepDottedRule***) VLO_BOUND(ps->dotted_rules_table_vlo);
        context_dotted_rules_table_ptr = ps->dotted_rules_table + context;
        ptr = bound - diff / sizeof(YaepDottedRule**);

        while(ptr < bound)
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
    if ((dotted_rule = (*context_dotted_rules_table_ptr)[rule->rule_start_offset + dot_j]) != NULL)
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
    dotted_rule->context = context;
    dotted_rule->empty_tail_p = dotted_rule_set_lookahead(ps, dotted_rule);
    (*context_dotted_rules_table_ptr)[rule->rule_start_offset + dot_j] = dotted_rule;

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

/* Finalize work with dotted_rules. */
static void free_dotted_rules(YaepParseState *ps)
{
    VLO_DELETE(ps->dotted_rules_table_vlo);
    OS_DELETE(ps->dotted_rules_os);
}

/* Hash of set core. */
static unsigned set_core_hash(hash_table_entry_t s)
{
    return ((YaepStateSet*)s)->core->hash;
}

/* Equality of set cores. */
static bool set_core_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSetCore*set_core1 = ((YaepStateSet*) s1)->core;
    YaepStateSetCore*set_core2 = ((YaepStateSet*) s2)->core;
    YaepDottedRule **dotted_rule_ptr1, **dotted_rule_ptr2, **dotted_rule_bound1;

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

/* Hash of set matched_lengths. */
static unsigned matched_lengths_hash(hash_table_entry_t s)
{
    return((YaepStateSet*) s)->matched_lengths_hash;
}

/* Compare all the matched_lengths stored in the two state sets. */
static bool matched_lengths_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSet *st1 = (YaepStateSet*)s1;
    YaepStateSet *st2 = (YaepStateSet*)s2;
    int *i = st1->matched_lengths;
    int *j = st2->matched_lengths;
    int num_matched_lengths = st1->core->num_started_dotted_rules;

    if (num_matched_lengths != st2->core->num_started_dotted_rules)
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

/* Hash of set core and matched_lengths. */
static unsigned set_core_matched_lengths_hash(hash_table_entry_t s)
{
    return set_core_hash(s)* hash_shift + matched_lengths_hash(s);
}

/* Equality of set cores and matched_lengths. */
static bool set_core_matched_lengths_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSetCore *set_core1 = ((YaepStateSet*)s1)->core;
    YaepStateSetCore *set_core2 = ((YaepStateSet*)s2)->core;
    int*matched_lengths1 = ((YaepStateSet*)s1)->matched_lengths;
    int*matched_lengths2 = ((YaepStateSet*)s2)->matched_lengths;

    return set_core1 == set_core2 && matched_lengths1 == matched_lengths2;
}

/* Hash of triple(set, term, lookahead). */
static unsigned core_term_lookahead_hash(hash_table_entry_t s)
{
    YaepStateSet *set = ((YaepStateSetTermLookAhead*)s)->set;
    YaepSymbol *term = ((YaepStateSetTermLookAhead*)s)->term;
    int lookahead = ((YaepStateSetTermLookAhead*)s)->lookahead;

    return ((set_core_matched_lengths_hash(set)* hash_shift + term->u.terminal.term_id)* hash_shift + lookahead);
}

/* Equality of tripes(set, term, lookahead).*/
static bool core_term_lookahead_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSet *set1 =((YaepStateSetTermLookAhead*)s1)->set;
    YaepStateSet *set2 =((YaepStateSetTermLookAhead*)s2)->set;
    YaepSymbol *term1 =((YaepStateSetTermLookAhead*)s1)->term;
    YaepSymbol *term2 =((YaepStateSetTermLookAhead*)s2)->term;
    int lookahead1 =((YaepStateSetTermLookAhead*)s1)->lookahead;
    int lookahead2 =((YaepStateSetTermLookAhead*)s2)->lookahead;

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
    ps->set_of_cores = create_hash_table(ps->run.grammar->alloc, 2000, set_core_hash, set_core_eq);
    ps->set_of_matched_lengthses = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n, matched_lengths_hash, matched_lengths_eq);
    ps->set_of_tuples_core_matched_lengths = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                set_core_matched_lengths_hash, set_core_matched_lengths_eq);
    ps->set_of_triplets_core_term_lookahead = create_hash_table(ps->run.grammar->alloc, n < 30000 ? 30000 : n,
                                               core_term_lookahead_hash, core_term_lookahead_eq);
    ps->num_set_cores = ps->num_set_core_start_dotted_rules= 0;
    ps->num_set_matched_lengths = ps->num_set_matched_lengths_len = ps->num_parent_dotted_rule_ids = 0;
    ps->n_sets = ps->n_sets_start_dotted_rules= 0;
    ps->num_triplets_core_term_lookahead = 0;
    dotted_rule_matched_length_set_init(ps);
}

/* The following function starts forming of new set.*/
static void set_new_start(YaepParseState *ps)
{
    ps->new_set = NULL;
    ps->new_core = NULL;
    ps->new_set_ready_p = false;
    ps->new_dotted_rules = NULL;
    ps->new_matched_lengths = NULL;
    ps->new_num_started_dotted_rules = 0;
}

/* Add start DOTTED_RULE with matched_length DIST at the end of the dotted_rule array
   of the state set being formed. */
static void set_add_dotted_rule(YaepParseState *ps, YaepDottedRule *dotted_rule, int matched_length)
{
    assert(!ps->new_set_ready_p);
    OS_TOP_EXPAND(ps->set_matched_lengths_os, sizeof(int));
    ps->new_matched_lengths =(int*) OS_TOP_BEGIN(ps->set_matched_lengths_os);
    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_dotted_rules =(YaepDottedRule**) OS_TOP_BEGIN(ps->set_dotted_rules_os);
    ps->new_dotted_rules[ps->new_num_started_dotted_rules] = dotted_rule;
    ps->new_matched_lengths[ps->new_num_started_dotted_rules] = matched_length;
    ps->new_num_started_dotted_rules++;

    TRACE_FA(ps, "id=%d", dotted_rule->id);
}

/* Add not_yet_started, noninitial DOTTED_RULE with matched_length DIST at the end of the
   dotted_rule array of the set being formed.  If this is dotted_rule and
   there is already the same pair(dotted_rule, the corresponding
   matched_length), we do not add it.*/
static void set_add_nys_dotted_rule(YaepParseState *ps,
                                                    YaepDottedRule *dotted_rule,
                                                    int parent_dotted_rule_id)
{
    assert(ps->new_set_ready_p);

    /* When we add not-yet-started dotted_rules we need to have pairs
       (dotted_rule + parent_dotted_rule_id) without duplicates
       because we also form core_symb_ids at that time. */
    for(int id = ps->new_num_started_dotted_rules; id < ps->new_core->num_dotted_rules; id++)
    {
        if (ps->new_dotted_rules[id] == dotted_rule &&
            ps->new_core->parent_dotted_rule_ids[id] == parent_dotted_rule_id)
        {
            // The dotted_rule + parent dotted rule already existed.
            return;
        }
    }

    // Increase the object stack storing dotted_rules, with the size of a new dotted_rule.
    OS_TOP_EXPAND(ps->set_dotted_rules_os, sizeof(YaepDottedRule*));
    ps->new_dotted_rules = ps->new_core->dotted_rules = (YaepDottedRule**)OS_TOP_BEGIN(ps->set_dotted_rules_os);

    // Increase the parent index vector with another int.
    // This integer points to ...?
    OS_TOP_EXPAND(ps->set_parent_dotted_rule_ids_os, sizeof(int));
    ps->new_core->parent_dotted_rule_ids = (int*)OS_TOP_BEGIN(ps->set_parent_dotted_rule_ids_os) - ps->new_num_started_dotted_rules;

    // Store dotted_rule into new dotted_rules.
    ps->new_dotted_rules[ps->new_core->num_dotted_rules++] = dotted_rule;
    // Store parent index. Meanst what...?
    ps->new_core->parent_dotted_rule_ids[ps->new_core->num_all_matched_lengths++] = parent_dotted_rule_id;
    ps->num_parent_dotted_rule_ids++;

    TRACE_FA(ps, "id=%d", dotted_rule->id);
}

/* Add a not-yet-started dotted_rule (initial) DOTTED_RULE with zero matched_length at the end of the
   dotted_rule array of the set being formed.  If this is not-yet-started
   dotted_rule and there is already the same pair(dotted_rule, zero matched_length), we do not add it.*/
static void set_add_initial_dotted_rule(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    assert(ps->new_set_ready_p);

    /* When we add not-yet-started dotted_rules we need to have pairs
      (dotted_rule, the corresponding matched_length) without duplicates
       because we also form core_symb_ids at that time.*/
    for (int i = ps->new_num_started_dotted_rules; i < ps->new_core->num_dotted_rules; i++)
    {
        // Check if already added.
        if (ps->new_dotted_rules[i] == dotted_rule) return;
    }
    /* Remember we do not store matched_length for not-yet-started dotted_rules.*/
    OS_TOP_ADD_MEMORY(ps->set_dotted_rules_os, &dotted_rule, sizeof(YaepDottedRule*));
    ps->new_dotted_rules = ps->new_core->dotted_rules = (YaepDottedRule**)OS_TOP_BEGIN(ps->set_dotted_rules_os);
    ps->new_core->num_dotted_rules++;
}

/* Set up hash of matched_lengths of set S.*/
static void setup_set_matched_lengths_hash(hash_table_entry_t s)
{
    YaepStateSet *set = ((YaepStateSet*) s);
    int*dist_ptr = set->matched_lengths;
    int num_matched_lengths = set->core->num_started_dotted_rules;
    int*dist_bound;
    unsigned result;

    dist_bound = dist_ptr + num_matched_lengths;
    result = jauquet_prime_mod32;
    while(dist_ptr < dist_bound)
    {
        result = result* hash_shift +*dist_ptr++;
    }
    set->matched_lengths_hash = result;
}

/* Set up hash of core of set S.*/
static void setup_set_core_hash(hash_table_entry_t s)
{
    YaepStateSetCore*set_core =((YaepStateSet*) s)->core;

    set_core->hash = dotted_rules_hash(set_core->num_started_dotted_rules, set_core->dotted_rules);
}

/* The new set should contain only start dotted_rules.  Sort dotted_rules,
   remove duplicates and insert set into the set table.  If the
   function returns true then set contains new set core(there was no
   such core in the table). */
static int set_insert(YaepParseState *ps)
{
    hash_table_entry_t*entry;
    int result;

    OS_TOP_EXPAND(ps->sets_os, sizeof(YaepStateSet));
    ps->new_set = (YaepStateSet*)OS_TOP_BEGIN(ps->sets_os);
    ps->new_set->matched_lengths = ps->new_matched_lengths;
    OS_TOP_EXPAND(ps->set_cores_os, sizeof(YaepStateSetCore));
    ps->new_set->core = ps->new_core = (YaepStateSetCore*) OS_TOP_BEGIN(ps->set_cores_os);
    ps->new_core->num_started_dotted_rules = ps->new_num_started_dotted_rules;
    ps->new_core->dotted_rules = ps->new_dotted_rules;
    ps->new_set_ready_p = true;
#ifdef USE_SET_HASH_TABLE
    /* Insert matched_lengths into table. */
    setup_set_matched_lengths_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_of_matched_lengthses, ps->new_set, true);
    if (*entry != NULL)
    {
        ps->new_matched_lengths = ps->new_set->matched_lengths =((YaepStateSet*)*entry)->matched_lengths;
        OS_TOP_NULLIFY(ps->set_matched_lengths_os);
    }
    else
    {
        OS_TOP_FINISH(ps->set_matched_lengths_os);
       *entry =(hash_table_entry_t)ps->new_set;
        ps->num_set_matched_lengths++;
        ps->num_set_matched_lengths_len += ps->new_num_started_dotted_rules;
    }
#else
    OS_TOP_FINISH(ps->set_matched_lengths_os);
    ps->num_set_matched_lengths++;
    ps->num_set_matched_lengths_len += ps->new_num_started_dotted_rules;
#endif
    /* Insert set core into table.*/
    setup_set_core_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_of_cores, ps->new_set, true);
    if (*entry != NULL)
    {
        OS_TOP_NULLIFY(ps->set_cores_os);
        ps->new_set->core = ps->new_core = ((YaepStateSet*)*entry)->core;
        ps->new_dotted_rules = ps->new_core->dotted_rules;
        OS_TOP_NULLIFY(ps->set_dotted_rules_os);
        result = false;
    }
    else
    {
        OS_TOP_FINISH(ps->set_cores_os);
        ps->new_core->id = ps->num_set_cores++;
        ps->new_core->num_dotted_rules = ps->new_num_started_dotted_rules;
        ps->new_core->num_all_matched_lengths = ps->new_num_started_dotted_rules;
        ps->new_core->parent_dotted_rule_ids = NULL;
       *entry =(hash_table_entry_t)ps->new_set;
        ps->num_set_core_start_dotted_rules+= ps->new_num_started_dotted_rules;
        result = true;
    }
#ifdef USE_SET_HASH_TABLE
    /* Insert set into table.*/
    entry = find_hash_table_entry(ps->set_of_tuples_core_matched_lengths, ps->new_set, true);
    if (*entry == NULL)
    {
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_sets++;
        ps->n_sets_start_dotted_rules+= ps->new_num_started_dotted_rules;
        OS_TOP_FINISH(ps->sets_os);
    }
    else
    {
        ps->new_set = (YaepStateSet*)*entry;
        OS_TOP_NULLIFY(ps->sets_os);
    }
#else
    OS_TOP_FINISH(ps->sets_os);
#endif
    return result;
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
    delete_hash_table(ps->set_of_triplets_core_term_lookahead);
    delete_hash_table(ps->set_of_tuples_core_matched_lengths);
    delete_hash_table(ps->set_of_matched_lengthses);
    delete_hash_table(ps->set_of_cores);
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
/* Hash of core_symb_ids.*/
static unsigned core_symb_ids_hash(hash_table_entry_t t)
{
    YaepCoreSymbVect*core_symb_ids =(YaepCoreSymbVect*) t;

    return((jauquet_prime_mod32* hash_shift
            +(size_t)/* was unsigned */core_symb_ids->core)* hash_shift
           +(size_t)/* was unsigned */core_symb_ids->symb);
}

/* Equality of core_symb_idss.*/
static bool core_symb_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    YaepCoreSymbVect*core_symb_ids1 =(YaepCoreSymbVect*) t1;
    YaepCoreSymbVect*core_symb_ids2 =(YaepCoreSymbVect*) t2;

    return(core_symb_ids1->core == core_symb_ids2->core
            && core_symb_ids1->symb == core_symb_ids2->symb);
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
    return vect_ids_hash(&((YaepCoreSymbVect*)t)->predictions);
}

static bool prediction_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbVect*)t1)->predictions,
                       &((YaepCoreSymbVect*)t2)->predictions);
}

static unsigned completion_ids_hash(hash_table_entry_t t)
{
    return vect_ids_hash(&((YaepCoreSymbVect*)t)->completions);
}

static bool completion_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbVect*) t1)->completions,
                       &((YaepCoreSymbVect*) t2)->completions);
}

/* Initialize work with the triples(set core, symbol, vector).*/
static void core_symb_ids_init(YaepParseState *ps)
{
    OS_CREATE(ps->core_symb_ids_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->new_core_symb_ids_vlo, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->vect_ids_os, ps->run.grammar->alloc, 0);

    vlo_array_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    ps->map_core_symb_to_vect = create_hash_table(ps->run.grammar->alloc, 3000, core_symb_ids_hash, core_symb_ids_eq);
#else
    VLO_CREATE(ps->core_symb_table_vlo, ps->run.grammar->alloc, 4096);
    ps->core_symb_table = (YaepCoreSymbVect***)VLO_BEGIN(ps->core_symb_table_vlo);
    OS_CREATE(ps->core_symb_tab_rows, ps->run.grammar->alloc, 8192);
#endif

    ps->map_transition_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000, prediction_ids_hash, prediction_ids_eq);
    ps->map_reduce_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000, completion_ids_hash, completion_ids_eq);

    ps->n_core_symb_pairs = ps->n_core_symb_ids_len = 0;
    ps->n_transition_vects = ps->n_transition_vect_len = 0;
    ps->n_reduce_vects = ps->n_reduce_vect_len = 0;
}

#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed.*/
static YaepCoreSymbVect **core_symb_ids_addr_get(YaepParseState *ps, YaepCoreSymbVect *triple, int reserv_p)
{
    YaepCoreSymbVect**result;

    if (triple->symb->cached_core_symb_ids != NULL
        && triple->symb->cached_core_symb_ids->core == triple->core)
    {
        return &triple->symb->cached_core_symb_ids;
    }

    result = ((YaepCoreSymbVect**)find_hash_table_entry(ps->map_core_symb_to_vect, triple, reserv_p));

    triple->symb->cached_core_symb_ids = *result;

    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed. */
static YaepCoreSymbVect **core_symb_ids_addr_get(YaepParseState *ps, YaepStateSetCore *set_core, YaepSymbol *symb)
{
    YaepCoreSymbVect***core_symb_ids_ptr;

    core_symb_ids_ptr = ps->core_symb_table + set_core->id;

    if ((char*) core_symb_ids_ptr >=(char*) VLO_BOUND(ps->core_symb_table_vlo))
    {
        YaepCoreSymbVect***ptr,***bound;
        int diff, i;

        diff =((char*) core_symb_ids_ptr
                -(char*) VLO_BOUND(ps->core_symb_table_vlo));
        diff += sizeof(YaepCoreSymbVect**);
        if (diff == sizeof(YaepCoreSymbVect**))
            diff*= 10;

        VLO_EXPAND(ps->core_symb_table_vlo, diff);
        ps->core_symb_table
            =(YaepCoreSymbVect***) VLO_BEGIN(ps->core_symb_table_vlo);
        core_symb_ids_ptr = ps->core_symb_table + set_core->id;
        bound =(YaepCoreSymbVect***) VLO_BOUND(ps->core_symb_table_vlo);

        ptr = bound - diff / sizeof(YaepCoreSymbVect**);
        while(ptr < bound)
        {
            OS_TOP_EXPAND(ps->core_symb_tab_rows,
                          (ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals)
                          * sizeof(YaepCoreSymbVect*));
           *ptr =(YaepCoreSymbVect**) OS_TOP_BEGIN(ps->core_symb_tab_rows);
            OS_TOP_FINISH(ps->core_symb_tab_rows);
            for(i = 0; i < ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals; i++)
               (*ptr)[i] = NULL;
            ptr++;
        }
    }
    return &(*core_symb_ids_ptr)[symb->id];
}
#endif

/* The following function returns the triple(if any) for given SET_CORE and SYMB. */
static YaepCoreSymbVect *core_symb_ids_find(YaepParseState *ps, YaepStateSetCore *core, YaepSymbol *symb)
{
    YaepCoreSymbVect *r;

#ifdef USE_CORE_SYMB_HASH_TABLE
    YaepCoreSymbVect core_symb_ids;

    core_symb_ids.core = core;
    core_symb_ids.symb = symb;
    r = *core_symb_ids_addr_get(ps, &core_symb_ids, false);
#else
    r = *core_symb_ids_addr_get(ps, core, symb);
#endif

    TRACE_FA(ps, "core=%d %s -> %p", core->id, symb->hr, r);

    return r;
}

/* Add given triple(SET_CORE, TERM, ...) to the table and return
   pointer to it.*/
static YaepCoreSymbVect *core_symb_ids_new(YaepParseState *ps, YaepStateSetCore*core, YaepSymbol*symb)
{
    YaepCoreSymbVect*triple;
    YaepCoreSymbVect**addr;
    vlo_t*vlo_ptr;

    /* Create table element.*/
    OS_TOP_EXPAND(ps->core_symb_ids_os, sizeof(YaepCoreSymbVect));
    triple =((YaepCoreSymbVect*) OS_TOP_BEGIN(ps->core_symb_ids_os));
    triple->core = core;
    triple->symb = symb;
    OS_TOP_FINISH(ps->core_symb_ids_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_ids_addr_get(ps, triple, true);
#else
    addr = core_symb_ids_addr_get(ps, core, symb);
#endif
    assert(*addr == NULL);
   *addr = triple;

    triple->predictions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->predictions.intern);
    triple->predictions.len = 0;
    triple->predictions.ids =(int*) VLO_BEGIN(*vlo_ptr);

    triple->completions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->completions.intern);
    triple->completions.len = 0;
    triple->completions.ids =(int*) VLO_BEGIN(*vlo_ptr);
    VLO_ADD_MEMORY(ps->new_core_symb_ids_vlo, &triple,
                    sizeof(YaepCoreSymbVect*));
    ps->n_core_symb_pairs++;
    return triple;
}

static void vect_add_id(YaepParseState *ps, YaepVect *vec, int id)
{
    vec->len++;
    vlo_t *vlo_ptr = vlo_array_el(ps, vec->intern);
    VLO_ADD_MEMORY(*vlo_ptr, &id, sizeof(int));
    vec->ids =(int*) VLO_BEGIN(*vlo_ptr);
    ps->n_core_symb_ids_len++;
}

/* Add index EL to the transition vector of CORE_SYMB_IDS being formed.*/
static void core_symb_ids_add_predict(YaepParseState *ps,
                                      YaepCoreSymbVect *core_symb_ids,
                                      int id)
{
    vect_add_id(ps, &core_symb_ids->predictions, id);

    TRACE_FA(ps, "core=%d %s --> id=%d",
             core_symb_ids->core->id,
             core_symb_ids->symb->hr,
             id+1);
}

/* Add index id to the reduce vector of CORE_SYMB_IDS being formed.*/
static void core_symb_ids_add_complete(YaepParseState *ps,
                                       YaepCoreSymbVect *core_symb_ids,
                                       int id)
{
    vect_add_id(ps, &core_symb_ids->completions, id);
}

/* Insert vector VEC from CORE_SYMB_IDS into table TAB.  Update
   *N_VECTS and INT*N_VECT_LEN if it is a new vector in the table. */
static void process_core_symb_ids_el(YaepParseState *ps,
                                      YaepCoreSymbVect *core_symb_ids,
                                      YaepVect *vec,
                                      hash_table_t *tab,
                                      int *n_vects,
                                      int *n_vect_len)
{
    hash_table_entry_t*entry;

    if (vec->len == 0)
        vec->ids = NULL;
    else
    {
        entry = find_hash_table_entry(*tab, core_symb_ids, true);
        if (*entry != NULL)
            vec->ids
                =(&core_symb_ids->predictions == vec
                   ?((YaepCoreSymbVect*)*entry)->predictions.ids
                   :((YaepCoreSymbVect*)*entry)->completions.ids);
        else
	{
           *entry =(hash_table_entry_t) core_symb_ids;
            OS_TOP_ADD_MEMORY(ps->vect_ids_os, vec->ids, vec->len* sizeof(int));
            vec->ids =(int*) OS_TOP_BEGIN(ps->vect_ids_os);
            OS_TOP_FINISH(ps->vect_ids_os);
           (*n_vects)++;
           *n_vect_len += vec->len;
	}
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_ids.*/
static void core_symb_ids_new_all_stop(YaepParseState *ps)
{
    YaepCoreSymbVect**triple_ptr;

    for(triple_ptr =(YaepCoreSymbVect**) VLO_BEGIN(ps->new_core_symb_ids_vlo);
        (char*) triple_ptr <(char*) VLO_BOUND(ps->new_core_symb_ids_vlo);
         triple_ptr++)
    {
        process_core_symb_ids_el(ps, *triple_ptr, &(*triple_ptr)->predictions,
                                  &ps->map_transition_to_coresymbvect, &ps->n_transition_vects,
                                  &ps->n_transition_vect_len);
        process_core_symb_ids_el(ps, *triple_ptr, &(*triple_ptr)->completions,
                                  &ps->map_reduce_to_coresymbvect, &ps->n_reduce_vects,
                                  &ps->n_reduce_vect_len);
    }
    vlo_array_nullify(ps);
    VLO_NULLIFY(ps->new_core_symb_ids_vlo);
}

/* Finalize work with all triples(set core, symbol, vector).*/
static void free_core_symb_to_vect_lookup(YaepParseState *ps)
{
    delete_hash_table(ps->map_transition_to_coresymbvect);
    delete_hash_table(ps->map_reduce_to_coresymbvect);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table(ps->map_core_symb_to_vect);
#else
    OS_DELETE(ps->core_symb_tab_rows);
    VLO_DELETE(ps->core_symb_table_vlo);
#endif
    free_vlo_array(ps);
    OS_DELETE(ps->vect_ids_os);
    VLO_DELETE(ps->new_core_symb_ids_vlo);
    OS_DELETE(ps->core_symb_ids_os);
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
    longjmp(error_longjump_buff, code);
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
    if (setjmp(error_longjump_buff) != 0)
    {
        //yaepFreeGrammar(pr, grammar);
        assert(0);
        return NULL;
    }
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
        symb->u.nonterminal.first = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
        terminal_bitset_clear(symb->u.nonterminal.first, ps->run.grammar->symbs_ptr->num_terminals);
        symb->u.nonterminal.follow = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
        terminal_bitset_clear(symb->u.nonterminal.follow, ps->run.grammar->symbs_ptr->num_terminals);
    }
    do
    {
        changed_p = false;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterminal.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                first_continue_p = true;
                rhs = rule->rhs;
                rhs_len = rule->rhs_len;
                for(j = 0; j < rhs_len; j++)
                {
                    rhs_symb = rhs[j];
                    if (rhs_symb->terminal_p)
                    {
                        if (first_continue_p)
                            changed_p |= terminal_bitset_up(symb->u.nonterminal.first,
                                                     rhs_symb->u.terminal.term_id,
                                                     ps->run.grammar->symbs_ptr->num_terminals);
                    }
                    else
                    {
                        if (first_continue_p)
                            changed_p |= terminal_bitset_or(symb->u.nonterminal.first,
                                                     rhs_symb->u.nonterminal.first,
                                                     ps->run.grammar->symbs_ptr->num_terminals);
                        for(k = j + 1; k < rhs_len; k++)
                        {
                            next_rhs_symb = rhs[k];
                            if (next_rhs_symb->terminal_p)
                                changed_p
                                    |= terminal_bitset_up(rhs_symb->u.nonterminal.follow,
                                                   next_rhs_symb->u.terminal.term_id,
                                                   ps->run.grammar->symbs_ptr->num_terminals);
                            else
                                changed_p
                                    |= terminal_bitset_or(rhs_symb->u.nonterminal.follow,
                                                   next_rhs_symb->u.nonterminal.first,
                                                   ps->run.grammar->symbs_ptr->num_terminals);
                            if (!next_rhs_symb->empty_p)
                                break;
                        }
                        if (k == rhs_len)
                            changed_p |= terminal_bitset_or(rhs_symb->u.nonterminal.follow,
                                                     symb->u.nonterminal.follow,
                                                     ps->run.grammar->symbs_ptr->num_terminals);
                    }
                    if (!rhs_symb->empty_p)
                        first_continue_p = false;
                }
            }
    }
    while(changed_p);
}

/* The following function sets up flags empty_p, access_p and
   derivation_p for all grammar symbols.*/
static void set_empty_access_derives(YaepParseState *ps)
{
    YaepSymbol*symb,*rhs_symb;
    YaepRule*rule;
    bool empty_p, derivation_p;
    bool empty_changed_p, derivation_changed_p, accessibility_change_p;
    int i, j;

    for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
    {
        symb->empty_p = false;
        symb->derivation_p = (symb->terminal_p ? true : false);
        symb->access_p = false;
    }
    ps->run.grammar->axiom->access_p = 1;
    do
    {
        empty_changed_p = derivation_changed_p = accessibility_change_p = false;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterminal.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                empty_p = derivation_p = 1;
                for(j = 0; j < rule->rhs_len; j++)
                {
                    rhs_symb = rule->rhs[j];
                    if (symb->access_p)
                    {
                        accessibility_change_p |= rhs_symb->access_p ^ 1;
                        rhs_symb->access_p = 1;
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
            if (!(symb = rule->rhs[i])->terminal_p)
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
                        if (!(symb = rule->rhs[j])->terminal_p && symb->u.nonterminal.loop_p)
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
#define AXIOM_NAME "$S"
#define END_MARKER_NAME "$eof"
#define TERM_ERROR_NAME "error"

/* It should be negative.*/
#define END_MARKER_CODE -1
#define TERM_ERROR_CODE -2

/* The following function reads terminals/rules.  The function returns
   pointer to the grammar(or NULL if there were errors in
   grammar).*/
int yaep_read_grammar(YaepParseRun *pr,
                      YaepGrammar *g,
                      int strict_p,
                      const char*(*read_terminal)(YaepParseRun*pr,YaepGrammar*g,int*code),
                      const char*(*read_rule)(YaepParseRun*pr,YaepGrammar*g,const char***rhs,
                                              const char**abs_node,
                                              int*anode_cost, int**transl, char*mark, char**marks))
{
    const char*name,*lhs,**rhs,*anode;
    YaepSymbol*symb,*start;
    YaepRule*rule;
    int anode_cost;
    int*transl;
    char mark;
    char*marks;
    int i, el, code;

    assert(g != NULL);
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));

    if ((code = setjmp(error_longjump_buff)) != 0)
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
        else if (symb->terminal_p)
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
/*        printf("MARKS %s >", lhs);
        for (int i=0; i<rhs_len; ++i) printf("%c", rule->marks[i]);
        printf("<\n");*/
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
        fprintf(stderr, "(ixml) ----- grammar\n");
        for(rule = ps->run.grammar->rulestorage_ptr->first_rule; rule != NULL; rule = rule->next)
	{
            if (rule->lhs->repr[0] != '$')
            {
                rule_print(ps, stderr, rule, true);
            }
	}
        fprintf(stderr, "\n");
        /* Print symbol sets.*/
        if (ps->run.debug)
        {
            fprintf(stderr, "(ixml) lookahead\n");

            for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            {
                fprintf(stderr, "lh %s%s%s%s\n",
                        symb->repr,(symb->empty_p ? " CAN_BECOME_EMPTY" : ""),
                        (symb->access_p ? "" : " OUPS_NOT_REACHABLE"),
                        (symb->derivation_p ? "" : " OUPS_NO_TEXT"));
                fprintf(stderr, "  1st: ");
                terminal_bitset_print(ps, stderr, symb->u.nonterminal.first, ps->run.grammar->symbs_ptr->num_terminals);
                fprintf(stderr, "\n  2nd: ");
                terminal_bitset_print(ps, stderr, symb->u.nonterminal.follow, ps->run.grammar->symbs_ptr->num_terminals);
                fprintf(stderr, "\n\n");
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

/* The function initializes all internal data for parser for N_INPUT tokens. */
static void yaep_parse_init(YaepParseState *ps, int n_input)
{
    YaepRule*rule;

    create_dotted_rules(ps);
    set_init(ps, n_input);
    core_symb_ids_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    {
        int i;
        YaepSymbol*symb;

        for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
            symb->cached_core_symb_ids = NULL;
    }
#endif
    for(rule = ps->run.grammar->rulestorage_ptr->first_rule; rule != NULL; rule = rule->next)
        rule->caller_anode = NULL;
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
static void add_predicted_not_yet_started_dotted_rules(YaepParseState *ps,
                                                       YaepDottedRule *dotted_rule,
                                                       int dotted_rule_parent_id)
{
    YaepRule *rule = dotted_rule->rule;
    int context = dotted_rule->context;

    for(int j = dotted_rule->dot_j; rule->rhs[j] && rule->rhs[j]->empty_p; ++j)
    {
        YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, rule, j+1, context);
        set_add_nys_dotted_rule(ps, new_dotted_rule, dotted_rule_parent_id);
    }
}

/* The following function adds the rest(predicted not-yet-started) dotted_rules to the
   new set and and forms triples(set core, symbol, indexes) for
   further fast search of start dotted_rules from given core by
   transition on given symbol(see comment for abstract data `core_symb_ids'). */
static void expand_new_start_set(YaepParseState *ps)
{
    YaepDottedRule *dotted_rule;
    YaepSymbol *symb;
    YaepCoreSymbVect *core_symb_ids;
    YaepRule *rule;

    /* Add not yet started dotted_rules with nonzero matched_lengths. */
    for(int id = 0; id < ps->new_num_started_dotted_rules; id++)
    {
        add_predicted_not_yet_started_dotted_rules(ps, ps->new_dotted_rules[id], id);
    }

    /* Add not yet started dotted_rules and form predictions vectors. */
    for(int i = 0; i < ps->new_core->num_dotted_rules; i++)
    {
        dotted_rule = ps->new_dotted_rules[i];
        // Check that there is a symbol after the dot! */
        if (dotted_rule->dot_j < dotted_rule->rule->rhs_len)
	{
            // Yes.
            symb = dotted_rule->rule->rhs[dotted_rule->dot_j];
            core_symb_ids = core_symb_ids_find(ps, ps->new_core, symb);
            if (core_symb_ids == NULL)
	    {
                // No vector found for this core+symb combo.
                core_symb_ids = core_symb_ids_new(ps, ps->new_core, symb);
                if (!symb->terminal_p)
                {
                    for(rule = symb->u.nonterminal.rules; rule != NULL; rule = rule->lhs_next)
                    {
                        YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, rule, 0, 0);
                        set_add_initial_dotted_rule(ps, new_dotted_rule);
                        TRACE_FA(ps, "1 dotted rule %d", new_dotted_rule->id);
                    }
                }
	    }
            // Add a prediction to the core+symb lookup that points to this dotted rule.
            // I.e. when we reach a certain symbol within this core, the we just find
            // a vector using the core+symb lookup. This vector stores all predicted dotted_rules
            // that should be added for furtherparsing.
            core_symb_ids_add_predict(ps, core_symb_ids, i);

            if (symb->empty_p && i >= ps->new_core->num_all_matched_lengths)
            {
                YaepDottedRule *new_dotted_rule = create_dotted_rule(ps, dotted_rule->rule, dotted_rule->dot_j+1, 0);
                set_add_initial_dotted_rule(ps, new_dotted_rule);
                TRACE_FA(ps, "2 dotted rule %d", new_dotted_rule->id);
            }
	}
    }

    /* Now forming completion vectors. */
    for(int i = 0; i < ps->new_core->num_dotted_rules; i++)
    {
        dotted_rule = ps->new_dotted_rules[i];
        // Check that there is NO symbol after the dot! */
        if (dotted_rule->dot_j == dotted_rule->rule->rhs_len)
	{
            symb = dotted_rule->rule->lhs;
            core_symb_ids = core_symb_ids_find(ps, ps->new_core, symb);
            if (core_symb_ids == NULL)
            {
                core_symb_ids = core_symb_ids_new(ps, ps->new_core, symb);
            }
            core_symb_ids_add_complete(ps, core_symb_ids, i);
	}
    }

    if (ps->run.grammar->lookahead_level > 1)
    {
        YaepDottedRule *new_dotted_rule, *shifted_dotted_rule;
        terminal_bitset_t *context_set;
        int dotted_rule_id, context, j;
        bool changed_p;

        /* Now we have incorrect initial dotted_rules because their context is not correct. */
        context_set = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
        do
	{
            changed_p = false;
            for(int i = ps->new_core->num_all_matched_lengths; i < ps->new_core->num_dotted_rules; i++)
	    {
                terminal_bitset_clear(context_set, ps->run.grammar->symbs_ptr->num_terminals);
                new_dotted_rule = ps->new_dotted_rules[i];
                core_symb_ids = core_symb_ids_find(ps, ps->new_core, new_dotted_rule->rule->lhs);
                for(j = 0; j < core_symb_ids->predictions.len; j++)
		{
                    dotted_rule_id = core_symb_ids->predictions.ids[j];
                    dotted_rule = ps->new_dotted_rules[dotted_rule_id];
                    shifted_dotted_rule = create_dotted_rule(ps,
                                                             dotted_rule->rule,
                                                             dotted_rule->dot_j+1,
                                                             dotted_rule->context);
                    terminal_bitset_or(context_set, shifted_dotted_rule->lookahead, ps->run.grammar->symbs_ptr->num_terminals);
                    TRACE_FA(ps, "shifted dotted rule %d", shifted_dotted_rule->id);
		}
                context = terminal_bitset_insert(ps, context_set);
                if (context >= 0)
                {
                    context_set = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
                }
                else
                {
                    context = -context - 1;
                }
                dotted_rule = create_dotted_rule(ps,
                                                 new_dotted_rule->rule,
                                                 new_dotted_rule->dot_j,
                                                 context);
                TRACE_FA(ps, "added %d", new_dotted_rule->id);
                if (dotted_rule != new_dotted_rule)
		{
                    ps->new_dotted_rules[i] = dotted_rule;
                    changed_p = true;
		}
	    }
	}
        while(changed_p);
    }
    set_new_core_stop(ps);
    core_symb_ids_new_all_stop(ps);
}

/* The following function forms the 1st set. */
static void build_start_set(YaepParseState *ps)
{
    int context = 0;

    set_new_start(ps);

    if (ps->run.grammar->lookahead_level > 1)
    {
        terminal_bitset_t *empty_context_set = terminal_bitset_create(ps, ps->run.grammar->symbs_ptr->num_terminals);
        terminal_bitset_clear(empty_context_set, ps->run.grammar->symbs_ptr->num_terminals);
        context = terminal_bitset_insert(ps, empty_context_set);

        /* Empty context in the table has always number zero.*/
        assert(context == 0);
    }

    for (YaepRule *rule = ps->run.grammar->axiom->u.nonterminal.rules; rule != NULL; rule = rule->lhs_next)
    {
        YaepDottedRule *dotted_rule = create_dotted_rule(ps, rule, 0, context);
        set_add_dotted_rule(ps, dotted_rule, 0);
    }

    if (!set_insert(ps)) assert(false);

    expand_new_start_set(ps);
    ps->state_sets[0] = ps->new_set;
}

/* The following function predicts a new state set by shifting dotted_rules
   of SET given in CORE_SYMB_IDS with given lookahead terminal number.
   If the number is negative, we ignore lookahead at all. */
static void complete_and_predict_new_state_set(YaepParseState *ps,
                                               YaepStateSet *set,
                                               YaepCoreSymbVect *core_symb_ids,
                                               YaepSymbol *NEXT_TERMINAL)
{
    YaepStateSet *prev_set;
    YaepStateSetCore *set_core, *prev_set_core;
    YaepDottedRule *dotted_rule, *new_dotted_rule, **prev_dotted_rules;
    YaepCoreSymbVect *prev_core_symb_ids;
    int local_lookahead_level, matched_length, dotted_rule_id, new_matched_length;
    int place;
    YaepVect *predictions;

    int lookahead_term_id = NEXT_TERMINAL?NEXT_TERMINAL->u.terminal.term_id:-1;
    local_lookahead_level = (lookahead_term_id < 0 ? 0 : ps->run.grammar->lookahead_level);
    set_core = set->core;
    set_new_start(ps);
    predictions = &core_symb_ids->predictions;

    clear_dotted_rule_matched_length_set(ps);
    for (int i = 0; i < predictions->len; i++)
    {
        dotted_rule_id = predictions->ids[i];
        dotted_rule = set_core->dotted_rules[dotted_rule_id];

        new_dotted_rule = create_dotted_rule(ps, dotted_rule->rule,
                                             dotted_rule->dot_j+1, dotted_rule->context);

        if (local_lookahead_level != 0
            && !terminal_bitset_test(new_dotted_rule->lookahead, lookahead_term_id, ps->run.grammar->symbs_ptr->num_terminals)
            && !terminal_bitset_test(new_dotted_rule->lookahead, ps->run.grammar->term_error_id, ps->run.grammar->symbs_ptr->num_terminals))
        {
            // Lookahead predicted no-match. Stop here.
            continue;
        }
        matched_length = 0;
        if (dotted_rule_id >= set_core->num_all_matched_lengths)
        {
        }
        else if (dotted_rule_id < set_core->num_started_dotted_rules)
        {
            matched_length = set->matched_lengths[dotted_rule_id];
        }
        else
        {
            matched_length = set->matched_lengths[set_core->parent_dotted_rule_ids[dotted_rule_id]];
        }
        matched_length++;
        if (!dotted_rule_matched_length_test_and_set(ps, new_dotted_rule, matched_length))
        {
            // This combo dotted_rule + matched_length did not already exist, lets add it.
            set_add_dotted_rule(ps, new_dotted_rule, matched_length);
        }
    }

    for (int i = 0; i < ps->new_num_started_dotted_rules; i++)
    {
        new_dotted_rule = ps->new_dotted_rules[i];
        if (new_dotted_rule->empty_tail_p)
	{
            int *curr_el, *bound;

            /* All tail in new sitiation may derivate empty string so
               make reduce and add new dotted_rules.*/
            new_matched_length = ps->new_matched_lengths[i];
            place = ps->state_set_k + 1 - new_matched_length;
            prev_set = ps->state_sets[place];
            prev_set_core = prev_set->core;
            prev_core_symb_ids = core_symb_ids_find(ps, prev_set_core, new_dotted_rule->rule->lhs);
            if (prev_core_symb_ids == NULL)
	    {
                assert(new_dotted_rule->rule->lhs == ps->run.grammar->axiom);
                continue;
	    }
            curr_el = prev_core_symb_ids->predictions.ids;
            bound = curr_el + prev_core_symb_ids->predictions.len;

            assert(curr_el != NULL);
            prev_dotted_rules= prev_set_core->dotted_rules;
            while (curr_el < bound)
	    {
                dotted_rule_id = *curr_el++;
                dotted_rule = prev_dotted_rules[dotted_rule_id];
                new_dotted_rule = create_dotted_rule(ps, dotted_rule->rule, dotted_rule->dot_j+1, dotted_rule->context);

                if (local_lookahead_level != 0
                    && !terminal_bitset_test(new_dotted_rule->lookahead, lookahead_term_id, ps->run.grammar->symbs_ptr->num_terminals)
                    && !terminal_bitset_test(new_dotted_rule->lookahead,
                                      ps->run.grammar->term_error_id,
                                      ps->run.grammar->symbs_ptr->num_terminals))
                {
                    continue;
                }
                matched_length = 0;
                if (dotted_rule_id >= prev_set_core->num_all_matched_lengths)
                {
                }
                else if (dotted_rule_id < prev_set_core->num_started_dotted_rules)
                {
                    matched_length = prev_set->matched_lengths[dotted_rule_id];
                }
                else
                {
                    matched_length = prev_set->matched_lengths[prev_set_core->parent_dotted_rule_ids[dotted_rule_id]];
                }
                matched_length += new_matched_length;

                if (!dotted_rule_matched_length_test_and_set(ps, new_dotted_rule, matched_length))
                {
                    // This combo dotted_ruled + matched_length did not already exist, lets add it.
                    set_add_dotted_rule(ps, new_dotted_rule, matched_length);
                }
	    }
	}
    }

    if (set_insert(ps))
    {
        expand_new_start_set(ps);
        ps->new_core->term = core_symb_ids->symb;
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
            print_state_set(ps,
                            stderr,
                            ps->state_sets[curr_pl],
                            curr_pl,
                            ps->run.debug,
                            ps->run.debug);
            fprintf(stderr, "\n");
	}

    }
    ps->original_last_state_set_el = ps->state_set_k - 1;
}

/* If it is necessary, the following function restores original pl
   part with states in range [0, last_state_set_el].*/
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
            print_state_set(ps, stderr, ps->state_sets[ps->original_last_state_set_el], ps->original_last_state_set_el,
                            ps->run.debug, ps->run.debug);
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
        if (core_symb_ids_find(ps, ps->state_sets[curr_pl]->core, ps->run.grammar->term_error) != NULL)
            break;
        else if (ps->state_sets[curr_pl]->core->term != ps->run.grammar->term_error)
           (*cost)++;
    assert(curr_pl >= 0);
    return curr_pl;
}

/* The following function creates and returns new error recovery state
   with charcteristics(LAST_ORIGINAL_STATE_SET_EL, BACKWARD_MOVE_COST,
   state_set_k, tok_i).*/
static YaepRecoveryState new_recovery_state(YaepParseState *ps, int last_original_state_set_el, int backward_move_cost)
{
    YaepRecoveryState state;
    int i;

    assert(backward_move_cost >= 0);

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Creating recovery state: original set=%d, tok=%d, ",
                last_original_state_set_el, ps->tok_i);
        symbol_print(stderr, ps->input[ps->tok_i].symb, true);
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
            print_state_set(ps, stderr, ps->state_sets[i], i, ps->run.debug,
                            ps->run.debug);
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
        symbol_print(stderr, ps->input[ps->tok_i].symb, true);
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
        symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, "\n");
    }

    for(i = 0; i < state->state_set_tail_length; i++)
    {
        ps->state_sets[++ps->state_set_k] = state->state_set_tail[i];

        if (ps->run.debug)
	{
            fprintf(stderr, "++++++Add saved set=%d\n", ps->state_set_k);
            print_state_set(ps, stderr, ps->state_sets[ps->state_set_k], ps->state_set_k, ps->run.debug,
                      ps->run.debug);
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

static YaepStateSetTermLookAhead *lookup_cached_set(YaepParseState *ps,
                                                    YaepSymbol *THE_TERMINAL,
                                                    YaepSymbol *NEXT_TERMINAL,
                                                    YaepStateSet *set)
{
    int i;
    hash_table_entry_t *entry;
    YaepStateSetTermLookAhead *new_core_term_lookahead;

    OS_TOP_EXPAND(ps->triplet_core_term_lookahead_os, sizeof(YaepStateSetTermLookAhead));

    new_core_term_lookahead = (YaepStateSetTermLookAhead*) OS_TOP_BEGIN(ps->triplet_core_term_lookahead_os);
    new_core_term_lookahead->set = set;
    new_core_term_lookahead->term = THE_TERMINAL;
    new_core_term_lookahead->lookahead = NEXT_TERMINAL?NEXT_TERMINAL->u.terminal.term_id:-1;

    for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
    {
        new_core_term_lookahead->result[i] = NULL;
    }
    new_core_term_lookahead->curr = 0;
    entry = find_hash_table_entry(ps->set_of_triplets_core_term_lookahead, new_core_term_lookahead, true);

    if (*entry != NULL)
    {
        YaepStateSet *s;

        OS_TOP_NULLIFY(ps->triplet_core_term_lookahead_os);
        for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
        {
            if ((s = ((YaepStateSetTermLookAhead*)*entry)->result[i]) == NULL)
            {
                break;
            }
            else if (check_cached_transition_set(ps,
                                                 s,
                                                 ((YaepStateSetTermLookAhead*)*entry)->place[i]))
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
        *entry =(hash_table_entry_t) new_core_term_lookahead;
        ps->num_triplets_core_term_lookahead++;
    }

    return (YaepStateSetTermLookAhead*)*entry;
}

/* Save(set, term, lookahead) -> new_set in the table. */
static void save_cached_set(YaepParseState *ps, YaepStateSetTermLookAhead *entry, YaepSymbol *NEXT_TERMINAL)
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

    if (ps->run.debug)
    {
        fprintf(stderr, "(ixml) ----- begin parse -----\n");
        print_state_set(ps, stderr, ps->new_set, 0, ps->run.debug, ps->run.debug);
    }

    ps->tok_i = 0;
    ps->state_set_k = 0;

    for(; ps->tok_i < ps->input_len; ps->tok_i++)
    {
        assert(ps->state_set_k == ps->tok_i);
        YaepSymbol *THE_TERMINAL = ps->input[ps->tok_i].symb;
        YaepSymbol *NEXT_TERMINAL = NULL;

        if (ps->run.grammar->lookahead_level != 0 && ps->tok_i < ps->input_len-1)
        {
            NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
        }

        if (ps->run.debug)
	{
            fprintf(stderr, "\n(ixml) scan input[%d]= ", ps->tok_i);
            symbol_print(stderr, THE_TERMINAL, true);
            fprintf(stderr, " state_set_k=%d\n\n", ps->state_set_k);
	}

        YaepStateSet *set = ps->state_sets[ps->state_set_k];
        ps->new_set = NULL;

#ifdef USE_SET_HASH_TABLE
        YaepStateSetTermLookAhead *entry = lookup_cached_set(ps, THE_TERMINAL, NEXT_TERMINAL, set);
#endif

        if (ps->new_set == NULL)
	{
            YaepCoreSymbVect *core_symb_ids = core_symb_ids_find(ps, set->core, THE_TERMINAL);

            if (core_symb_ids == NULL)
	    {
                int c = try_to_recover(ps);
                if (c == 1) continue;
                else if (c == 2) break;
	    }

            complete_and_predict_new_state_set(ps, set, core_symb_ids, NEXT_TERMINAL);

#ifdef USE_SET_HASH_TABLE
            save_cached_set(ps, entry, NEXT_TERMINAL);
#endif
	}

        ps->state_set_k++;
        ps->state_sets[ps->state_set_k] = ps->new_set;

        if (ps->run.debug)
	{
            print_state_set(ps, stderr, ps->new_set, ps->state_set_k, ps->run.debug, ps->run.debug);
	}
    }
    free_error_recovery(ps);

    if (ps->run.debug)
    {
        fprintf(stderr, "(ixml) ----- end parse -----\n");
    }
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
        ps->map_rule_orig_statesetind_to_internalstate =
            create_hash_table(ps->run.grammar->alloc, ps->input_len* 2, parse_state_hash,
                               parse_state_eq);
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
    hash_table_entry_t*entry;

    entry = find_hash_table_entry(ps->map_rule_orig_statesetind_to_internalstate, state, true);

   *new_p = false;
    if (*entry != NULL)
        return(YaepParseTreeBuildState*)*entry;
   *new_p = true;
    /* We make copy because state_set_k can be changed in further processing state.*/
   *entry = parse_state_alloc(ps);
   *(YaepParseTreeBuildState*)*entry =*state;
    return(YaepParseTreeBuildState*)*entry;
}

static void free_parse_state(YaepParseState *ps)
{
    if (!ps->run.grammar->one_parse_p)
    {
        delete_hash_table(ps->map_rule_orig_statesetind_to_internalstate);
    }
    OS_DELETE(ps->parse_state_os);
}

/* This page conatins code to traverse translation.*/

/* Hash of translation visit node.*/
static unsigned trans_visit_node_hash(hash_table_entry_t n)
{
    return(size_t)((YaepTreeNodeVisit*) n)->node;
}

/* Equality of translation visit nodes.*/
static bool trans_visit_node_eq(hash_table_entry_t n1, hash_table_entry_t n2)
{
    return(((YaepTreeNodeVisit*) n1)->node == ((YaepTreeNodeVisit*) n2)->node);
}

/* The following function checks presence translation visit node with
   given NODE in the table and if it is not present in the table, the
   function creates the translation visit node and inserts it into
   the table.*/
static YaepTreeNodeVisit *visit_node(YaepParseState *ps, YaepTreeNode*node)
{
    YaepTreeNodeVisit trans_visit_node;
    hash_table_entry_t*entry;

    trans_visit_node.node = node;
    entry = find_hash_table_entry(ps->map_node_to_visit,
                                   &trans_visit_node, true);

    if (*entry == NULL)
    {
        /* If it is the new node, we did not visit it yet.*/
        trans_visit_node.num = -1 - ps->num_nodes_visits;
        ps->num_nodes_visits++;
        OS_TOP_ADD_MEMORY(ps->node_visits_os,
                           &trans_visit_node, sizeof(trans_visit_node));
       *entry =(hash_table_entry_t) OS_TOP_BEGIN(ps->node_visits_os);
        OS_TOP_FINISH(ps->node_visits_os);
    }
    return(YaepTreeNodeVisit*)*entry;
}

/* The following function returns the positive order number of node with number NUM.*/
static int canon_node_id(int id)
{
    return (id < 0 ? -id-1 : id);
}

/* The following recursive function prints NODE into file F and prints
   all its children(if debug_level < 0 output format is for graphviz).*/
static void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node)
{
    YaepTreeNodeVisit*trans_visit_node;
    YaepTreeNode*child;
    int i;

    assert(node != NULL);
    trans_visit_node = visit_node(ps, node);
    if (trans_visit_node->num >= 0)
    {
        return;
    }
    trans_visit_node->num = -trans_visit_node->num - 1;
    if (ps->run.debug) fprintf(f, "%7d: ", trans_visit_node->num);
    switch(node->type)
    {
    case YAEP_NIL:
        if (ps->run.debug)
            fprintf(f, "EMPTY\n");
        break;
    case YAEP_ERROR:
        if (ps->run.debug > 0)
            fprintf(f, "ERROR\n");
        break;
    case YAEP_TERM:
        if (ps->run.debug)
            fprintf(f, "TERMINAL: code=%d, repr=%s, mark=%d %c\n", node->val.terminal.code,
                    symb_find_by_code(ps, node->val.terminal.code)->repr, node->val.terminal.mark, node->val.terminal.mark>32?node->val.terminal.mark:' ');
        break;
    case YAEP_ANODE:
        if (ps->run.debug)
	{
            fprintf(f, "ABSTRACT: %c%s(", node->val.anode.mark?node->val.anode.mark:' ', node->val.anode.name);
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
            {
                fprintf(f, " %d", canon_node_id(visit_node(ps, child)->num));
            }
            fprintf(f, ")\n");
	}
        else
	{
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
	    {
                fprintf(f, "  \"%d: %s\" -> \"%d: ", trans_visit_node->num,
                         node->val.anode.name,
                        canon_node_id(visit_node(ps, child)->num));
                switch(child->type)
		{
		case YAEP_NIL:
                    fprintf(f, "EMPTY");
                    break;
		case YAEP_ERROR:
                    fprintf(f, "ERROR");
                    break;
		case YAEP_TERM:
                    fprintf(f, "%s",
                            symb_find_by_code(ps, child->val.terminal.code)->repr);
                    break;
		case YAEP_ANODE:
                    fprintf(f, "%s", child->val.anode.name);
                    break;
		case YAEP_ALT:
                    fprintf(f, "ALT");
                    break;
		default:
                    assert(false);
		}
                fprintf(f, "\";\n");
	    }
	}
        for (i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            print_yaep_node(ps, f, child);
        }
        break;
    case YAEP_ALT:
        if (ps->run.debug)
	{
            fprintf(f, "ALTERNATIVE: node=%d, next=",
                    canon_node_id(visit_node(ps, node->val.alt.node)->num));
            if (node->val.alt.next != NULL)
                fprintf(f, "%d\n",
                        canon_node_id(visit_node(ps, node->val.alt.next)->num));
            else
                fprintf(f, "nil\n");
	}
        else
	{
            fprintf(f, "  \"%d: ALT\" -> \"%d: ", trans_visit_node->num,
                    canon_node_id(visit_node(ps, node->val.alt.node)->num));
            switch(node->val.alt.node->type)
	    {
	    case YAEP_NIL:
                fprintf(f, "EMPTY");
                break;
	    case YAEP_ERROR:
                fprintf(f, "ERROR");
                break;
	    case YAEP_TERM:
                fprintf(f, "%s",
                        symb_find_by_code(ps, node->val.alt.node->val.terminal.code)->
                         repr);
                break;
	    case YAEP_ANODE:
                fprintf(f, "%s", node->val.alt.node->val.anode.name);
                break;
	    case YAEP_ALT:
                fprintf(f, "ALT");
                break;
	    default:
                assert(false);
	    }
            fprintf(f, "\";\n");
            if (node->val.alt.next != NULL)
            {
                fprintf(f, "  \"%d: ALT\" -> \"%d: ALT\";\n",
                        trans_visit_node->num,
                        canon_node_id(visit_node(ps, node->val.alt.next)->num));
            }
	}
        print_yaep_node(ps, f, node->val.alt.node);
        if (node->val.alt.next != NULL)
        {
            print_yaep_node(ps, f, node->val.alt.next);
        }
        break;
    default:
        assert(false);
    }
}

/* The following function prints parse tree with ROOT.*/
static void print_parse(YaepParseState *ps, FILE* f, YaepTreeNode*root)
{
    ps->map_node_to_visit = create_hash_table(ps->run.grammar->alloc,
                                              ps->input_len* 2,
                                              trans_visit_node_hash,
                                              trans_visit_node_eq);

    ps->num_nodes_visits = 0;
    OS_CREATE(ps->node_visits_os, ps->run.grammar->alloc, 0);
    print_yaep_node(ps, f, root);
    OS_DELETE(ps->node_visits_os);
    delete_hash_table(ps->map_node_to_visit);
}

/* The following function places translation NODE into *PLACE and
   creates alternative nodes if it is necessary. */
static void place_translation(YaepParseState *ps, YaepTreeNode **place, YaepTreeNode *node)
{
    YaepTreeNode *alt, *next_alt;

    assert(place != NULL);
    if (*place == NULL)
    {
        TRACE_FA(ps, "immediate %p %p", place, node);
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

   TRACE_FA(ps, "ind %p %p", place, node);
}

static YaepTreeNode *copy_anode(YaepParseState *ps,
                                YaepTreeNode **place,
                                YaepTreeNode *anode,
                                YaepRule *rule,
                                int disp)
{
    YaepTreeNode*node;
    int i;

    TRACE_F(ps);

    node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                              + sizeof(YaepTreeNode*)
                                              *(rule->trans_len + 1)));
   *node =*anode;
    node->val.anode.children = ((YaepTreeNode**)((char*) node + sizeof(YaepTreeNode)));
    for(i = 0; i <= rule->trans_len; i++)
    {
        node->val.anode.children[i] = anode->val.anode.children[i];
    }
    node->val.anode.children[disp] = NULL;
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

    TRACE_F(ps);

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
            node->val.anode.cost = -node->val.anode.cost - 1;	/* flag of visit*/
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

    TRACE_F(ps);

    return;
}

/* The function finds and returns a minimal cost parse(s). */
static YaepTreeNode *find_minimal_translation(YaepParseState *ps, YaepTreeNode *root)
{
    YaepTreeNode**node_ptr;
    int cost;

    if (ps->run.parse_free != NULL)
    {
        ps->set_of_reserved_memory = create_hash_table(ps->run.grammar->alloc, ps->input_len* 4, reserv_mem_hash,
                                           reserv_mem_eq);

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

    TRACE_F(ps);

    return root;
}

/* The following function finds parse tree of parsed input.  The
   function sets up*AMBIGUOUS_P if we found that the grammer is
   ambigous(it works even we asked only one parse tree without
   alternatives). */
static YaepTreeNode *build_parse_tree(YaepParseState *ps, bool *ambiguous_p)
{
    YaepStateSet *set, *check_set;
    YaepStateSetCore *set_core, *check_set_core;
    YaepDottedRule *dotted_rule, *check_dotted_rule;
    YaepRule *rule, *dotted_rule_rule;
    YaepSymbol *symb;
    YaepCoreSymbVect *core_symb_ids, *check_core_symb_ids;
    int i, j, k, found, pos, from_i;
    int state_set_k, n_candidates, disp;
    int dotted_rule_id, check_dotted_rule_id;
    int dotted_rule_from_i, check_dotted_rule_from_i;
    bool new_p;
    YaepParseTreeBuildState *state, *orig_state, *curr_state;
    YaepParseTreeBuildState *table_state, *parent_anode_state;
    YaepParseTreeBuildState root_state;
    YaepTreeNode *result, *empty_node, *node, *error_node;
    YaepTreeNode *parent_anode, *anode, root_anode;
    int parent_disp;
    bool saved_one_parse_p;
    YaepTreeNode **term_node_array = NULL;
    vlo_t stack, orig_states;

    ps->n_parse_term_nodes = ps->n_parse_abstract_nodes = ps->n_parse_alt_nodes = 0;
    set = ps->state_sets[ps->state_set_k];
    assert(ps->run.grammar->axiom != NULL);
    /* We have only one start dotted_rule: "$S : <start symb> $eof .". */
    dotted_rule =(set->core->dotted_rules != NULL ? set->core->dotted_rules[0] : NULL);
    if (dotted_rule == NULL
        || set->matched_lengths[0] != ps->state_set_k
        || dotted_rule->rule->lhs != ps->run.grammar->axiom || dotted_rule->dot_j != dotted_rule->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'.*/
        assert(!ps->run.grammar->error_recovery_p);
        return NULL;
    }
    saved_one_parse_p = ps->run.grammar->one_parse_p;
    if (ps->run.grammar->cost_p)
        /* We need all parses to choose the minimal one*/
        ps->run.grammar->one_parse_p = false;
    dotted_rule = set->core->dotted_rules[0];
    parse_state_init(ps);
    if (!ps->run.grammar->one_parse_p)
    {
        void*mem;

        /* We need this array to reuse terminal nodes only for
           generation of several parses.*/
        mem = yaep_malloc(ps->run.grammar->alloc,
                          sizeof(YaepTreeNode*)* ps->input_len);
        term_node_array =(YaepTreeNode**) mem;
        for(i = 0; i < ps->input_len; i++)
        {
            term_node_array[i] = NULL;
        }
        /* The following is used to check necessity to create current
           state with different state_set_k.*/
        VLO_CREATE(orig_states, ps->run.grammar->alloc, 0);
    }
    VLO_CREATE(stack, ps->run.grammar->alloc, 10000);
    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
    state = parse_state_alloc(ps);
   ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
    rule = state->rule = dotted_rule->rule;
    state->dot_j = dotted_rule->dot_j;
    state->from_i = 0;
    state->state_set_k = ps->state_set_k;
    result = NULL;
    root_state.anode = &root_anode;
    root_anode.val.anode.children = &result;
    state->parent_anode_state = &root_state;
    state->parent_disp = 0;
    state->anode = NULL;
    /* Create empty and error node:*/
    empty_node =((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    empty_node->type = YAEP_NIL;
    empty_node->val.nil.used = 0;
    error_node =((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    error_node->type = YAEP_ERROR;
    error_node->val.error.used = 0;

    if (ps->run.debug) fprintf(stderr, "(ixml) ----- building parse tree ------\n");

    while(VLO_LENGTH(stack) != 0)
    {
        if (ps->run.debug && state->dot_j == state->rule->rhs_len)
	{
            fprintf(stderr, "processing top=%ld state_set_k=%d dotted_rule=",
                    (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1,
                     state->state_set_k);
            print_rule_with_dot(ps, stderr, state->rule, state->dot_j);
            fprintf(stderr, " state->from_i=%d\n", state->from_i);
	}

        pos = --state->dot_j;
        rule = state->rule;
        parent_anode_state = state->parent_anode_state;
        parent_anode = parent_anode_state->anode;
        parent_disp = state->parent_disp;
        anode = state->anode;
        disp = rule->order[pos];
        state_set_k = state->state_set_k;
        from_i = state->from_i;
        if (pos < 0)
	{
            /* We've processed all rhs of the rule.*/

            if (ps->run.debug && state->dot_j == state->rule->rhs_len)
	    {
                fprintf(stderr, "    * popping top=%ld state_set_k=%d dotted_rule=",
                        (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1,
                        state->state_set_k);

                print_rule_with_dot(ps, stderr, state->rule, 0);

                fprintf(stderr, " state->from_i=%d\n", state->from_i);
	    }

            parse_state_free(ps, state);
            VLO_SHORTEN(stack, sizeof(YaepParseTreeBuildState*));
            if (VLO_LENGTH(stack) != 0)
                state =((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
	    {
                /* We do dotted_ruleuce nothing but we should. So write empty node.*/
                place_translation(ps, parent_anode->val.anode.children + parent_disp, empty_node);
                empty_node->val.nil.used = 1;
	    }
            else if (anode != NULL)
	    {
                /* Change NULLs into empty nodes.  We can not make it
                   the first time because when building several parses
                   the NULL means flag of absence of translations(see
                   function `place_translation').*/
                for(i = 0; i < rule->trans_len; i++)
                    if (anode->val.anode.children[i] == NULL)
                    {
                        anode->val.anode.children[i] = empty_node;
                        empty_node->val.nil.used = 1;
                    }
	    }
            continue;
	}
        assert(pos >= 0);
        symb = rule->rhs[pos];
        if (symb->terminal_p)
	{
            /* Terminal before dot:*/
            state_set_k--;		/* l*/
            /* Because of error recovery input [state_set_k].symb may be not equal to symb.*/
            //assert(ps->input[state_set_k].symb == symb);
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   terminal.  Add reference to the current node.*/
                if (symb == ps->run.grammar->term_error)
		{
                    node = error_node;
                    error_node->val.error.used = 1;
		}
                else if (!ps->run.grammar->one_parse_p
                         &&(node = term_node_array[state_set_k]) != NULL)
                    ;
                else
		{
                    ps->n_parse_term_nodes++;
                    node =((YaepTreeNode*)
                           (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
                    node->type = YAEP_TERM;
                    node->val.terminal.code = symb->u.terminal.code;
                    // IXML
                    if (rule->marks && rule->marks[pos])
                    {
                        // Copy the mark from the rhs position on to the terminal.
                        node->val.terminal.mark = rule->marks[pos];
                    }
                    node->val.terminal.attr = ps->input[state_set_k].attr;
                    if (!ps->run.grammar->one_parse_p)
                        term_node_array[state_set_k] = node;
		}
                place_translation(ps,
                                  anode != NULL ?
                                  anode->val.anode.children + disp
                                  : parent_anode->val.anode.children + parent_disp, node);
	    }
            if (pos != 0)
                state->state_set_k = state_set_k;
            continue;
	}
        /* Nonterminal before dot:*/
        set = ps->state_sets[state_set_k];
        set_core = set->core;
        core_symb_ids = core_symb_ids_find(ps, set_core, symb);
        assert(core_symb_ids->completions.len != 0);
        n_candidates = 0;
        orig_state = state;
        if (!ps->run.grammar->one_parse_p)
        {
            VLO_NULLIFY(orig_states);
        }
        for(i = 0; i < core_symb_ids->completions.len; i++)
	{
            dotted_rule_id = core_symb_ids->completions.ids[i];
            dotted_rule = set_core->dotted_rules[dotted_rule_id];
            if (dotted_rule_id < set_core->num_started_dotted_rules)
            {
                // The state set i is the tok_i for which the state set was created.
                // Ie, it is the to_i inside the Earley item.
                // Now subtract the matched length from this to_i to get the from_i
                // which is the origin.
                dotted_rule_from_i = state_set_k - set->matched_lengths[dotted_rule_id];
            }
            else if (dotted_rule_id < set_core->num_all_matched_lengths)
            {
                dotted_rule_from_i = state_set_k - set->matched_lengths[set_core->parent_dotted_rule_ids[dotted_rule_id]];
            }
            else
            {
                dotted_rule_from_i = state_set_k;
            }

            if (ps->run.debug)
	    {
                fprintf(stderr, "    * trying state_set_k=%d dotted_rule=", state_set_k);
                print_dotted_rule(ps, stderr, "", "", dotted_rule, ps->run.debug, -1);
                fprintf(stderr, " dotted_rule_from_i=%d\n", dotted_rule_from_i);
	    }

            check_set = ps->state_sets[dotted_rule_from_i];
            check_set_core = check_set->core;
            check_core_symb_ids = core_symb_ids_find(ps, check_set_core, symb);
            assert(check_core_symb_ids != NULL);
            found = false;
            for(j = 0; j < check_core_symb_ids->predictions.len; j++)
	    {
                check_dotted_rule_id = check_core_symb_ids->predictions.ids[j];
                check_dotted_rule = check_set->core->dotted_rules[check_dotted_rule_id];
                if (check_dotted_rule->rule != rule || check_dotted_rule->dot_j != pos)
                {
                    continue;
                }
                check_dotted_rule_from_i = dotted_rule_from_i;
                if (check_dotted_rule_id < check_set_core->num_all_matched_lengths)
		{
                    if (check_dotted_rule_id < check_set_core->num_started_dotted_rules)
                    {
                        check_dotted_rule_from_i = dotted_rule_from_i - check_set->matched_lengths[check_dotted_rule_id];
                    }
                    else
                    {
                        check_dotted_rule_from_i = (dotted_rule_from_i - check_set->matched_lengths[check_set_core->parent_dotted_rule_ids[check_dotted_rule_id]]);
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
               *ambiguous_p = true;
                if (ps->run.grammar->one_parse_p)
                {
                    break;
                }
	    }
            dotted_rule_rule = dotted_rule->rule;
            if (n_candidates == 0)
            {
                orig_state->state_set_k = dotted_rule_from_i;
            }
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the nonterminal. */
                curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule. */
                if (n_candidates != 0)
		{
                    assert(!ps->run.grammar->one_parse_p);
                    if (n_candidates == 1)
		    {
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                       ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1]
                            = orig_state;
		    }
                    for(j =(VLO_LENGTH(orig_states)
                              / sizeof(YaepParseTreeBuildState*) - 1); j >= 0; j--)
                        if (((YaepParseTreeBuildState**)
                             VLO_BEGIN(orig_states))[j]->state_set_k == dotted_rule_from_i)
                            break;
                    if (j >= 0)
		    {
                        /* [A -> x., n] & [A -> y., n]*/
                        curr_state =((YaepParseTreeBuildState**)
                                      VLO_BEGIN(orig_states))[j];
                        anode = curr_state->anode;
		    }
                    else
		    {
                        /* [A -> x., n] & [A -> y., m] where n != m.*/
                        /* It is different from the previous ones so add
                           it to process.*/
                        state = parse_state_alloc(ps);
                        VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                       ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                       *state =*orig_state;
                        state->state_set_k = dotted_rule_from_i;
                        if (anode != NULL)
                            state->anode
                                = copy_anode(ps, parent_anode->val.anode.children
                                              + parent_disp, anode, rule, disp);
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                       ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1]
                            = state;

                        if (ps->run.debug)
			{
                            fprintf(stderr, "    * adding top=%ld dotted_rule_from_i=%d modified dotted_rule=",
                                    (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1,
                                     dotted_rule_from_i);
                            print_rule_with_dot(ps, stderr, state->rule, state->dot_j);
                            fprintf(stderr, " state->from_i=%d\n", state->from_i);
			}

                        curr_state = state;
                        anode = state->anode;
		    }
		}		/* if (n_candidates != 0)*/
                if (dotted_rule_rule->anode != NULL)
		{
                    /* This rule creates abstract node. */
                    state = parse_state_alloc(ps);
                    state->rule = dotted_rule_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    table_state = NULL;
                    if (!ps->run.grammar->one_parse_p)
                    {
                        table_state = parse_state_insert(ps, state, &new_p);
                    }
                    if (table_state == NULL || new_p)
		    {
                        /* We need new abtract node.*/
                        ps->n_parse_abstract_nodes++;
                        node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                                                      + sizeof(YaepTreeNode*)
                                                                      *(dotted_rule_rule->trans_len + 1)));
                        state->anode = node;
                        if (table_state != NULL)
                            table_state->anode = node;
                        node->type = YAEP_ANODE;
                        if (dotted_rule_rule->caller_anode == NULL)
			{
                            dotted_rule_rule->caller_anode = ((char*)(*ps->run.parse_alloc)(strlen(dotted_rule_rule->anode) + 1));
                            strcpy(dotted_rule_rule->caller_anode, dotted_rule_rule->anode);
			}
                        node->val.anode.name = dotted_rule_rule->caller_anode;
                        node->val.anode.cost = dotted_rule_rule->anode_cost;
                        // IXML Copy the rule name -to the generated abstract node.
                        node->val.anode.mark = dotted_rule_rule->mark;
                        if (rule->marks && rule->marks[pos])
                        {
                            // But override the mark with the rhs mark!
                            node->val.anode.mark = rule->marks[pos];
                        }
                        /////////
                        node->val.anode.children = ((YaepTreeNode**)((char*) node + sizeof(YaepTreeNode)));
                        for(k = 0; k <= dotted_rule_rule->trans_len; k++)
                        {
                            node->val.anode.children[k] = NULL;
                        }
                        VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                        if (anode == NULL)
			{
                            state->parent_anode_state = curr_state->parent_anode_state;
                            state->parent_disp = parent_disp;
			}
                        else
			{
                            state->parent_anode_state = curr_state;
                            state->parent_disp = disp;
			}

                        if (ps->run.debug)
			{
                            fprintf(stderr, "    * adding top %ld, state_set_k = %d, dotted_rule = ",
                                    (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1,
                                    state_set_k);
                            print_dotted_rule(ps, stderr, "", "", dotted_rule, ps->run.debug, -1);
                            fprintf(stderr, ", %d\n", dotted_rule_from_i);
			}

		    }
                    else
		    {
                        /* We allready have the translation.*/
                        assert(!ps->run.grammar->one_parse_p);
                        parse_state_free(ps, state);
                        state =((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
                        node = table_state->anode;
                        assert(node != NULL);

                        if (ps->run.debug)
			{
                            fprintf(stderr, "    * found prev. translation: state_set_k = %d, dotted_rule = ",
                                     state_set_k);
                            print_dotted_rule(ps, stderr, "", "", dotted_rule, ps->run.debug, -1);
                            fprintf(stderr, ", %d\n", dotted_rule_from_i);
			}

		    }
                    place_translation(ps, anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       node);
		}		/* if (dotted_rule_rule->anode != NULL)*/
                else if (dotted_rule->dot_j != 0)
		{
                    /* We should generate and use the translation of the
                       nonterminal.  Add state to get a translation.*/
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                   ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = dotted_rule_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    state->parent_anode_state =(anode == NULL
                                                 ? curr_state->
                                                 parent_anode_state :
                                                 curr_state);
                    state->parent_disp = anode == NULL ? parent_disp : disp;
                    state->anode = NULL;

                    if (ps->run.debug)
		    {
                        fprintf(stderr, "    * adding top %ld, state_set_k = %d, dotted_rule = ",
                                (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1,
                                state_set_k);
                        print_dotted_rule(ps, stderr, "", "", dotted_rule, ps->run.debug, -1);
                        fprintf(stderr, ", %d\n", dotted_rule_from_i);
		    }

		}
                else
		{
                    /* Empty rule should dotted_ruleuce something not abtract
                       node.  So place empty node.*/
                    place_translation(ps, anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       empty_node);
                    empty_node->val.nil.used = 1;
		}
	    }			/* if (parent_anode != NULL && disp >= 0)*/
            n_candidates++;
	}			/* For all completions of the nonterminal.*/
        /* We should have a parse.*/
        assert(n_candidates != 0 && (!ps->run.grammar->one_parse_p || n_candidates == 1));
    } /* For all parser states.*/
    VLO_DELETE(stack);

    if (ps->run.debug) fprintf(stderr, "(ixml) ----- done parse tree ------\n");

    if (!ps->run.grammar->one_parse_p)
    {
        VLO_DELETE(orig_states);
        yaep_free(ps->run.grammar->alloc, term_node_array);
    }
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
    if (ps->run.trace)
    {
        fprintf(stderr, "(ixml) yaep parse tree:\n");
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

    if ((code = setjmp(error_longjump_buff)) != 0)
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
        fprintf(stderr, "(ixml) %sparse statistics\n       #terminals = %d, #nonterms = %d\n",
                *ambiguous_p ? "AMBIGUOUS " : "",
                 ps->run.grammar->symbs_ptr->num_terminals, ps->run.grammar->symbs_ptr->num_nonterminals);
        fprintf(stderr, "       #rules = %d, rules size = %d\n",
                 ps->run.grammar->rulestorage_ptr->num_rules,
                 ps->run.grammar->rulestorage_ptr->n_rhs_lens + ps->run.grammar->rulestorage_ptr->num_rules);
        fprintf(stderr, "       #tokens = %d, #unique dotted_rules = %d\n",
                 ps->input_len, ps->num_all_dotted_rules);
        fprintf(stderr, "       #terminal sets = %d, their size = %d\n",
                 ps->run.grammar->term_sets_ptr->n_term_sets, ps->run.grammar->term_sets_ptr->n_term_sets_size);
        fprintf(stderr, "       #unique set cores = %d, #their start dotted_rules = %d\n",
                 ps->num_set_cores, ps->num_set_core_start_dotted_rules);
        fprintf(stderr, "       #parent indexes for some non start dotted_rules = %d\n",
                 ps->num_parent_dotted_rule_ids);
        fprintf(stderr, "       #unique set dist. vects = %d, their length = %d\n",
                 ps->num_set_matched_lengths, ps->num_set_matched_lengths_len);
        fprintf(stderr, "       #unique sets = %d, #their start dotted_rules = %d\n",
                 ps->n_sets, ps->n_sets_start_dotted_rules);
        fprintf(stderr, "       #unique triples(set, term, lookahead) = %d, goto successes=%d\n",
                ps->num_triplets_core_term_lookahead, ps->n_goto_successes);
        fprintf(stderr, "       #pairs(set core, symb) = %d, their trans+reduce vects length = %d\n",
                 ps->n_core_symb_pairs, ps->n_core_symb_ids_len);
        fprintf(stderr, "       #unique transition vectors = %d, their length = %d\n",
                ps->n_transition_vects, ps->n_transition_vect_len);
        fprintf(stderr, "       #unique reduce vectors = %d, their length = %d\n",
                 ps->n_reduce_vects, ps->n_reduce_vect_len);
        fprintf(stderr, "       #term nodes = %d, #abstract nodes = %d\n",
                 ps->n_parse_term_nodes, ps->n_parse_abstract_nodes);
        fprintf(stderr, "       #alternative nodes = %d, #all nodes = %d\n",
                 ps->n_parse_alt_nodes,
                 ps->n_parse_term_nodes + ps->n_parse_abstract_nodes
                 + ps->n_parse_alt_nodes);
        if (table_searches == 0)
            table_searches++;
        fprintf(stderr,
                 "       #table collisions = %.2g%%(%d out of %d)\n",
                 table_collisions* 100.0 / table_searches,
                 table_collisions, table_searches);
    }

    free_state_sets(ps);
    free_inside_parse_state(ps);
    free_input(ps);
    return 0;
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

    TRACE_F(ps);
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
        return;			/* Tail recursion*/

    default:
        assert("This should not happen" == NULL);
    }

    parse_free(node);
}

void yaepFreeTree(YaepTreeNode *root,
                  void(*parse_free)(void*),
                  void(*termcb)(YaepTerminalNode*))
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


/* The following function prints symbol SYMB to file F.  Terminal is
   printed with its code if CODE_P.*/
static void symbol_print(FILE* f, YaepSymbol*symb, bool code_p)
{
    if (symb->terminal_p)
    {
        fprintf(f, "%s", symb->hr);
        return;
    }
    fprintf(f, "%s", symb->repr);
    /*
    if (code_p && symb->terminal_p)
    {
        fprintf(f, "(%d)", symb->u.terminal.code);
    }*/
}

/* The following function prints RULE with its translation(if TRANS_P) to file F.*/
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, bool trans_p)
{
    int i, j;

    if (rule->mark != 0
        && rule->mark != ' '
        && rule->mark != '-'
        && rule->mark != '@'
        && rule->mark != '^'
        && rule->mark != '*')
    {
        fprintf(f, "(yaep) internal error bad rule: ");
        symbol_print(f, rule->lhs, false);
        fprintf(f, "\n");
        assert(false);
    }

    char m = rule->mark;
    if (m >= 32 && m < 127)
    {
        fprintf(f, "%c", m);
    }
    symbol_print(f, rule->lhs, false);
    if (strcmp(rule->lhs->repr, rule->anode)) fprintf(stderr, "(%s)", rule->anode);
    fprintf(f, " → ");
    int cost = rule->anode_cost;
    while (cost > 0)
    {
        fprintf(f, "<");
        cost--;
    }
    for (i = 0; i < rule->rhs_len; i++)
    {
        char m = rule->marks[i];
        if (m >= 32 && m < 127)
        {
            fprintf(f, " %c", m);
        }
        else
        {
            if (!m) fprintf(f, "  ");
            else    fprintf(f, " ?%d?", rule->marks[i]);
        }
        symbol_print(f, rule->rhs[i], false);
    }
    if (false && trans_p)
    {
        fprintf(f, "      | ");
        if (rule->anode != NULL)
        {
            fprintf(f, "%s(", rule->anode);
        }
        for(i = 0; i < rule->trans_len; i++)
	{
            for(j = 0; j < rule->rhs_len; j++)
            {
                if (rule->order[j] == i)
                {
                    fprintf(f, " %d:", j);
                    symbol_print(f, rule->rhs[j], false);
                    break;
                }
            }
            if (j >= rule->rhs_len)
            {
                fprintf(f, " nil");
            }
	}
        if (rule->anode != NULL)
        {
            fprintf(f, " )");
        }
    }
    fprintf(f, "\n");
}

/* The following function prints RULE to file F with dot in position pos.
   Pos == 0 means that the dot is all the way to the left in the starting position.
   Pos == rhs_len means that the whole rule has been matched. */
static void print_rule_with_dot(YaepParseState *ps, FILE *f, YaepRule *rule, int pos)
{
    int i;

    assert(pos >= 0 && pos <= rule->rhs_len);

    symbol_print(f, rule->lhs, false);
    fprintf(f, " → ");
    for(i = 0; i < rule->rhs_len; i++)
    {
        fprintf(f, i == pos ? " 🞄 " : " ");
        symbol_print(f, rule->rhs[i], false);
    }
    if (rule->rhs_len == pos)
    {
        fprintf(f, " 🞄 ");
    }
}

/* The following function prints dotted_rule DOTTED_RULE to file F.  The
   dotted_rule is printed with the lookahead set if LOOKAHEAD_P.*/
static void print_dotted_rule(YaepParseState *ps, FILE *f,
                              const char *prefix, const char *suffix,
                              YaepDottedRule *dotted_rule, bool lookahead_p, int matched_length)
{
    if (prefix) fprintf(f, "%s", prefix);
    fprintf(f, "(%d)    ", dotted_rule->id);
    print_rule_with_dot(ps, f, dotted_rule->rule, dotted_rule->dot_j);

    if (matched_length > 0)
    {
        fprintf(f, " matched %d", matched_length);
    }
    if (ps->run.grammar->lookahead_level != 0 && lookahead_p && matched_length >= 0)
    {
        fprintf(f, "    ");
        terminal_bitset_print(ps, f, dotted_rule->lookahead, ps->run.grammar->symbs_ptr->num_terminals);
    }
    if (matched_length != -1) fprintf(f, "\n");
    if (suffix) fprintf(f, "%s", suffix);
}

/* The following function prints SET to file F.  If NOT_YET_STARTED_P is true
   then print all dotted_rules.  The dotted_rules are printed with the
   lookahead set if LOOKAHEAD_P.  SET_DIST is used to print absolute
   matched_lengths of not-yet-started dotted_rules. */
static void print_state_set(YaepParseState *ps,
                            FILE* f,
                            YaepStateSet *state_set,
                            int set_dist,
                            int print_all_dotted_rules,
                            int lookahead_p)
{
    int i;
    int num, num_started_dotted_rules, num_dotted_rules, num_all_matched_lengths;
    YaepDottedRule **dotted_rules;
    int*matched_lengths,*parent_dotted_rule_ids;

    if (state_set == NULL && !ps->new_set_ready_p)
    {
        /* The following is necessary if we call the function from a
           debugger.  In this case new_set, new_core and their members
           may be not set up yet. */
        num = -1;
        num_started_dotted_rules = num_dotted_rules = num_all_matched_lengths = ps->new_num_started_dotted_rules;
        dotted_rules = ps->new_dotted_rules;
        matched_lengths = ps->new_matched_lengths;
        parent_dotted_rule_ids = NULL;
    }
    else
    {
        num = state_set->core->id;
        num_dotted_rules = state_set->core->num_dotted_rules;
        dotted_rules = state_set->core->dotted_rules;
        num_started_dotted_rules = state_set->core->num_started_dotted_rules;
        matched_lengths = state_set->matched_lengths;
        num_all_matched_lengths = state_set->core->num_all_matched_lengths;
        parent_dotted_rule_ids = state_set->core->parent_dotted_rule_ids;
        num_started_dotted_rules = state_set->core->num_started_dotted_rules;
    }

    fprintf(f, "  core(%d)\n", num);

    for(i = 0; i < num_dotted_rules; i++)
    {
        fprintf(f, "    ");

        int dist = 0;
        if (i < num_started_dotted_rules) dist = matched_lengths[i];
        else if (i < num_all_matched_lengths) dist = parent_dotted_rule_ids[i];
        else dist = 0;

        assert(dist == (i < num_started_dotted_rules ? matched_lengths[i] : i < num_all_matched_lengths ? parent_dotted_rule_ids[i] : 0));

        print_dotted_rule(ps, f, "", "", dotted_rules[i], lookahead_p, dist);

        if (i == num_started_dotted_rules - 1 && num_dotted_rules > num_started_dotted_rules)
        {
            if (!print_all_dotted_rules)
            {
                // We have printed the start dotted_rules. Stop here.
                break;
            }
            fprintf(f, "    ----------- predictions\n");
        }
    }
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
    YaepCoreSymbVect*core_symb_ids;
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
    ps->state_set_k
        = ps->back_state_set_frontier = find_error_state_set_set(ps, ps->state_set_k, &backward_move_cost);
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
                    symbol_print(stderr, ps->input[ps->tok_i].symb, true);
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
            symbol_print(stderr, ps->input[ps->tok_i].symb, true);
            fprintf(stderr, "\n");
	}

        /* Shift error:*/
        core_symb_ids = core_symb_ids_find(ps, set->core, ps->run.grammar->term_error);
        assert(core_symb_ids != NULL);

        if (ps->run.debug)
            fprintf(stderr, "++++Making error shift in set=%d\n", ps->state_set_k);

        complete_and_predict_new_state_set(ps, set, core_symb_ids, NULL);
        ps->state_sets[++ps->state_set_k] = ps->new_set;

        if (ps->run.debug)
	{
            fprintf(stderr, "++Trying new set=%d\n", ps->state_set_k);
            print_state_set(ps, stderr, ps->new_set, ps->state_set_k, ps->run.debug, ps->run.debug);
            fprintf(stderr, "\n");
	}

        /* Search the first right token.*/
        while(ps->tok_i < ps->input_len)
	{
            core_symb_ids = core_symb_ids_find(ps, ps->new_core, ps->input[ps->tok_i].symb);
            if (core_symb_ids != NULL)
                break;

            if (ps->run.debug)
	    {
                fprintf(stderr, "++++++Skipping=%d ", ps->tok_i);
                symbol_print(stderr, ps->input[ps->tok_i].symb, true);
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
        complete_and_predict_new_state_set(ps, ps->new_set, core_symb_ids, NEXT_TERMINAL);
        ps->state_sets[++ps->state_set_k] = ps->new_set;

        if (ps->run.debug)
	{
            fprintf(stderr, "++++++++Building new set=%d\n", ps->state_set_k);
            if (ps->run.debug)
            {
                print_state_set(ps, stderr, ps->new_set, ps->state_set_k, ps->run.debug, ps->run.debug);
            }
	}

        num_matched_input = 0;
        for(;;)
	{

            if (ps->run.debug)
	    {
                fprintf(stderr, "++++++Matching=%d ", ps->tok_i);
                symbol_print(stderr, ps->input[ps->tok_i].symb, true);
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
            if (core_symb_ids_find(ps, ps->new_core, ps->run.grammar->term_error) != NULL)
	    {
                if (ps->run.debug)
		{
                    fprintf(stderr, "++++Found secondary state: original set=%d, tok=%d, ",
                            state.last_original_state_set_el, ps->tok_i);
                    symbol_print(stderr, ps->input[ps->tok_i].symb, true);
                    fprintf(stderr, "\n");
		}

                push_recovery_state(ps, state.last_original_state_set_el, cost);
	    }
            core_symb_ids = core_symb_ids_find(ps, ps->new_core, ps->input[ps->tok_i].symb);
            if (core_symb_ids == NULL)
            {
                break;
            }
            YaepSymbol *NEXT_TERMINAL = NULL;
            if (ps->tok_i + 1 < ps->input_len)
            {
                NEXT_TERMINAL = ps->input[ps->tok_i + 1].symb;
            }
            complete_and_predict_new_state_set(ps, ps->new_set, core_symb_ids, NEXT_TERMINAL);
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
        symbol_print(stderr, ps->input[ps->tok_i].symb, true);
        fprintf(stderr, ", Current set=%d:\n", ps->state_set_k);
        if (ps->run.debug)
        {
            print_state_set(ps, stderr, ps->state_sets[ps->state_set_k],
                            ps->state_set_k, ps->run.debug, ps->run.debug);
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
