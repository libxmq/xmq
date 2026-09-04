package org.libxmq.imp;

/**
 * Callbacks invoked while parsing xmq input.
 */
abstract class XMQParseCallbacks
{
    /**
     * Invoked when whitespace is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a quote is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a quoted element value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a compound element value quote is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a quoted attribute value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a compound attribute value quote is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an entity is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an entity inside an element value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an entity inside a compound element value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an entity inside an attribute value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an entity inside a compound attribute value is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a comment is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a continued comment is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an element key is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_key(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an element name is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_name(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an element namespace is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_ns(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a colon is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_colon(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a left parenthesis is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_apar_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a right parenthesis is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a left square bracket is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_cpar_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a right square bracket is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_cpar_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a left brace is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a right brace is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an equals sign is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an equals sign completes a construct.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when unquoted attribute value text is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when unquoted element value text is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a namespace declaration is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an attribute key is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when a namespace colon is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix);
    /**
     * Invoked when an attribute namespace is parsed.
     * @param start_line The 0-based starting line.
     * @param start_col The 0-based starting column.
     * @param start The starting character index.
     * @param stop The stopping character index.
     * @param stop_suffix The index after the suffix of the token.
     */
    protected abstract void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix);
}
