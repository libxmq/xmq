/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

import org.libxmq.OutputSettings;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Entity;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;
import org.w3c.dom.Comment;

public class XMQPrinter
{
    boolean is_comment_node(Node node)
    {
        return node.getNodeType() == Node.COMMENT_NODE;
    }

    boolean is_document_node(Node node)
    {
        return node.getNodeType() == Node.DOCUMENT_NODE;
    }

    boolean is_element_node(Node node)
    {
        return node.getNodeType() == Node.ELEMENT_NODE;
    }

    boolean is_entity_node(Node node)
    {
        return node.getNodeType() == Node.ENTITY_NODE ||
            node.getNodeType() == Node.ENTITY_REFERENCE_NODE;
    }

    boolean is_content_node(Node node)
    {
        return node.getNodeType() == Node.TEXT_NODE
            || node.getNodeType() == Node.CDATA_SECTION_NODE;
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

    boolean need_separation_before_entity(XMQPrintState ps)
    {
        // No space needed for:
        // 'x y z'&nbsp;
        // =&nbsp;
        // {&nbsp;
        // }&nbsp;
        // ;&nbsp;
        // Otherwise a space is needed:
        // xyz &nbsp;
        char c = ps.last_char;
        return c != 0 && c != '=' && c != '\'' && c != '"' && c != '{' && c != '}' && c != ';' && c != '(' && c != ')';
    }

    void print_white_spaces(XMQPrintState ps, int num)
    {
        OutputSettings os = ps.output_settings;
        XMQTheme c = ps.theme;

        if (c != null && c.whitespace.pre() != null) ps.buffer.append(c.whitespace.pre());

        for (int i=0; i<num; ++i)
        {
            ps.buffer.append(c.indentation_space);
        }
        ps.current_indent += num;
        if (c != null && c.whitespace.post() != null) ps.buffer.append(c.whitespace.post());
    }

    void print_nl_and_indent(XMQPrintState ps, String prefix, String postfix)
    {
        OutputSettings os = ps.output_settings;
        XMQTheme c = ps.theme;

        if (postfix != null) ps.buffer.append(postfix);
        ps.buffer.append(c.explicit_nl);
        ps.current_indent = 0;
        ps.last_char = 0;
        print_white_spaces(ps, ps.line_indent);

        if (ps.restart_line != null) ps.buffer.append(ps.restart_line);
        if (prefix != null) ps.buffer.append(prefix);
    }

    void print_nl(XMQPrintState ps, String prefix, String postfix)
    {
        OutputSettings os = ps.output_settings;
        XMQTheme c = ps.theme;

        if (postfix != null) ps.buffer.append(postfix);
        ps.buffer.append(c.explicit_nl);
        ps.current_indent = 0;
        ps.last_char = 0;
        if (ps.restart_line != null) ps.buffer.append(ps.restart_line);
        if (prefix != null) ps.buffer.append(prefix);
    }

    void print_element_node(XMQPrintState ps, Node node)
    {
        Element element = (Element)node;

        ps.buffer.append(element.getTagName());
        NodeList children = element.getChildNodes();

        if (children.getLength() > 0)
        {
            ps.buffer.append("\n");
            ps.indent();
            ps.buffer.append("{\n");
            ps.current_indent += 4;
            for (int i = 0; i < children.getLength(); i++)
            {
                Node child = children.item(i);
                print_node(ps, child, 0);
            }
            ps.current_indent -= 4;
            ps.indent();
            ps.buffer.append("}\n");
        }
    }

    void print_document_node(XMQPrintState ps, Node node)
    {
        Document root = (Document)node;

        NodeList children = root.getChildNodes();

        if (children.getLength() > 0)
        {
            for (int i = 0; i < children.getLength(); i++)
            {
                Node child = children.item(i);
                print_node(ps, child, 0);
            }
        }
    }

    void print_content_node(XMQPrintState ps, Node node)
    {
        Text text = (Text)node;
        ps.indent();
        ps.buffer.append(">>>"+text.getNodeValue()+"<<<\n");
    }

    void print_comment_node(XMQPrintState ps, Node node)
    {
        Comment c = (Comment)node;
        ps.indent();
        ps.buffer.append("///*"+c.getTextContent()+"*///\n");
    }

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

    void print_entity_node(XMQPrintState ps, Node node)
    {
        Entity entity = (Entity)node;

        check_space_before_entity_node(ps);
/*
        print_utf8(ps, COLOR_entity, 1, "&", NULL);
        print_utf8(ps, COLOR_entity, 1, (const char*)node->name, NULL);
        print_utf8(ps, COLOR_entity, 1, ";", NULL);*/
    }

    public void print_node(XMQPrintState ps, Node node, int align)
    {
        // Standalone quote must be quoted: 'word' 'some words'
        if (is_content_node(node))
        {
            print_content_node(ps, node);
            return;
        }
        if (is_element_node(node))
        {
            print_element_node(ps, node);
            return;
        }
        if (is_entity_node(node))
        {
            print_entity_node(ps, node);
            return;
        }
        if (is_document_node(node))
        {
            print_document_node(ps, node);
            return;
        }
        if (is_comment_node(node))
        {
            print_comment_node(ps, node);
            return;
        }

    }
}
