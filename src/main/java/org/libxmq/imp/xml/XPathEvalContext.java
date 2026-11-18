/* This file is ported from libxml2 (xpath.c).
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

import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

/**
 * An evaluation context, like XmlXPathContext in the C version (used by xslt): the document
 * order index and the id tables of the trees seen so far are kept between evaluations, and
 * variables and extension functions are resolved through callbacks. The trees must not
 * change while the context knows them: call {@link #forget} before a tree is changed.
 */
public final class XPathEvalContext
{
    /** Resolves a variable reference, returns null if the variable is not defined. */
    public interface VariableLookup
    {
        /**
         * Look up a variable.
         * @param name The local name.
         * @param ns_uri The namespace uri, null for an unprefixed name.
         * @return The value, or null if not defined. The caller does not change it.
         */
        XPathResult lookup(String name, String ns_uri);
    }

    /** An extension function. */
    public interface Function
    {
        /**
         * Call the function.
         * @param ctx The context, for the context node, position, size and errors.
         * @param args The arguments.
         * @return The result, or null on an error (set with {@link XPathEvalContext#setError}).
         */
        XPathResult call(XPathEvalContext ctx, XPathResult[] args);
    }

    /** Finds extension functions. */
    public interface FunctionLookup
    {
        /**
         * Find a function.
         * @param name The local name.
         * @param ns_uri The namespace uri, null for an unprefixed name.
         * @return The function or null.
         */
        Function lookup(String name, String ns_uri);
    }

    private static final class Tree
    {
        IdentityHashMap<XmlNode, Long> order;
        Map<String, XmlNode> ids;
    }

    private final IdentityHashMap<XmlNode, Tree> trees_ = new IdentityHashMap<>();
    private Tree last_;
    private long next_ordinal_;

    VariableLookup var_lookup_;
    FunctionLookup func_lookup_;

    XPathContext cur_;        // The evaluation calling an extension function.
    int func_error_;
    boolean func_error_quiet_;

    /** A new context. */
    public XPathEvalContext()
    {
    }

    /**
     * Set the variable lookup.
     * @param lookup The lookup, null for none.
     */
    public void setVariableLookup(VariableLookup lookup)
    {
        var_lookup_ = lookup;
    }

    /**
     * Set the extension function lookup.
     * @param lookup The lookup, null for none.
     */
    public void setFunctionLookup(FunctionLookup lookup)
    {
        func_lookup_ = lookup;
    }

    /**
     * Forget the indexes of the tree with this root, before it is changed.
     * @param root The root of the tree.
     */
    public void forget(XmlNode root)
    {
        if (root == null) return;
        trees_.remove(root);
        last_ = null;
    }

    /**
     * Evaluate a compiled expression.
     * @param comp The expression.
     * @param node The context node.
     * @param position The context position, -1 if unknown (then position() is an error).
     * @param size The context size, -1 if unknown (then last() is an error).
     * @return The value.
     * @throws XPathException On an error.
     */
    public XPathResult eval(XPath comp, XmlNode node, long position, long size) throws XPathException
    {
        return comp.evalIn(this, node, position, size);
    }

    // For extension functions, during an evaluation.

    /**
     * The context node of the evaluation calling the function.
     * @return The node or null.
     */
    public XmlNode contextNode()
    {
        return cur_ != null ? cur_.node : null;
    }

    /**
     * The context position of the evaluation calling the function.
     * @return The position or -1.
     */
    public long position()
    {
        return cur_ != null ? cur_.position : -1;
    }

    /**
     * The context size of the evaluation calling the function.
     * @return The size or -1.
     */
    public long size()
    {
        return cur_ != null ? cur_.size : -1;
    }

    /**
     * Look up a prefix in the namespaces of the expression calling the function.
     * @param prefix The prefix.
     * @return The uri or null.
     */
    public String lookupNamespace(String prefix)
    {
        if (cur_ == null || prefix == null) return null;
        return cur_.lookupNs(prefix);
    }

    /**
     * The element with the id in the document of node.
     * @param node A node of the document.
     * @param id The id.
     * @return The element or null.
     */
    public XmlNode getId(XmlNode node, String id)
    {
        if (node == null || id == null) return null;
        XPathContext c = new XPathContext(this, new XPathSettings(), "", node, -1, -1, false);
        return lookupId(c, id);
    }

    /**
     * Set the error of a failing extension function (it returns null).
     * @param code The XPathException code.
     */
    public void setError(int code)
    {
        func_error_ = code;
        func_error_quiet_ = false;
    }

    /**
     * Set the error of a failing extension function that reported its own message.
     * @param code The XPathException code.
     */
    public void setErrorQuiet(int code)
    {
        func_error_ = code;
        func_error_quiet_ = true;
    }

    /**
     * Remove duplicates from a node set and sort it in document order, like a union.
     * @param r The node set.
     */
    public void sort(XPathResult r)
    {
        if (r == null || r.type() != XPathResult.NODESET || r.size() < 2) return;
        List<XmlNode> l = r.nodes_;
        IdentityHashMap<XmlNode, Boolean> seen = new IdentityHashMap<>();
        int k = 0;
        for (int i = 0; i < l.size(); i++)
        {
            XmlNode n = l.get(i);
            if (seen.put(n, Boolean.TRUE) != null) continue;
            l.set(k++, n);
        }
        while (l.size() > k) l.remove(l.size() - 1);
        XPathContext c = new XPathContext(this, new XPathSettings(), "", null, -1, -1, false);
        c.sort(l);
    }

    // Document order and ids.

    private static XmlNode rootOf(XmlNode n)
    {
        while (n.parent() != null) n = n.parent();
        return n;
    }

    private Tree tree(XmlNode root)
    {
        Tree t = trees_.get(root);
        if (t == null)
        {
            t = new Tree();
            trees_.put(root, t);
        }
        return t;
    }

    long ordinal(XmlNode n)
    {
        if (n == null) return -1;
        if (last_ != null && last_.order != null)
        {
            Long o = last_.order.get(n);
            if (o != null) return o;
        }
        Tree t = tree(rootOf(n));
        if (t.order == null) indexTree(t, rootOf(n));
        last_ = t;
        Long o = t.order.get(n);
        return o != null ? o : -1;
    }

    /** Assign preorder ordinals: node, its attributes, its children. */
    private void indexTree(Tree t, XmlNode root)
    {
        t.order = new IdentityHashMap<>();
        XmlNode n = root;
        while (n != null)
        {
            t.order.put(n, next_ordinal_);
            next_ordinal_ += 2;
            if (n instanceof XmlElement)
            {
                for (XmlAttr a : ((XmlElement)n).attributes())
                {
                    t.order.put(a, next_ordinal_);
                    next_ordinal_ += 2;
                }
            }
            if (n.firstChild() != null)
            {
                n = n.firstChild();
                continue;
            }
            while (n != null && n != root && n.next() == null) n = n.parent();
            if (n == null || n == root) break;
            n = n.next();
        }
    }

    /** Like libxml2 xmlGetID, the element with the ID attribute value in the document of c. */
    XmlNode lookupId(XPathContext c, String id)
    {
        if (c.doc == null) return null;
        Tree t = tree(rootOf(c.doc));
        if (t.ids == null)
        {
            t.ids = new HashMap<>();
            c.collectIds(t.ids);
        }
        return t.ids.get(id);
    }
}
