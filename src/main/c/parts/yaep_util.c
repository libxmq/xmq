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

#include "yaep_structs.h"
#include "yaep_util.h"
#include "yaep_symbols.h"
#include "yaep_terminal_bitset.h"
#include "yaep_tree.h"
#include "yaep_print.h"

#endif

#ifdef YAEP_UTIL_MODULE

void dbg_breakpoint()
{
}

void dbg_print_core(YaepParseState *ps, YaepStateSetCore *c)
{
    MemBuffer *mb = new_membuffer();
    print_core(mb, c);
    free_membuffer_and_free_content(mb);
}

void dbg_print_coresymbvects(YaepParseState *ps, YaepCoreSymbToPredComps *v)
{
    MemBuffer *mb = new_membuffer();
    print_coresymbvects(mb, ps, v);
    free_membuffer_and_free_content(mb);
}

void dbg_print_dotted_rule(YaepParseState *ps, YaepDottedRule *dotted_rule)
{
    MemBuffer *mb = new_membuffer();
    print_dotted_rule(mb, ps, 0 /*from_i*/, dotted_rule, 0, 0);
    membuffer_append_null(mb);
    puts(mb->buffer_);
    free_membuffer_and_free_content(mb);
}

void fetch_state_vars(YaepParseState *ps,
                      YaepStateSet *state_set,
                      StateVars *out)
{
    if (state_set == NULL && !ps->new_set_ready_p)
    {
        /* The following is necessary if we call the function from a
           debugger.  In this case new_set, new_core and their members
           may be not set up yet. */
        out->state_id = -1;
        out->core_id = -1;
        out->num_started_dotted_rules = out->num_dotted_rules
            = out->num_all_matched_lengths = ps->new_num_leading_dotted_rules;
        out->dotted_rules = ps->new_dotted_rules;
        out->matched_lengths = ps->new_matched_lengths;
        out->to_parent_rule_index = NULL;
        return;
    }

    out->state_id = state_set->id;
    out->core_id = state_set->core->id;
    out->num_dotted_rules = state_set->core->num_dotted_rules;
    out->dotted_rules = state_set->core->dotted_rules;
    out->num_started_dotted_rules = state_set->core->num_started_dotted_rules;
    out->matched_lengths = state_set->matched_lengths;
    out->num_all_matched_lengths = state_set->core->num_all_matched_lengths;
    out->to_parent_rule_index = state_set->core->to_parent_rule_index;
    out->num_started_dotted_rules = state_set->core->num_started_dotted_rules;
}

int find_matched_length(YaepParseState *ps,
                        YaepStateSet *state_set,
                        StateVars *vars,
                        int rule_index_in_core)
{
    int matched_length;

    if (rule_index_in_core >= vars->num_all_matched_lengths)
    {
        // No match yet.
        matched_length = 0;
    }
    else if (rule_index_in_core< vars->num_started_dotted_rules)
    {
        matched_length = vars->matched_lengths[rule_index_in_core];
    }
    else
    {
        matched_length = vars->matched_lengths[vars->to_parent_rule_index[rule_index_in_core]];
    }

    return matched_length;
}

void yaep_debug(YaepParseState *ps, const char *format, ...)
{
    if (!ps->run.debug) return;

    va_list ap;
    va_start(ap, format);

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "@%d ", ps->tok_i);

    char *buf = buf_vsnprintf(format, ap);
    membuffer_append(mb, buf);
    membuffer_append_null(mb);
    free(buf);

    debug_mb("ixml.pa.debug=", mb);
    free_membuffer_and_free_content(mb);
    return;
}

void yaep_trace(YaepParseState *ps, const char *format, ...)
{
    if (!ps->run.trace) return;

    va_list ap;
    va_start(ap, format);

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "@%d ", ps->tok_i);

    char *buf = buf_vsnprintf(format, ap);
    membuffer_append(mb, buf);
    membuffer_append_null(mb);
    free(buf);

    debug_mb("ixml.pa.trace=", mb);
    free_membuffer_and_free_content(mb);
    return;
}

void yaep_view(YaepParseState *ps, const char *format, ...)
{
    if (!ps->run.debug) return;

    va_list ap;
    va_start(ap, format);

    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "@%d ", ps->tok_i);

    char *buf = buf_vsnprintf(format, ap);
    membuffer_append(mb, buf);
    membuffer_append_null(mb);
    free(buf);

    debug_mb("ixml.pa.view=", mb);
    free_membuffer_and_free_content(mb);
    return;
}

#endif
