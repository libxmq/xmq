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

package org.libxmq.imp.xml;

import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;

/**
 * Axis traversal for the xpath engine. Each axis is a port of the
 * corresponding libxml2 xmlXPathNext* function in xpath.c (Copyright (C)
 * 1998-2012 Daniel Veillard, MIT license), including which node types
 * are visited, eg the dtd node is skipped by the descendant axis.
 *
 * Entity references: in libxml2 the children/last of an entity reference
 * node is the entity declaration, whose children are the parsed entity
 * content. The descendant axis never enters them, but the following and
 * preceding axes do, which makes the preceding axis wander into the dtd
 * and the following axis loop forever (libxml2 then fails with a memory
 * error, or hangs). This is emulated with {@link XPathContext#entityContent}.
 */
final class XPathAxis
{
    private XPathAxis()
    {
    }

    /**
     * Receives the nodes on an axis, applies the node test and implements
     * the libxml2 early exits: "[n]" (axis range) and the first hit when
     * only a boolean result is needed.
     */
    static final class Collector
    {
        final XPathExpr.Step step;
        final String uri;
        final List<XmlNode> seq = new ArrayList<>();
        final boolean range;
        final int max_pos;
        final boolean break_on_first_hit;
        int pos;
        int matches;

        Collector(XPathExpr.Step step, String uri, boolean range, int max_pos, boolean break_on_first_hit)
        {
            this.step = step;
            this.uri = uri;
            this.range = range;
            this.max_pos = max_pos;
            this.break_on_first_hit = break_on_first_hit;
        }

        /** Test a node, returns true if the traversal should stop. */
        boolean visit(XmlNode n)
        {
            if (!step.matches(n, uri)) return false;
            matches++;
            if (range)
            {
                if (++pos == max_pos)
                {
                    seq.add(n);
                    return true;
                }
                return false;
            }
            seq.add(n);
            return break_on_first_hit;
        }

        boolean collectsAll()
        {
            return !range && !break_on_first_hit;
        }
    }

    /** Traverse the axis of the step from the context node. */
    static void traverse(XmlNode c, XPathContext ctx, Collector col) throws XPathException
    {
        XmlNode doc = ctx.doc;
        switch (col.step.axis)
        {
        case XPathExpr.AXIS_ATTRIBUTE:
            if (c.type() == XmlNodeType.ELEMENT)
            {
                for (XmlAttr a : ((XmlElement)c).attributes())
                {
                    if (col.visit(a)) return;
                }
            }
            return;
        case XPathExpr.AXIS_NAMESPACE:
            if (c.type() == XmlNodeType.ELEMENT)
            {
                for (XmlNode n : namespaceNodes((XmlElement)c))
                {
                    if (col.visit(n)) return;
                }
            }
            return;
        case XPathExpr.AXIS_SELF:
            col.visit(c);
            return;
        case XPathExpr.AXIS_CHILD:
            for (XmlNode cur = nextChild(c, null, ctx); cur != null; cur = nextChild(c, cur, ctx))
            {
                if (col.visit(cur)) return;
            }
            return;
        case XPathExpr.AXIS_DESCENDANT:
            for (XmlNode cur = nextDescendant(c, null, ctx); cur != null; cur = nextDescendant(c, cur, ctx))
            {
                if (col.visit(cur)) return;
            }
            return;
        case XPathExpr.AXIS_DESCENDANT_OR_SELF:
            if (col.visit(c)) return;
            if (isAttrOrNs(c)) return;
            for (XmlNode cur = nextDescendant(c, c, ctx); cur != null; cur = nextDescendant(c, cur, ctx))
            {
                if (col.visit(cur)) return;
            }
            return;
        case XPathExpr.AXIS_PARENT:
        {
            XmlNode p = parentOf(c, doc);
            if (p != null) col.visit(p);
            return;
        }
        case XPathExpr.AXIS_ANCESTOR:
        case XPathExpr.AXIS_ANCESTOR_OR_SELF:
        {
            XmlNode first;
            if (col.step.axis == XPathExpr.AXIS_ANCESTOR_OR_SELF)
            {
                if (col.visit(c)) return;
                first = nextAncestor(c, doc);
            }
            else
            {
                first = parentOf(c, doc);
            }
            for (XmlNode cur = first; cur != null; cur = nextAncestor(cur, doc))
            {
                if (col.visit(cur)) return;
            }
            return;
        }
        case XPathExpr.AXIS_FOLLOWING_SIBLING:
            if (isAttrOrNs(c)) return;
            for (XmlNode cur = c.next(); cur != null; cur = cur.next())
            {
                if (col.visit(cur)) return;
            }
            return;
        case XPathExpr.AXIS_PRECEDING_SIBLING:
        {
            if (isAttrOrNs(c)) return;
            XmlNode cur = c.prev();
            while (cur != null)
            {
                if (col.visit(cur)) return;
                if (cur.prev() != null && cur.prev().type() == XmlNodeType.DTD) cur = cur.prev();
                cur = cur.prev();
            }
            return;
        }
        case XPathExpr.AXIS_FOLLOWING:
            following(c, ctx, col);
            return;
        case XPathExpr.AXIS_PRECEDING:
            preceding(c, ctx, col);
            return;
        default:
            return;
        }
    }

    private static boolean isAttrOrNs(XmlNode n)
    {
        XmlNodeType t = n.type();
        return t == XmlNodeType.ATTRIBUTE || t == XmlNodeType.NAMESPACE;
    }

    private static boolean isDocument(XmlNode n)
    {
        XmlNodeType t = n.type();
        return t == XmlNodeType.DOCUMENT || t == XmlNodeType.HTML_DOCUMENT;
    }

    /** The libxml2 children pointer, where entity references point to their declaration. */
    private static XmlNode fc(XmlNode n, XPathContext ctx)
    {
        switch (n.type())
        {
        case ENTITY_REF:
            return ((XmlEntityRef)n).entity();
        case ENTITY_DECL:
            return ctx.entityContent(n);
        default:
            return n.firstChild();
        }
    }

    /** The libxml2 last pointer, where entity references point to their declaration. */
    private static XmlNode lc(XmlNode n, XPathContext ctx)
    {
        switch (n.type())
        {
        case ENTITY_REF:
            return ((XmlEntityRef)n).entity();
        case ENTITY_DECL:
            return ctx.entityContent(n);
        default:
            return n.lastChild();
        }
    }

    /** Like libxml2 xmlXPathNextChild. */
    private static XmlNode nextChild(XmlNode c, XmlNode cur, XPathContext ctx)
    {
        if (cur == null)
        {
            switch (c.type())
            {
            case ELEMENT: case TEXT: case CDATA: case ENTITY_REF: case PI: case COMMENT: case DTD:
            case DOCUMENT: case HTML_DOCUMENT:
                return fc(c, ctx);
            default:
                return null;
            }
        }
        if (isDocument(cur)) return null;
        return cur.next();
    }

    /** Like libxml2 xmlXPathNextDescendant. */
    private static XmlNode nextDescendant(XmlNode c, XmlNode cur, XPathContext ctx)
    {
        if (cur == null)
        {
            if (isAttrOrNs(c)) return null;
            return fc(c, ctx);
        }
        if (cur.type() == XmlNodeType.NAMESPACE) return null;
        XmlNode ch = fc(cur, ctx);
        if (ch != null && ch.type() != XmlNodeType.ENTITY_DECL)
        {
            cur = ch;
            if (cur.type() != XmlNodeType.DTD) return cur;
        }
        if (cur == c) return null;
        while (cur.next() != null)
        {
            cur = cur.next();
            if (cur.type() != XmlNodeType.ENTITY_DECL && cur.type() != XmlNodeType.DTD) return cur;
        }
        do
        {
            cur = cur.parent();
            if (cur == null) break;
            if (cur == c) return null;
            if (cur.next() != null)
            {
                cur = cur.next();
                return cur;
            }
        }
        while (cur != null);
        return null;
    }

    /** Like libxml2 xmlXPathNextParent (and the first step of xmlXPathNextAncestor). */
    private static XmlNode parentOf(XmlNode c, XmlNode doc)
    {
        switch (c.type())
        {
        case DOCUMENT: case HTML_DOCUMENT:
            return null;
        case ATTRIBUTE: case NAMESPACE:
            return c.parent();
        default:
            if (c.parent() == null)
            {
                return (doc != null && doc != c && isDocument(doc)) ? doc : null;
            }
            return c.parent();
        }
    }

    /** Like libxml2 xmlXPathNextAncestor for the following steps. */
    private static XmlNode nextAncestor(XmlNode cur, XmlNode doc)
    {
        if (doc != null && cur == doc.firstChild()) return doc;
        if (cur == doc) return null;
        if (isDocument(cur)) return null;
        return cur.parent();
    }

    /** Like libxml2 xmlXPathNextFollowing, with detection of the endless entity loop. */
    private static void following(XmlNode c, XPathContext ctx, Collector col) throws XPathException
    {
        XmlNode doc = ctx.doc;
        IdentityHashMap<XmlNode, Integer> entered = null;
        XmlNode cur = null;
        for (;;)
        {
            // Compute the next node.
            XmlNode ch = (cur != null && !isAttrOrNs(cur)) ? fc(cur, ctx) : null;
            if (ch != null)
            {
                if (cur.type() == XmlNodeType.ENTITY_REF)
                {
                    // Entering an entity declaration, which leads back into the dtd
                    // and then to the same entity reference again.
                    if (entered == null) entered = new IdentityHashMap<>();
                    Integer m = entered.get(cur);
                    if (m != null && (col.collectsAll() || m == col.matches))
                    {
                        throw ctx.error(XPathException.MEMORY_ERROR);
                    }
                    entered.put(cur, col.matches);
                }
                cur = ch;
            }
            else
            {
                if (cur == null)
                {
                    cur = c;
                    if (isAttrOrNs(cur)) cur = cur.parent();
                    if (cur == null) return;
                }
                if (cur.next() != null)
                {
                    cur = cur.next();
                }
                else
                {
                    do
                    {
                        cur = cur.parent();
                        if (cur == null) return;
                        if (cur == doc) return;
                    }
                    while (cur.next() == null);
                    cur = cur.next();
                }
            }
            if (col.visit(cur)) return;
        }
    }

    /** Like libxml2 xmlXPathNextPrecedingInternal. */
    private static void preceding(XmlNode c, XPathContext ctx, Collector col)
    {
        XmlNode cur = c;
        if (isAttrOrNs(cur)) cur = cur.parent();
        if (cur == null) return;
        XmlNode ancestor = cur.parent();
        XmlNode doc_children = ctx.doc == null ? null : ctx.doc.firstChild();
        for (;;)
        {
            if (cur.type() == XmlNodeType.NAMESPACE) return;
            if (cur.prev() != null && cur.prev().type() == XmlNodeType.DTD) cur = cur.prev();
            boolean found = false;
            while (cur.prev() == null)
            {
                cur = cur.parent();
                if (cur == null) return;
                if (cur == doc_children) return;
                if (cur != ancestor)
                {
                    found = true;
                    break;
                }
                ancestor = cur.parent();
            }
            if (!found)
            {
                cur = cur.prev();
                for (XmlNode l = lc(cur, ctx); l != null; l = lc(cur, ctx)) cur = l;
            }
            if (col.visit(cur)) return;
        }
    }

    /**
     * The namespace axis of an element, like libxml2 xmlXPathNextNamespace:
     * first the xml namespace, then the in scope namespaces (from
     * xmlGetNsList, innermost declaration wins) in reverse order.
     */
    static List<XmlNode> namespaceNodes(XmlElement e)
    {
        List<XmlNs> list = new ArrayList<>();
        for (XmlNode n = e; n != null; n = n.parent())
        {
            if (n.type() != XmlNodeType.ELEMENT) continue;
            for (XmlNs ns : ((XmlElement)n).nsDefs())
            {
                boolean dup = false;
                for (XmlNs o : list)
                {
                    if (java.util.Objects.equals(o.prefix(), ns.prefix()))
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup) list.add(ns);
            }
        }
        List<XmlNode> out = new ArrayList<>(list.size() + 1);
        int index = 0;
        out.add(new XPathNamespaceNode(e, "xml", XPathUtil.XML_NAMESPACE, index++));
        for (int i = list.size() - 1; i >= 0; i--)
        {
            XmlNs ns = list.get(i);
            out.add(new XPathNamespaceNode(e, ns.prefix(), ns.href(), index++));
        }
        return out;
    }
}
