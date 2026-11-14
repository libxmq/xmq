/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDtd;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlSave;

import java.util.List;



/**
 * Prints a document object model in xmq.
 */
public class XMQPrinter
{
    /**
     * Creates the printer.
     */
    public XMQPrinter()
    {
    }

    /**
     * A parse error raised while printing.
     */
    public class ParseException extends Exception
    {
        /**
         * Creates the exception.
         */
        public ParseException()
        {
        }
    }

    static void print_string(XMQPrintState ps, String s)
    {
        ps.buffer.append(s);
        if (s.length() > 0)
        {
            ps.last_char = s.charAt(s.length() - 1);
        }
        ps.current_indent += s.length();
    }

    /** The theme pre string of a color, or null. Mirrors C getThemeStrings. */
    static String color_pre(XMQPrintState ps, XMQColor color)
    {
        if (color == null || ps.theme == null) return null;
        XMQThemeStrings ts = ps.theme.getStringsFor(color);
        return ts == null ? null : ts.pre();
    }

    /** The theme post string of a color, or null. Mirrors C getThemeStrings. */
    static String color_post(XMQPrintState ps, XMQColor color)
    {
        if (color == null || ps.theme == null) return null;
        XMQThemeStrings ts = ps.theme.getStringsFor(color);
        return ts == null ? null : ts.post();
    }

    /** Mirrors the C content write: a raw write that does not track the indentation. */
    static void write(XMQPrintState ps, String s)
    {
        if (s != null) ps.buffer.append(s);
    }

    /** Mirrors C print_color_pre: writes the pre string of the color, if any. */
    void print_color_pre(XMQPrintState ps, XMQColor color)
    {
        write(ps, color_pre(ps, color));
    }

    /**
     * Mirrors C print_color_post: writes the post string of the color. When
     * the theme has no post string (eg ansi colors) the active color is
     * replayed instead.
     */
    void print_color_post(XMQPrintState ps, XMQColor color)
    {
        String post = color_post(ps, color);
        if (post != null) write(ps, post);
        else write(ps, ps.replay_active_color_pre);
    }

    /** Mirrors C needs_escape: the escape of a character when rendering html or tex, or null. */
    static String needs_escape(XMQPrintState.RenderTo f, int c)
    {
        if (f == XMQPrintState.RenderTo.HTML)
        {
            if (c == '&') return "&amp;";
            if (c == '<') return "&lt;";
            if (c == '>') return "&gt;";
            return null;
        }
        if (f == XMQPrintState.RenderTo.TEX)
        {
            if (c == '\\') return "\\backslash;";
            if (c == '&') return "\\&";
            if (c == '#') return "\\#";
            if (c == '{') return "\\{";
            if (c == '}') return "\\}";
            if (c == '_') return "\\_";
            if (c == '\'') return "{'}";
        }
        return null;
    }

    /** Mirrors C is_unicode_whitespace: nbsp and the en/em quads and spaces. */
    static boolean is_unicode_whitespace(int c)
    {
        return c == 0xa0 || (c >= 0x2000 && c <= 0x2003);
    }

    /**
     * Mirrors C print_utf8_char: prints the character at index i, with the
     * explicit space/tab strings and the render escapes. Returns the number of
     * chars used.
     */
    int print_utf8_char(XMQPrintState ps, String s, int i)
    {
        int c = s.codePointAt(i);
        boolean uw = is_unicode_whitespace(c);
        if (uw) print_color_pre(ps, XMQColor.COLOR_unicode_whitespace);
        if (c == ' ') write(ps, ps.explicit_space);
        else if (c == '\t') write(ps, ps.explicit_tab);
        else
        {
            String e = needs_escape(ps.render_to, c);
            if (e == null) ps.buffer.appendCodePoint(c);
            else write(ps, e);
        }
        if (uw) print_color_post(ps, XMQColor.COLOR_unicode_whitespace);
        ps.last_char = s.charAt(i);
        ps.current_indent++;
        return Character.charCount(c);
    }

    /** Mirrors C print_utf8_internal: prints a string, see print_utf8_char. */
    void print_utf8_internal(XMQPrintState ps, String s)
    {
        if (s == null || s.isEmpty()) return;
        int u_len = 0;
        for (int i = 0; i < s.length(); )
        {
            int c = s.codePointAt(i);
            boolean uw = is_unicode_whitespace(c);
            if (uw) print_color_pre(ps, XMQColor.COLOR_unicode_whitespace);
            if (c == ' ') write(ps, ps.explicit_space);
            else if (c == '\t') write(ps, ps.explicit_tab);
            else
            {
                String e = needs_escape(ps.render_to, c);
                if (e == null) ps.buffer.appendCodePoint(c);
                else write(ps, e);
            }
            if (uw) print_color_post(ps, XMQColor.COLOR_unicode_whitespace);
            u_len++;
            i += Character.charCount(c);
        }
        ps.last_char = s.charAt(s.length() - 1);
        ps.current_indent += u_len;
    }

    /**
     * Mirrors C print_utf8: prints the strings wrapped in the color. While
     * printing, the pre string of the color is the active color that is
     * replayed by a nested color without a post string (ansi colors).
     */
    void print_utf8(XMQPrintState ps, XMQColor color, String... parts)
    {
        String pre = color_pre(ps, color);
        String post = color_post(ps, color);
        String previous_color = null;
        if (pre != null)
        {
            write(ps, pre);
            previous_color = ps.replay_active_color_pre;
            ps.replay_active_color_pre = pre;
        }
        for (String p : parts) print_utf8_internal(ps, p);
        if (post != null) write(ps, post);
        if (previous_color != null) ps.replay_active_color_pre = previous_color;
    }

    /**
     * Mirrors C xmq_print_xmq: prints a document wrapped in the theme's
     * document/header/style/body/content pre/post strings. Used by the render
     * commands (render-html, render-terminal, render-tex). In the default
     * to-xmq path the theme has none of these strings set, so this just
     * prints the nodes.
     */
    void print_rendered(XMQPrintState ps, XmlDocument doc)
    {
        XMQTheme theme = ps.theme;
        if (theme.document != null && theme.document.pre()  != null) ps.buffer.append(theme.document.pre());
        if (theme.header   != null && theme.header.pre()    != null) ps.buffer.append(theme.header.pre());
        if (theme.style    != null && theme.style.pre()     != null) ps.buffer.append(theme.style.pre());
        if (theme.header   != null && theme.header.post()   != null) ps.buffer.append(theme.header.post());
        if (theme.body     != null && theme.body.pre()      != null) ps.buffer.append(theme.body.pre());
        if (theme.content  != null && theme.content.pre()   != null) ps.buffer.append(theme.content.pre());
        print_nodes(ps, doc.firstChild());
        if (theme.content  != null && theme.content.post()  != null) ps.buffer.append(theme.content.post());
        if (theme.body     != null && theme.body.post()     != null) ps.buffer.append(theme.body.post());
        if (theme.document != null && theme.document.post() != null) ps.buffer.append(theme.document.post());
    }

    /** Mirrors C check_space_before_entity_node. */
    void check_space_before_doctype_node(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == '(') return;
        if (!ps.output_settings.compact() && c != '=')
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_entity(ps))
        {
            print_white_spaces(ps, 1);
        }
    }

    /** Mirrors C check_space_before_entity_node. */
    void check_space_before_entity_node(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == '(') return;
        if (!ps.output_settings.compact() && c != '=')
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_entity(ps))
        {
            print_white_spaces(ps, 1);
        }
    }

    /** Mirrors C check_space_before_quote. */
    void check_space_before_quote(XMQPrintState ps, Level level)
    {
        char c = ps.last_char;
        if (c == 0) return;
        if (!ps.output_settings.compact() && (c != '=' || level == Level.XMQ) && c != '(')
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_quote(c))
        {
            print_white_spaces(ps, 1);
        }
    }

    /** Mirrors C need_separation_before_quote: previous char is a quote. */
    static boolean need_separation_before_quote(char c)
    {
        return c == '\'' || c == '"';
    }

    /** Mirrors C print_white_spaces: indentation, printed with the indentation space string. */
    void print_white_spaces(XMQPrintState ps, int num)
    {
        XMQTheme c = ps.theme;
        if (c != null && c.whitespace != null) write(ps, c.whitespace.pre());
        for (int i = 0; i < num; ++i) write(ps, ps.indentation_space);
        ps.current_indent += num;
        if (c != null && c.whitespace != null) write(ps, c.whitespace.post());
    }

    /** Mirrors C print_nl. */
    void print_nl(XMQPrintState ps, String prefix, String postfix)
    {
        write(ps, postfix);
        write(ps, ps.explicit_nl);
        ps.current_indent = 0;
        ps.last_char = 0;
        write(ps, ps.restart_line);
        write(ps, prefix);
    }

    /** Mirrors C print_nl_and_indent. */
    void print_nl_and_indent(XMQPrintState ps, String prefix, String postfix)
    {
        write(ps, postfix);
        write(ps, ps.explicit_nl);
        ps.current_indent = 0;
        ps.last_char = 0;
        print_white_spaces(ps, ps.line_indent);
        write(ps, ps.restart_line);
        write(ps, prefix);
    }

    /** A newline and indentation before an element key, unless just started a line. */
    void check_space_before_key(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == 0) return;

        if (!ps.output_settings.compact())
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_element_name(ps))
        {
            print_white_spaces(ps, 1);
        }
    }

    static boolean need_separation_before_element_name(XMQPrintState ps)
    {
        char c = ps.last_char;
        return c != 0
                && c != '\''
                && c != '"'
                && c != '{'
                && c != '}'
                && c != ';'
                && c != ')'
                && c != '/';
    }

    static boolean need_separation_before_entity(XMQPrintState ps)
    {
        char c = ps.last_char;
        return c != 0
                && c != '='
                && c != '\''
                && c != '"'
                && c != '{'
                && c != '}'
                && c != ';'
                && c != '('
                && c != ')';
    }

    void check_space_before_attribute(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == '(') return;
        if (!ps.output_settings.compact())
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_attribute_key(ps))
        {
            print_white_spaces(ps, 1);
        }
    }

    /** Mirrors C need_separation_before_attribute_key. No space is needed after
     *  a quote, an attribute start, a compound end or an entity, ie 'x y z'key= */
    static boolean need_separation_before_attribute_key(XMQPrintState ps)
    {
        char c = ps.last_char;
        return c != 0 && c != '\'' && c != '"' && c != '(' && c != ')' && c != ';';
    }

    void check_space_before_opening_brace(XMQPrintState ps)
    {
        char c = ps.last_char;

        if (!ps.output_settings.compact())
        {
            if (c == ')')
            {
                print_nl_and_indent(ps, null, null);
            }
            else
            {
                print_white_spaces(ps, 1);
            }
        }
    }

    void check_space_before_closing_brace(XMQPrintState ps)
    {
        if (!ps.output_settings.compact())
        {
            print_nl_and_indent(ps, null, null);
        }
    }

    /** Mirrors C find_attr_key_max_u_width and find_namespace_max_u_width. */
    static int find_attr_key_max_u_width(XmlElement element)
    {
        int max = 0;
        for (XmlAttr a : element.attributes())
        {
            int len = u_len(a.fullName());
            if (len > max) max = len;
        }
        for (XmlNs ns : element.nsDefs())
        {
            int len = 5 + (ns.prefix() != null ? u_len(ns.prefix()) + 1 : 0);
            if (len > max) max = len;
        }
        return max;
    }

    /** The number of unicode code points in a string, like C str_b_u_len. */
    static int u_len(String s)
    {
        return s.codePointCount(0, s.length());
    }

    /** Mirrors C print_attributes. */
    void print_attributes(XMQPrintState ps, XmlElement element)
    {
        int max = 0;
        if (!ps.output_settings.compact()) max = find_attr_key_max_u_width(element);

        int line_indent = ps.line_indent;
        ps.line_indent = ps.current_indent;
        for (XmlAttr a : element.attributes())
        {
            print_attribute(ps, a, max);
        }
        for (XmlNs ns : element.nsDefs())
        {
            print_namespace_declaration(ps, ns, max);
        }
        ps.line_indent = line_indent;
    }

    /** Mirrors C is_single_empty_text_node. */
    static boolean is_single_empty_text_node(XmlNode node)
    {
        if (node.type() != XmlNodeType.TEXT) return false;
        String c = node.content();
        return c == null || c.isEmpty();
    }

    /** Mirrors C print_attribute. */
    void print_attribute(XMQPrintState ps, XmlAttr a, int align)
    {
        check_space_before_attribute(ps);

        String key = a.name();
        String prefix = (a.ns() != null) ? a.ns().prefix() : null;
        int total_u_len = u_len(key) + (prefix != null ? u_len(prefix) + 1 : 0);

        if (prefix != null)
        {
            print_utf8(ps, XMQColor.COLOR_attr_ns, prefix);
            print_utf8(ps, XMQColor.COLOR_ns_colon, ":");
        }
        print_utf8(ps, XMQColor.COLOR_attr_key, key);

        if (a.firstChild() != null && !is_single_empty_text_node(a.firstChild()))
        {
            if (!ps.output_settings.compact()) print_white_spaces(ps, 1 + align - total_u_len);
            print_utf8(ps, XMQColor.COLOR_equals, "=");
            if (!ps.output_settings.compact()) print_white_spaces(ps, 1);
            print_value(ps, a.firstChild(), Level.ATTR_VALUE);
        }
    }

    /** Mirrors C print_namespace_declaration. */
    void print_namespace_declaration(XMQPrintState ps, XmlNs ns, int align)
    {
        check_space_before_attribute(ps);

        String prefix = ns.prefix();
        int total_u_len = 5 + (prefix != null ? u_len(prefix) + 1 : 0);

        print_utf8(ps, XMQColor.COLOR_ns_declaration, "xmlns");

        if (prefix != null)
        {
            print_utf8(ps, XMQColor.COLOR_ns_colon, ":");
            XMQColor ns_color = prefix.equals("xsl") ? XMQColor.COLOR_ns_override_xsl : XMQColor.COLOR_attr_ns;
            print_utf8(ps, ns_color, prefix);
        }

        String v = ns.href();
        if (v != null)
        {
            if (!ps.output_settings.compact()) print_white_spaces(ps, 1 + align - total_u_len);
            print_utf8(ps, XMQColor.COLOR_equals, "=");
            if (!ps.output_settings.compact()) print_white_spaces(ps, 1);
            print_value_string(ps, v, Level.ATTR_VALUE, false);
        }
    }

    /** Mirrors C print_content_node. */
    void print_content_node(XMQPrintState ps, XmlNode node)
    {
        // Do NOT trim: leading and ending spaces can be part of the value,
        // ie. key = ' x y z '
        String t = node.content();
        print_value_string(ps, t == null ? "" : t, Level.XMQ, false);
    }

    /** Mirrors C print_entity_node. Prints &amp;name; */
    void print_entity_node(XMQPrintState ps, XmlNode node)
    {
        check_space_before_entity_node(ps);
        String name = node.name();
        XMQColor c = XMQColor.COLOR_entity;
        if (name.isEmpty() || name.charAt(0) == '_') c = XMQColor.COLOR_quote;
        print_utf8(ps, c, "&");
        print_utf8(ps, c, name);
        print_utf8(ps, c, ";");
    }

    /** Mirrors C is_safe_value_char/count_whitespace. True if the character does not need quoting. */
    static boolean is_safe_value_char(char c)
    {
        return c != ' ' && c != '\n' && c != '\t' && c != '\r'
            && c != '\u00a0'
            && c != '\u2000' && c != '\u2001' && c != '\u2002' && c != '\u2003'
            && c != '(' && c != ')' && c != '{' && c != '}'
            && c != '\'' && c != '"';
    }

    /** Mirrors C unsafe_value_start. True if the value cannot start with these characters. */
    static boolean is_unsafe_value_start(String s)
    {
        if (s.isEmpty())
        {
            return true;
        }
        char c = s.charAt(0);
        char cc = s.length() > 1 ? s.charAt(1) : 0;
        return c == '&' || c == '=' || (c == '/' && (cc == '/' || cc == '*'));
    }

    /** Mirrors C is_xmq_text_value. True if the value can be printed without quotes. */
    static boolean is_xmq_text_value(String s)
    {
        if (is_unsafe_value_start(s))
        {
            return false;
        }
        for (int i = 0; i < s.length(); i++)
        {
            if (!is_safe_value_char(s.charAt(i)))
            {
                return false;
            }
        }
        return true;
    }

    /** Mirrors C count_necessary_quotes.
     *  Returns the number of quotes needed, and sets the output arrays
     *  use_double_quotes and add_nls (multi-line quote). */
    static int count_necessary_quotes(String s, boolean prefer_double_quotes, boolean[] add_nls, boolean[] add_compound, boolean[] use_double_quotes)
    {
        boolean all_safe = !is_unsafe_value_start(s);

        // Leading/ending whitespace mixed with newlines force a compound.
        int[] ls = has_leading_space_nl(s);
        int[] es = has_ending_nl_space(s, 0, s.length());
        add_compound[0] = (ls != null && ls[1] == 0) || (es != null && es[1] == 0);

        int max_single = 0;
        int curr_single = 0;
        int max_double = 0;
        int curr_double = 0;

        for (char c : s.toCharArray())
        {
            if (!is_safe_value_char(c))
            {
                all_safe = false;
            }
            if (c == '\'')
            {
                curr_single++;
                if (curr_single > max_single) max_single = curr_single;
            }
            else
            {
                curr_single = 0;
                if (c == '"')
                {
                    curr_double++;
                    if (curr_double > max_double) max_double = curr_double;
                }
                else
                {
                    curr_double = 0;
                }
            }
        }

        boolean leading_ending_sqs = s.charAt(0) == '\'' || s.charAt(s.length() - 1) == '\'';
        boolean leading_ending_dqs = s.charAt(0) == '"' || s.charAt(s.length() - 1) == '"';

        boolean use_dqs = prefer_double_quotes;
        if (leading_ending_sqs && !leading_ending_dqs)
        {
            // If there is a leading or ending single quote, then use double quotes.
            use_dqs = true;
        }
        else if (!leading_ending_sqs && leading_ending_dqs)
        {
            // If there are leading or ending double quotes, then use single quotes.
            use_dqs = false;
        }
        else if (max_double > max_single && max_double > 0)
        {
            // We have more doubles than singles, use single quotes.
            use_dqs = false;
        }
        else if (max_double < max_single)
        {
            // We have fewer doubles than singles, use double quotes.
            use_dqs = true;
        }
        else if (max_double > 0)
        {
            // Equal number of quotes, then always use single quotes.
            use_dqs = false;
        }

        int max = use_dqs ? max_double : max_single;
        // We found x quotes, thus we need x+1 quotes to quote them.
        if (max > 0) max++;
        // Content contains no quotes ', but has unsafe chars, a single quote is enough.
        if (max == 0 && !all_safe) max = 1;
        // Content contains two sequential '' quotes, must bump number of required quotes to 3.
        if (max == 2) max = 3;

        // If the value has a leading or ending quote of the same kind as
        // chosen, then we need the multi-line quote format.
        add_nls[0] = (use_dqs && leading_ending_dqs)
            || (!use_dqs && leading_ending_sqs);

        use_double_quotes[0] = use_dqs;
        return max;
    }

    /** Mirrors C Level. */
    enum Level { XMQ, ELEMENT_VALUE, ELEMENT_VALUE_COMPOUND, ATTR_VALUE, ATTR_VALUE_COMPOUND }

    /** Mirrors C enter_compound_level. */
    static Level enter_compound_level(Level l)
    {
        if (l == Level.ELEMENT_VALUE) return Level.ELEMENT_VALUE_COMPOUND;
        if (l == Level.ATTR_VALUE) return Level.ATTR_VALUE_COMPOUND;
        return l;
    }

    /** Mirrors C level_to_quote_color. */
    static XMQColor level_quote_color(Level level)
    {
        switch (level)
        {
            case ELEMENT_VALUE: return XMQColor.COLOR_element_value_quote;
            case ATTR_VALUE:    return XMQColor.COLOR_attr_value_quote;
            case ELEMENT_VALUE_COMPOUND: return XMQColor.COLOR_element_value_compound_quote;
            case ATTR_VALUE_COMPOUND: return XMQColor.COLOR_attr_value_compound_quote;
            case XMQ:
            default:            return XMQColor.COLOR_quote;
        }
    }

    /** Mirrors C level_to_entity_color. */
    static XMQColor level_entity_color(Level level)
    {
        switch (level)
        {
            case ELEMENT_VALUE: return XMQColor.COLOR_element_value_entity;
            case ATTR_VALUE:    return XMQColor.COLOR_attr_value_entity;
            case ELEMENT_VALUE_COMPOUND: return XMQColor.COLOR_element_value_compound_entity;
            case ATTR_VALUE_COMPOUND: return XMQColor.COLOR_attr_value_compound_entity;
            case XMQ:
            default:            return XMQColor.COLOR_entity;
        }
    }

    /** Mirrors C print_quotes. Prints num quotes of the chosen kind, wrapped in the color. */
    void print_quotes(XMQPrintState ps, int num, XMQColor color, boolean use_double_quotes)
    {
        // Raw writes of the quotes, like C print_quotes.
        String q = use_double_quotes ? "\"" : "'";
        write(ps, color_pre(ps, color));
        write(ps, q.repeat(num));
        ps.current_indent += num;
        ps.last_char = q.charAt(0);
        write(ps, color_post(ps, color));
    }

    /** Mirrors C print_quoted_spaces. Prints '...spaces...', wrapped in the color. */
    void print_quoted_spaces(XMQPrintState ps, XMQColor color, int num)
    {
        // Like C print_quoted_spaces this always uses the quote color.
        XMQThemeStrings q = ps.theme == null ? null : ps.theme.quote;
        if (q != null) write(ps, q.pre());
        write(ps, "'");
        for (int i = 0; i < num; ++i) write(ps, ps.explicit_space);
        ps.current_indent += num;
        ps.last_char = '\'';
        write(ps, "'");
        if (q != null) write(ps, q.post());
    }

    /** Mirrors C print_char_entity for a single character, wrapped in the color. */
    void print_char_entity(XMQPrintState ps, XMQColor color, int c)
    {
        String replacement = null;
        if (ps.output_settings.escapeNon7bit() && ps.output_settings.htmq())
        {
            replacement = XMQHtmlEntities.toHtmlEntity(c);
        }
        String buf = replacement != null ? "&" + replacement + ";" : "&#" + c + ";";
        // Like C print_char_entity, the entity text is printed (and escaped) without a color of its own.
        write(ps, color_pre(ps, color));
        print_utf8(ps, null, buf);
        write(ps, color_post(ps, color));
        ps.last_char = ';';
        ps.current_indent += buf.length();
    }

    /** Mirrors C print_all_whitespace. Spaces are printed as '...'. Other whitespace as entities. */
    void print_all_whitespace(XMQPrintState ps, String s, Level level)
    {
        XMQColor qc = level_quote_color(level);
        XMQColor ec = level_entity_color(level);
        int i = 0;
        while (i < s.length())
        {
            if (s.charAt(i) == ' ')
            {
                int j = i;
                while (j < s.length() && s.charAt(j) == ' ')
                {
                    j++;
                }
                check_space_before_quote(ps, level);
                print_quoted_spaces(ps, qc, j - i);
                i = j;
            }
            else
            {
                check_space_before_entity_node(ps);
                print_char_entity(ps, ec, s.charAt(i));
                i++;
            }
        }
    }

    /** Mirrors C is_xml_whitespace (exactly space, tab, newline, CR). */
    static boolean is_xml_whitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /** Mirrors C has_all_quotes. All single or all double quotes. */
    static boolean has_all_quotes(String v)
    {
        if (v.isEmpty())
        {
            return false;
        }
        char first = v.charAt(0);
        if (first != '\'' && first != '"')
        {
            return false;
        }
        for (int i = 1; i < v.length(); i++)
        {
            if (v.charAt(i) != first)
            {
                return false;
            }
        }
        return true;
    }

    /** Mirrors C has_all_whitespace.
     *  Returns [all_whitespace, all_space, only_newlines] as 0/1. */
    static int[] has_all_whitespace(String v)
    {
        boolean all_ws = true, all_space = true, only_nls = true;
        for (int i = 0; i < v.length(); i++)
        {
            char c = v.charAt(i);
            if (!is_xml_whitespace(c))
            {
                all_ws = false;
                all_space = false;
                only_nls = false;
                break;
            }
            if (c != ' ') all_space = false;
            if (c != '\n') only_nls = false;
        }
        return new int[] { all_ws ? 1 : 0, all_space ? 1 : 0, only_nls ? 1 : 0 };
    }

    /** Mirrors C has_leading_space_nl.
     *  Returns null if no leading whitespace contains newlines, else
     *  [end index of the leading ws run, only_newlines] where only_newlines
     *  is the number of leading newlines if the run is pure newlines, else 0. */
    static int[] has_leading_space_nl(String v)
    {
        int i = 0;
        int only_nls = 0;
        while (i < v.length() && v.charAt(i) == '\n')
        {
            i++;
            only_nls++;
        }
        int middle = only_nls > 0 ? i : -1;
        boolean found = only_nls > 0;
        while (i < v.length())
        {
            if (v.charAt(i) == '\n') found = true;
            if (!is_xml_whitespace(v.charAt(i)))
            {
                break;
            }
            i++;
        }
        if (!found)
        {
            return null;
        }
        return new int[] { i, middle == i ? only_nls : 0 };
    }

    /** Mirrors C has_ending_nl_space.
     *  Returns null if no ending whitespace contains newlines, else
     *  [new stop index, only_newlines] where only_newlines is the number of
     *  ending newlines if the run is pure newlines, else 0. */
    static int[] has_ending_nl_space(String v, int start, int stop)
    {
        if (start >= stop)
        {
            return null;
        }
        int i = stop;
        boolean found = false;
        int only_nls = 0;
        i--;
        while (i >= start && v.charAt(i) == '\n')
        {
            i--;
            only_nls++;
            found = true;
        }
        int middle = i;
        while (i >= start)
        {
            if (v.charAt(i) == '\n') found = true;
            if (!is_xml_whitespace(v.charAt(i)))
            {
                break;
            }
            i--;
        }
        if (!found)
        {
            return null;
        }
        int nls = middle == i ? only_nls : 0;
        i++;
        return new int[] { i, nls };
    }

    /** Mirrors C has_newlines. */
    static boolean has_newlines(String v)
    {
        for (int i = 0; i < v.length(); i++)
        {
            if (v.charAt(i) == '\n')
            {
                return true;
            }
        }
        return false;
    }

    /** Mirrors C has_leading_ending_different_quotes. */
    static boolean has_leading_ending_different_quotes(String v)
    {
        if (v.isEmpty())
        {
            return false;
        }
        char a = v.charAt(0);
        char b = v.charAt(v.length() - 1);
        return (a == '\'' && b == '"') || (a == '"' && b == '\'');
    }

    /** Mirrors C quote_needs_compounded. Sets use_dquotes[0] on return. */
    static boolean quote_needs_compounded(XMQPrintState ps, String v, boolean prefer_d, boolean[] use_dquotes)
    {
        boolean compact = ps.output_settings.compact();
        boolean escape_tabs = ps.output_settings.escapeTabs();

        if (v.length() == 1)
        {
            char c = v.charAt(0);
            if (c == '\'' || c == '\n' || c == '\r' || c == '\t')
            {
                return false;
            }
        }

        int[] ls = has_leading_space_nl(v);
        if (ls != null && ls[1] == 0)
        {
            return true;
        }
        int[] es = has_ending_nl_space(v, 0, v.length());
        if (es != null && es[1] == 0)
        {
            return true;
        }

        if (compact)
        {
            // In compact form newlines must be escaped: &#10;
            if (has_newlines(v))
            {
                return true;
            }
            // In compact form leading or ending different quotes trigger &#39; escapes.
            if (has_leading_ending_different_quotes(v))
            {
                return true;
            }
        }

        boolean newlines = ps.output_settings.escapeNewlines();
        boolean non7bit = ps.output_settings.escapeNon7bit();
        int num_sq = 0, num_dq = 0;
        boolean needs = false;

        for (int i = 0; i < v.length(); i++)
        {
            char c = v.charAt(i);
            if (c == '\'')
            {
                num_sq++;
                continue;
            }
            if (c == '"')
            {
                num_dq++;
                continue;
            }
            if (newlines && c == '\n')
            {
                needs = true;
                continue;
            }
            if (non7bit && c > 126)
            {
                needs = true;
                continue;
            }
            if (c < 32 && c != '\t' && c != '\n')
            {
                needs = true;
                continue;
            }
            if (c == '\t' && escape_tabs)
            {
                needs = true;
                continue;
            }
        }

        if (num_dq == 0 && num_sq == 0)
        {
            use_dquotes[0] = prefer_d;
        }
        else if (num_dq > 0 && num_sq > 0)
        {
            char a = v.charAt(0);
            char b = v.charAt(v.length() - 1);
            if (a == b)
            {
                if (a == '\'' || a == '"')
                {
                    use_dquotes[0] = (a == '\'');
                }
                else
                {
                    use_dquotes[0] = prefer_d;
                }
            }
            else
            {
                boolean aq = a == '\'' || a == '"';
                boolean bq = b == '\'' || b == '"';
                if (aq && bq)
                {
                    needs = true;
                    use_dquotes[0] = prefer_d;
                }
                else if (aq)
                {
                    use_dquotes[0] = (a == '\'');
                }
                else if (bq)
                {
                    use_dquotes[0] = (b == '\'');
                }
                else
                {
                    use_dquotes[0] = (num_sq > num_dq);
                }
            }
        }
        else
        {
            if (num_sq > 0) use_dquotes[0] = true;
            if (num_dq > 0) use_dquotes[0] = false;
        }
        return needs;
    }

    /** Mirrors C find_next_char_that_needs_escape.
     *  Returns index of first char at/after from that must be printed as a
     *  character entity, or stop if there is none. */
    static int find_next_char_that_needs_escape(XMQPrintState ps, String v, int from, int stop, boolean using_dquotes)
    {
        boolean compact = ps.output_settings.compact();
        boolean newlines = ps.output_settings.escapeNewlines();
        boolean escape_tabs = ps.output_settings.escapeTabs();
        boolean non7bit = ps.output_settings.escapeNon7bit();

        char q = using_dquotes ? '"' : '\'';
        if (from < stop && v.charAt(from) == q && compact)
        {
            return from;
        }
        int pre_stop = stop - 1;
        if (compact && pre_stop >= from && v.charAt(pre_stop) == q)
        {
            while (pre_stop > from && v.charAt(pre_stop) == q)
            {
                pre_stop--;
            }
            pre_stop++;
        }

        int i = from;
        while (i < stop)
        {
            char c = v.charAt(i);
            if ((compact && c == q && i == pre_stop)
                || (newlines && c == '\n')
                || (non7bit && c > 126)
                || (c < 32 && c != '\t' && c != '\n')
                || (c == '\t' && escape_tabs))
            {
                return i;
            }
            i++;
        }
        return i;
    }

    /** Mirrors C print_safe_leaf_quote. Prints one quoted leaf, possibly multiline. */
    void print_safe_leaf_quote(XMQPrintState ps, String v, XMQColor color)
    {
        boolean compact = ps.output_settings.compact();
        boolean add_nls = false;
        boolean[] udd = { false };
        boolean[] anls = { false };
        boolean[] acomp = { false };
        int numq = count_necessary_quotes(v, ps.output_settings.preferDoubleQuotes(), anls, acomp, udd);
        add_nls = anls[0];

        if (numq > 0 && !add_nls)
        {
            // Mirrors C print_safe_leaf_quote: a quote that requested its own
            // line (indent -1), or more than 3 quotes when not compact, is
            // printed with leading and ending newlines.
            if (ps.current_indent == -1 || !(numq < 4 || compact))
            {
                add_nls = true;
            }
        }

        if (numq == 0)
        {
            numq = 1;
        }

        int old_line_indent = 0;

        if (add_nls)
        {
            old_line_indent = ps.line_indent;
            ps.line_indent = ps.current_indent;
        }

        print_quotes(ps, numq, color, udd[0]);

        if (!add_nls)
        {
            old_line_indent = ps.line_indent;
            ps.line_indent = ps.current_indent;
        }

        if (add_nls)
        {
            print_nl_and_indent(ps, null, null);
        }

        print_quote_lines_and_color_uwhitespace(ps, color, v);

        if (!add_nls)
        {
            ps.line_indent = old_line_indent;
        }

        if (add_nls)
        {
            print_nl_and_indent(ps, null, null);
        }

        print_quotes(ps, numq, color, udd[0]);

        if (add_nls)
        {
            ps.line_indent = old_line_indent;
        }
    }

    /**
     * Mirrors C print_quote_lines_and_color_uwhitespace: prints the content
     * of a quote, each line wrapped in the color. A color without a post
     * string (ansi) is restarted on every new line.
     */
    void print_quote_lines_and_color_uwhitespace(XMQPrintState ps, XMQColor color, String v)
    {
        String pre = color_pre(ps, color);
        String post = color_post(ps, color);

        write(ps, pre);

        String old_restart_line = ps.restart_line;
        if (post == null) ps.restart_line = pre;
        else ps.restart_line = null;

        // We are leading with a newline, print an extra into the quote, which will be trimmed away during parse.
        if (v.charAt(0) == '\n')
        {
            print_nl(ps, pre, post);
        }

        boolean all_newlines = true;
        for (int i = 0; i < v.length(); )
        {
            if (v.charAt(i) == '\n')
            {
                if (i + 1 < v.length() && v.charAt(i + 1) != '\n')
                {
                    print_nl_and_indent(ps, pre, post);
                }
                else
                {
                    print_nl(ps, pre, post);
                }
                i++;
            }
            else
            {
                i += print_utf8_char(ps, v, i);
                all_newlines = false;
            }
        }
        // We are ending with a newline, print an extra into the quote, which will be trimmed away during parse.
        if (v.charAt(v.length() - 1) == '\n')
        {
            ps.line_indent--;
            if (!all_newlines)
            {
                print_nl_and_indent(ps, null, post);
            }
            else
            {
                ps.current_indent = 0;
                ps.last_char = 0;
                print_white_spaces(ps, ps.line_indent);
            }
            ps.line_indent++;
        }
        if (v.charAt(v.length() - 1) != '\n') write(ps, post);
        ps.restart_line = old_restart_line;
    }

    /** Mirrors C print_value_internal_text. */
    void print_value_internal_text(XMQPrintState ps, String v, Level level, boolean using_dquotes, boolean already)
    {
        XMQColor qc = level_quote_color(level);
        XMQColor ec = level_entity_color(level);

        if (v.isEmpty())
        {
            check_space_before_quote(ps, level);
            print_utf8(ps, qc, "''");
            return;
        }

        if (has_all_quotes(v))
        {
            // A text with all single quotes or all double quotes.
            check_space_before_quote(ps, level);
            boolean is_dq = v.charAt(0) == '"';
            print_quotes(ps, 1, qc, !is_dq);
            print_quotes(ps, v.length(), qc, is_dq);
            print_quotes(ps, 1, qc, !is_dq);
            return;
        }

        int[] ws = has_all_whitespace(v);
        if (ws[1] == 1)
        {
            // All normal spaces: '     '
            check_space_before_quote(ps, level);
            print_quoted_spaces(ps, qc, v.length());
            return;
        }
        if (ws[0] == 1)
        {
            if (ws[2] == 1 && !ps.output_settings.compact() && v.length() > 1)
            {
                // All newlines, more than one. This is printed as a multiline quote further on.
            }
            else
            {
                print_all_whitespace(ps, v, level);
                return;
            }
        }

        if (is_xmq_text_value(v) && (level == Level.ELEMENT_VALUE || level == Level.ATTR_VALUE))
        {
            // key = 123 or color=blue, no quoting needed.
            print_utf8(ps, qc, v);
            return;
        }

        int start = 0;
        int stop = v.length();

        int[] lead = has_leading_space_nl(v);
        if (lead != null && lead[1] == 0)
        {
            // Leading mix of newlines and whitespace: entities.
            print_all_whitespace(ps, v.substring(0, lead[0]), level);
            start = lead[0];
        }

        int[] tail = has_ending_nl_space(v, start, stop);
        int old_stop = stop;
        if (tail != null && tail[1] == 0)
        {
            // Ending mix of newlines and whitespace: entities after the quote.
            stop = tail[0];
            while (stop < old_stop && v.charAt(stop) == ' ')
            {
                stop++;
            }
        }

        for (int from = start; from < stop; )
        {
            int to = find_next_char_that_needs_escape(ps, v, from, stop, using_dquotes);
            if (from == to)
            {
                check_space_before_entity_node(ps);
                int c = v.codePointAt(from);
                print_char_entity(ps, ec, c);
                int from1 = from + Character.charCount(c);
                // C checks the byte after the escaped char, which is never a newline
                // after a multi byte utf8 char. So only ascii chars (eg CR) escape
                // the following newlines as well.
                while (c < 0x80 && from1 < stop && v.charAt(from1) == '\n')
                {
                    // Special case, eg CR LF: escape the newline(s) as well.
                    from1++;
                    check_space_before_entity_node(ps);
                    print_char_entity(ps, ec, '\n');
                }
                from = from1;
            }
            else
            {
                String seg = v.substring(from, to);
                boolean[] anls = { false }, acomp = { false }, udd = { false };
                count_necessary_quotes(seg, ps.output_settings.preferDoubleQuotes(), anls, acomp, udd);
                if (!acomp[0] && (!anls[0] || !ps.output_settings.compact()))
                {
                    check_space_before_quote(ps, level);
                    print_safe_leaf_quote(ps, seg, qc);
                }
                else
                {
                    print_value_string(ps, seg, level, already);
                }
                from = to;
            }
        }

        if (tail != null && tail[1] == 0)
        {
            // This trailing whitespace could not be printed inside the quote.
            print_all_whitespace(ps, v.substring(stop, old_stop), level);
        }
    }

    /** Mirrors C print_value for a start-stop string value. */
    void print_value_string(XMQPrintState ps, String v, Level level, boolean already_compounded)
    {
        boolean prefer_d = ps.output_settings.preferDoubleQuotes();
        boolean[] ud = { prefer_d };
        boolean is_compound = level != Level.XMQ && quote_needs_compounded(ps, v, prefer_d, ud);

        int old_line_indent = ps.line_indent;

        if (is_compound && !already_compounded)
        {
            level = enter_compound_level(level);
            print_utf8(ps, XMQColor.COLOR_cpar_left, "(");
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            ps.line_indent = ps.current_indent;
        }

        print_value_internal_text(ps, v, level, ud[0], is_compound);

        if (is_compound && !already_compounded)
        {
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            print_utf8(ps, XMQColor.COLOR_cpar_right, ")");
        }

        ps.line_indent = old_line_indent;
    }

    /** Mirrors C print_value for a node list value, ie the node and its following siblings. */
    void print_value(XMQPrintState ps, XmlNode node, Level level)
    {
        // Check if there are more than one part, if so the value has to be compounded.
        boolean is_compound = level != Level.XMQ && node != null && node.next() != null;
        boolean prefer_d = ps.output_settings.preferDoubleQuotes();
        boolean[] ud = { prefer_d };

        // Check if the single part will split into multiple parts and therefore needs to be compounded.
        if (level != Level.XMQ && !is_compound && node != null && !is_entity_node(node))
        {
            String t = node.content();
            is_compound = quote_needs_compounded(ps, t == null ? "" : t, prefer_d, ud);
        }

        int old_line_indent = ps.line_indent;

        if (is_compound)
        {
            level = enter_compound_level(level);
            print_utf8(ps, XMQColor.COLOR_cpar_left, "(");
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            ps.line_indent = ps.current_indent;
        }

        for (XmlNode i = node; i != null; i = i.next())
        {
            if (is_entity_node(i))
            {
                // Like C print_value_internal, an entity in a value is printed as an entity node.
                print_entity_node(ps, i);
            }
            else
            {
                String t = i.content();
                print_value_string(ps, t == null ? "" : t, level, is_compound);
            }
            if (level == Level.XMQ) break;
        }

        if (is_compound)
        {
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            print_utf8(ps, XMQColor.COLOR_cpar_right, ")");
        }

        ps.line_indent = old_line_indent;
    }

    /** Mirrors C print_element_name_and_attributes. */
    int print_element_name_and_attributes(XMQPrintState ps, XmlElement node)
    {
        XMQColor ns_color = XMQColor.COLOR_element_ns;
        XMQColor key_color = XMQColor.COLOR_element_key;
        XMQColor name_color = XMQColor.COLOR_element_name;

        check_space_before_key(ps);

        String name = node.name();
        String prefix = (node.ns() != null) ? node.ns().prefix() : null;
        int total_u_len = u_len(name) + (prefix != null ? u_len(prefix) + 1 : 0);

        if (prefix != null)
        {
            if (prefix.equals("xsl"))
            {
                key_color = XMQColor.COLOR_ns_override_xsl;
                name_color = XMQColor.COLOR_ns_override_xsl;
            }
            print_utf8(ps, ns_color, prefix);
            print_utf8(ps, XMQColor.COLOR_ns_colon, ":");
        }

        XMQColor c = (is_key_value_node(node) && node.attributes().isEmpty()) ? key_color : name_color;
        print_utf8(ps, c, name);

        if (!node.attributes().isEmpty() || !node.nsDefs().isEmpty())
        {
            print_utf8(ps, XMQColor.COLOR_apar_left, "(");
            print_attributes(ps, node);
            print_utf8(ps, XMQColor.COLOR_apar_right, ")");
        }
        return total_u_len;
    }

    /** Mirrors C print_key_node. */
    void print_key_node(XMQPrintState ps, XmlElement node, int align)
    {
        print_element_name_and_attributes(ps, node);

        if (!ps.output_settings.compact())
        {
            int len = ps.current_indent - ps.line_indent;
            int pad = 1;
            if (len < align) pad = 1 + align - len;
            print_white_spaces(ps, pad);
        }
        print_utf8(ps, XMQColor.COLOR_equals, "=");
        if (!ps.output_settings.compact()) print_white_spaces(ps, 1);

        print_value(ps, node.firstChild(), Level.ELEMENT_VALUE);
    }

    /** Mirrors C find_element_key_max_width. Scan nodes until there is a node which is
     *  not suitable for using the = sign, store it in restart[0] (or null). */
    static int find_element_key_max_width(XmlNode element, XmlNode[] restart)
    {
        int max = 0;
        XmlNode i = element;
        while (i != null)
        {
            if (!is_key_value_node(i) || !((XmlElement)i).attributes().isEmpty())
            {
                if (i == element) restart[0] = i.next();
                else restart[0] = i;
                return max;
            }
            XmlElement e = (XmlElement)i;
            String prefix = e.ns() != null ? e.ns().prefix() : null;
            int len = u_len(e.name()) + (prefix != null ? u_len(prefix) + 1 : 0);
            if (len > max) max = len;
            i = i.next();
        }
        restart[0] = null;
        return max;
    }

    /** Mirrors C print_nodes. Prints the node and all its following siblings,
     *  aligning the equal signs of runs of key=value nodes, unless compact. */
    void print_nodes(XMQPrintState ps, XmlNode from)
    {
        XmlNode i = from;
        XmlNode restart_find_at_node = from;
        int max = 0;

        while (i != null)
        {
            // We need to search ahead to find the max width of the node names so that we can align the equal signs.
            if (!ps.output_settings.compact() && i == restart_find_at_node)
            {
                XmlNode[] restart = new XmlNode[1];
                max = find_element_key_max_width(i, restart);
                restart_find_at_node = restart[0];
            }
            print_node(ps, i, max);
            i = i.next();
        }
    }

    /** Mirrors C print_element_with_children. */
    void print_element_with_children(XMQPrintState ps, XmlElement element, int align)
    {
        print_element_name_and_attributes(ps, element);

        check_space_before_opening_brace(ps);
        print_utf8(ps, XMQColor.COLOR_brace_left, "{");

        int old_line_indent = ps.line_indent;
        ps.line_indent += ps.output_settings != null ? ps.output_settings.indentAmount() : 4;

        print_nodes(ps, element.firstChild());

        ps.line_indent = old_line_indent;

        check_space_before_closing_brace(ps);
        print_utf8(ps, XMQColor.COLOR_brace_right, "}");
    }

    /** Mirrors C is_element_node. */
    static boolean is_element_node(XmlNode node)
    {
        return node.type() == XmlNodeType.ELEMENT;
    }

    /** Mirrors C is_content_node: a text or cdata node. */
    static boolean is_content_node(XmlNode node)
    {
        return node.type() == XmlNodeType.TEXT || node.type() == XmlNodeType.CDATA;
    }

    /** Mirrors C is_pi_node. */
    static boolean is_pi_node(XmlNode node)
    {
        return node.type() == XmlNodeType.PI;
    }

    /** Mirrors C is_doctype_node. */
    static boolean is_doctype_node(XmlNode node)
    {
        return node.type() == XmlNodeType.DTD;
    }

    /** Mirrors C is_key_value_node.
     *  Single content or entity child, or multiple text or entity children. */
    static boolean is_key_value_node(XmlNode node)
    {
        XmlNode from = node.firstChild();
        XmlNode to = node.lastChild();

        // Single content or entity node.
        if (from != null && from == to && (is_content_node(from) || is_entity_node(from))) return true;
        if (from == null) return false;

        // Multiple text or entity nodes.
        for (XmlNode i = from; i != null; i = i.next())
        {
            if (i.type() != XmlNodeType.TEXT && i.type() != XmlNodeType.ENTITY_REF) return false;
        }
        return true;
    }

    /** Mirrors C is_entity_node. */
    static boolean is_entity_node(XmlNode node)
    {
        return node.type() == XmlNodeType.ENTITY_REF;
    }

    /**
     * Recursively prints a node, like C print_node. A document prints all its
     * top level nodes.
     * @param ps The print state.
     * @param node The node to print.
     * @param align The alignment used for the equal signs.
     */
    /**
     * Print a node as xmq like the xmq command line does by default (indentation
     * 4, no colors), without a final newline. Used by generated parsers.
     * @param node The document or node.
     * @return The xmq text.
     */
    public static String toXmq(Object node)
    {
        XMQPrintState ps = new XMQPrintState();
        ps.defaultTheme();
        ps.output_settings = new org.libxmq.OutputSettings().setIndentAmount(4);
        new XMQPrinter().print_node(ps, node, 0);
        return ps.buffer.toString();
    }

    public void print_node(XMQPrintState ps, Object node, int align)
    {
        if (node instanceof XmlDocument d)
        {
            print_nodes(ps, d.firstChild());
            return;
        }
        XmlNode n = (XmlNode)node;
        if (is_content_node(n))
        {
            print_content_node(ps, n);
        }
        else if (is_entity_node(n))
        {
            print_entity_node(ps, n);
        }
        else if (is_comment_node(n))
        {
            print_comment_node(ps, n);
        }
        else if (is_pi_node(n))
        {
            print_pi_node(ps, n);
        }
        else if (is_doctype_node(n))
        {
            print_doctype(ps, (XmlDtd)n);
        }
        else if (n instanceof XmlElement e)
        {
            if (e.firstChild() == null)
            {
                // A node with no children, ie br
                print_element_name_and_attributes(ps, e);
            }
            else if (is_key_value_node(e))
            {
                // A key = value or key = 'value value' node.
                print_key_node(ps, e, align);
            }
            else
            {
                print_element_with_children(ps, e, align);
            }
        }
    }

    /** Mirrors C is_comment_node. */
    static boolean is_comment_node(XmlNode node)
    {
        return node.type() == XmlNodeType.COMMENT;
    }

    /**
     * Scan the comment to determine how it must be commented.
     * If the comment contains asterisk plus slashes, then find the max num
     * slashes after an asterisk. The returned value is 1 + this max.
     */
    static int count_necessary_slashes(String content)
    {
        int max = 0;
        int curr = 0;
        boolean counting = false;

        for (int i = 0; i < content.length(); i++)
        {
            char c = content.charAt(i);
            if (counting)
            {
                if (c == '/')
                {
                    curr++;
                    if (curr > max) max = curr;
                }
                else
                {
                    counting = false;
                }
            }

            if (!counting)
            {
                if (c == '*')
                {
                    counting = true;
                    curr = 0;
                }
            }
        }
        return max+1;
    }

    // Mirrors C print_comment_line: a single physical comment line, colored.
    void print_comment_line_span(XMQPrintState ps, String line)
    {
        print_utf8(ps, XMQColor.COLOR_comment, line);
    }

    // Mirrors C print_slashes: pre + '/'*n + post, colored with COLOR_comment.
    void print_slashes(XMQPrintState ps, String pre, String post, int n)
    {
        // Raw writes, the indentation is not updated, like C print_slashes.
        write(ps, color_pre(ps, XMQColor.COLOR_comment));
        write(ps, pre);
        write(ps, "/".repeat(n));
        write(ps, post);
        write(ps, color_post(ps, XMQColor.COLOR_comment));
    }

    // Mirrors C print_comment_lines.
    void print_comment_lines(XMQPrintState ps, String content, boolean compact)
    {
        int num_slashes = count_necessary_slashes(content);

        // Mirrors C: line_indent = current_indent + 1 + num_slashes (+ 1 for the
        // space, when not compact) so that continuation lines start just after /* .
        int add_spaces = ps.current_indent + 1 + num_slashes;
        print_slashes(ps, null, "*", num_slashes);
        if (!compact)
        {
            if (!content.isEmpty() && content.charAt(0) != '\n')
            {
                print_white_spaces(ps, 1);
            }
            add_spaces++;
        }

        int prev_line_indent = ps.line_indent;
        ps.line_indent = add_spaces;

        int line_start = 0;
        for (int i = 0; i < content.length(); i++)
        {
            if (content.charAt(i) == '\n')
            {
                if (line_start > 0)
                {
                    if (compact)
                    {
                        print_slashes(ps, "*", "*", num_slashes);
                    }
                    else if (i > 0 && content.charAt(i-1) == '\n' && i + 1 < content.length())
                    {
                        // This is an empty line. Do not indent.
                        // Except the last line which must be indented.
                        print_nl(ps, null, null);
                    }
                    else
                    {
                        print_nl_and_indent(ps, null, null);
                    }
                }
                print_comment_line_span(ps, content.substring(line_start, i));
                line_start = i+1;
            }
        }

        if (line_start == 0)
        {
            // No newlines found.
            print_comment_line_span(ps, content);
        }
        else if (line_start < content.length())
        {
            // There is a remaining line that ends with stop and not newline.
            if (line_start > 0)
            {
                if (compact)
                {
                    print_slashes(ps, "*", "*", num_slashes);
                }
                else
                {
                    print_nl_and_indent(ps, null, null);
                }
            }
            print_comment_line_span(ps, content.substring(line_start));
        }

        if (!compact)
        {
            print_white_spaces(ps, 1);
        }
        print_slashes(ps, "*", null, num_slashes);
        ps.last_char = '/';
        ps.line_indent = prev_line_indent;
    }

    // Mirrors C print_comment_node.
    void print_comment_node(XMQPrintState ps, XmlNode comment)
    {
        String content = comment.content();
        if (content == null) content = "";

        check_space_before_comment(ps);

        boolean has_newline = content.indexOf('\n') >= 0;
        if (!has_newline)
        {
            if (ps.output_settings.compact())
            {
                print_utf8(ps, XMQColor.COLOR_comment, "/*", content, "*/");
                ps.last_char = '/';
            }
            else
            {
                print_utf8(ps, XMQColor.COLOR_comment, "// ", content);
                ps.last_char = 1;
            }
        }
        else
        {
            print_comment_lines(ps, content, ps.output_settings.compact());
            ps.last_char = '/';
        }
    }

    static boolean need_separation_before_comment(XMQPrintState ps)
    {
        // If the previous value was quoted, then then no space is needed, ie.
        // 'x y z'/*comment*/
        // If the previous value was an entity &...; then then no space is needed, ie.
        // &nbsp;/*comment*/
        // if previous value was text, then a space is necessary, ie.
        // xyz /*comment*/
        // if previous value was } or )) then no space is is needed.
        // }/*comment*/   ((...))/*comment*/
        char c = ps.last_char;
        return c != 0 && c != '\'' && c != '"' && c != '{' && c != ')' && c != '}' && c != ';';
    }

    void check_space_before_comment(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == 0) return;
        if (!ps.output_settings.compact())
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (need_separation_before_comment(ps))
        {
            print_white_spaces(ps, 1);
        }
    }

    /** Mirrors C print_pi_node. Prints ?target = value */
    void print_pi_node(XMQPrintState ps, XmlNode node)
    {
        check_space_before_key(ps);
        String name = node.name();
        print_utf8(ps, XMQColor.COLOR_element_key, "?" + name);
        if (!ps.output_settings.compact()) print_white_spaces(ps, 1);
        print_utf8(ps, XMQColor.COLOR_equals, "=");
        if (!ps.output_settings.compact()) print_white_spaces(ps, 1);

        String c = XmlSave.nodeDump(node.doc(), node, 0, false);
        int n = c.length();
        // Now detect if we need to add a leading/ending space.
        if (n >= 2 && c.charAt(n - 1) == '>' && c.charAt(n - 2) == '?') n -= 2;
        int start = Math.min(name.length() + 3, n);
        String content = XMQParseIntoDOM.potentially_add_leading_ending_space(c.substring(start, n));
        if (ps.output_settings.compact()) content = content.replace('\n', ' ');
        print_value_string(ps, content, Level.ELEMENT_VALUE, false);
    }

    /** Mirrors C print_doctype. Prints !DOCTYPE = value */
    void print_doctype(XMQPrintState ps, XmlDtd node)
    {
        check_space_before_key(ps);
        print_utf8(ps, XMQColor.COLOR_element_key, "!DOCTYPE");
        if (!ps.output_settings.compact()) print_white_spaces(ps, 1);
        print_utf8(ps, XMQColor.COLOR_equals, "=");
        if (!ps.output_settings.compact()) print_white_spaces(ps, 1);

        String c = XmlSave.nodeDump(node.doc(), node, 0, false);
        if (ps.output_settings.compact()) c = c.replace('\n', ' ');
        // Skip the leading <!DOCTYPE and the ending >
        print_value_string(ps, c.substring(10, c.length() - 1), Level.ELEMENT_VALUE, false);
    }
}
