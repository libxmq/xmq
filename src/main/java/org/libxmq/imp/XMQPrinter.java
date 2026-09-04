/*
This file is part of libxmq.

libxmq is free software: you can redistribute it and/or modify
it under the terms of the MIT license.

libxmq is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
MIT license for more details.

You should have received a copy of the MIT License along with
libxmq.  If not, see <https://opensource.org/licenses/MIT>.
*/

package org.libxmq.imp;

import org.w3c.dom.Attr;
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
        if (c == 0) return;

        if (!ps.output_settings.compact())
        {
            print_nl_and_indent(ps, null, null);
        }
        else
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

            for (int i = 0; i < attributes.getLength(); i++)
            {
                Attr attr = (Attr)attributes.item(i);
                check_space_before_attribute(ps);
                print_string(ps, attr.getName());
                String value = attr.getValue();
                if (value != null && !value.isEmpty())
                {
                    if (!ps.output_settings.compact())
                    {
                        ps.buffer.append(" ");
                        ps.current_indent += 1;
                    }
                    ps.buffer.append("=");
                    ps.last_char = '=';
                    ps.current_indent += 1;
                    if (!ps.output_settings.compact())
                    {
                        ps.buffer.append(" ");
                        ps.current_indent += 1;
                    }
                    // Attribute values are kept as-is. Quotes for values with
                    // spaces should be added by the parser.
                    print_string(ps, value);
                }
            }

            ps.buffer.append(")");
            ps.last_char = ')';
            ps.current_indent += 1;
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
        print_value_text(ps, value.trim());
    }

    void print_value_text(XMQPrintState ps, String value)
    {
        // In non-compact output, text containing newlines would need quotes
        // to be kept as-is. For now, print the text as-is.
        print_string(ps, value);
    }

    void print_element_node(XMQPrintState ps, Node node)
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
            print_key_node(ps, element, 0);
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

        // Print the value, i.e. the (first) child node.
        Node first = element.getChildNodes().item(0);
        if (is_content_node(first))
        {
            print_node(ps, first, align);
        }
        else
        {
            print_node(ps, first, align);
        }
    }

    void print_element_with_children(XMQPrintState ps, Element element)
    {
        NodeList children = element.getChildNodes();

        check_space_before_opening_brace(ps);
        print_string(ps, "{");

        int old_line_indent = ps.line_indent;
        ps.line_indent += 4;

        for (int i = 0; i < children.getLength(); i++)
        {
            print_node(ps, children.item(i), 0);
        }

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
            NodeList children = node.getChildNodes();
            for (int i = 0; i < children.getLength(); i++)
            {
                print_node(ps, children.item(i), align);
            }
        }
        else if (is_content_node(node)) {
            print_content_node(ps, node);
        }
        else if (is_element_node(node)) {
            print_element_node(ps, node);
        }
        else if (is_pi_node(node)) {
            print_pi_node(ps, node);
        }
        else
        {
            throw new RuntimeException("Unknown node type: " + node.getNodeType());
        }
    }

    void print_pi_node(XMQPrintState ps, Node node)
    {
        check_space_before_key(ps);
        ps.buffer.append("<?");
        ps.last_char = '?';
        ps.current_indent += 2;
        String content = node.getNodeValue() != null ? node.getNodeValue() : "";
        String target = node.getNodeName();
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
}
