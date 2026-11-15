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
import java.util.List;

/**
 * The value of an xpath evaluation, like XmlXPathResult in the C version: a node set
 * (possibly a result tree fragment), a boolean, a number or a string. Values are also
 * created by the caller, eg variable values and extension function results.
 */
public final class XPathResult
{
    public static final int NODESET = 1;
    public static final int BOOLEAN = 2;
    public static final int NUMBER = 3;
    public static final int STRING = 4;

    int type_;
    boolean b_;
    double num_;
    String str_;
    List<XmlNode> nodes_ = new ArrayList<>();
    boolean tree_;

    private XPathResult(int type)
    {
        type_ = type;
    }

    /**
     * A string value.
     * @param s The string, null is "".
     * @return The value.
     */
    public static XPathResult newString(String s)
    {
        XPathResult r = new XPathResult(STRING);
        r.str_ = s == null ? "" : s;
        return r;
    }

    /**
     * A number value.
     * @param d The number.
     * @return The value.
     */
    public static XPathResult newNumber(double d)
    {
        XPathResult r = new XPathResult(NUMBER);
        r.num_ = d;
        return r;
    }

    /**
     * A boolean value.
     * @param b The boolean.
     * @return The value.
     */
    public static XPathResult newBoolean(boolean b)
    {
        XPathResult r = new XPathResult(BOOLEAN);
        r.b_ = b;
        return r;
    }

    /**
     * An empty node set.
     * @return The value.
     */
    public static XPathResult newNodeSet()
    {
        return new XPathResult(NODESET);
    }

    /**
     * Add a node to a node set.
     * @param n The node, null is ignored.
     */
    public void addNode(XmlNode n)
    {
        if (n != null) nodes_.add(n);
    }

    /**
     * Mark a node set as a result tree fragment. Copies keep the mark.
     * @param tree True for a result tree fragment.
     */
    public void setTree(boolean tree)
    {
        if (type_ == NODESET) tree_ = tree;
    }

    /**
     * True if this is a node set that is a result tree fragment.
     * @return True for a result tree fragment.
     */
    public boolean isTree()
    {
        return type_ == NODESET && tree_;
    }

    /**
     * A copy of this value.
     * @return The copy.
     */
    public XPathResult copy()
    {
        XPathResult c = new XPathResult(type_);
        c.b_ = b_;
        c.num_ = num_;
        c.str_ = str_;
        c.tree_ = tree_;
        c.nodes_ = new ArrayList<>(nodes_);
        return c;
    }

    /**
     * The type: NODESET, BOOLEAN, NUMBER or STRING.
     * @return The type.
     */
    public int type()
    {
        return type_;
    }

    /**
     * The number of nodes of a node set.
     * @return The size.
     */
    public int size()
    {
        return nodes_.size();
    }

    /**
     * A node of a node set.
     * @param i The index.
     * @return The node or null.
     */
    public XmlNode node(int i)
    {
        return i < nodes_.size() ? nodes_.get(i) : null;
    }

    /**
     * The nodes of a node set, the list may be changed by the caller.
     * @return The nodes.
     */
    public List<XmlNode> nodes()
    {
        return nodes_;
    }

    /**
     * The boolean of a BOOLEAN value.
     * @return The boolean.
     */
    public boolean booleanValue()
    {
        return b_;
    }

    /**
     * The number of a NUMBER value.
     * @return The number.
     */
    public double numberValue()
    {
        return num_;
    }

    /**
     * The string of a STRING value.
     * @return The string.
     */
    public String stringValue()
    {
        return str_;
    }

    /**
     * Convert to a string like xpath string() (a node set gives the string value of its first node).
     * @return The string.
     */
    public String castToString()
    {
        switch (type_)
        {
        case STRING: return str_ == null ? "" : str_;
        case NUMBER: return XPathUtil.formatNumber(num_);
        case BOOLEAN: return b_ ? "true" : "false";
        default:
            if (nodes_.isEmpty()) return "";
            return XPathUtil.stringValue(nodes_.get(0));
        }
    }

    /**
     * Convert to a number like xpath number().
     * @return The number.
     */
    public double castToNumber()
    {
        switch (type_)
        {
        case NUMBER: return num_;
        case BOOLEAN: return b_ ? 1.0 : 0.0;
        default: return XPathUtil.stringToNumber(castToString());
        }
    }

    /**
     * Convert to a boolean like xpath boolean().
     * @return The boolean.
     */
    public boolean castToBoolean()
    {
        switch (type_)
        {
        case BOOLEAN: return b_;
        case NUMBER: return num_ != 0 && !Double.isNaN(num_);
        case STRING: return str_ != null && !str_.isEmpty();
        default: return !nodes_.isEmpty();
        }
    }

    /** A value of an evaluation (List, Boolean, Double or String) as a result. */
    static XPathResult of(Object v)
    {
        if (v instanceof Boolean) return newBoolean((Boolean)v);
        if (v instanceof Double) return newNumber((Double)v);
        if (v instanceof String) return newString((String)v);
        XPathResult r = newNodeSet();
        r.nodes_ = new ArrayList<>(XPathExpr.nodes(v));
        r.tree_ = v instanceof XPathTree;
        return r;
    }

    /** This result as a value of an evaluation, the nodes are copied. */
    Object value()
    {
        switch (type_)
        {
        case BOOLEAN: return b_;
        case NUMBER: return num_;
        case STRING: return str_ == null ? "" : str_;
        default: return tree_ ? new XPathTree(nodes_) : new ArrayList<>(nodes_);
        }
    }
}
