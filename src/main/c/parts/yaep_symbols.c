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

#include<stdio.h>
#include "yaep.h"
#include "yaep_hashtab.h"
#include "yaep_objstack.h"
#include "yaep_vlobject.h"
#include "yaep_structs.h"
#include "yaep_symbols.h"
#include "yaep_util.h"

#endif

#ifdef YAEP_SYMBOLS_MODULE

unsigned symb_repr_hash(hash_table_entry_t s)
{
    YaepSymbol *sym = (YaepSymbol*)s;
    unsigned result = jauquet_prime_mod32;

    for (const char *i = sym->repr; *i; i++)
    {
        result = result * hash_shift +(unsigned)*i;
    }

     return result;
}

bool symb_repr_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymbol *sym1 = (YaepSymbol*)s1;
    YaepSymbol *sym2 = (YaepSymbol*)s2;

    return !strcmp(sym1->repr, sym2->repr);
}

unsigned symb_code_hash(hash_table_entry_t s)
{
    YaepSymbol *sym = (YaepSymbol*)s;

    assert(sym->is_terminal);

    return sym->u.terminal.code;
}

bool symb_code_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymbol *sym1 = (YaepSymbol*)s1;
    YaepSymbol *sym2 = (YaepSymbol*)s2;

    assert(sym1->is_terminal && sym2->is_terminal);

    return sym1->u.terminal.code == sym2->u.terminal.code;
}

YaepSymbolStorage *symbolstorage_create(YaepGrammar *grammar)
{
    void*mem;
    YaepSymbolStorage*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepSymbolStorage));
    result = (YaepSymbolStorage*)mem;
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
YaepSymbol *symb_find_by_repr(YaepParseState *ps, const char*repr)
{
    YaepSymbol symb;
    symb.repr = repr;
    YaepSymbol*r = (YaepSymbol*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_repr_to_symb, &symb, false);

    return r;
}

/* Return symbol(or NULL if it does not exist) which is terminal with CODE. */
YaepSymbol *symb_find_by_code(YaepParseState *ps, int code)
{
    YaepSymbol symb;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        if ((code < ps->run.grammar->symbs_ptr->symb_code_trans_vect_start) ||(code >= ps->run.grammar->symbs_ptr->symb_code_trans_vect_end))
        {
            return NULL;
        }
        else
        {
            YaepSymbol *r = ps->run.grammar->symbs_ptr->symb_code_trans_vect[code - ps->run.grammar->symbs_ptr->symb_code_trans_vect_start];
            return r;
        }
    }

    symb.is_terminal = true;
    symb.u.terminal.code = code;
    YaepSymbol*r =(YaepSymbol*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->map_code_to_symb, &symb, false);

    return r;
}

YaepSymbol *symb_find_by_term_id(YaepParseState *ps, int term_id)
{
    return symb_find_by_code(ps, term_id+ps->run.grammar->symbs_ptr->symb_code_trans_vect_start);
}

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
YaepSymbol *symb_add_terminal(YaepParseState *ps, const char*name, int code)
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
    symb.is_terminal = true;
    // Assign the next available id.
    symb.id = ps->run.grammar->symbs_ptr->num_nonterminals + ps->run.grammar->symbs_ptr->num_terminals;
    symb.u.terminal.code = code;
    symb.u.terminal.term_id = ps->run.grammar->symbs_ptr->num_terminals++;
    symb.empty_p = false;
    symb.is_not_lookahead_p = false;
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

    return result;
}

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table. The function
   should create own copy of name for the new symbol. */
YaepSymbol *symb_add_nonterm(YaepParseState *ps, const char *name)
{
    YaepSymbol symb,*result;
    hash_table_entry_t*entry;

    symb.repr = name;
    strncpy(symb.hr, name, 6);

    symb.is_terminal = false;
    symb.is_not_lookahead_p = false;
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

    if (is_not_rule(result))
    {
        result->is_not_lookahead_p = true;
    }
    return result;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *symb_get(YaepParseState *ps, int id)
{
    if (id < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->symbs_vlo) / sizeof(YaepSymbol*) <=(size_t) id))
    {
        return NULL;
    }
    YaepSymbol **vect = (YaepSymbol**)VLO_BEGIN(ps->run.grammar->symbs_ptr->symbs_vlo);
    YaepSymbol *symb = vect[id];
    assert(symb->id == id);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *term_get(YaepParseState *ps, int n)
{
    if (n < 0 || (VLO_LENGTH(ps->run.grammar->symbs_ptr->terminals_vlo) / sizeof(YaepSymbol*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymbol*symb =((YaepSymbol**) VLO_BEGIN(ps->run.grammar->symbs_ptr->terminals_vlo))[n];
    assert(symb->is_terminal && symb->u.terminal.term_id == n);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
YaepSymbol *nonterm_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->nonterminals_vlo) / sizeof(YaepSymbol*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymbol*symb =((YaepSymbol**) VLO_BEGIN(ps->run.grammar->symbs_ptr->nonterminals_vlo))[n];
    assert(!symb->is_terminal && symb->u.nonterminal.nonterm_id == n);

    return symb;
}

void symb_finish_adding_terms(YaepParseState *ps)
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

    //debug("ixml.stats=", "num_codes=%zu offset=%d size=%zu", num_codes, ps->run.grammar->symbs_ptr->symb_code_trans_vect_start, vec_size);
}

/* Free memory for symbols. */
void symb_empty(YaepParseState *ps, YaepSymbolStorage *symbs)
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
}

void symbolstorage_free(YaepParseState *ps, YaepSymbolStorage *symbs)
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
}

/* The following function prints symbol SYMB to file F.  Terminal is
   printed with its code if CODE_P. */
void symbol_print(MemBuffer *mb, YaepSymbol *symb, bool code_p)
{
    if (symb->is_terminal)
    {
        membuffer_append(mb, symb->hr);
        return;
    }
    membuffer_append(mb, symb->repr);
    if (code_p && symb->is_terminal)
    {
        membuffer_printf(mb, "(%d)", symb->u.terminal.code);
    }
}

#endif
