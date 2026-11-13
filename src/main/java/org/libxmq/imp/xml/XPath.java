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
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * A compiled XPath 1.0 expression that is evaluated against the xmq DOM.
 *
 * The engine behaves like the XPath implementation in libxml2 2.9.14
 * (xpath.c, Copyright (C) 1998-2012 Daniel Veillard, MIT license), since
 * the C version of xmq uses libxml2 and the java version must produce
 * identical results. This covers which expressions are accepted, the
 * traversal of the axes (eg dtd nodes are skipped and entity references
 * are never descended into), document order, the conversion of numbers
 * to strings, the comparison rules and the error behaviour (eg an
 * undefined namespace prefix or an unknown function is an error).
 *
 * The result of an evaluation is one of: a List&lt;XmlNode&gt; (a node-set
 * in document order without duplicates), a String, a Double or a Boolean.
 * Namespace nodes in a node-set are {@link XPathNamespaceNode}s.
 *
 * Example:
 * <pre>
 * XPath xp = XPath.compile("//item[@id='a']/name");
 * List&lt;XmlNode&gt; nodes = xp.selectNodes(doc);
 * </pre>
 */
public final class XPath
{
    private static final int MAX_CACHE = 1024;
    private static final Map<String, XPath> cache_ = new ConcurrentHashMap<>();

    private final String expr_;
    private final XPathExpr compiled_;
    private final XPathStream stream_;

    private XPath(String expr, XPathExpr compiled, XPathStream stream)
    {
        expr_ = expr;
        compiled_ = compiled;
        stream_ = stream;
    }

    /**
     * Compile an expression. Compiled expressions are cached, compiling
     * the same expression again returns the cached object.
     * @param expr The xpath expression.
     * @return The compiled expression.
     * @throws XPathException If the expression has a syntax error.
     */
    public static XPath compile(String expr) throws XPathException
    {
        if (expr == null) throw new XPathException(XPathException.EXPR_ERROR, null, -1);
        XPath xp = cache_.get(expr);
        if (xp != null) return xp;
        // Like libxml2, simple expressions are evaluated by the streaming pattern
        // engine, the others by the full xpath engine.
        XPathStream stream = XPathStream.tryCompile(expr);
        if (stream != null) xp = new XPath(expr, null, stream);
        else xp = new XPath(expr, new XPathParser(expr).parse(), null);
        if (cache_.size() >= MAX_CACHE) cache_.clear();
        cache_.put(expr, xp);
        return xp;
    }

    /**
     * Returns the source text of the expression.
     * @return The expression.
     */
    public String expression()
    {
        return expr_;
    }

    /**
     * Evaluate the expression.
     * @param context The context node, can be the XmlDocument.
     * @param namespaces Maps prefixes used in the expression to namespace uris,
     *        may be null or empty. Using an unmapped prefix (other than xml) is an error.
     * @return A List&lt;XmlNode&gt; (node-set in document order), String, Double or Boolean.
     * @throws XPathException If the evaluation fails.
     */
    public Object evaluate(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        return evaluate(context, namespaces, null);
    }

    /**
     * Evaluate the expression with variable bindings.
     * @param context The context node, can be the XmlDocument.
     * @param namespaces Maps prefixes used in the expression to namespace uris, may be null.
     * @param variables Maps variable names to values, may be null. A value can be a
     *        String, Number, Boolean, XmlNode or a Collection of XmlNodes. A variable
     *        with a prefix, eg $p:v, is looked up using the key {uri}v.
     * @return A List&lt;XmlNode&gt; (node-set in document order), String, Double or Boolean.
     * @throws XPathException If the evaluation fails.
     */
    public Object evaluate(XmlNode context, Map<String, String> namespaces, Map<String, Object> variables)
        throws XPathException
    {
        XPathContext ctx = new XPathContext(expr_, context, namespaces, variables);
        if (stream_ != null) return stream_.eval(context, ctx.doc);
        Object o;
        try
        {
            o = compiled_.eval(ctx);
        }
        catch (StackOverflowError so)
        {
            throw new XPathException(XPathException.RECURSION_LIMIT_EXCEEDED, expr_, -1);
        }
        if (o instanceof List)
        {
            // The top level of a compiled expression is always sorted, return a fresh list.
            return new ArrayList<>(XPathExpr.nodes(o));
        }
        return o;
    }

    /**
     * Evaluate and return the resulting node-set.
     * @param context The context node.
     * @return The nodes in document order.
     * @throws XPathException If the evaluation fails or the result is not a node-set.
     */
    public List<XmlNode> selectNodes(XmlNode context) throws XPathException
    {
        return selectNodes(context, null);
    }

    /**
     * Evaluate and return the resulting node-set.
     * @param context The context node.
     * @param namespaces Maps prefixes to namespace uris, may be null.
     * @return The nodes in document order.
     * @throws XPathException If the evaluation fails or the result is not a node-set.
     */
    public List<XmlNode> selectNodes(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        Object o = evaluate(context, namespaces, null);
        if (!(o instanceof List)) throw new XPathException(XPathException.INVALID_TYPE, expr_, -1);
        return XPathExpr.nodes(o);
    }

    /**
     * Evaluate and convert the result using the xpath string() function.
     * @param context The context node.
     * @return The string value.
     * @throws XPathException If the evaluation fails.
     */
    public String evaluateString(XmlNode context) throws XPathException
    {
        return evaluateString(context, null);
    }

    /**
     * Evaluate and convert the result using the xpath string() function.
     * @param context The context node.
     * @param namespaces Maps prefixes to namespace uris, may be null.
     * @return The string value.
     * @throws XPathException If the evaluation fails.
     */
    public String evaluateString(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        XPathContext ctx = new XPathContext(expr_, context, namespaces, null);
        return XPathExpr.toStringValue(evaluate(context, namespaces, null), ctx);
    }

    /**
     * Evaluate and convert the result using the xpath number() function.
     * @param context The context node.
     * @return The number value.
     * @throws XPathException If the evaluation fails.
     */
    public double evaluateNumber(XmlNode context) throws XPathException
    {
        return evaluateNumber(context, null);
    }

    /**
     * Evaluate and convert the result using the xpath number() function.
     * @param context The context node.
     * @param namespaces Maps prefixes to namespace uris, may be null.
     * @return The number value.
     * @throws XPathException If the evaluation fails.
     */
    public double evaluateNumber(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        XPathContext ctx = new XPathContext(expr_, context, namespaces, null);
        return XPathExpr.toNumber(evaluate(context, namespaces, null), ctx);
    }

    /**
     * Evaluate and convert the result using the xpath boolean() function.
     * @param context The context node.
     * @return The boolean value.
     * @throws XPathException If the evaluation fails.
     */
    public boolean evaluateBoolean(XmlNode context) throws XPathException
    {
        return evaluateBoolean(context, null);
    }

    /**
     * Evaluate and convert the result using the xpath boolean() function.
     * @param context The context node.
     * @param namespaces Maps prefixes to namespace uris, may be null.
     * @return The boolean value.
     * @throws XPathException If the evaluation fails.
     */
    public boolean evaluateBoolean(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        return XPathExpr.toBoolean(evaluate(context, namespaces, null));
    }

    /**
     * Format a number like xpath string(), eg 1 gives "1" and 0.5 gives "0.5".
     * Uses the same algorithm as libxml2 xmlXPathFormatNumber.
     * @param d The number.
     * @return The string.
     */
    public static String numberToString(double d)
    {
        return XPathUtil.formatNumber(d);
    }

    /**
     * Convert a string to a number like xpath number(), using the same
     * algorithm as libxml2 xmlXPathStringEvalNumber.
     * @param s The string.
     * @return The number, NaN if the string is not a number.
     */
    public static double stringToNumber(String s)
    {
        return XPathUtil.stringToNumber(s);
    }

    @Override
    public String toString()
    {
        return expr_;
    }
}
