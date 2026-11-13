/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;

/**
 * Print the xmq DOM as clines, one /path/to/node="value" per line, a port of
 * xmq_print_clines and the cline_print functions in the C xmq.c.
 */
final class Clines
{
    private Clines()
    {
    }

    /**
     * Print the document (or node) as clines.
     * @param doc The document.
     * @return The clines.
     */
    static String print(XmlNode doc)
    {
        StringBuilder b = new StringBuilder();
        for (XmlNode i = doc.firstChild(); i != null; i = i.next()) node(b, i);
        return b.toString();
    }

    // The path to the node, the document gives the leading empty step.
    static void xpath(StringBuilder b, XmlNode node)
    {
        if (node == null) return;
        if (node.parent() != null)
        {
            xpath(b, node.parent());
            b.append('/');
        }
        if (node.type() == XmlNodeType.ELEMENT) b.append(node.name());
    }

    static void node(StringBuilder b, XmlNode node)
    {
        if (node instanceof XmlElement e)
        {
            for (XmlAttr a : e.attributes()) attr(b, e, a);
        }
        if (node.type() == XmlNodeType.TEXT || node.type() == XmlNodeType.CDATA)
        {
            xpath(b, node.parent());
            b.append("=\"").append(JsonPrinter.quote(node.content(), false)).append("\"\n");
        }
        else if (node.firstChild() != null)
        {
            for (XmlNode i = node.firstChild(); i != null; i = i.next()) node(b, i);
        }
        else if (node.type() == XmlNodeType.ELEMENT)
        {
            xpath(b, node);
            b.append("=\"\"\n");
        }
    }

    static void attr(StringBuilder b, XmlElement parent, XmlAttr a)
    {
        xpath(b, parent);
        b.append("/@");
        if (a.ns() != null && a.ns().prefix() != null) b.append(a.ns().prefix()).append(':');
        b.append(a.name()).append('=');
        if (a.firstChild() != null) b.append(JsonPrinter.quote(a.textContent(), true));
        else b.append("\"\"");
        b.append('\n');
    }
}
