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

import java.util.List;

import org.jdom2.Attribute;
import org.jdom2.Comment;
import org.jdom2.Content;
import org.jdom2.DocType;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.EntityRef;
import org.jdom2.Namespace;
import org.jdom2.ProcessingInstruction;
import org.jdom2.Text;

import org.libxmq.imp.xml.RawXmlParser;
import org.libxmq.imp.xml.ValuePart;

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

    /**
     * Mirrors C print_color_pre: writes the pre string for a token color if
     * one is set in the theme. Remember the pre string so it can be replayed
     * (re-printed) if the theme uses a NULL post (e.g. ansi codes).
     */
    void print_color_pre(XMQPrintState ps, XMQColor color)
    {
        XMQThemeStrings ts = ps.theme.getStringsFor(color);
        if (ts == null || ts.pre() == null)
        {
            return;
        }
        ps.replay_active_color_pre = ts.pre();
        ps.buffer.append(ts.pre());
    }

    /**
     * Mirrors C print_color_post: writes the post string for a token color if
     * one is set in the theme. When the theme uses a NULL post (e.g. ansi
     * codes) the remembered pre string is re-printed instead.
     */
    void print_color_post(XMQPrintState ps, XMQColor color)
    {
        XMQThemeStrings ts = ps.theme.getStringsFor(color);
        if (ts == null)
        {
            return;
        }
        if (ts.post() == null)
        {
            if (ps.replay_active_color_pre != null)
            {
                ps.buffer.append(ps.replay_active_color_pre);
            }
            return;
        }
        ps.buffer.append(ts.post());
        ps.replay_active_color_pre = null;
    }

    /**
     * Mirrors C xmq_print_xmq: prints a document wrapped in the theme's
     * document/header/style/body/content pre/post strings. Used by the render
     * commands (render-html, render-terminal, render-tex). In the default
     * to-xmq path the theme has none of these strings set, so this just
     * prints the nodes.
     */
    void print_rendered(XMQPrintState ps, org.jdom2.Document doc)
    {
        XMQTheme theme = ps.theme;
        if (theme.document != null && theme.document.pre()  != null) ps.buffer.append(theme.document.pre());
        if (theme.header   != null && theme.header.pre()    != null) ps.buffer.append(theme.header.pre());
        if (theme.style    != null && theme.style.pre()     != null) ps.buffer.append(theme.style.pre());
        if (theme.header   != null && theme.header.post()   != null) ps.buffer.append(theme.header.post());
        if (theme.body     != null && theme.body.pre()      != null) ps.buffer.append(theme.body.pre());
        if (theme.content  != null && theme.content.pre()   != null) ps.buffer.append(theme.content.pre());
        print_nodes(ps, doc.getContent());
        if (theme.content  != null && theme.content.post()  != null) ps.buffer.append(theme.content.post());
        if (theme.body     != null && theme.body.post()     != null) ps.buffer.append(theme.body.post());
        if (theme.document != null && theme.document.post() != null) ps.buffer.append(theme.document.post());
    }

    /** Mirrors C print_utf8_char. Prints a single character (updates last_char). */
    void print_char(XMQPrintState ps, char c)
    {
        ps.buffer.append(c);
        ps.last_char = c;
        ps.current_indent += 1;
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

    void print_white_spaces(XMQPrintState ps, int num)
    {
        XMQTheme c = ps.theme;
        if (c != null && c.whitespace != null && c.whitespace.pre() != null)
        {
            ps.buffer.append(c.whitespace.pre());
        }
        ps.buffer.append(" ".repeat(num));
        ps.current_indent += num;
        if (c != null && c.whitespace != null && c.whitespace.post() != null)
        {
            ps.buffer.append(c.whitespace.post());
        }
    }

    void print_nl(XMQPrintState ps, String prefix, String postfix)
    {
        if (postfix != null)
        {
            ps.buffer.append(postfix);
        }
        ps.buffer.append("\n");
        ps.current_indent = 0;
        ps.last_char = 0;
        if (prefix != null)
        {
            ps.buffer.append(prefix);
            ps.current_indent += prefix.length();
        }
    }

    void print_nl_and_indent(XMQPrintState ps, String prefix, String postfix)
    {
        print_nl(ps, null, postfix);
        print_white_spaces(ps, ps.line_indent);
        if (prefix != null)
        {
            ps.buffer.append(prefix);
            ps.current_indent += prefix.length();
        }
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
        else if (c == '\'' || c == '"' || Character.isDigit(c) || Character.isLetter(c))
        {
            print_white_spaces(ps, 1);
        }
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

    void print_attributes(XMQPrintState ps, Element element)
    {
        List<Attribute> attributes = element.getAttributes();

        // Mirrors C xml.c: libxml2 stores regular attributes in node->properties
        // and xmlns/ns: declarations in node->nsDef; C prints the regular
        // attributes (source order) first, then the namespace declarations
        // (source order).
        //
        // Two different parsers feed this printer:
        //   - the XML parser (RawXmlParser) records xmlns/xmlns:* declarations
        //     as regular JDOM2 attributes literally named "xmlns" / "xmlns:PREFIX"
        //     (JDOM2's normal namespace machinery is bypassed), so they show up
        //     in element.getAttributes();
        //   - the XMQ parser cannot use an attribute named "xmlns" (JDOM2 throws
        //     IllegalNameException for a colon in an attribute name, and
        //     setAttribute("xmlns",...) is rejected outright), so it records
        //     declarations as namespace objects on the element (element's own
        //     namespace for the default one, plus additional namespace
        //     declarations for "xmlns:PREFIX").
        // Handle both: collect xmlns-named attributes and namespace objects, and
        // print regular attributes first, then the declarations (in their
        // relative source order), with the default (empty-prefix) one last.
        List<Attribute> regular = new java.util.ArrayList<>();
        List<Attribute> xmlns_attrs = new java.util.ArrayList<>();
        if (attributes != null)
        {
            for (Attribute attr : attributes)
            {
                String n = attr.getName();
                if (n.equals("xmlns") || (n.startsWith("xmlns:") && !n.startsWith("xmlns:xmlns:")))
                {
                    xmlns_attrs.add(attr);
                }
                else
                {
                    regular.add(attr);
                }
            }
        }

        List<Namespace> nspaces = new java.util.ArrayList<>(element.getAdditionalNamespaces());
        Namespace own = element.getNamespace();
        boolean own_decl = own != null && own.getPrefix().isEmpty()
            && (own.getURI() != null && !own.getURI().isEmpty());
        if (own_decl)
        {
            nspaces.add(own); // Default namespace declaration, printed last.
        }

        if (regular.isEmpty() && xmlns_attrs.isEmpty() && nspaces.isEmpty())
        {
            return;
        }

        ps.buffer.append("(");
        ps.last_char = '(';
        ps.current_indent += 1;

        // Mirrors C find_attr_key_max_u_width: find the max width of the keys
        // across regular attributes and namespace declarations.
        int max = 0;
        if (!ps.output_settings.compact())
        {
            for (Attribute attr : regular)
            {
                String value = attr.getValue();
                if (value == null || value.isEmpty()) break;
                int len = attrFullname(attr).length();
                if (len > max) max = len;
            }
            for (Attribute attr : xmlns_attrs)
            {
                int len = attr.getName().length();
                if (len > max) max = len;
            }
            for (Namespace ns : nspaces)
            {
                int len = nsName(ns).length();
                if (len > max) max = len;
            }
        }

        // Mirrors C print_attributes: subsequent entries are indented so that
        // they line up just after the opening parenthesis.
        int old_line_indent = ps.line_indent;
        ps.line_indent = ps.current_indent;

        for (Attribute attr : regular)
        {
            print_attribute(ps, attr, max);
        }

        // xmlns / xmlns:PREFIX declarations recorded as attributes (XML parser).
        for (Attribute attr : xmlns_attrs)
        {
            print_attribute(ps, attr, max);
        }

        // xmlns / xmlns:PREFIX declarations recorded as namespace objects
        // (XMQ parser).
        for (Namespace ns : nspaces)
        {
            print_ns_declaration(ps, nsName(ns), ns.getURI(), max);
        }

        ps.line_indent = old_line_indent;

        ps.buffer.append(")");
        ps.last_char = ')';
        ps.current_indent += 1;
    }

    // The xmq name rendered for a JDOM2 namespace declaration: "xmlns" for a
    // default (empty-prefix) namespace, "xmlns:prefix" otherwise.
    static String nsName(Namespace ns)
    {
        String prefix = ns.getPrefix();
        if (prefix == null || prefix.isEmpty())
        {
            return "xmlns";
        }
        return "xmlns:"+prefix;
    }

    // Mirrors C print_attribute, for an element namespace declaration whose
    // "name" is xmlns / xmlns:prefix and whose value is the namespace uri.
    void print_ns_declaration(XMQPrintState ps, String name, String value, int max)
    {
        check_space_before_attribute(ps);

        // name is "xmlns" or "xmlns:PREFIX".
        int colon = name.indexOf(':');
        if (colon > 0)
        {
            String prefix = name.substring(colon + 1);
            XMQColor ns = prefix.equals("xsl") ? XMQColor.COLOR_ns_override_xsl : XMQColor.COLOR_attr_ns;
            print_color_pre(ps, XMQColor.COLOR_ns_declaration);
            print_string(ps, name.substring(0, colon)); // "xmlns"
            print_color_post(ps, XMQColor.COLOR_ns_declaration);
            print_color_pre(ps, XMQColor.COLOR_ns_colon);
            print_string(ps, ":");
            print_color_post(ps, XMQColor.COLOR_ns_colon);
            print_color_pre(ps, ns);
            print_string(ps, prefix);
            print_color_post(ps, ns);
        }
        else
        {
            print_color_pre(ps, XMQColor.COLOR_ns_declaration);
            print_string(ps, name); // "xmlns"
            print_color_post(ps, XMQColor.COLOR_ns_declaration);
        }

        if (value != null && !value.isEmpty())
        {
            if (!ps.output_settings.compact())
            {
                int len = ps.current_indent - ps.line_indent;
                int pad = 1;
                if (len < max)
                {
                    pad = 1 + max - len;
                }
                print_white_spaces(ps, pad);
            }
            print_string(ps, "=");
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            print_attribute_value(ps, value);
        }
    }

    // The name rendered for an attribute: the name itself when it already
    // carries a colon, otherwise the attribute's namespace prefix (eg a
    // prefixed name remembered at parse time) followed by ":" and the name.
    static String attrFullname(Attribute attr)
    {
        String name = attr.getName();
        if (name.indexOf(':') > 0)
        {
            return name;
        }
        Namespace ns = attr.getNamespace();
        if (ns != null && ns.getPrefix() != null && !ns.getPrefix().isEmpty())
        {
            return ns.getPrefix()+":"+name;
        }
        return name;
    }

    // Mirrors C print_attribute. Name already printed by caller.
    void print_attribute(XMQPrintState ps, Attribute attr, int max)
    {
        check_space_before_attribute(ps);

        String name = attrFullname(attr);
        int colon = name.indexOf(':');
        if (colon > 0 && colon < name.length() - 1)
        {
            String prefix = name.substring(0, colon);
            String local  = name.substring(colon + 1);
            XMQColor ns = prefix.equals("xsl") ? XMQColor.COLOR_ns_override_xsl : XMQColor.COLOR_attr_ns;
            print_color_pre(ps, ns);
            print_string(ps, prefix);
            print_color_post(ps, ns);
            print_color_pre(ps, XMQColor.COLOR_ns_colon);
            print_string(ps, ":");
            print_color_post(ps, XMQColor.COLOR_ns_colon);
            print_color_pre(ps, XMQColor.COLOR_attr_key);
            print_string(ps, local);
            print_color_post(ps, XMQColor.COLOR_attr_key);
        }
        else
        {
            print_color_pre(ps, XMQColor.COLOR_attr_key);
            print_string(ps, name);
            print_color_post(ps, XMQColor.COLOR_attr_key);
        }

        String value = attr.getValue();
        if (value != null && !value.isEmpty())
        {
            if (!ps.output_settings.compact())
            {
                // Align the equal signs by padding after the key name.
                int len = ps.current_indent - ps.line_indent;
                int pad = 1;
                if (len < max)
                {
                    pad = 1 + max - len;
                }
                print_white_spaces(ps, pad);
            }
            print_string(ps, "=");
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            print_attribute_value(ps, value);
        }
    }

    /** Mirrors C print_value for an attribute value which may contain
     *  entity references: references are printed as-is, other text is
     *  quoted/escaped, and more than one part is wrapped in parentheses. */
    void print_attribute_value(XMQPrintState ps, String value)
    {
        List<ValuePart> parts = RawXmlParser.value_parts(value);
        if (parts.isEmpty() || (parts.size() == 1 && !parts.get(0).is_entity))
        {
            print_value_string(ps, value, Level.ATTR_VALUE, false);
            return;
        }
        if (parts.size() == 1)
        {
            print_entity_name(ps, XMQColor.COLOR_attr_value_entity, parts.get(0).text);
            return;
        }

        int old_line_indent = ps.line_indent;
        print_color_pre(ps, XMQColor.COLOR_cpar_left);
        print_string(ps, "(");
        print_color_post(ps, XMQColor.COLOR_cpar_left);
        if (!ps.output_settings.compact())
        {
            print_white_spaces(ps, 1);
        }
        ps.line_indent = ps.current_indent;
        for (ValuePart p : parts)
        {
            if (p.is_entity)
            {
                print_entity_name(ps, XMQColor.COLOR_attr_value_compound_entity, p.text);
            }
            else
            {
                print_value_string(ps, p.text, Level.COMPOUND, true);
            }
        }
        if (!ps.output_settings.compact())
        {
            print_white_spaces(ps, 1);
        }
        print_color_pre(ps, XMQColor.COLOR_cpar_right);
        print_string(ps, ")");
        print_color_post(ps, XMQColor.COLOR_cpar_right);
        ps.line_indent = old_line_indent;
    }

    /** Mirrors C print_entity_node. */
    void print_entity_name(XMQPrintState ps, XMQColor color, String name)
    {
        check_space_before_entity_node(ps);
        print_color_pre(ps, color);
        print_string(ps, "&" + name + ";");
        print_color_post(ps, color);
    }

    void print_content_node(XMQPrintState ps, Content node)
    {
        Text text = (Text)node;
        // Do NOT trim: leading and ending spaces can be part of the value,
        // ie. key = ' x y z '
        print_value_string(ps, text.getText(), Level.XMQ, false);
    }

    /** Mirrors C print_entity_node. Prints &name;. */
    void print_entity_node(XMQPrintState ps, Content node)
    {
        check_space_before_entity_node(ps);
        String name = ((EntityRef)node).getName();
        XMQColor c = XMQColor.COLOR_entity;
        if (name.isEmpty() || name.charAt(0) == '_') c = XMQColor.COLOR_quote;
        print_color_pre(ps, c);
        print_string(ps, "&"+name+";");
        print_color_post(ps, c);
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
    enum Level { XMQ, ELEMENT_VALUE, ATTR_VALUE, COMPOUND }

    /** Mirrors C level_to_quote_color. */
    static XMQColor level_quote_color(Level level)
    {
        switch (level)
        {
            case ELEMENT_VALUE: return XMQColor.COLOR_element_value_quote;
            case ATTR_VALUE:    return XMQColor.COLOR_attr_value_quote;
            case COMPOUND:      return XMQColor.COLOR_element_value_compound_quote;
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
            case COMPOUND:      return XMQColor.COLOR_element_value_compound_entity;
            case XMQ:
            default:            return XMQColor.COLOR_entity;
        }
    }

    /** Mirrors C print_quotes. Prints num quotes of the chosen kind, wrapped in the color. */
    void print_quotes(XMQPrintState ps, int num, XMQColor color, boolean use_double_quotes)
    {
        print_color_pre(ps, color);
        print_string(ps, (use_double_quotes ? "\"" : "'").repeat(num));
        print_color_post(ps, color);
    }

    /** Mirrors C print_quoted_spaces. Prints '...spaces...', wrapped in the color. */
    void print_quoted_spaces(XMQPrintState ps, XMQColor color, int num)
    {
        print_color_pre(ps, color);
        print_string(ps, "'" + " ".repeat(num) + "'");
        print_color_post(ps, color);
    }

    /** Mirrors C print_char_entity for a single character, wrapped in the color. */
    void print_char_entity(XMQPrintState ps, XMQColor color, char c)
    {
        print_color_pre(ps, color);
        print_string(ps, "&#" + (int)c + ";");
        print_color_post(ps, color);
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
        i++;
        return new int[] { i, middle == i ? only_nls : 0 };
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

        print_color_pre(ps, color);

        if (!add_nls)
        {
            old_line_indent = ps.line_indent;
            ps.line_indent = ps.current_indent;
        }

        if (add_nls)
        {
            print_nl_and_indent(ps, null, null);
        }

        // Mirrors C print_quote_lines_and_color_uwhitespace.
        if (v.charAt(0) == '\n')
        {
            // We are leading with a newline, print an extra into the quote,
            // which will be trimmed away during parse.
            print_nl(ps, null, null);
        }

        boolean all_newlines = true;
        for (int i = 0; i < v.length(); i++)
        {
            char c = v.charAt(i);
            if (c == '\n')
            {
                if (i + 1 < v.length() && v.charAt(i + 1) != '\n')
                {
                    print_nl_and_indent(ps, null, null);
                }
                else
                {
                    print_nl(ps, null, null);
                }
            }
            else
            {
                print_char(ps, c);
                all_newlines = false;
            }
        }
        // We are ending with a newline, print an extra into the quote,
        // which will be trimmed away during parse.
        if (v.charAt(v.length() - 1) == '\n')
        {
            ps.line_indent--;
            if (!all_newlines)
            {
                print_nl_and_indent(ps, null, null);
            }
            else
            {
                ps.current_indent = 0;
                ps.last_char = 0;
                print_white_spaces(ps, ps.line_indent);
            }
            ps.line_indent++;
        }

        if (!add_nls)
        {
            ps.line_indent = old_line_indent;
        }

        if (add_nls)
        {
            print_nl_and_indent(ps, null, null);
        }

        print_color_post(ps, color);

        print_quotes(ps, numq, color, udd[0]);

        if (add_nls)
        {
            ps.line_indent = old_line_indent;
        }
    }

    /** Mirrors C print_value_internal_text. */
    void print_value_internal_text(XMQPrintState ps, String v, Level level, boolean using_dquotes, boolean already)
    {
        XMQColor qc = level_quote_color(level);
        XMQColor ec = level_entity_color(level);

        if (v.isEmpty())
        {
            check_space_before_quote(ps, level);
            print_color_pre(ps, qc);
            print_string(ps, "''");
            print_color_post(ps, qc);
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
            print_color_pre(ps, qc);
            print_string(ps, v);
            print_color_post(ps, qc);
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
                char c = v.charAt(from);
                print_char_entity(ps, ec, c);
                int from1 = from + 1;
                while (from1 < stop && v.charAt(from1) == '\n')
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
            level = Level.COMPOUND;
            print_color_pre(ps, XMQColor.COLOR_cpar_left);
            print_string(ps, "(");
            print_color_post(ps, XMQColor.COLOR_cpar_left);
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
            print_color_pre(ps, XMQColor.COLOR_cpar_right);
            print_string(ps, ")");
            print_color_post(ps, XMQColor.COLOR_cpar_right);
        }

        ps.line_indent = old_line_indent;
    }

    /** Mirrors C print_value for an element (node list) value. */
    void print_value(XMQPrintState ps, Element element, Level level)
    {
        List<Content> children = element.getContent();

        boolean is_compound = level != Level.XMQ && children.size() > 1;
        boolean prefer_d = ps.output_settings.preferDoubleQuotes();
        boolean[] ud = { prefer_d };

        if (level != Level.XMQ && !is_compound && children.size() == 1 && is_content_node(children.get(0)))
        {
            String t = ((Text)children.get(0)).getText();
            if (t != null)
            {
                is_compound = quote_needs_compounded(ps, t, prefer_d, ud);
            }
        }

        int old_line_indent = ps.line_indent;

        if (is_compound && level != Level.COMPOUND)
        {
            level = Level.COMPOUND;
            print_color_pre(ps, XMQColor.COLOR_cpar_left);
            print_string(ps, "(");
            print_color_post(ps, XMQColor.COLOR_cpar_left);
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            ps.line_indent = ps.current_indent;
        }

        for (Content child : children)
        {
            if (level == Level.XMQ)
            {
                // Only first node in xmq level.
                if (is_entity_node(child))
                {
                    print_entity_name(ps, level_entity_color(level), ((EntityRef)child).getName());
                }
                else if (is_content_node(child))
                {
                    String t = ((Text)child).getText();
                    print_value_string(ps, t == null ? "" : t, level, is_compound);
                }
                else
                {
                    print_element_node(ps, (Element)child, 0);
                }
                break;
            }

            if (is_entity_node(child))
            {
                print_entity_name(ps, level_entity_color(level), ((EntityRef)child).getName());
            }
            else if (is_content_node(child))
            {
                String t = ((Text)child).getText();
                print_value_string(ps, t == null ? "" : t, level, is_compound);
            }
            else
            {
                print_element_node(ps, (Element)child, 0);
            }
        }

        if (is_compound && level == Level.COMPOUND)
        {
            if (!ps.output_settings.compact())
            {
                print_white_spaces(ps, 1);
            }
            print_color_pre(ps, XMQColor.COLOR_cpar_right);
            print_string(ps, ")");
            print_color_post(ps, XMQColor.COLOR_cpar_right);
        }

        ps.line_indent = old_line_indent;
    }

    void print_element_node(XMQPrintState ps, Element element, int align)
    {
        check_space_before_key(ps);

        // The tag name is colored either as a key (key=value style) or as an
        // element name (element with a list of children), depending on the node.
        boolean key_value = !element.getContent().isEmpty() && is_key_value_node(element);
        XMQColor name_color = key_value ? XMQColor.COLOR_element_key : XMQColor.COLOR_element_name;
        // A prefixed element name, eg xxx:proj: print the prefix before the
        // element name, using the element's namespace when the name itself
        // does not already carry a colon (mirrors C, where the node name is
        // printed with its namespace prefix).
        String el_name = element.getName();
        String el_prefix = null;
        if (el_name.indexOf(':') < 0)
        {
            Namespace el_ns = element.getNamespace();
            if (el_ns != null && el_ns.getPrefix() != null && !el_ns.getPrefix().isEmpty())
            {
                el_prefix = el_ns.getPrefix();
            }
        }
        if (el_prefix != null)
        {
            XMQColor ns_color = el_prefix.equals("xsl") ? XMQColor.COLOR_ns_override_xsl : XMQColor.COLOR_attr_ns;
            print_color_pre(ps, ns_color);
            print_string(ps, el_prefix);
            print_color_post(ps, ns_color);
            print_color_pre(ps, XMQColor.COLOR_ns_colon);
            print_string(ps, ":");
            print_color_post(ps, XMQColor.COLOR_ns_colon);
        }
        print_color_pre(ps, name_color);
        print_string(ps, el_name);
        print_color_post(ps, name_color);

        print_attributes(ps, element);

        List<Content> children = element.getContent();

        // This is a node with no children, just the key.
        if (children.isEmpty())
        {
            return;
        }

        // This is a key = value or key = 'value value' node.
        if (is_key_value_node(element))
        {
            print_key_node(ps, element, align);
            return;
        }

        // All other nodes are printed name {children}
        print_element_with_children(ps, element);
    }

    void print_key_node(XMQPrintState ps, Element element, int align)
    {
        // Name and attributes were already printed by the caller.
        if (!ps.output_settings.compact())
        {
            int len = ps.current_indent - ps.line_indent;
            int pad = 1;
            if (len < align)
            {
                pad = 1 + align - len;
            }
            print_white_spaces(ps, pad);
        }
        ps.buffer.append("=");
        ps.last_char = '=';
        ps.current_indent += 1;
        if (!ps.output_settings.compact())
        {
            print_white_spaces(ps, 1);
        }

        // Print the value, ie. the child node(s).
        print_value(ps, element, Level.ELEMENT_VALUE);
    }

    /** Mirrors C find_element_key_max_width.
     *  Scans the list of siblings from from_index until there is a node which
     *  is not a suitable key=value node (ie. no children, multiple children,
     *  or it has attributes). The max width of the keys found is returned,
     *  and restart[0] is set to the index where the next scan must begin. */
    static int find_element_key_max_width(List<Content> children, int from_index, int[] restart)
    {
        int max = 0;

        for (int i = from_index; i < children.size(); i++)
        {
            Content n = children.get(i);
            if (!is_element_node(n)
                || !is_key_value_node(n)
                || (((Element)n).getAttributes() != null
                    && !((Element)n).getAttributes().isEmpty()))
            {
                if (i == from_index) restart[0] = i + 1;
                else restart[0] = i;
                return max;
            }
            int len = n instanceof Element el ? el.getName().length() : 0;
            if (len > max) max = len;
        }

        restart[0] = children.size();
        return max;
    }

    /** Mirrors C print_nodes.
     *  Prints a list of sibling nodes, aligning the equal signs of runs of
     *  key=value nodes, unless compact. */
    void print_nodes(XMQPrintState ps, List<Content> children)
    {
        int restart_find_at_node = 0;
        int max = 0;

        for (int i = 0; i < children.size(); i++)
        {
            // We need to search ahead to find the max width of the node names so that we can align the equal signs.
            if (!ps.output_settings.compact() && i == restart_find_at_node)
            {
                int[] restart = new int[1];
                max = find_element_key_max_width(children, i, restart);
                restart_find_at_node = restart[0];
            }

            print_node(ps, children.get(i), max);
        }
    }

    void print_element_with_children(XMQPrintState ps, Element element)
    {
        List<Content> children = element.getContent();

        check_space_before_opening_brace(ps);
        print_string(ps, "{");

        int old_line_indent = ps.line_indent;
        ps.line_indent += 4;

        print_nodes(ps, children);

        ps.line_indent = old_line_indent;

        check_space_before_closing_brace(ps);
        print_string(ps, "}");
    }

    // Check if the node is an element node (not text, comment, etc.)
    static boolean is_element_node(Content node)
    {
        return node instanceof Element;
    }

    // Check if the node is a text node
    static boolean is_content_node(Content node)
    {
        return node instanceof Text;
    }

    // Check if the node is a processing instruction
    static boolean is_pi_node(Content node)
    {
        return node instanceof ProcessingInstruction;
    }

    static boolean is_doctype_node(Content node)
    {
        return node instanceof DocType;
    }

    /** Mirrors C is_key_value_node.
     *  Single content or entity child, or multiple text or entity children. */
    static boolean is_key_value_node(Content node)
    {
        if (!(node instanceof Element element))
        {
            return false;
        }

        List<Content> children = element.getContent();
        if (children.isEmpty())
        {
            return false;
        }

        Content from = children.get(0);
        Content to = children.get(children.size() - 1);

        // Single content or entity node.
        if (from == to && (is_content_node(from) || is_entity_node(from)))
        {
            return true;
        }

        // Multiple text or entity nodes.
        for (Content child : children)
        {
            if (!is_content_node(child) && !is_entity_node(child))
            {
                return false;
            }
        }
        return true;
    }

    static boolean is_entity_node(Content node)
    {
        return node instanceof EntityRef;
    }

    /**
     * Recursively prints a node.
     * @param ps The print state.
     * @param node The node to print.
     * @param align The alignment level used for indentation.
     */
    public void print_node(XMQPrintState ps, Object node, int align)
    {
        if (node instanceof Document) {
            print_nodes(ps, ((Document)node).getContent());
        }
        else if (is_content_node((Content)node)) {
            print_content_node(ps, (Content)node);
        }
        else if (node instanceof Element) {
        print_element_node(ps, (Element)node, align);
        }
        else if (node instanceof ProcessingInstruction) {
            print_pi_node(ps, (ProcessingInstruction)node);
        }
        else if (node instanceof Comment) {
            print_comment_node(ps, (Comment)node);
        }
        else if (is_entity_node((Content)node)) {
            print_entity_node(ps, (Content)node);
        }
        else if (is_doctype_node((Content)node)) {
            print_doctype_node(ps, (DocType)node);
        }
        else
        {
            throw new RuntimeException("Unknown node type: " + node.getClass().getName());
        }
    }

    static boolean is_comment_node(Content node)
    {
        return node instanceof Comment;
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
        print_color_pre(ps, XMQColor.COLOR_comment);
        print_string(ps, line);
        print_color_post(ps, XMQColor.COLOR_comment);
    }

    // Mirrors C print_slashes: pre + '/'*n + post, colored with COLOR_comment.
    void print_slashes(XMQPrintState ps, String pre, String post, int n)
    {
        print_color_pre(ps, XMQColor.COLOR_comment);
        if (pre != null) print_string(ps, pre);
        print_string(ps, "/".repeat(n));
        if (post != null) print_string(ps, post);
        print_color_post(ps, XMQColor.COLOR_comment);
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
                print_string(ps, " ");
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
            print_string(ps, " ");
        }
        print_slashes(ps, "*", null, num_slashes);
        ps.last_char = '/';
        ps.line_indent = prev_line_indent;
    }

    // Mirrors C print_comment_node.
    void print_comment_node(XMQPrintState ps, Comment comment)
    {
        String content = comment.getText();
        if (content == null) content = "";

        check_space_before_comment(ps);

        boolean has_newline = content.indexOf('\n') >= 0;
        if (!has_newline)
        {
            if (ps.output_settings.compact())
            {
                print_color_pre(ps, XMQColor.COLOR_comment);
                print_string(ps, "/*");
                print_string(ps, content);
                print_string(ps, "*/");
                print_color_post(ps, XMQColor.COLOR_comment);
                ps.last_char = '/';
            }
            else
            {
                print_color_pre(ps, XMQColor.COLOR_comment);
                print_string(ps, "// ");
                print_string(ps, content);
                print_color_post(ps, XMQColor.COLOR_comment);
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

    void print_pi_node(XMQPrintState ps, ProcessingInstruction pi)
    {
        String target = pi.getTarget();

        // A pi node named DOCTYPE holds a xmq doctype value (see XMQParseIntoDOM).
        if (target != null && target.equals("DOCTYPE"))
        {
            String content = pi.getData() != null ? pi.getData() : "";
            print_doctype(ps, content);
            return;
        }

        check_space_before_key(ps);
        ps.buffer.append("<?");
        ps.last_char = '?';
        ps.current_indent += 2;
        String content = pi.getData() != null ? pi.getData() : "";
        print_string(ps, target);
        if (!content.isEmpty())
        {
            ps.buffer.append(" ");
            ps.current_indent += 1;
            print_string(ps, content);
        }
        ps.buffer.append("?>");
        ps.last_char = '>';
        ps.current_indent += 2;
    }

    void print_doctype_internal(XMQPrintState ps, DocType node)
    {
        String name = node.getElementName();
        StringBuilder v = new StringBuilder(name);
        // libxml serializes public/system ids as: name PUBLIC "pid" "sid"
        String pid = node.getPublicID();
        String sid = node.getSystemID();
        if (pid != null && !pid.isEmpty())
        {
            v.append(" PUBLIC \"").append(pid).append("\"");
            if (sid != null && !sid.isEmpty())
            {
                v.append(" \"").append(sid).append("\"");
            }
        }
        else if (sid != null && !sid.isEmpty())
        {
            v.append(" SYSTEM \"").append(sid).append("\"");
        }
        String sub = node.getInternalSubset();
        if (sub != null)
        {
            sub = sub.strip();
            if (!sub.isEmpty())
            {
                v.append(" [\n");
                v.append(sub);
                v.append("\n]");
            }
        }
        print_doctype(ps, v.toString());
    }

    void print_doctype_node(XMQPrintState ps, DocType node)
    {
        print_doctype_internal(ps, node);
    }

    /** Mirrors C print_doctype. Prints !DOCTYPE = value. */
    void print_doctype(XMQPrintState ps, String content)
    {
        // C converts all newlines to spaces in the doctype value when compact
        // mode is set.
        if (ps.output_settings.compact())
        {
            content = content.replace('\n', ' ');
        }

        check_space_before_key(ps);
        print_string(ps, "!DOCTYPE");
        if (!ps.output_settings.compact())
        {
            print_white_spaces(ps, 1);
        }
        print_string(ps, "=");
        if (!ps.output_settings.compact())
        {
            print_white_spaces(ps, 1);
        }
        print_value_string(ps, content, Level.ELEMENT_VALUE, false);
    }
}
