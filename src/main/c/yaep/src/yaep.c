/*
  YAEP(Yet Another Earley Parser)

  Copyright(c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>
  Copyright(c) 2024 Fredrik Öhrström <oehrstroem@gmail.com>

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
   This file implements parsing any context free grammar with minimal
   error recovery and syntax directed translation.  The algorithm is
   originated from Earley's algorithm.  The algorithm is sufficiently
   fast to be used in serious language processors.

   2024 Fredrik Öhrström
   Refactored to fit ixml use case, removed global variables, restructured
   code, commented and renamed variables and structures, added ixml charset
   matching.

   Terminology:

   Input tokens: The content to be parsed stored as an array of symbols (with attribytes attached).
                 The symbols can be lexer symbols or unicode characters (IXML).
   Rule: a grammar rule S -> NP VP
   Production: a rule put into production: NP 🞄 VP [origin]
   StateSet: The state of a parse, has started and not-yet-started productions (copies of rules)
       The started productions have distances to their origin in the source
       StateSetCore + distances
   StateSetCore: part of a state set that can be shared.


*/

#include <assert.h>

#define TRACE_F(ps) { \
        if (false && ps->run.trace) fprintf(stderr, "TRACE %s\n", __func__); \
    }

#define TRACE_FA(ps, cformat, ...) { \
    if (false && ps->run.trace) fprintf(stderr, "TRACE %s " cformat "\n", __func__, __VA_ARGS__); \
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

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* Terminals are stored a in term set using bits in a bit array.
   The array consists of long ints, typedefed as term_set_el_t.
   A long int is 8 bytes, ie 64 bits. */
typedef long int term_set_el_t;

/* Calculate the number of required term set elements from the number of bits we want to store. */
#define CALC_NUM_ELEMENTS(num_bits) ((num_bits+63)/64)

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

/* The following is default number of tokens sucessfully matched to
   stop error recovery alternative(state).*/
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* Define this if you want to reuse already calculated state sets.
   It considerably speed up the parser. */
//define USE_SET_HASH_TABLE

/* This does not seem to be enabled by default? */
//define USE_CORE_SYMB_HASH_TABLE

/* Maximal goto sets saved for triple(set, terminal, lookahead). */
#define MAX_CACHED_GOTO_RESULTS 3

/* Prime number(79087987342985798987987) mod 32 used for hash calculations. */
static const unsigned jauquet_prime_mod32 = 2053222611;

/* Shift used for hash calculations. */
static const unsigned hash_shift = 611;

struct YaepSymb;
typedef struct YaepSymb YaepSymb;

struct YaepVocabulary;
typedef struct YaepVocabulary YaepVocabulary;

struct YaepTermSet;
typedef struct YaepTermSet YaepTermSet;

struct YaepTermStorage;
typedef struct YaepTermStorage YaepTermStorage;

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

struct YaepProduction;
typedef struct YaepProduction YaepProduction;

struct YaepInputToken;
typedef struct YaepInputToken YaepInputToken;

struct YaepStateSetTermLookAhead;
typedef struct YaepStateSetTermLookAhead YaepStateSetTermLookAhead;

struct YaepInternalParseState;
typedef struct YaepInternalParseState YaepInternalParseState;

struct YaepTreeNodeVisit;
typedef struct YaepTreeNodeVisit YaepTreeNodeVisit;

// Structure definitions ////////////////////////////////////////////////////

struct YaepGrammar
{
    /* The following member is TRUE if the grammar is undefined(you
       should set up the grammar by yaep_read_grammar or yaep_parse_grammar)
       or bad(error was occured in setting up the grammar). */
    int undefined_p;

    /* This member always contains the last occurred error code for given grammar. */
    int error_code;

    /* This member contains message are always contains error message
       corresponding to the last occurred error code.*/
    char error_message[YAEP_MAX_ERROR_MESSAGE_LENGTH + 1];

    /* The grammar axiom is named $S. */
    YaepSymb *axiom;

    /* The following auxiliary symbol denotes EOF.*/
    YaepSymb *end_marker;

    /* The following auxiliary symbol is used for describing error recovery.*/
    YaepSymb *term_error;

    /* And its internal id.*/
    int term_error_id;

    /* The level of usage of lookaheads:
       0    - no usage
       1    - statik lookaheads
       >= 2 - dynamic lookaheads*/
    int lookahead_level;

    /* The following value means how much subsequent tokens should be
       successfuly shifted to finish error recovery.*/
    int recovery_token_matches;

    /* The following value is TRUE if we need only one parse.*/
    int one_parse_p;

    /* The following value is TRUE if we need parse(s) with minimal costs.*/
    int cost_p;

    /* The following value is TRUE if we need to make error recovery.*/
    int error_recovery_p;

    /* The following vocabulary used for this grammar.*/
    YaepVocabulary *symbs_ptr;

    /* The following rules used for this grammar.*/
    YaepRuleStorage *rules_ptr;

    /* The following terminal sets used for this grammar.*/
    YaepTermStorage *term_sets_ptr;

    /* Allocator.*/
    YaepAllocator *alloc;

    /* A user supplied pointer that is available to user callbacks through the grammar pointer. */
    void *user_data;
};

struct YaepSymb
{
    /* The following is external representation of the symbol.  It
       should be allocated by parse_alloc because the string will be
       referred from parse tree. */
    const char *repr;
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
        } term;
        struct
        {
            /* The following refers for all rules with the nonterminal
               symbol is in the left hand side of the rules. */
            YaepRule*rules;
            /* Each nonterm is given a unique integer starting from 0. */
            int nonterm_id;
            /* The following value is nonzero if nonterminal may derivate
               itself.  In other words there is a grammar loop for this
               nonterminal.*/
            int loop_p;
            /* The following members are FIRST and FOLLOW sets of the nonterminal. */
            term_set_el_t *first, *follow;
        } nonterm;
    } u;
    /* The following member is TRUE if it is nonterminal.*/
    char term_p;
    /* The following member value(if defined) is TRUE if the symbol is
       accessible(derivated) from the axiom.*/
    char access_p;
    /* The following member is TRUE if it is a termainal or it is a
       nonterminal which derivates a terminal string.*/
    char derivation_p;
    /* The following is TRUE if it is nonterminal which may derivate
       empty string.*/
    char empty_p;
    /* The following member is order number of symbol.*/
    int num;
#ifdef USE_CORE_SYMB_HASH_TABLE
    /* The following is used as cache for subsequent search for
       core_symb_vect with given symb.*/
    YaepCoreSymbVect *cached_core_symb_vect;
#endif
};

/* The following structure contians all information about grammar vocabulary.*/
struct YaepVocabulary
{

    int num_terms, num_nonterms;

    /* All symbols are placed in the following object.*/
    os_t symbs_os;

    /* All references to the symbols, terminals, nonterminals are stored
       in the following vlos.  The indexes in the arrays are the same as
       corresponding symbol, terminal, and nonterminal numbers.*/
    vlo_t symbs_vlo;
    vlo_t terms_vlo;
    vlo_t nonterms_vlo;

    /* The following are tables to find terminal by its code and symbol by
       its representation.*/
    hash_table_t map_repr_to_symb;	/* key is `repr'*/
    hash_table_t map_code_to_symb;	/* key is `code'*/

    /* If terminal symbol codes are not spared(in this case the member
       value is not NULL, we use translation vector instead of hash table. */
    YaepSymb **symb_code_trans_vect;
    int symb_code_trans_vect_start;
    int symb_code_trans_vect_end;
};

/* A set of terminals represented as a bit array. */
struct YaepTermSet
{
    // Set identity.
    int id;

    // Number of long ints (term_set_el_t) used to store the bit array.
    int num_elements;

    // The bit array itself.
    term_set_el_t *set;
};

/* The following container for the abstract data.*/
struct YaepTermStorage
{
    /* All terminal sets are stored in the following os. */
    os_t term_set_os;

    /* Their values are number of terminal sets and their overall size.*/
    int n_term_sets, n_term_sets_size;

    /* The YaepTermSet objects are stored in this vlo. */
    vlo_t term_set_vlo;

    /* Hashmap from key set (a bit array) to the YaepTermSet object from which we use the id. */
    hash_table_t map_term_set_to_id;
};

/* This page contains table for fast search for vector of indexes of
   productions with symbol after dot in given set core. */
struct YaepVect
{
    /* The following member is used internally.  The value is
       nonnegative for core_symb_vect being formed.  It is index of vlo
       in vlos array which contains the vector elements.*/
    int intern;

    /* The following memebers defines array of indexes of productions in
       given set core.  You should access to values through these
       members(in other words don't save the member values in another
       variable).*/
    int len;
    int *els;
};

/* The following is element of the table.*/
struct YaepCoreSymbVect
{
    /* The set core.*/
    YaepStateSetCore *set_core;

    /* The symbol.*/
    YaepSymb *symb;

    /* The following vector contains indexes of productions with given symb in production after dot.*/
    YaepVect transitions;

    /* The following vector contains indexes of reduce production with given symb in lhs. */
    YaepVect reduces;
};

/* A StateSetCore is a set in Earley's algorithm however without
   distance information. Because there are many duplications of such
   structures we extract the set cores and store them in one
   exemplar. */
struct YaepStateSetCore
{
    /* The following is unique number of the set core. It is defined only after forming all set.*/
    int core_id;

    /* The state set core hash.  We save it as it is used several times. */
    unsigned int hash;

    /* The following is term shifting which resulted into this core.  It
       is defined only after forming all set.*/
    YaepSymb *term;

    /* The variable num_productionsare all productions in the set. Both starting and predicted. */
    int num_productions;
    int num_started_productions;
    // num_not_yet_started_productions== num_productions-num_started_productions

    /* Array of productions.  Start productions are always placed the
       first in the order of their creation(with subsequent duplicates
       are removed), then nonstart noninitial(production with at least
       one symbol before the dot) productions are placed and then initial
       productions are placed.  You should access to a set production only
       through this member or variable `new_productions'(in other words don't
       save the member value in another variable).*/
    YaepProduction **productions;

    /* The following member is number of started productions and not-yet-started
       (noninitial) productions whose distance is defined from a start
       production distance.  All not-yet-started initial productions have zero
       distances.  This distances are not stored. */
    int n_all_distances;

    /* The following is array containing number of start production from
       which distance of(nonstart noninitial) production with given
       index(between n_start_productions -> n_all_distances) is taken. */
    int *parent_indexes;
};

/* The following describes a state set in Earley's algorithm. */
struct YaepStateSet
{
    /* The following is set core of the set.  You should access to set
       core only through this member or variable `new_core'(in other
       words don't save the member value in another variable).*/
    YaepStateSetCore *core;

    /* Hash of the set distances. We save it as it is used several times. */
    unsigned int distances_hash;

    /* The following is distances only for started productions.  Not-yet-started
       productions have their distances set to 0 implicitly.  A started production
       in the set core and its corresponding distance have the same index.
       You should access to distances only through this member or
       variable `new_distances' (in other words don't save the member value
       in another variable). */
    int *distances;
};

/* The following describes abstract data production without distance of its original
   set.  To save memory we extract such structure because there are
   many duplicated structures. */
struct YaepProduction
{
    /* Unique production identifier. */
    int prod_id;

    /* The following is the production rule. */
    YaepRule *rule;

    /* The following is position of dot in rhs of the production rule.
       Starts at 0 (left of all rhs terms) and ends at rhs.len (right of all rhs terms). */
    short dot_i;

    /* The following member is TRUE if the tail can derive empty string. */
    char empty_tail_p;

    /* The following is number of production context which is number of
       the corresponding terminal set in the table.  It is really used
       only for dynamic lookahead. */
    int context;

    /* The following member is the production lookahead it is equal to
       FIRST(the production tail || FOLLOW(lhs)) for statik lookaheads
       and FIRST(the production tail || context) for dynamic ones. */
    term_set_el_t *lookahead;
};

/* The following describes input token.*/
struct YaepInputToken
{
    /* A symbol has a name "BEGIN",code 17, or for ixml "A",code 65. */
    YaepSymb *symb;

    /* The token can have a provided attribute attached. This does not affect
       the parse, but can be extracted from the final parse tree. */
    void *attr;
};

/* The triple and possible goto sets for it. */
struct YaepStateSetTermLookAhead
{
    YaepStateSet*set;
    YaepSymb*term;
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

/* The following describes rule of grammar.*/
struct YaepRule
{
    /* The following is order number of rule.*/
    int num;

    /* The following is length of rhs.*/
    int rhs_len;

    /* The following is the next grammar rule.*/
    YaepRule *next;

    /* The following is the next grammar rule with the same nonterminal
       in lhs of the rule.*/
    YaepRule*lhs_next;

    /* The following is nonterminal in the left hand side of the rule.*/
    YaepSymb *lhs;

    /* The ixml default mark of the rule*/
    char mark;

    /* The following is symbols in the right hand side of the rule.*/
    YaepSymb **rhs;

    /* The ixml marks for all the terms in the right hand side of the rule.*/
    char *marks;
    /* The following three members define rule translation.*/

    const char *anode;		/* abstract node name if any.*/
    int anode_cost;		/* the cost of the abstract node if any, otherwise 0.*/
    int trans_len;		/* number of symbol translations in the rule translation.*/

    /* The following array elements correspond to element of rhs with
       the same index.  The element value is order number of the
       corresponding symbol translation in the rule translation.  If the
       symbol translation is rejected, the corresponding element value is
       negative.*/
    int *order;

    /* The following member value is equal to size of all previous rule
       lengths + number of the previous rules.  Imagine that all left
       hand symbol and right hand size symbols of the rules are stored
       in array.  Then the following member is index of the rule lhs in
       the array.*/
    int rule_start_offset;

    /* The following is the same string as anode but memory allocated in
       parse_alloc.*/
    char *caller_anode;
};

/* The following container for the abstract data.*/
struct YaepRuleStorage
{
    /* The following is number of all rules and their summary rhs length. */
    int n_rules, n_rhs_lens;

    /* The following is the first rule.*/
    YaepRule *first_rule;

    /* The following is rule being formed. */
    YaepRule *current_rule;

    /* All rules are placed in the following object.*/
    os_t rules_os;
};

/* The following describes parser state.*/
struct YaepInternalParseState
{
    /* The rule which we are processing.*/
    YaepRule *rule;

    /* Position in the rule where we are now.*/
    int dot_i;

    /* The rule origin (start point of derivated string from rule rhs)
       and the current state set index position. I.e. the same
       as the current_input_token_i. */
    int origin_i, current_state_set_i;

    /* If the following value is NULL, then we do not need to create
       translation for this rule.  If we should create abstract node
       for this rule, the value refers for the abstract node and the
       displacement is undefined.  Otherwise, the two members is
       place into which we should place the translation of the rule.
       The following member is used only for states in the stack.*/
    YaepInternalParseState *parent_anode_state;
    int parent_disp;

    /* The following is used only for states in the table.*/
    YaepTreeNode *anode;
};

/* To make better traversing and don't waist tree parse memory,
   we use the following structures to enumerate the tree node. */
struct YaepTreeNodeVisit
{
    /* The following member is order number of the node.  This value is
       negative if we did not visit the node yet.*/
    int num;
    /* The tree node itself.*/
    YaepTreeNode*node;
};

struct YaepParseState
{
    YaepParseRun run;
    int magic_cookie; // Must be set to 736268273 when the state is created.

    /* The input token array to be parsed. */
    YaepInputToken *input_tokens;
    int input_tokens_len;
    vlo_t input_tokens_vlo;

    /* When parsing, the current input token is incremented from 0 to len. */
    int current_input_token_i;

    /* The following says that new_set, new_core and their members are
       defined. Before this the access to data of the set being formed
       are possible only through the following variables. */
    int new_set_ready_p;

    /* The following variable is set being created. It is defined only when new_set_ready_p is TRUE. */
    YaepStateSet *new_set;

   /* The following variable is always set core of set being created.
      Member core of new_set has always the
      following value.  It is defined only when new_set_ready_p is TRUE. */
    YaepStateSetCore *new_core;

   /* To optimize code we use the following variables to access to data
      of new set.  They are always defined and correspondingly
      productions, distances, and the current number of start productions
      of the set being formed.*/
    YaepProduction **new_productions;
    int *new_distances;
    int new_num_started_productions;

    /* The following are number of unique set cores and their start
       productions, unique distance vectors and their summary length, and
       number of parent indexes. */
    int n_set_cores, n_set_core_start_productions;
    int n_set_distances, n_set_distances_len, n_parent_indexes;

    /* Number unique sets and their start productions. */
    int n_sets, n_sets_start_productions;

    /* Number unique triples(core, term, lookahead). */
    int num_triplets_core_term_lookahead;

    /* The set cores of formed sets are placed in the following os.*/
    os_t set_cores_os;

    /* The productions of formed sets are placed in the following os.*/
    os_t set_productions_os;

    /* The indexes of the parent start productions whose distances are used
       to get distances of some nonstart productions are placed in the
       following os.*/
    os_t set_parent_indexes_os;

    /* The distances of formed sets are placed in the following os.*/
    os_t set_distances_os;

    /* The sets themself are placed in the following os.*/
    os_t sets_os;

    /* Container for triples(set, term, lookahead. */
    os_t triplet_core_term_lookahead_os;

    /* The following 3 tables contain references for sets which refers
       for set cores or distances or both which are in the tables.*/
    hash_table_t set_of_cores;	/* key is only start productions.*/
    hash_table_t set_of_distanceses;	/* key is distances we have a set of distanceses.*/
    hash_table_t set_of_tuples_core_distances;	/* key is(core, distances).*/

    /* Table for triplets (core, term, lookahead). */
    hash_table_t set_of_triplets_core_term_lookahead;	/* key is (core, term, lookeahed). */

    /* The following contains current number of unique productions. */
    int n_all_productions;

    /* The following two dimensional array(the first dimension is context
       number, the second one is production number) contains references to
       all possible productions.*/
    YaepProduction ***prod_table;

    /* The following vlo is indexed by production context number and gives
       array which is indexed by production number
      (prod->rule->rule_start_offset + prod->dot_i).*/
    vlo_t prod_table_vlo;

    /* All productions are placed in the following object.*/
    os_t productions_os;

    /* The set of pairs (production,distance) used for test-setting such pairs
       is implemented using a vec[prod id] -> vec[distance] -> generation since prod_id
       is unique and incrementing, we use a vector[max_prod_id] to find another vector[max_distance]
       each distance entry storing a generation number. To clear the set of pairs
       we only need to increment the current generation below. Yay! No need to free, alloc, memset.*/
    vlo_t production_distance_vec_vlo;

    /* The value used to check the validity of elements of check_dist structures. */
    int production_distance_vec_generation;

    /* The following are number of unique(set core, symbol) pairs and
       their summary(transitive) transition and reduce vectors length,
       unique(transitive) transition vectors and their summary length,
       and unique reduce vectors and their summary length. */
    int n_core_symb_pairs, n_core_symb_vect_len;
    int n_transition_vects, n_transition_vect_len;
    int n_reduce_vects, n_reduce_vect_len;

    /* All triples(set core, symbol, vect) are placed in the following object. */
    os_t core_symb_vect_os;

    /* Pointers to triples(set core, symbol, vect) being formed are
       placed in the following object. */
    vlo_t new_core_symb_vect_vlo;

    /* All elements of vectors in the table(see
       (transitive_)transition_els_tab and reduce_els_tab) are placed in
       the following os. */
    os_t vect_els_os;

#ifdef USE_CORE_SYMB_HASH_TABLE
    hash_table_t map_core_symb_to_vect;	/* key is set_core and symb.*/
#else
    /* The following two variables contains table(set core,
       symbol)->core_symb_vect implemented as two dimensional array.*/
    /* The following object contains pointers to the table rows for each
       set core.*/
    vlo_t core_symb_table_vlo;

    /* The following is always start of the previous object.*/
    YaepCoreSymbVect ***core_symb_table;

    /* The following contains rows of the table.  The element in the rows
       are indexed by symbol number.*/
    os_t core_symb_tab_rows;
#endif

    /* The following tables contains references for core_symb_vect which
       (through(transitive) transitions and reduces correspondingly)
       refers for elements which are in the tables.  Sequence elements are
       stored in one exemplar to save memory.*/
    hash_table_t map_transition_to_coresymbvect;	/* key is elements.*/
    hash_table_t map_reduce_to_coresymbvect;	/* key is elements.*/

    /* The following two variables represents Earley's parser list. */
    YaepStateSet **state_sets;
    int state_set_curr;

    /* The following is number of created terminal, abstract, and
       alternative nodes.*/
    int n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

    /* All tail sets of error recovery are saved in the following os.*/
    os_t recovery_state_tail_sets;

    /* The following variable values is state_set_curr and current_input_token_i at error
       recovery start(when the original syntax error has been fixed).*/
    int recovery_start_set_curr, recovery_start_current_input_token_i;

    /* The following variable value means that all error sets in pl with
       indexes [back_state_set_frontier, recovery_start_set_curr] are being processed or
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
       only to implement abstract data `core_symb_vect'.*/

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
    YaepInternalParseState *free_parse_state;

    /* The following table is used to make translation for ambiguous
       grammar more compact.  It is used only when we want all
       translations.*/
    hash_table_t map_rule_orig_statesetind_to_internalstate;	/* Key is rule, origin, current_state_set_i.*/
};
typedef struct YaepParseState YaepParseState;

#define CHECK_PARSE_STATE_MAGIC(ps) (ps->magic_cookie == 736268273)
#define INSTALL_PARSE_STATE_MAGIC(ps) ps->magic_cookie=736268273

// Declarations ///////////////////////////////////////////////////

static void read_input_tokens(YaepParseState *ps);
static void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node);
static void print_rule_with_dot(YaepParseState *ps, FILE *f, YaepRule *rule, int pos);
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, int trans_p);
static void print_state_set(YaepParseState *ps,
                            FILE* f,
                            YaepStateSet*set,
                            int set_dist,
                            int print_all_productions,
                            int lookahead_p);
static void print_production(YaepParseState *ps, FILE *f, YaepProduction *prod, int lookahead_p, int origin);
static YaepVocabulary *symb_init(YaepGrammar *g);
static void symb_empty(YaepParseState *ps, YaepVocabulary *symbs);
static void symb_finish_adding_terms(YaepParseState *ps);
static void symb_print(FILE* f, YaepSymb*symb, int code_p);
static void yaep_error(YaepParseState *ps, int code, const char*format, ...);

// Global variables /////////////////////////////////////////////////////

/* Jump buffer for processing errors.*/
jmp_buf error_longjump_buff;

// Implementations ////////////////////////////////////////////////////////////////////

/* Hash of symbol representation. */
static unsigned symb_repr_hash(hash_table_entry_t s)
{
    YaepSymb *sym = (YaepSymb*)s;
    unsigned result = jauquet_prime_mod32;

    for (const char *i = sym->repr; *i; i++)
    {
        result = result * hash_shift +(unsigned)*i;
    }

     return result;
}

/* Equality of symbol representations. */
static int symb_repr_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymb *sym1 = (YaepSymb*)s1;
    YaepSymb *sym2 = (YaepSymb*)s2;

    return !strcmp(sym1->repr, sym2->repr);
}

/* Hash of terminal code. */
static unsigned symb_code_hash(hash_table_entry_t s)
{
    YaepSymb *sym = (YaepSymb*)s;

    assert(sym->term_p);

    return sym->u.term.code;
}

/* Equality of terminal codes.*/
static int symb_code_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymb *sym1 = (YaepSymb*)s1;
    YaepSymb *sym2 = (YaepSymb*)s2;

    assert(sym1->term_p && sym2->term_p);

    return sym1->u.term.code == sym2->u.term.code;
}

/* Initialize work with symbols and returns storage for the symbols.*/
static YaepVocabulary *symb_init(YaepGrammar *grammar)
{
    void*mem;
    YaepVocabulary*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepVocabulary));
    result =(YaepVocabulary*) mem;
    OS_CREATE(result->symbs_os, grammar->alloc, 0);
    VLO_CREATE(result->symbs_vlo, grammar->alloc, 1024);
    VLO_CREATE(result->terms_vlo, grammar->alloc, 512);
    VLO_CREATE(result->nonterms_vlo, grammar->alloc, 512);
    result->map_repr_to_symb = create_hash_table(grammar->alloc, 300, symb_repr_hash, symb_repr_eq);
    result->map_code_to_symb = create_hash_table(grammar->alloc, 200, symb_code_hash, symb_code_eq);
    result->symb_code_trans_vect = NULL;
    result->num_nonterms = 0;
    result->num_terms = 0;

    return result;
}

/* Return symbol(or NULL if it does not exist) whose representation is REPR.*/
static YaepSymb *symb_find_by_repr(YaepParseState *ps, const char*repr)
{
    YaepSymb symb;
    symb.repr = repr;
    YaepSymb*r = (YaepSymb*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, FALSE);

    TRACE_FA(ps, "%s -> %p", repr, r);

    return r;
}

/* Return symbol(or NULL if it does not exist) which is terminal with CODE. */
static YaepSymb *symb_find_by_code(YaepParseState *ps, int code)
{
    YaepSymb symb;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        if ((code < ps->run.grammar->symbs_ptr->symb_code_trans_vect_start) ||(code >= ps->run.grammar->symbs_ptr->symb_code_trans_vect_end))
        {
            TRACE_FA(ps, "vec %d -> NULL", code);
            return NULL;
        }
        else
        {
            YaepSymb*r = ps->run.grammar->symbs_ptr->symb_code_trans_vect[code - ps->run.grammar->symbs_ptr->symb_code_trans_vect_start];
            TRACE_FA(ps, "vec %d -> %p", code, r);
            return r;
        }
    }

    symb.term_p = TRUE;
    symb.u.term.code = code;
    YaepSymb*r =(YaepSymb*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_code_to_symb, &symb, FALSE);

    TRACE_FA(ps, "hash %d -> %p", code, r);

    return r;
}

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
static YaepSymb *symb_add_term(YaepParseState *ps, const char*name, int code)
{
    YaepSymb symb, *result;
    hash_table_entry_t *repr_entry, *code_entry;

    symb.repr = name;
    symb.term_p = TRUE;
    symb.num = ps->run.grammar->symbs_ptr->num_nonterms + ps->run.grammar->symbs_ptr->num_terms;
    symb.u.term.code = code;
    symb.u.term.term_id = ps->run.grammar->symbs_ptr->num_terms++;
    symb.empty_p = FALSE;
    repr_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, TRUE);
    assert(*repr_entry == NULL);
    code_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_code_to_symb, &symb, TRUE);
    assert(*code_entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymb));
    result =(YaepSymb*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *repr_entry =(hash_table_entry_t) result;
   *code_entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymb*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->terms_vlo, &result, sizeof(YaepSymb*));

    TRACE_FA(ps, "%s %d -> %p", name, code, result);

    return result;
}

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table. The function
   should create own copy of name for the new symbol. */
static YaepSymb *symb_add_nonterm(YaepParseState *ps, const char *name)
{
    YaepSymb symb,*result;
    hash_table_entry_t*entry;

    symb.repr = name;
    symb.term_p = FALSE;
    symb.num = ps->run.grammar->symbs_ptr->num_nonterms + ps->run.grammar->symbs_ptr->num_terms;
    symb.u.nonterm.rules = NULL;
    symb.u.nonterm.loop_p = 0;
    symb.u.nonterm.nonterm_id = ps->run.grammar->symbs_ptr->num_nonterms++;
    entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, TRUE);
    assert(*entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymb));
    result =(YaepSymb*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymb*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->nonterms_vlo, &result, sizeof(YaepSymb*));

    TRACE_FA(ps, "%s -> %p", name, result);

    return result;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *symb_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->symbs_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->symbs_vlo))[n];
    assert(symb->num == n);

    TRACE_FA(ps, "%d -> %s", n, symb->repr);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *term_get(YaepParseState *ps, int n)
{
    if (n < 0 || (VLO_LENGTH(ps->run.grammar->symbs_ptr->terms_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->terms_vlo))[n];
    assert(symb->term_p && symb->u.term.term_id == n);

    TRACE_FA(ps, "%d -> %s", n, symb->repr);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *nonterm_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->nonterms_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->nonterms_vlo))[n];
    assert(!symb->term_p && symb->u.nonterm.nonterm_id == n);

    TRACE_FA(ps, "%d -> %s", n, symb->repr);

    return symb;
}

static void symb_finish_adding_terms(YaepParseState *ps)
{
    int i, max_code, min_code;
    YaepSymb*symb;
    void*mem;

    for (min_code = max_code = i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        if (i == 0 || min_code > symb->u.term.code) min_code = symb->u.term.code;
        if (i == 0 || max_code < symb->u.term.code) max_code = symb->u.term.code;
    }
    assert(i != 0);
    assert((max_code - min_code) < MAX_SYMB_CODE_TRANS_VECT_SIZE);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect_start = min_code;
    ps->run.grammar->symbs_ptr->symb_code_trans_vect_end = max_code + 1;

    size_t num_codes = max_code - min_code + 1;
    size_t vec_size = sizeof(YaepSymb*)* num_codes;
    mem = yaep_malloc(ps->run.grammar->alloc, vec_size);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect =(YaepSymb**)mem;

    for(i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        ps->run.grammar->symbs_ptr->symb_code_trans_vect[symb->u.term.code - min_code] = symb;
    }

    TRACE_FA(ps, "num_codes=%zu size=%zu", num_codes, vec_size);
}

/* Free memory for symbols. */
static void symb_empty(YaepParseState *ps, YaepVocabulary *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
        ps->run.grammar->symbs_ptr->symb_code_trans_vect = NULL;
    }

    empty_hash_table(symbs->map_repr_to_symb);
    empty_hash_table(symbs->map_code_to_symb);
    VLO_NULLIFY(symbs->nonterms_vlo);
    VLO_NULLIFY(symbs->terms_vlo);
    VLO_NULLIFY(symbs->symbs_vlo);
    OS_EMPTY(symbs->symbs_os);
    symbs->num_nonterms = symbs->num_terms = 0;

    TRACE_FA(ps, "%p\n" , symbs);
}

/* Finalize work with symbols. */
static void symb_fin(YaepParseState *ps, YaepVocabulary *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
    }

    delete_hash_table(ps->run.grammar->symbs_ptr->map_repr_to_symb);
    delete_hash_table(ps->run.grammar->symbs_ptr->map_code_to_symb);
    VLO_DELETE(ps->run.grammar->symbs_ptr->nonterms_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->terms_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->symbs_vlo);
    OS_DELETE(ps->run.grammar->symbs_ptr->symbs_os);
    yaep_free(ps->run.grammar->alloc, symbs);

    TRACE_FA(ps, "%p\n" , symbs);
}


/* Hash of table terminal set.*/
static unsigned term_set_hash(hash_table_entry_t s)
{
    YaepTermSet *ts = (YaepTermSet*)s;
    term_set_el_t *set = ts->set;
    int num_elements = ts->num_elements;
    term_set_el_t *bound = set + num_elements;
    unsigned result = jauquet_prime_mod32;

    while (set < bound)
    {
        result = result * hash_shift + *set++;
    }
    return result;
}

/* Equality of terminal sets. */
static int term_set_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepTermSet *ts1 = (YaepTermSet*)s1;
    YaepTermSet *ts2 = (YaepTermSet*)s2;
    term_set_el_t *i = ts1->set;
    term_set_el_t *j = ts2->set;

    assert(ts1->num_elements == ts2->num_elements);

    int num_elements = ts1->num_elements;
    term_set_el_t *i_bound = i + num_elements;

    while (i < i_bound)
    {
        if (*i++ != *j++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Initialize work with terminal sets and returns storage for terminal sets.*/
static YaepTermStorage *term_set_init(YaepGrammar *grammar)
{
    void*mem;
    YaepTermStorage*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepTermStorage));
    result =(YaepTermStorage*) mem;
    OS_CREATE(result->term_set_os, grammar->alloc, 0);
    result->map_term_set_to_id = create_hash_table(grammar->alloc, 1000, term_set_hash, term_set_eq);
    VLO_CREATE(result->term_set_vlo, grammar->alloc, 4096);
    result->n_term_sets = result->n_term_sets_size = 0;

    return result;
}

/* Return new terminal SET.  Its value is undefined. */
static term_set_el_t *term_set_create(YaepParseState *ps, int num_terms)
{
    int size;
    term_set_el_t*result;

    assert(sizeof(term_set_el_t) <= 8);
    size = 8;
    /* Make it 64 bit multiple to have the same statistics for 64 bit
       machines. num_terms = global variable ps->run.grammar->symbs_ptr->n_terms*/
    size =((num_terms + CHAR_BIT* 8 - 1) /(CHAR_BIT* 8))* 8;
    OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->term_set_os, size);
    result =(term_set_el_t*) OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->term_set_os);
    OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->term_set_os);
    ps->run.grammar->term_sets_ptr->n_term_sets++;
    ps->run.grammar->term_sets_ptr->n_term_sets_size += size;

    return result;
}

/* Make terminal SET empty.*/
static void term_set_clear(term_set_el_t* set, int num_terms)
{
    term_set_el_t*bound;
    int size;

    size =((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1)
            /(CHAR_BIT* sizeof(term_set_el_t)));
    bound = set + size;
    while(set < bound)
       *set++ = 0;
}

/* Copy SRC into DEST. */
static void term_set_copy(term_set_el_t *dest, term_set_el_t *src, int num_terms)
{
    term_set_el_t *bound;
    int size;

    size = ((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1) / (CHAR_BIT* sizeof(term_set_el_t)));
    bound = dest + size;

    while (dest < bound)
    {
       *dest++ = *src++;
    }
}

/* Add all terminals from set OP with to SET.  Return TRUE if SET has been changed.*/
static int term_set_or(term_set_el_t *set, term_set_el_t *op, int num_terms)
{
    term_set_el_t *bound;
    int size, changed_p;

    size = ((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1) / (CHAR_BIT* sizeof(term_set_el_t)));
    bound = set + size;
    changed_p = 0;
    while (set < bound)
    {
        if ((*set |*op) !=*set)
        {
            changed_p = 1;
        }
       *set++ |= *op++;
    }
    return changed_p;
}

/* Add terminal with number NUM to SET.  Return TRUE if SET has been changed.*/
static int term_set_up(term_set_el_t *set, int num, int num_terms)
{
    int ind, changed_p;
    term_set_el_t bit;

    assert(num < num_terms);

    ind = num / (CHAR_BIT* sizeof(term_set_el_t));
    bit = ((term_set_el_t) 1) << (num %(CHAR_BIT* sizeof(term_set_el_t)));
    changed_p =(set[ind] & bit ? 0 : 1);
    set[ind] |= bit;

    return changed_p;
}

/* Return TRUE if terminal with number NUM is in SET. */
static int term_set_test(term_set_el_t *set, int num, int num_terms)
{
    int ind;
    term_set_el_t bit;

    assert(num >= 0 && num < num_terms);

    ind = num /(CHAR_BIT* sizeof(term_set_el_t));
    bit = ((term_set_el_t) 1) << (num %(CHAR_BIT* sizeof(term_set_el_t)));

    return (set[ind] & bit) != 0;
}

/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1(which is always negative).  Don't set after
   insertion!!!*/
static int term_set_insert(YaepParseState *ps, term_set_el_t *set)
{
    hash_table_entry_t *entry;
    YaepTermSet term_set,*term_set_ptr;

    term_set.set = set;
    entry = find_hash_table_entry(ps->run.grammar->term_sets_ptr->map_term_set_to_id, &term_set, TRUE);

    if (*entry != NULL)
    {
        return -((YaepTermSet*)*entry)->id - 1;
    }
    else
    {
        OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->term_set_os, sizeof(YaepTermSet));
        term_set_ptr = (YaepTermSet*)OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->term_set_os);
        OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->term_set_os);
       *entry =(hash_table_entry_t) term_set_ptr;
        term_set_ptr->set = set;
        term_set_ptr->id = (VLO_LENGTH(ps->run.grammar->term_sets_ptr->term_set_vlo) / sizeof(YaepTermSet*));
        term_set_ptr->num_elements = CALC_NUM_ELEMENTS(ps->run.grammar->symbs_ptr->num_terms);

        VLO_ADD_MEMORY(ps->run.grammar->term_sets_ptr->term_set_vlo, &term_set_ptr, sizeof(YaepTermSet*));

        return((YaepTermSet*)*entry)->id;
    }
}

/* The following function returns set which is in the table with number NUM.*/
static term_set_el_t *term_set_from_table(YaepParseState *ps, int num)
{
    assert(num >= 0);
    assert((long unsigned int)num < VLO_LENGTH(ps->run.grammar->term_sets_ptr->term_set_vlo) / sizeof(YaepTermSet*));

    return ((YaepTermSet**)VLO_BEGIN(ps->run.grammar->term_sets_ptr->term_set_vlo))[num]->set;
}

/* Print terminal SET into file F. */
static void term_set_print(YaepParseState *ps, FILE *f, term_set_el_t *set, int num_terms)
{
    int i;

    fprintf(f, "[");
    for (i = 0; i < num_terms; i++)
    {
        if (term_set_test(set, i, num_terms))
        {
            if (i) fprintf(f, " ");
            symb_print(f, term_get(ps, i), FALSE);
        }
    }
    fprintf(f, "]");
}

/* Free memory for terminal sets. */
static void term_set_empty(YaepTermStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_NULLIFY(term_sets->term_set_vlo);
    empty_hash_table(term_sets->map_term_set_to_id);
    OS_EMPTY(term_sets->term_set_os);
    term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

/* Finalize work with terminal sets. */
static void term_set_fin(YaepGrammar *grammar, YaepTermStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_DELETE(term_sets->term_set_vlo);
    delete_hash_table(term_sets->map_term_set_to_id);
    OS_DELETE(term_sets->term_set_os);
    yaep_free(grammar->alloc, term_sets);
    term_sets = NULL;
}


/* Initialize work with rules and returns pointer to rules storage. */
static YaepRuleStorage *rule_init(YaepGrammar *grammar)
{
    void *mem;
    YaepRuleStorage *result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepRuleStorage));
    result = (YaepRuleStorage*)mem;
    OS_CREATE(result->rules_os, grammar->alloc, 0);
    result->first_rule = result->current_rule = NULL;
    result->n_rules = result->n_rhs_lens = 0;

    return result;
}

/* Create new rule with LHS empty rhs. */
static YaepRule *rule_new_start(YaepParseState *ps, YaepSymb *lhs, const char *anode, int anode_cost)
{
    YaepRule *rule;
    YaepSymb *empty;

    assert(!lhs->term_p);

    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, sizeof(YaepRule));
    rule =(YaepRule*) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    rule->lhs = lhs;
    if (anode == NULL)
    {
        rule->anode = NULL;
        rule->anode_cost = 0;
    }
    else
    {
        OS_TOP_ADD_STRING(ps->run.grammar->rules_ptr->rules_os, anode);
        rule->anode =(char*) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
        OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
        rule->anode_cost = anode_cost;
    }
    rule->trans_len = 0;
    rule->marks = NULL;
    rule->order = NULL;
    rule->next = NULL;
    if (ps->run.grammar->rules_ptr->current_rule != NULL)
    {
        ps->run.grammar->rules_ptr->current_rule->next = rule;
    }
    rule->lhs_next = lhs->u.nonterm.rules;
    lhs->u.nonterm.rules = rule;
    rule->rhs_len = 0;
    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rules_ptr->rules_os, &empty, sizeof(YaepSymb*));
    rule->rhs =(YaepSymb**) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    ps->run.grammar->rules_ptr->current_rule = rule;
    if (ps->run.grammar->rules_ptr->first_rule == NULL)
    {
        ps->run.grammar->rules_ptr->first_rule = rule;
    }
    rule->rule_start_offset = ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules;
    rule->num = ps->run.grammar->rules_ptr->n_rules++;

    return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void rule_new_symb_add(YaepParseState *ps, YaepSymb *symb)
{
    YaepSymb *empty;

    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rules_ptr->rules_os, &empty, sizeof(YaepSymb*));
    ps->run.grammar->rules_ptr->current_rule->rhs = (YaepSymb**)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    ps->run.grammar->rules_ptr->current_rule->rhs[ps->run.grammar->rules_ptr->current_rule->rhs_len] = symb;
    ps->run.grammar->rules_ptr->current_rule->rhs_len++;
    ps->run.grammar->rules_ptr->n_rhs_lens++;
}

/* The function should be called at end of forming each rule.  It
   creates and initializes production cache.*/
static void rule_new_stop(YaepParseState *ps)
{
    int i;

    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, ps->run.grammar->rules_ptr->current_rule->rhs_len* sizeof(int));
    ps->run.grammar->rules_ptr->current_rule->order = (int*)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    for(i = 0; i < ps->run.grammar->rules_ptr->current_rule->rhs_len; i++)
    {
        ps->run.grammar->rules_ptr->current_rule->order[i] = -1;
    }

    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, ps->run.grammar->rules_ptr->current_rule->rhs_len* sizeof(char));
    ps->run.grammar->rules_ptr->current_rule->marks = (char*)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
}

/* The following function frees memory for rules.*/
static void rule_empty(YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_EMPTY(rules->rules_os);
    rules->first_rule = rules->current_rule = NULL;
    rules->n_rules = rules->n_rhs_lens = 0;
}

/* Finalize work with rules.*/
static void rule_fin(YaepGrammar *grammar, YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_DELETE(rules->rules_os);
    yaep_free(grammar->alloc, rules);
    rules = NULL;
}

/* Initialize work with tokens.*/
static void tok_init(YaepParseState *ps)
{
    VLO_CREATE(ps->input_tokens_vlo, ps->run.grammar->alloc, NUM_INITIAL_YAEP_TOKENS * sizeof(YaepInputToken));
    ps->input_tokens_len = 0;
}

/* Add input token with CODE and attribute at the end of input tokens array.*/
static void tok_add(YaepParseState *ps, int code, void *attr)
{
    YaepInputToken tok;

    tok.attr = attr;
    tok.symb = symb_find_by_code(ps, code);
    if (tok.symb == NULL)
    {
        yaep_error(ps, YAEP_INVALID_TOKEN_CODE, "syntax error at offset %d '%c'", ps->input_tokens_len, code);
    }
    VLO_ADD_MEMORY(ps->input_tokens_vlo, &tok, sizeof(YaepInputToken));
    ps->input_tokens = (YaepInputToken*)VLO_BEGIN(ps->input_tokens_vlo);
    ps->input_tokens_len++;
}

/* Finalize work with tokens. */
static void tok_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->input_tokens_vlo);
}

/* Initialize work with productions.*/
static void prod_init(YaepParseState *ps)
{
    ps->n_all_productions= 0;
    OS_CREATE(ps->productions_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->prod_table_vlo, ps->run.grammar->alloc, 4096);
    ps->prod_table = (YaepProduction***)VLO_BEGIN(ps->prod_table_vlo);
}

/* The following function sets up lookahead of production SIT.  The
   function returns TRUE if the production tail may derive empty
   string.*/
static int prod_set_lookahead(YaepParseState *ps, YaepProduction *prod)
{
    YaepSymb *symb, **symb_ptr;

    if (ps->run.grammar->lookahead_level == 0)
    {
        prod->lookahead = NULL;
    }
    else
    {
        prod->lookahead = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
        term_set_clear(prod->lookahead, ps->run.grammar->symbs_ptr->num_terms);
    }
    symb_ptr = &prod->rule->rhs[prod->dot_i];
    while ((symb =*symb_ptr) != NULL)
    {
        if (ps->run.grammar->lookahead_level != 0)
	{
            if (symb->term_p)
            {
                term_set_up(prod->lookahead, symb->u.term.term_id, ps->run.grammar->symbs_ptr->num_terms);
            }
            else
            {
                term_set_or(prod->lookahead, symb->u.nonterm.first, ps->run.grammar->symbs_ptr->num_terms);
            }
	}
        if (!symb->empty_p) break;
        symb_ptr++;
    }
    if (symb == NULL)
    {
        if (ps->run.grammar->lookahead_level == 1)
        {
            term_set_or(prod->lookahead, prod->rule->lhs->u.nonterm.follow, ps->run.grammar->symbs_ptr->num_terms);
        }
        else if (ps->run.grammar->lookahead_level != 0)
        {
            term_set_or(prod->lookahead, term_set_from_table(ps, prod->context), ps->run.grammar->symbs_ptr->num_terms);
        }
        return TRUE;
    }
    return FALSE;
}

/* The following function returns productions with given
   characteristics.  Remember that productions are stored in one
   exemplar.*/
static YaepProduction *prod_create(YaepParseState *ps, YaepRule *rule, int pos, int context)
{
    YaepProduction*prod;
    YaepProduction***context_prod_table_ptr;

    assert(context >= 0);
    context_prod_table_ptr = ps->prod_table + context;

    if ((char*) context_prod_table_ptr >=(char*) VLO_BOUND(ps->prod_table_vlo))
    {
        YaepProduction***bound,***ptr;
        int i, diff;

        assert((ps->run.grammar->lookahead_level <= 1 && context == 0) || (ps->run.grammar->lookahead_level > 1 && context >= 0));
        diff = (char*) context_prod_table_ptr -(char*) VLO_BOUND(ps->prod_table_vlo);
        diff += sizeof(YaepProduction**);
        if (ps->run.grammar->lookahead_level > 1 && diff == sizeof(YaepProduction**))
        {
            diff *= 10;
        }
        VLO_EXPAND(ps->prod_table_vlo, diff);
        ps->prod_table =(YaepProduction***) VLO_BEGIN(ps->prod_table_vlo);
        bound =(YaepProduction***) VLO_BOUND(ps->prod_table_vlo);
        context_prod_table_ptr = ps->prod_table + context;
        ptr = bound - diff / sizeof(YaepProduction**);
        while(ptr < bound)
	{
            OS_TOP_EXPAND(ps->productions_os,(ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules)
                          * sizeof(YaepProduction*));
           *ptr =(YaepProduction**) OS_TOP_BEGIN(ps->productions_os);
            OS_TOP_FINISH(ps->productions_os);
            for(i = 0; i < ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules; i++)
               (*ptr)[i] = NULL;
            ptr++;
	}
    }
    if ((prod = (*context_prod_table_ptr)[rule->rule_start_offset + pos]) != NULL)
    {
        return prod;
    }
    OS_TOP_EXPAND(ps->productions_os, sizeof(YaepProduction));
    prod =(YaepProduction*) OS_TOP_BEGIN(ps->productions_os);
    OS_TOP_FINISH(ps->productions_os);
    ps->n_all_productions++;
    prod->rule = rule;
    prod->dot_i = pos;
    prod->prod_id = ps->n_all_productions;
    prod->context = context;
    prod->empty_tail_p = prod_set_lookahead(ps, prod);
    (*context_prod_table_ptr)[rule->rule_start_offset + pos] = prod;

    return prod;
}


/* Return hash of sequence of NUM_PRODUCTIONS productions in array PRODUCTIONS. */
static unsigned productions_hash(int num_productions, YaepProduction **productions)
{
    int n, i;
    unsigned result;

    result = jauquet_prime_mod32;
    for(i = 0; i < num_productions; i++)
    {
        n = productions[i]->prod_id;
        result = result* hash_shift + n;
    }
    return result;
}

/* Finalize work with productions. */
static void prod_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->prod_table_vlo);
    OS_DELETE(ps->productions_os);
}

/* Hash of set core. */
static unsigned set_core_hash(hash_table_entry_t s)
{
    return ((YaepStateSet*)s)->core->hash;
}

/* Equality of set cores. */
static int set_core_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSetCore*set_core1 = ((YaepStateSet*) s1)->core;
    YaepStateSetCore*set_core2 = ((YaepStateSet*) s2)->core;
    YaepProduction **prod_ptr1, **prod_ptr2, **prod_bound1;

    if (set_core1->num_started_productions != set_core2->num_started_productions)
    {
        return FALSE;
    }
    prod_ptr1 = set_core1->productions;
    prod_bound1 = prod_ptr1 + set_core1->num_started_productions;
    prod_ptr2 = set_core2->productions;
    while(prod_ptr1 < prod_bound1)
    {
        if (*prod_ptr1++ !=*prod_ptr2++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Hash of set distances. */
static unsigned distances_hash(hash_table_entry_t s)
{
    return((YaepStateSet*) s)->distances_hash;
}

/* Compare all the distances stored in the two state sets. */
static int distances_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSet *st1 = (YaepStateSet*)s1;
    YaepStateSet *st2 = (YaepStateSet*)s2;
    int *i = st1->distances;
    int *j = st2->distances;
    int n_distances = st1->core->num_started_productions;

    if (n_distances != st2->core->num_started_productions)
    {
        return FALSE;
    }

    int *bound = i + n_distances;
    while (i < bound)
    {
        if (*i++ != *j++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Hash of set core and distances. */
static unsigned set_core_distances_hash(hash_table_entry_t s)
{
    return set_core_hash(s)* hash_shift + distances_hash(s);
}

/* Equality of set cores and distances. */
static int set_core_distances_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSetCore *set_core1 = ((YaepStateSet*)s1)->core;
    YaepStateSetCore *set_core2 = ((YaepStateSet*)s2)->core;
    int*distances1 = ((YaepStateSet*)s1)->distances;
    int*distances2 = ((YaepStateSet*)s2)->distances;

    return set_core1 == set_core2 && distances1 == distances2;
}

/* Hash of triple(set, term, lookahead). */
static unsigned core_term_lookahead_hash(hash_table_entry_t s)
{
    YaepStateSet *set = ((YaepStateSetTermLookAhead*)s)->set;
    YaepSymb *term = ((YaepStateSetTermLookAhead*)s)->term;
    int lookahead = ((YaepStateSetTermLookAhead*)s)->lookahead;

    return ((set_core_distances_hash(set)* hash_shift + term->u.term.term_id)* hash_shift + lookahead);
}

/* Equality of tripes(set, term, lookahead).*/
static int core_term_lookahead_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepStateSet *set1 =((YaepStateSetTermLookAhead*)s1)->set;
    YaepStateSet *set2 =((YaepStateSetTermLookAhead*)s2)->set;
    YaepSymb *term1 =((YaepStateSetTermLookAhead*)s1)->term;
    YaepSymb *term2 =((YaepStateSetTermLookAhead*)s2)->term;
    int lookahead1 =((YaepStateSetTermLookAhead*)s1)->lookahead;
    int lookahead2 =((YaepStateSetTermLookAhead*)s2)->lookahead;

    return set1 == set2 && term1 == term2 && lookahead1 == lookahead2;
}

/* Initiate the set of pairs(sit, dist). */
static void production_distance_set_init(YaepParseState *ps)
{
    VLO_CREATE(ps->production_distance_vec_vlo, ps->run.grammar->alloc, 8192);
    ps->production_distance_vec_generation = 0;
}

/* The clear the set we only need to increment the generation.
   The test for set membership compares with the active generation.
   Thus all previously stored memberships are immediatly invalidated
   through the increment below. Thus clearing the set! */
static void clear_production_distance_set(YaepParseState *ps)
{
    ps->production_distance_vec_generation++;
}

/* Insert pair(PROD, DIST) into the ps->production_distance_vec_vlo.
   Each production has a unique prod_id incrementally counted from 0 to the most recent production added.
   This prod_id is used as in index into the vector, the vector storing vlo objects.
   Each vlo object maintains a memory region used for an integer array of distances.

   If such pair exists return true (was FALSE), otherwise return false. (was TRUE). */
static bool production_distance_test_and_set(YaepParseState *ps, YaepProduction *prod, int dist)
{
    int i, len, prod_id;
    vlo_t *dist_vlo;

    prod_id = prod->prod_id;

    // Expand the vector to accommodate a new production.
    len = VLO_LENGTH(ps->production_distance_vec_vlo)/sizeof(vlo_t);
    if (len <= prod_id)
    {
        VLO_EXPAND(ps->production_distance_vec_vlo,(prod_id + 1 - len)* sizeof(vlo_t));
        for(i = len; i <= prod_id; i++)
        {
            // For each new slot in the vector, initialize a new vlo, to be used for distances.
            VLO_CREATE(((vlo_t*) VLO_BEGIN(ps->production_distance_vec_vlo))[i], ps->run.grammar->alloc, 64);
        }
    }

    // Now fetch the vlo for this prod_id, which is either an existing vlo or a freshly initialized vlo.
    // The vlo stores an array of integersCheck if the vlo is big enough for this distance?
    dist_vlo = &((vlo_t*)VLO_BEGIN(ps->production_distance_vec_vlo))[prod_id];
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
    if (*generation == ps->production_distance_vec_generation)
    {
        // The pair was already inserted! We know this since we found the current generation in this slot.
        // Remember that we clear the set by incrementing the current generation.
        return true;
    }
    // The pair did not exist in the set. (Since the generation number did not match.)
    // Insert this pair my marking the vec[prod_id][dist] with the current generation.
    *generation = ps->production_distance_vec_generation;
    return false;
}

/* Finish the set of pairs(sit, dist). */
static void production_distance_set_fin(YaepParseState *ps)
{
    int i, len = VLO_LENGTH(ps->production_distance_vec_vlo) / sizeof(vlo_t);

    for(i = 0; i < len; i++)
    {
        VLO_DELETE(((vlo_t*) VLO_BEGIN(ps->production_distance_vec_vlo))[i]);
    }
    VLO_DELETE(ps->production_distance_vec_vlo);
}

/* Initialize work with sets for parsing input with N_INPUT_TOKENS tokens.*/
static void set_init(YaepParseState *ps, int n_input_tokens)
{
    int n = n_input_tokens >> 3;

    OS_CREATE(ps->set_cores_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->set_productions_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_parent_indexes_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_distances_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->sets_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->triplet_core_term_lookahead_os, ps->run.grammar->alloc, 0);
    ps->set_of_cores = create_hash_table(ps->run.grammar->alloc, 2000, set_core_hash, set_core_eq);
    ps->set_of_distanceses = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n, distances_hash, distances_eq);
    ps->set_of_tuples_core_distances = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                set_core_distances_hash, set_core_distances_eq);
    ps->set_of_triplets_core_term_lookahead = create_hash_table(ps->run.grammar->alloc, n < 30000 ? 30000 : n,
                                               core_term_lookahead_hash, core_term_lookahead_eq);
    ps->n_set_cores = ps->n_set_core_start_productions= 0;
    ps->n_set_distances = ps->n_set_distances_len = ps->n_parent_indexes = 0;
    ps->n_sets = ps->n_sets_start_productions= 0;
    ps->num_triplets_core_term_lookahead = 0;
    production_distance_set_init(ps);
}

/* The following function starts forming of new set.*/
static void set_new_start(YaepParseState *ps)
{
    ps->new_set = NULL;
    ps->new_core = NULL;
    ps->new_set_ready_p = FALSE;
    ps->new_productions = NULL;
    ps->new_distances = NULL;
    ps->new_num_started_productions = 0;
}

/* Add start PROD with distance DIST at the end of the production array
   of the state set being formed. */
static void set_new_add_start_prod(YaepParseState *ps, YaepProduction*prod, int dist)
{
    assert(!ps->new_set_ready_p);
    OS_TOP_EXPAND(ps->set_distances_os, sizeof(int));
    ps->new_distances =(int*) OS_TOP_BEGIN(ps->set_distances_os);
    OS_TOP_EXPAND(ps->set_productions_os, sizeof(YaepProduction*));
    ps->new_productions =(YaepProduction**) OS_TOP_BEGIN(ps->set_productions_os);
    ps->new_productions[ps->new_num_started_productions] = prod;
    ps->new_distances[ps->new_num_started_productions] = dist;
    ps->new_num_started_productions++;
}

/* Add nonstart, noninitial PROD with distance DIST at the end of the
   production array of the set being formed.  If this is production and
   there is already the same pair(production, the corresponding
   distance), we do not add it.*/
static void set_add_new_nonstart_prod(YaepParseState *ps, YaepProduction*prod, int parent)
{
    int i;

    assert(ps->new_set_ready_p);
    /* When we add not-yet-started productions we need to have pairs
      (production, the corresponding distance) without duplicates
       because we also forms core_symb_vect at that time.*/
    for(i = ps->new_num_started_productions; i < ps->new_core->num_productions; i++)
    {
        if (ps->new_productions[i] == prod && ps->new_core->parent_indexes[i] == parent)
        {
            return;
        }
    }
    OS_TOP_EXPAND(ps->set_productions_os, sizeof(YaepProduction*));
    ps->new_productions = ps->new_core->productions =(YaepProduction**) OS_TOP_BEGIN(ps->set_productions_os);
    OS_TOP_EXPAND(ps->set_parent_indexes_os, sizeof(int));
    ps->new_core->parent_indexes = (int*)OS_TOP_BEGIN(ps->set_parent_indexes_os) - ps->new_num_started_productions;
    ps->new_productions[ps->new_core->num_productions++] = prod;
    ps->new_core->parent_indexes[ps->new_core->n_all_distances++] = parent;
    ps->n_parent_indexes++;
}

/* Add a not-yet-started production (initial) PROD with zero distance at the end of the
   production array of the set being formed.  If this is not-yet-started
   production and there is already the same pair(production, zero distance), we do not add it.*/
static void set_new_add_initial_prod(YaepParseState *ps, YaepProduction*prod)
{
    assert(ps->new_set_ready_p);

    /* When we add not-yet-started productions we need to have pairs
      (production, the corresponding distance) without duplicates
       because we also form core_symb_vect at that time.*/
    for (int i = ps->new_num_started_productions; i < ps->new_core->num_productions; i++)
    {
        // Check if already added.
        if (ps->new_productions[i] == prod) return;
    }
    /* Remember we do not store distance for not-yet-started productions.*/
    OS_TOP_ADD_MEMORY(ps->set_productions_os, &prod, sizeof(YaepProduction*));
    ps->new_productions = ps->new_core->productions = (YaepProduction**)OS_TOP_BEGIN(ps->set_productions_os);
    ps->new_core->num_productions++;
}

/* Set up hash of distances of set S.*/
static void setup_set_distances_hash(hash_table_entry_t s)
{
    YaepStateSet *set = ((YaepStateSet*) s);
    int*dist_ptr = set->distances;
    int n_distances = set->core->num_started_productions;
    int*dist_bound;
    unsigned result;

    dist_bound = dist_ptr + n_distances;
    result = jauquet_prime_mod32;
    while(dist_ptr < dist_bound)
    {
        result = result* hash_shift +*dist_ptr++;
    }
    set->distances_hash = result;
}

/* Set up hash of core of set S.*/
static void setup_set_core_hash(hash_table_entry_t s)
{
    YaepStateSetCore*set_core =((YaepStateSet*) s)->core;

    set_core->hash = productions_hash(set_core->num_started_productions, set_core->productions);
}

/* The new set should contain only start productions.  Sort productions,
   remove duplicates and insert set into the set table.  If the
   function returns TRUE then set contains new set core(there was no
   such core in the table).*/
static int set_insert(YaepParseState *ps)
{
    hash_table_entry_t*entry;
    int result;

    OS_TOP_EXPAND(ps->sets_os, sizeof(YaepStateSet));
    ps->new_set = (YaepStateSet*)OS_TOP_BEGIN(ps->sets_os);
    ps->new_set->distances = ps->new_distances;
    OS_TOP_EXPAND(ps->set_cores_os, sizeof(YaepStateSetCore));
    ps->new_set->core = ps->new_core = (YaepStateSetCore*) OS_TOP_BEGIN(ps->set_cores_os);
    ps->new_core->num_started_productions = ps->new_num_started_productions;
    ps->new_core->productions = ps->new_productions;
    ps->new_set_ready_p = TRUE;
#ifdef USE_SET_HASH_TABLE
    /* Insert distances into table.*/
    setup_set_distances_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_of_distanceses, ps->new_set, TRUE);
    if (*entry != NULL)
    {
        ps->new_distances = ps->new_set->distances =((YaepStateSet*)*entry)->distances;
        OS_TOP_NULLIFY(ps->set_distances_os);
    }
    else
    {
        OS_TOP_FINISH(ps->set_distances_os);
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_set_distances++;
        ps->n_set_distances_len += ps->new_num_started_productions;
    }
#else
    OS_TOP_FINISH(ps->set_distances_os);
    ps->n_set_distances++;
    ps->n_set_distances_len += ps->new_num_started_productions;
#endif
    /* Insert set core into table.*/
    setup_set_core_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_of_cores, ps->new_set, TRUE);
    if (*entry != NULL)
    {
        OS_TOP_NULLIFY(ps->set_cores_os);
        ps->new_set->core = ps->new_core = ((YaepStateSet*)*entry)->core;
        ps->new_productions = ps->new_core->productions;
        OS_TOP_NULLIFY(ps->set_productions_os);
        result = FALSE;
    }
    else
    {
        OS_TOP_FINISH(ps->set_cores_os);
        ps->new_core->core_id = ps->n_set_cores++;
        ps->new_core->num_productions = ps->new_num_started_productions;
        ps->new_core->n_all_distances = ps->new_num_started_productions;
        ps->new_core->parent_indexes = NULL;
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_set_core_start_productions+= ps->new_num_started_productions;
        result = TRUE;
    }
#ifdef USE_SET_HASH_TABLE
    /* Insert set into table.*/
    entry = find_hash_table_entry(ps->set_of_tuples_core_distances, ps->new_set, TRUE);
    if (*entry == NULL)
    {
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_sets++;
        ps->n_sets_start_productions+= ps->new_num_started_productions;
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
    OS_TOP_FINISH(ps->set_productions_os);
    OS_TOP_FINISH(ps->set_parent_indexes_os);
}


/* Finalize work with sets.*/
static void set_fin(YaepParseState *ps)
{
    production_distance_set_fin(ps);
    delete_hash_table(ps->set_of_triplets_core_term_lookahead);
    delete_hash_table(ps->set_of_tuples_core_distances);
    delete_hash_table(ps->set_of_distanceses);
    delete_hash_table(ps->set_of_cores);
    OS_DELETE(ps->triplet_core_term_lookahead_os);
    OS_DELETE(ps->sets_os);
    OS_DELETE(ps->set_parent_indexes_os);
    OS_DELETE(ps->set_productions_os);
    OS_DELETE(ps->set_distances_os);
    OS_DELETE(ps->set_cores_os);
}

/* Initialize work with the parser list.*/
static void pl_init(YaepParseState *ps)
{
    ps->state_sets = NULL;
}

/* The following function creates Earley's parser list.*/
static void pl_create(YaepParseState *ps)
{
    /* Because of error recovery we may have sets 2 times more than tokens.*/
    void *mem = yaep_malloc(ps->run.grammar->alloc, sizeof(YaepStateSet*)*(ps->input_tokens_len + 1)* 2);
    ps->state_sets = (YaepStateSet**)mem;
    ps->state_set_curr = -1;
}

/* Finalize work with the parser list.*/
static void pl_fin(YaepParseState *ps)
{
    if (ps->state_sets != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->state_sets);
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

/* Finalize work with array of vlos.*/
static void vlo_array_fin(YaepParseState *ps)
{
    vlo_t *vlo_ptr;

    for (vlo_ptr = (vlo_t*)VLO_BEGIN(ps->vlo_array); (char*) vlo_ptr < (char*) VLO_BOUND(ps->vlo_array); vlo_ptr++)
    {
        VLO_DELETE(*vlo_ptr);
    }
    VLO_DELETE(ps->vlo_array);
}

#ifdef USE_CORE_SYMB_HASH_TABLE
/* Hash of core_symb_vect.*/
static unsigned core_symb_vect_hash(hash_table_entry_t t)
{
    YaepCoreSymbVect*core_symb_vect =(YaepCoreSymbVect*) t;

    return((jauquet_prime_mod32* hash_shift
            +(size_t)/* was unsigned */core_symb_vect->set_core)* hash_shift
           +(size_t)/* was unsigned */core_symb_vect->symb);
}

/* Equality of core_symb_vects.*/
static int core_symb_vect_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    YaepCoreSymbVect*core_symb_vect1 =(YaepCoreSymbVect*) t1;
    YaepCoreSymbVect*core_symb_vect2 =(YaepCoreSymbVect*) t2;

    return(core_symb_vect1->set_core == core_symb_vect2->set_core
            && core_symb_vect1->symb == core_symb_vect2->symb);
}
#endif

/* Return hash of vector V. */
static unsigned vect_els_hash(YaepVect*v)
{
    unsigned result = jauquet_prime_mod32;
    int i;

    for(i = 0; i < v->len; i++)
        result = result* hash_shift + v->els[i];
    return result;
}

/* Return TRUE if V1 is equal to V2. */
static unsigned vect_els_eq(YaepVect*v1, YaepVect*v2)
{
    int i;
    if (v1->len != v2->len)
        return FALSE;

    for(i = 0; i < v1->len; i++)
        if (v1->els[i] != v2->els[i])
            return FALSE;
    return TRUE;
}

/* Hash of vector transition elements.*/
static unsigned transition_els_hash(hash_table_entry_t t)
{
    return vect_els_hash(&((YaepCoreSymbVect*) t)->transitions);
}

/* Equality of transition vector elements.*/
static int transition_els_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq(&((YaepCoreSymbVect*) t1)->transitions,
                        &((YaepCoreSymbVect*) t2)->transitions);
}

/* Hash of reduce vector elements.*/
static unsigned reduce_els_hash(hash_table_entry_t t)
{
    return vect_els_hash(&((YaepCoreSymbVect*) t)->reduces);
}

/* Equality of reduce vector elements.*/
static int reduce_els_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq(&((YaepCoreSymbVect*) t1)->reduces,
                        &((YaepCoreSymbVect*) t2)->reduces);
}

/* Initialize work with the triples(set core, symbol, vector).*/
static void core_symb_vect_init(YaepParseState *ps)
{
    OS_CREATE(ps->core_symb_vect_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->new_core_symb_vect_vlo, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->vect_els_os, ps->run.grammar->alloc, 0);

    vlo_array_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    ps->map_core_symb_to_vect = create_hash_table(ps->run.grammar->alloc, 3000, core_symb_vect_hash, core_symb_vect_eq);
#else
    VLO_CREATE(ps->core_symb_table_vlo, ps->run.grammar->alloc, 4096);
    ps->core_symb_table = (YaepCoreSymbVect***)VLO_BEGIN(ps->core_symb_table_vlo);
    OS_CREATE(ps->core_symb_tab_rows, ps->run.grammar->alloc, 8192);
#endif

    ps->map_transition_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000, transition_els_hash, transition_els_eq);
    ps->map_reduce_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000, reduce_els_hash, reduce_els_eq);

    ps->n_core_symb_pairs = ps->n_core_symb_vect_len = 0;
    ps->n_transition_vects = ps->n_transition_vect_len = 0;
    ps->n_reduce_vects = ps->n_reduce_vect_len = 0;
}

#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed.*/
static YaepCoreSymbVect **core_symb_vect_addr_get(YaepParseState *ps, YaepCoreSymbVect *triple, int reserv_p)
{
    YaepCoreSymbVect**result;

    if (triple->symb->cached_core_symb_vect != NULL
        && triple->symb->cached_core_symb_vect->set_core == triple->set_core)
    {
        return &triple->symb->cached_core_symb_vect;
    }

    result = ((YaepCoreSymbVect**)find_hash_table_entry(ps->map_core_symb_to_vect, triple, reserv_p));

    triple->symb->cached_core_symb_vect = *result;

    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed.*/
static YaepCoreSymbVect **core_symb_vect_addr_get(YaepParseState *ps, YaepStateSetCore *set_core, YaepSymb *symb)
{
    YaepCoreSymbVect***core_symb_vect_ptr;

    core_symb_vect_ptr = ps->core_symb_table + set_core->core_id;

    if ((char*) core_symb_vect_ptr >=(char*) VLO_BOUND(ps->core_symb_table_vlo))
    {
        YaepCoreSymbVect***ptr,***bound;
        int diff, i;

        diff =((char*) core_symb_vect_ptr
                -(char*) VLO_BOUND(ps->core_symb_table_vlo));
        diff += sizeof(YaepCoreSymbVect**);
        if (diff == sizeof(YaepCoreSymbVect**))
            diff*= 10;

        VLO_EXPAND(ps->core_symb_table_vlo, diff);
        ps->core_symb_table
            =(YaepCoreSymbVect***) VLO_BEGIN(ps->core_symb_table_vlo);
        core_symb_vect_ptr = ps->core_symb_table + set_core->core_id;
        bound =(YaepCoreSymbVect***) VLO_BOUND(ps->core_symb_table_vlo);

        ptr = bound - diff / sizeof(YaepCoreSymbVect**);
        while(ptr < bound)
        {
            OS_TOP_EXPAND(ps->core_symb_tab_rows,
                          (ps->run.grammar->symbs_ptr->num_terms + ps->run.grammar->symbs_ptr->num_nonterms)
                          * sizeof(YaepCoreSymbVect*));
           *ptr =(YaepCoreSymbVect**) OS_TOP_BEGIN(ps->core_symb_tab_rows);
            OS_TOP_FINISH(ps->core_symb_tab_rows);
            for(i = 0; i < ps->run.grammar->symbs_ptr->num_terms + ps->run.grammar->symbs_ptr->num_nonterms; i++)
               (*ptr)[i] = NULL;
            ptr++;
        }
    }
    return &(*core_symb_vect_ptr)[symb->num];
}
#endif

/* The following function returns the triple(if any) for given SET_CORE and SYMB. */
static YaepCoreSymbVect *core_symb_vect_find(YaepParseState *ps, YaepStateSetCore *set_core, YaepSymb *symb)
{
    YaepCoreSymbVect *r;

#ifdef USE_CORE_SYMB_HASH_TABLE
    YaepCoreSymbVect core_symb_vect;

    core_symb_vect.set_core = set_core;
    core_symb_vect.symb = symb;
    r = *core_symb_vect_addr_get(ps, &core_symb_vect, FALSE);
#else
    r = *core_symb_vect_addr_get(ps, set_core, symb);
#endif

    TRACE_FA(ps, "core=%d %s -> %p", set_core->core_id, symb->repr, r);

    return r;
}

/* Add given triple(SET_CORE, TERM, ...) to the table and return
   pointer to it.*/
static YaepCoreSymbVect *core_symb_vect_new(YaepParseState *ps, YaepStateSetCore*set_core, YaepSymb*symb)
{
    YaepCoreSymbVect*triple;
    YaepCoreSymbVect**addr;
    vlo_t*vlo_ptr;

    /* Create table element.*/
    OS_TOP_EXPAND(ps->core_symb_vect_os, sizeof(YaepCoreSymbVect));
    triple =((YaepCoreSymbVect*) OS_TOP_BEGIN(ps->core_symb_vect_os));
    triple->set_core = set_core;
    triple->symb = symb;
    OS_TOP_FINISH(ps->core_symb_vect_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_vect_addr_get(ps, triple, TRUE);
#else
    addr = core_symb_vect_addr_get(ps, set_core, symb);
#endif
    assert(*addr == NULL);
   *addr = triple;

    triple->transitions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->transitions.intern);
    triple->transitions.len = 0;
    triple->transitions.els =(int*) VLO_BEGIN(*vlo_ptr);

    triple->reduces.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->reduces.intern);
    triple->reduces.len = 0;
    triple->reduces.els =(int*) VLO_BEGIN(*vlo_ptr);
    VLO_ADD_MEMORY(ps->new_core_symb_vect_vlo, &triple,
                    sizeof(YaepCoreSymbVect*));
    ps->n_core_symb_pairs++;
    return triple;
}

/* Add EL to vector VEC. */
static void vect_new_add_el(YaepParseState *ps, YaepVect*vec, int el)
{
    vlo_t*vlo_ptr;

    vec->len++;
    vlo_ptr = vlo_array_el(ps, vec->intern);
    VLO_ADD_MEMORY(*vlo_ptr, &el, sizeof(int));
    vec->els =(int*) VLO_BEGIN(*vlo_ptr);
    ps->n_core_symb_vect_len++;
}

/* Add index EL to the transition vector of CORE_SYMB_VECT being formed.*/
static void core_symb_vect_new_add_transition_el(YaepParseState *ps,
                                                 YaepCoreSymbVect *core_symb_vect,
                                                 int el)
{
    vect_new_add_el(ps, &core_symb_vect->transitions, el);
}

/* Add index EL to the reduce vector of CORE_SYMB_VECT being formed.*/
static void core_symb_vect_new_add_reduce_el(YaepParseState *ps,
                                             YaepCoreSymbVect *core_symb_vect,
                                             int el)
{
    vect_new_add_el(ps, &core_symb_vect->reduces, el);
}

/* Insert vector VEC from CORE_SYMB_VECT into table TAB.  Update
   *N_VECTS and INT*N_VECT_LEN if it is a new vector in the table. */
static void process_core_symb_vect_el(YaepParseState *ps,
                                      YaepCoreSymbVect *core_symb_vect,
                                      YaepVect *vec,
                                      hash_table_t *tab,
                                      int *n_vects,
                                      int *n_vect_len)
{
    hash_table_entry_t*entry;

    if (vec->len == 0)
        vec->els = NULL;
    else
    {
        entry = find_hash_table_entry(*tab, core_symb_vect, TRUE);
        if (*entry != NULL)
            vec->els
                =(&core_symb_vect->transitions == vec
                   ?((YaepCoreSymbVect*)*entry)->transitions.els
                   :((YaepCoreSymbVect*)*entry)->reduces.els);
        else
	{
           *entry =(hash_table_entry_t) core_symb_vect;
            OS_TOP_ADD_MEMORY(ps->vect_els_os, vec->els, vec->len* sizeof(int));
            vec->els =(int*) OS_TOP_BEGIN(ps->vect_els_os);
            OS_TOP_FINISH(ps->vect_els_os);
           (*n_vects)++;
           *n_vect_len += vec->len;
	}
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_vect.*/
static void core_symb_vect_new_all_stop(YaepParseState *ps)
{
    YaepCoreSymbVect**triple_ptr;

    for(triple_ptr =(YaepCoreSymbVect**) VLO_BEGIN(ps->new_core_symb_vect_vlo);
        (char*) triple_ptr <(char*) VLO_BOUND(ps->new_core_symb_vect_vlo);
         triple_ptr++)
    {
        process_core_symb_vect_el(ps, *triple_ptr, &(*triple_ptr)->transitions,
                                  &ps->map_transition_to_coresymbvect, &ps->n_transition_vects,
                                  &ps->n_transition_vect_len);
        process_core_symb_vect_el(ps, *triple_ptr, &(*triple_ptr)->reduces,
                                  &ps->map_reduce_to_coresymbvect, &ps->n_reduce_vects,
                                  &ps->n_reduce_vect_len);
    }
    vlo_array_nullify(ps);
    VLO_NULLIFY(ps->new_core_symb_vect_vlo);
}

/* Finalize work with all triples(set core, symbol, vector).*/
static void core_symb_vect_fin(YaepParseState *ps)
{
    delete_hash_table(ps->map_transition_to_coresymbvect);
    delete_hash_table(ps->map_reduce_to_coresymbvect);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table(ps->map_core_symb_to_vect);
#else
    OS_DELETE(ps->core_symb_tab_rows);
    VLO_DELETE(ps->core_symb_table_vlo);
#endif
    vlo_array_fin(ps);
    OS_DELETE(ps->vect_els_os);
    VLO_DELETE(ps->new_core_symb_vect_vlo);
    OS_DELETE(ps->core_symb_vect_os);
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
    grammar->undefined_p = TRUE;
    grammar->error_code = 0;
   *grammar->error_message = '\0';
    grammar->lookahead_level = 1;
    grammar->one_parse_p = 1;
    grammar->cost_p = 0;
    grammar->error_recovery_p = 1;
    grammar->recovery_token_matches = DEFAULT_RECOVERY_TOKEN_MATCHES;
    grammar->symbs_ptr = symb_init(grammar);
    grammar->term_sets_ptr = term_set_init(grammar);
    grammar->rules_ptr = rule_init(grammar);
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
        rule_empty(grammar->rules_ptr);
        term_set_empty(grammar->term_sets_ptr);
        symb_empty(ps, grammar->symbs_ptr);
    }
}

/* The function returns the last occurred error code for given
   grammar.*/
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
    YaepSymb *symb, **rhs, *rhs_symb, *next_rhs_symb;
    YaepRule *rule;
    int changed_p, first_continue_p;
    int i, j, k, rhs_len;

    for (i = 0; (symb = nonterm_get(ps, i)) != NULL; i++)
    {
        symb->u.nonterm.first = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
        term_set_clear(symb->u.nonterm.first, ps->run.grammar->symbs_ptr->num_terms);
        symb->u.nonterm.follow = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
        term_set_clear(symb->u.nonterm.follow, ps->run.grammar->symbs_ptr->num_terms);
    }
    do
    {
        changed_p = 0;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterm.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                first_continue_p = TRUE;
                rhs = rule->rhs;
                rhs_len = rule->rhs_len;
                for(j = 0; j < rhs_len; j++)
                {
                    rhs_symb = rhs[j];
                    if (rhs_symb->term_p)
                    {
                        if (first_continue_p)
                            changed_p |= term_set_up(symb->u.nonterm.first,
                                                     rhs_symb->u.term.term_id,
                                                     ps->run.grammar->symbs_ptr->num_terms);
                    }
                    else
                    {
                        if (first_continue_p)
                            changed_p |= term_set_or(symb->u.nonterm.first,
                                                     rhs_symb->u.nonterm.first,
                                                     ps->run.grammar->symbs_ptr->num_terms);
                        for(k = j + 1; k < rhs_len; k++)
                        {
                            next_rhs_symb = rhs[k];
                            if (next_rhs_symb->term_p)
                                changed_p
                                    |= term_set_up(rhs_symb->u.nonterm.follow,
                                                   next_rhs_symb->u.term.term_id,
                                                   ps->run.grammar->symbs_ptr->num_terms);
                            else
                                changed_p
                                    |= term_set_or(rhs_symb->u.nonterm.follow,
                                                   next_rhs_symb->u.nonterm.first,
                                                   ps->run.grammar->symbs_ptr->num_terms);
                            if (!next_rhs_symb->empty_p)
                                break;
                        }
                        if (k == rhs_len)
                            changed_p |= term_set_or(rhs_symb->u.nonterm.follow,
                                                     symb->u.nonterm.follow,
                                                     ps->run.grammar->symbs_ptr->num_terms);
                    }
                    if (!rhs_symb->empty_p)
                        first_continue_p = FALSE;
                }
            }
    }
    while(changed_p);
}

/* The following function sets up flags empty_p, access_p and
   derivation_p for all grammar symbols.*/
static void set_empty_access_derives(YaepParseState *ps)
{
    YaepSymb*symb,*rhs_symb;
    YaepRule*rule;
    int empty_p, derivation_p;
    int empty_changed_p, derivation_changed_p, accessibility_change_p;
    int i, j;

    for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
    {
        symb->empty_p = 0;
        symb->derivation_p =(symb->term_p ? 1 : 0);
        symb->access_p = 0;
    }
    ps->run.grammar->axiom->access_p = 1;
    do
    {
        empty_changed_p = derivation_changed_p = accessibility_change_p = 0;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterm.rules;
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
    YaepSymb*symb,*lhs;
    YaepRule*rule;
    int i, j, k, loop_p, changed_p;

    /* Initialize accoding to minimal criteria: There is a rule in which
       the nonterminal stands and all the rest symbols can derive empty
       strings.*/
    for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
        for(i = 0; i < rule->rhs_len; i++)
            if (!(symb = rule->rhs[i])->term_p)
            {
                for(j = 0; j < rule->rhs_len; j++)
                    if (i == j)
                        continue;
                    else if (!rule->rhs[j]->empty_p)
                        break;
                if (j >= rule->rhs_len)
                    symb->u.nonterm.loop_p = 1;
            }
    /* Major cycle: Check looped nonterminal that there is a rule with
       the nonterminal in lhs with a looped nonterminal in rhs and all
       the rest rhs symbols deriving empty string.*/
    do
    {
        changed_p = FALSE;
        for(i = 0;(lhs = nonterm_get(ps, i)) != NULL; i++)
            if (lhs->u.nonterm.loop_p)
            {
                loop_p = 0;
                for(rule = lhs->u.nonterm.rules;
                     rule != NULL; rule = rule->lhs_next)
                    for(j = 0; j < rule->rhs_len; j++)
                        if (!(symb = rule->rhs[j])->term_p && symb->u.nonterm.loop_p)
                        {
                            for(k = 0; k < rule->rhs_len; k++)
                                if (j == k)
                                    continue;
                                else if (!rule->rhs[k]->empty_p)
                                    break;
                            if (k >= rule->rhs_len)
                                loop_p = 1;
                        }
                if (!loop_p)
                    changed_p = TRUE;
                lhs->u.nonterm.loop_p = loop_p;
            }
    }
    while(changed_p);
}

/* The following function evaluates different sets and flags for
   grammar and checks the grammar on correctness.*/
static void check_grammar(YaepParseState *ps, int strict_p)
{
    YaepSymb*symb;
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
        if (symb->u.nonterm.loop_p)
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
                      const char*(*read_terminal)(int*code),
                      const char*(*read_rule)(const char***rhs,
                                              const char**abs_node,
                                              int*anode_cost, int**transl, char*mark, char**marks))
{
    const char*name,*lhs,**rhs,*anode;
    YaepSymb*symb,*start;
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
    while((name =(*read_terminal)(&code)) != NULL)
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
        symb_add_term(ps, name, code);
    }

    /* Adding error symbol.*/
    if (symb_find_by_repr(ps, TERM_ERROR_NAME) != NULL)
    {
        yaep_error(ps, YAEP_FIXED_NAME_USAGE, "do not use fixed name `%s'", TERM_ERROR_NAME);
    }

    if (symb_find_by_code(ps, TERM_ERROR_CODE) != NULL) abort();

    ps->run.grammar->term_error = symb_add_term(ps, TERM_ERROR_NAME, TERM_ERROR_CODE);
    ps->run.grammar->term_error_id = ps->run.grammar->term_error->u.term.term_id;
    ps->run.grammar->axiom = ps->run.grammar->end_marker = NULL;

    for (;;)
    {
        lhs = (*read_rule)(&rhs, &anode, &anode_cost, &transl, &mark, &marks);
        if (lhs == NULL) break;

        symb = symb_find_by_repr(ps, lhs);
        if (symb == NULL)
        {
            symb = symb_add_nonterm(ps, lhs);
        }
        else if (symb->term_p)
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
            ps->run.grammar->end_marker = symb_add_term(ps, END_MARKER_NAME, END_MARKER_CODE);

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
    for(rule = start->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
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
        fprintf(stderr, "Rules:\n");
        for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
	{
            fprintf(stderr, "  ");
            rule_print(ps, stderr, rule, TRUE);
	}
        fprintf(stderr, "\n");
        /* Print symbol sets.*/
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
	{
            fprintf(stderr, "Nonterm %s:  Empty=%s , Access=%s, Derive=%s\n",
                     symb->repr,(symb->empty_p ? "Yes" : "No"),
                    (symb->access_p ? "Yes" : "No"),
                    (symb->derivation_p ? "Yes" : "No"));
            if (ps->run.debug)
	    {
                fprintf(stderr, "  First: ");
                term_set_print(ps, stderr, symb->u.nonterm.first, ps->run.grammar->symbs_ptr->num_terms);
                fprintf(stderr, "\n  Follow: ");
                term_set_print(ps, stderr, symb->u.nonterm.follow, ps->run.grammar->symbs_ptr->num_terms);
                fprintf(stderr, "\n\n");
	    }
	}
    }

    ps->run.grammar->undefined_p = FALSE;
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

int yaep_set_one_parse_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->one_parse_p;
    grammar->one_parse_p = flag;
    return old;
}

int yaep_set_cost_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->cost_p;
    grammar->cost_p = flag;
    return old;
}

int yaep_set_error_recovery_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->error_recovery_p;
    grammar->error_recovery_p = flag;
    return old;
}

int yaep_set_recovery_match(YaepGrammar *grammar, int n_input_tokens)
{
    int old;

    assert(grammar != NULL);
    old = grammar->recovery_token_matches;
    grammar->recovery_token_matches = n_input_tokens;
    return old;
}

/* The function initializes all internal data for parser for N_INPUT_TOKENS
   tokens.*/
static void yaep_parse_init(YaepParseState *ps, int n_input_tokens)
{
    YaepRule*rule;

    prod_init(ps);
    set_init(ps, n_input_tokens);
    core_symb_vect_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    {
        int i;
        YaepSymb*symb;

        for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
            symb->cached_core_symb_vect = NULL;
    }
#endif
    for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
        rule->caller_anode = NULL;
}

/* The function should be called the last(it frees all allocated
   data for parser).*/
static void yaep_parse_fin(YaepParseState *ps)
{
    core_symb_vect_fin(ps);
    set_fin(ps);
    prod_fin(ps);
}

/* The following function reads all input tokens.*/
static void read_input_tokens(YaepParseState *ps)
{
    int code;
    void *attr;

    while((code = ps->run.read_token((YaepParseRun*)ps, &attr)) >= 0)
    {
        tok_add(ps, code, attr);
    }
    tok_add(ps, END_MARKER_CODE, NULL);
}

/* The following function add start productions which is formed from
   given start production PROD with distance DIST by reducing symbol
   which can derivate empty string and which is placed after dot in
   given production.  The function returns TRUE if the dot is placed on
   the last position in given production or in the added productions.*/
static void add_derived_nonstart_productions(YaepParseState *ps, YaepProduction*prod, int parent)
{
    YaepSymb*symb;
    YaepRule*rule = prod->rule;
    int context = prod->context;
    int i;

    for(i = prod->dot_i;(symb = rule->rhs[i]) != NULL && symb->empty_p; i++)
    {
        set_add_new_nonstart_prod(ps, prod_create(ps, rule, i + 1, context), parent);
    }
}

/* The following function adds the rest(not-yet-started) productions to the
   new set and and forms triples(set core, symbol, indexes) for
   further fast search of start productions from given core by
   transition on given symbol(see comment for abstract data
   `core_symb_vect').*/
static void expand_new_start_set(YaepParseState *ps)
{
    YaepProduction *prod;
    YaepSymb *symb;
    YaepCoreSymbVect *core_symb_vect;
    YaepRule *rule;
    int i;

    /* Add non start productions with nonzero distances.*/
    for(i = 0; i < ps->new_num_started_productions; i++)
        add_derived_nonstart_productions(ps, ps->new_productions[i], i);
    /* Add non start productions and form transitions vectors.*/
    for(i = 0; i < ps->new_core->num_productions; i++)
    {
        prod = ps->new_productions[i];
        if (prod->dot_i < prod->rule->rhs_len)
	{
            /* There is a symbol after dot in the production.*/
            symb = prod->rule->rhs[prod->dot_i];
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, symb);
            if (core_symb_vect == NULL)
	    {
                core_symb_vect = core_symb_vect_new(ps, ps->new_core, symb);
                if (!symb->term_p)
                {
                    for(rule = symb->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
                    {
                        set_new_add_initial_prod(ps, prod_create(ps, rule, 0, 0));
                    }
                }
	    }
            core_symb_vect_new_add_transition_el(ps, core_symb_vect, i);
            if (symb->empty_p && i >= ps->new_core->n_all_distances)
            {
                set_new_add_initial_prod(ps, prod_create(ps, prod->rule, prod->dot_i + 1, 0));
            }
	}
    }
    /* Now forming reduce vectors.*/
    for(i = 0; i < ps->new_core->num_productions; i++)
    {
        prod = ps->new_productions[i];
        if (prod->dot_i == prod->rule->rhs_len)
	{
            symb = prod->rule->lhs;
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, symb);
            if (core_symb_vect == NULL)
                core_symb_vect = core_symb_vect_new(ps, ps->new_core, symb);
            core_symb_vect_new_add_reduce_el(ps, core_symb_vect, i);
	}
    }
    if (ps->run.grammar->lookahead_level > 1)
    {
        YaepProduction *new_prod, *shifted_prod;
        term_set_el_t *context_set;
        int changed_p, prod_ind, context, j;

        /* Now we have incorrect initial productions because their context is not correct. */
        context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
        do
	{
            changed_p = FALSE;
            for(i = ps->new_core->n_all_distances; i < ps->new_core->num_productions; i++)
	    {
                term_set_clear(context_set, ps->run.grammar->symbs_ptr->num_terms);
                new_prod = ps->new_productions[i];
                core_symb_vect = core_symb_vect_find(ps, ps->new_core, new_prod->rule->lhs);
                for(j = 0; j < core_symb_vect->transitions.len; j++)
		{
                    prod_ind = core_symb_vect->transitions.els[j];
                    prod = ps->new_productions[prod_ind];
                    shifted_prod = prod_create(ps, prod->rule, prod->dot_i + 1,
                                              prod->context);
                    term_set_or(context_set, shifted_prod->lookahead, ps->run.grammar->symbs_ptr->num_terms);
		}
                context = term_set_insert(ps, context_set);
                if (context >= 0)
                {
                    context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
                }
                else
                {
                    context = -context - 1;
                }
                prod = prod_create(ps, new_prod->rule, new_prod->dot_i, context);
                if (prod != new_prod)
		{
                    ps->new_productions[i] = prod;
                    changed_p = TRUE;
		}
	    }
	}
        while(changed_p);
    }
    set_new_core_stop(ps);
    core_symb_vect_new_all_stop(ps);
}

/* The following function forms the 1st set.*/
static void build_start_set(YaepParseState *ps)
{
    int context = 0;

    set_new_start(ps);

    if (ps->run.grammar->lookahead_level > 1)
    {
        term_set_el_t *empty_context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->num_terms);
        term_set_clear(empty_context_set, ps->run.grammar->symbs_ptr->num_terms);
        context = term_set_insert(ps, empty_context_set);

        /* Empty context in the table has always number zero.*/
        assert(context == 0);
    }

    for (YaepRule *rule = ps->run.grammar->axiom->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
    {
        YaepProduction *prod = prod_create(ps, rule, 0, context);
        set_new_add_start_prod(ps, prod, 0);
    }

    if (!set_insert(ps)) assert(FALSE);

    expand_new_start_set(ps);
    ps->state_sets[0] = ps->new_set;
}

/* The following function predicts a new state set by shifting productions
   of SET given in CORE_SYMB_VECT with given lookahead terminal number.
   If the number is negative, we ignore lookahead at all.*/
static void complete_and_predict_new_state_set(YaepParseState *ps,
                                               YaepStateSet *set,
                                               YaepCoreSymbVect *core_symb_vect,
                                               YaepSymb *NEXT_TERM)
{
    YaepStateSet *prev_set;
    YaepStateSetCore *set_core, *prev_set_core;
    YaepProduction *prod, *new_prod, **prev_productions;
    YaepCoreSymbVect *prev_core_symb_vect;
    int local_lookahead_level, dist, prod_ind, new_dist;
    int i, place;
    YaepVect *transitions;

    int lookahead_term_id = NEXT_TERM?NEXT_TERM->u.term.term_id:-1;
    local_lookahead_level = (lookahead_term_id < 0 ? 0 : ps->run.grammar->lookahead_level);
    set_core = set->core;
    set_new_start(ps);
    transitions = &core_symb_vect->transitions;

    clear_production_distance_set(ps);
    for(i = 0; i < transitions->len; i++)
    {
        prod_ind = transitions->els[i];
        prod = set_core->productions[prod_ind];

        new_prod = prod_create(ps, prod->rule, prod->dot_i + 1, prod->context);

        if (local_lookahead_level != 0
            && !term_set_test(new_prod->lookahead, lookahead_term_id, ps->run.grammar->symbs_ptr->num_terms)
            && !term_set_test(new_prod->lookahead, ps->run.grammar->term_error_id, ps->run.grammar->symbs_ptr->num_terms))
        {
            continue;
        }
        dist = 0;
        if (prod_ind >= set_core->n_all_distances)
        {
        }
        else if (prod_ind < set_core->num_started_productions)
        {
            dist = set->distances[prod_ind];
        }
        else
        {
            dist = set->distances[set_core->parent_indexes[prod_ind]];
        }
        dist++;
        if (!production_distance_test_and_set(ps, new_prod, dist))
        {
            // This combo prod+dist did not already exist, lets add it.
            set_new_add_start_prod(ps, new_prod, dist);
        }
    }

    for(i = 0; i < ps->new_num_started_productions; i++)
    {
        new_prod = ps->new_productions[i];
        if (new_prod->empty_tail_p)
	{
            int *curr_el, *bound;

            /* All tail in new sitiation may derivate empty string so
               make reduce and add new productions.*/
            new_dist = ps->new_distances[i];
            place = ps->state_set_curr + 1 - new_dist;
            prev_set = ps->state_sets[place];
            prev_set_core = prev_set->core;
            prev_core_symb_vect = core_symb_vect_find(ps, prev_set_core, new_prod->rule->lhs);
            if (prev_core_symb_vect == NULL)
	    {
                assert(new_prod->rule->lhs == ps->run.grammar->axiom);
                continue;
	    }
            curr_el = prev_core_symb_vect->transitions.els;
            bound = curr_el + prev_core_symb_vect->transitions.len;

            assert(curr_el != NULL);
            prev_productions= prev_set_core->productions;
            do
	    {
                prod_ind = *curr_el++;
                prod = prev_productions[prod_ind];
                new_prod = prod_create(ps, prod->rule, prod->dot_i + 1, prod->context);
                if (local_lookahead_level != 0
                    && !term_set_test(new_prod->lookahead, lookahead_term_id, ps->run.grammar->symbs_ptr->num_terms)
                    && !term_set_test(new_prod->lookahead,
                                      ps->run.grammar->term_error_id,
                                      ps->run.grammar->symbs_ptr->num_terms))
                {
                    continue;
                }
                dist = 0;
                if (prod_ind >= prev_set_core->n_all_distances)
                {
                }
                else if (prod_ind < prev_set_core->num_started_productions)
                {
                    dist = prev_set->distances[prod_ind];
                }
                else
                {
                    dist = prev_set->distances[prev_set_core->parent_indexes[prod_ind]];
                }
                dist += new_dist;

                if (!production_distance_test_and_set(ps, new_prod, dist))
                {
                    // This combo prod+dist did not already exist, lets add it.
                    set_new_add_start_prod(ps, new_prod, dist);
                }
	    }
            while(curr_el < bound);
	}
    }

    if (set_insert(ps))
    {
        expand_new_start_set(ps);
        ps->new_core->term = core_symb_vect->symb;
    }
}

/* This page contains error recovery code.  This code finds minimal
   cost error recovery.  The cost of error recovery is number of
   tokens ignored by error recovery.  The error recovery is successful
   when we match at least RECOVERY_TOKEN_MATCHES tokens.*/

/* The following strucrture describes an error recovery state(an
   error recovery alternative.*/
struct recovery_state
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
/* The following function may be called if you know that state set has
   original sets upto LAST element(including it).  Such call can
   decrease number of restored sets.*/
static void set_original_set_bound(YaepParseState *ps, int last)
{
    assert(last >= 0 && last <= ps->recovery_start_set_curr
            && ps->original_last_state_set_el <= ps->recovery_start_set_curr);
    ps->original_last_state_set_el = last;
}

/* The following function guarantees that original state set tail sets
   starting with state_set_curr(including the state) is saved.  The function
   should be called after any decreasing state_set_curr with subsequent
   writing to state set [state_set_curr]. */
static void save_original_sets(YaepParseState *ps)
{
    int length, curr_pl;

    assert(ps->state_set_curr >= 0 && ps->original_last_state_set_el <= ps->recovery_start_set_curr);
    length = VLO_LENGTH(ps->original_state_set_tail_stack) / sizeof(YaepStateSet*);

    for(curr_pl = ps->recovery_start_set_curr - length; curr_pl >= ps->state_set_curr; curr_pl--)
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
    ps->original_last_state_set_el = ps->state_set_curr - 1;
}

/* If it is necessary, the following function restores original pl
   part with states in range [0, last_state_set_el].*/
static void restore_original_sets(YaepParseState *ps, int last_state_set_el)
{
    assert(last_state_set_el <= ps->recovery_start_set_curr
            && ps->original_last_state_set_el <= ps->recovery_start_set_curr);
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
            [ps->recovery_start_set_curr - ps->original_last_state_set_el];

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
   START_STATE_SET_EL and returns state set element which refers set with production
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
        if (core_symb_vect_find(ps, ps->state_sets[curr_pl]->core, ps->run.grammar->term_error) != NULL)
            break;
        else if (ps->state_sets[curr_pl]->core->term != ps->run.grammar->term_error)
           (*cost)++;
    assert(curr_pl >= 0);
    return curr_pl;
}

/* The following function creates and returns new error recovery state
   with charcteristics(LAST_ORIGINAL_STATE_SET_EL, BACKWARD_MOVE_COST,
   state_set_curr, current_input_token_i).*/
static struct recovery_state new_recovery_state(YaepParseState *ps, int last_original_state_set_el, int backward_move_cost)
{
    struct recovery_state state;
    int i;

    assert(backward_move_cost >= 0);

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Creating recovery state: original set=%d, tok=%d, ",
                last_original_state_set_el, ps->current_input_token_i);
        symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
        fprintf(stderr, "\n");
    }

    state.last_original_state_set_el = last_original_state_set_el;
    state.state_set_tail_length = ps->state_set_curr - last_original_state_set_el;
    assert(state.state_set_tail_length >= 0);
    for(i = last_original_state_set_el + 1; i <= ps->state_set_curr; i++)
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
    state.start_tok = ps->current_input_token_i;
    state.backward_move_cost = backward_move_cost;
    return state;
}

/* The following function creates new error recovery state and pushes
   it on the states stack top. */
static void push_recovery_state(YaepParseState *ps, int last_original_state_set_el, int backward_move_cost)
{
    struct recovery_state state;

    state = new_recovery_state(ps, last_original_state_set_el, backward_move_cost);

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Push recovery state: original set=%d, tok=%d, ",
                 last_original_state_set_el, ps->current_input_token_i);
        symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
        fprintf(stderr, "\n");
    }

    VLO_ADD_MEMORY(ps->recovery_state_stack, &state, sizeof(state));
}

/* The following function sets up parser state(pl, state_set_curr, ps->current_input_token_i)
   according to error recovery STATE. */
static void set_recovery_state(YaepParseState *ps, struct recovery_state*state)
{
    int i;

    ps->current_input_token_i = state->start_tok;
    restore_original_sets(ps, state->last_original_state_set_el);
    ps->state_set_curr = state->last_original_state_set_el;

    if (ps->run.debug)
    {
        fprintf(stderr, "++++Set recovery state: set=%d, tok=%d, ",
                 ps->state_set_curr, ps->current_input_token_i);
        symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
        fprintf(stderr, "\n");
    }

    for(i = 0; i < state->state_set_tail_length; i++)
    {
        ps->state_sets[++ps->state_set_curr] = state->state_set_tail[i];

        if (ps->run.debug)
	{
            fprintf(stderr, "++++++Add saved set=%d\n", ps->state_set_curr);
            print_state_set(ps, stderr, ps->state_sets[ps->state_set_curr], ps->state_set_curr, ps->run.debug,
                      ps->run.debug);
            fprintf(stderr, "\n");
	}

    }
}

/* The following function pops the top error recovery state from
   states stack.  The current parser state will be setup according to
   the state. */
static struct recovery_state pop_recovery_state(YaepParseState *ps)
{
    struct recovery_state *state;

    state = &((struct recovery_state*) VLO_BOUND(ps->recovery_state_stack))[-1];
    VLO_SHORTEN(ps->recovery_state_stack, sizeof(struct recovery_state));

    if (ps->run.debug)
        fprintf(stderr, "++++Pop error recovery state\n");

    set_recovery_state(ps, state);
    return*state;
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
    YaepCoreSymbVect*core_symb_vect;
    struct recovery_state best_state, state;
    int best_cost, cost, n_matched_input_tokens;
    int back_to_frontier_move_cost, backward_move_cost;


    if (ps->run.verbose)
        fprintf(stderr, "\n++Error recovery start\n");

   *stop =*start = -1;
    OS_CREATE(ps->recovery_state_tail_sets, ps->run.grammar->alloc, 0);
    VLO_NULLIFY(ps->original_state_set_tail_stack);
    VLO_NULLIFY(ps->recovery_state_stack);
    ps->recovery_start_set_curr = ps->state_set_curr;
    ps->recovery_start_current_input_token_i = ps->current_input_token_i;
    /* Initialize error recovery state stack.*/
    ps->state_set_curr
        = ps->back_state_set_frontier = find_error_state_set_set(ps, ps->state_set_curr, &backward_move_cost);
    back_to_frontier_move_cost = backward_move_cost;
    save_original_sets(ps);
    push_recovery_state(ps, ps->back_state_set_frontier, backward_move_cost);
    best_cost = 2* ps->input_tokens_len;
    while(VLO_LENGTH(ps->recovery_state_stack) > 0)
    {
        state = pop_recovery_state(ps);
        cost = state.backward_move_cost;
        assert(cost >= 0);
        /* Advance back frontier.*/
        if (ps->back_state_set_frontier > 0)
	{
            int saved_state_set_curr = ps->state_set_curr;
            int saved_current_input_token_i = ps->current_input_token_i;

            /* Advance back frontier.*/
            ps->state_set_curr = find_error_state_set_set(ps, ps->back_state_set_frontier - 1,
                                         &backward_move_cost);

            if (ps->run.debug)
                fprintf(stderr, "++++Advance back frontier: old=%d, new=%d\n",
                         ps->back_state_set_frontier, ps->state_set_curr);

            if (best_cost >= back_to_frontier_move_cost + backward_move_cost)
	    {
                ps->back_state_set_frontier = ps->state_set_curr;
                ps->current_input_token_i = ps->recovery_start_current_input_token_i;
                save_original_sets(ps);
                back_to_frontier_move_cost += backward_move_cost;
                push_recovery_state(ps, ps->back_state_set_frontier,
                                    back_to_frontier_move_cost);
                set_original_set_bound(ps, state.last_original_state_set_el);
                ps->current_input_token_i = saved_current_input_token_i;
	    }
            ps->state_set_curr = saved_state_set_curr;
	}
        /* Advance head frontier.*/
        if (best_cost >= cost + 1)
	{
            ps->current_input_token_i++;
            if (ps->current_input_token_i < ps->input_tokens_len)
	    {

                if (ps->run.debug)
		{
                    fprintf(stderr,
                             "++++Advance head frontier(one pos): tok=%d, ",
                             ps->current_input_token_i);
                    symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
                    fprintf(stderr, "\n");

		}
                push_recovery_state(ps, state.last_original_state_set_el, cost + 1);
	    }
            ps->current_input_token_i--;
	}
        set = ps->state_sets[ps->state_set_curr];

        if (ps->run.debug)
	{
            fprintf(stderr, "++++Trying set=%d, tok=%d, ", ps->state_set_curr, ps->current_input_token_i);
            symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
            fprintf(stderr, "\n");
	}

        /* Shift error:*/
        core_symb_vect = core_symb_vect_find(ps, set->core, ps->run.grammar->term_error);
        assert(core_symb_vect != NULL);

        if (ps->run.debug)
            fprintf(stderr, "++++Making error shift in set=%d\n", ps->state_set_curr);

        complete_and_predict_new_state_set(ps, set, core_symb_vect, NULL);
        ps->state_sets[++ps->state_set_curr] = ps->new_set;

        if (ps->run.debug)
	{
            fprintf(stderr, "++Trying new set=%d\n", ps->state_set_curr);
            print_state_set(ps, stderr, ps->new_set, ps->state_set_curr, ps->run.debug, ps->run.debug);
            fprintf(stderr, "\n");
	}

        /* Search the first right token.*/
        while(ps->current_input_token_i < ps->input_tokens_len)
	{
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, ps->input_tokens[ps->current_input_token_i].symb);
            if (core_symb_vect != NULL)
                break;

            if (ps->run.debug)
	    {
                fprintf(stderr, "++++++Skipping=%d ", ps->current_input_token_i);
                symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
                fprintf(stderr, "\n");
	    }

            cost++;
            ps->current_input_token_i++;
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
        if (ps->current_input_token_i >= ps->input_tokens_len)
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
        YaepSymb *NEXT_TERM = NULL;
        if (ps->current_input_token_i + 1 < ps->input_tokens_len)
        {
            NEXT_TERM = ps->input_tokens[ps->current_input_token_i + 1].symb;
        }
        complete_and_predict_new_state_set(ps, ps->new_set, core_symb_vect, NEXT_TERM);
        ps->state_sets[++ps->state_set_curr] = ps->new_set;

        if (ps->run.debug)
	{
            fprintf(stderr, "++++++++Building new set=%d\n", ps->state_set_curr);
            if (ps->run.debug)
            {
                print_state_set(ps, stderr, ps->new_set, ps->state_set_curr, ps->run.debug, ps->run.debug);
            }
	}

        n_matched_input_tokens = 0;
        for(;;)
	{

            if (ps->run.debug)
	    {
                fprintf(stderr, "++++++Matching=%d ", ps->current_input_token_i);
                symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
                fprintf(stderr, "\n");
	    }

            n_matched_input_tokens++;
            if (n_matched_input_tokens >= ps->run.grammar->recovery_token_matches)
            {
                break;
            }
            ps->current_input_token_i++;
            if (ps->current_input_token_i >= ps->input_tokens_len)
            {
                break;
            }
            /* Push secondary recovery state(with error in set).*/
            if (core_symb_vect_find(ps, ps->new_core, ps->run.grammar->term_error) != NULL)
	    {
                if (ps->run.debug)
		{
                    fprintf(stderr, "++++Found secondary state: original set=%d, tok=%d, ",
                            state.last_original_state_set_el, ps->current_input_token_i);
                    symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
                    fprintf(stderr, "\n");
		}

                push_recovery_state(ps, state.last_original_state_set_el, cost);
	    }
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, ps->input_tokens[ps->current_input_token_i].symb);
            if (core_symb_vect == NULL)
            {
                break;
            }
            YaepSymb *NEXT_TERM = NULL;
            if (ps->current_input_token_i + 1 < ps->input_tokens_len)
            {
                NEXT_TERM = ps->input_tokens[ps->current_input_token_i + 1].symb;
            }
            complete_and_predict_new_state_set(ps, ps->new_set, core_symb_vect, NEXT_TERM);
            ps->state_sets[++ps->state_set_curr] = ps->new_set;
	}
        if (n_matched_input_tokens >= ps->run.grammar->recovery_token_matches || ps->current_input_token_i >= ps->input_tokens_len)
	{
            /* We found an error recovery.  Compare costs.*/
            if (best_cost > cost)
	    {

                if (ps->run.debug)
                {
                    fprintf(stderr, "++++Ignore %d tokens(the best recovery now): Save it:\n", cost);
                }
                best_cost = cost;
                if (ps->current_input_token_i == ps->input_tokens_len)
                {
                    ps->current_input_token_i--;
                }
                best_state = new_recovery_state(ps, state.last_original_state_set_el,
                                                 /* It may be any constant here
                                                    because it is not used.*/
                                                 0);
               *start = ps->recovery_start_current_input_token_i - state.backward_move_cost;
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
        fprintf(stderr, "\n++Error recovery end: curr token %d=", ps->current_input_token_i);
        symb_print(stderr, ps->input_tokens[ps->current_input_token_i].symb, TRUE);
        fprintf(stderr, ", Current set=%d:\n", ps->state_set_curr);
        if (ps->run.debug)
        {
            print_state_set(ps, stderr, ps->state_sets[ps->state_set_curr],
                            ps->state_set_curr, ps->run.debug, ps->run.debug);
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
static void error_recovery_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->recovery_state_stack);
    VLO_DELETE(ps->original_state_set_tail_stack);
}

/* Return TRUE if goto set SET from parsing list PLACE can be used as
   the next set.  The criterium is that all origin sets of start
   productions are the same as from PLACE. */
static int check_cached_transition_set(YaepParseState *ps, YaepStateSet*set, int place)
{
    int i, dist;
    int*distances = set->distances;

    for(i = set->core->num_started_productions - 1; i >= 0; i--)
    {
        if ((dist = distances[i]) <= 1)
            continue;
        /* Sets at origins of productions with distance one are supposed
           to be the same. */
        if (ps->state_sets[ps->state_set_curr + 1 - dist] != ps->state_sets[place + 1 - dist])
            return FALSE;
    }
    return TRUE;
}

static int try_to_recover(YaepParseState *ps)
{
    int saved_current_input_token_i, start, stop;

    /* Error recovery.  We do not check transition vector
       because for terminal transition vector is never NULL
       and reduce is always NULL. */

    saved_current_input_token_i = ps->current_input_token_i;
    if (ps->run.grammar->error_recovery_p)
    {
        fprintf(stderr, "Attempting error recovery...\n");
        error_recovery(ps, &start, &stop);
        ps->run.syntax_error(saved_current_input_token_i, ps->input_tokens[saved_current_input_token_i].attr,
                             start, ps->input_tokens[start].attr, stop,
                             ps->input_tokens[stop].attr);
        return 1;
    }
    else
    {
        ps->run.syntax_error(saved_current_input_token_i, ps->input_tokens[saved_current_input_token_i].attr, -1, NULL, -1, NULL);
        return 2;
    }

    return 0;
}

static YaepStateSetTermLookAhead *lookup_cached_set(YaepParseState *ps,
                                                    YaepSymb *THE_TERM,
                                                    YaepSymb *NEXT_TERM,
                                                    YaepStateSet *set)
{
    int i;
    hash_table_entry_t *entry;
    YaepStateSetTermLookAhead *new_core_term_lookahead;

    OS_TOP_EXPAND(ps->triplet_core_term_lookahead_os, sizeof(YaepStateSetTermLookAhead));

    new_core_term_lookahead = (YaepStateSetTermLookAhead*) OS_TOP_BEGIN(ps->triplet_core_term_lookahead_os);
    new_core_term_lookahead->set = set;
    new_core_term_lookahead->term = THE_TERM;
    new_core_term_lookahead->lookahead = NEXT_TERM?NEXT_TERM->u.term.term_id:-1;

    for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
    {
        new_core_term_lookahead->result[i] = NULL;
    }
    new_core_term_lookahead->curr = 0;
    entry = find_hash_table_entry(ps->set_of_triplets_core_term_lookahead, new_core_term_lookahead, TRUE);

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
static void save_cached_set(YaepParseState *ps, YaepStateSetTermLookAhead *entry, YaepSymb *NEXT_TERM)
{
    int i = entry->curr;
    entry->result[i] = ps->new_set;
    entry->place[i] = ps->state_set_curr;
    entry->lookahead = NEXT_TERM ? NEXT_TERM->u.term.term_id : -1;
    entry->curr = (i + 1) % MAX_CACHED_GOTO_RESULTS;
}

/* The following function is major function forming parsing list in Earley's algorithm.*/
static void perform_parse(YaepParseState *ps)
{
    error_recovery_init(ps);
    build_start_set(ps);

    if (ps->run.debug)
    {
        fprintf(stderr, "\n\n------ Parsing start ---------------\n\n");
        print_state_set(ps, stderr, ps->new_set, 0, ps->run.debug, ps->run.debug);
    }

    ps->current_input_token_i = 0;
    ps->state_set_curr = 0;

    for(; ps->current_input_token_i < ps->input_tokens_len; ps->current_input_token_i++)
    {
        assert(ps->state_set_curr == ps->current_input_token_i);
        YaepSymb *THE_TERM = ps->input_tokens[ps->current_input_token_i].symb;
        YaepSymb *NEXT_TERM = NULL;

        if (ps->run.grammar->lookahead_level != 0 && ps->current_input_token_i < ps->input_tokens_len-1)
        {
            NEXT_TERM = ps->input_tokens[ps->current_input_token_i + 1].symb;
        }

        if (ps->run.debug)
	{
            fprintf(stderr, "\nScan input_tokens[%d]= ", ps->current_input_token_i);
            symb_print(stderr, THE_TERM, TRUE);
            fprintf(stderr, " state_set_curr=%d\n", ps->state_set_curr);
	}

        YaepStateSet *set = ps->state_sets[ps->state_set_curr];
        ps->new_set = NULL;

#ifdef USE_SET_HASH_TABLE
        YaepStateSetTermLookAhead *entry = lookup_cached_set(ps, THE_TERM, NEXT_TERM, set);
#endif

        if (ps->new_set == NULL)
	{
            YaepCoreSymbVect *core_symb_vect = core_symb_vect_find(ps, set->core, THE_TERM);

            if (core_symb_vect == NULL)
	    {
                int c = try_to_recover(ps);
                if (c == 1) continue;
                else if (c == 2) break;
	    }

            complete_and_predict_new_state_set(ps, set, core_symb_vect, NEXT_TERM);

#ifdef USE_SET_HASH_TABLE
            save_cached_set(ps, entry, NEXT_TERM);
#endif
	}

        ps->state_set_curr++;
        ps->state_sets[ps->state_set_curr] = ps->new_set;

        if (ps->run.debug)
	{
            print_state_set(ps, stderr, ps->new_set, ps->state_set_curr, ps->run.debug, ps->run.debug);
	}
    }
    error_recovery_fin(ps);

    if (ps->run.debug)
    {
        fprintf(stderr, "\n\n----- Parsing done -----------------\n\n\n");
    }
}

/* Hash of parse state.*/
static unsigned parse_state_hash(hash_table_entry_t s)
{
    YaepInternalParseState*state =((YaepInternalParseState*) s);

    /* The table contains only states with dot at the end of rule.*/
    assert(state->dot_i == state->rule->rhs_len);
    return(((jauquet_prime_mod32* hash_shift +
             (unsigned)(size_t) state->rule)* hash_shift +
             state->origin_i)* hash_shift + state->current_state_set_i);
}

/* Equality of parse states.*/
static int parse_state_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepInternalParseState*state1 =((YaepInternalParseState*) s1);
    YaepInternalParseState*state2 =((YaepInternalParseState*) s2);

    /* The table contains only states with dot at the end of rule.*/
    assert(state1->dot_i == state1->rule->rhs_len
            && state2->dot_i == state2->rule->rhs_len);
    return(state1->rule == state2->rule && state1->origin_i == state2->origin_i
            && state1->current_state_set_i == state2->current_state_set_i);
}

/* The following function initializes work with parser states.*/
static void parse_state_init(YaepParseState *ps)
{
    ps->free_parse_state = NULL;
    OS_CREATE(ps->parse_state_os, ps->run.grammar->alloc, 0);
    if (!ps->run.grammar->one_parse_p)
        ps->map_rule_orig_statesetind_to_internalstate =
            create_hash_table(ps->run.grammar->alloc, ps->input_tokens_len* 2, parse_state_hash,
                               parse_state_eq);
}

/* The following function returns new parser state.*/
static YaepInternalParseState *parse_state_alloc(YaepParseState *ps)
{
    YaepInternalParseState*result;

    if (ps->free_parse_state == NULL)
    {
        OS_TOP_EXPAND(ps->parse_state_os, sizeof(YaepInternalParseState));
        result =(YaepInternalParseState*) OS_TOP_BEGIN(ps->parse_state_os);
        OS_TOP_FINISH(ps->parse_state_os);
    }
    else
    {
        result = ps->free_parse_state;
        ps->free_parse_state =(YaepInternalParseState*) ps->free_parse_state->rule;
    }
    return result;
}

/* The following function frees STATE.*/
static void parse_state_free(YaepParseState *ps, YaepInternalParseState*state)
{
    state->rule = (YaepRule*)ps->free_parse_state;
    ps->free_parse_state = state;
}

/* The following function searches for state in the table with the
   same characteristics as*STATE.  If it found it, it returns pointer
   to the state in the table.  Otherwise the function makes copy of
  *STATE, inserts into the table and returns pointer to copied state.
   In the last case, the function also sets up*NEW_P.*/
static YaepInternalParseState *parse_state_insert(YaepParseState *ps, YaepInternalParseState *state, int *new_p)
{
    hash_table_entry_t*entry;

    entry = find_hash_table_entry(ps->map_rule_orig_statesetind_to_internalstate, state, TRUE);

   *new_p = FALSE;
    if (*entry != NULL)
        return(YaepInternalParseState*)*entry;
   *new_p = TRUE;
    /* We make copy because current_state_set_i can be changed in further processing state.*/
   *entry = parse_state_alloc(ps);
   *(YaepInternalParseState*)*entry =*state;
    return(YaepInternalParseState*)*entry;
}

/* The following function finalizes work with parser states.*/
static void parse_state_fin(YaepParseState *ps)
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
static int trans_visit_node_eq(hash_table_entry_t n1, hash_table_entry_t n2)
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
                                   &trans_visit_node, TRUE);

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
        return;
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
            fprintf(f, "TERMINAL: code=%d, repr=%s, mark=%d %c\n", node->val.term.code,
                    symb_find_by_code(ps, node->val.term.code)->repr, node->val.term.mark, node->val.term.mark>32?node->val.term.mark:' ');
        break;
    case YAEP_ANODE:
        if (ps->run.debug)
	{
            fprintf(f, "ABSTRACT: %c%s(", node->val.anode.mark?node->val.anode.mark:' ', node->val.anode.name);
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
                fprintf(f, " %d", canon_node_id(visit_node(ps, child)->num));
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
                            symb_find_by_code(ps, child->val.term.code)->repr);
                    break;
		case YAEP_ANODE:
                    fprintf(f, "%s", child->val.anode.name);
                    break;
		case YAEP_ALT:
                    fprintf(f, "ALT");
                    break;
		default:
                    assert(FALSE);
		}
                fprintf(f, "\";\n");
	    }
	}
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
            print_yaep_node(ps, f, child);
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
                        symb_find_by_code(ps, node->val.alt.node->val.term.code)->
                         repr);
                break;
	    case YAEP_ANODE:
                fprintf(f, "%s", node->val.alt.node->val.anode.name);
                break;
	    case YAEP_ALT:
                fprintf(f, "ALT");
                break;
	    default:
                assert(FALSE);
	    }
            fprintf(f, "\";\n");
            if (node->val.alt.next != NULL)
                fprintf(f, "  \"%d: ALT\" -> \"%d: ALT\";\n",
                        trans_visit_node->num,
                        canon_node_id(visit_node(ps, node->val.alt.next)->num));
	}
        print_yaep_node(ps, f, node->val.alt.node);
        if (node->val.alt.next != NULL)
            print_yaep_node(ps, f, node->val.alt.next);
        break;
    default:
        assert(FALSE);
    }
}

/* The following function prints parse tree with ROOT.*/
static void print_parse(YaepParseState *ps, FILE* f, YaepTreeNode*root)
{
    ps->map_node_to_visit =
        create_hash_table(ps->run.grammar->alloc, ps->input_tokens_len* 2, trans_visit_node_hash,
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
static int reserv_mem_eq(hash_table_entry_t m1, hash_table_entry_t m2)
{
    return m1 == m2;
}

/* The following function sets up minimal cost for each abstract node.
   The function returns minimal translation corresponding to NODE.
   The function also collects references to memory which can be
   freed. Remeber that the translation is DAG, altenatives form lists
  (alt node may not refer for another alternative).*/
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
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
	    {
                node->val.anode.children[i] = prune_to_minimal(ps, child, cost);
                node->val.anode.cost +=*cost;
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
            if (alt == node || min_cost >*cost)
	    {
                min_cost =*cost;
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
        assert(FALSE);
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
    if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->set_of_reserved_memory, node, TRUE)) == NULL)
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
                                                                      TRUE)) == NULL)
        {
            *entry =(hash_table_entry_t) node->val.anode.name;
        }
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            traverse_pruned_translation(ps, child);
        }
        assert(node->val.anode.cost < 0);
        node->val.anode.cost = -node->val.anode.cost - 1;
        break;
    case YAEP_ALT:
        traverse_pruned_translation(ps, node->val.alt.node);
        if ((node = node->val.alt.next) != NULL)
            goto next;
        break;
    default:
        assert(FALSE);
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
        ps->set_of_reserved_memory = create_hash_table(ps->run.grammar->alloc, ps->input_tokens_len* 4, reserv_mem_hash,
                                           reserv_mem_eq);

        VLO_CREATE(ps->tnodes_vlo, ps->run.grammar->alloc, ps->input_tokens_len* 4* sizeof(void*));
    }
    root = prune_to_minimal(ps, root, &cost);
    traverse_pruned_translation(ps, root);
    if (ps->run.parse_free != NULL)
    {
        for(node_ptr =(YaepTreeNode**) VLO_BEGIN(ps->tnodes_vlo);
             node_ptr <(YaepTreeNode**) VLO_BOUND(ps->tnodes_vlo);
             node_ptr++)
            if (*find_hash_table_entry(ps->set_of_reserved_memory,*node_ptr, TRUE) == NULL)
            {
                if ((*node_ptr)->type == YAEP_ANODE
                    &&*find_hash_table_entry(ps->set_of_reserved_memory,
					      (*node_ptr)->val.anode.name,
					       TRUE) == NULL)
                {
                   (*ps->run.parse_free)((void*)(*node_ptr)->val.anode.name);
                }
               (*ps->run.parse_free)(*node_ptr);
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
   alternatives).*/
static YaepTreeNode *build_parse_tree(YaepParseState *ps, int *ambiguous_p)
{
    YaepStateSet *set, *check_set;
    YaepStateSetCore *set_core, *check_set_core;
    YaepProduction *prod, *check_prod;
    YaepRule *rule, *prod_rule;
    YaepSymb *symb;
    YaepCoreSymbVect *core_symb_vect, *check_core_symb_vect;
    int i, j, k, found, pos, origin, current_state_set_i, n_candidates, disp;
    int prod_ind, check_prod_ind, prod_origin, check_prod_origin, new_p;
    YaepInternalParseState *state, *orig_state, *curr_state;
    YaepInternalParseState *table_state, *parent_anode_state;
    YaepInternalParseState root_state;
    YaepTreeNode *result, *empty_node, *node, *error_node;
    YaepTreeNode *parent_anode, *anode, root_anode;
    int parent_disp;
    int saved_one_parse_p;
    YaepTreeNode **term_node_array = NULL;
    vlo_t stack, orig_states;

    ps->n_parse_term_nodes = ps->n_parse_abstract_nodes = ps->n_parse_alt_nodes = 0;
    set = ps->state_sets[ps->state_set_curr];
    assert(ps->run.grammar->axiom != NULL);
    /* We have only one start production: "$S : <start symb> $eof .". */
    prod =(set->core->productions != NULL ? set->core->productions[0] : NULL);
    if (prod == NULL
        || set->distances[0] != ps->state_set_curr
        || prod->rule->lhs != ps->run.grammar->axiom || prod->dot_i != prod->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'.*/
        assert(!ps->run.grammar->error_recovery_p);
        return NULL;
    }
    saved_one_parse_p = ps->run.grammar->one_parse_p;
    if (ps->run.grammar->cost_p)
        /* We need all parses to choose the minimal one*/
        ps->run.grammar->one_parse_p = FALSE;
    prod = set->core->productions[0];
    parse_state_init(ps);
    if (!ps->run.grammar->one_parse_p)
    {
        void*mem;

        /* We need this array to reuse terminal nodes only for
           generation of several parses.*/
        mem = yaep_malloc(ps->run.grammar->alloc,
                          sizeof(YaepTreeNode*)* ps->input_tokens_len);
        term_node_array =(YaepTreeNode**) mem;
        for(i = 0; i < ps->input_tokens_len; i++)
        {
            term_node_array[i] = NULL;
        }
        /* The following is used to check necessity to create current
           state with different current_state_set_i.*/
        VLO_CREATE(orig_states, ps->run.grammar->alloc, 0);
    }
    VLO_CREATE(stack, ps->run.grammar->alloc, 10000);
    VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
    state = parse_state_alloc(ps);
   ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
    rule = state->rule = prod->rule;
    state->dot_i = prod->dot_i;
    state->origin_i = 0;
    state->current_state_set_i = ps->state_set_curr;
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
    while(VLO_LENGTH(stack) != 0)
    {
        if (ps->run.debug && state->dot_i == state->rule->rhs_len)
	{
            fprintf(stderr, "\n\nProcessing top %ld, current_state_set_i = %d, prod = ",
                    (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                     state->current_state_set_i);
            print_rule_with_dot(ps, stderr, state->rule, state->dot_i);
            fprintf(stderr, ", state->origin_i=%d\n", state->origin_i);
	}

        pos = --state->dot_i;
        rule = state->rule;
        parent_anode_state = state->parent_anode_state;
        parent_anode = parent_anode_state->anode;
        parent_disp = state->parent_disp;
        anode = state->anode;
        disp = rule->order[pos];
        current_state_set_i = state->current_state_set_i;
        origin = state->origin_i;
        if (pos < 0)
	{
            /* We've processed all rhs of the rule.*/

            if (ps->run.debug && state->dot_i == state->rule->rhs_len)
	    {
                fprintf(stderr, "Poping top %ld, current_state_set_i = %d, prod = ",
                        (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                        state->current_state_set_i);

                print_rule_with_dot(ps, stderr, state->rule, 0);

                fprintf(stderr, ", state->origin_i = %d\n", state->origin_i);
	    }

            parse_state_free(ps, state);
            VLO_SHORTEN(stack, sizeof(YaepInternalParseState*));
            if (VLO_LENGTH(stack) != 0)
                state =((YaepInternalParseState**) VLO_BOUND(stack))[-1];
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
	    {
                /* We do produce nothing but we should.  So write empty
                   node.*/
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
        if ((symb = rule->rhs[pos])->term_p)
	{
            /* Terminal before dot:*/
            current_state_set_i--;		/* l*/
            /* Because of error recovery input_tokens [current_state_set_i].symb may be not equal to symb.*/
            //assert(ps->input_tokens[current_state_set_i].symb == symb);
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
                         &&(node = term_node_array[current_state_set_i]) != NULL)
                    ;
                else
		{
                    ps->n_parse_term_nodes++;
                    node =((YaepTreeNode*)
                           (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
                    node->type = YAEP_TERM;
                    node->val.term.code = symb->u.term.code;
                    // IXML
                    if (rule->marks && rule->marks[pos])
                    {
                        // Copy the mark from the rhs position on to the terminal.
                        node->val.term.mark = rule->marks[pos];
                    }
                    node->val.term.attr = ps->input_tokens[current_state_set_i].attr;
                    if (!ps->run.grammar->one_parse_p)
                        term_node_array[current_state_set_i] = node;
		}
                place_translation(ps,
                                  anode != NULL ?
                                  anode->val.anode.children + disp
                                  : parent_anode->val.anode.children + parent_disp, node);
	    }
            if (pos != 0)
                state->current_state_set_i = current_state_set_i;
            continue;
	}
        /* Nonterminal before dot:*/
        set = ps->state_sets[current_state_set_i];
        set_core = set->core;
        core_symb_vect = core_symb_vect_find(ps, set_core, symb);
        assert(core_symb_vect->reduces.len != 0);
        n_candidates = 0;
        orig_state = state;
        if (!ps->run.grammar->one_parse_p)
        {
            VLO_NULLIFY(orig_states);
        }
        for(i = 0; i < core_symb_vect->reduces.len; i++)
	{
            prod_ind = core_symb_vect->reduces.els[i];
            prod = set_core->productions[prod_ind];
            if (prod_ind < set_core->num_started_productions)
            {
                fprintf(stderr, "PRUTT current_state_set_i %d set->distances[prod_ind] = %d prod_ind = %d\n",
                        current_state_set_i, set->distances[prod_ind], prod_ind);
                prod_origin = current_state_set_i - set->distances[prod_ind];
            }
            else if (prod_ind < set_core->n_all_distances)
            {
                fprintf(stderr, "BAJS\n");
                prod_origin = current_state_set_i - set->distances[set_core->parent_indexes[prod_ind]];
            }
            else
            {
                fprintf(stderr, "KISS\n");
                prod_origin = current_state_set_i;
            }

            if (ps->run.debug)
	    {
                fprintf(stderr, "    Trying current_state_set_i = %d, prod = ", current_state_set_i);
                print_production(ps, stderr, prod, ps->run.debug, -1);
                fprintf(stderr, ", prod_origin = %d\n", prod_origin);
	    }

            check_set = ps->state_sets[prod_origin];
            check_set_core = check_set->core;
            check_core_symb_vect = core_symb_vect_find(ps, check_set_core, symb);
            assert(check_core_symb_vect != NULL);
            found = FALSE;
            for(j = 0; j < check_core_symb_vect->transitions.len; j++)
	    {
                check_prod_ind = check_core_symb_vect->transitions.els[j];
                check_prod = check_set->core->productions[check_prod_ind];
                if (check_prod->rule != rule || check_prod->dot_i != pos)
                    continue;
                check_prod_origin = prod_origin;
                if (check_prod_ind < check_set_core->n_all_distances)
		{
                    if (check_prod_ind < check_set_core->num_started_productions)
                        check_prod_origin
                            = prod_origin - check_set->distances[check_prod_ind];
                    else
                        check_prod_origin
                            =(prod_origin
                               - check_set->distances[check_set_core->parent_indexes
                                                  [check_prod_ind]]);
		}
                if (check_prod_origin == origin)
		{
                    found = TRUE;
                    break;
		}
	    }
            if (!found)
                continue;
            if (n_candidates != 0)
	    {
               *ambiguous_p = TRUE;
                if (ps->run.grammar->one_parse_p)
                    break;
	    }
            prod_rule = prod->rule;
            if (n_candidates == 0)
                orig_state->current_state_set_i = prod_origin;
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   nonterminal.*/
                curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule.*/
                if (n_candidates != 0)
		{
                    assert(!ps->run.grammar->one_parse_p);
                    if (n_candidates == 1)
		    {
                        VLO_EXPAND(orig_states, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(orig_states))[-1]
                            = orig_state;
		    }
                    for(j =(VLO_LENGTH(orig_states)
                              / sizeof(YaepInternalParseState*) - 1); j >= 0; j--)
                        if (((YaepInternalParseState**)
                             VLO_BEGIN(orig_states))[j]->current_state_set_i == prod_origin)
                            break;
                    if (j >= 0)
		    {
                        /* [A -> x., n] & [A -> y., n]*/
                        curr_state =((YaepInternalParseState**)
                                      VLO_BEGIN(orig_states))[j];
                        anode = curr_state->anode;
		    }
                    else
		    {
                        /* [A -> x., n] & [A -> y., m] where n != m.*/
                        /* It is different from the previous ones so add
                           it to process.*/
                        state = parse_state_alloc(ps);
                        VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
                       *state =*orig_state;
                        state->current_state_set_i = prod_origin;
                        if (anode != NULL)
                            state->anode
                                = copy_anode(ps, parent_anode->val.anode.children
                                              + parent_disp, anode, rule, disp);
                        VLO_EXPAND(orig_states, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(orig_states))[-1]
                            = state;

                        if (ps->run.debug)
			{
                            fprintf(stderr,
                                     "  Adding top %ld, prod_origin = %d, modified prod = ",
                                    (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                                     prod_origin);
                            print_rule_with_dot(ps, stderr, state->rule, state->dot_i);
                            fprintf(stderr, ", state->origin_i = %d\n", state->origin_i);
			}

                        curr_state = state;
                        anode = state->anode;
		    }
		}		/* if (n_candidates != 0)*/
                if (prod_rule->anode != NULL)
		{
                    /* This rule creates abstract node.*/
                    state = parse_state_alloc(ps);
                    state->rule = prod_rule;
                    state->dot_i = prod->dot_i;
                    state->origin_i = prod_origin;
                    state->current_state_set_i = current_state_set_i;
                    table_state = NULL;
                    if (!ps->run.grammar->one_parse_p)
                        table_state = parse_state_insert(ps, state, &new_p);
                    if (table_state == NULL || new_p)
		    {
                        /* We need new abtract node.*/
                        ps->n_parse_abstract_nodes++;
                        node
                            =((YaepTreeNode*)
                              (*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                               + sizeof(YaepTreeNode*)
                                              *(prod_rule->trans_len + 1)));
                        state->anode = node;
                        if (table_state != NULL)
                            table_state->anode = node;
                        node->type = YAEP_ANODE;
                        if (prod_rule->caller_anode == NULL)
			{
                            prod_rule->caller_anode
                                =((char*)
                                  (*ps->run.parse_alloc)(strlen(prod_rule->anode) + 1));
                            strcpy(prod_rule->caller_anode, prod_rule->anode);
			}
                        node->val.anode.name = prod_rule->caller_anode;
                        node->val.anode.cost = prod_rule->anode_cost;
                        // IXML Copy the rule name -to the generated abstract node.
                        node->val.anode.mark = prod_rule->mark;
                        if (rule->marks && rule->marks[pos])
                        {
                            // But override the mark with the rhs mark!
                            node->val.anode.mark = rule->marks[pos];
                        }
                        /////////
                        node->val.anode.children
                            =((YaepTreeNode**)
                              ((char*) node + sizeof(YaepTreeNode)));
                        for(k = 0; k <= prod_rule->trans_len; k++)
                            node->val.anode.children[k] = NULL;
                        VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
                        if (anode == NULL)
			{
                            state->parent_anode_state
                                = curr_state->parent_anode_state;
                            state->parent_disp = parent_disp;
			}
                        else
			{
                            state->parent_anode_state = curr_state;
                            state->parent_disp = disp;
			}

                        if (ps->run.debug)
			{
                            fprintf(stderr, "  Adding top %ld, current_state_set_i = %d, prod = ",
                                    (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                                    current_state_set_i);
                            print_production(ps, stderr, prod, ps->run.debug, -1);
                            fprintf(stderr, ", %d\n", prod_origin);
			}

		    }
                    else
		    {
                        /* We allready have the translation.*/
                        assert(!ps->run.grammar->one_parse_p);
                        parse_state_free(ps, state);
                        state =((YaepInternalParseState**) VLO_BOUND(stack))[-1];
                        node = table_state->anode;
                        assert(node != NULL);

                        if (ps->run.debug)
			{
                            fprintf(stderr,
                                     "  Found prev. translation: current_state_set_i = %d, prod = ",
                                     current_state_set_i);
                            print_production(ps, stderr, prod, ps->run.debug, -1);
                            fprintf(stderr, ", %d\n", prod_origin);
			}

		    }
                    place_translation(ps, anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       node);
		}		/* if (prod_rule->anode != NULL)*/
                else if (prod->dot_i != 0)
		{
                    /* We should generate and use the translation of the
                       nonterminal.  Add state to get a translation.*/
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                   ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = prod_rule;
                    state->dot_i = prod->dot_i;
                    state->origin_i = prod_origin;
                    state->current_state_set_i = current_state_set_i;
                    state->parent_anode_state =(anode == NULL
                                                 ? curr_state->
                                                 parent_anode_state :
                                                 curr_state);
                    state->parent_disp = anode == NULL ? parent_disp : disp;
                    state->anode = NULL;

                    if (ps->run.debug)
		    {
                        fprintf(stderr,
                                 "  Adding top %ld, current_state_set_i = %d, prod = ",
                                (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                                current_state_set_i);
                        print_production(ps, stderr, prod, ps->run.debug, -1);
                        fprintf(stderr, ", %d\n", prod_origin);
		    }

		}
                else
		{
                    /* Empty rule should produce something not abtract
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
	}			/* For all reduces of the nonterminal.*/
        /* We should have a parse.*/
        assert(n_candidates != 0 && (!ps->run.grammar->one_parse_p || n_candidates == 1));
    } /* For all parser states.*/
    VLO_DELETE(stack);
    if (!ps->run.grammar->one_parse_p)
    {
        VLO_DELETE(orig_states);
        yaep_free(ps->run.grammar->alloc, term_node_array);
    }
    parse_state_fin(ps);
    ps->run.grammar->one_parse_p = saved_one_parse_p;
    if (ps->run.grammar->cost_p &&*ambiguous_p)
        /* We can not build minimal tree during building parsing list
           because we have not the translation yet.  We can not make it
           during parsing because the abstract nodes are created before
           their children.*/
        result = find_minimal_translation(ps, result);

    if (ps->run.debug)
    {
        fprintf(stderr, "Translation:\n");
        print_parse(ps, stderr, result);
        fprintf(stderr, "\n");
    }
    else if (ps->run.debug)
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
  *AMBIGUOUS_P if we found that the grammer is ambigous(it works even
   we asked only one parse tree without alternatives).*/
int yaepParse(YaepParseRun *pr, YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)pr;

    assert(CHECK_PARSE_STATE_MAGIC(ps));

    ps->run.grammar = g;
    YaepTreeNode **root = &ps->run.root;
    int *ambiguous_p = &ps->run.ambiguous_p;

    int code, tok_init_p, parse_init_p;
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
   *ambiguous_p = FALSE;
    pl_init(ps);
    tok_init_p = parse_init_p = FALSE;
    if ((code = setjmp(error_longjump_buff)) != 0)
    {
        pl_fin(ps);
        if (parse_init_p)
            yaep_parse_fin(ps);
        if (tok_init_p)
            tok_fin(ps);
        return code;
    }
    if (g->undefined_p)
    {
        yaep_error(ps, YAEP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
    }
    ps->n_goto_successes = 0;
    tok_init(ps);
    tok_init_p = TRUE;
    read_input_tokens(ps);
    yaep_parse_init(ps, ps->input_tokens_len);
    parse_init_p = TRUE;
    pl_create(ps);
    table_collisions = get_all_collisions();
    table_searches = get_all_searches();

    perform_parse(ps);
    *root = build_parse_tree(ps, ambiguous_p);

    table_collisions = get_all_collisions() - table_collisions;
    table_searches = get_all_searches() - table_searches;


    if (ps->run.verbose)
    {
        fprintf(stderr, "%sGrammar: #terms = %d, #nonterms = %d, ",
                *ambiguous_p ? "AMBIGUOUS " : "",
                 ps->run.grammar->symbs_ptr->num_terms, ps->run.grammar->symbs_ptr->num_nonterms);
        fprintf(stderr, "#rules = %d, rules size = %d\n",
                 ps->run.grammar->rules_ptr->n_rules,
                 ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules);
        fprintf(stderr, "Input: #tokens = %d, #unique productions = %d\n",
                 ps->input_tokens_len, ps->n_all_productions);
        fprintf(stderr, "       #terminal sets = %d, their size = %d\n",
                 ps->run.grammar->term_sets_ptr->n_term_sets, ps->run.grammar->term_sets_ptr->n_term_sets_size);
        fprintf(stderr,
                 "       #unique set cores = %d, #their start productions = %d\n",
                 ps->n_set_cores, ps->n_set_core_start_productions);
        fprintf(stderr,
                 "       #parent indexes for some non start productions = %d\n",
                 ps->n_parent_indexes);
        fprintf(stderr,
                 "       #unique set dist. vects = %d, their length = %d\n",
                 ps->n_set_distances, ps->n_set_distances_len);
        fprintf(stderr,
                 "       #unique sets = %d, #their start productions = %d\n",
                 ps->n_sets, ps->n_sets_start_productions);
        fprintf(stderr,
                 "       #unique triples(set, term, lookahead) = %d, goto successes=%d\n",
                ps->num_triplets_core_term_lookahead, ps->n_goto_successes);
        fprintf(stderr,
                 "       #pairs(set core, symb) = %d, their trans+reduce vects length = %d\n",
                 ps->n_core_symb_pairs, ps->n_core_symb_vect_len);
        fprintf(stderr,
                 "       #unique transition vectors = %d, their length = %d\n",
                ps->n_transition_vects, ps->n_transition_vect_len);
        fprintf(stderr,
                 "       #unique reduce vectors = %d, their length = %d\n",
                 ps->n_reduce_vects, ps->n_reduce_vect_len);
        fprintf(stderr,
                 "       #term nodes = %d, #abstract nodes = %d\n",
                 ps->n_parse_term_nodes, ps->n_parse_abstract_nodes);
        fprintf(stderr,
                 "       #alternative nodes = %d, #all nodes = %d\n",
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

    yaep_parse_fin(ps);
    tok_fin(ps);
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
        pl_fin(ps);
        rule_fin(g, g->rules_ptr);
        term_set_fin(g, g->term_sets_ptr);
        symb_fin(ps, g->symbs_ptr);
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
                            void(*termcb)(YaepTermNode*))
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
            termcb(&node->val.term);
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
                  void(*termcb)(YaepTermNode*))
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
static void symb_print(FILE* f, YaepSymb*symb, int code_p)
{
    fprintf(f, "%s", symb->repr);
    if (code_p && symb->term_p)
    {
        fprintf(f, "(%d)", symb->u.term.code);
    }
}

/* The following function prints RULE with its translation(if TRANS_P) to file F.*/
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, int trans_p)
{
    int i, j;

    assert(rule->mark >= 0 && rule->mark < 128);
    fprintf(f, "%c", rule->mark?rule->mark:' ');
    symb_print(f, rule->lhs, FALSE);
    fprintf(f, " :");
    for(i = 0; i < rule->rhs_len; i++)
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
        symb_print(f, rule->rhs[i], FALSE);
    }
    if (trans_p)
    {
        fprintf(f, " ---- ");
        if (rule->anode != NULL)
            fprintf(f, "%s(", rule->anode);
        for(i = 0; i < rule->trans_len; i++)
	{
            for(j = 0; j < rule->rhs_len; j++)
                if (rule->order[j] == i)
                {
                    fprintf(f, " %d:", j);
                    symb_print(f, rule->rhs[j], FALSE);
                    break;
                }
            if (j >= rule->rhs_len)
                fprintf(f, " nil");
	}
        if (rule->anode != NULL)
            fprintf(f, " )");
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

    symb_print(f, rule->lhs, FALSE);
    fprintf(f, " → ");
    for(i = 0; i < rule->rhs_len; i++)
    {
        fprintf(f, i == pos ? " 🞄 " : " ");
        symb_print(f, rule->rhs[i], FALSE);
    }
    if (rule->rhs_len == pos)
    {
        fprintf(f, " 🞄 ");
    }
}

/* The following function prints production PROD to file F.  The
   production is printed with the lookahead set if LOOKAHEAD_P.*/
static void print_production(YaepParseState *ps, FILE *f, YaepProduction *prod, int lookahead_p, int distance)
{
    fprintf(f, "(%3d)    ", prod->prod_id);
    print_rule_with_dot(ps, f, prod->rule, prod->dot_i);

    if (distance >= 0)
    {
        fprintf(f, ", distance %d", distance);
    }
    if (ps->run.grammar->lookahead_level != 0 && lookahead_p)
    {
        fprintf(f, "    ");
        term_set_print(ps, f, prod->lookahead, ps->run.grammar->symbs_ptr->num_terms);
    }
    if (distance != -1) fprintf(f, "\n");
}

/* The following function prints SET to file F.  If NONSTART_P is TRUE
   then print all productions.  The productions are printed with the
   lookahead set if LOOKAHEAD_P.  SET_DIST is used to print absolute
   distances of not-yet-started productions. */
static void print_state_set(YaepParseState *ps,
                            FILE* f,
                            YaepStateSet *state_set,
                            int set_dist,
                            int print_all_productions,
                            int lookahead_p)
{
    int i;
    int num, num_started_productions, num_productions, n_all_distances;
    YaepProduction**productions;
    int*distances,*parent_indexes;

    if (state_set == NULL && !ps->new_set_ready_p)
    {
        /* The following is necessary if we call the function from a
           debugger.  In this case new_set, new_core and their members
           may be not set up yet. */
        num = -1;
        num_started_productions = num_productions = n_all_distances = ps->new_num_started_productions;
        productions = ps->new_productions;
        distances = ps->new_distances;
        parent_indexes = NULL;
    }
    else
    {
        num = state_set->core->core_id;
        num_productions = state_set->core->num_productions;
        productions = state_set->core->productions;
        num_started_productions = state_set->core->num_started_productions;
        distances = state_set->distances;
        n_all_distances = state_set->core->n_all_distances;
        parent_indexes = state_set->core->parent_indexes;
        num_started_productions = state_set->core->num_started_productions;
    }

    fprintf(f, "  core(%d)\n", num);

    for(i = 0; i < num_productions; i++)
    {
        fprintf(f, "    ");

        int dist = 0;
        if (i < num_started_productions) dist = distances[i];
        else if (i < n_all_distances) dist = parent_indexes[i];
        else dist = 0;

        assert(dist == (i < num_started_productions ? distances[i] : i < n_all_distances ? parent_indexes[i] : 0));

        print_production(ps, f, productions[i], lookahead_p, dist);

        if (i == num_started_productions - 1 && num_productions > num_started_productions)
        {
            if (!print_all_productions)
            {
                // We have printed the start productions. Stop here.
                break;
            }
            fprintf(f, "    ----------- predictions\n");
        }
    }
}
