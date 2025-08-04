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

#ifndef YAEP_CSPC_H
#define YAEP_CSPC_H

#ifndef BUILDING_DIST_XMQ

#include "yaep_structs.h"

#endif

YaepCoreSymbToPredComps *core_symb_to_predcomps_new(YaepParseState *ps, YaepStateSetCore*core, YaepSymbol*symb);
void core_symb_to_predcomps_init(YaepParseState *ps);
YaepCoreSymbToPredComps *core_symb_to_predcomps_find(YaepParseState *ps, YaepStateSetCore *core, YaepSymbol *symb);
void free_core_symb_to_vect_lookup(YaepParseState *ps);
void core_symb_to_predcomps_add_predict(YaepParseState *ps,
                                        YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                        int rule_index_in_core);
void core_symb_to_predcomps_add_complete(YaepParseState *ps,
                                         YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                         int rule_index_in_core);
void core_symb_to_predcomps_new_all_stop(YaepParseState *ps);

#define YAEP_CSPC_MODULE

#endif
