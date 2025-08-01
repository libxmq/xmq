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
#include "yaep_print.h"
#include "yaep_structs.h"
#include "yaep_tree.h"
#include "yaep_util.h"

#endif

#ifdef YAEP_TREE_MODULE

static unsigned parse_state_hash(hash_table_entry_t s)
{
    YaepParseTreeBuildState*state =((YaepParseTreeBuildState*) s);

    /* The table contains only states with dot at the end of rule. */
    assert(state->dot_j == state->rule->rhs_len);
    return(((jauquet_prime_mod32* hash_shift +
             (unsigned)(size_t) state->rule)* hash_shift +
             state->from_i)* hash_shift + state->state_set_k);
}

static bool parse_state_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepParseTreeBuildState*state1 =((YaepParseTreeBuildState*) s1);
    YaepParseTreeBuildState*state2 =((YaepParseTreeBuildState*) s2);

    /* The table contains only states with dot at the end of rule.*/
    assert(state1->dot_j == state1->rule->rhs_len
            && state2->dot_j == state2->rule->rhs_len);
    return(state1->rule == state2->rule && state1->from_i == state2->from_i
            && state1->state_set_k == state2->state_set_k);
}

/* The following function initializes work with parser states.*/
static void parse_state_init(YaepParseState *ps)
{
    ps->free_parse_state = NULL;
    OS_CREATE(ps->parse_state_os, ps->run.grammar->alloc, 0);
    if (!ps->run.grammar->one_parse_p)
    {
        ps->map_rule_orig_statesetind_to_internalstate =
            create_hash_table(ps->run.grammar->alloc, ps->input_len* 2,
                              (hash_table_hash_function)parse_state_hash,
                              (hash_table_eq_function)parse_state_eq);
    }
}

/* The following function returns new parser state.*/
static YaepParseTreeBuildState *parse_state_alloc(YaepParseState *ps)
{
    YaepParseTreeBuildState*result;

    if (ps->free_parse_state == NULL)
    {
        OS_TOP_EXPAND(ps->parse_state_os, sizeof(YaepParseTreeBuildState));
        result =(YaepParseTreeBuildState*) OS_TOP_BEGIN(ps->parse_state_os);
        OS_TOP_FINISH(ps->parse_state_os);
    }
    else
    {
        result = ps->free_parse_state;
        ps->free_parse_state =(YaepParseTreeBuildState*) ps->free_parse_state->rule;
    }
    return result;
}

/* The following function frees STATE.*/
static void parse_state_free(YaepParseState *ps, YaepParseTreeBuildState*state)
{
    state->rule = (YaepRule*)ps->free_parse_state;
    ps->free_parse_state = state;
}

/* The following function searches for state in the table with the
   same characteristics as "state".  If found, then it returns a pointer
   to the state in the table.  Otherwise the function makes copy of
  *STATE, inserts into the table and returns pointer to copied state.
   In the last case, the function also sets up*NEW_P.*/
static YaepParseTreeBuildState *parse_state_insert(YaepParseState *ps, YaepParseTreeBuildState *state, bool *new_p)
{
    YaepParseTreeBuildState **entry = (YaepParseTreeBuildState**)find_hash_table_entry(
        ps->map_rule_orig_statesetind_to_internalstate,
        state,
        true);

    *new_p = false;

    if (*entry != NULL)
    {
        return *entry;
    }

    *new_p = true;

    /* We make copy because state_set_k can be changed in further processing state. */
    *entry = parse_state_alloc(ps);
    **entry = *state;

    return *entry;
}

static void free_parse_state(YaepParseState *ps)
{
    if (!ps->run.grammar->one_parse_p)
    {
        delete_hash_table(ps->map_rule_orig_statesetind_to_internalstate);
    }
    OS_DELETE(ps->parse_state_os);
}

/* The following function places translation NODE into *PLACE and
   creates alternative nodes if it is necessary. */
static void place_translation(YaepParseState *ps, YaepTreeNode **place, YaepTreeNode *node)
{
    YaepTreeNode *alt, *next_alt;

    assert(place != NULL);
    if (*place == NULL)
    {
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
}

static YaepTreeNode *copy_anode(YaepParseState *ps,
                                YaepTreeNode **place,
                                YaepTreeNode *anode,
                                YaepRule *rule,
                                int rhs_offset)
{
    YaepTreeNode*node;
    int i;

    node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                              + sizeof(YaepTreeNode*)
                                              *(rule->trans_len + 1)));
   *node =*anode;
    node->val.anode.children = ((YaepTreeNode**)((char*) node + sizeof(YaepTreeNode)));
    for(i = 0; i <= rule->trans_len; i++)
    {
        node->val.anode.children[i] = anode->val.anode.children[i];
    }
    node->val.anode.children[rhs_offset] = NULL;
    place_translation(ps, place, node);

    return node;
}

static void loop_stack(YaepTreeNode **result,
                       int n_candidates,
                       YaepParseState *ps,
                       YaepTreeNode *empty_node,
                       YaepTreeNode *error_node,
                       YaepStateSet *set,
                       YaepDottedRule *dotted_rule,
                       bool *ambiguous_p)
{
    YaepParseTreeBuildState root_state;
    YaepTreeNode root_anode;

    vlo_t stack, orig_states;

    YaepTreeNode **term_node_array = NULL;

    VLO_CREATE(stack, ps->run.grammar->alloc, 10000);

    if (!ps->run.grammar->one_parse_p)
    {
        /* We need this array to reuse terminal nodes when building a parse tree with ALT nodes.
           I.e. a tree with multiple possible parses. */
        size_t term_node_array_size = ps->input_len * sizeof(YaepTreeNode*);
        term_node_array = (YaepTreeNode**)yaep_malloc(ps->run.grammar->alloc, term_node_array_size);
        memset(term_node_array, 0, term_node_array_size);
        /* The following is used to check necessity to create current state with different state_set_k. */
        VLO_CREATE(orig_states, ps->run.grammar->alloc, 0);
    }

    // The root abstract node points to the result tree pointer.
    root_anode.val.anode.children = result;
    // The root state pointsto the root abstract node.
    root_state.anode = &root_anode;

    // Push a new state on the stack
    YaepParseTreeBuildState *state = parse_state_alloc(ps);
    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;

    YaepRule *rule = state->rule = dotted_rule->rule;
    state->dotted_rule = dotted_rule;
    state->dot_j = dotted_rule->dot_j;
    state->from_i = 0;
    state->state_set_k = ps->state_set_k;

    // Again is state_set_k always == tok_i ????
    assert(ps->state_set_k == ps->tok_i);

    state->parent_anode_state = &root_state;
    state->parent_rhs_offset = 0;
    state->anode = NULL;

    if (ps->run.debug)
    {
        // Log the starting node.
        MemBuffer *mb = new_membuffer();
	membuffer_printf(mb, "adding (d%d,%d-%d) ",
                         state->dotted_rule->id,
                         state->from_i,
                         state->state_set_k);
	print_rule(mb, ps, state->rule);
        debug_mb("ixml.bt.step=", mb);
        free_membuffer_and_free_content(mb);
    }

    while (VLO_LENGTH(stack) != 0)
    {
        if (ps->run.debug && state->dot_j == state->rule->rhs_len)
        {
            // top = (long) VLO_LENGTH(stack) / sizeof(YaepParseTreeBuildState*) - 1
            MemBuffer *mb = new_membuffer();
            membuffer_printf(mb, "push (s%d,d%d) [%d-%d]    ", state->state_set_k, state->dotted_rule->id, state->from_i, state->state_set_k);
            print_rule(mb, ps, state->rule);
            debug_mb("ixml.bt.info=", mb);
            free_membuffer_and_free_content(mb);
        }
        int pos_j = --state->dot_j;
        rule = state->rule;
        YaepParseTreeBuildState *parent_anode_state = state->parent_anode_state;
        YaepTreeNode *parent_anode = parent_anode_state->anode;
        int parent_rhs_offset = state->parent_rhs_offset;
        YaepTreeNode *anode = state->anode;
        int rhs_offset = rule->order[pos_j];
        int state_set_k = state->state_set_k;
        int from_i = state->from_i;
        if (pos_j < 0)
        {
            /* We've processed all rhs of the rule.*/
            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "pop ");
                print_rule(mb, ps, state->rule);
                debug_mb("ixml.bt.info=", mb);
                free_membuffer_and_free_content(mb);
            }
            parse_state_free(ps, state);
            VLO_SHORTEN(stack, sizeof(YaepParseTreeBuildState*));
            if (VLO_LENGTH(stack) != 0)
            {
                state = ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
            }
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
            {
                /* We do dotted_ruleuce nothing but we should. So write empty node.*/
                place_translation(ps, parent_anode->val.anode.children + parent_rhs_offset, empty_node);
                empty_node->val.nil.used = 1;
            } else
            if (anode != NULL)
            {
                /* Change NULLs into empty nodes.  We can not make it
                 the first time because when building several parses
                 the NULL means flag of absence of translations(see
                 function `place_translation'). */
                for (int i = 0; i < rule->trans_len; i++)
                {
                    if (anode->val.anode.children[i] == NULL)
                    {
                        anode->val.anode.children[i] = empty_node;
                        empty_node->val.nil.used = 1;
                    }
                }
            }

            continue;
        }
        assert(pos_j >= 0);
        YaepTreeNode *node;
        YaepSymbol *symb = rule->rhs[pos_j];
        if (symb->is_terminal)
        {
            /* Terminal before dot:*/
            state_set_k--; /* l*/
            /* Because of error recovery input [state_set_k].symb may be not equal to symb.*/
            //assert(ps->input[state_set_k].symb == symb);
            if (parent_anode != NULL && rhs_offset >= 0)
            {
                /* We should generate and use the translation of the terminal.  Add reference to the current node.*/
                if (symb == ps->run.grammar->term_error)
                {
                    // Oups error node.
                    node = error_node;
                    error_node->val.error.used = 1;
                } else
                if (!ps->run.grammar->one_parse_p && (node = term_node_array[state_set_k]) != NULL)
                {
                    // Reuse existing terminal node.
                } else
                {
                    // Allocate terminal node.
                    ps->n_parse_term_nodes++;
                    node = ((YaepTreeNode*) (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
                    node->type = YAEP_TERM;
                    node->val.terminal.code = symb->u.terminal.code;
                    if (rule->marks && rule->marks[pos_j])
                    {
                        // Copy the ixml mark from the rhs position on to the terminal.
                        node->val.terminal.mark = rule->marks[pos_j];
                    }
                    // Copy any attr from input token to parsed terminal.
                    node->val.terminal.attr = ps->input[state_set_k].attr;
                    if (!ps->run.grammar->one_parse_p)
                    {
                        term_node_array[state_set_k] = node;
                    }
                }

                YaepTreeNode **placement = NULL;
                if (anode)
                {
                    placement = anode->val.anode.children + rhs_offset;
                } else
                {
                    placement = parent_anode->val.anode.children + parent_rhs_offset;
                }
                place_translation(ps, placement, node);
            }
            if (pos_j != 0)
            {
                state->state_set_k = state_set_k;
            }
            continue;
        }
        /* Nonterminal before dot: */
        set = ps->state_sets[state_set_k];
        YaepStateSetCore *set_core = set->core;
        YaepCoreSymbToPredComps *core_symb_to_predcomps = core_symb_to_predcomps_find(ps, set_core, symb);
        //debug("ixml.pa.c=", "core core%d symb %s -> %p", set_core->id, symb->hr, core_symb_to_predcomps);
        if (!core_symb_to_predcomps)
            continue;

        assert(core_symb_to_predcomps->completions.len != 0);
        n_candidates = 0;
        YaepParseTreeBuildState *orig_state = state;
        if (!ps->run.grammar->one_parse_p)
        {
            VLO_NULLIFY(orig_states);
        }
        for (int i = 0; i < core_symb_to_predcomps->completions.len; i++)
        {
            int rule_index_in_core = core_symb_to_predcomps->completions.ids[i];
            dotted_rule = set_core->dotted_rules[rule_index_in_core];
            int dotted_rule_from_i;
            if (rule_index_in_core < set_core->num_started_dotted_rules)
            {
                // The state_set_k is the tok_i for which the state set was created.
                // Ie, it is the to_i inside the Earley item.
                // Now subtract the matched length from this to_i to get the from_i
                // which is the origin.
                dotted_rule_from_i = state_set_k - set->matched_lengths[rule_index_in_core];
            } else
            if (rule_index_in_core < set_core->num_all_matched_lengths)
            {
                // Parent??
                dotted_rule_from_i = state_set_k - set->matched_lengths[set_core->to_parent_rule_index[rule_index_in_core]];
            } else
            {
                dotted_rule_from_i = state_set_k;
            }

            YaepStateSet *check_set = ps->state_sets[dotted_rule_from_i];
            YaepStateSetCore *check_set_core = check_set->core;
            YaepCoreSymbToPredComps *check_core_symb_to_predcomps = core_symb_to_predcomps_find(ps, check_set_core, symb);
            assert(check_core_symb_to_predcomps != NULL);
            bool found = false;
            if (ps->run.debug)
            {
                MemBuffer *mb = new_membuffer();
                membuffer_printf(mb, "trying (s%d,d%d) [%d-%d]  cspc%d check_cspc%d  ",
                                 state_set_k,
                                 dotted_rule->id,
                                 dotted_rule_from_i,
                                 state_set_k,
                                 core_symb_to_predcomps->id,
                                 check_core_symb_to_predcomps->id);
                print_rule(mb, ps, dotted_rule->rule);
                debug_mb("ixml.bt.info=", mb);
                free_membuffer_and_free_content(mb);
            }
            for (int j = 0; j < check_core_symb_to_predcomps->predictions.len; j++)
            {
                int rule_index_in_check_core = check_core_symb_to_predcomps->predictions.ids[j];
                YaepDottedRule *check_dotted_rule = check_set->core->dotted_rules[rule_index_in_check_core];
                if (check_dotted_rule->rule != rule || check_dotted_rule->dot_j != pos_j)
                {
                    continue;
                }
                int check_dotted_rule_from_i = dotted_rule_from_i;
                if (rule_index_in_check_core < check_set_core->num_all_matched_lengths)
                {
                    if (rule_index_in_check_core < check_set_core->num_started_dotted_rules)
                    {
                        check_dotted_rule_from_i = dotted_rule_from_i - check_set->matched_lengths[rule_index_in_check_core];
                    } else
                    {
                        check_dotted_rule_from_i = (dotted_rule_from_i
                                        - check_set->matched_lengths[check_set_core->to_parent_rule_index[rule_index_in_check_core]]);
                    }
                }
                if (check_dotted_rule_from_i == from_i)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                continue;
            }
            if (n_candidates != 0)
            {
                // Oups, n_candidates is > 0 already, this means that a previous completion matched.
                // We have more than one parse.
                debug("ixml.bt.info=", "n_candidates=%d -> ambiguous=true", n_candidates);
                *ambiguous_p = true;
                if (ps->run.grammar->one_parse_p)
                {
                    break;
                }
            }
            YaepRule *dotted_rule_rule = dotted_rule->rule;
            if (n_candidates == 0)
            {
                orig_state->state_set_k = dotted_rule_from_i;
            }
            if (parent_anode != NULL && rhs_offset >= 0)
            {
                /* We should generate and use the translation of the nonterminal. */
                YaepParseTreeBuildState *curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule. */
                if (n_candidates != 0)
                {
                    assert(!ps->run.grammar->one_parse_p);
                    if (n_candidates == 1)
                    {
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1] = orig_state;
                    }
                    int j = VLO_LENGTH(orig_states) / sizeof(YaepParseTreeBuildState*) - 1;
                    while (j >= 0)
                    {
                        if (((YaepParseTreeBuildState**) VLO_BEGIN(orig_states))[j]->state_set_k == dotted_rule_from_i)
                        {
                            break;
                        }
                        j--;
                    }
                    if (j >= 0)
                    {
                        /* [A -> x., n] & [A -> y., n]*/
                        curr_state = ((YaepParseTreeBuildState**) VLO_BEGIN(orig_states))[j];
                        anode = curr_state->anode;
                    } else
                    {
                        /* [A -> x., n] & [A -> y., m] where n != m.*/
                        /* It is different from the previous ones so add
                         it to process.*/
                        // Push a new state on the stack.
                        state = parse_state_alloc(ps);
                        VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                        *state = *orig_state;
                        state->state_set_k = dotted_rule_from_i;
                        if (anode != NULL)
                        {
                            state->anode = copy_anode(ps, parent_anode->val.anode.children + parent_rhs_offset, anode, rule, rhs_offset);
                        }
                        VLO_EXPAND(orig_states, sizeof(YaepParseTreeBuildState*));
                        ((YaepParseTreeBuildState**) VLO_BOUND(orig_states))[-1] = state;
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "* (f%d,d%d) add1 modified dotted_rule=", dotted_rule_from_i, state->dotted_rule->id);
                            print_rule_with_dot(mb, ps, state->rule, state->dot_j);
                            membuffer_printf(mb, " state->from_i=%d", state->from_i);
                            debug_mb("ixml.bt.c=", mb);
                            free_membuffer_and_free_content(mb);
                            assert(false);
                        }
                        curr_state = state;
                        anode = state->anode;
                    }
                }
                /* WOOT? if (n_candidates != 0)*/
                if (dotted_rule_rule->anode != NULL)
                {
                    /* This rule creates abstract node. */
                    // Push a new state on the stack.
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = dotted_rule_rule;
                    state->dotted_rule = dotted_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    YaepParseTreeBuildState *table_state = NULL;
                    _Bool new_p;
                    if (!ps->run.grammar->one_parse_p)
                    {
                        table_state = parse_state_insert(ps, state, &new_p);
                    }
                    if (table_state == NULL || new_p)
                    {
                        /* Allocate abtract node. */
                        ps->n_parse_abstract_nodes++;
                        node = ((YaepTreeNode*) (*ps->run.parse_alloc)(sizeof(YaepTreeNode) + sizeof(YaepTreeNode*) * (dotted_rule_rule->trans_len + 1)));
                        node->type = YAEP_ANODE;
                        state->anode = node;
                        if (table_state != NULL)
                        {
                            table_state->anode = node;
                        }
                        if (dotted_rule_rule->caller_anode == NULL)
                        {
                            dotted_rule_rule->caller_anode = ((char*) (*ps->run.parse_alloc)(strlen(dotted_rule_rule->anode) + 1));
                            strcpy(dotted_rule_rule->caller_anode, dotted_rule_rule->anode);
                        }
                        node->val.anode.name = dotted_rule_rule->caller_anode;
                        node->val.anode.cost = dotted_rule_rule->anode_cost;
                        // IXML Copy the rule name -to the generated abstract node.
                        node->val.anode.mark = dotted_rule_rule->mark;
                        if (rule->marks && rule->marks[pos_j])
                        {
                            // But override the mark with the rhs mark!
                            node->val.anode.mark = rule->marks[pos_j];
                        }
                        /////////
                        node->val.anode.children = ((YaepTreeNode**) ((char*) node + sizeof(YaepTreeNode)));
                        for (int k = 0; k <= dotted_rule_rule->trans_len; k++)
                        {
                            node->val.anode.children[k] = NULL;
                        }
                        if (anode == NULL)
                        {
                            state->parent_anode_state = curr_state->parent_anode_state;
                            state->parent_rhs_offset = parent_rhs_offset;
                        } else
                        {
                            state->parent_anode_state = curr_state;
                            state->parent_rhs_offset = rhs_offset;
                        }
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "adding (d%d,%d-%d) ",
                                             state->dotted_rule->id,
                                             state->from_i, state->state_set_k);
                            print_rule(mb, ps, dotted_rule->rule);
                            debug_mb("ixml.bt.step=", mb);
                            free_membuffer_and_free_content(mb);
                        }
                    } else
                    {
                        /* We allready have the translation.*/
                        assert(!ps->run.grammar->one_parse_p);
                        parse_state_free(ps, state);
                        state = ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1];
                        node = table_state->anode;
                        assert(node != NULL);
                        if (ps->run.debug)
                        {
                            MemBuffer *mb = new_membuffer();
                            membuffer_printf(mb, "* found prev. translation: state_set_k = %d, dotted_rule = ", state_set_k);
                            print_dotted_rule(mb, ps, -1, dotted_rule, -1, -1);
                            membuffer_printf(mb, ", %d\n", dotted_rule_from_i);
                            debug_mb("ixml.bt.info=", mb);
                            free_membuffer_and_free_content(mb);
                            // assert(false);
                        }
                    }
                    YaepTreeNode **placement = NULL;
                    if (anode)
                    {
                        placement = anode->val.anode.children + rhs_offset;
                    } else
                    {
                        placement = parent_anode->val.anode.children + parent_rhs_offset;
                    }
                    place_translation(ps, placement, node);
                } /* if (dotted_rule_rule->anode != NULL)*/else
                if (dotted_rule->dot_j != 0)
                {
                    /* We should generate and use the translation of the
                     nonterminal.  Add state to get a translation.*/
                    // Push a new state on the stack
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepParseTreeBuildState*));
                    ((YaepParseTreeBuildState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = dotted_rule_rule;
                    state->dotted_rule = dotted_rule;
                    state->dot_j = dotted_rule->dot_j;
                    state->from_i = dotted_rule_from_i;
                    state->state_set_k = state_set_k;
                    state->parent_anode_state = (anode == NULL ? curr_state->parent_anode_state : curr_state);
                    assert(state->parent_anode_state);
                    state->parent_rhs_offset = anode == NULL ? parent_rhs_offset : rhs_offset;
                    state->anode = NULL;
                    if (ps->run.debug)
                    {
                        MemBuffer *mb = new_membuffer();
                        membuffer_printf(mb, "* add3   state_set_k=%d   dotted_rule_from_i=%d    ", state_set_k, dotted_rule_from_i);
                        print_rule(mb, ps, dotted_rule->rule);
                        debug_mb("ixml.bt.info=", mb);
                        free_membuffer_and_free_content(mb);
                        //assert(false);
                    }
                } else
                {
                    /* Empty rule should dotted_ruleuce something not abtract
                     node.  So place empty node.*/
                    place_translation(ps, anode == NULL ? parent_anode->val.anode.children + parent_rhs_offset : anode->val.anode.children + rhs_offset,
                                    empty_node);
                    empty_node->val.nil.used = 1;
                }
            }
            /* if (parent_anode != NULL && rhs_offset >= 0)*/                // Continue with next completion.
            n_candidates++;
        }
        /* For all completions of the nonterminal.*//* We should have a parse.*/
        assert(n_candidates != 0 && (!ps->run.grammar->one_parse_p || n_candidates == 1));
    }
    /* For all parser states.*/

    VLO_DELETE(stack);

    if (!ps->run.grammar->one_parse_p)
    {
        VLO_DELETE(orig_states);
        yaep_free(ps->run.grammar->alloc, term_node_array);
    }
}

/* The hash of the memory reference. */
static unsigned reserv_mem_hash(hash_table_entry_t m)
{
    return (size_t)m;
}

/* The equity of the memory reference. */
static bool reserv_mem_eq(hash_table_entry_t m1, hash_table_entry_t m2)
{
    return m1 == m2;
}

/* The following function sets up minimal cost for each abstract node.
   The function returns minimal translation corresponding to NODE.
   The function also collects references to memory which can be
   freed. Remeber that the translation is DAG, altenatives form lists
   (alt node may not refer for another alternative). */
static YaepTreeNode *prune_to_minimal(YaepParseState *ps, YaepTreeNode *node, int *cost)
{
    YaepTreeNode*child,*alt,*next_alt,*result = NULL;
    int i, min_cost = INT_MAX;

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
            for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
            {
                node->val.anode.children[i] = prune_to_minimal(ps, child, cost);
                if (node->val.anode.children[i] != child)
                {
                    //fprintf(stderr, "PRUNEDDDDDDD\n");
                }
                node->val.anode.cost += *cost;
            }
           *cost = node->val.anode.cost;
            node->val.anode.cost = -node->val.anode.cost - 1;        /* flag of visit*/
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
            if (alt == node || min_cost > *cost)
            {
                if (ps->run.debug)
                {
                    fprintf(stderr, "FOUND smaller cost %d %s\n", *cost, alt->val.alt.node->val.anode.name);
                }
                min_cost = *cost;
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
        assert(false);
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
    if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->set_of_reserved_memory, node, true)) == NULL)
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
                                                                      true)) == NULL)
        {
            *entry =(hash_table_entry_t) node->val.anode.name;
        }
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            traverse_pruned_translation(ps, child);
        }
        // FIXME Is this assert needed? What is its purpose?
        // assert(node->val.anode.cost < 0);
        node->val.anode.cost = -node->val.anode.cost - 1;
        break;
    case YAEP_ALT:
        traverse_pruned_translation(ps, node->val.alt.node);
        if ((node = node->val.alt.next) != NULL)
            goto next;
        break;
    default:
        assert(false);
    }

    return;
}

/* The function finds and returns a minimal cost parse(s). */
static YaepTreeNode *find_minimal_translation(YaepParseState *ps, YaepTreeNode *root)
{
    YaepTreeNode**node_ptr;
    int cost;

    if (ps->run.parse_free != NULL)
    {
        ps->set_of_reserved_memory = create_hash_table(ps->run.grammar->alloc, ps->input_len* 4,
                                                       (hash_table_hash_function)reserv_mem_hash,
                                                       (hash_table_eq_function)reserv_mem_eq);

        VLO_CREATE(ps->tnodes_vlo, ps->run.grammar->alloc, ps->input_len* 4* sizeof(void*));
    }
    root = prune_to_minimal(ps, root, &cost);

    traverse_pruned_translation(ps, root);

    if (ps->run.parse_free != NULL)
    {
        for(node_ptr = (YaepTreeNode**)VLO_BEGIN(ps->tnodes_vlo);
            node_ptr <(YaepTreeNode**)VLO_BOUND(ps->tnodes_vlo);
            node_ptr++)
        {
            if (*find_hash_table_entry(ps->set_of_reserved_memory,*node_ptr, true) == NULL)
            {
                if ((*node_ptr)->type == YAEP_ANODE
                    &&*find_hash_table_entry(ps->set_of_reserved_memory,
                                             (*node_ptr)->val.anode.name,
                                             true) == NULL)
                {
                    // (*ps->run.parse_free)((void*)(*node_ptr)->val.anode.name);
                }
                //(*ps->run.parse_free)(*node_ptr);
            }
        }
        VLO_DELETE(ps->tnodes_vlo);
        delete_hash_table(ps->set_of_reserved_memory);
    }

    return root;
}

YaepTreeNode *build_parse_tree(YaepParseState *ps, bool *ambiguous_p)
{
    yaep_debug(ps, "build_parse_tree()");

    // Number of candiate trees found.
    int n_candidates = 0;

    // The result pointer points to the final parse tree.
    YaepTreeNode *result;

    ps->n_parse_term_nodes = ps->n_parse_abstract_nodes = ps->n_parse_alt_nodes = 0;

    // Pick the final state set, where we completed the axiom $.
    YaepStateSet *set = ps->state_sets[ps->state_set_k];
    assert(ps->run.grammar->axiom != NULL);

    /* We have only one start dotted_rule: "$ : <start symb> eof .". */
    YaepDottedRule *dotted_rule = (set->core->dotted_rules != NULL ? set->core->dotted_rules[0] : NULL);

    if (dotted_rule == NULL
        || set->matched_lengths[0] != ps->state_set_k
        || dotted_rule->rule->lhs != ps->run.grammar->axiom || dotted_rule->dot_j != dotted_rule->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'.*/
        assert(!ps->run.grammar->error_recovery_p);
        return NULL;
    }
    bool saved_one_parse_p = ps->run.grammar->one_parse_p;
    if (ps->run.grammar->cost_p)
    {
        /* We need all parses to choose the minimal one. */
        ps->run.grammar->one_parse_p = false;
    }

    parse_state_init(ps);

    // The result tree starts empty.
    result = NULL;

    /* Create empty and error node:*/
    YaepTreeNode *empty_node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    empty_node->type = YAEP_NIL;
    empty_node->val.nil.used = 0;

    YaepTreeNode *error_node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    error_node->type = YAEP_ERROR;
    error_node->val.error.used = 0;

    verbose("ixml=", "building tree");

    loop_stack(&result, n_candidates, ps, empty_node, error_node, set, dotted_rule, ambiguous_p);

    free_parse_state(ps);
    ps->run.grammar->one_parse_p = saved_one_parse_p;
    if (ps->run.grammar->cost_p && *ambiguous_p)
    {
        /* We can not build minimal tree during building parsing list
           because we have not the translation yet. We can not make it
           during parsing because the abstract nodes are created before
           their children. */
        result = find_minimal_translation(ps, result);
    }
    if (false)
    {
        fprintf(stderr, "(ixml) yaep parse tree: %p\n", result);
        print_parse(ps, stderr, result);
        fprintf(stderr, "\n");
    }
    if (false)
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

/* Hash of translation visit node.*/
unsigned trans_visit_node_hash(hash_table_entry_t n)
{
    return(size_t)((YaepTreeNodeVisit*) n)->node;
}

/* Equality of translation visit nodes.*/
bool trans_visit_node_eq(hash_table_entry_t n1, hash_table_entry_t n2)
{
    return(((YaepTreeNodeVisit*) n1)->node == ((YaepTreeNodeVisit*) n2)->node);
}

/* The following function returns the positive order number of node with number NUM.*/
int canon_node_id(int id)
{
    return (id < 0 ? -id-1 : id);
}

/* The following function checks presence translation visit node with
   given NODE in the table and if it is not present in the table, the
   function creates the translation visit node and inserts it into
   the table.*/
YaepTreeNodeVisit *visit_node(YaepParseState *ps, YaepTreeNode*node)
{
    YaepTreeNodeVisit trans_visit_node;
    hash_table_entry_t*entry;

    trans_visit_node.node = node;
    entry = find_hash_table_entry(ps->map_node_to_visit,
                                   &trans_visit_node, true);

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

#endif
