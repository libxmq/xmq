package org.libxmq.imp;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Text;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;


public class XMQPrinter
{
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
        ps.buffer.append(">>>"+text.getNodeValue()+"<<<");
    }

    void print_entity_node(XMQPrintState ps, Node node)
    {
//        check_space_before_entity_node(ps);
/*
        print_utf8(ps, COLOR_entity, 1, "&", NULL);
        print_utf8(ps, COLOR_entity, 1, (const char*)node->name, NULL);
        print_utf8(ps, COLOR_entity, 1, ";", NULL);*/
    }

    void print_node(XMQPrintState ps, Node node, int align)
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
        if (is_document_node(node))
        {
            print_document_node(ps, node);
            return;
        }

    }
}
