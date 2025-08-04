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

#ifndef YAEP_TREE_H
#define YAEP_TREE_H

#ifndef BUILDING_DIST_XMQ

#include "yaep_structs.h"

#endif

/* Build the parse tree from the yaep parse. */
YaepTreeNode *build_parse_tree(YaepParseState *ps, bool *ambiguous_p);

/* Hash of translation visit node.*/
unsigned trans_visit_node_hash(hash_table_entry_t n);

/* Equality of translation visit nodes.*/
bool trans_visit_node_eq(hash_table_entry_t n1, hash_table_entry_t n2);

/* The following function returns the positive order number of node with number NUM.*/
int canon_node_id(int id);

/* The following function checks presence translation visit node with
   given NODE in the table and if it is not present in the table, the
   function creates the translation visit node and inserts it into
   the table.*/
YaepTreeNodeVisit *visit_node(YaepParseState *ps, YaepTreeNode*node);

#define YAEP_TREE_MODULE

#endif
