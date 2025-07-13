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

/*
    A terminal bitset stores a bit per terminal. For the ixml use case
    this is a bit per unicode character used in the input file.

    If the input file only contains the characters ABC, eg "ABCBABABCBCBA"
    then only three bits are used. This will fit in a single entry terminal_bitset_t
    aka a long int (64 bits) aka 8 bytes.

    If the input file contains an ascii only source file, then the number of
    distinct characters used are less than 128, typically ~100. This means 100 bits
    are used, and this requires two terminal_bitset_t (aka 128 bits aka 16 bytes).

    A rule lookahead scans all possible child rules and their leading characters.
    These characters are translated into a bits and or:ed togeter into a lookahead bitset.

    E.g. for the ABC file, if a rule only allows a single possible parse starting with A
    then the lookahead bitset is 0x1 since A was assigned bit 0x1.
    But if a rule allows for A and C as possible parses, then the lookahead bitset will
    be 0x5 which is the or of 0x1 (A) and 0x4 (C).

    The test whether the rule should be predicted or not can now be done with a simple bit test.
    The next char is B (0x2) is the bit 0x2 set in 0x5? No, it isn't then we avoid adding the
    rule to the predictions.

    This means that an ixml charset rule such as ~['A'] with the possible child rules 'B' and 'C'.
    will get a single lookahead terminal bitset 0x6 and the test wether a charset is applicable
    also becomes a single bit test. I.e. even if there are a lot of child rules (one for each possible char)
    the actual prediction is fast, a single bit test.
*/

#ifndef YAEP_TERMINAL_BITSET_H
#define YAEP_TERMINAL_BITSET_H

#ifndef BUILDING_DIST_XMQ

#include "yaep.h"
#include "yaep_allocate.h"
#include "yaep_hashtab.h"
#include "yaep_structs.h"

#endif

unsigned terminal_bitset_hash(hash_table_entry_t s);
bool terminal_bitset_eq(hash_table_entry_t s1, hash_table_entry_t s2);
YaepTerminalSetStorage *termsetstorage_create(YaepGrammar *grammar);
terminal_bitset_t *terminal_bitset_create(YaepParseState *ps);
void terminal_bitset_clear(YaepParseState *ps, terminal_bitset_t* set);
void terminal_bitset_fill(YaepParseState *ps, terminal_bitset_t* set);
void terminal_bitset_copy(YaepParseState *ps, terminal_bitset_t *dest, terminal_bitset_t *src);
/* Add all terminals from set OP with to SET.  Return true if SET has been changed.*/
bool terminal_bitset_or(YaepParseState *ps, terminal_bitset_t *set, terminal_bitset_t *op);
/* Add terminal with number NUM to SET.  Return true if SET has been changed.*/
bool terminal_bitset_up(YaepParseState *ps, terminal_bitset_t *set, int num);
/* Remove terminal with number NUM from SET.  Return true if SET has been changed.*/
bool terminal_bitset_down(YaepParseState *ps, terminal_bitset_t *set, int num);
/* Return true if terminal with number NUM is in SET. */
int terminal_bitset_test(YaepParseState *ps, terminal_bitset_t *set, int num);
/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1(which is always negative).  Don't set after
   insertion!!! */
int terminal_bitset_insert(YaepParseState *ps, terminal_bitset_t *set);
/* The following function returns set which is in the table with number NUM. */
terminal_bitset_t *terminal_bitset_from_table(YaepParseState *ps, int num);
/* Print terminal SET into file F. */
void terminal_bitset_print(MemBuffer *mb, YaepParseState *ps, terminal_bitset_t *set);
/* Free memory for terminal sets. */
void terminal_bitset_empty(YaepTerminalSetStorage *term_sets);
void termsetstorage_free(YaepGrammar *grammar, YaepTerminalSetStorage *term_sets);

#define YAEP_TERMINAL_BITSET_MODULE

#endif
