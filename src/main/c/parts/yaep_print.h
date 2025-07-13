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

#ifndef YAEP_PRINT_H
#define YAEP_PRINT_H

#ifndef BUILDING_DIST_XMQ

#include <stdio.h>
#include "yaep_structs.h"

#endif

void print_core(MemBuffer*mb, YaepStateSetCore *c, YaepStateSet *s);
void print_coresymbvects(MemBuffer*mb, YaepParseState *ps, YaepCoreSymbToPredComps *v);
void print_dotted_rule(MemBuffer *mb, YaepParseState *ps, int from_i, YaepDottedRule *dotted_rule, int matched_length, int parent_id, const char *why);
void print_matched_lenghts(MemBuffer *mb, YaepStateSet *s);
void print_parse(YaepParseState *ps, FILE* f, YaepTreeNode*root);
void print_rule(MemBuffer *mb, YaepParseState *ps, YaepRule *rule);
void print_rule_with_dot(MemBuffer *mb, YaepParseState *ps, YaepRule *rule, int pos);
void print_state_set(MemBuffer *mb, YaepParseState *ps, YaepStateSet *set, int from_i);
void print_symbol(MemBuffer *mb, YaepSymbol *symb, bool code_p);
void print_terminal_bitset(MemBuffer *mb, YaepParseState *ps, terminal_bitset_t *set);
void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node);
void rule_print(MemBuffer *mb, YaepParseState *ps, YaepRule *rule, bool trans_p);

#define YAEP_PRINT_MODULE

#endif
