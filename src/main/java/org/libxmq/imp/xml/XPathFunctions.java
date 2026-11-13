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

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * The xpath 1.0 core function library (and the libxml2 escape-uri
 * extension). Each function is a port of the corresponding libxml2
 * xmlXPath*Function in xpath.c (Copyright (C) 1998-2012 Daniel Veillard,
 * MIT license), including arity and type checks.
 */
final class XPathFunctions
{
    static final String XQUERY_FUNCTIONS_NS = "http://www.w3.org/2002/08/xquery-functions";

    private XPathFunctions()
    {
    }

    private static void arity(Object[] a, int n, XPathContext ctx) throws XPathException
    {
        if (a.length != n) throw ctx.error(XPathException.INVALID_ARITY);
    }

    private static List<XmlNode> nodeSetArg(Object o, XPathContext ctx) throws XPathException
    {
        if (!(o instanceof List)) throw ctx.error(XPathException.INVALID_TYPE);
        return XPathExpr.nodes(o);
    }

    private static String str(Object o, XPathContext ctx)
    {
        return XPathExpr.toStringValue(o, ctx);
    }

    private static double num(Object o, XPathContext ctx)
    {
        return XPathExpr.toNumber(o, ctx);
    }

    private static List<XmlNode> contextSet(XPathContext ctx)
    {
        List<XmlNode> l = new ArrayList<>(1);
        if (ctx.node != null) l.add(ctx.node);
        return l;
    }

    /** Call a function, uri is null for the core functions. */
    static Object call(String uri, String name, Object[] a, XPathContext ctx) throws XPathException
    {
        if (uri != null)
        {
            if (XQUERY_FUNCTIONS_NS.equals(uri) && "escape-uri".equals(name)) return escapeUri(a, ctx);
            throw ctx.error(XPathException.UNKNOWN_FUNC_ERROR);
        }
        switch (name)
        {
        case "last":
            arity(a, 0, ctx);
            if (ctx.size < 0) throw ctx.error(XPathException.INVALID_CTXT_SIZE);
            return (double)ctx.size;
        case "position":
            arity(a, 0, ctx);
            if (ctx.position < 0) throw ctx.error(XPathException.INVALID_CTXT_POSITION);
            return (double)ctx.position;
        case "count":
            arity(a, 1, ctx);
            return (double)nodeSetArg(a[0], ctx).size();
        case "id":
            arity(a, 1, ctx);
            return id(a[0], ctx);
        case "local-name":
        case "namespace-uri":
        case "name":
        {
            if (a.length == 0) a = new Object[] { contextSet(ctx) };
            arity(a, 1, ctx);
            List<XmlNode> l = nodeSetArg(a[0], ctx);
            if (l.isEmpty()) return "";
            XmlNode n = l.get(0);
            if (name.equals("local-name")) return localName(n);
            if (name.equals("namespace-uri")) return namespaceUri(n);
            return qname(n);
        }
        case "string":
            if (a.length == 0) return ctx.node == null ? "" : XPathUtil.stringValue(ctx.node);
            arity(a, 1, ctx);
            return str(a[0], ctx);
        case "string-length":
        {
            String s;
            if (a.length == 0)
            {
                if (ctx.node == null) return 0.0;
                s = XPathUtil.stringValue(ctx.node);
            }
            else
            {
                arity(a, 1, ctx);
                s = str(a[0], ctx);
            }
            return (double)s.codePointCount(0, s.length());
        }
        case "concat":
        {
            if (a.length < 2) throw ctx.error(XPathException.INVALID_ARITY);
            StringBuilder sb = new StringBuilder();
            for (Object o : a) sb.append(str(o, ctx));
            return sb.toString();
        }
        case "contains":
            arity(a, 2, ctx);
            return str(a[0], ctx).contains(str(a[1], ctx));
        case "starts-with":
            arity(a, 2, ctx);
            return str(a[0], ctx).startsWith(str(a[1], ctx));
        case "substring":
            if (a.length < 2 || a.length > 3) throw ctx.error(XPathException.INVALID_ARITY);
            return substring(a, ctx);
        case "substring-before":
        {
            arity(a, 2, ctx);
            String s = str(a[0], ctx);
            int i = s.indexOf(str(a[1], ctx));
            return i < 0 ? "" : s.substring(0, i);
        }
        case "substring-after":
        {
            arity(a, 2, ctx);
            String s = str(a[0], ctx);
            String f = str(a[1], ctx);
            int i = s.indexOf(f);
            return i < 0 ? "" : s.substring(i + f.length());
        }
        case "normalize-space":
        {
            String s;
            if (a.length == 0) s = ctx.node == null ? "" : XPathUtil.stringValue(ctx.node);
            else
            {
                arity(a, 1, ctx);
                s = str(a[0], ctx);
            }
            return normalize(s);
        }
        case "translate":
            arity(a, 3, ctx);
            return translate(str(a[0], ctx), str(a[1], ctx), str(a[2], ctx));
        case "boolean":
            arity(a, 1, ctx);
            return XPathExpr.toBoolean(a[0]);
        case "not":
            arity(a, 1, ctx);
            return !XPathExpr.toBoolean(a[0]);
        case "true":
            arity(a, 0, ctx);
            return Boolean.TRUE;
        case "false":
            arity(a, 0, ctx);
            return Boolean.FALSE;
        case "lang":
            arity(a, 1, ctx);
            return lang(str(a[0], ctx), ctx.node);
        case "number":
            if (a.length == 0)
            {
                if (ctx.node == null) return 0.0;
                return XPathUtil.stringToNumber(XPathUtil.stringValue(ctx.node));
            }
            arity(a, 1, ctx);
            return num(a[0], ctx);
        case "sum":
        {
            arity(a, 1, ctx);
            double res = 0.0;
            for (XmlNode n : nodeSetArg(a[0], ctx))
            {
                res += XPathUtil.stringToNumber(XPathUtil.stringValue(n));
            }
            return res;
        }
        case "floor":
            arity(a, 1, ctx);
            return Math.floor(num(a[0], ctx));
        case "ceiling":
            arity(a, 1, ctx);
            return Math.ceil(num(a[0], ctx));
        case "round":
        {
            arity(a, 1, ctx);
            double f = num(a[0], ctx);
            if (f >= -0.5 && f < 0.5) return f * 0.0;
            double rounded = Math.floor(f);
            if (f - rounded >= 0.5) rounded += 1.0;
            return rounded;
        }
        default:
            throw ctx.error(XPathException.UNKNOWN_FUNC_ERROR);
        }
    }

    private static String localName(XmlNode n)
    {
        switch (n.type())
        {
        case ELEMENT: case ATTRIBUTE: case PI:
            return n.name() == null ? "" : n.name();
        case NAMESPACE:
            return n.name() == null ? "" : n.name();
        default:
            return "";
        }
    }

    private static String namespaceUri(XmlNode n)
    {
        XmlNs ns = null;
        if (n instanceof XmlElement) ns = ((XmlElement)n).ns();
        else if (n instanceof XmlAttr) ns = ((XmlAttr)n).ns();
        else return "";
        if (ns == null || ns.href() == null) return "";
        return ns.href();
    }

    private static String qname(XmlNode n)
    {
        XmlNs ns = null;
        if (n instanceof XmlElement) ns = ((XmlElement)n).ns();
        else if (n instanceof XmlAttr) ns = ((XmlAttr)n).ns();
        else return localName(n);
        String nm = n.name() == null ? "" : n.name();
        if (ns == null || ns.prefix() == null) return nm;
        return ns.prefix() + ":" + nm;
    }

    /** Like libxml2 xmlXPathIdFunction. */
    private static List<XmlNode> id(Object o, XPathContext ctx)
    {
        List<XmlNode> ret = new ArrayList<>();
        if (o instanceof List)
        {
            for (XmlNode n : XPathExpr.nodes(o))
            {
                for (XmlNode e : elementsByIds(XPathUtil.stringValue(n), ctx))
                {
                    if (!ret.contains(e)) ret.add(e);
                }
            }
            return ret;
        }
        return elementsByIds(str(o, ctx), ctx);
    }

    /** Like libxml2 xmlXPathGetElementsByIds (including its handling of leading blanks). */
    private static List<XmlNode> elementsByIds(String ids, XPathContext ctx)
    {
        List<XmlNode> ret = new ArrayList<>();
        int n = ids.length();
        int cur = 0;
        int start = 0;
        while (cur < n && XPathUtil.isBlank(ids.charAt(cur))) cur++;
        while (cur < n)
        {
            while (cur < n && !XPathUtil.isBlank(ids.charAt(cur))) cur++;
            XmlElement e = ctx.lookupId(ids.substring(start, cur));
            if (e != null && !ret.contains(e)) ret.add(e);
            while (cur < n && XPathUtil.isBlank(ids.charAt(cur))) cur++;
            start = cur;
        }
        return ret;
    }

    /** Like libxml2 xmlXPathSubstringFunction. */
    private static String substring(Object[] a, XPathContext ctx)
    {
        double le = 0;
        if (a.length == 3) le = num(a[2], ctx);
        double in = num(a[1], ctx);
        String s = str(a[0], ctx);
        int i = 1;
        int j = Integer.MAX_VALUE;
        if (!(in < Integer.MAX_VALUE))
        {
            i = Integer.MAX_VALUE;
        }
        else if (in >= 1.0)
        {
            i = (int)in;
            if (in - Math.floor(in) >= 0.5) i += 1;
        }
        if (a.length == 3)
        {
            double rin = Math.floor(in);
            if (in - rin >= 0.5) rin += 1.0;
            double rle = Math.floor(le);
            if (le - rle >= 0.5) rle += 1.0;
            double end = rin + rle;
            if (!(end >= 1.0)) j = 1;
            else if (end < Integer.MAX_VALUE) j = (int)end;
        }
        if (i >= j) return "";
        int cps = s.codePointCount(0, s.length());
        int from = i - 1;
        if (from > cps) return "";
        long to = Math.min((long)from + (long)(j - i), (long)cps);
        int b = s.offsetByCodePoints(0, from);
        int e = s.offsetByCodePoints(0, (int)to);
        return s.substring(b, e);
    }

    /** Like libxml2 xmlXPathNormalizeFunction. */
    private static String normalize(String s)
    {
        StringBuilder sb = new StringBuilder();
        int i = 0;
        int n = s.length();
        while (i < n && XPathUtil.isBlank(s.charAt(i))) i++;
        boolean blank = false;
        for (; i < n; i++)
        {
            char c = s.charAt(i);
            if (XPathUtil.isBlank(c))
            {
                blank = true;
            }
            else
            {
                if (blank)
                {
                    sb.append(' ');
                    blank = false;
                }
                sb.append(c);
            }
        }
        return sb.toString();
    }

    /** Like libxml2 xmlXPathTranslateFunction. */
    private static String translate(String s, String from, String to)
    {
        int[] f = from.codePoints().toArray();
        int[] t = to.codePoints().toArray();
        StringBuilder sb = new StringBuilder();
        s.codePoints().forEach(cp ->
        {
            int offset = -1;
            for (int k = 0; k < f.length; k++)
            {
                if (f[k] == cp)
                {
                    offset = k;
                    break;
                }
            }
            if (offset >= 0)
            {
                if (offset < t.length) sb.appendCodePoint(t[offset]);
            }
            else
            {
                sb.appendCodePoint(cp);
            }
        });
        return sb.toString();
    }

    /** Like libxml2 xmlNodeGetLang followed by the comparison in xmlXPathLangFunction. */
    private static boolean lang(String lang, XmlNode node)
    {
        String the_lang = null;
        if (node != null && node.type() != XmlNodeType.NAMESPACE)
        {
            for (XmlNode cur = node; cur != null; cur = cur.parent())
            {
                if (cur instanceof XmlElement)
                {
                    String v = xmlLang((XmlElement)cur);
                    if (v != null)
                    {
                        the_lang = v;
                        break;
                    }
                }
            }
        }
        if (the_lang == null) return false;
        int i;
        for (i = 0; i < lang.length(); i++)
        {
            if (i >= the_lang.length()) return false;
            if (upper(lang.charAt(i)) != upper(the_lang.charAt(i))) return false;
        }
        return i == the_lang.length() || the_lang.charAt(i) == '-';
    }

    private static char upper(char c)
    {
        return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }

    private static String xmlLang(XmlElement e)
    {
        for (XmlAttr a : e.attributes())
        {
            XmlNs ns = a.ns();
            if ("lang".equals(a.name()) && ns != null && XPathUtil.XML_NAMESPACE.equals(ns.href())) return a.value();
            if (ns == null && "xml:lang".equals(a.name())) return a.value();
        }
        return null;
    }

    /** Like libxml2 xmlXPathEscapeUriFunction. */
    private static String escapeUri(Object[] a, XPathContext ctx) throws XPathException
    {
        arity(a, 2, ctx);
        boolean escape_reserved = XPathExpr.toBoolean(a[1]);
        byte[] bytes = str(a[0], ctx).getBytes(StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < bytes.length; i++)
        {
            int c = bytes[i] & 0xff;
            int c1 = i + 1 < bytes.length ? bytes[i + 1] & 0xff : 0;
            int c2 = i + 2 < bytes.length ? bytes[i + 2] & 0xff : 0;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\''
                || c == '(' || c == ')'
                || (c == '%' && isHex(c1) && isHex(c2))
                || (!escape_reserved && (c == ';' || c == '/' || c == '?' || c == ':' || c == '@' || c == '&'
                                         || c == '=' || c == '+' || c == '$' || c == ',')))
            {
                sb.append((char)c);
            }
            else
            {
                sb.append('%').append("0123456789ABCDEF".charAt(c >> 4)).append("0123456789ABCDEF".charAt(c & 0xF));
            }
        }
        return sb.toString();
    }

    private static boolean isHex(int c)
    {
        return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
    }
}
