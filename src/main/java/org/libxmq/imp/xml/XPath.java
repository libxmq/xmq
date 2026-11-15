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
    private final XPathSettings settings_;

    private XPath(String expr, XPathExpr compiled, XPathSettings settings)
    {
        expr_ = expr;
        compiled_ = compiled;
        settings_ = settings;
    }

    /**
     * Compile an expression with the default settings, the namespaces are given when evaluating.
     * @param expr The expression.
     * @return The compiled expression.
     * @throws XPathException If the expression is invalid.
     */
    public static XPath compile(String expr) throws XPathException
    {
        if (expr == null) throw new XPathException(XPathException.EXPR_ERROR, null, -1);
        XPath xp = cache_.get(expr);
        if (xp != null) return xp;
        xp = compile(expr, new XPathSettings());
        if (cache_.size() >= MAX_CACHE) cache_.clear();
        cache_.put(expr, xp);
        return xp;
    }

    /**
     * Compile an expression, like xml_xpath_compile in the C version. The settings (and their
     * namespaces) are copied, prefixes are resolved when the expression is evaluated.
     * @param expr The expression.
     * @param settings The settings.
     * @return The compiled expression.
     * @throws XPathException If the expression is invalid.
     */
    public static XPath compile(String expr, XPathSettings settings) throws XPathException
    {
        if (expr == null) throw new XPathException(XPathException.EXPR_ERROR, null, -1);
        // libxml2 2.16 does not use the streaming pattern engine for xpath
        // (XPATH_STREAMING is disabled), neither do we.
        XPathSettings s = new XPathSettings(settings);
        return new XPath(expr, new XPathParser(expr, s.maxDepth, s.noVariables).parse(), s);
    }

    /**
     * Returns the source of the expression.
     * @return The expression.
     */
    public String expression()
    {
        return expr_;
    }

    /** Evaluate in a context, like xml_xpath_eval_comp. */
    XPathResult evalIn(XPathEvalContext px, XmlNode node, long position, long size) throws XPathException
    {
        return XPathResult.of(run(px, settings_, node, position, size, true));
    }

    private Object run(XPathEvalContext px, XPathSettings settings, XmlNode node, long position, long size,
                       boolean evaluating) throws XPathException
    {
        XPathContext ctx = new XPathContext(px, settings, expr_, node, position, size, evaluating);
        Object o;
        try
        {
            o = compiled_.eval(ctx);
        }
        catch (StackOverflowError so)
        {
            throw ctx.error(XPathException.RECURSION_LIMIT_EXCEEDED);
        }
        // A result tree fragment stays one (XPATH_XSLT_TREE), eg the value of $rtf.
        if (o instanceof XPathTree) return new XPathTree(XPathExpr.nodes(o));
        if (o instanceof List)
        {
            // The top level of a compiled expression is always sorted, return a fresh list.
            return new ArrayList<>(XPathExpr.nodes(o));
        }
        return o;
    }

    /**
     * Evaluate the expression with a fresh context, like xml_xpath_eval.
     * @param context The context node.
     * @param namespaces Extra prefixes, or null.
     * @return A List&lt;XmlNode&gt;, String, Double or Boolean.
     * @throws XPathException On an error.
     */
    public Object evaluate(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        XPathSettings s = settings_;
        if (namespaces != null && !namespaces.isEmpty())
        {
            s = new XPathSettings(settings_);
            s.namespaces.putAll(namespaces);
        }
        return run(new XPathEvalContext(), s, context, -1, -1, false);
    }

    /**
     * Select the nodes of a node set expression.
     * @param context The context node.
     * @return The nodes in document order.
     * @throws XPathException On an error or if the value is not a node set.
     */
    public List<XmlNode> selectNodes(XmlNode context) throws XPathException
    {
        return selectNodes(context, null);
    }

    /**
     * Select the nodes of a node set expression.
     * @param context The context node.
     * @param namespaces Extra prefixes, or null.
     * @return The nodes in document order.
     * @throws XPathException On an error or if the value is not a node set.
     */
    public List<XmlNode> selectNodes(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        Object o = evaluate(context, namespaces);
        if (!(o instanceof List)) throw new XPathException(XPathException.INVALID_TYPE, expr_, -1);
        return XPathExpr.nodes(o);
    }

    /**
     * Evaluate as a string, like xpath string().
     * @param context The context node.
     * @return The string.
     * @throws XPathException On an error.
     */
    public String evaluateString(XmlNode context) throws XPathException
    {
        return evaluateString(context, null);
    }

    /**
     * Evaluate as a string, like xpath string().
     * @param context The context node.
     * @param namespaces Extra prefixes, or null.
     * @return The string.
     * @throws XPathException On an error.
     */
    public String evaluateString(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        return XPathResult.of(evaluate(context, namespaces)).castToString();
    }

    /**
     * Evaluate as a number, like xpath number().
     * @param context The context node.
     * @return The number.
     * @throws XPathException On an error.
     */
    public double evaluateNumber(XmlNode context) throws XPathException
    {
        return evaluateNumber(context, null);
    }

    /**
     * Evaluate as a number, like xpath number().
     * @param context The context node.
     * @param namespaces Extra prefixes, or null.
     * @return The number.
     * @throws XPathException On an error.
     */
    public double evaluateNumber(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        return XPathResult.of(evaluate(context, namespaces)).castToNumber();
    }

    /**
     * Evaluate as a boolean, like xpath boolean().
     * @param context The context node.
     * @return The boolean.
     * @throws XPathException On an error.
     */
    public boolean evaluateBoolean(XmlNode context) throws XPathException
    {
        return evaluateBoolean(context, null);
    }

    /**
     * Evaluate as a boolean, like xpath boolean().
     * @param context The context node.
     * @param namespaces Extra prefixes, or null.
     * @return The boolean.
     * @throws XPathException On an error.
     */
    public boolean evaluateBoolean(XmlNode context, Map<String, String> namespaces) throws XPathException
    {
        return XPathExpr.toBoolean(evaluate(context, namespaces));
    }

    /**
     * Format a number like xpath string() (libxml2 xmlXPathFormatNumber).
     * @param d The number.
     * @return The string.
     */
    public static String numberToString(double d)
    {
        return XPathUtil.formatNumber(d);
    }

    /**
     * Parse a number like xpath number() (libxml2 xmlXPathStringEvalNumber).
     * @param s The string.
     * @return The number, NaN if not a number.
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
