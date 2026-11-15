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

/* Portions of this file are ported from libxml2 (xpath.c).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


package org.libxmq.imp.xml;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/**
 * The evaluation state of an xpath expression, like the libxml2
 * xmlXPathContext: context node, size and position, the namespace and
 * variable bindings. Also implements document order and the id lookup.
 * Ported from libxml2 xpath.c (Copyright (C) 1998-2012 Daniel Veillard,
 * MIT license).
 */
final class XPathContext
{
    /** Sets larger than this are sorted using a precomputed document order index. */
    private static final int INDEX_SORT_THRESHOLD = 32;

    final XPathEvalContext px;  // Kept caches and callbacks.
    final XPathSettings settings;
    final String expr;
    final boolean evaluating;   // An evaluation of a compiled expression, errors have no expression context.
    XmlNode node;
    XmlNode doc;
    long size = -1;
    long position = -1;

    XPathContext(XPathEvalContext px, XPathSettings settings, String expr, XmlNode context,
                 long position, long size, boolean evaluating)
    {
        this.px = px;
        this.settings = settings;
        this.expr = expr;
        this.node = settings.noContextNode ? null : context;
        this.doc = documentOf(context);
        this.position = position;
        this.size = size;
        this.evaluating = evaluating;
    }

    /** Returns the document of a node, or the top most ancestor if the node is not in a document. */
    static XmlNode documentOf(XmlNode n)
    {
        if (n == null) return null;
        if (n.doc() != null) return n.doc();
        XmlNode i = n;
        while (i.parent() != null) i = i.parent();
        return i;
    }

    XPathException error(int code)
    {
        XPathException e = new XPathException(code, expr, -1);
        e.setEvaluating(evaluating);
        return e;
    }

    /** An error with a detailed message, like libxml2 xmlXPathErrFmt. */
    XPathException errorDetail(int code, String what, String prefix, String name)
    {
        XPathException e = XPathException.detailed(code, expr, -1, what, prefix, name);
        e.setEvaluating(evaluating);
        return e;
    }

    /** Like libxml2 xmlXPathNsLookup. */
    String lookupNs(String prefix)
    {
        if ("xml".equals(prefix)) return XPathUtil.XML_NAMESPACE;
        return settings.namespaces.get(prefix);
    }

    // ----------------------------------------------------------------------
    // Document order

    /** Sort a node set in document order, like libxml2 xmlXPathNodeSetSort. */
    void sort(List<XmlNode> nodes)
    {
        if (nodes.size() <= 1) return;
        for (XmlNode n : nodes)
        {
            if (n instanceof XPathNamespaceNode)
            {
                sortWithNamespaces(nodes);
                return;
            }
        }
        if (nodes.size() > INDEX_SORT_THRESHOLD)
        {
            nodes.sort(indexComparator());
        }
        else
        {
            nodes.sort(XPathContext::compareTurtle);
        }
    }

    /**
     * Sort a node set containing namespace nodes. libxml2 xmlXPathCmpNodesExt
     * claims that a namespace node comes before any other node, which with
     * the binary insertion sort of libxml2 timsort (used for sets smaller
     * than 64 nodes) means that namespace nodes are never moved. The same
     * insertion sort is used here (for all sizes).
     */
    private void sortWithNamespaces(List<XmlNode> nodes)
    {
        Comparator<XmlNode> doc_order = nodes.size() > INDEX_SORT_THRESHOLD ? indexComparator()
                                                                            : XPathContext::compareTurtle;
        Comparator<XmlNode> cmp = (x, y) ->
        {
            if (x == y) return 0;
            if (x instanceof XPathNamespaceNode || y instanceof XPathNamespaceNode) return -1;
            return Integer.signum(doc_order.compare(x, y));
        };
        XmlNode[] dst = nodes.toArray(new XmlNode[0]);
        int size = dst.length;
        for (int i = 1; i < size; i++)
        {
            if (cmp.compare(dst[i - 1], dst[i]) <= 0) continue;
            XmlNode x = dst[i];
            int location = binaryInsertionFind(dst, x, i, cmp);
            for (int j = i - 1; j >= location; j--)
            {
                dst[j + 1] = dst[j];
            }
            dst[location] = x;
        }
        for (int i = 0; i < size; i++) nodes.set(i, dst[i]);
    }

    /** Like BINARY_INSERTION_FIND in libxml2 timsort.h. */
    private static int binaryInsertionFind(XmlNode[] dst, XmlNode x, int size, Comparator<XmlNode> cmp)
    {
        int l = 0;
        int r = size - 1;
        int c = r >> 1;
        if (cmp.compare(x, dst[0]) < 0) return 0;
        if (cmp.compare(x, dst[r]) > 0) return r;
        XmlNode cx = dst[c];
        for (;;)
        {
            int val = cmp.compare(x, cx);
            if (val < 0)
            {
                if (c - l <= 1) return c;
                r = c;
            }
            else
            {
                if (r - c <= 1) return c + 1;
                l = c;
            }
            c = l + ((r - l) >> 1);
            cx = dst[c];
        }
    }

    /** Find the first node in document order. */
    XmlNode first(List<XmlNode> nodes)
    {
        for (XmlNode n : nodes)
        {
            if (n instanceof XPathNamespaceNode)
            {
                List<XmlNode> copy = new ArrayList<>(nodes);
                sortWithNamespaces(copy);
                return copy.get(0);
            }
        }
        Comparator<XmlNode> cmp = nodes.size() > INDEX_SORT_THRESHOLD ? indexComparator() : XPathContext::compareTurtle;
        XmlNode best = null;
        for (XmlNode n : nodes)
        {
            if (best == null || cmp.compare(n, best) < 0) best = n;
        }
        return best;
    }

    private Comparator<XmlNode> indexComparator()
    {
        return (a, b) ->
        {
            if (a == b) return 0;
            XmlNode ba = a instanceof XPathNamespaceNode ? a.parent() : a;
            XmlNode bb = b instanceof XPathNamespaceNode ? b.parent() : b;
            long oa = ordinal(ba);
            long ob = ordinal(bb);
            if (oa != ob) return Long.compare(oa, ob);
            // Same base node, ie at least one is a namespace node of the other/same element.
            boolean na = a instanceof XPathNamespaceNode;
            boolean nb = b instanceof XPathNamespaceNode;
            if (na && nb) return Integer.compare(((XPathNamespaceNode)a).index(), ((XPathNamespaceNode)b).index());
            return na ? 1 : -1;
        };
    }

    private long ordinal(XmlNode n)
    {
        return px.ordinal(n);
    }

    /** Kind of a node relative to its parent: 0 namespace, 1 attribute, 2 child. */
    private static int kind(XmlNode n)
    {
        if (n instanceof XPathNamespaceNode) return 0;
        if (n instanceof XmlAttr) return 1;
        return 2;
    }

    /** Compare two nodes in document order by walking the tree. */
    static int compareTurtle(XmlNode a, XmlNode b)
    {
        if (a == b) return 0;
        if (a.equals(b)) return 0;
        List<XmlNode> pa = ancestry(a);
        List<XmlNode> pb = ancestry(b);
        if (pa.get(0) != pb.get(0))
        {
            // Different trees, in the order they were created (the C version compares the
            // addresses of the roots, like libxml2).
            return Long.compare(pa.get(0).seq_, pb.get(0).seq_);
        }
        int i = 0;
        while (i < pa.size() && i < pb.size() && pa.get(i) == pb.get(i)) i++;
        if (i == pa.size()) return -1; // a is an ancestor of b
        if (i == pb.size()) return 1;  // b is an ancestor of a
        XmlNode x = pa.get(i);
        XmlNode y = pb.get(i);
        XmlNode parent = pa.get(i - 1);
        int kx = kind(x);
        int ky = kind(y);
        if (kx != ky) return Integer.compare(kx, ky);
        if (kx == 0)
        {
            return Integer.compare(((XPathNamespaceNode)x).index(), ((XPathNamespaceNode)y).index());
        }
        if (kx == 1)
        {
            List<XmlAttr> attrs = ((XmlElement)parent).attributes();
            return Integer.compare(attrs.indexOf(x), attrs.indexOf(y));
        }
        for (XmlNode s = x.next(); s != null; s = s.next())
        {
            if (s == y) return -1;
        }
        return 1;
    }

    private static List<XmlNode> ancestry(XmlNode n)
    {
        ArrayList<XmlNode> l = new ArrayList<>();
        for (XmlNode i = n; i != null; i = i.parent()) l.add(i);
        java.util.Collections.reverse(l);
        return l;
    }

    // ----------------------------------------------------------------------
    // Entities

    // ----------------------------------------------------------------------
    // Ids

    /** Like libxml2 xmlGetID, returns the element with the given ID attribute value. */
    XmlNode lookupId(String id)
    {
        return px.lookupId(this, id);
    }

    /** Collect the ids of the document of this context (the first element with an id wins). */
    void collectIds(Map<String, XmlNode> ids)
    {
        XmlDocument d = doc instanceof XmlDocument ? (XmlDocument)doc : null;
        Map<String, java.util.Set<String>> dtd_ids = d == null ? null : dtdIdAttributes(d);
        boolean html = settings.htmlIds;
        XmlNode n = doc;
        while (n != null)
        {
            if (n instanceof XmlElement)
            {
                XmlElement e = (XmlElement)n;
                for (XmlAttr a : e.attributes())
                {
                    if (isId(e, a, dtd_ids, html))
                    {
                        ids.putIfAbsent(a.value(), e);
                    }
                }
            }
            if (n.firstChild() != null && !(n instanceof XmlDtd) && !(n instanceof XmlEntityRef))
            {
                n = n.firstChild();
                continue;
            }
            while (n != null && n != doc && n.next() == null) n = n.parent();
            if (n == null || n == doc) break;
            n = n.next();
        }
    }

    /** Like libxml2 xmlIsID. */
    private static boolean isId(XmlElement e, XmlAttr a, Map<String, java.util.Set<String>> dtd_ids, boolean html)
    {
        XmlNs ns = a.ns();
        if (ns != null && ns.prefix() != null && "id".equals(a.name()) && "xml".equals(ns.prefix())) return true;
        if (html)
        {
            return "id".equals(a.name()) || ("name".equals(a.name()) && "a".equals(e.name()));
        }
        if (dtd_ids == null) return false;
        java.util.Set<String> s = dtd_ids.get(e.fullName());
        return s != null && s.contains(a.fullName());
    }

    /**
     * Find the attributes declared with type ID in the internal subset.
     * Returns null if there is no internal subset.
     */
    private static Map<String, java.util.Set<String>> dtdIdAttributes(XmlDocument d)
    {
        XmlDtd dtd = d.intSubset();
        if (dtd == null) return null;
        Map<String, java.util.Set<String>> m = new HashMap<>();
        for (XmlNode n = dtd.firstChild(); n != null; n = n.next())
        {
            if (n.type() == XmlNodeType.ATTRIBUTE_DECL && n instanceof XmlDtdDecl)
            {
                parseAttlist(((XmlDtdDecl)n).text(), m);
            }
        }
        return m;
    }

    /** Parse &lt;!ATTLIST elem attr type default ...&gt; and record attributes of type ID. */
    private static void parseAttlist(String text, Map<String, java.util.Set<String>> m)
    {
        if (text == null) return;
        List<String> toks = new ArrayList<>();
        int i = 0;
        int n = text.length();
        String t = text.trim();
        if (t.startsWith("<!ATTLIST")) i = text.indexOf("<!ATTLIST") + 9;
        while (i < n)
        {
            char c = text.charAt(i);
            if (XPathUtil.isBlank(c) || c == '>') { i++; continue; }
            int start = i;
            if (c == '"' || c == '\'')
            {
                int e = text.indexOf(c, i + 1);
                i = e < 0 ? n : e + 1;
            }
            else if (c == '(')
            {
                int e = text.indexOf(')', i + 1);
                i = e < 0 ? n : e + 1;
            }
            else
            {
                while (i < n && !XPathUtil.isBlank(text.charAt(i)) && text.charAt(i) != '>'
                       && text.charAt(i) != '(' && text.charAt(i) != '"' && text.charAt(i) != '\'') i++;
            }
            toks.add(text.substring(start, i));
        }
        if (toks.isEmpty()) return;
        String elem = toks.get(0);
        int k = 1;
        while (k + 1 < toks.size())
        {
            String attr = toks.get(k++);
            String type = toks.get(k++);
            if ("NOTATION".equals(type) && k < toks.size()) k++; // the (a|b) enumeration
            if (k < toks.size())
            {
                String def = toks.get(k++);
                if ("#FIXED".equals(def) && k < toks.size()) k++;
            }
            if ("ID".equals(type)) m.computeIfAbsent(elem, x -> new java.util.HashSet<>()).add(attr);
        }
    }
}
