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

#ifndef BUILDING_DIST_XMQ

#include "yaep_terminal_bitset.h"
#include "yaep_symbols.h"

#endif

#ifdef YAEP_TERMINAL_BITSET_MODULE

unsigned terminal_bitset_hash(hash_table_entry_t s)
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
bool terminal_bitset_eq(hash_table_entry_t s1, hash_table_entry_t s2)
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

YaepTerminalSetStorage *termsetstorage_create(YaepGrammar *grammar)
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

terminal_bitset_t *terminal_bitset_create(YaepParseState *ps)
{
    int size_bytes;
    terminal_bitset_t *result;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    assert(sizeof(terminal_bitset_t) <= 8);

    size_bytes = sizeof(terminal_bitset_t) * CALC_NUM_ELEMENTS(num_terminals);

    OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->terminal_bitset_os, size_bytes);
    result =(terminal_bitset_t*) OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
    OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->terminal_bitset_os);
    ps->run.grammar->term_sets_ptr->n_term_sets++;
    ps->run.grammar->term_sets_ptr->n_term_sets_size += size_bytes;

    return result;
}

void terminal_bitset_clear(YaepParseState *ps, terminal_bitset_t* set)
{
    terminal_bitset_t*bound;
    int size;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = set + size;
    while(set < bound)
    {
       *set++ = 0;
    }
}

void terminal_bitset_fill(YaepParseState *ps, terminal_bitset_t* set)
{
    terminal_bitset_t*bound;
    int size;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = set + size;
    while(set < bound)
    {
        *set++ = (terminal_bitset_t)-1;
    }
}

/* Copy SRC into DEST. */
void terminal_bitset_copy(YaepParseState *ps, terminal_bitset_t *dest, terminal_bitset_t *src)
{
    terminal_bitset_t *bound;
    int size;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    size = CALC_NUM_ELEMENTS(num_terminals);
    bound = dest + size;

    while (dest < bound)
    {
       *dest++ = *src++;
    }
}

/* Add all terminals from set OP with to SET.  Return true if SET has been changed.*/
bool terminal_bitset_or(YaepParseState *ps, terminal_bitset_t *set, terminal_bitset_t *op)
{
    terminal_bitset_t *bound;
    int size;
    bool changed_p;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

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
bool terminal_bitset_up(YaepParseState *ps, terminal_bitset_t *set, int num)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;
    bool changed_p;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);
    changed_p = (set[word_offset] & bit_in_word ? false : true);
    set[word_offset] |= bit_in_word;

    return changed_p;
}

/* Remove terminal with number NUM from SET.  Return true if SET has been changed.*/
bool terminal_bitset_down(YaepParseState *ps, terminal_bitset_t *set, int num)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;
    bool changed_p;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);
    changed_p = (set[word_offset] & bit_in_word ? true : false);
    set[word_offset] &= ~bit_in_word;

    return changed_p;
}

/* Return true if terminal with number NUM is in SET. */
int terminal_bitset_test(YaepParseState *ps, terminal_bitset_t *set, int num)
{
    const int bits_in_word = CHAR_BIT*sizeof(terminal_bitset_t);

    int word_offset;
    terminal_bitset_t bit_in_word;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;

    assert(num < num_terminals);

    word_offset = num / bits_in_word;
    bit_in_word = ((terminal_bitset_t)1) << (num % bits_in_word);

    return (set[word_offset] & bit_in_word) != 0;
}

/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1(which is always negative).  Don't set after
   insertion!!! */
int terminal_bitset_insert(YaepParseState *ps, terminal_bitset_t *set)
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
terminal_bitset_t *terminal_bitset_from_table(YaepParseState *ps, int num)
{
    assert(num >= 0);
    assert((long unsigned int)num < VLO_LENGTH(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo) / sizeof(YaepTerminalSet*));

    return ((YaepTerminalSet**)VLO_BEGIN(ps->run.grammar->term_sets_ptr->terminal_bitset_vlo))[num]->set;
}

/* Free memory for terminal sets. */
void terminal_bitset_empty(YaepTerminalSetStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_NULLIFY(term_sets->terminal_bitset_vlo);
    empty_hash_table(term_sets->map_terminal_bitset_to_id);
    OS_EMPTY(term_sets->terminal_bitset_os);
    term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

void termsetstorage_free(YaepGrammar *grammar, YaepTerminalSetStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_DELETE(term_sets->terminal_bitset_vlo);
    delete_hash_table(term_sets->map_terminal_bitset_to_id);
    OS_DELETE(term_sets->terminal_bitset_os);
    yaep_free(grammar->alloc, term_sets);
    term_sets = NULL;
}

#endif
