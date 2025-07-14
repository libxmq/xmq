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

#include "membuffer.h"
#include "yaep_print.h"
#include "yaep_structs.h"
#include "yaep_util.h"
#include "yaep_symbols.h"
#include "yaep_terminal_bitset.h"
#include "yaep_tree.h"

#endif

#ifdef YAEP_PRINT_MODULE

void print_core(MemBuffer*mb, YaepStateSetCore *c)
{
    membuffer_printf(mb, "core%d{", c->id);

    for (int i = 0; i < c->num_started_dotted_rules; ++i)
    {
        if (i > 0) membuffer_append_char(mb, ' ');

        // num_started_dotted_rules;
        YaepDottedRule *dotted_rule = c->dotted_rules[i];
        membuffer_printf(mb, "d%d", dotted_rule->id);
    }
    membuffer_printf(mb, "}");
}

void print_coresymbvects(MemBuffer*mb, YaepParseState *ps, YaepCoreSymbToPredComps *v)
{
    membuffer_printf(mb, "coresymbvect %d %s preds: ", v->core->id, v->symb->hr);
    for (int i = 0; i < v->predictions.len; ++i)
    {
        int dotted_rule_id = v->predictions.ids[i];
        YaepDottedRule *dotted_rule = v->core->dotted_rules[dotted_rule_id];
        membuffer_printf(mb, " (%d)%s", dotted_rule_id, dotted_rule->rule->lhs->hr);
    }
    membuffer_printf(mb, " comps:");
    for (int i = 0; i < v->completions.len; ++i)
    {
        int dotted_rule_id = v->completions.ids[i];
        YaepDottedRule *dotted_rule = v->core->dotted_rules[dotted_rule_id];
        membuffer_printf(mb, " (%d)%s", dotted_rule_id, dotted_rule->rule->lhs->hr);
    }
}

/* The following function prints RULE with its translation(if TRANS_P) to file F.*/
void rule_print(MemBuffer *mb, YaepParseState *ps, YaepRule *rule, bool trans_p)
{
    int i;

    if (rule->mark != 0
        && rule->mark != ' '
        && rule->mark != '-'
        && rule->mark != '@'
        && rule->mark != '^'
        && rule->mark != '*')
    {
        membuffer_append(mb, "\n(yaep) internal error bad rule: ");
        print_symbol(mb, rule->lhs, false);
        debug_mb("ixml=", mb);
        free_membuffer_and_free_content(mb);
        assert(false);
    }

    char m = rule->mark;
    if (m >= 32 && m < 127)
    {
        membuffer_append_char(mb, m);
    }
    print_symbol(mb, rule->lhs, false);
    if (strcmp(rule->lhs->repr, rule->anode))
    {
        membuffer_append_char(mb, '(');
        membuffer_append(mb, rule->anode);
        membuffer_append_char(mb, ')');
    }
    membuffer_append(mb, " → ");
    int cost = rule->anode_cost;
    while (cost > 0)
    {
        membuffer_append_char(mb, '<');
        cost--;
    }
    for (i = 0; i < rule->rhs_len; i++)
    {
        char m = rule->marks[i];
        if (m >= 32 && m < 127)
        {
            membuffer_append_char(mb, m);
        }
        else
        {
            if (!m) membuffer_append(mb, "  ");
            else    assert(false);
        }
        print_symbol(mb, rule->rhs[i], false);
    }
    /*
      if (false && trans_p)
      {
      fprintf(f, "      | ");
      if (rule->anode != NULL)
      {
      fprintf(f, "%s(", rule->anode);
      }
      for(i = 0; i < rule->trans_len; i++)
      {
      for(j = 0; j < rule->rhs_len; j++)
      {
      if (rule->order[j] == i)
      {
      fprintf(f, " %d:", j);
      // TODO symbol_print(f, rule->rhs[j], false);
      break;
      }
      }
      if (j >= rule->rhs_len)
      {
      fprintf(f, " nil");
      }
      }
      if (rule->anode != NULL)
      {
      fprintf(f, " )");
      }
    */
}

/* The following function prints RULE to file F with dot in position pos.
   Pos == 0 means that the dot is all the way to the left in the starting position.
   Pos == rhs_len means that the whole rule has been matched. */
void print_rule_with_dot(MemBuffer *mb, YaepParseState *ps, YaepRule *rule, int pos)
{
    int i;

    assert(pos >= 0 && pos <= rule->rhs_len);

    print_symbol(mb, rule->lhs, false);
    membuffer_append(mb, " → ");
    for(i = 0; i < rule->rhs_len; i++)
    {
        membuffer_append(mb, i == pos ? " · " : " ");
        print_symbol(mb, rule->rhs[i], false);
    }
}

void print_rule(MemBuffer *mb, YaepParseState *ps, YaepRule *rule)
{
    int i;

    print_symbol(mb, rule->lhs, false);
    membuffer_append(mb, " → ");
    for(i = 0; i < rule->rhs_len; i++)
    {
        membuffer_append_char(mb, ' ');
        print_symbol(mb, rule->rhs[i], false);
    }
}

/* The following function prints the dotted_rule.
   Print the lookahead set if lookahead_p is true. */
void print_dotted_rule(MemBuffer *mb,
                       YaepParseState *ps,
                       int from_i,
                       YaepDottedRule *dotted_rule,
                       int matched_length,
                       int parent_id,
                       const char *why)
{
    char buf[256];

    snprintf(buf, 256, "(s%d,d%d) ", from_i, dotted_rule->id);
    membuffer_append(mb, buf);
    print_rule_with_dot(mb, ps, dotted_rule->rule, dotted_rule->dot_j);

    if (matched_length == 0)
    {
        if (dotted_rule->rule->rhs_len == dotted_rule->dot_j)
        {
            if (dotted_rule->rule->rhs_len == 0) membuffer_append(mb, " ε");
            snprintf(buf, 256, " complete[%d-%d/%d]", ps->tok_i, 1+ps->tok_i, ps->input_len);
            membuffer_append(mb, buf);
        }
        else
        {
            snprintf(buf, 256, " prediction[%d-%d/%d]", ps->tok_i, 1+ps->tok_i, ps->input_len);
            membuffer_append(mb, buf);
        }
    }
    else if (matched_length > 0)
    {
        if (dotted_rule->rule->rhs_len == dotted_rule->dot_j)
        {
            snprintf(buf, 256, " complete[%d-%d/%d]", 1+ps->tok_i-matched_length, 1+ps->tok_i, ps->input_len);
            membuffer_append(mb, buf);
        }
        else
        {
            snprintf(buf, 256, " partial[%d-%d/%d]", 1+ps->tok_i-matched_length, 1+ps->tok_i, ps->input_len);
            membuffer_append(mb, buf);
        }
    }
    else
    {
        membuffer_append(mb, " n/a[]");
    }

    if (why)
    {
        int cost = dotted_rule->rule->anode_cost;
        if (cost > 0)
        {
            while (cost-- > 0) membuffer_append(mb, "<");
            membuffer_append(mb, " ");
        }
        if (dotted_rule->rule->lhs->empty_p ||
            dotted_rule->empty_tail_p ||
            dotted_rule->info)
        {
            membuffer_append(mb, " ");
        }

        membuffer_printf(mb, "{%s:%s", dotted_rule->info, why);
        if (dotted_rule->rule->lhs->empty_p || dotted_rule->empty_tail_p)
        {
            membuffer_append(mb, " ");
        }
        if (dotted_rule->empty_tail_p)
        {
            membuffer_append(mb, " empty_tail");
        }
        if (dotted_rule->rule->lhs->empty_p)
        {
            membuffer_append(mb, " empty_rule");
        }

        if (parent_id >= 0) membuffer_printf(mb, " parent=d%d", parent_id);
        membuffer_printf(mb, " ml=%d", matched_length);
        membuffer_append(mb, "}");

        if (*why)
        {
            if (ps->run.grammar->lookahead_level != 0 && matched_length >= 0)
            {
                membuffer_append(mb, "    ");
                print_terminal_bitset(mb, ps, dotted_rule->lookahead);
            }
        }
    }
}

void print_matched_lenghts(MemBuffer *mb, YaepStateSet *s)
{
    for (int i = 0; i < s->core->num_started_dotted_rules; ++i)
    {
        if (i > 0) membuffer_append_char(mb, ' ');
        membuffer_printf(mb, "%d(d%d)", s->matched_lengths[i], s->core->dotted_rules[i]->id);
    }
}

/* The following recursive function prints NODE into file F and prints
   all its children(if debug_level < 0 output format is for graphviz).*/
void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node)
{
    YaepTreeNodeVisit*trans_visit_node;
    YaepTreeNode*child;
    int i;

    assert(node != NULL);
    trans_visit_node = visit_node(ps, node);
    if (trans_visit_node->num >= 0)
    {
        return;
    }
    trans_visit_node->num = -trans_visit_node->num - 1;
    if (ps->run.debug) fprintf(f, "%7d: ", trans_visit_node->num);
    switch(node->type)
    {
    case YAEP_NIL:
        if (ps->run.debug)
            fprintf(f, "EMPTY\n");
        break;
    case YAEP_ERROR:
        if (ps->run.debug > 0)
            fprintf(f, "ERROR\n");
        break;
    case YAEP_TERM:
        if (ps->run.debug)
            fprintf(f, "TERMINAL: code=%d, repr=%s, mark=%d %c\n", node->val.terminal.code,
                    symb_find_by_code(ps, node->val.terminal.code)->repr, node->val.terminal.mark, node->val.terminal.mark>32?node->val.terminal.mark:' ');
        break;
    case YAEP_ANODE:
        if (ps->run.debug)
        {
            fprintf(f, "ABSTRACT: %c%s(", node->val.anode.mark?node->val.anode.mark:' ', node->val.anode.name);
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
            {
                fprintf(f, " %d", canon_node_id(visit_node(ps, child)->num));
            }
            fprintf(f, ")\n");
        }
        else
        {
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
            {
                fprintf(f, "  \"%d: %s\" -> \"%d: ", trans_visit_node->num,
                         node->val.anode.name,
                        canon_node_id(visit_node(ps, child)->num));
                switch(child->type)
                {
                case YAEP_NIL:
                    fprintf(f, "EMPTY");
                    break;
                case YAEP_ERROR:
                    fprintf(f, "ERROR");
                    break;
                case YAEP_TERM:
                    fprintf(f, "%s",
                            symb_find_by_code(ps, child->val.terminal.code)->repr);
                    break;
                case YAEP_ANODE:
                    fprintf(f, "%s", child->val.anode.name);
                    break;
                case YAEP_ALT:
                    fprintf(f, "ALT");
                    break;
                default:
                    assert(false);
                }
                fprintf(f, "\";\n");
            }
        }
        for (i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            print_yaep_node(ps, f, child);
        }
        break;
    case YAEP_ALT:
        if (ps->run.debug)
        {
            fprintf(f, "ALTERNATIVE: node=%d, next=",
                    canon_node_id(visit_node(ps, node->val.alt.node)->num));
            if (node->val.alt.next != NULL)
                fprintf(f, "%d\n",
                        canon_node_id(visit_node(ps, node->val.alt.next)->num));
            else
                fprintf(f, "nil\n");
        }
        else
        {
            fprintf(f, "  \"%d: ALT\" -> \"%d: ", trans_visit_node->num,
                    canon_node_id(visit_node(ps, node->val.alt.node)->num));
            switch(node->val.alt.node->type)
            {
            case YAEP_NIL:
                fprintf(f, "EMPTY");
                break;
            case YAEP_ERROR:
                fprintf(f, "ERROR");
                break;
            case YAEP_TERM:
                fprintf(f, "%s",
                        symb_find_by_code(ps, node->val.alt.node->val.terminal.code)->
                         repr);
                break;
            case YAEP_ANODE:
                fprintf(f, "%s", node->val.alt.node->val.anode.name);
                break;
            case YAEP_ALT:
                fprintf(f, "ALT");
                break;
            default:
                assert(false);
            }
            fprintf(f, "\";\n");
            if (node->val.alt.next != NULL)
            {
                fprintf(f, "  \"%d: ALT\" -> \"%d: ALT\";\n",
                        trans_visit_node->num,
                        canon_node_id(visit_node(ps, node->val.alt.next)->num));
            }
        }
        print_yaep_node(ps, f, node->val.alt.node);
        if (node->val.alt.next != NULL)
        {
            print_yaep_node(ps, f, node->val.alt.next);
        }
        break;
    default:
        assert(false);
    }
}

/* The following function prints parse tree with ROOT.*/
void print_parse(YaepParseState *ps, FILE* f, YaepTreeNode*root)
{
    ps->map_node_to_visit = create_hash_table(ps->run.grammar->alloc,
                                              ps->input_len* 2,
                                              trans_visit_node_hash,
                                              trans_visit_node_eq);

    ps->num_nodes_visits = 0;
    OS_CREATE(ps->node_visits_os, ps->run.grammar->alloc, 0);
    print_yaep_node(ps, f, root);
    OS_DELETE(ps->node_visits_os);
    delete_hash_table(ps->map_node_to_visit);
}

/* The following function prints SET to file F.  If NOT_YET_STARTED_P is true
   then print all dotted_rules.  The dotted_rules are printed with the
   lookahead set if LOOKAHEAD_P.  SET_DIST is used to print absolute
   matched_lengths of not-yet-started dotted_rules. */
void print_state_set(MemBuffer *mb,
                     YaepParseState *ps,
                     YaepStateSet *state_set,
                     int from_i)
{
    StateVars vars;
    fetch_state_vars(ps, state_set, &vars);

    membuffer_printf(mb, "state=%d core=%d", vars.state_id, vars.core_id);

    int dotted_rule_id = 0;
    for(; dotted_rule_id < vars.num_dotted_rules; dotted_rule_id++)
    {
        int matched_length = find_matched_length(ps, state_set, &vars, dotted_rule_id);
        membuffer_append(mb, "\n");
        print_dotted_rule(mb, ps, from_i, vars.dotted_rules[dotted_rule_id], matched_length, -1, "woot3");
    }
}

/* The following function prints symbol SYMB to file F.  Terminal is
   printed with its code if CODE_P. */
void print_symbol(MemBuffer *mb, YaepSymbol *symb, bool code_p)
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

void print_terminal_bitset(MemBuffer *mb, YaepParseState *ps, terminal_bitset_t *set)
{
    bool first = true;
    int num_set = 0;
    int num_terminals = ps->run.grammar->symbs_ptr->num_terminals;
    for (int i = 0; i < num_terminals; i++) num_set += terminal_bitset_test(ps, set, i);

    if (num_set > num_terminals/2)
    {
        // Print the negation
        membuffer_append(mb, "~[");
        for (int i = 0; i < num_terminals; i++)
        {
            if (!terminal_bitset_test(ps, set, i))
            {
                if (!first) membuffer_append(mb, " "); else first = false;
                print_symbol(mb, term_get(ps, i), false);
            }
        }
        membuffer_append_char(mb, ']');
    }

    membuffer_append_char(mb, '[');
    for (int i = 0; i < num_terminals; i++)
    {
        if (terminal_bitset_test(ps, set, i))
        {
            if (!first) membuffer_append(mb, " "); else first = false;
            print_symbol(mb, term_get(ps, i), false);
        }
    }
    membuffer_append_char(mb, ']');
}

#endif
