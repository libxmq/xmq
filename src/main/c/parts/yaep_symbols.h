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

#ifndef YAEP_SYMBOLS_H
#define YAEP_SYMBOLS_H

unsigned symb_repr_hash(hash_table_entry_t s);
bool symb_repr_eq(hash_table_entry_t s1, hash_table_entry_t s2);
unsigned symb_code_hash(hash_table_entry_t s);
bool symb_code_eq(hash_table_entry_t s1, hash_table_entry_t s2);
YaepSymbolStorage *symbolstorage_create(YaepGrammar *grammar);

/* Return symbol(or NULL if it does not exist) whose representation is REPR.*/
YaepSymbol *symb_find_by_repr(YaepParseState *ps, const char*repr);

/* Return symbol(or NULL if it does not exist) which is terminal with CODE. */
YaepSymbol *symb_find_by_code(YaepParseState *ps, int code);

YaepSymbol *symb_find_by_term_id(YaepParseState *ps, int term_id);

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
YaepSymbol *symb_add_terminal(YaepParseState *ps, const char*name, int code);

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table. The function
   should create own copy of name for the new symbol. */
YaepSymbol *symb_add_nonterm(YaepParseState *ps, const char *name);

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *symb_get(YaepParseState *ps, int id);

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *term_get(YaepParseState *ps, int n);

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *nonterm_get(YaepParseState *ps, int n);

void symb_finish_adding_terms(YaepParseState *ps);

/* Free memory for symbols. */
void symb_empty(YaepParseState *ps, YaepSymbolStorage *symbs);

void symbolstorage_free(YaepParseState *ps, YaepSymbolStorage *symbs);


#define YAEP_SYMBOLS_MODULE

#endif
