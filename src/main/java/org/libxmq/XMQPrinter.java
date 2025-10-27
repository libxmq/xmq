package org.libxmq;

import org.w3c.dom.Node;

public class XMQPrinter
{
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

    void print_content_node(XMQPrintState ps, Node node)
    {
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
    }
}
