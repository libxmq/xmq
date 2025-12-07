package org.libxmq.imp;

abstract class XMQParseCallbacks
{
    protected abstract void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_key(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_name(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_ns(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_colon(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_apar_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_cpar_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_cpar_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix);
    protected abstract void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix);
}
