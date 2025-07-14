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

#ifndef YAEP_UTIL_H
#define YAEP_UTIL_H

#ifndef BUILDING_DIST_XMQ

#include <stdio.h>

#endif

// Debug functions available to gdb.

void dbg_breakpoint();

void dbg_print_core(YaepParseState *ps, YaepStateSetCore *c);
void dbg_print_coresymbvects(YaepParseState *ps, YaepCoreSymbToPredComps *v);
void dbg_print_dotted_rule(YaepParseState *ps, YaepDottedRule *dotted_rule);
void fetch_state_vars(YaepParseState *ps, YaepStateSet *state_set, StateVars *out);
int find_matched_length(YaepParseState *ps, YaepStateSet *state_set, StateVars *vars, int dotted_rule_id);

#define YAEP_UTIL_MODULE

#endif
