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
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * The compiled form of an xpath expression, an expression tree that is
 * evaluated recursively. The values are: List&lt;XmlNode&gt; (node-set),
 * String, Double and Boolean.
 *
 * The evaluation semantics (axes, node tests, comparisons, conversions)
 * are ported from libxml2 xpath.c (Copyright (C) 1998-2012 Daniel
 * Veillard, MIT license).
 */
abstract class XPathExpr
{
    // Axes, same as libxml2 xmlXPathAxisVal.
    static final int AXIS_ANCESTOR = 1;
    static final int AXIS_ANCESTOR_OR_SELF = 2;
    static final int AXIS_ATTRIBUTE = 3;
    static final int AXIS_CHILD = 4;
    static final int AXIS_DESCENDANT = 5;
    static final int AXIS_DESCENDANT_OR_SELF = 6;
    static final int AXIS_FOLLOWING = 7;
    static final int AXIS_FOLLOWING_SIBLING = 8;
    static final int AXIS_NAMESPACE = 9;
    static final int AXIS_PARENT = 10;
    static final int AXIS_PRECEDING = 11;
    static final int AXIS_PRECEDING_SIBLING = 12;
    static final int AXIS_SELF = 13;

    // Node tests, same as libxml2 xmlXPathTestVal.
    static final int NODE_TEST_TYPE = 1;
    static final int NODE_TEST_PI = 2;
    static final int NODE_TEST_ALL = 3;
    static final int NODE_TEST_NAME = 5;

    // Node types for NODE_TEST_TYPE.
    static final int NODE_TYPE_NODE = 0;
    static final int NODE_TYPE_COMMENT = 8;
    static final int NODE_TYPE_TEXT = 3;
    static final int NODE_TYPE_PI = 7;

    abstract Object eval(XPathContext ctx) throws XPathException;

    /** True if this is a constant (libxml2 XPATH_OP_VALUE). */
    boolean isValue()
    {
        return false;
    }

    // ----------------------------------------------------------------------
    // Conversions

    @SuppressWarnings("unchecked")
    static List<XmlNode> nodes(Object o)
    {
        return (List<XmlNode>)o;
    }

    static boolean isNodeSet(Object o)
    {
        return o instanceof List;
    }

    /** Like libxml2 xmlXPathCastToBoolean. */
    static boolean toBoolean(Object o)
    {
        if (o instanceof Boolean) return (Boolean)o;
        if (o instanceof Double) return XPathUtil.numberToBoolean((Double)o);
        if (o instanceof String) return !((String)o).isEmpty();
        if (o instanceof List) return !nodes(o).isEmpty();
        return false;
    }

    /** Like libxml2 xmlXPathCastToNumber. */
    static double toNumber(Object o, XPathContext ctx)
    {
        if (o instanceof Double) return (Double)o;
        if (o instanceof Boolean) return ((Boolean)o) ? 1.0 : 0.0;
        if (o instanceof String) return XPathUtil.stringToNumber((String)o);
        if (o instanceof List) return XPathUtil.stringToNumber(toStringValue(o, ctx));
        return Double.NaN;
    }

    /** Like libxml2 xmlXPathCastToString. */
    static String toStringValue(Object o, XPathContext ctx)
    {
        if (o instanceof String) return (String)o;
        if (o instanceof Double) return XPathUtil.formatNumber((Double)o);
        if (o instanceof Boolean) return ((Boolean)o) ? "true" : "false";
        if (o instanceof List)
        {
            List<XmlNode> l = nodes(o);
            if (l.isEmpty()) return "";
            XmlNode n = l.size() == 1 ? l.get(0) : ctx.first(l);
            return XPathUtil.stringValue(n);
        }
        return "";
    }

    // ----------------------------------------------------------------------
    // Constants

    static final class Literal extends XPathExpr
    {
        final String value;

        Literal(String v)
        {
            value = v;
        }

        @Override
        Object eval(XPathContext ctx)
        {
            return value;
        }

        @Override
        boolean isValue()
        {
            return true;
        }
    }

    static final class NumberLit extends XPathExpr
    {
        final Double value;

        NumberLit(double v)
        {
            value = v;
        }

        @Override
        Object eval(XPathContext ctx)
        {
            return value;
        }

        @Override
        boolean isValue()
        {
            return true;
        }
    }

    // ----------------------------------------------------------------------
    // Variables

    static final class Var extends XPathExpr
    {
        final String prefix;
        final String name;

        Var(String prefix, String name)
        {
            this.prefix = prefix;
            this.name = name;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            String key = name;
            if (prefix != null)
            {
                String uri = ctx.lookupNs(prefix);
                if (uri == null) throw ctx.error(XPathException.UNDEF_PREFIX_ERROR);
                key = "{" + uri + "}" + name;
            }
            if (ctx.variables == null || !ctx.variables.containsKey(key))
            {
                throw ctx.error(XPathException.UNDEF_VARIABLE_ERROR);
            }
            Object v = ctx.variables.get(key);
            if (v instanceof String || v instanceof Boolean || v instanceof Double) return v;
            if (v instanceof Number) return ((Number)v).doubleValue();
            if (v instanceof XmlNode)
            {
                List<XmlNode> l = new ArrayList<>();
                l.add((XmlNode)v);
                return l;
            }
            if (v instanceof Collection)
            {
                List<XmlNode> l = new ArrayList<>();
                for (Object x : (Collection<?>)v)
                {
                    if (!(x instanceof XmlNode)) throw ctx.error(XPathException.INVALID_TYPE);
                    l.add((XmlNode)x);
                }
                return l;
            }
            if (v == null) throw ctx.error(XPathException.UNDEF_VARIABLE_ERROR);
            return v.toString();
        }
    }

    // ----------------------------------------------------------------------
    // Operators

    static final class Or extends XPathExpr
    {
        final XPathExpr l, r;

        Or(XPathExpr l, XPathExpr r)
        {
            this.l = l;
            this.r = r;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            if (toBoolean(l.eval(ctx))) return Boolean.TRUE;
            return toBoolean(r.eval(ctx));
        }
    }

    static final class And extends XPathExpr
    {
        final XPathExpr l, r;

        And(XPathExpr l, XPathExpr r)
        {
            this.l = l;
            this.r = r;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            if (!toBoolean(l.eval(ctx))) return Boolean.FALSE;
            return toBoolean(r.eval(ctx));
        }
    }

    static final class Equal extends XPathExpr
    {
        final XPathExpr l, r;
        final boolean eq;

        Equal(XPathExpr l, XPathExpr r, boolean eq)
        {
            this.l = l;
            this.r = r;
            this.eq = eq;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object a = l.eval(ctx);
            Object b = r.eval(ctx);
            return equalValues(a, b, !eq, ctx);
        }
    }

    static final class Compare extends XPathExpr
    {
        final XPathExpr l, r;
        final boolean inf, strict;

        Compare(XPathExpr l, XPathExpr r, boolean inf, boolean strict)
        {
            this.l = l;
            this.r = r;
            this.inf = inf;
            this.strict = strict;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object a = l.eval(ctx);
            Object b = r.eval(ctx);
            return compareValues(a, b, inf, strict, ctx);
        }
    }

    static final int OP_SUB = 0;
    static final int OP_ADD = 1;
    static final int OP_MUL = 2;
    static final int OP_DIV = 3;
    static final int OP_MOD = 4;

    static final class Arith extends XPathExpr
    {
        final XPathExpr l, r;
        final int op;

        Arith(XPathExpr l, XPathExpr r, int op)
        {
            this.l = l;
            this.r = r;
            this.op = op;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object a = l.eval(ctx);
            Object b = r.eval(ctx);
            double x = toNumber(a, ctx);
            double y = toNumber(b, ctx);
            switch (op)
            {
            case OP_SUB: return x - y;
            case OP_ADD: return x + y;
            case OP_MUL: return x * y;
            case OP_DIV: return x / y;
            default: return x % y;
            }
        }
    }

    /** Unary minus, minus false means an even number of minus signs (a cast to number). */
    static final class Negate extends XPathExpr
    {
        final XPathExpr e;
        final boolean minus;

        Negate(XPathExpr e, boolean minus)
        {
            this.e = e;
            this.minus = minus;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            double d = toNumber(e.eval(ctx), ctx);
            return minus ? -d : d;
        }
    }

    static final class Union extends XPathExpr
    {
        final XPathExpr l, r;

        Union(XPathExpr l, XPathExpr r)
        {
            this.l = l;
            this.r = r;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object a = l.eval(ctx);
            Object b = r.eval(ctx);
            if (!isNodeSet(a) || !isNodeSet(b)) throw ctx.error(XPathException.INVALID_TYPE);
            List<XmlNode> la = nodes(a);
            List<XmlNode> lb = nodes(b);
            if (lb.isEmpty()) return la;
            List<XmlNode> out = new ArrayList<>(la.size() + lb.size());
            Set<XmlNode> seen = new HashSet<>(la);
            out.addAll(la);
            for (XmlNode n : lb)
            {
                if (seen.add(n)) out.add(n);
            }
            return out;
        }
    }

    /** Sort a node-set in document order, libxml2 XPATH_OP_SORT. */
    static final class Sort extends XPathExpr
    {
        final XPathExpr e;

        Sort(XPathExpr e)
        {
            this.e = e;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object o = e.eval(ctx);
            if (o instanceof List && nodes(o).size() > 1)
            {
                List<XmlNode> l = new ArrayList<>(nodes(o));
                ctx.sort(l);
                return l;
            }
            return o;
        }
    }

    /** A primary expression followed by predicates, libxml2 XPATH_OP_FILTER. */
    static final class Filter extends XPathExpr
    {
        final XPathExpr e;
        final XPathExpr pred;

        Filter(XPathExpr e, XPathExpr pred)
        {
            this.e = e;
            this.pred = pred;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object o = e.eval(ctx);
            if (!isNodeSet(o)) throw ctx.error(XPathException.INVALID_TYPE);
            List<XmlNode> l = nodes(o);
            if (l.isEmpty()) return l;
            return applyPredicate(l, pred, ctx);
        }
    }

    /** Filter a node set with a predicate, like libxml2 xmlXPathNodeSetFilter. */
    static List<XmlNode> applyPredicate(List<XmlNode> set, XPathExpr pred, XPathContext ctx) throws XPathException
    {
        XmlNode old_node = ctx.node;
        XmlNode old_doc = ctx.doc;
        int old_size = ctx.size;
        int old_pos = ctx.position;
        List<XmlNode> out = new ArrayList<>();
        try
        {
            int n = set.size();
            ctx.size = n;
            for (int i = 0; i < n; i++)
            {
                XmlNode node = set.get(i);
                ctx.node = node;
                ctx.position = i + 1;
                if (!(node instanceof XPathNamespaceNode) && node.doc() != null) ctx.doc = node.doc();
                Object r = evalPredicate(pred, ctx);
                boolean keep;
                if (r instanceof Double) keep = ((Double)r) == (double)(i + 1);
                else keep = toBoolean(r);
                if (keep) out.add(node);
            }
        }
        finally
        {
            ctx.node = old_node;
            ctx.doc = old_doc;
            ctx.size = old_size;
            ctx.position = old_pos;
        }
        return out;
    }

    /**
     * Evaluate a predicate expression, like libxml2 xmlXPathCompOpEvalToBoolean:
     * if the predicate is a location path, its last step only needs to find
     * the first node.
     */
    static Object evalPredicate(XPathExpr pred, XPathContext ctx) throws XPathException
    {
        XPathExpr e = pred;
        while (e instanceof Sort) e = ((Sort)e).e;
        if (e instanceof Path && !((Path)e).steps.isEmpty()) return ((Path)e).evalPath(ctx, true);
        return pred.eval(ctx);
    }

    // ----------------------------------------------------------------------
    // Location paths

    static final int START_ROOT = 0;
    static final int START_CONTEXT = 1;
    static final int START_FILTER = 2;

    static final class Path extends XPathExpr
    {
        final int start;
        final XPathExpr filter;
        final List<Step> steps;

        Path(int start, XPathExpr filter, List<Step> steps)
        {
            this.start = start;
            this.filter = filter;
            this.steps = steps;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            return evalPath(ctx, false);
        }

        /** Evaluate the path, if to_bool then the result is only used as a boolean. */
        Object evalPath(XPathContext ctx, boolean to_bool) throws XPathException
        {
            Object cur;
            if (start == START_ROOT)
            {
                List<XmlNode> l = new ArrayList<>(1);
                if (ctx.doc != null) l.add(ctx.doc);
                cur = l;
            }
            else if (start == START_CONTEXT)
            {
                List<XmlNode> l = new ArrayList<>(1);
                if (ctx.node != null) l.add(ctx.node);
                cur = l;
            }
            else
            {
                cur = filter.eval(ctx);
            }
            int n = steps.size();
            for (int i = 0; i < n; i++)
            {
                if (!isNodeSet(cur)) throw ctx.error(XPathException.INVALID_TYPE);
                cur = steps.get(i).collect(nodes(cur), ctx, to_bool && i == n - 1);
            }
            return cur;
        }
    }

    static final class Step
    {
        int axis;
        final int test;
        final int type;
        final String prefix;
        final String name;
        final List<XPathExpr> predicates;

        Step(int axis, int test, int type, String prefix, String name, List<XPathExpr> predicates)
        {
            this.axis = axis;
            this.test = test;
            this.type = type;
            this.prefix = prefix;
            this.name = name;
            this.predicates = predicates;
        }

        /** True for descendant-or-self::node() without predicates. */
        boolean isDescendantOrSelfNode()
        {
            return axis == AXIS_DESCENDANT_OR_SELF && test == NODE_TEST_TYPE && type == NODE_TYPE_NODE
                && predicates.isEmpty();
        }

        /**
         * If the last predicate is a number literal [n], like libxml2
         * xmlXPathIsPositionalPredicate, then return n, otherwise null.
         */
        Integer positionalPredicate()
        {
            if (predicates.isEmpty()) return null;
            XPathExpr p = predicates.get(predicates.size() - 1);
            if (!(p instanceof NumberLit)) return null;
            double f = ((NumberLit)p).value;
            if (f > Integer.MIN_VALUE && f < Integer.MAX_VALUE && f == (double)(int)f) return (int)f;
            return null;
        }

        /** Like libxml2 xmlXPathNodeCollectAndTest. */
        List<XmlNode> collect(List<XmlNode> context, XPathContext ctx, boolean to_bool) throws XPathException
        {
            String uri = null;
            if (prefix != null)
            {
                uri = ctx.lookupNs(prefix);
                if (uri == null) throw ctx.error(XPathException.UNDEF_PREFIX_ERROR);
            }
            List<XmlNode> out = new ArrayList<>();
            if (context.isEmpty()) return out;
            Integer max_pos = positionalPredicate();
            // A single [n] predicate stops the axis traversal at the n:th node.
            boolean axis_range = max_pos != null && predicates.size() == 1;
            boolean break_on_first_hit = to_bool && (predicates.isEmpty() || axis_range);
            Set<XmlNode> seen = context.size() > 1 ? new HashSet<>() : null;
            XmlNode old_node = ctx.node;
            try
            {
                for (XmlNode c : context)
                {
                    ctx.node = c;
                    XPathAxis.Collector col = new XPathAxis.Collector(this, uri, axis_range,
                                                                      axis_range ? max_pos : 0,
                                                                      break_on_first_hit);
                    XPathAxis.traverse(c, ctx, col);
                    List<XmlNode> seq = col.seq;
                    if (!axis_range)
                    {
                        for (XPathExpr p : predicates)
                        {
                            if (seq.isEmpty()) break;
                            seq = applyPredicate(seq, p, ctx);
                        }
                    }
                    if (seen == null)
                    {
                        out.addAll(seq);
                    }
                    else
                    {
                        for (XmlNode n : seq)
                        {
                            if (seen.add(n)) out.add(n);
                        }
                    }
                    if (to_bool && !seq.isEmpty()) break;
                }
            }
            finally
            {
                ctx.node = old_node;
            }
            return out;
        }

        /** The node test, like the test switch in libxml2 xmlXPathNodeCollectAndTest. */
        boolean matches(XmlNode cur, String uri)
        {
            XmlNodeType t = cur.type();
            switch (test)
            {
            case NODE_TEST_TYPE:
                if (type == NODE_TYPE_NODE)
                {
                    switch (t)
                    {
                    case DOCUMENT: case HTML_DOCUMENT: case ELEMENT: case ATTRIBUTE: case PI:
                    case COMMENT: case CDATA: case TEXT: case NAMESPACE:
                        return true;
                    default:
                        return false;
                    }
                }
                if (type == NODE_TYPE_COMMENT) return t == XmlNodeType.COMMENT;
                if (type == NODE_TYPE_PI) return t == XmlNodeType.PI;
                if (type == NODE_TYPE_TEXT) return t == XmlNodeType.TEXT || t == XmlNodeType.CDATA;
                return false;
            case NODE_TEST_PI:
                return t == XmlNodeType.PI && (name == null || name.equals(cur.name()));
            case NODE_TEST_ALL:
                if (axis == AXIS_ATTRIBUTE)
                {
                    if (t != XmlNodeType.ATTRIBUTE) return false;
                    if (prefix == null) return true;
                    XmlNs ns = ((XmlAttr)cur).ns();
                    return ns != null && uri.equals(ns.href());
                }
                if (axis == AXIS_NAMESPACE)
                {
                    return t == XmlNodeType.NAMESPACE;
                }
                if (t != XmlNodeType.ELEMENT) return false;
                if (prefix == null) return true;
                {
                    XmlNs ns = ((XmlElement)cur).ns();
                    return ns != null && uri.equals(ns.href());
                }
            case NODE_TEST_NAME:
                if (axis == AXIS_ATTRIBUTE)
                {
                    if (t != XmlNodeType.ATTRIBUTE) return false;
                    XmlAttr a = (XmlAttr)cur;
                    if (!name.equals(a.name())) return false;
                    XmlNs ns = a.ns();
                    if (prefix == null) return ns == null || ns.prefix() == null;
                    return ns != null && uri.equals(ns.href());
                }
                if (axis == AXIS_NAMESPACE)
                {
                    if (t != XmlNodeType.NAMESPACE) return false;
                    String p = ((XPathNamespaceNode)cur).prefix();
                    return p != null && p.equals(name);
                }
                if (t != XmlNodeType.ELEMENT) return false;
                {
                    XmlElement e = (XmlElement)cur;
                    if (!name.equals(e.name())) return false;
                    XmlNs ns = e.ns();
                    if (prefix == null) return ns == null;
                    return ns != null && uri.equals(ns.href());
                }
            default:
                return false;
            }
        }
    }

    // ----------------------------------------------------------------------
    // Functions

    static final class Call extends XPathExpr
    {
        final String prefix;
        final String name;
        final List<XPathExpr> args;

        Call(String prefix, String name, List<XPathExpr> args)
        {
            this.prefix = prefix;
            this.name = name;
            this.args = args;
        }

        @Override
        Object eval(XPathContext ctx) throws XPathException
        {
            Object[] values = new Object[args.size()];
            for (int i = 0; i < values.length; i++) values[i] = args.get(i).eval(ctx);
            String uri = null;
            if (prefix != null)
            {
                uri = ctx.lookupNs(prefix);
                if (uri == null) throw ctx.error(XPathException.UNDEF_PREFIX_ERROR);
            }
            return XPathFunctions.call(uri, name, values, ctx);
        }
    }

    // ----------------------------------------------------------------------
    // Comparisons

    /** Like libxml2 xmlXPathEqualValues and xmlXPathNotEqualValues. */
    static boolean equalValues(Object a, Object b, boolean neq, XPathContext ctx)
    {
        if (isNodeSet(a) || isNodeSet(b))
        {
            if (!isNodeSet(a))
            {
                Object t = a;
                a = b;
                b = t;
            }
            List<XmlNode> ns = nodes(a);
            if (isNodeSet(b)) return equalNodeSets(ns, nodes(b), neq);
            if (b instanceof Boolean)
            {
                boolean r = !ns.isEmpty();
                return neq ? (r != (Boolean)b) : (r == (Boolean)b);
            }
            if (b instanceof Double) return equalNodeSetFloat(ns, (Double)b, neq);
            return equalNodeSetString(ns, (String)b, neq);
        }
        boolean r = equalValuesCommon(a, b, ctx);
        return neq ? !r : r;
    }

    private static boolean equalValuesCommon(Object a, Object b, XPathContext ctx)
    {
        if (a instanceof Boolean)
        {
            boolean x = (Boolean)a;
            if (b instanceof Boolean) return x == (Boolean)b;
            if (b instanceof Double) return x == XPathUtil.numberToBoolean((Double)b);
            return x == !((String)b).isEmpty();
        }
        if (a instanceof Double)
        {
            if (b instanceof Boolean) return (Boolean)b == XPathUtil.numberToBoolean((Double)a);
            double y = b instanceof String ? XPathUtil.stringToNumber((String)b) : (Double)b;
            return (Double)a == y;
        }
        String x = (String)a;
        if (b instanceof Boolean) return (Boolean)b == !x.isEmpty();
        if (b instanceof String) return x.equals(b);
        return XPathUtil.stringToNumber(x) == (Double)b;
    }

    private static boolean equalNodeSetString(List<XmlNode> ns, String s, boolean neq)
    {
        for (XmlNode n : ns)
        {
            boolean eq = XPathUtil.stringValue(n).equals(s);
            if (eq && !neq) return true;
            if (!eq && neq) return true;
        }
        return false;
    }

    private static boolean equalNodeSetFloat(List<XmlNode> ns, double f, boolean neq)
    {
        for (XmlNode n : ns)
        {
            double v = XPathUtil.stringToNumber(XPathUtil.stringValue(n));
            if (!Double.isNaN(v))
            {
                if (!neq && v == f) return true;
                if (neq && v != f) return true;
            }
            else if (neq)
            {
                return true;
            }
        }
        return false;
    }

    private static boolean equalNodeSets(List<XmlNode> ns1, List<XmlNode> ns2, boolean neq)
    {
        if (ns1.isEmpty() || ns2.isEmpty()) return false;
        if (!neq)
        {
            Set<XmlNode> s1 = new HashSet<>(ns1);
            for (XmlNode n : ns2) if (s1.contains(n)) return true;
        }
        String[] v2 = new String[ns2.size()];
        for (int j = 0; j < v2.length; j++) v2[j] = XPathUtil.stringValue(ns2.get(j));
        for (XmlNode n : ns1)
        {
            String v1 = XPathUtil.stringValue(n);
            for (String s : v2)
            {
                if (v1.equals(s) != neq) return true;
            }
        }
        return false;
    }

    /** Like libxml2 xmlXPathCompareValues. */
    static boolean compareValues(Object a, Object b, boolean inf, boolean strict, XPathContext ctx)
    {
        if (isNodeSet(a) || isNodeSet(b))
        {
            if (isNodeSet(a) && isNodeSet(b)) return compareNodeSets(nodes(a), nodes(b), inf, strict);
            if (isNodeSet(a)) return compareNodeSetValue(nodes(a), b, inf, strict, ctx);
            return compareNodeSetValue(nodes(b), a, !inf, strict, ctx);
        }
        return compareNumbers(toNumber(a, ctx), toNumber(b, ctx), inf, strict);
    }

    static boolean compareNumbers(double x, double y, boolean inf, boolean strict)
    {
        if (inf && strict) return x < y;
        if (inf) return x <= y;
        if (strict) return x > y;
        return x >= y;
    }

    private static boolean compareNodeSetValue(List<XmlNode> ns, Object val, boolean inf, boolean strict,
                                               XPathContext ctx)
    {
        if (val instanceof Boolean)
        {
            return compareNumbers(ns.isEmpty() ? 0 : 1, ((Boolean)val) ? 1 : 0, inf, strict);
        }
        double f = toNumber(val, ctx);
        for (XmlNode n : ns)
        {
            double v = XPathUtil.stringToNumber(XPathUtil.stringValue(n));
            if (compareNumbers(v, f, inf, strict)) return true;
        }
        return false;
    }

    private static boolean compareNodeSets(List<XmlNode> ns1, List<XmlNode> ns2, boolean inf, boolean strict)
    {
        if (ns1.isEmpty() || ns2.isEmpty()) return false;
        double[] v2 = new double[ns2.size()];
        for (int j = 0; j < v2.length; j++) v2[j] = XPathUtil.stringToNumber(XPathUtil.stringValue(ns2.get(j)));
        for (XmlNode n : ns1)
        {
            double v1 = XPathUtil.stringToNumber(XPathUtil.stringValue(n));
            if (Double.isNaN(v1)) continue;
            for (double v : v2)
            {
                if (compareNumbers(v1, v, inf, strict)) return true;
            }
        }
        return false;
    }
}
