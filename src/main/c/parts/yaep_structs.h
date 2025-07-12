/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018 Vladimir Makarov <vmakarov@gcc.gnu.org>
   Copyright (c) 2024-2025 Fredrik Öhrström <oehrstroem@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
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

/* This is interface file of general (working on any context free grammar)
   syntax parser with minimal error recovery and syntax directed translation.
   The algorithm is originated from Earley's algorithm. The algorithm
   is sufficiently fast to be used in serious language processors. */

#ifndef YAEP_STRUCTS_H
#define YAEP_STRUCTS_H

#ifndef BUILDING_DIST_XMQ

#include <stdbool.h>

#include "yaep.h"
#include "yaep_allocate.h"
#include "yaep_hashtab.h"
#include "yaep_objstack.h"
#include "yaep_vlobject.h"

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

struct YaepCoreSymbToPredComps;
typedef struct YaepCoreSymbToPredComps YaepCoreSymbToPredComps;

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
       corresponding to the last occurred error code. */
    char error_message[YAEP_MAX_ERROR_MESSAGE_LENGTH + 1];

    /* The grammar axiom is named $. */
    YaepSymbol *axiom;

    /* The end marker denotes EOF in the input token sequence. */
    YaepSymbol *end_marker;

    /* The term error is used for create error recovery nodes. */
    YaepSymbol *term_error;

    /* And its internal id. */
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
    char hr[7];    // #1ffff or ' ' or #78 or #0 (for eof)
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
               The term_ids are used for picking the bit in the bit arrays. */
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
               nonterminal. */
            bool loop_p;
            /* The following members are FIRST and FOLLOW sets of the nonterminal. */
            terminal_bitset_t *first, *follow;
        } nonterminal;
    } u;
    /* If true, the use terminal in union. */
    bool is_terminal;
    /* The following member value(if defined) is true if the symbol is
       accessible(derivated) from the axiom. */
    bool access_p;
    /* The following member is true if it is a termainal/nonterminal which derives a terminal string. */
    bool derivation_p;
    /* The following is true if it is a nonterminal which may derive the empty string. */
    bool empty_p;
    /* If the rule is a not lookahead. */
    bool is_not_lookahead_p;
#ifdef USE_CORE_SYMB_HASH_TABLE
    /* The following is used as cache for subsequent search for
       core_symb_ids with given symb. */
    YaepCoreSymbToPredComps *cached_core_symb_ids;
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
    hash_table_t map_repr_to_symb;        /* key is `repr'*/
    hash_table_t map_code_to_symb;        /* key is `code'*/

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

    /* Their values are number of terminal sets and their overall size. */
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

struct YaepCoreSymbToPredComps
{
    /* Unique incrementing id. Not strictly necessary but useful for debugging. */
    int id;

    /* The set core. */
    YaepStateSetCore *core;

    /* The symbol. */
    YaepSymbol *symb;

    /* The following vector contains ids of dotted_rules with given symb in dotted_rule after dot.
       We use this to predict the next set of dotted rules to add after symb has been reach in state core. */
    YaepVect predictions;

    /* The following vector contains id of completed dotted_rule with given symb in lhs. */
    YaepVect completions;
};

/* A StateSetCore is a state set in Earley's algorithm but without matched lengths for the dotted rules.
   The state set cores can be reused between state sets and thus save memory. */
struct YaepStateSetCore
{
    /* The following is unique number of the set core. It is defined only after forming all set. */
    int id;

    /* The state set core hash.  We save it as it is used several times. */
    unsigned int hash;

    /* The following is term shifting which resulted into this core.  It
       is defined only after forming all set. */
    YaepSymbol *term;

    /* The variable num_dotted_rules are all dotted_rules in the set. Both starting and predicted. */
    int num_dotted_rules;
    int num_started_dotted_rules;
    // num_not_yet_started_dotted_rules== num_dotted_rules-num_started_dotted_rules

    /* Array of dotted_rules.  Started dotted_rules are always placed the
       first in the order of their creation(with subsequent duplicates
       are removed), then not_yet_started noninitial(dotted_rule with at least
       one symbol before the dot) dotted_rules are placed and then initial
       dotted_rules are placed.  You should access to a set dotted_rule only
       through this member or variable `new_dotted_rules' (in other words don't
       save the member value in another variable). */
    YaepDottedRule **dotted_rules;

    /* The following member is number of started dotted_rules and not-yet-started
       (noninitial) dotted_rules whose matched_length is defined from a start
       dotted_rule matched_length.  All not-yet-started initial dotted_rules have zero
       matched_lengths.  This matched_lengths are not stored. */
    int num_all_matched_lengths;

    /* The following is an array containing the number of dotted rules from
       which matched_length of dotted_rule with given index (between n_start_dotted_rules -> num_all_matched_lengths) is taken. */
    int *parent_dotted_rule_ids;
};

/* A YaepStateSet (aka parse list) stores chart entries (aka items) [from, to, S →  VP 🞄 NP ]
   Scanning an input token triggers the creation of a state set. If we have n input tokens,
   then we have n+2  state sets (we add the final eof token and a final state after eof
   has been scanned.) */
struct YaepStateSet
{
    /* The following is unique number of the state set. */
    int id;

    /* The following is set core of the set.  You should access to set
       core only through this member or variable `new_core'(in other
       words don't save the member value in another variable). */
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

    /* The following is number of dotted_rule dyn_lookahead_context which is number of
       the corresponding terminal set in the table.  It is really used
       only for dynamic lookahead. */
    int dyn_lookahead_context;

    /* The following member is the dotted_rule lookahead it is equal to
       FIRST(the dotted_rule tail || FOLLOW(lhs)) for statik lookaheads
       and FIRST(the dotted_rule tail || dyn_lookahead_context) for dynamic ones. */
    terminal_bitset_t *lookahead;

    /* Debug information about which call added this dotted rule. */
    const char *info;
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
       in lhs of the rule. */
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

    const char *anode;                /* abstract node name if any. */
    int anode_cost;                /* the cost of the abstract node if any, otherwise 0. */
    int trans_len;                /* number of symbol translations in the rule translation. */

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

    /* The following is the first rule. */
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

    /* The source dotted rule id. */
    YaepDottedRule *dotted_rule;

    /* Current position in rule->rhs[]. */
    int dot_j;

    /* An index into input[] and is the starting point of the matched tokens for the rule. */
    int from_i;

    /* The current state set index into YaepParseState->state_sets. */
    int state_set_k;

    /* If the following value is NULL, then we do not need to create
       translation for this rule.  If we should create abstract node
       for this rule, the value refers for the abstract node and the
       parent_rhs_offset is undefined.  Otherwise, the two members is
       place into which we should place the translation of the rule.
       The following member is used only for states in the stack. */
    YaepParseTreeBuildState *parent_anode_state;

    /* The parent anode index into input[] */
    int parent_rhs_offset;

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
       recovery state(alternative). */
    /* The following members define what part of original(at the error
       recovery start) state set will be head of error recovery state.  The
       head will be all states from original state set with indexes in range
       [0, last_original_state_set_el]. */
    int last_original_state_set_el;
    /* The following two members define tail of state set for this error recovery state. */
    int state_set_tail_length;
    YaepStateSet **state_set_tail;
    /* The following member is index of start token for given error recovery state. */
    int start_tok;
    /* The following member value is number of tokens already ignored in
       order to achieved given error recovery state. */
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

    /* The following says that variables new_set and new_core are defined
       including their members. Before new set is ready, then the access to
       data of the set being formed is only possible through the new_dotted_rules,
       new_matched_lenghts and new_num_dotted_rules variables. */
    int new_set_ready_p;

    /* The following variable is set being created.
       This variable is defined only when new_set_ready_p is true. */
    YaepStateSet *new_set;

   /* The following variable is always set core of set being created.
      Member core of new_set has always the following value.
      This variable is defined only when new_set_ready_p is true. */
    YaepStateSetCore *new_core;

   /* To optimize code we use the following variables to access to data
      of new set. I.e. the point into the new set if new_set_read_p.
      Theses variables are always defined and correspondingly
      dotted_rules, matched_lengths,
      and the current number of started dotted_rules
      of the set being formed. */
    YaepDottedRule **new_dotted_rules;
    int *new_matched_lengths;
    int new_num_leading_dotted_rules;

    /* The following are number of unique set cores and their start
       dotted_rules, unique matched_length vectors and their summary length, and
       number of parent indexes. */
    int num_set_cores, num_set_core_start_dotted_rules;
    int num_set_matched_lengths, num_set_matched_lengths_len;
    int num_parent_dotted_rule_ids;

    /* Number of state sets and their number of dotted_rules. */
    int num_sets_total, num_dotted_rules_total;

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
    hash_table_t cache_stateset_cores;                /* key is the started dotted rules from a state set. */
    hash_table_t cache_matched_lengthses;             /* key is matched_lengths.*/
    hash_table_t set_of_tuples_core_matched_lengths;        /* key is(core, matched_lengths).*/

    /* Table for triplets (core, term, lookahead). */
    hash_table_t set_of_triplets_core_term_lookahead;        /* key is (core, term, lookeahed). */

    /* The following contains current number of unique dotted_rules. */
    int num_all_dotted_rules;

    /* The following two dimensional array(the first dimension is dyn_lookahead_context
       number, the second one is dotted_rule number) contains references to
       all possible dotted_rules.*/
    YaepDottedRule ***dotted_rules_table;

    /* The following vlo is indexed by dotted_rule dyn_lookahead_context number and gives
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
       their summary(transitive) prediction and completed vectors length,
       unique(transitive) prediction vectors and their summary length,
       and unique completed vectors and their summary length. */
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
    hash_table_t map_core_symb_to_vect;        /* key is set_core and symb.*/
#else
    /* The following two variables contains table(set core,
       symbol)->core_symb_ids implemented as two dimensional array.*/
    /* The following object contains pointers to the table rows for each
       set core.*/
    vlo_t core_symb_table_vlo;

    /* The following is always start of the previous object.*/
    YaepCoreSymbToPredComps ***core_symb_table;

    /* The following contains rows of the table.  The element in the rows
       are indexed by symbol number.*/
    os_t core_symb_tab_rows;
#endif

    /* The following tables contains references for core_symb_ids which
       (through(transitive) predictions and completions correspondingly)
       refers for elements which are in the tables.  Sequence elements are
       stored in one exemplar to save memory.*/
    hash_table_t map_transition_to_coresymbvect;        /* key is elements.*/
    hash_table_t map_reduce_to_coresymbvect;        /* key is elements.*/

    /* Store state sets in a growing array. Even though early parser
       specifies a new state set per token, we can reuse a state set if
       the matched lengths are the same. This means that the
       state_set_k can increment fewer times than tok_i. */
    YaepStateSet **state_sets;
    int state_set_k;

    /* The following is number of created terminal, abstract, and
       alternative nodes. */
    int n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

    /* All tail sets of error recovery are saved in the following os. */
    os_t recovery_state_tail_sets;

    /* The following variable values is state_set_k and tok_i at error
       recovery start(when the original syntax error has been fixed). */
    int recovery_start_set_k, recovery_start_tok_i;

    /* The following variable value means that all error sets in pl with
       indexes [back_state_set_frontier, recovery_start_set_k] are being processed or
       have been processed. */
    int back_state_set_frontier;

    /* The following variable stores original state set tail in reversed order.
       This object only grows.  The last object sets may be used to
       restore original state set in order to try another error recovery state
       (alternative). */
    vlo_t original_state_set_tail_stack;

    /* The following variable value is last state set element which is original
       set(set before the error_recovery start). */
    int original_last_state_set_el;

    /* This page contains code for work with array of vlos.  It is used
       only to implement abstract data `core_symb_ids'. */

    /* All vlos being formed are placed in the following object. */
    vlo_t vlo_array;

    /* The following is current number of elements in vlo_array. */
    int vlo_array_len;

    /* The following table is used to find allocated memory which should not be freed. */
    hash_table_t set_of_reserved_memory; // (key is memory pointer)

    /* The following vlo will contain references to memory which should be
       freed.  The same reference can be represented more on time. */
    vlo_t tnodes_vlo;

    /* The key of the following table is node itself. */
    hash_table_t map_node_to_visit;

    /* All translation visit nodes are placed in the following stack.  All
       the nodes are in the table.*/
    os_t node_visits_os;

    /* The following value is number of translation visit nodes. */
    int num_nodes_visits;

    /* How many times we reuse Earley's sets without their recalculation. */
    int n_goto_successes;

    /* The following vlo is error recovery states stack.  The stack
       contains error recovery state which should be investigated to find
       the best error recovery. */
    vlo_t recovery_state_stack;

    /* The following os contains all allocated parser states. */
    os_t parse_state_os;

    /* The following variable refers to head of chain of already allocated
       and then freed parser states. */
    YaepParseTreeBuildState *free_parse_state;

    /* The following table is used to make translation for ambiguous
       grammar more compact.  It is used only when we want all
       translations. */
    hash_table_t map_rule_orig_statesetind_to_internalstate;        /* Key is rule, origin, state_set_k.*/

    int core_symb_to_pred_comps_counter;
};
typedef struct YaepParseState YaepParseState;

#define CHECK_PARSE_STATE_MAGIC(ps) (ps->magic_cookie == 736268273)
#define INSTALL_PARSE_STATE_MAGIC(ps) ps->magic_cookie=736268273

struct StateVars;
typedef struct StateVars StateVars;

struct StateVars
{
    int state_id;
    int core_id;
    int num_started_dotted_rules;
    int num_dotted_rules;
    int num_all_matched_lengths;
    YaepDottedRule **dotted_rules;
    int *matched_lengths;
    int *parent_dotted_rule_ids;
};

#endif
