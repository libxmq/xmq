/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

import org.w3c.dom.Attr;
import org.w3c.dom.Comment;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;

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

    /** Mirrors C print_utf8_char. Prints a single character (updates last_char). */
    void print_char(XMQPrintState ps, char c)
    {
        ps.buffer.append(c);
        ps.last_char = c;
        ps.current_indent += 1;
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
    void check_space_before_quote(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == 0) return;
        if (!ps.output_settings.compact() && c != '=' && c != '(')
        {
            print_nl_and_indent(ps, null, null);
        }
        else if (c == '\'' || c == '"' || (c >= 'A' && c <= 'Z'))
        {
            print_white_spaces(ps, 1);
        }
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
        NamedNodeMap attributes = element.getAttributes();

        if (attributes != null && attributes.getLength() > 0)
        {
            ps.buffer.append("(");
            ps.last_char = '(';
            ps.current_indent += 1;

            // Mirrors C find_attr_key_max_u_width: scan the attributes
            // until there is one which is not a key = value attribute
            // (ie. no value), and find the max width of the keys.
            int max = 0;
            if (!ps.output_settings.compact())
            {
                for (int i = 0; i < attributes.getLength(); i++)
                {
                    Attr attr = (Attr)attributes.item(i);
                    String value = attr.getValue();
                    if (value == null || value.isEmpty()) break;
                    int len = attr.getName().length();
                    if (len > max) max = len;
                }
            }

            // Mirrors C print_attributes: subsequent attributes are indented
            // so that they line up just after the opening parenthesis.
            int old_line_indent = ps.line_indent;
            ps.line_indent = ps.current_indent;

            for (int i = 0; i < attributes.getLength(); i++)
            {
                print_attribute(ps, (Attr)attributes.item(i), max);
            }

            ps.line_indent = old_line_indent;

            ps.buffer.append(")");
            ps.last_char = ')';
            ps.current_indent += 1;
        }
    }

    // Mirrors C print_attribute. Name already printed by caller.
    void print_attribute(XMQPrintState ps, Attr attr, int max)
    {
        check_space_before_attribute(ps);

        print_string(ps, attr.getName());

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
            print_value_text(ps, value, false);
        }
    }

    void print_content_node(XMQPrintState ps, Node node)
    {
        Text text = (Text)node;
        String value = text.getNodeValue();

        if (value == null || value.trim().isEmpty())
        {
            return;
        }

        if (ps.last_char == '=')
        {
            // Key = value: print_key_node has already printed the space after =.
        }
        else
        {
            check_space_before_key(ps);
        }
        // Do NOT trim: leading and ending spaces can be part of the value,
        // ie. key = ' x y z '
        print_value_text(ps, value, false);
    }

    /**
     * Mirrors C print_value. Prints the value of a key=value node. If there
     * are multiple children (text and/or entity references) the value is
     * compounded: ( value parts... ).
     */
    void print_value(XMQPrintState ps, Element element)
    {
        NodeList children = element.getChildNodes();

        // In C: is_compound = node->next != NULL, ie. the value has siblings.
        boolean is_compound = children.getLength() > 1;

        int old_line_indent = ps.line_indent;

        if (is_compound)
        {
            print_string(ps, "(");
            print_white_spaces(ps, 1);
            ps.line_indent = ps.current_indent;
        }

        for (int i = 0; i < children.getLength(); i++)
        {
            Node child = children.item(i);
            if (is_entity_node(child))
            {
                print_entity_node(ps, child);
            }
            else if (is_content_node(child))
            {
                String value = ((Text)child).getNodeValue();
                if (value == null) value = "";
                print_value_text(ps, value, is_compound);
            }
            else
            {
                // Sub element, should not happen for a key=value node.
                print_node(ps, child, 0);
            }
        }

        if (is_compound)
        {
            print_white_spaces(ps, 1);
            print_string(ps, ")");
        }

        ps.line_indent = old_line_indent;
    }

    /** Mirrors C print_entity_node. Prints &name;. */
    void print_entity_node(XMQPrintState ps, Node node)
    {
        check_space_before_entity_node(ps);
        String name = node.getNodeName();
        print_string(ps, "&"+name+";");
    }

    /** Mirrors C is_safe_value_char. True if the character does not need quoting. */
    static boolean is_safe_value_char(char c)
    {
        return c != ' ' && c != '\n' && c != '\t' && c != '\r'
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
    static int count_necessary_quotes(String s, boolean prefer_double_quotes, boolean[] use_double_quotes, boolean[] add_nls)
    {
        boolean all_safe = !is_unsafe_value_start(s);

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

    /**
     * Mirrors C print_value_internal_text + print_safe_leaf_quote.
     * Prints a text value, quoted if necessary (unless it is a safe plain
     * xmq text value and not in a compound). If in_compound is true then
     * quoting is forced, like in C where the level is not ELEMENT_VALUE
     * inside a compound.
     */
    void print_value_text(XMQPrintState ps, String value, boolean in_compound)
    {
        if (value.isEmpty())
        {
            // Empty values are printed like ''
            check_space_before_quote(ps);
            print_string(ps, "''");
            return;
        }

        if (!in_compound && is_xmq_text_value(value))
        {
            // Safe text, no quotes needed: key = 123 or key = blue
            print_string(ps, value);
            return;
        }

        boolean[] use_double_quotes = { false };
        boolean[] add_nls = { false };
        int numq = count_necessary_quotes(value, false, use_double_quotes, add_nls);
        if (numq < 1) numq = 1;
        String q = use_double_quotes[0] ? "\"" : "'";

        int old_line_indent = ps.line_indent;
        if (add_nls[0])
        {
            ps.line_indent = ps.current_indent;
        }

        check_space_before_quote(ps);
        print_string(ps, q.repeat(numq));

        if (!add_nls[0])
        {
            ps.line_indent = ps.current_indent;
        }
        if (add_nls[0])
        {
            print_nl_and_indent(ps, null, null);
        }

        // Mirrors C print_quote_lines_and_color_uwhitespace.
        if (value.charAt(0) == '\n')
        {
            // We are leading with a newline, print an extra into the quote,
            // which will be trimmed away during parse.
            print_nl(ps, null, null);
        }

        boolean all_newlines = true;
        for (int i = 0; i < value.length(); i++)
        {
            char ch = value.charAt(i);
            if (ch == '\n')
            {
                if (i + 1 < value.length() && value.charAt(i + 1) != '\n')
                {
                    // This newline has content after it, indent the next line.
                    print_nl_and_indent(ps, null, null);
                }
                else
                {
                    // Empty line or last line, no indent.
                    print_nl(ps, null, null);
                }
            }
            else
            {
                print_char(ps, ch);
                all_newlines = false;
            }
        }
        if (value.charAt(value.length() - 1) == '\n')
        {
            // We are ending with a newline, print an extra into the quote,
            // which will be trimmed away during parse.
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

        if (!add_nls[0])
        {
            ps.line_indent = old_line_indent;
        }
        if (add_nls[0])
        {
            print_nl_and_indent(ps, null, null);
        }

        print_string(ps, q.repeat(numq));

        if (add_nls[0])
        {
            ps.line_indent = old_line_indent;
        }
    }

    void print_element_node(XMQPrintState ps, Node node, int align)
    {
        Element element = (Element)node;

        check_space_before_key(ps);
        print_string(ps, element.getTagName());

        print_attributes(ps, element);

        NodeList children = element.getChildNodes();

        // This is a node with no children, just the key.
        if (children.getLength() == 0)
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

        // Print the value, i.e. the first (or all) child nodes.
        print_value(ps, element);
    }

    /** Mirrors C find_element_key_max_width.
     *  Scans the list of siblings from from_index until there is a node which
     *  is not a suitable key=value node (ie. no children, multiple children,
     *  or it has attributes). The max width of the keys found is returned,
     *  and restart[0] is set to the index where the next scan must begin. */
    static int find_element_key_max_width(NodeList children, int from_index, int[] restart)
    {
        int max = 0;

        for (int i = from_index; i < children.getLength(); i++)
        {
            Node n = children.item(i);
            if (!is_element_node(n)
                || !is_key_value_node(n)
                || (((Element)n).getAttributes() != null
                    && ((Element)n).getAttributes().getLength() > 0))
            {
                if (i == from_index) restart[0] = i + 1;
                else restart[0] = i;
                return max;
            }
            int len = n.getNodeName().length();
            if (len > max) max = len;
        }

        restart[0] = children.getLength();
        return max;
    }

    /** Mirrors C print_nodes.
     *  Prints a list of sibling nodes, aligning the equal signs of runs of
     *  key=value nodes, unless compact. */
    void print_nodes(XMQPrintState ps, NodeList children)
    {
        int restart_find_at_node = 0;
        int max = 0;

        for (int i = 0; i < children.getLength(); i++)
        {
            // We need to search ahead to find the max width of the node names so that we can align the equal signs.
            if (!ps.output_settings.compact() && i == restart_find_at_node)
            {
                int[] restart = new int[1];
                max = find_element_key_max_width(children, i, restart);
                restart_find_at_node = restart[0];
            }

            print_node(ps, children.item(i), max);
        }
    }

    void print_element_with_children(XMQPrintState ps, Element element)
    {
        NodeList children = element.getChildNodes();

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
    static boolean is_element_node(Node node)
    {
        return node.getNodeType() == Node.ELEMENT_NODE;
    }

    // Check if the node is a text node
    static boolean is_content_node(Node node)
    {
        return node.getNodeType() == Node.TEXT_NODE ||
            node.getNodeType() == Node.CDATA_SECTION_NODE;
    }

    // Check if the node is a processing instruction
    static boolean is_pi_node(Node node)
    {
        return node.getNodeType() == Node.PROCESSING_INSTRUCTION_NODE;
    }

    static boolean is_leaf_node(Node node)
    {
        return node.getChildNodes().getLength() == 0;
    }

    /** Mirrors C is_key_value_node.
     *  Single content or entity child, or multiple text or entity children. */
    static boolean is_key_value_node(Node node)
    {
        NodeList children = node.getChildNodes();
        if (children.getLength() == 0)
        {
            return false;
        }

        Node from = children.item(0);
        Node to = children.item(children.getLength() - 1);

        // Single content or entity node.
        if (from == to && (is_content_node(from) || is_entity_node(from)))
        {
            return true;
        }

        // Multiple text or entity nodes.
        for (int i = 0; i < children.getLength(); i++)
        {
            int type = children.item(i).getNodeType();
            if (type != Node.TEXT_NODE && type != Node.ENTITY_REFERENCE_NODE)
            {
                return false;
            }
        }
        return true;
    }

    static boolean is_entity_node(Node node)
    {
        return node.getNodeType() == Node.ENTITY_NODE ||
            node.getNodeType() == Node.ENTITY_REFERENCE_NODE;
    }

    /**
     * Recursively prints a node.
     * @param ps The print state.
     * @param node The node to print.
     * @param align The alignment level used for indentation.
     */
    public void print_node(XMQPrintState ps, Node node, int align)
    {
        if (node.getNodeType() == Node.DOCUMENT_NODE) {
            print_nodes(ps, node.getChildNodes());
        }
        else if (is_content_node(node)) {
            print_content_node(ps, node);
        }
        else if (is_element_node(node)) {
        print_element_node(ps, node, align);
        }
        else if (is_pi_node(node)) {
            print_pi_node(ps, node);
        }
        else if (is_comment_node(node)) {
            print_comment_node(ps, (Comment)node);
        }
        else
        {
            throw new RuntimeException("Unknown node type: " + node.getNodeType());
        }
    }

    static boolean is_comment_node(Node node)
    {
        return node.getNodeType() == Node.COMMENT_NODE;
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

    // Mirrors C print_comment_lines.
    void print_comment_lines(XMQPrintState ps, String content, boolean compact)
    {
        int num_slashes = count_necessary_slashes(content);
        String slashes = "/".repeat(num_slashes);

        // Mirrors C: line_indent = current_indent + 1 + num_slashes (+ 1 for the
        // space, when not compact) so that continuation lines start just after /* .
        int add_spaces = ps.current_indent + 1 + num_slashes;
        print_string(ps, slashes + "*");
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
                        print_string(ps, "*" + slashes + "*");
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
                print_string(ps, content.substring(line_start, i));
                line_start = i+1;
            }
        }

        if (line_start == 0)
        {
            // No newlines found.
            print_string(ps, content);
        }
        else if (line_start < content.length())
        {
            // There is a remaining line that ends with stop and not newline.
            if (line_start > 0)
            {
                if (compact)
                {
                    print_string(ps, "*" + slashes + "*");
                }
                else
                {
                    print_nl_and_indent(ps, null, null);
                }
            }
            print_string(ps, content.substring(line_start));
        }

        if (!compact)
        {
            print_string(ps, " ");
        }
        print_string(ps, "*" + slashes);
        ps.last_char = '/';
        ps.line_indent = prev_line_indent;
    }

    // Mirrors C print_comment_node.
    void print_comment_node(XMQPrintState ps, Comment comment)
    {
        String content = comment.getNodeValue();
        if (content == null) content = "";

        check_space_before_comment(ps);

        boolean has_newline = content.indexOf('\n') >= 0;
        if (!has_newline)
        {
            if (ps.output_settings.compact())
            {
                print_string(ps, "/*");
                print_string(ps, content);
                print_string(ps, "*/");
                ps.last_char = '/';
            }
            else
            {
                print_string(ps, "// ");
                print_string(ps, content);
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

    void print_pi_node(XMQPrintState ps, Node node)
    {
        String target = node.getNodeName();

        // A pi node named DOCTYPE holds a xmq doctype value (see XMQParseIntoDOM).
        if (target != null && target.equals("DOCTYPE"))
        {
            String content = node.getNodeValue() != null ? node.getNodeValue() : "";
            print_doctype(ps, content);
            return;
        }

        check_space_before_key(ps);
        ps.buffer.append("<?");
        ps.last_char = '?';
        ps.current_indent += 2;
        String content = node.getNodeValue() != null ? node.getNodeValue() : "";
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

    /** Mirrors C print_doctype. Prints !DOCTYPE = value. */
    void print_doctype(XMQPrintState ps, String content)
    {
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
        print_value_text(ps, content, false /* in_compound */);
    }
}
