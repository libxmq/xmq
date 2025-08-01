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

#include "yaep_cspc.h"
#include "yaep_vlobject.h"
#include "yaep_util.h"

#endif

#ifdef YAEP_CSPC_MODULE

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

static void free_vlo_array(YaepParseState *ps)
{
    vlo_t *vlo_ptr;

    for (vlo_ptr = (vlo_t*)VLO_BEGIN(ps->vlo_array); (char*) vlo_ptr < (char*) VLO_BOUND(ps->vlo_array); vlo_ptr++)
    {
        VLO_DELETE(*vlo_ptr);
    }
    VLO_DELETE(ps->vlo_array);
}


#ifdef USE_CORE_SYMB_HASH_TABLE
/* Hash of core_symb_to_predcomps.*/
static unsigned core_symb_to_predcomps_hash(YaepCoreSymbToPredComps *core_symb_to_predcomps)
{
    return (jauquet_prime_mod32* hash_shift+(size_t)/* was unsigned */core_symb_to_predcomps->core) * hash_shift
        +(size_t)/* was unsigned */core_symb_to_predcomps->symb;
}

/* Equality of core_symb_to_predcomps.*/
static bool core_symb_to_predcomps_eq(YaepCoreSymbToPredComps *core_symb_to_predcomps1, YaepCoreSymbToPredComps *core_symb_to_predcomps2)
{
    return core_symb_to_predcomps1->core == core_symb_to_predcomps2->core && core_symb_to_predcomps1->symb == core_symb_to_predcomps2->symb;
}
#endif

static unsigned vect_ids_hash(YaepVect*v)
{
    unsigned result = jauquet_prime_mod32;

    for (int i = 0; i < v->len; i++)
    {
        result = result* hash_shift + v->ids[i];
    }
    return result;
}

static bool vect_ids_eq(YaepVect *v1, YaepVect *v2)
{
    if (v1->len != v2->len) return false;

    for (int i = 0; i < v1->len; i++)
    {
        if (v1->ids[i] != v2->ids[i]) return false;
    }
    return true;
}

static unsigned prediction_ids_hash(hash_table_entry_t t)
{
    return vect_ids_hash(&((YaepCoreSymbToPredComps*)t)->predictions);
}

static bool prediction_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbToPredComps*)t1)->predictions,
                       &((YaepCoreSymbToPredComps*)t2)->predictions);
}

static unsigned completion_ids_hash(hash_table_entry_t t)
{
    return vect_ids_hash(&((YaepCoreSymbToPredComps*)t)->completions);
}

static bool completion_ids_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_ids_eq(&((YaepCoreSymbToPredComps*) t1)->completions,
                       &((YaepCoreSymbToPredComps*) t2)->completions);
}

/* Initialize work with the triples(set core, symbol, vector).*/
void core_symb_to_predcomps_init(YaepParseState *ps)
{
    OS_CREATE(ps->core_symb_to_predcomps_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->new_core_symb_to_predcomps_vlo, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->vect_ids_os, ps->run.grammar->alloc, 0);

    vlo_array_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    ps->map_core_symb_to_predcomps = create_hash_table(ps->run.grammar->alloc, 3000,
                                                       (hash_table_hash_function)core_symb_to_predcomps_hash,
                                                       (hash_table_eq_function)core_symb_to_predcomps_eq);
#else
    VLO_CREATE(ps->core_symb_table_vlo, ps->run.grammar->alloc, 4096);
    ps->core_symb_table = (YaepCoreSymbToPredComps***)VLO_BEGIN(ps->core_symb_table_vlo);
    OS_CREATE(ps->core_symb_tab_rows, ps->run.grammar->alloc, 8192);
#endif

    ps->map_transition_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000,
                                                           (hash_table_hash_function)prediction_ids_hash,
                                                           (hash_table_eq_function)prediction_ids_eq);

    ps->map_reduce_to_coresymbvect = create_hash_table(ps->run.grammar->alloc, 3000,
                                                       (hash_table_hash_function)completion_ids_hash,
                                                       (hash_table_eq_function)completion_ids_eq);

    ps->n_core_symb_pairs = ps->n_core_symb_to_predcomps_len = 0;
    ps->n_transition_vects = ps->n_transition_vect_len = 0;
    ps->n_reduce_vects = ps->n_reduce_vect_len = 0;
}


#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed.*/
static YaepCoreSymbToPredComps **core_symb_to_pred_comps_addr_get(YaepParseState *ps, YaepCoreSymbToPredComps *triple, int reserv_p)
{
    YaepCoreSymbToPredComps **result;

    if (triple->symb->cached_core_symb_to_predcomps != NULL
        && triple->symb->cached_core_symb_to_predcomps->core == triple->core)
    {
        return &triple->symb->cached_core_symb_to_predcomps;
    }

    result = ((YaepCoreSymbToPredComps**)find_hash_table_entry(ps->map_core_symb_to_predcomps, triple, reserv_p));

    triple->symb->cached_core_symb_to_predcomps = *result;

    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed. */
static YaepCoreSymbToPredComps **core_symb_to_predcomps_addr_get(YaepParseState *ps, YaepStateSetCore *set_core, YaepSymbol *symb)
{
    YaepCoreSymbToPredComps***core_symb_to_predcomps_ptr;

    core_symb_to_predcomps_ptr = ps->core_symb_table + set_core->id;

    if ((char*) core_symb_to_predcomps_ptr >=(char*) VLO_BOUND(ps->core_symb_table_vlo))
    {
        YaepCoreSymbToPredComps***ptr,***bound;
        int diff, i;

        diff =((char*) core_symb_to_predcomps_ptr
                -(char*) VLO_BOUND(ps->core_symb_table_vlo));
        diff += sizeof(YaepCoreSymbToPredComps**);
        if (diff == sizeof(YaepCoreSymbToPredComps**))
            diff*= 10;

        VLO_EXPAND(ps->core_symb_table_vlo, diff);
        ps->core_symb_table = (YaepCoreSymbToPredComps***) VLO_BEGIN(ps->core_symb_table_vlo);
        core_symb_to_predcomps_ptr = ps->core_symb_table + set_core->id;
        bound =(YaepCoreSymbToPredComps***) VLO_BOUND(ps->core_symb_table_vlo);

        ptr = bound - diff / sizeof(YaepCoreSymbToPredComps**);
        while(ptr < bound)
        {
            OS_TOP_EXPAND(ps->core_symb_tab_rows,
                          (ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals)
                          * sizeof(YaepCoreSymbToPredComps*));
           *ptr =(YaepCoreSymbToPredComps**) OS_TOP_BEGIN(ps->core_symb_tab_rows);
            OS_TOP_FINISH(ps->core_symb_tab_rows);
            for(i = 0; i < ps->run.grammar->symbs_ptr->num_terminals + ps->run.grammar->symbs_ptr->num_nonterminals; i++)
               (*ptr)[i] = NULL;
            ptr++;
        }
    }
    return &(*core_symb_to_predcomps_ptr)[symb->id];
}
#endif

/* The following function returns the triple(if any) for given SET_CORE and SYMB. */
YaepCoreSymbToPredComps *core_symb_to_predcomps_find(YaepParseState *ps, YaepStateSetCore *core, YaepSymbol *symb)
{
    YaepCoreSymbToPredComps *r;

#ifdef USE_CORE_SYMB_HASH_TABLE
    YaepCoreSymbToPredComps core_symb_to_predcomps;

    core_symb_to_predcomps.core = core;
    core_symb_to_predcomps.symb = symb;
    r = *core_symb_to_pred_comps_addr_get(ps, &core_symb_to_predcomps, false);
#else
    r = *core_symb_to_predcomps_addr_get(ps, core, symb);
#endif

    return r;
}

YaepCoreSymbToPredComps *core_symb_to_predcomps_new(YaepParseState *ps, YaepStateSetCore*core, YaepSymbol*symb)
{
    YaepCoreSymbToPredComps*core_symb_to;
    YaepCoreSymbToPredComps**addr;
    vlo_t*vlo_ptr;

    /* Create table element.*/
    OS_TOP_EXPAND(ps->core_symb_to_predcomps_os, sizeof(YaepCoreSymbToPredComps));
    core_symb_to = ((YaepCoreSymbToPredComps*) OS_TOP_BEGIN(ps->core_symb_to_predcomps_os));
    core_symb_to->id = ps->core_symb_to_pred_comps_counter++;
    core_symb_to->core = core;
    core_symb_to->symb = symb;
    OS_TOP_FINISH(ps->core_symb_to_predcomps_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_to_pred_comps_addr_get(ps, core_symb_to, true);
#else
    addr = core_symb_to_pred_comps_addr_get(ps, core, symb);
#endif
    assert(*addr == NULL);
   *addr = core_symb_to;

    core_symb_to->predictions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, core_symb_to->predictions.intern);
    core_symb_to->predictions.len = 0;
    core_symb_to->predictions.ids =(int*) VLO_BEGIN(*vlo_ptr);

    core_symb_to->completions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, core_symb_to->completions.intern);
    core_symb_to->completions.len = 0;
    core_symb_to->completions.ids =(int*) VLO_BEGIN(*vlo_ptr);
    VLO_ADD_MEMORY(ps->new_core_symb_to_predcomps_vlo, &core_symb_to,
                    sizeof(YaepCoreSymbToPredComps*));
    ps->n_core_symb_pairs++;

    return core_symb_to;
}

static void vect_add_id(YaepParseState *ps, YaepVect *vec, int id)
{
    vec->len++;
    vlo_t *vlo_ptr = vlo_array_el(ps, vec->intern);
    VLO_ADD_MEMORY(*vlo_ptr, &id, sizeof(int));
    vec->ids =(int*) VLO_BEGIN(*vlo_ptr);
    ps->n_core_symb_to_predcomps_len++;
}

void core_symb_to_predcomps_add_predict(YaepParseState *ps,
                                      YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                      int rule_index_in_core)
{
    vect_add_id(ps, &core_symb_to_predcomps->predictions, rule_index_in_core);

    YaepDottedRule *dotted_rule = core_symb_to_predcomps->core->dotted_rules[rule_index_in_core];
    yaep_trace(ps, "add prediction cspc%d[c%d %s] -> d%d",
               core_symb_to_predcomps->id,
               core_symb_to_predcomps->core->id,
               core_symb_to_predcomps->symb->hr,
               dotted_rule->id);
}

void core_symb_to_predcomps_add_complete(YaepParseState *ps,
                                       YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                       int rule_index_in_core)
{
    vect_add_id(ps, &core_symb_to_predcomps->completions, rule_index_in_core);
    YaepDottedRule *dotted_rule = core_symb_to_predcomps->core->dotted_rules[rule_index_in_core];
    yaep_trace(ps, "completed d%d store in cspc%d[c%d %s]",
               dotted_rule->id,
               core_symb_to_predcomps->id,
               core_symb_to_predcomps->core->id,
               core_symb_to_predcomps->symb->hr
               );

}

/* Insert vector VEC from CORE_SYMB_TO_PREDCOMPS into table TAB.  Update
   *N_VECTS and INT*N_VECT_LEN if it is a new vector in the table. */
static void process_core_symb_to_predcomps_el(YaepParseState *ps,
                                      YaepCoreSymbToPredComps *core_symb_to_predcomps,
                                      YaepVect *vec,
                                      hash_table_t *tab,
                                      int *n_vects,
                                      int *n_vect_len)
{
    hash_table_entry_t*entry;

    if (vec->len == 0)
    {
        vec->ids = NULL;
    }
    else
    {
        entry = find_hash_table_entry(*tab, core_symb_to_predcomps, true);
        if (*entry != NULL)
        {
            vec->ids = (&core_symb_to_predcomps->predictions == vec
                        ?((YaepCoreSymbToPredComps*)*entry)->predictions.ids
                        :((YaepCoreSymbToPredComps*)*entry)->completions.ids);
        }
        else
        {
            *entry = (hash_table_entry_t)core_symb_to_predcomps;
            OS_TOP_ADD_MEMORY(ps->vect_ids_os, vec->ids, vec->len* sizeof(int));
            vec->ids =(int*) OS_TOP_BEGIN(ps->vect_ids_os);
            OS_TOP_FINISH(ps->vect_ids_os);
            (*n_vects)++;
            *n_vect_len += vec->len;
        }
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_to_predcomps.*/
void core_symb_to_predcomps_new_all_stop(YaepParseState *ps)
{
    YaepCoreSymbToPredComps**triple_ptr;

    for(triple_ptr =(YaepCoreSymbToPredComps**) VLO_BEGIN(ps->new_core_symb_to_predcomps_vlo);
        (char*) triple_ptr <(char*) VLO_BOUND(ps->new_core_symb_to_predcomps_vlo);
         triple_ptr++)
    {
        process_core_symb_to_predcomps_el(ps, *triple_ptr, &(*triple_ptr)->predictions,
                                  &ps->map_transition_to_coresymbvect, &ps->n_transition_vects,
                                  &ps->n_transition_vect_len);
        process_core_symb_to_predcomps_el(ps, *triple_ptr, &(*triple_ptr)->completions,
                                  &ps->map_reduce_to_coresymbvect, &ps->n_reduce_vects,
                                  &ps->n_reduce_vect_len);
    }
    vlo_array_nullify(ps);
    VLO_NULLIFY(ps->new_core_symb_to_predcomps_vlo);
}

/* Finalize work with all triples(set core, symbol, vector).*/
void free_core_symb_to_vect_lookup(YaepParseState *ps)
{
    delete_hash_table(ps->map_transition_to_coresymbvect);
    delete_hash_table(ps->map_reduce_to_coresymbvect);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table(ps->map_core_symb_to_predcomps);
#else
    OS_DELETE(ps->core_symb_tab_rows);
    VLO_DELETE(ps->core_symb_table_vlo);
#endif
    free_vlo_array(ps);
    OS_DELETE(ps->vect_ids_os);
    VLO_DELETE(ps->new_core_symb_to_predcomps_vlo);
    OS_DELETE(ps->core_symb_to_predcomps_os);
}


#endif
