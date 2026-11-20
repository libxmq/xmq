/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */
package org.libxmq.imp.ixml.earley;

import org.libxmq.imp.ixml.earley.Grammar.Call;
import org.libxmq.imp.ixml.earley.Grammar.Chars;
import org.libxmq.imp.ixml.earley.Grammar.Insertion;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlText;

/**
 * Serialize a parse tree into an xml document, following the ixml marks:
 *
 * - ^ (or no mark) a rule becomes an element, with its content inside.
 * - @ a rule becomes an attribute of the nearest element, its value is the
 *   text of everything inside it.
 * - - a hidden rule, its content goes directly into its parent.
 *     A hidden terminal, -'x', is dropped.
 *
 * The mark of a call wins over the mark of the rule. Inside an attribute
 * every rule is hidden, only the text counts. Insertions add their text.
 *
 * An element must not get the same attribute twice, that is the ixml dynamic
 * error D02.
 */
public final class XmlWriter
{
    /** The namespace of the ixml:state attribute. */
    public static final String IXML_NS = "http://invisiblexml.org/NS";

    /** A dynamic error, eg D02, found when serializing. */
    public static final class DynamicError extends Exception
    {
        private static final long serialVersionUID = 1L;

        DynamicError(String message)
        {
            super(message);
        }
    }

    private XmlWriter() { }

    /** Serialize the tree of the start rule.
     * @param tree the tree from TreeBuilder
     * @param grammar the grammar
     * @return the document
     * @throws DynamicError on a dynamic error, eg D02
     */
    public static XmlDocument toXml(Tree tree, Grammar grammar) throws DynamicError
    {
        // The start rule is written like a call of it without a mark of its own.
        // It is written into a holder element, its first child is the root.
        XmlElement holder = new XmlElement("holder");
        write(tree.as(new Call(grammar.start, (char)0, null)), holder, null, 0);

        XmlDocument doc = new XmlDocument();
        XmlNode root = holder.firstChild();
        if (root instanceof XmlElement e)
        {
            e.unlink();
            doc.appendRaw(e);
            if (tree.ambiguous())
            {
                XmlNs ns = e.declareNs("ixml", IXML_NS);
                e.addAttribute(new org.libxmq.imp.xml.XmlAttr("state", "ambiguous")).setNs(ns);
            }
        }
        return doc;
    }

    /**
     * Write the node into the parent element. If attribute is not null we are
     * inside an attribute and only collect its text.
     */
    /** Like the other parsers, more nested elements than this is an error, as in C. */
    static final int MAX_NESTING_DEPTH = 10000;

    private static void write(Tree node, XmlElement parent, StringBuilder attribute, int depth) throws DynamicError
    {
        if (node.symbol() instanceof Chars chars)
        {
            if (!chars.hidden()) text(new String(Character.toChars(node.codepoint())), parent, attribute);
            return;
        }
        if (node.symbol() instanceof Insertion insertion)
        {
            text(insertion.text(), parent, attribute);
            return;
        }
        if (!(node.symbol() instanceof Call call)) return; // A not, it outputs nothing.

        char mark = attribute != null ? '-' : call.effectiveMark();
        String name = call.effectiveName();
        switch (mark)
        {
        case '^' -> {
            if (depth >= MAX_NESTING_DEPTH)
            {
                throw new DynamicError("dynamic error: nesting is too deep, the output has more than 10000 nested elements");
            }
            XmlElement element = new XmlElement(name);
            parent.appendRaw(element);
            writeChildren(node, element, null, depth + 1);
        }
        case '@' -> {
            StringBuilder value = new StringBuilder();
            writeChildren(node, parent, value, depth);
            if (parent.attribute(name) != null)
            {
                throw new DynamicError("dynamic error D02: the attribute " + name + " occurs more than once on an element");
            }
            parent.setAttribute(name, value.toString());
        }
        default -> writeChildren(node, parent, attribute, depth); // Hidden, the content goes into the parent.
        }
    }

    private static void writeChildren(Tree node, XmlElement parent, StringBuilder attribute, int depth) throws DynamicError
    {
        for (Tree child : node.children()) write(child, parent, attribute, depth);
    }

    private static void text(String s, XmlElement parent, StringBuilder attribute)
    {
        if (attribute != null) attribute.append(s);
        else parent.addChild(new XmlText(s)); // Merges with a text just before it.
    }
}
