/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

package org.libxmq.imp;

/**
 * Tokenizes xmq and prints every token wrapped in the theme strings of its
 * color, like the C tokenize_* callbacks (xmqSetupParseCallbacksColorizeTokens)
 * used by tokenize --type=terminal, --type=html and --type=tex.
 */
class XMQParseIntoColoredTokens extends XMQParser
{
    final StringBuilder out = new StringBuilder();
    final XMQTheme theme;
    final XMQPrintState.RenderTo render_to;

    XMQParseIntoColoredTokens(XMQTheme theme, XMQPrintState.RenderTo render_to)
    {
        this.theme = theme;
        this.render_to = render_to;
    }

    /** Mirrors C tokenize_TYPE: pre, the escaped token text, post. */
    void token(XMQColor color, int start, int stop)
    {
        XMQThemeStrings ts = color == null ? null : theme.getStringsFor(color);
        if (ts != null && ts.pre() != null) out.append(ts.pre());
        if (render_to == XMQPrintState.RenderTo.HTML) writeSafeHtml(start, stop);
        else if (render_to == XMQPrintState.RenderTo.TEX) writeSafeTex(start, stop);
        else out.append(buffer_, start, stop);
        if (ts != null && ts.post() != null) out.append(ts.post());
    }

    /** Mirrors C write_safe_html. */
    void writeSafeHtml(int start, int stop)
    {
        for (int i = start; i < stop; ++i)
        {
            char c = buffer_.charAt(i);
            if (c == '&') out.append("&amp;");
            else if (c == '<') out.append("&lt;");
            else if (c == '>') out.append("&gt;");
            else if (c == '"') out.append("&quot;");
            else out.append(c);
        }
    }

    /** Mirrors C write_safe_tex. */
    void writeSafeTex(int start, int stop)
    {
        for (int i = start; i < stop; ++i)
        {
            char c = buffer_.charAt(i);
            if (c == '&') out.append("\\&");
            else if (c == '\\') out.append("\\\\");
            else if (c == '_') out.append("\\_");
            else out.append(c);
        }
    }

    protected void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_whitespace, start, stop);
    }

    protected void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_quote, start, stop);
    }

    protected void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_value_quote, start, stop);
    }

    protected void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_value_compound_quote, start, stop);
    }

    protected void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_value_quote, start, stop);
    }

    protected void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_value_compound_quote, start, stop);
    }

    protected void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_entity, start, stop);
    }

    protected void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_value_entity, start, stop);
    }

    protected void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_value_compound_entity, start, stop);
    }

    protected void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_value_entity, start, stop);
    }

    protected void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_value_compound_entity, start, stop);
    }

    protected void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_comment, start, stop);
    }

    protected void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_comment_continuation, start, stop);
    }

    protected void do_element_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_key, start, stop);
    }

    protected void do_element_name(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_name, start, stop);
    }

    protected void do_element_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_ns, start, stop);
    }

    protected void do_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        // Not a token in C, nothing is printed.
    }

    protected void do_apar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_apar_left, start, stop);
    }

    protected void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_apar_right, start, stop);
    }

    protected void do_cpar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_cpar_left, start, stop);
    }

    protected void do_cpar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_cpar_right, start, stop);
    }

    protected void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_brace_left, start, stop);
    }

    protected void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_brace_right, start, stop);
    }

    protected void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_equals, start, stop);
    }

    protected void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        // Not a token in C, nothing is printed.
    }

    protected void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_value_text, start, stop);
    }

    protected void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_element_value_text, start, stop);
    }

    protected void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_ns_declaration, start, stop);
    }

    protected void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_key, start, stop);
    }

    protected void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_ns_colon, start, stop);
    }

    protected void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        token(XMQColor.COLOR_attr_ns, start, stop);
    }
}
