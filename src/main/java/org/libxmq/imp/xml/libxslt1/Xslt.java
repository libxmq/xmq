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

/* Portions of this file are ported from libxslt (version 1.1.45 as found in the
   libxmq fork in 3rdparty/libxslt). libxslt is covered by the following notice
   (from libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

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
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/

package org.libxmq.imp.xml.libxslt1;

import static org.libxmq.imp.xml.XmlDomApi.*;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.AccessDeniedException;
import java.nio.file.FileSystemException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.libxmq.imp.xml.CFormat;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathException;
import org.libxmq.imp.xml.XPathResult;
import org.libxmq.imp.xml.XPathSettings;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlChars;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDomApi;
import org.libxmq.imp.xml.XmlEntityExpander;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlParseException;
import org.libxmq.imp.xml.XmlParser;
import org.libxmq.imp.xml.XmlUri;

/**
 * XSLT 1.0, a port of libxslt onto the native dom and xpath (the port of
 * xml_libxslt1_xslt.c in the C version).
 *
 * This part has the patterns (pattern.c: compilation, matching, template registration
 * and lookup) and the compilation of stylesheets (xslt.c, preproc.c, imports.c,
 * attributes.c); the transformation is in {@link Transform}.
 */
public final class Xslt
{
    private Xslt()
    {
    }

    // The priority of a template without a priority attribute (XSLT_PAT_NO_PRIORITY).
    static final float PAT_NO_PRIORITY = -12345789.0f;

    static final String XSLT_NAMESPACE = "http://www.w3.org/1999/XSL/Transform";
    static final String LIBXSLT_NAMESPACE = "http://xmlsoft.org/XSLT/namespace";
    static final String SAXON_NAMESPACE = "http://icl.com/saxon";
    static final String XT_NAMESPACE = "http://www.jclark.com/xt";
    static final String XALAN_NAMESPACE = "org.apache.xalan.xslt.extensions.Redirect";
    static final String UNDEFINED_DEFAULT_NS = "#UNDEFINED_DEFAULT_NS";
    static final int MAX_NESTING = 40;

    // Errors /////////////////////////////////////////////////////////

    private static void emit(Settings s, String msg)
    {
        if (s != null && s.message_callback != null)
        {
            s.message_callback.message(msg);
        }
        else
        {
            byte[] b = msg.getBytes(StandardCharsets.UTF_8);
            System.err.write(b, 0, b.length);
            System.err.flush();
        }
    }

    /** Like xsltGenericError, the message without context. */
    static void genericError(Settings s, String message)
    {
        emit(s, message);
    }

    // Port of xsltPrintErrorContext.
    /** Port of xmlGetLineNo (without XML_PARSE_BIG_LINES the lines are 16 bits): attributes and
        other nodes without lines use a neighbour or the parent. -1 if unknown. */
    private static long libxml2LineNo(XmlNode node, int depth)
    {
        if (depth >= 5 || node == null) return -1;
        int k = nodeKind(node);
        if (k == ELEMENT || k == TEXT || k == COMMENT || k == PI)
        {
            long line = nodeLine(node);
            return line > 65535 ? 65535 : line;
        }
        XmlNode prev = k == ATTRIBUTE ? null : nodePrev(node);
        if (prev != null)
        {
            int pk = nodeKind(prev);
            if (pk == ELEMENT || pk == TEXT || pk == COMMENT || pk == PI)
                return libxml2LineNo(prev, depth + 1);
        }
        XmlNode parent = nodeParent(node);
        if (parent != null && nodeKind(parent) == ELEMENT) return libxml2LineNo(parent, depth + 1);
        return -1;
    }

    /** The name libxml2 gives a node (node->name): text and comment nodes have names too. */
    static String libxml2NodeName(XmlNode node)
    {
        String name = nodeName(node);
        int k = nodeKind(node);
        if (k == TEXT && name == null) return "text";
        if (k == COMMENT) return "comment";
        if (k == CDATA) return null;
        return name;
    }

    private static String urlOf(XmlNode node)
    {
        XmlDocument d = nodeDom(node);
        return d == null ? null : domUrl(d);
    }

    private static void printErrorContext(Settings s, TransformCtx t, Stylesheet style, XmlNode node)
    {
        int line = 0;
        String file = null;
        String name = null;
        String type = "error";

        if (t != null && t.state == TransformCtx.STATE_OK) t.state = TransformCtx.STATE_ERROR;
        if (node == null && t != null) node = t.inst;
        if (node != null)
        {
            if (nodeKind(node) == DOCUMENT)
            {
                file = urlOf(node);
            }
            else
            {
                line = (int)libxml2LineNo(node, 0);
                file = urlOf(node);
                name = libxml2NodeName(node);
            }
        }
        if (t != null) type = "runtime error";
        else if (style != null) type = "compilation error";

        if (file != null && line != 0 && name != null) genericError(s, CFormat.format("%s: file %s line %d element %s\n", type, file, line, name));
        else if (file != null && name != null) genericError(s, CFormat.format("%s: file %s element %s\n", type, file, name));
        else if (file != null && line != 0) genericError(s, CFormat.format("%s: file %s line %d\n", type, file, line));
        else if (file != null) genericError(s, CFormat.format("%s: file %s\n", type, file));
        else if (name != null) genericError(s, CFormat.format("%s: element %s\n", type, name));
        else genericError(s, CFormat.format("%s\n", type));
    }

    static Settings settingsOf(TransformCtx t, Stylesheet style)
    {
        if (t != null) return t.style.settings;
        if (style != null) return style.settings;
        return null;
    }

    private static void vtransformError(Settings s, TransformCtx t, Stylesheet style, XmlNode node, String message)
    {
        if (t != null && t.state == TransformCtx.STATE_OK) t.state = TransformCtx.STATE_ERROR;
        if (node == null && t != null) node = t.inst;
        printErrorContext(s, t, style, node);
        emit(s, message);
    }

    /** Port of xsltTransformError: the context, then the message. */
    static void transformError(TransformCtx t, Stylesheet style, XmlNode node, String message)
    {
        vtransformError(settingsOf(t, style), t, style, node, message);
    }

    /** xsltTransformError without transformation or stylesheet, like libxslt (the error context
        is then just "error"), but reported through these settings. */
    static void settingsError(Settings s, XmlNode node, String message)
    {
        vtransformError(s, null, null, node, message);
    }

    // Helpers ////////////////////////////////////////////////////////

    static boolean streq(String a, String b)
    {
        if (a == null || b == null) return a == b;
        return a.equals(b);
    }

    static boolean isBlank(int c)
    {
        return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
    }

    /** Port of xsltGetUTF8CharZ: one char from a string, 0 (length 1) at the end. The
        length is counted in chars of the java string. */
    static int utf8Char(String s, int pos, int[] len)
    {
        if (pos >= s.length())
        {
            len[0] = 1;
            return 0;
        }
        int c = s.codePointAt(pos);
        len[0] = Character.charCount(c);
        return c;
    }

    /** The char at i of s, 0 beyond the end (the C nul terminator). */
    private static char at(String s, int i)
    {
        return i < s.length() ? s.charAt(i) : 0;
    }

    /** libxml2 xmlIsCharQ. */
    private static boolean isChar(int c)
    {
        return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) ||
               (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
    }

    /** The namespaces in scope of an element, innermost first, like xmlGetNsList. */
    static Map<String, String> nsList(XmlNode node)
    {
        Map<String, String> list = null;
        for (XmlNode e = node; e != null; e = nodeParent(e))
        {
            if (nodeKind(e) != ELEMENT) continue;
            for (XmlNs ns : nodeNsDecls(e))
            {
                String prefix = nsPrefix(ns);
                if (list != null && list.containsKey(prefix)) continue;
                if (list == null) list = new LinkedHashMap<>();
                list.put(prefix, nsHref(ns));
            }
        }
        return list;
    }

    private static int nsNr(Map<String, String> l)
    {
        return l == null ? 0 : l.size();
    }

    /** The namespace uri of a node (null if none, possibly empty). */
    private static String nodeNsHref(XmlNode node)
    {
        XmlNs ns = nodeNs(node);
        return ns != null ? nsHref(ns) : null;
    }

    /** Port of xsltGetQNameURI: split name into its local part and return the namespace
        uri of the prefix, null if unprefixed. name[0] is null after an unbound prefix. */
    static String getQnameUri(Settings s, XmlNode node, String[] name)
    {
        String qname = name[0];
        if (qname == null || qname.isEmpty()) return null;
        if (node == null)
        {
            genericError(s, CFormat.format("QName: no element for namespace lookup %s\n", qname));
            name[0] = null;
            return null;
        }
        // Nasty but valid.
        if (qname.charAt(0) == ':') return null;
        int len = qname.indexOf(':');
        if (len < 0) return null;
        if (qname.startsWith("xml:"))
        {
            if (qname.length() == 4) return null;
            name[0] = qname.substring(4);
            return XML_NAMESPACE;
        }
        String prefix = qname.substring(0, len);
        XmlNs ns = nsLookupPrefix(node, prefix);
        if (ns == null)
        {
            genericError(s, CFormat.format("%s:%s : no namespace bound to prefix %s\n", prefix, qname.substring(len + 1), prefix));
            name[0] = null;
            return null;
        }
        name[0] = qname.substring(len + 1);
        return nsHref(ns);
    }

    private static int utf8Length(String s)
    {
        return s.getBytes(StandardCharsets.UTF_8).length;
    }

    /** Report an xpath error like libxml2 xmlXPathErr with the default handler: the message,
        then the expression and a marker when the position is inside it. */
    static void reportXpathError(Settings s, String expr, XPathException err)
    {
        if (err.quiet()) return;
        genericError(s, CFormat.format("XPath error : %s\n", !err.detail().isEmpty() ? err.detail() : err.staticMessage()));
        // libxml2 reports errors of compiled expressions without the expression.
        if (err.evaluating()) return;
        int len = expr != null ? utf8Length(expr) : 0;
        int offset = err.byteOffset();
        if (expr != null && offset >= 0 && offset < 100 && offset < len)
        {
            StringBuilder buf = new StringBuilder();
            for (int i = 0; i < offset; i++) buf.append(' ');
            buf.append('^');
            genericError(s, CFormat.format("%s\n%s\n", expr, buf.toString()));
        }
    }

    private static XPathSettings xpathSettings(Map<String, String> nss)
    {
        XPathSettings st = new XPathSettings();
        if (nss != null) st.namespaces = new LinkedHashMap<>(nss);
        return st;
    }

    static XPath xpathCompile(Settings s, XmlNode elem, String str, boolean no_variables)
    {
        Map<String, String> nss = elem != null ? nsList(elem) : null;
        XPathSettings st = xpathSettings(nss);
        st.no_variables = no_variables;
        try
        {
            return XPath.compile(str, st);
        }
        catch (XPathException err)
        {
            reportXpathError(s, str, err);
            return null;
        }
    }

    private static int allocateExtra(Stylesheet style)
    {
        return style.extras_nr++;
    }

    /** Port of xsltAllocateExtraCtxt. */
    static int allocateExtraCtxt(TransformCtx t)
    {
        while (t.extras.size() <= t.extras_nr) t.extras.add(new RuntimeExtra());
        return t.extras_nr++;
    }

    /** XSLT_RUNTIME_EXTRA(t, nr). */
    private static RuntimeExtra extra(TransformCtx t, int nr)
    {
        while (t.extras.size() <= nr) t.extras.add(new RuntimeExtra());
        return t.extras.get(nr);
    }

    // Patterns, port of libxslt pattern.c /////////////////////////////

    /** The pattern parser state (libxslt xsltParserContext). */
    private static final class ParserContext
    {
        Stylesheet style;
        TransformCtx ctxt;  // The transformation or null.
        String base;
        int cur;
        XmlNode elem;
        int error;
        Pattern comp;

        // CUR, NXT(val), NEXT and SKIP_BLANKS.
        char cur()
        {
            return at(base, cur);
        }

        char nxt(int val)
        {
            return at(base, cur + val);
        }

        void next()
        {
            if (cur() != 0) cur++;
        }

        void skipBlanks()
        {
            while (isBlank(cur())) next();
        }
    }

    private static Settings pctxSettings(ParserContext ctxt)
    {
        return ctxt.style != null ? ctxt.style.settings : null;
    }

    private static Pattern newCompMatch()
    {
        return new Pattern();
    }

    static double patternPriority(Pattern pattern)
    {
        return pattern != null ? pattern.priority : 0;
    }

    /** Port of xsltCompMatchAdd. */
    private static int compMatchAdd(ParserContext ctxt, Pattern comp, int op, String value, String value2, int novar)
    {
        Pattern.StepOp s = new Pattern.StepOp();
        comp.steps.add(s);
        s.op = op;
        s.value = value;
        s.value2 = value2;
        if (ctxt.ctxt != null)
        {
            s.previous_extra = allocateExtraCtxt(ctxt.ctxt);
            s.index_extra = allocateExtraCtxt(ctxt.ctxt);
            s.len_extra = allocateExtraCtxt(ctxt.ctxt);
        }
        else
        {
            s.previous_extra = allocateExtra(ctxt.style);
            s.index_extra = allocateExtra(ctxt.style);
            s.len_extra = allocateExtra(ctxt.style);
        }
        if (op == Pattern.OP_PREDICATE)
        {
            s.comp = xpathCompile(pctxSettings(ctxt), ctxt.elem, value, novar != 0);
            if (s.comp == null)
            {
                transformError(null, ctxt.style, ctxt.elem, "Failed to compile predicate\n");
                if (ctxt.style != null) ctxt.style.errors++;
            }
        }
        comp.nb_step++;
        return 0;
    }

    /** Port of xsltSwapTopCompMatch: reverse the two top steps. */
    private static void swapTopCompMatch(Pattern comp)
    {
        int j = comp.nb_step - 1;
        if (j > 0) Collections.swap(comp.steps, j - 1, j);
    }

    /** Port of xsltReverseCompMatch. */
    private static void reverseCompMatch(ParserContext ctxt, Pattern comp)
    {
        int i = 0;
        int j = comp.nb_step - 1;
        while (j > i)
        {
            Collections.swap(comp.steps, i, j);
            j--;
            i++;
        }
        compMatchAdd(ctxt, comp, Pattern.OP_END, null, null, 0);

        // Consecutive predicates: match by evaluating the whole pattern.
        for (i = 0; i < comp.nb_step - 1; i++)
        {
            if (comp.steps.get(i).op == Pattern.OP_PREDICATE && comp.steps.get(i + 1).op == Pattern.OP_PREDICATE)
            {
                comp.direct = 1;
                if (at(comp.pattern, 0) != '/')
                {
                    comp.pattern = "//" + comp.pattern;
                }
                break;
            }
        }
    }

    // The interpreter for the precompiled patterns.

    private static int patPushState(TransformCtx t, List<Pattern.StepState> states, int step, XmlNode node)
    {
        states.add(new Pattern.StepState(step, node));
        return 0;
    }

    private static boolean isDocument(XmlNode n)
    {
        return nodeKind(n) == DOCUMENT;
    }

    /** Evaluate an expression with namespaces from a pattern, like libxml2 xmlXPathEval with the
        namespaces of the xpath context set to nsList. */
    private static XPathResult evalWithNs(TransformCtx t, String expr, Map<String, String> ns_list, XmlNode node, long position, long size)
    {
        XPathSettings st = xpathSettings(ns_list);
        XPath comp;
        try
        {
            comp = XPath.compile(expr, st);
        }
        catch (XPathException err)
        {
            return null;
        }
        try
        {
            return t.xpath.eval(comp, node, position, size);
        }
        catch (XPathException err)
        {
            return null;
        }
    }

    /** Port of xsltTestCompMatchDirect: evaluate the pattern as an expression and look for the node. */
    private static int testCompMatchDirect(TransformCtx t, Pattern comp, XmlNode node, Map<String, String> ns_list)
    {
        XmlDocument doc = nodeDom(node);
        boolean is_rvt = Transform.isRtf(t, doc);
        Pattern.StepOp sel = comp.steps.get(0); // Store extra in first step arbitrarily.
        boolean nocache = false;

        XmlDocument prevdoc = (XmlDocument)extra(t, sel.previous_extra).ptr;
        XPathResult list = (XPathResult)extra(t, sel.len_extra).info;

        if (list == null || prevdoc != doc)
        {
            XmlNode parent = nodeParent(node);
            XPathResult newlist = evalWithNs(t, comp.pattern, ns_list, node, t.xp_position, t.xp_size);
            if (newlist == null) return -1;
            if (newlist.type() != XPathResult.NODESET)
            {
                return -1;
            }
            if (parent == null || is_rvt) nocache = true;
            if (!nocache)
            {
                list = newlist;
                extra(t, sel.len_extra).info = list;
                extra(t, sel.previous_extra).ptr = doc;
                extra(t, sel.index_extra).ival = 0;
            }
            else
            {
                list = newlist;
            }
        }
        int found = 0;
        for (int j = 0; j < list.size(); ++j)
        {
            if (list.node(j) == node)
            {
                found = 1;
                break;
            }
        }
        return found;
    }

    /** Port of xsltTestStepMatch. Returns 1 if the node matches the step, 0 if not, -1 on an error. */
    private static int testStepMatch(TransformCtx t, XmlNode node, Pattern.StepOp step)
    {
        int k = nodeKind(node);
        switch (step.op)
        {
        case Pattern.OP_ROOT:
            return isDocument(node) ? 1 : 0;
        case Pattern.OP_ELEM:
        {
            if (k != ELEMENT) return 0;
            if (step.value == null) return 1;
            if (!step.value.equals(nodeName(node))) return 0;
            // Namespace test.
            XmlNs ns = nodeNs(node);
            if (ns == null)
            {
                if (step.value2 != null) return 0;
            }
            else if (nsHref(ns) != null)
            {
                if (step.value2 == null) return 0;
                if (!step.value2.equals(nsHref(ns))) return 0;
            }
            return 1;
        }
        case Pattern.OP_ATTR:
        {
            if (k != ATTRIBUTE) return 0;
            if (step.value != null && !step.value.equals(nodeName(node))) return 0;
            XmlNs ns = nodeNs(node);
            if (ns == null)
            {
                if (step.value2 != null) return 0;
            }
            else if (step.value2 != null)
            {
                if (!streq(step.value2, nsHref(ns))) return 0;
            }
            return 1;
        }
        case Pattern.OP_ID:
        {
            if (k != ELEMENT) return 0;
            XmlNode e = t.xpath.getId(node, step.value);
            if (e != node) return 0;
            break;
        }
        case Pattern.OP_KEY:
        {
            XPathResult list = Transform.getKey(t, step.value, step.value3, step.value2);
            if (list == null) return 0;
            boolean found = false;
            for (int i = 0; i < list.size(); ++i)
            {
                if (list.node(i) == node) found = true;
            }
            if (!found) return 0;
            break;
        }
        case Pattern.OP_NS:
        {
            if (k != ELEMENT) return 0;
            XmlNs ns = nodeNs(node);
            if (ns == null)
            {
                if (step.value != null) return 0;
            }
            else if (nsHref(ns) != null)
            {
                if (step.value == null) return 0;
                if (!step.value.equals(nsHref(ns))) return 0;
            }
            break;
        }
        case Pattern.OP_ALL:
            if (k != ELEMENT) return 0;
            break;
        case Pattern.OP_PI:
            if (k != PI) return 0;
            if (step.value != null && !streq(step.value, nodeName(node))) return 0;
            break;
        case Pattern.OP_COMMENT:
            if (k != COMMENT) return 0;
            break;
        case Pattern.OP_TEXT:
            if (k != TEXT && k != CDATA) return 0;
            break;
        case Pattern.OP_NODE:
            switch (k)
            {
            case ELEMENT:
            case CDATA:
            case PI:
            case COMMENT:
            case TEXT:
                break;
            default:
                return 0;
            }
            break;
        default:
            transformError(t, null, node, CFormat.format("xsltTestStepMatch: unexpected step op %d\n", step.op));
            return -1;
        }
        return 1;
    }

    /** Port of xsltEvalXPathPredicate (templates.c): a number is compared with the context position. */
    private static int evalXpathPredicate(TransformCtx t, XPath comp)
    {
        if (t.inst == null)
        {
            transformError(t, null, null, "xsltEvalXPathPredicate: No context or instruction\n");
            return 0;
        }
        XPathResult res;
        try
        {
            res = t.xpath.eval(comp, t.node, t.xp_position, t.xp_size);
        }
        catch (XPathException err)
        {
            res = null;
        }
        int ret;
        if (res != null)
        {
            // libxml2 xmlXPathEvalPredicate.
            if (res.type() == XPathResult.NUMBER) ret = res.numberValue() == (double)t.xp_position ? 1 : 0;
            else ret = res.castToBoolean() ? 1 : 0;
        }
        else
        {
            t.state = TransformCtx.STATE_STOPPED;
            ret = 0;
        }
        return ret;
    }

    /** Port of xsltTestPredicateMatch. */
    private static int testPredicateMatch(TransformCtx t, Pattern comp, XmlNode node, Pattern.StepOp step, Pattern.StepOp sel)
    {
        int pos = 0, len = 0;
        if (step.value == null || step.comp == null || sel == null) return 0;

        XmlDocument doc = nodeDom(node);
        boolean is_rvt = Transform.isRtf(t, doc);

        // Recompute the context size and position.
        long old_cs = t.xp_size;
        long old_cp = t.xp_position;
        {
            boolean nocache = false;
            XmlNode previous = (XmlNode)extra(t, sel.previous_extra).ptr;
            if (previous != null && nodeParent(previous) == nodeParent(node))
            {
                // Just walk back to adjust the index.
                int indx = 0;
                XmlNode sibling = node;
                while (sibling != null)
                {
                    if (sibling == previous) break;
                    if (testStepMatch(t, sibling, sel) != 0) indx++;
                    sibling = nodePrev(sibling);
                }
                if (sibling == null)
                {
                    // Going backward in document order.
                    indx = 0;
                    sibling = node;
                    while (sibling != null)
                    {
                        if (sibling == previous) break;
                        if (testStepMatch(t, sibling, sel) != 0) indx--;
                        sibling = nodeNext(sibling);
                    }
                }
                if (sibling != null)
                {
                    pos = extra(t, sel.index_extra).ival + indx;
                    // In a result tree fragment len is kept but the node cannot be cached.
                    len = extra(t, sel.len_extra).ival;
                    if (!is_rvt)
                    {
                        extra(t, sel.previous_extra).ptr = node;
                        extra(t, sel.index_extra).ival = pos;
                    }
                }
                else
                {
                    pos = 0;
                }
            }
            else
            {
                // Recompute the index.
                XmlNode parent = nodeParent(node);
                // Like libxml2 parent->children: an attribute is never found among them, then
                // the position stays 0 and the context position and size are not changed.
                XmlNode siblings = parent != null ? nodeFirstChild(parent) : null;
                while (siblings != null)
                {
                    if (siblings == node)
                    {
                        len++;
                        pos = len;
                    }
                    else if (testStepMatch(t, siblings, sel) != 0)
                    {
                        len++;
                    }
                    siblings = nodeNext(siblings);
                }
                if (parent == null)
                {
                    nocache = true;
                }
                else
                {
                    XmlNode top = parent;
                    while (nodeParent(top) != null) top = nodeParent(top);
                    if (!isDocument(top) || top != domDocument(doc)) nocache = true;
                }
            }
            if (pos != 0)
            {
                t.xp_size = len;
                t.xp_position = pos;
                if (!is_rvt && !nocache)
                {
                    extra(t, sel.previous_extra).ptr = node;
                    extra(t, sel.index_extra).ival = pos;
                    extra(t, sel.len_extra).ival = len;
                }
            }
        }

        XmlNode old_node = t.node;
        t.node = node;
        int match = evalXpathPredicate(t, step.comp);
        if (pos != 0)
        {
            t.xp_size = old_cs;
            t.xp_position = old_cp;
        }
        t.node = old_node;
        return match;
    }

    private static boolean nameAndNsMatch(Pattern.StepOp step, XmlNode node)
    {
        if (!step.value.equals(nodeName(node))) return false;
        XmlNs ns = nodeNs(node);
        if (ns == null) return step.value2 == null;
        if (nsHref(ns) != null)
        {
            if (step.value2 == null) return false;
            if (!step.value2.equals(nsHref(ns))) return false;
        }
        return true;
    }

    /** Port of xsltTestCompMatch. Returns 1 if the node matches, 0 if not, -1 on an error. */
    private static int testCompMatch(TransformCtx t, Pattern comp, XmlNode match_node, String mode, String mode_uri)
    {
        int i;
        int found = 0;
        XmlNode node = match_node;
        Pattern.StepOp step, sel = null;
        List<Pattern.StepState> states = new ArrayList<>();

        if (comp == null || node == null || t == null)
        {
            transformError(t, null, node, "xsltTestCompMatch: null arg\n");
            return -1;
        }
        if (!streq(comp.mode, mode)) return 0;
        if (!streq(comp.mode_uri, mode_uri)) return 0;

        // Some XPath functions rely on inst being set correctly.
        XmlNode old_inst = t.inst;
        t.inst = comp.node;

        // The gotos of the C version: what to do after the loop over the steps.
        final int GO_FOUND = 1, GO_EXIT = 2, GO_ROLLBACK = 3;
        i = 0;
        restart:
        while (true)
        {
            int go = GO_FOUND;
            steps:
            for (; i < comp.nb_step; i++)
            {
                step = comp.steps.get(i);
                if (step.op != Pattern.OP_PREDICATE) sel = step;
                switch (step.op)
                {
                case Pattern.OP_END:
                    go = GO_FOUND;
                    break steps;
                case Pattern.OP_PARENT:
                    if (isDocument(node) || nodeKind(node) == XmlDomApi.NAMESPACE) { go = GO_ROLLBACK; break steps; }
                    node = nodeParent(node);
                    if (node == null) { go = GO_ROLLBACK; break steps; }
                    if (step.value == null) continue;
                    if (nodeKind(node) != ELEMENT || nodeName(node) == null) { go = GO_ROLLBACK; break steps; }
                    if (!nameAndNsMatch(step, node)) { go = GO_ROLLBACK; break steps; }
                    continue;
                case Pattern.OP_ANCESTOR:
                    // TODO: implement coalescing of ANCESTOR/NODE ops.
                    if (step.value == null)
                    {
                        step = comp.step(i + 1);
                        if (step.op == Pattern.OP_ROOT) { go = GO_FOUND; break steps; }
                        // Added NS, ID and KEY as a result of bug 168208.
                        if (step.op != Pattern.OP_ELEM && step.op != Pattern.OP_ALL && step.op != Pattern.OP_NS &&
                            step.op != Pattern.OP_ID && step.op != Pattern.OP_KEY)
                        {
                            go = GO_ROLLBACK;
                            break steps;
                        }
                    }
                    if (node == null) { go = GO_ROLLBACK; break steps; }
                    if (isDocument(node) || nodeKind(node) == XmlDomApi.NAMESPACE) { go = GO_ROLLBACK; break steps; }
                    node = nodeParent(node);
                    if (step.op != Pattern.OP_ELEM && step.op != Pattern.OP_ALL)
                    {
                        patPushState(t, states, i, node);
                        continue;
                    }
                    i++;
                    sel = step;
                    if (step.value == null)
                    {
                        patPushState(t, states, i - 1, node);
                        continue;
                    }
                    while (node != null)
                    {
                        if (nodeKind(node) == ELEMENT && step.value.equals(nodeName(node)))
                        {
                            XmlNs ns = nodeNs(node);
                            if (ns == null)
                            {
                                if (step.value2 == null) break;
                            }
                            else if (nsHref(ns) != null)
                            {
                                if (step.value2 != null && step.value2.equals(nsHref(ns))) break;
                            }
                        }
                        node = nodeParent(node);
                    }
                    if (node == null) { go = GO_ROLLBACK; break steps; }
                    patPushState(t, states, i - 1, node);
                    continue;
                case Pattern.OP_PREDICATE:
                    // Cascading predicates, or a predicate after an op that is not optimized yet:
                    // evaluate the whole pattern.
                    if (comp.direct != 0)
                    {
                        found = testCompMatchDirect(t, comp, match_node, comp.ns_list);
                        go = GO_EXIT;
                        break steps;
                    }
                    if (testPredicateMatch(t, comp, node, step, sel) == 0) { go = GO_ROLLBACK; break steps; }
                    break;
                default:
                    if (testStepMatch(t, node, step) != 1) { go = GO_ROLLBACK; break steps; }
                    break;
                }
            }
            if (go == GO_FOUND)
            {
                found = 1;
                break restart;
            }
            if (go == GO_EXIT) break restart;
            // rollback: Got an error, try to roll back.
            if (states.isEmpty())
            {
                found = 0;
                break restart;
            }
            Pattern.StepState st = states.remove(states.size() - 1);
            i = st.step;
            node = st.node;
        }
        t.inst = old_inst;
        return found;
    }

    /** Does the node match any alternative of the pattern (libxslt xsltTestCompMatchList)?
        Returns 1 or 0, -1 on an error. */
    static int patternMatch(TransformCtx t, XmlNode node, Pattern comp)
    {
        if (t == null || node == null) return -1;
        while (comp != null)
        {
            int ret = testCompMatch(t, comp, node, null, null);
            if (ret == 1) return 1;
            comp = comp.next;
        }
        return 0;
    }

    /** Port of xsltCompMatchClearCache: drop the cached node list of a direct match. */
    static void compMatchClearCache(TransformCtx t, Pattern comp)
    {
        if (t == null || comp == null || comp.nb_step == 0) return;
        Pattern.StepOp sel = comp.steps.get(0);
        XPathResult list = (XPathResult)extra(t, sel.len_extra).info;
        if (list != null)
        {
            extra(t, sel.len_extra).info = null;
            extra(t, sel.previous_extra).ptr = null;
            extra(t, sel.index_extra).ival = 0;
        }
    }

    // The pattern parser (xsltCompileLocationPathPattern etc).

    private static void push(ParserContext ctxt, int op, String val, String val2, int novar)
    {
        compMatchAdd(ctxt, ctxt.comp, op, val, val2, novar);
    }

    /** Port of xsltScanLiteral. */
    private static String scanLiteral(ParserContext ctxt)
    {
        ctxt.skipBlanks();
        char q = ctxt.cur();
        if (q != '"' && q != '\'')
        {
            ctxt.error = 1;
            return null;
        }
        ctxt.next();
        int start = ctxt.cur;
        int cur = start;
        int[] len = new int[1];
        int val = utf8Char(ctxt.base, cur, len);
        while (isChar(val) && val != q)
        {
            cur += len[0];
            val = utf8Char(ctxt.base, cur, len);
        }
        if (!isChar(val))
        {
            ctxt.error = 1;
            return null;
        }
        String ret = ctxt.base.substring(start, cur);
        cur += len[0];
        ctxt.cur = cur;
        return ret;
    }

    /** Port of xsltScanNCName. */
    private static String scanNcname(ParserContext ctxt)
    {
        ctxt.skipBlanks();
        int q = ctxt.cur, cur = ctxt.cur;
        int[] len = new int[1];
        int val = utf8Char(ctxt.base, cur, len);
        if (val < 0 || (!XmlChars.isBaseChar(val) && !XmlChars.isIdeographic(val) && val != '_')) return null;
        while (val >= 0 && (XmlChars.isBaseChar(val) || XmlChars.isIdeographic(val) || XmlChars.isDigit(val) ||
                            val == '.' || val == '-' || val == '_' || XmlChars.isCombining(val) || XmlChars.isExtender(val)))
        {
            cur += len[0];
            val = utf8Char(ctxt.base, cur, len);
        }
        String ret = ctxt.base.substring(q, cur);
        ctxt.cur = cur;
        return ret;
    }

    /** libxml2 xmlXPathIsNodeType. */
    private static boolean isNodeType(String name)
    {
        return name.equals("node") || name.equals("comment") || name.equals("text") ||
               name.equals("processing-instruction");
    }

    /** Port of xsltCompileIdKeyPattern. */
    private static void compileIdKeyPattern(ParserContext ctxt, String name, int aid, int novar, int axis)
    {
        String lit = null;
        String lit2 = null;
        if (ctxt.cur() != '(')
        {
            settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ( expected\n");
            ctxt.error = 1;
            return;
        }
        if (aid != 0 && name.equals("id"))
        {
            if (axis != 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : NodeTest expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            ctxt.skipBlanks();
            lit = scanLiteral(ctxt);
            if (ctxt.error != 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : Literal expected\n");
                return;
            }
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            push(ctxt, Pattern.OP_ID, lit, null, novar);
        }
        else if (aid != 0 && name.equals("key"))
        {
            if (axis != 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : NodeTest expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            ctxt.skipBlanks();
            lit = scanLiteral(ctxt);
            if (ctxt.error != 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : Literal expected\n");
                return;
            }
            ctxt.skipBlanks();
            if (ctxt.cur() != ',')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : , expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            ctxt.skipBlanks();
            lit2 = scanLiteral(ctxt);
            if (ctxt.error != 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : Literal expected\n");
                return;
            }
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            // URGENT TODO (libxslt): support namespace in keys.
            push(ctxt, Pattern.OP_KEY, lit, lit2, novar);
        }
        else if (name.equals("processing-instruction"))
        {
            ctxt.next();
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                lit = scanLiteral(ctxt);
                if (ctxt.error != 0)
                {
                    settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : Literal expected\n");
                    return;
                }
                ctxt.skipBlanks();
                if (ctxt.cur() != ')')
                {
                    settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                    ctxt.error = 1;
                    return;
                }
            }
            ctxt.next();
            push(ctxt, Pattern.OP_PI, lit, null, novar);
        }
        else if (name.equals("text"))
        {
            ctxt.next();
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            push(ctxt, Pattern.OP_TEXT, null, null, novar);
        }
        else if (name.equals("comment"))
        {
            ctxt.next();
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            push(ctxt, Pattern.OP_COMMENT, null, null, novar);
        }
        else if (name.equals("node"))
        {
            ctxt.next();
            ctxt.skipBlanks();
            if (ctxt.cur() != ')')
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.next();
            if (axis == Pattern.AXIS_ATTRIBUTE)
            {
                push(ctxt, Pattern.OP_ATTR, null, null, novar);
            }
            else
            {
                push(ctxt, Pattern.OP_NODE, null, null, novar);
            }
        }
        else if (aid != 0)
        {
            settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : expecting 'key' or 'id' or node type\n");
            ctxt.error = 1;
            return;
        }
        else
        {
            settingsError(pctxSettings(ctxt), null, "xsltCompileIdKeyPattern : node type\n");
            ctxt.error = 1;
            return;
        }
    }

    /** Port of xsltCompileStepPattern. */
    private static void compileStepPattern(ParserContext ctxt, String token, int novar)
    {
        String url = null;
        String ret = null;
        int level;
        int axis = Pattern.AXIS_NONE;

        ctxt.skipBlanks();
        if (token == null && ctxt.cur() == '@')
        {
            ctxt.next();
            axis = Pattern.AXIS_ATTRIBUTE;
        }
        parse_node_test:
        while (true)
        {
            if (token == null) token = scanNcname(ctxt);
            if (token == null)
            {
                if (ctxt.cur() == '*')
                {
                    ctxt.next();
                    if (axis == Pattern.AXIS_ATTRIBUTE)
                    {
                        push(ctxt, Pattern.OP_ATTR, null, null, novar);
                    }
                    else
                    {
                        push(ctxt, Pattern.OP_ALL, null, null, novar);
                    }
                    break parse_node_test; // goto parse_predicate
                }
                else
                {
                    settingsError(pctxSettings(ctxt), null, "xsltCompileStepPattern : Name expected\n");
                    ctxt.error = 1;
                    return;
                }
            }

            ctxt.skipBlanks();
            if (ctxt.cur() == '(')
            {
                compileIdKeyPattern(ctxt, token, 0, novar, axis);
                token = null;
                if (ctxt.error != 0) return;
            }
            else if (ctxt.cur() == ':')
            {
                ctxt.next();
                if (ctxt.cur() != ':')
                {
                    String prefix = token;
                    // This is a namespace match.
                    token = scanNcname(ctxt);
                    XmlNs ns = ctxt.elem != null ? nsLookupPrefix(ctxt.elem, prefix) : null;
                    if (ns == null)
                    {
                        settingsError(pctxSettings(ctxt), null, CFormat.format("xsltCompileStepPattern : no namespace bound to prefix %s\n", prefix));
                        ctxt.error = 1;
                        return;
                    }
                    url = nsHref(ns);
                    if (token == null)
                    {
                        if (ctxt.cur() == '*')
                        {
                            ctxt.next();
                            if (axis == Pattern.AXIS_ATTRIBUTE)
                            {
                                push(ctxt, Pattern.OP_ATTR, null, url, novar);
                                url = null;
                            }
                            else
                            {
                                push(ctxt, Pattern.OP_NS, url, null, novar);
                                url = null;
                            }
                        }
                        else
                        {
                            settingsError(pctxSettings(ctxt), null, "xsltCompileStepPattern : Name expected\n");
                            ctxt.error = 1;
                            return;
                        }
                    }
                    else
                    {
                        if (axis == Pattern.AXIS_ATTRIBUTE)
                        {
                            push(ctxt, Pattern.OP_ATTR, token, url, novar);
                            token = null;
                            url = null;
                        }
                        else
                        {
                            push(ctxt, Pattern.OP_ELEM, token, url, novar);
                            token = null;
                            url = null;
                        }
                    }
                }
                else
                {
                    if (axis != 0)
                    {
                        settingsError(pctxSettings(ctxt), null, "xsltCompileStepPattern : NodeTest expected\n");
                        ctxt.error = 1;
                        return;
                    }
                    ctxt.next();
                    if (token.equals("child"))
                    {
                        axis = Pattern.AXIS_CHILD;
                    }
                    else if (token.equals("attribute"))
                    {
                        axis = Pattern.AXIS_ATTRIBUTE;
                    }
                    else
                    {
                        settingsError(pctxSettings(ctxt), null, "xsltCompileStepPattern : 'child' or 'attribute' expected\n");
                        ctxt.error = 1;
                        return;
                    }
                    token = null;
                    ctxt.skipBlanks();
                    token = scanNcname(ctxt);
                    continue parse_node_test;
                }
            }
            else
            {
                String[] tok = { token };
                String uri = getQnameUri(pctxSettings(ctxt), ctxt.elem, tok);
                token = tok[0];
                if (token == null)
                {
                    ctxt.error = 1;
                    return;
                }
                if (uri != null) url = uri;
                if (axis == Pattern.AXIS_ATTRIBUTE)
                {
                    push(ctxt, Pattern.OP_ATTR, token, url, novar);
                    token = null;
                    url = null;
                }
                else
                {
                    push(ctxt, Pattern.OP_ELEM, token, url, novar);
                    token = null;
                    url = null;
                }
            }
            break;
        }
        // parse_predicate:
        ctxt.skipBlanks();
        level = 0;
        while (ctxt.cur() == '[')
        {
            level++;
            ctxt.next();
            int q = ctxt.cur;
            while (ctxt.cur() != 0)
            {
                // Skip over nested predicates.
                if (ctxt.cur() == '[')
                {
                    level++;
                }
                else if (ctxt.cur() == ']')
                {
                    level--;
                    if (level == 0) break;
                }
                else if (ctxt.cur() == '"')
                {
                    ctxt.next();
                    while (ctxt.cur() != 0 && ctxt.cur() != '"') ctxt.next();
                }
                else if (ctxt.cur() == '\'')
                {
                    ctxt.next();
                    while (ctxt.cur() != 0 && ctxt.cur() != '\'') ctxt.next();
                }
                ctxt.next();
            }
            if (ctxt.cur() == 0)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileStepPattern : ']' expected\n");
                ctxt.error = 1;
                return;
            }
            ret = ctxt.base.substring(q, ctxt.cur);
            push(ctxt, Pattern.OP_PREDICATE, ret, null, novar);
            ret = null;
            // Push the predicate lower than the local test.
            swapTopCompMatch(ctxt.comp);
            ctxt.next();
            ctxt.skipBlanks();
        }
    }

    /** Port of xsltCompileRelativePathPattern. */
    private static void compileRelativePathPattern(ParserContext ctxt, String token, int novar)
    {
        compileStepPattern(ctxt, token, novar);
        if (ctxt.error != 0) return;
        ctxt.skipBlanks();
        while (ctxt.cur() != 0 && ctxt.cur() != '|')
        {
            if (ctxt.cur() == '/' && ctxt.nxt(1) == '/')
            {
                push(ctxt, Pattern.OP_ANCESTOR, null, null, novar);
                ctxt.next();
                ctxt.next();
                ctxt.skipBlanks();
                compileStepPattern(ctxt, null, novar);
            }
            else if (ctxt.cur() == '/')
            {
                push(ctxt, Pattern.OP_PARENT, null, null, novar);
                ctxt.next();
                ctxt.skipBlanks();
                compileStepPattern(ctxt, null, novar);
            }
            else
            {
                ctxt.error = 1;
            }
            if (ctxt.error != 0) return;
            ctxt.skipBlanks();
        }
    }

    /** Port of xsltCompileLocationPathPattern. */
    private static void compileLocationPathPattern(ParserContext ctxt, int novar)
    {
        ctxt.skipBlanks();
        if (ctxt.cur() == '/' && ctxt.nxt(1) == '/')
        {
            // Since the query is reversed a leading // can be safely ignored.
            ctxt.next();
            ctxt.next();
            ctxt.comp.priority = 0.5f; // '//' means not 0 priority.
            compileRelativePathPattern(ctxt, null, novar);
        }
        else if (ctxt.cur() == '/')
        {
            // We need to find root as the parent.
            ctxt.next();
            ctxt.skipBlanks();
            push(ctxt, Pattern.OP_ROOT, null, null, novar);
            if (ctxt.cur() != 0 && ctxt.cur() != '|')
            {
                push(ctxt, Pattern.OP_PARENT, null, null, novar);
                compileRelativePathPattern(ctxt, null, novar);
            }
        }
        else if (ctxt.cur() == '*')
        {
            compileRelativePathPattern(ctxt, null, novar);
        }
        else if (ctxt.cur() == '@')
        {
            compileRelativePathPattern(ctxt, null, novar);
        }
        else
        {
            String name = scanNcname(ctxt);
            if (name == null)
            {
                settingsError(pctxSettings(ctxt), null, "xsltCompileLocationPathPattern : Name expected\n");
                ctxt.error = 1;
                return;
            }
            ctxt.skipBlanks();
            if (ctxt.cur() == '(' && !isNodeType(name))
            {
                compileIdKeyPattern(ctxt, name, 1, novar, Pattern.AXIS_NONE);
                name = null;
                if (ctxt.error != 0) return;
                if (ctxt.cur() == '/' && ctxt.nxt(1) == '/')
                {
                    push(ctxt, Pattern.OP_ANCESTOR, null, null, novar);
                    ctxt.next();
                    ctxt.next();
                    ctxt.skipBlanks();
                    compileRelativePathPattern(ctxt, null, novar);
                }
                else if (ctxt.cur() == '/')
                {
                    push(ctxt, Pattern.OP_PARENT, null, null, novar);
                    ctxt.next();
                    ctxt.skipBlanks();
                    compileRelativePathPattern(ctxt, null, novar);
                }
                return;
            }
            compileRelativePathPattern(ctxt, name, novar);
        }
    }

    /** Port of xsltCompilePatternInternal. */
    private static Pattern compilePatternInternal(String pattern, XmlNode node, Stylesheet style, TransformCtx runtime, int novar)
    {
        Pattern element, first = null, previous = null;
        int current, start, end, level;

        if (pattern == null)
        {
            settingsError(style != null ? style.settings : null, node, "xsltCompilePattern : NULL pattern\n");
            return null;
        }

        ParserContext ctxt = new ParserContext();
        ctxt.style = style;
        ctxt.ctxt = runtime;
        ctxt.elem = node;
        current = end = 0;
        while (at(pattern, current) != 0)
        {
            start = current;
            while (isBlank(at(pattern, current))) current++;
            end = current;
            level = 0;
            while (at(pattern, end) != 0 && (at(pattern, end) != '|' || level != 0))
            {
                if (at(pattern, end) == '[')
                {
                    level++;
                }
                else if (at(pattern, end) == ']')
                {
                    level--;
                }
                else if (at(pattern, end) == '\'')
                {
                    end++;
                    while (at(pattern, end) != 0 && at(pattern, end) != '\'') end++;
                }
                else if (at(pattern, end) == '"')
                {
                    end++;
                    while (at(pattern, end) != 0 && at(pattern, end) != '"') end++;
                }
                if (at(pattern, end) == 0) break;
                end++;
            }
            if (current == end)
            {
                settingsError(style != null ? style.settings : null, node, "xsltCompilePattern : NULL pattern\n");
                return null;
            }
            element = newCompMatch();
            if (first == null) first = element;
            else if (previous != null) previous.next = element;
            previous = element;

            ctxt.comp = element;
            ctxt.base = pattern.substring(start, end);
            ctxt.cur = current - start;
            element.pattern = ctxt.base;
            element.node = node;
            element.ns_list = node != null ? nsList(node) : null;

            // Preset default priority to be zero, may be changed by the location path.
            element.priority = 0;
            compileLocationPathPattern(ctxt, novar);
            if (ctxt.error != 0)
            {
                transformError(null, style, node, CFormat.format("xsltCompilePattern : failed to compile '%s'\n", element.pattern));
                if (style != null) style.errors++;
                return null;
            }

            // Reverse for faster interpretation.
            reverseCompMatch(ctxt, element);

            // Set-up the priority.
            if (element.priority == 0)
            {
                Pattern.StepOp s0 = element.step(0);
                Pattern.StepOp s1 = element.step(1);
                if ((s0.op == Pattern.OP_ELEM || s0.op == Pattern.OP_ATTR || s0.op == Pattern.OP_PI) &&
                    s0.value != null && s1.op == Pattern.OP_END)
                {
                    ; // Previously preset.
                }
                else if (s0.op == Pattern.OP_ATTR && s0.value2 != null && s1.op == Pattern.OP_END)
                {
                    element.priority = -0.25f;
                }
                else if (s0.op == Pattern.OP_NS && s0.value != null && s1.op == Pattern.OP_END)
                {
                    element.priority = -0.25f;
                }
                else if (s0.op == Pattern.OP_ATTR && s0.value == null && s0.value2 == null && s1.op == Pattern.OP_END)
                {
                    element.priority = -0.5f;
                }
                else if ((s0.op == Pattern.OP_PI || s0.op == Pattern.OP_TEXT || s0.op == Pattern.OP_ALL ||
                          s0.op == Pattern.OP_NODE || s0.op == Pattern.OP_COMMENT) && s1.op == Pattern.OP_END)
                {
                    element.priority = -0.5f;
                }
                else
                {
                    element.priority = 0.5f;
                }
            }
            if (at(pattern, end) == '|') end++;
            current = end;
        }
        if (end == 0)
        {
            transformError(null, style, node, "xsltCompilePattern : NULL pattern\n");
            if (style != null) style.errors++;
            return null;
        }
        return first;
    }

    /** Compile a pattern, element (in the stylesheet document) gives the namespaces in
        scope. With no_variables, variable references in predicates are errors (template
        match patterns). Returns null after reporting an error. */
    static Pattern patternCompile(Stylesheet style, String pattern, XmlNode element, boolean no_variables)
    {
        return compilePatternInternal(pattern, element, style, null, no_variables ? 1 : 0);
    }

    // Template registration and lookup (pattern.c xsltAddTemplate, xsltGetTemplate).

    /** The key of the template hash, like xmlHashAddEntry3 with name, mode and mode uri. */
    static String hash3Key(String a, String b, String c)
    {
        StringBuilder mb = new StringBuilder();
        String[] parts = { a, b, c };
        for (int i = 0; i < 3; ++i)
        {
            if (i != 0) mb.append('\u0001');
            if (parts[i] != null) mb.append(parts[i]);
            else mb.append('\u0002');
        }
        return mb.toString();
    }

    /** Insert pat into a priority ordered list (equal priorities: the later one first), returns the new head. */
    private static Pattern insertByPriority(Pattern top, Pattern pat)
    {
        Pattern list = top;
        if (list == null)
        {
            top = pat;
            pat.next = null;
        }
        else if (list.priority <= pat.priority)
        {
            pat.next = list;
            top = pat;
        }
        else
        {
            while (list.next != null)
            {
                if (list.next.priority <= pat.priority) break;
                list = list.next;
            }
            pat.next = list.next;
            list.next = pat;
        }
        return top;
    }

    // The generic match lists of a stylesheet (the C XmlLibxslt1Pattern **top).
    private static final int TOP_NONE = 0, TOP_ATTR = 1, TOP_ELEM = 2, TOP_ROOT = 3, TOP_KEY = 4, TOP_PI = 5,
                             TOP_COMMENT = 6, TOP_TEXT = 7;

    private static void insertInTop(Stylesheet style, int top, Pattern pat)
    {
        switch (top)
        {
        case TOP_ATTR: style.attr_match = insertByPriority(style.attr_match, pat); break;
        case TOP_ELEM: style.elem_match = insertByPriority(style.elem_match, pat); break;
        case TOP_ROOT: style.root_match = insertByPriority(style.root_match, pat); break;
        case TOP_KEY: style.key_match = insertByPriority(style.key_match, pat); break;
        case TOP_PI: style.pi_match = insertByPriority(style.pi_match, pat); break;
        case TOP_COMMENT: style.comment_match = insertByPriority(style.comment_match, pat); break;
        case TOP_TEXT: style.text_match = insertByPriority(style.text_match, pat); break;
        default: break;
        }
    }

    /** Port of xsltAddTemplate. */
    private static int addTemplate(Stylesheet style, Template cur, String mode, String mode_uri)
    {
        if (style == null || cur == null) return -1;
        if (cur.next != null) cur.position = cur.next.position + 1;

        // Register a named template.
        if (cur.name != null)
        {
            if (style.named_templates == null) style.named_templates = new HashMap<>();
            String key = hash3Key(cur.name, cur.name_uri, null);
            if (style.named_templates.get(key) != null)
            {
                transformError(null, style, cur.elem, CFormat.format("xsl:template: error duplicate name '%s'\n", cur.name));
                style.errors++;
                return -1;
            }
            style.named_templates.put(key, cur);
        }

        if (cur.match == null)
        {
            if (cur.name == null)
            {
                transformError(null, style, cur.elem, "xsl:template: need to specify match or name attribute\n");
                style.errors++;
                return -1;
            }
            return 0;
        }

        float priority = cur.priority;
        Pattern pat = compilePatternInternal(cur.match, cur.elem, style, null, 1);
        if (pat == null) return -1;
        while (pat != null)
        {
            Pattern next = pat.next;
            pat.next = null;
            String name = null;
            int top = TOP_NONE;

            pat.templ = cur;
            if (mode != null) pat.mode = mode;
            if (mode_uri != null) pat.mode_uri = mode_uri;
            if (priority != PAT_NO_PRIORITY) pat.priority = priority;

            // Insert it in the hash table list corresponding to its lookup name.
            Pattern.StepOp s0 = pat.step(0);
            switch (s0.op)
            {
            case Pattern.OP_ATTR:
                if (s0.value != null) name = s0.value;
                else top = TOP_ATTR;
                break;
            case Pattern.OP_PARENT:
            case Pattern.OP_ANCESTOR:
                top = TOP_ELEM;
                break;
            case Pattern.OP_ROOT:
                top = TOP_ROOT;
                break;
            case Pattern.OP_KEY:
                top = TOP_KEY;
                break;
            case Pattern.OP_ID:
            case Pattern.OP_NS:
            case Pattern.OP_ALL:
                top = TOP_ELEM;
                break;
            case Pattern.OP_END:
            case Pattern.OP_PREDICATE:
                transformError(null, style, null, "xsltAddTemplate: invalid compiled pattern\n");
                return -1;
            case Pattern.OP_PI:
                if (s0.value != null) name = s0.value;
                else top = TOP_PI;
                break;
            case Pattern.OP_COMMENT:
                top = TOP_COMMENT;
                break;
            case Pattern.OP_TEXT:
                top = TOP_TEXT;
                break;
            case Pattern.OP_ELEM:
            case Pattern.OP_NODE:
                if (s0.value != null) name = s0.value;
                else top = TOP_ELEM;
                break;
            default:
                break;
            }
            if (name != null)
            {
                if (style.templates_hash == null) style.templates_hash = new HashMap<>();
                String key = hash3Key(name, mode, mode_uri);
                Pattern list = style.templates_hash.get(key);
                if (list == null)
                {
                    style.templates_hash.put(key, pat);
                }
                else
                {
                    // Note <= since one must choose, among the matching template rules that are
                    // left, the one that occurs last in the stylesheet.
                    Pattern head = insertByPriority(list, pat);
                    if (head != list) style.templates_hash.put(key, head);
                }
            }
            else if (top != TOP_NONE)
            {
                insertInTop(style, top, pat);
            }
            pat = next;
        }
        return 0;
    }

    /** Port of xsltNextImport (imports.c). */
    static Stylesheet nextImport(Stylesheet cur)
    {
        if (cur == null) return null;
        if (cur.imports != null) return cur.imports;
        if (cur.next != null) return cur.next;
        do
        {
            cur = cur.parent;
            if (cur == null) break;
            if (cur.next != null) return cur.next;
        } while (cur != null);
        return cur;
    }

    private static Pattern firstMatch(TransformCtx t, Pattern list, XmlNode node, Template[] ret, float[] priority)
    {
        while (list != null &&
               (ret[0] == null || list.priority > priority[0] ||
                (list.priority == priority[0] && list.templ.position > ret[0].position)))
        {
            if (testCompMatch(t, list, node, t.mode, t.mode_uri) == 1)
            {
                ret[0] = list.templ;
                priority[0] = list.priority;
                break;
            }
            list = list.next;
        }
        return list;
    }

    /** Port of xsltGetTemplate: the template for the node, with style the next import in scope. */
    static Template getTemplate(TransformCtx t, XmlNode node, Stylesheet style)
    {
        if (t == null || node == null) return null;
        Stylesheet curstyle = style != null ? nextImport(style) : t.style;
        while (curstyle != null && curstyle != style)
        {
            float[] priority = { PAT_NO_PRIORITY };
            Template[] ret = { null };
            String name = null;
            int k = nodeKind(node);
            if (curstyle.templates_hash != null)
            {
                // Use the top name as selector.
                if (k == ELEMENT || k == ATTRIBUTE || k == PI) name = nodeName(node);
            }
            if (name != null)
            {
                // The list of applicable expressions based on the name.
                String key = hash3Key(name, t.mode, t.mode_uri);
                Pattern list = curstyle.templates_hash.get(key);
                while (list != null)
                {
                    if (testCompMatch(t, list, node, t.mode, t.mode_uri) == 1)
                    {
                        ret[0] = list.templ;
                        priority[0] = list.priority;
                        break;
                    }
                    list = list.next;
                }
            }

            // Alternate generic matches.
            Pattern list = null;
            switch (k)
            {
            case ELEMENT: list = curstyle.elem_match; break;
            case ATTRIBUTE: list = curstyle.attr_match; break;
            case PI: list = curstyle.pi_match; break;
            case DOCUMENT: list = curstyle.root_match; break;
            case TEXT:
            case CDATA: list = curstyle.text_match; break;
            case COMMENT: list = curstyle.comment_match; break;
            default: break;
            }
            firstMatch(t, list, node, ret, priority);
            // Some of the tests for elements can also apply to documents.
            if (k == DOCUMENT || k == TEXT || k == PI || k == COMMENT)
            {
                firstMatch(t, curstyle.elem_match, node, ret, priority);
            }
            // keyed_match:
            while (true)
            {
                if (Transform.nodeHasKey(t, node))
                {
                    firstMatch(t, curstyle.key_match, node, ret, priority);
                }
                else if (t.has_templ_key_patterns != 0 && (t.document == null || t.document.nb_keys_computed < t.nb_keys))
                {
                    // Compute all remaining keys for this document.
                    if (Transform.computeAllKeys(t, node) == -1) return null;
                    if (Transform.nodeHasKey(t, node)) continue; // goto keyed_match
                }
                break;
            }
            if (ret[0] != null) return ret[0];
            // Cycle on the next stylesheet import.
            curstyle = nextImport(curstyle);
        }
        return null;
    }

    // Compilation helpers ////////////////////////////////////////////

    static boolean isXsltElem(XmlNode n)
    {
        if (n == null || nodeKind(n) != ELEMENT) return false;
        XmlNs ns = nodeNs(n);
        return ns != null && streq(nsHref(ns), XSLT_NAMESPACE);
    }

    static boolean isXsltName(XmlNode n, String name)
    {
        return streq(nodeName(n), name);
    }

    static boolean isBlankStr(String s)
    {
        if (s == null) return true;
        for (int i = 0; i < s.length(); i++) if (!isBlank(s.charAt(i))) return false;
        return true;
    }

    private static boolean isBlankNode(XmlNode n)
    {
        return nodeKind(n) == TEXT && isBlankStr(nodeContent(n));
    }

    private static String attrValue(XmlAttr a)
    {
        return nodeStringValue(attrNode(a));
    }

    /** libxml2 xmlGetNsProp(node, name, ns): the attribute in exactly that namespace (none if ns is null). */
    static String getNsPropExact(XmlNode node, String name, String ns)
    {
        if (node == null || nodeKind(node) != ELEMENT) return null;
        for (XmlAttr a : nodeAttrs(node))
        {
            if (!attrName(a).equals(name)) continue;
            String u = nodeNsHref(attrNode(a));
            if (u != null && u.isEmpty()) u = null;
            if (streq(u, ns)) return attrValue(a);
        }
        return null;
    }

    /** libxml2 xmlGetProp: the first attribute with this name, in any namespace. */
    private static String getProp(XmlNode node, String name)
    {
        if (node == null || nodeKind(node) != ELEMENT) return null;
        for (XmlAttr a : nodeAttrs(node))
        {
            if (attrName(a).equals(name)) return attrValue(a);
        }
        return null;
    }

    /** Port of xsltGetNsProp (also xsltGetCNsProp): an attribute in the namespace, or without
        namespace when the element is in that namespace. ns null is xmlGetProp. */
    static String getNsProp(XmlNode node, String name, String ns)
    {
        if (node == null) return null;
        if (ns == null) return getProp(node, name);
        if (nodeKind(node) != ELEMENT) return null;
        String nuri = nodeNsHref(node);
        for (XmlAttr a : nodeAttrs(node))
        {
            if (!attrName(a).equals(name)) continue;
            XmlNs an = nodeNs(attrNode(a));
            if ((an == null && nuri != null && nuri.equals(ns)) || (an != null && streq(nsHref(an), ns)))
            {
                String v = attrValue(a);
                return v != null ? v : "";
            }
        }
        return null;
    }

    /** libxml2 xmlNodeGetSpacePreserve: 1 for preserve, 0 for default, -1 if no xml:space applies. */
    private static int getSpacePreserve(XmlNode node)
    {
        for (XmlNode n = node; n != null; n = nodeParent(n))
        {
            if (nodeKind(n) != ELEMENT) continue;
            String v = getNsPropExact(n, "space", XML_NAMESPACE);
            if (v != null)
            {
                int r = -1;
                if (v.equals("preserve")) r = 1;
                else if (v.equals("default")) r = 0;
                if (r >= 0) return r;
            }
        }
        return -1;
    }

    private static boolean nameStartChar(int c)
    {
        return XmlChars.isLetter(c) || c == '_';
    }

    private static boolean nameChar(int c)
    {
        return XmlChars.isLetter(c) || XmlChars.isDigit(c) || c == '.' || c == '-' || c == '_' ||
               XmlChars.isCombining(c) || XmlChars.isExtender(c);
    }

    /** libxml2 xmlValidateNCName(value, 0): 0 if valid. */
    private static int validateNcname(String value)
    {
        if (value == null) return -1;
        int cur = 0;
        int[] len = new int[1];
        int c = utf8Char(value, cur, len);
        if (c < 0 || !nameStartChar(c)) return 1;
        cur += len[0];
        c = utf8Char(value, cur, len);
        while (c > 0 && nameChar(c))
        {
            cur += len[0];
            c = utf8Char(value, cur, len);
        }
        return c == 0 ? 0 : 1;
    }

    /** libxml2 xmlValidateQName(value, 0): 0 if valid. */
    static int validateQname(String value)
    {
        if (value == null) return -1;
        int cur = 0;
        int[] len = new int[1];
        int c = utf8Char(value, cur, len);
        if (c < 0 || !nameStartChar(c)) return 1;
        cur += len[0];
        c = utf8Char(value, cur, len);
        while (c > 0 && nameChar(c))
        {
            cur += len[0];
            c = utf8Char(value, cur, len);
        }
        if (c == ':')
        {
            cur += len[0];
            c = utf8Char(value, cur, len);
            if (c < 0 || !nameStartChar(c)) return 1;
            cur += len[0];
            c = utf8Char(value, cur, len);
            while (c > 0 && nameChar(c))
            {
                cur += len[0];
                c = utf8Char(value, cur, len);
            }
        }
        return c == 0 ? 0 : 1;
    }

    /** Port of xsltSplitQName: the local name, prefix[0] gets the prefix (null if none). */
    static String splitQname(String name, String[] prefix)
    {
        prefix[0] = null;
        if (name == null) return null;
        if (at(name, 0) == ':') return name;
        int len = name.indexOf(':');
        if (len < 0) return name;
        prefix[0] = name.substring(0, len);
        return name.substring(len + 1);
    }

    /** Port of xsltGetQNameURI2: like xsltGetQNameURI but reported on the stylesheet. */
    private static String getQnameUri2(Stylesheet style, XmlNode node, String[] name)
    {
        String qname = name[0];
        if (qname == null || qname.isEmpty()) return null;
        if (node == null)
        {
            genericError(style.settings, CFormat.format("QName: no element for namespace lookup %s\n", qname));
            name[0] = null;
            return null;
        }
        int len = qname.indexOf(':');
        if (len < 0) return null;
        if (qname.startsWith("xml:"))
        {
            if (qname.length() == 4) return null;
            name[0] = qname.substring(4);
            return XML_NAMESPACE;
        }
        String prefix = qname.substring(0, len);
        XmlNs ns = nsLookupPrefix(node, prefix);
        if (ns == null)
        {
            if (style != null)
            {
                transformError(null, style, node, CFormat.format("No namespace bound to prefix '%s'.\n", prefix));
                style.errors++;
            }
            else
            {
                genericError(null, CFormat.format("%s : no namespace bound to prefix %s\n", qname, prefix));
            }
            name[0] = null;
            return null;
        }
        name[0] = qname.substring(len + 1);
        return nsHref(ns);
    }

    // URIs, resolved with XmlUri (libxml2 uri.c).

    /** Port of xmlNodeGetBase: xml:base attributes and the document url, null on errors. */
    static String nodeGetBase(XmlDocument doc, XmlNode cur)
    {
        String ret = null;
        for (XmlNode n = cur; n != null; n = nodeParent(n))
        {
            if (nodeKind(n) != ELEMENT) continue;
            String base = getNsPropExact(n, "base", XML_NAMESPACE);
            if (base == null) continue;
            if (ret != null)
            {
                int[] res = { 0 };
                String newbase = XmlUri.buildSafe(ret, base, res);
                if (res[0] != 0) return null;
                ret = newbase;
            }
            else
            {
                ret = base;
            }
            if (ret.startsWith("http://") || ret.startsWith("ftp://") || ret.startsWith("urn:")) return ret;
        }
        String url = doc != null ? domUrl(doc) : null;
        if (url != null)
        {
            if (ret == null) return url;
            int[] res = { 0 };
            String newbase = XmlUri.buildSafe(ret, url, res);
            if (res[0] != 0) return null;
            ret = newbase;
        }
        return ret;
    }

    // The default loader.

    /** The strerror text of a failed file read. */
    private static String ioReason(IOException e)
    {
        if (e instanceof NoSuchFileException) return "No such file or directory";
        if (e instanceof AccessDeniedException) return "Permission denied";
        if (e instanceof FileSystemException fe && fe.getReason() != null) return fe.getReason();
        return "Input/output error";
    }

    /**
     * The default loader (xml_libxslt1_default_loader): read the file (a file: url or a path)
     * and parse it as xml, with entities expanded like libxslt XSLT_PARSE_OPTIONS.
     * @param s The settings, where the messages go.
     * @param uri The uri.
     * @param type What it is loaded for, a Settings.LOAD_* value.
     * @return The document, null if it cannot be loaded.
     */
    public static XmlDocument defaultLoader(Settings s, String uri, int type)
    {
        if (uri == null) return null;
        String path = uri;
        if (path.startsWith("file://"))
        {
            path = path.substring(7);
            if (path.startsWith("localhost/")) path = path.substring(9);
        }
        byte[] buf;
        try
        {
            if (path.isEmpty()) throw new NoSuchFileException(path);
            Path p = Path.of(path);
            if (Files.isDirectory(p))
            {
                // Like the libxml2 parser: an error while reading is reported as a parse error at line 1.
                emit(s, CFormat.format("%s:1: I/O error : %s\n\n^\n", uri, "Is a directory"));
                return null;
            }
            buf = Files.readAllBytes(p);
        }
        catch (IOException | RuntimeException e)
        {
            String reason = e instanceof IOException ie ? ioReason(ie) : "No such file or directory";
            emit(s, CFormat.format("I/O warning : failed to load \"%s\": %s\n", uri, reason));
            return null;
        }
        // XML_PARSE_NOCDATA and XML_PARSE_DTDATTR, part of XSLT_PARSE_OPTIONS.
        int options = XmlParser.NOCDATA | XmlParser.DTDATTR;
        XmlDocument dom;
        try
        {
            dom = XmlParser.parse(buf, uri, options, m -> emit(s, m));
        }
        catch (XmlParseException e)
        {
            emit(s, new String(e.bytes, StandardCharsets.UTF_8));
            return null;
        }
        // XML_PARSE_NOENT: entities are substituted.
        try
        {
            XmlEntityExpander.expand(dom, null, buf.length, options);
        }
        catch (XmlParseException e)
        {
            emit(s, new String(e.bytes, StandardCharsets.UTF_8));
            dom = null;
        }
        if (dom != null) domSetUrl(dom, uri);
        return dom;
    }

    static XmlDocument load(Stylesheet style, String uri, int type)
    {
        if (style.settings.loader != null) return style.settings.loader.load(uri, type);
        return defaultLoader(style.settings, uri, type);
    }

    // Precomputation of instructions, port of libxslt preproc.c ///////

    /** Port of xsltCheckTopLevelElement. */
    private static int checkTopLevelElement(Stylesheet style, XmlNode inst, int err)
    {
        if (style == null || inst == null || nodeNs(inst) == null) return -1;
        XmlNode parent = nodeParent(inst);
        if (parent == null)
        {
            if (err != 0)
            {
                transformError(null, style, inst, "internal problem: element has no parent\n");
                style.errors++;
            }
            return 0;
        }
        if (nodeKind(parent) != ELEMENT || nodeNs(parent) == null ||
            (nodeNs(parent) != nodeNs(inst) && !streq(nodeNsHref(parent), nodeNsHref(inst))) ||
            (!streq(nodeName(parent), "stylesheet") && !streq(nodeName(parent), "transform")))
        {
            if (err != 0)
            {
                transformError(null, style, inst, CFormat.format("element %s only allowed as child of stylesheet\n", nodeName(inst)));
                style.errors++;
            }
            return 0;
        }
        return 1;
    }

    private static boolean sameNs(XmlNode a, XmlNode b)
    {
        XmlNs na = nodeNs(a), nb = nodeNs(b);
        return na == nb || (na != null && nb != null && streq(nsHref(na), nsHref(nb)));
    }

    /** Port of xsltCheckInstructionElement. */
    private static void checkInstructionElement(Stylesheet style, XmlNode inst)
    {
        if (style == null || inst == null || nodeNs(inst) == null || style.literal_result != 0) return;
        XmlNode parent = nodeParent(inst);
        if (parent == null)
        {
            transformError(null, style, inst, "internal problem: element has no parent\n");
            style.errors++;
            return;
        }
        while (parent != null && nodeKind(parent) != DOCUMENT)
        {
            if (nodeKind(parent) == ELEMENT && nodeNs(parent) != null && sameNs(parent, inst) &&
                (streq(nodeName(parent), "template") || streq(nodeName(parent), "param") ||
                 streq(nodeName(parent), "attribute") || streq(nodeName(parent), "variable")))
            {
                return;
            }
            // No extension module has stylesheet data (extInfos), so there are no extension elements to skip.
            parent = nodeParent(parent);
        }
        transformError(null, style, inst, CFormat.format("element %s only allowed within a template, variable or param\n", nodeName(inst)));
        style.errors++;
    }

    /** Port of xsltCheckParentElement. */
    private static void checkParentElement(Stylesheet style, XmlNode inst, String allow1, String allow2)
    {
        if (style == null || inst == null || nodeNs(inst) == null || style.literal_result != 0) return;
        XmlNode parent = nodeParent(inst);
        if (parent == null)
        {
            transformError(null, style, inst, "internal problem: element has no parent\n");
            style.errors++;
            return;
        }
        if (nodeKind(parent) == ELEMENT && sameNs(parent, inst) &&
            (streq(nodeName(parent), allow1) || streq(nodeName(parent), allow2)))
        {
            return;
        }
        transformError(null, style, inst, CFormat.format("element %s is not allowed within that context\n", nodeName(inst)));
        style.errors++;
    }

    /** Port of xsltNewStylePreComp. */
    private static StylePreComp newStylePreComp(Stylesheet style, int type)
    {
        if (style == null) return null;
        StylePreComp cur = new StylePreComp();
        cur.type = type;
        cur.next = style.pre_comps;
        style.pre_comps = cur;
        return cur;
    }

    /** Port of xsltEvalStaticAttrValueTemplate: the value, unless it is an AVT (then null with
        found set). found[0] is left unchanged when nothing is looked up. */
    private static String evalStaticAvt(Stylesheet style, XmlNode inst, String name, String ns, int[] found)
    {
        if (style == null || inst == null || name == null || nodeKind(inst) != ELEMENT) return null;
        String expr = getNsProp(inst, name, ns);
        if (expr == null)
        {
            found[0] = 0;
            return null;
        }
        found[0] = 1;
        if (expr.indexOf('{') >= 0)
        {
            return null;
        }
        return expr;
    }

    private static XPath xpathCompileInst(Stylesheet style, XmlNode inst, String str, boolean novar)
    {
        return xpathCompile(style.settings, inst, str, novar);
    }

    /** Port of xsltDocumentComp, for xsl:document (XSLT 1.1 draft), saxon:output, xalan:write and xt:document. */
    private static StylePreComp documentComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_DOCUMENT);
        if (comp == null) return null;
        comp.ext = StylePreComp.EXT_DOCUMENT;
        comp.inst = inst;
        comp.ver11 = 0;
        String filename = null;
        String name = nodeName(inst);
        int[] f = { comp.has_filename };
        if (streq(name, "output"))
        {
            filename = evalStaticAvt(style, inst, "file", null, f);
        }
        else if (streq(name, "write"))
        {
            // The filename needs to be interpreted.
        }
        else if (streq(name, "document"))
        {
            if (nodeNs(inst) != null && streq(nodeNsHref(inst), XSLT_NAMESPACE)) comp.ver11 = 1;
            filename = evalStaticAvt(style, inst, "href", null, f);
        }
        comp.has_filename = f[0];
        if (comp.has_filename == 0)
        {
            return comp;
        }
        comp.filename = filename;
        return comp;
    }

    /** Port of xsltSortComp. */
    private static void sortComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_SORT);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] f = new int[1];

        f[0] = comp.has_stype;
        comp.stype = evalStaticAvt(style, inst, "data-type", null, f);
        comp.has_stype = f[0];
        if (comp.stype != null)
        {
            if (comp.stype.equals("text")) comp.number = 0;
            else if (comp.stype.equals("number")) comp.number = 1;
            else
            {
                transformError(null, style, inst, CFormat.format("xsltSortComp: no support for data-type = %s\n", comp.stype));
                comp.number = 0;
                style.warnings++;
            }
        }
        f[0] = comp.has_order;
        comp.order = evalStaticAvt(style, inst, "order", null, f);
        comp.has_order = f[0];
        if (comp.order != null)
        {
            if (comp.order.equals("ascending")) comp.descending = 0;
            else if (comp.order.equals("descending")) comp.descending = 1;
            else
            {
                transformError(null, style, inst, CFormat.format("xsltSortComp: invalid value %s for order\n", comp.order));
                comp.descending = 0;
                style.warnings++;
            }
        }
        f[0] = comp.has_use;
        comp.case_order = evalStaticAvt(style, inst, "case-order", null, f);
        comp.has_use = f[0];
        if (comp.case_order != null)
        {
            if (comp.case_order.equals("upper-first")) comp.lower_first = 0;
            else if (comp.case_order.equals("lower-first")) comp.lower_first = 1;
            else
            {
                // Like libxslt, the message shows the order.
                transformError(null, style, inst, CFormat.format("xsltSortComp: invalid value %s for order\n", comp.order != null ? comp.order : "(null)"));
                comp.lower_first = 0;
                style.warnings++;
            }
        }
        f[0] = comp.has_lang;
        comp.lang = evalStaticAvt(style, inst, "lang", null, f);
        comp.has_lang = f[0];

        comp.select = getNsProp(inst, "select", XSLT_NAMESPACE);
        // The default is ., the string-value of the current node is the sort key.
        if (comp.select == null) comp.select = ".";
        comp.comp = xpathCompileInst(style, inst, comp.select, false);
        if (comp.comp == null)
        {
            transformError(null, style, inst, CFormat.format("xsltSortComp: could not compile select expression '%s'\n", comp.select));
            style.errors++;
        }
        if (nodeFirstChild(inst) != null)
        {
            transformError(null, style, inst, "xsl:sort : is not empty\n");
            style.errors++;
        }
    }

    /** Port of xsltCopyComp. */
    private static void copyComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_COPY);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        comp.use = getNsProp(inst, "use-attribute-sets", XSLT_NAMESPACE);
        comp.has_use = comp.use != null ? 1 : 0;
    }

    /** Port of xsltTextComp. */
    private static void textComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_TEXT);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        comp.noescape = 0;
        String prop = getNsProp(inst, "disable-output-escaping", XSLT_NAMESPACE);
        if (prop != null)
        {
            if (prop.equals("yes")) comp.noescape = 1;
            else if (!prop.equals("no"))
            {
                transformError(null, style, inst, "xsl:text: disable-output-escaping allows only yes or no\n");
                style.warnings++;
            }
        }
    }

    /** strncasecmp(s, "xml", 3) == 0 (ASCII case folding like the C locale). */
    private static boolean startsWithXmlIgnoreCase(String s)
    {
        if (s.length() < 3) return false;
        return (s.charAt(0) | 0x20) == 'x' && (s.charAt(1) | 0x20) == 'm' && (s.charAt(2) | 0x20) == 'l' &&
               s.charAt(0) < 0x80 && s.charAt(1) < 0x80 && s.charAt(2) < 0x80;
    }

    /** Port of xsltElementComp. */
    private static void elementComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_ELEMENT);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] f = new int[1];

        f[0] = comp.has_name;
        comp.name = evalStaticAvt(style, inst, "name", null, f);
        comp.has_name = f[0];
        if (comp.has_name == 0)
        {
            transformError(null, style, inst, "xsl:element: The attribute 'name' is missing.\n");
            style.errors++;
            return;
        }
        f[0] = comp.has_ns;
        comp.ns = evalStaticAvt(style, inst, "namespace", null, f);
        comp.has_ns = f[0];
        if (comp.name != null)
        {
            if (validateQname(comp.name) != 0)
            {
                transformError(null, style, inst, CFormat.format("xsl:element: The value '%s' of the attribute 'name' is not a valid QName.\n", comp.name));
                style.errors++;
            }
            else
            {
                String[] prefix = { null };
                splitQname(comp.name, prefix);
                if (comp.has_ns == 0)
                {
                    // The QName is expanded with the namespace declarations in effect, including the default one.
                    XmlNs ns = nsLookupPrefix(inst, prefix[0]);
                    if (ns != null)
                    {
                        comp.ns = nsHref(ns);
                        comp.has_ns = 1;
                    }
                    else if (prefix[0] != null)
                    {
                        transformError(null, style, inst, CFormat.format(
                            "xsl:element: The prefixed QName '%s' has no namespace binding in scope in the stylesheet; " +
                            "this is an error, since the namespace was not specified by the instruction itself.\n", comp.name));
                        style.errors++;
                    }
                }
                if (prefix[0] != null && startsWithXmlIgnoreCase(prefix[0]))
                {
                    // Mark it to be skipped.
                    comp.has_name = 0;
                }
            }
        }
        f[0] = comp.has_use;
        comp.use = evalStaticAvt(style, inst, "use-attribute-sets", null, f);
        comp.has_use = f[0];
    }

    /** Port of xsltAttributeComp. */
    private static void attributeComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_ATTRIBUTE);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] f = new int[1];

        f[0] = comp.has_name;
        comp.name = evalStaticAvt(style, inst, "name", null, f);
        comp.has_name = f[0];
        if (comp.has_name == 0)
        {
            transformError(null, style, inst, "XSLT-attribute: The attribute 'name' is missing.\n");
            style.errors++;
            return;
        }
        f[0] = comp.has_ns;
        comp.ns = evalStaticAvt(style, inst, "namespace", null, f);
        comp.has_ns = f[0];
        if (comp.name != null)
        {
            if (validateQname(comp.name) != 0)
            {
                transformError(null, style, inst, CFormat.format("xsl:attribute: The value '%s' of the attribute 'name' is not a valid QName.\n", comp.name));
                style.errors++;
            }
            else if (comp.name.equals("xmlns"))
            {
                transformError(null, style, inst, "xsl:attribute: The attribute name 'xmlns' is not allowed.\n");
                style.errors++;
            }
            else
            {
                String[] prefix = { null };
                splitQname(comp.name, prefix);
                if (prefix[0] != null && comp.has_ns == 0)
                {
                    XmlNs ns = nsLookupPrefix(inst, prefix[0]);
                    if (ns != null)
                    {
                        comp.ns = nsHref(ns);
                        comp.has_ns = 1;
                    }
                    else
                    {
                        transformError(null, style, inst, CFormat.format(
                            "xsl:attribute: The prefixed QName '%s' has no namespace binding in scope in the stylesheet; " +
                            "this is an error, since the namespace was not specified by the instruction itself.\n", comp.name));
                        style.errors++;
                    }
                }
            }
        }
    }

    private static void simpleComp(Stylesheet style, XmlNode inst, int type)
    {
        StylePreComp comp = newStylePreComp(style, type);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
    }

    /** Port of xsltProcessingInstructionComp. */
    private static void piComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_PI);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] f = { comp.has_name };
        comp.name = evalStaticAvt(style, inst, "name", XSLT_NAMESPACE, f);
        comp.has_name = f[0];
    }

    /** Port of xsltCopyOfComp and xsltValueOfComp. */
    private static void selectComp(Stylesheet style, XmlNode inst, int type)
    {
        StylePreComp comp = newStylePreComp(style, type);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        String what = type == StylePreComp.FUNC_COPYOF ? "xsl:copy-of" : "xsl:value-of";
        if (type == StylePreComp.FUNC_VALUEOF)
        {
            String prop = getNsProp(inst, "disable-output-escaping", XSLT_NAMESPACE);
            if (prop != null)
            {
                if (prop.equals("yes")) comp.noescape = 1;
                else if (!prop.equals("no"))
                {
                    transformError(null, style, inst, "xsl:value-of : disable-output-escaping allows only yes or no\n");
                    style.warnings++;
                }
            }
        }
        comp.select = getNsProp(inst, "select", XSLT_NAMESPACE);
        if (comp.select == null)
        {
            transformError(null, style, inst, CFormat.format("%s : select is missing\n", what));
            style.errors++;
            return;
        }
        comp.comp = xpathCompileInst(style, inst, comp.select, false);
        if (comp.comp == null)
        {
            transformError(null, style, inst, CFormat.format("%s : could not compile select expression '%s'\n", what, comp.select));
            style.errors++;
        }
    }

    /** Port of xsltGetQNameProperty. The outputs (C pointers) are one element arrays or null. */
    private static void getQnameProperty(Stylesheet style, XmlNode inst, String prop_name, int mandatory,
                                         int[] has_prop, String[] ns_name, String[] local_name)
    {
        if (ns_name != null) ns_name[0] = null;
        if (local_name != null) local_name[0] = null;
        if (has_prop != null) has_prop[0] = 0;
        String prop = getNsProp(inst, prop_name, XSLT_NAMESPACE);
        if (prop == null)
        {
            if (mandatory != 0)
            {
                transformError(null, style, inst, CFormat.format("The attribute '%s' is missing.\n", prop_name));
                style.errors++;
            }
            return;
        }
        if (validateQname(prop) != 0)
        {
            transformError(null, style, inst, CFormat.format("The value '%s' of the attribute '%s' is not a valid QName.\n", prop, prop_name));
            style.errors++;
            return;
        }
        String[] p = { prop };
        String uri = getQnameUri2(style, inst, p);
        prop = p[0];
        if (prop == null)
        {
            style.errors++;
            return;
        }
        if (local_name != null) local_name[0] = prop;
        if (has_prop != null) has_prop[0] = 1;
        if (uri != null && ns_name != null) ns_name[0] = uri;
    }

    /** Port of xsltWithParamComp, xsltVariableComp and xsltParamComp. */
    private static void variableLikeComp(Stylesheet style, XmlNode inst, int type)
    {
        StylePreComp comp = newStylePreComp(style, type);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] has = new int[1];
        String[] ns = new String[1];
        String[] name = new String[1];
        getQnameProperty(style, inst, "name", 1, has, ns, name);
        comp.has_name = has[0];
        comp.ns = ns[0];
        comp.name = name[0];
        if (comp.ns != null) comp.has_ns = 1;
        comp.select = getNsProp(inst, "select", XSLT_NAMESPACE);
        if (comp.select == null) return;
        comp.comp = xpathCompileInst(style, inst, comp.select, false);
        if (type == StylePreComp.FUNC_WITHPARAM)
        {
            if (comp.comp == null)
            {
                transformError(null, style, inst, CFormat.format("XSLT-with-param: Failed to compile select expression '%s'\n", comp.select));
                style.errors++;
            }
            if (nodeFirstChild(inst) != null)
            {
                transformError(null, style, inst, "XSLT-with-param: The content should be empty since the attribute select is present.\n");
                style.warnings++;
            }
        }
        else if (type == StylePreComp.FUNC_VARIABLE)
        {
            if (comp.comp == null)
            {
                transformError(null, style, inst, CFormat.format("XSLT-variable: Failed to compile the XPath expression '%s'.\n", comp.select));
                style.errors++;
            }
            for (XmlNode cur = nodeFirstChild(inst); cur != null; cur = nodeNext(cur))
            {
                int k = nodeKind(cur);
                if (k != COMMENT && (k != TEXT || !isBlankStr(nodeContent(cur))))
                {
                    transformError(null, style, inst, "XSLT-variable: There must be no child nodes, since the attribute 'select' was specified.\n");
                    style.errors++;
                }
            }
        }
        else
        {
            if (comp.comp == null)
            {
                transformError(null, style, inst, CFormat.format("XSLT-param: could not compile select expression '%s'.\n", comp.select));
                style.errors++;
            }
            if (nodeFirstChild(inst) != null)
            {
                transformError(null, style, inst, "XSLT-param: The content should be empty since the attribute 'select' is present.\n");
                style.warnings++;
            }
        }
    }

    /** sscanf(s, "%d", &value): the value, unchanged if there is no number (glibc: strtol, truncated to int). */
    private static int scanInt(String s, int value)
    {
        int i = 0;
        int n = s.length();
        while (i < n && (s.charAt(i) == ' ' || (s.charAt(i) >= 0x09 && s.charAt(i) <= 0x0D))) i++;
        boolean neg = false;
        if (i < n && (s.charAt(i) == '+' || s.charAt(i) == '-'))
        {
            neg = s.charAt(i) == '-';
            i++;
        }
        if (i >= n || s.charAt(i) < '0' || s.charAt(i) > '9') return value;
        long v = 0;
        boolean overflow = false;
        while (i < n && s.charAt(i) >= '0' && s.charAt(i) <= '9')
        {
            int d = s.charAt(i) - '0';
            if (!overflow)
            {
                if (v > (Long.MAX_VALUE - d) / 10) overflow = true;
                else v = v * 10 + d;
            }
            i++;
        }
        if (overflow) v = neg ? Long.MIN_VALUE : Long.MAX_VALUE;
        else if (neg) v = -v;
        return (int)v;
    }

    /** Port of xsltNumberComp. */
    private static void numberComp(Stylesheet style, XmlNode cur)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_NUMBER);
        nodeSetUserData(cur, comp);

        comp.numdata.doc = nodeDom(cur);
        comp.numdata.node = cur;
        comp.numdata.value = getNsProp(cur, "value", XSLT_NAMESPACE);

        int[] f = { comp.numdata.has_format };
        String prop = evalStaticAvt(style, cur, "format", XSLT_NAMESPACE, f);
        comp.numdata.has_format = f[0];
        if (comp.numdata.has_format == 0)
        {
            comp.numdata.format = "";
        }
        else
        {
            comp.numdata.format = prop;
        }

        comp.numdata.count = getNsProp(cur, "count", XSLT_NAMESPACE);
        comp.numdata.from = getNsProp(cur, "from", XSLT_NAMESPACE);

        if (comp.numdata.count != null) comp.numdata.count_pat = compilePatternInternal(comp.numdata.count, cur, style, null, 0);
        if (comp.numdata.from != null) comp.numdata.from_pat = compilePatternInternal(comp.numdata.from, cur, style, null, 0);

        prop = getNsProp(cur, "level", XSLT_NAMESPACE);
        if (prop != null)
        {
            if (prop.equals("single") || prop.equals("multiple") || prop.equals("any"))
            {
                comp.numdata.level = prop;
            }
            else
            {
                transformError(null, style, cur, CFormat.format("xsl:number : invalid value %s for level\n", prop));
                style.warnings++;
            }
        }

        prop = getNsProp(cur, "lang", XSLT_NAMESPACE);
        if (prop != null)
        {
            transformError(null, style, cur, "xsl:number : lang attribute not implemented\n");
            // XSLT_TODO, reported with the position in libxslt.
            genericError(style.settings, "Unimplemented block at preproc.c:1459\n");
        }

        prop = getNsProp(cur, "letter-value", XSLT_NAMESPACE);
        if (prop != null)
        {
            if (prop.equals("alphabetic"))
            {
                transformError(null, style, cur, "xsl:number : letter-value 'alphabetic' not implemented\n");
                style.warnings++;
                genericError(style.settings, "Unimplemented block at preproc.c:1468\n");
            }
            else if (prop.equals("traditional"))
            {
                transformError(null, style, cur, "xsl:number : letter-value 'traditional' not implemented\n");
                style.warnings++;
                genericError(style.settings, "Unimplemented block at preproc.c:1473\n");
            }
            else
            {
                transformError(null, style, cur, CFormat.format("xsl:number : invalid value %s for letter-value\n", prop));
                style.warnings++;
            }
        }

        prop = getNsProp(cur, "grouping-separator", XSLT_NAMESPACE);
        if (prop != null)
        {
            int[] len = { prop.length() };
            int c = utf8Char(prop, 0, len);
            comp.numdata.grouping_character_len = len[0];
            comp.numdata.grouping_character = c < 0 ? 0 : c;
        }

        prop = getNsProp(cur, "grouping-size", XSLT_NAMESPACE);
        if (prop != null)
        {
            comp.numdata.digits_per_group = scanInt(prop, comp.numdata.digits_per_group);
        }
        else
        {
            comp.numdata.grouping_character = 0;
        }

        // Default values.
        if (comp.numdata.value == null && comp.numdata.level == null)
        {
            comp.numdata.level = "single";
        }
    }

    /** Port of xsltApplyTemplatesComp. */
    private static void applyTemplatesComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_APPLYTEMPLATES);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        String[] mode_uri = new String[1];
        String[] mode = new String[1];
        getQnameProperty(style, inst, "mode", 0, null, mode_uri, mode);
        comp.mode_uri = mode_uri[0];
        comp.mode = mode[0];
        comp.select = getNsProp(inst, "select", XSLT_NAMESPACE);
        if (comp.select != null)
        {
            comp.comp = xpathCompileInst(style, inst, comp.select, false);
            if (comp.comp == null)
            {
                transformError(null, style, inst, CFormat.format("XSLT-apply-templates: could not compile select expression '%s'\n", comp.select));
                style.errors++;
            }
        }
    }

    /** Port of xsltCallTemplateComp. */
    private static void callTemplateComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_CALLTEMPLATE);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        int[] has = new int[1];
        String[] ns = new String[1];
        String[] name = new String[1];
        getQnameProperty(style, inst, "name", 1, has, ns, name);
        comp.has_name = has[0];
        comp.ns = ns[0];
        comp.name = name[0];
        if (comp.ns != null) comp.has_ns = 1;
    }

    /** Port of xsltIfComp and xsltWhenComp. */
    private static void testComp(Stylesheet style, XmlNode inst, int type)
    {
        StylePreComp comp = newStylePreComp(style, type);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        String what = type == StylePreComp.FUNC_IF ? "xsl:if" : "xsl:when";
        comp.test = getNsProp(inst, "test", XSLT_NAMESPACE);
        if (comp.test == null)
        {
            transformError(null, style, inst, CFormat.format("%s : test is not defined\n", what));
            style.errors++;
            return;
        }
        comp.comp = xpathCompileInst(style, inst, comp.test, false);
        if (comp.comp == null)
        {
            transformError(null, style, inst, CFormat.format("%s : could not compile test expression '%s'\n", what, comp.test));
            style.errors++;
        }
    }

    /** Port of xsltForEachComp. */
    private static void forEachComp(Stylesheet style, XmlNode inst)
    {
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_FOREACH);
        nodeSetUserData(inst, comp);
        comp.inst = inst;
        comp.select = getNsProp(inst, "select", XSLT_NAMESPACE);
        if (comp.select == null)
        {
            transformError(null, style, inst, "xsl:for-each : select is missing\n");
            style.errors++;
            return;
        }
        comp.comp = xpathCompileInst(style, inst, comp.select, false);
        if (comp.comp == null)
        {
            transformError(null, style, inst, CFormat.format("xsl:for-each : could not compile select expression '%s'\n", comp.select));
            style.errors++;
        }
    }

    /** The built in extension elements of libxslt (extra.c xsltRegisterAllExtras). */
    private static int lookupExtElement(String name, String uri)
    {
        if (name == null || uri == null) return StylePreComp.EXT_NONE;
        if (uri.equals(LIBXSLT_NAMESPACE) && name.equals("debug")) return StylePreComp.EXT_DEBUG;
        if (uri.equals(SAXON_NAMESPACE) && name.equals("output")) return StylePreComp.EXT_DOCUMENT;
        if (uri.equals(XALAN_NAMESPACE) && name.equals("write")) return StylePreComp.EXT_DOCUMENT;
        if (uri.equals(XT_NAMESPACE) && name.equals("document")) return StylePreComp.EXT_DOCUMENT;
        if (uri.equals(XSLT_NAMESPACE) && name.equals("document")) return StylePreComp.EXT_DOCUMENT;
        return StylePreComp.EXT_NONE;
    }

    /** Port of xsltPreComputeExtModuleElement. */
    private static StylePreComp preComputeExtModuleElement(Stylesheet style, XmlNode inst)
    {
        if (style == null || inst == null || nodeKind(inst) != ELEMENT || nodeNs(inst) == null) return null;
        int ext = lookupExtElement(nodeName(inst), nodeNsHref(inst));
        if (ext == StylePreComp.EXT_NONE) return null;
        if (ext == StylePreComp.EXT_DOCUMENT) return documentComp(style, inst);
        // Default creation (xsltNewElemPreComp).
        StylePreComp comp = newStylePreComp(style, StylePreComp.FUNC_EXTENSION);
        comp.ext = ext;
        comp.inst = inst;
        return comp;
    }

    /** Port of xsltStylePreCompute. */
    private static void stylePreCompute(Stylesheet style, XmlNode inst)
    {
        if (inst == null || nodeKind(inst) != ELEMENT || nodeUserData(inst) != null) return;

        if (isXsltElem(inst))
        {
            if (isXsltName(inst, "apply-templates"))
            {
                checkInstructionElement(style, inst);
                applyTemplatesComp(style, inst);
            }
            else if (isXsltName(inst, "with-param"))
            {
                checkParentElement(style, inst, "apply-templates", "call-template");
                variableLikeComp(style, inst, StylePreComp.FUNC_WITHPARAM);
            }
            else if (isXsltName(inst, "value-of"))
            {
                checkInstructionElement(style, inst);
                selectComp(style, inst, StylePreComp.FUNC_VALUEOF);
            }
            else if (isXsltName(inst, "copy"))
            {
                checkInstructionElement(style, inst);
                copyComp(style, inst);
            }
            else if (isXsltName(inst, "copy-of"))
            {
                checkInstructionElement(style, inst);
                selectComp(style, inst, StylePreComp.FUNC_COPYOF);
            }
            else if (isXsltName(inst, "if"))
            {
                checkInstructionElement(style, inst);
                testComp(style, inst, StylePreComp.FUNC_IF);
            }
            else if (isXsltName(inst, "when"))
            {
                checkParentElement(style, inst, "choose", null);
                testComp(style, inst, StylePreComp.FUNC_WHEN);
            }
            else if (isXsltName(inst, "choose"))
            {
                checkInstructionElement(style, inst);
                simpleComp(style, inst, StylePreComp.FUNC_CHOOSE);
            }
            else if (isXsltName(inst, "for-each"))
            {
                checkInstructionElement(style, inst);
                forEachComp(style, inst);
            }
            else if (isXsltName(inst, "apply-imports"))
            {
                checkInstructionElement(style, inst);
                simpleComp(style, inst, StylePreComp.FUNC_APPLYIMPORTS);
            }
            else if (isXsltName(inst, "attribute"))
            {
                XmlNode parent = nodeParent(inst);
                if (parent == null || nodeKind(parent) != ELEMENT || nodeNs(parent) == null ||
                    !sameNs(parent, inst) || !streq(nodeName(parent), "attribute-set"))
                {
                    checkInstructionElement(style, inst);
                }
                attributeComp(style, inst);
            }
            else if (isXsltName(inst, "element"))
            {
                checkInstructionElement(style, inst);
                elementComp(style, inst);
            }
            else if (isXsltName(inst, "text"))
            {
                checkInstructionElement(style, inst);
                textComp(style, inst);
            }
            else if (isXsltName(inst, "sort"))
            {
                checkParentElement(style, inst, "apply-templates", "for-each");
                sortComp(style, inst);
            }
            else if (isXsltName(inst, "comment"))
            {
                checkInstructionElement(style, inst);
                simpleComp(style, inst, StylePreComp.FUNC_COMMENT);
            }
            else if (isXsltName(inst, "number"))
            {
                checkInstructionElement(style, inst);
                numberComp(style, inst);
            }
            else if (isXsltName(inst, "processing-instruction"))
            {
                checkInstructionElement(style, inst);
                piComp(style, inst);
            }
            else if (isXsltName(inst, "call-template"))
            {
                checkInstructionElement(style, inst);
                callTemplateComp(style, inst);
            }
            else if (isXsltName(inst, "param"))
            {
                if (checkTopLevelElement(style, inst, 0) == 0) checkInstructionElement(style, inst);
                variableLikeComp(style, inst, StylePreComp.FUNC_PARAM);
            }
            else if (isXsltName(inst, "variable"))
            {
                if (checkTopLevelElement(style, inst, 0) == 0) checkInstructionElement(style, inst);
                variableLikeComp(style, inst, StylePreComp.FUNC_VARIABLE);
            }
            else if (isXsltName(inst, "otherwise"))
            {
                checkParentElement(style, inst, "choose", null);
                checkInstructionElement(style, inst);
                return;
            }
            else if (isXsltName(inst, "template") || isXsltName(inst, "output") ||
                     isXsltName(inst, "preserve-space") || isXsltName(inst, "strip-space"))
            {
                checkTopLevelElement(style, inst, 1);
                return;
            }
            else if (isXsltName(inst, "stylesheet") || isXsltName(inst, "transform"))
            {
                XmlNode parent = nodeParent(inst);
                if (parent == null || nodeKind(parent) != DOCUMENT)
                {
                    transformError(null, style, inst, CFormat.format("element %s only allowed only as root element\n", nodeName(inst)));
                    style.errors++;
                }
                return;
            }
            else if (isXsltName(inst, "key") || isXsltName(inst, "attribute-set") ||
                     isXsltName(inst, "namespace-alias") || isXsltName(inst, "include") ||
                     isXsltName(inst, "import") || isXsltName(inst, "decimal-format"))
            {
                checkTopLevelElement(style, inst, 1);
                return;
            }
            else if (isXsltName(inst, "message") || isXsltName(inst, "fallback"))
            {
                checkInstructionElement(style, inst);
                return;
            }
            else if (isXsltName(inst, "document"))
            {
                checkInstructionElement(style, inst);
                nodeSetUserData(inst, documentComp(style, inst));
            }
            else if (style == null || style.forwards_compatible == 0)
            {
                transformError(null, style, inst, CFormat.format("xsltStylePreCompute: unknown xsl:%s\n", nodeName(inst)));
                if (style != null) style.warnings++;
            }

            // A namespace list is built for every xslt instruction, for its xpath expressions.
            Object cur = nodeUserData(inst);
            if (cur != null && cur != StylePreComp.EXT_MARKER) ((StylePreComp)cur).ns_list = nsList(inst);
        }
        else
        {
            StylePreComp comp = preComputeExtModuleElement(style, inst);
            // An unknown element, maybe registered at the context level: mark it for later recognition.
            nodeSetUserData(inst, comp != null ? comp : StylePreComp.EXT_MARKER);
        }
    }

    // Attribute value templates, port of libxslt attrvt.c (the compilation).

    private static AttrVT newAttrVt(Stylesheet style)
    {
        AttrVT cur = new AttrVT();
        cur.next = style.att_vts;
        style.att_vts = cur;
        return cur;
    }

    private static void setAttrVtSegment(AttrVT avt, Object val)
    {
        avt.segments.add(val);
        avt.nb_seg++;
    }

    /** Like xmlStrncat: a is returned unchanged (even null) when there is nothing to add. n
        counts chars of b. */
    static String strncat(String a, String b, int n)
    {
        if (b == null || n == 0) return a;
        String add = b.substring(0, n);
        return a == null ? add : a + add;
    }

    /** Like xmlStrcat: null a gives a copy of b (also of ""). */
    static String strcat(String a, String b)
    {
        if (b == null) return a;
        if (a == null) return b;
        return strncat(a, b, b.length());
    }

    /** Port of xsltCompileAttr: compile the attribute value template of a literal result element attribute. */
    private static void compileAttr(Stylesheet style, XmlAttr attr)
    {
        XmlNode an = attrNode(attr);
        XmlNode first = nodeFirstChild(an);
        if (style == null || first == null) return;
        if (nodeKind(first) != TEXT || nodeNext(first) != null)
        {
            transformError(null, style, attrOwner(attr), CFormat.format(
                "Attribute '%s': The content is expected to be a single text node when compiling an AVT.\n", attrName(attr)));
            style.errors++;
            return;
        }
        String s = nodeContent(first);
        if (s == null || (s.indexOf('{') < 0 && s.indexOf('}') < 0)) return;
        if (nodeUserData(an) != null) return;

        AttrVT avt = newAttrVt(style);
        nodeSetUserData(an, avt);
        avt.ns_list = nsList(attrOwner(attr));

        String ret = null;
        String expr = null;
        int lastavt = 0;
        // str and cur are char indexes in s.
        int str = 0;
        int cur = str;
        while (at(s, cur) != 0)
        {
            if (at(s, cur) == '{')
            {
                if (at(s, cur + 1) == '{')
                {
                    // An escaped {.
                    cur++;
                    ret = strncat(ret, s.substring(str), cur - str);
                    cur++;
                    str = cur;
                    continue;
                }
                if (at(s, cur + 1) == '}')
                {
                    // Skip an empty AVT.
                    ret = strncat(ret, s.substring(str), cur - str);
                    cur += 2;
                    str = cur;
                    continue;
                }
                if (ret != null || cur - str > 0)
                {
                    ret = strncat(ret, s.substring(str), cur - str);
                    str = cur;
                    if (avt.nb_seg == 0) avt.strstart = 1;
                    setAttrVtSegment(avt, ret);
                    ret = null;
                    lastavt = 0;
                }
                cur++;
                while (at(s, cur) != 0 && at(s, cur) != '}')
                {
                    // Need to check for literals (bug539741).
                    if (at(s, cur) == '\'' || at(s, cur) == '"')
                    {
                        char delim = at(s, cur++);
                        while (at(s, cur) != 0 && at(s, cur) != delim) cur++;
                        if (at(s, cur) != 0) cur++;
                    }
                    else
                    {
                        cur++;
                    }
                }
                if (at(s, cur) == 0)
                {
                    transformError(null, style, attrOwner(attr), CFormat.format("Attribute '%s': The AVT has an unmatched '{'.\n", attrName(attr)));
                    style.errors++;
                    return;
                }
                str++;
                expr = s.substring(str, cur);
                XPath comp = xpathCompile(style.settings, attrOwner(attr), expr, false);
                if (comp == null)
                {
                    transformError(null, style, attrOwner(attr), CFormat.format("Attribute '%s': Failed to compile the expression '%s' in the AVT.\n",
                                                                               attrName(attr), expr));
                    style.errors++;
                    return;
                }
                if (avt.nb_seg == 0) avt.strstart = 0;
                if (lastavt == 1) setAttrVtSegment(avt, null);
                setAttrVtSegment(avt, comp);
                lastavt = 1;
                expr = null;
                cur++;
                str = cur;
            }
            else if (at(s, cur) == '}')
            {
                cur++;
                if (at(s, cur) == '}')
                {
                    // An escaped }.
                    ret = strncat(ret, s.substring(str), cur - str);
                    cur++;
                    str = cur;
                    continue;
                }
                transformError(null, style, attrOwner(attr), CFormat.format("Attribute '%s': The AVT has an unmatched '}'.\n", attrName(attr)));
                return;
            }
            else
            {
                cur++;
            }
        }
        if (ret != null || cur - str > 0)
        {
            ret = strncat(ret, s.substring(str), cur - str);
            if (avt.nb_seg == 0) avt.strstart = 1;
            setAttrVtSegment(avt, ret);
            ret = null;
        }
    }

    // Stylesheets, port of libxslt xslt.c ///////////////////////////

    /** Port of xsltParseContentError. */
    private static void parseContentError(Stylesheet style, XmlNode node)
    {
        if (style == null || node == null) return;
        if (isXsltElem(node))
            transformError(null, style, node, CFormat.format("The XSLT-element '%s' is not allowed at this position.\n", nodeName(node)));
        else
            transformError(null, style, node, CFormat.format("The element '%s' is not allowed at this position.\n",
                                                             libxml2NodeName(node) != null ? libxml2NodeName(node) : "(null)"));
        style.errors++;
    }

    /** Port of exclPrefixPush: returns the index, -1 for duplicates. */
    private static int exclPrefixPush(Stylesheet style, String orig)
    {
        for (int i = 0; i < style.excl_prefix_tab.size(); i++)
        {
            if (streq(style.excl_prefix_tab.get(i), orig)) return -1;
        }
        style.excl_prefix_tab.add(orig);
        style.excl_prefix = orig;
        return style.excl_prefix_tab.size() - 1;
    }

    /** Port of exclPrefixPop. */
    private static void exclPrefixPop(Stylesheet style)
    {
        int nr = style.excl_prefix_tab.size();
        if (nr <= 0) return;
        nr--;
        style.excl_prefix = nr > 0 ? style.excl_prefix_tab.get(nr - 1) : null;
        style.excl_prefix_tab.remove(nr);
    }

    /** Port of xsltNewDecimalFormat. */
    private static DecimalFormat newDecimalFormat(String ns_uri, String name)
    {
        DecimalFormat self = new DecimalFormat();
        self.ns_uri = ns_uri;
        self.name = name;
        self.digit = "#";
        self.pattern_separator = ";";
        self.decimal_point = ".";
        self.grouping = ",";
        self.percent = "%";
        self.permille = "‰";
        self.zero_digit = "0";
        self.minus_sign = "-";
        self.infinity = "Infinity";
        self.no_number = "NaN";
        return self;
    }

    /** Port of xsltDecimalFormatGetByQName. */
    static DecimalFormat decimalFormatGetByQname(Stylesheet style, String ns_uri, String name)
    {
        if (name == null) return style.decimal_format;
        while (style != null)
        {
            for (DecimalFormat r = style.decimal_format.next; r != null; r = r.next)
            {
                if (streq(ns_uri, r.ns_uri) && streq(name, r.name)) return r;
            }
            style = nextImport(style);
        }
        return null;
    }

    private static Template newTemplate()
    {
        Template cur = new Template();
        cur.priority = PAT_NO_PRIORITY;
        return cur;
    }

    /** Port of xsltNewStylesheetInternal. */
    private static Stylesheet newStylesheetInternal(Stylesheet parent, Settings settings)
    {
        Stylesheet ret = new Stylesheet();
        if (settings != null) ret.settings = settings;
        else if (parent != null) ret.settings = parent.settings;
        else ret.settings = new Settings();
        ret.parent = parent;
        ret.omit_xml_declaration = -1;
        ret.standalone = -1;
        ret.decimal_format = newDecimalFormat(null, null);
        ret.indent = -1;
        ret.principal = parent != null ? parent.principal : ret;
        return ret;
    }

    /**
     * An empty stylesheet whose document is style_dom (not owned), patterns can be compiled
     * with namespaces from its elements (xml_libxslt1_stylesheet_new_empty).
     * @param style_dom The document.
     * @param settings The settings, null for the defaults.
     * @return The stylesheet.
     */
    public static Stylesheet stylesheetNewEmpty(XmlDocument style_dom, Settings settings)
    {
        Stylesheet style = newStylesheetInternal(null, settings);
        style.doc = style_dom;
        return style;
    }

    /**
     * Free a stylesheet (xml_libxslt1_stylesheet_free), nothing to do in java.
     * @param style The stylesheet.
     */
    public static void stylesheetFree(Stylesheet style)
    {
    }

    /** Port of xsltGetInheritedNsList: the namespaces in scope of a template, for literal result elements. */
    private static int getInheritedNsList(Stylesheet style, Template templ, XmlNode node)
    {
        if (style == null || templ == null || node == null || templ.inherited_ns_nr != 0 || templ.inherited_ns != null) return 0;
        List<XmlNs> ret = new ArrayList<>();
        int nbns = 0;
        for (; node != null; node = nodeParent(node))
        {
            if (nodeKind(node) != ELEMENT) continue;
            for (XmlNs cur : nodeNsDecls(node))
            {
                if (streq(nsHref(cur), XSLT_NAMESPACE)) continue;
                if (nsPrefix(cur) != null && checkExtPrefix(style, nsPrefix(cur)) != 0) continue;
                // Namespaces excluded on the topmost stylesheet element.
                boolean skip = false;
                for (int i = 0; i < style.excl_prefix_tab.size(); i++)
                {
                    if (streq(nsHref(cur), style.excl_prefix_tab.get(i))) skip = true;
                }
                if (skip) continue;
                // Skip shadowed namespace bindings.
                int i;
                for (i = 0; i < nbns; i++)
                {
                    if (streq(nsPrefix(cur), nsPrefix(ret.get(i)))) break;
                }
                if (i >= nbns)
                {
                    ret.add(cur);
                    nbns++;
                }
            }
        }
        if (nbns != 0)
        {
            templ.inherited_ns_nr = nbns;
            templ.inherited_ns = ret.toArray(new XmlNs[0]);
        }
        return nbns;
    }

    static String key2(String a, String b)
    {
        return hash3Key(a, b, null);
    }

    /** Called for each token of a whitespace separated list. */
    private interface TokenFn
    {
        void token(Stylesheet style, XmlNode cur, String token, Object data);
    }

    /** Parse a whitespace separated list of tokens, calling f for each. */
    private static void forEachToken(Stylesheet style, XmlNode cur, String list, TokenFn f, Object data)
    {
        int element = 0;
        while (at(list, element) != 0)
        {
            while (isBlank(at(list, element))) element++;
            if (at(list, element) == 0) break;
            int end = element;
            while (at(list, end) != 0 && !isBlank(at(list, end))) end++;
            f.token(style, cur, list.substring(element, end), data);
            element = end;
        }
    }

    private static void cdataToken(Stylesheet style, XmlNode cur, String element, Object data)
    {
        if (validateQname(element) != 0)
        {
            transformError(null, style, cur, CFormat.format("Attribute 'cdata-section-elements': The value '%s' is not a valid QName.\n", element));
            style.errors++;
            return;
        }
        String[] el = { element };
        String uri = getQnameUri(style.settings, cur, el);
        element = el[0];
        if (element == null)
        {
            transformError(null, style, cur, "Attribute 'cdata-section-elements': Not a valid QName.\n");
            style.errors++;
            return;
        }
        // Unprefixed names use the default namespace (bug #339570).
        if (uri == null)
        {
            XmlNs ns = nsLookupPrefix(cur, null);
            if (ns != null) uri = nsHref(ns);
        }
        String key = key2(element, uri);
        style.cdata_section.putIfAbsent(key, "cdata");
    }

    /** Port of xsltParseStylesheetOutput. */
    private static void parseStylesheetOutput(Stylesheet style, XmlNode cur)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        String prop;
        if ((prop = getNsPropExact(cur, "version", null)) != null) style.version = prop;
        if ((prop = getNsPropExact(cur, "encoding", null)) != null) style.encoding = prop;
        if ((prop = getNsPropExact(cur, "method", null)) != null)
        {
            style.method = null;
            style.method_uri = null;
            String[] p = { prop };
            String uri = getQnameUri(style.settings, cur, p);
            prop = p[0];
            if (prop == null)
            {
                style.errors++;
            }
            else if (uri == null)
            {
                if (prop.equals("xml") || prop.equals("html") || prop.equals("text"))
                {
                    style.method = prop;
                }
                else
                {
                    transformError(null, style, cur, CFormat.format("invalid value for method: %s\n", prop));
                    style.warnings++;
                }
            }
            else
            {
                style.method = prop;
                style.method_uri = uri;
            }
        }
        if ((prop = getNsPropExact(cur, "doctype-system", null)) != null) style.doctype_system = prop;
        if ((prop = getNsPropExact(cur, "doctype-public", null)) != null) style.doctype_public = prop;
        String[] yesno = { "standalone", "indent", "omit-xml-declaration" };
        for (int i = 0; i < 3; ++i)
        {
            if ((prop = getNsPropExact(cur, yesno[i], null)) != null)
            {
                int v = -2;
                if (prop.equals("yes")) v = 1;
                else if (prop.equals("no")) v = 0;
                else
                {
                    transformError(null, style, cur, CFormat.format("invalid value for %s: %s\n", yesno[i], prop));
                    style.errors++;
                }
                if (v != -2)
                {
                    if (i == 0) style.standalone = v;
                    else if (i == 1) style.indent = v;
                    else style.omit_xml_declaration = v;
                }
            }
        }
        String elements = getNsPropExact(cur, "cdata-section-elements", null);
        if (elements != null)
        {
            if (style.cdata_section == null) style.cdata_section = new HashMap<>();
            forEachToken(style, cur, elements, Xslt::cdataToken, null);
        }
        if ((prop = getNsPropExact(cur, "media-type", null)) != null) style.media_type = prop;
        if (nodeFirstChild(cur) != null) parseContentError(style, nodeFirstChild(cur));
    }

    /** Port of xsltParseStylesheetDecimalFormat. */
    private static void parseStylesheetDecimalFormat(Stylesheet style, XmlNode cur)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        DecimalFormat format = style.decimal_format;
        String prop = getNsPropExact(cur, "name", null);
        if (prop != null)
        {
            if (validateQname(prop) != 0)
            {
                transformError(null, style, cur, CFormat.format("xsl:decimal-format: Invalid QName '%s'.\n", prop));
                style.warnings++;
                return;
            }
            String[] p = { prop };
            String ns_uri = getQnameUri(style.settings, cur, p);
            prop = p[0];
            if (prop == null)
            {
                style.warnings++;
                return;
            }
            format = decimalFormatGetByQname(style, ns_uri, prop);
            if (format != null)
            {
                transformError(null, style, cur, CFormat.format("xsltParseStylestyleDecimalFormat: %s already exists\n", prop));
                style.warnings++;
                return;
            }
            format = newDecimalFormat(ns_uri, prop);
            // Append the new decimal-format structure.
            DecimalFormat iter = style.decimal_format;
            while (iter.next != null) iter = iter.next;
            iter.next = format;
        }
        if ((prop = getNsPropExact(cur, "decimal-separator", null)) != null) format.decimal_point = prop;
        if ((prop = getNsPropExact(cur, "grouping-separator", null)) != null) format.grouping = prop;
        if ((prop = getNsPropExact(cur, "infinity", null)) != null) format.infinity = prop;
        if ((prop = getNsPropExact(cur, "minus-sign", null)) != null) format.minus_sign = prop;
        if ((prop = getNsPropExact(cur, "NaN", null)) != null) format.no_number = prop;
        if ((prop = getNsPropExact(cur, "percent", null)) != null) format.percent = prop;
        if ((prop = getNsPropExact(cur, "per-mille", null)) != null) format.permille = prop;
        if ((prop = getNsPropExact(cur, "zero-digit", null)) != null) format.zero_digit = prop;
        if ((prop = getNsPropExact(cur, "digit", null)) != null) format.digit = prop;
        if ((prop = getNsPropExact(cur, "pattern-separator", null)) != null) format.pattern_separator = prop;
        if (nodeFirstChild(cur) != null) parseContentError(style, nodeFirstChild(cur));
    }

    private static void spaceToken(Stylesheet style, XmlNode cur, String element, Object data)
    {
        String what = (String)data;
        if (element.equals("*"))
        {
            style.strip_all = what.equals("strip") ? 1 : -1;
        }
        else
        {
            String[] el = { element };
            String uri = getQnameUri(style.settings, cur, el);
            element = el[0];
            if (element != null)
            {
                String key = key2(element, uri);
                style.strip_spaces.putIfAbsent(key, what);
            }
        }
    }

    /** Port of xsltParseStylesheetPreserveSpace and xsltParseStylesheetStripSpace. */
    private static void parseStylesheetSpace(Stylesheet style, XmlNode cur, boolean strip)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        // libxslt creates the table before checking the attribute for strip-space, after for preserve-space.
        if (strip && style.strip_spaces == null) style.strip_spaces = new HashMap<>();
        String elements = getNsPropExact(cur, "elements", null);
        if (elements == null)
        {
            transformError(null, style, cur, strip ? "xsltParseStylesheetStripSpace: missing elements attribute\n"
                                                   : "xsltParseStylesheetPreserveSpace: missing elements attribute\n");
            style.warnings++;
            return;
        }
        if (style.strip_spaces == null) style.strip_spaces = new HashMap<>();
        forEachToken(style, cur, elements, Xslt::spaceToken, strip ? "strip" : "preserve");
        if (nodeFirstChild(cur) != null) parseContentError(style, nodeFirstChild(cur));
    }

    /** Port of xsltRegisterExtPrefix (extensions.c). */
    private static int registerExtPrefix(Stylesheet style, String prefix, String uri)
    {
        if (style == null || uri == null) return -1;
        for (ExtDef def = style.ns_defs; def != null; def = def.next)
        {
            if (streq(prefix, def.prefix)) return -1;
        }
        ExtDef ret = new ExtDef();
        ret.prefix = prefix;
        ret.uri = uri;
        ret.next = style.ns_defs;
        style.ns_defs = ret;
        // No extension module has a stylesheet initialization function.
        return 0;
    }

    /** Port of xsltCheckExtPrefix (compares with the registered prefixes, see libxslt). */
    private static int checkExtPrefix(Stylesheet style, String uri)
    {
        if (style == null || style.ns_defs == null) return 0;
        if (uri == null) uri = "#default";
        for (ExtDef cur = style.ns_defs; cur != null; cur = cur.next)
        {
            if (streq(uri, cur.prefix)) return 1;
        }
        return 0;
    }

    /** Port of xsltCheckExtURI. */
    private static int checkExtUri(Stylesheet style, String uri)
    {
        if (style == null || style.ns_defs == null || uri == null) return 0;
        for (ExtDef cur = style.ns_defs; cur != null; cur = cur.next)
        {
            if (streq(uri, cur.uri)) return 1;
        }
        return 0;
    }

    /** Port of xsltParseStylesheetExtPrefix. */
    private static void parseStylesheetExtPrefix(Stylesheet style, XmlNode cur, int is_xslt_elem)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        String prefixes = is_xslt_elem != 0 ? getNsPropExact(cur, "extension-element-prefixes", null)
                                          : getNsPropExact(cur, "extension-element-prefixes", XSLT_NAMESPACE);
        if (prefixes == null) return;
        int prefix = 0;
        while (at(prefixes, prefix) != 0)
        {
            while (isBlank(at(prefixes, prefix))) prefix++;
            if (at(prefixes, prefix) == 0) break;
            int end = prefix;
            while (at(prefixes, end) != 0 && !isBlank(at(prefixes, end))) end++;
            String p = prefixes.substring(prefix, end);
            XmlNs ns = p.equals("#default") ? nsLookupPrefix(cur, null) : nsLookupPrefix(cur, p);
            if (ns == null)
            {
                transformError(null, style, cur, CFormat.format("xsl:extension-element-prefix : undefined namespace %s\n", p));
                style.warnings++;
            }
            else
            {
                registerExtPrefix(style, p, nsHref(ns));
            }
            prefix = end;
        }
    }

    /** Port of xsltParseStylesheetExcludePrefix: returns the number of prefixes pushed. */
    private static int parseStylesheetExcludePrefix(Stylesheet style, XmlNode cur, int is_xslt_elem)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return 0;
        String prefixes = is_xslt_elem != 0 ? getNsPropExact(cur, "exclude-result-prefixes", null)
                                          : getNsPropExact(cur, "exclude-result-prefixes", XSLT_NAMESPACE);
        if (prefixes == null) return 0;
        int nb = 0;
        int prefix = 0;
        while (at(prefixes, prefix) != 0)
        {
            while (isBlank(at(prefixes, prefix))) prefix++;
            if (at(prefixes, prefix) == 0) break;
            int end = prefix;
            while (at(prefixes, end) != 0 && !isBlank(at(prefixes, end))) end++;
            String p = prefixes.substring(prefix, end);
            XmlNs ns = p.equals("#default") ? nsLookupPrefix(cur, null) : nsLookupPrefix(cur, p);
            if (ns == null)
            {
                transformError(null, style, cur, CFormat.format("xsl:exclude-result-prefixes : undefined namespace %s\n", p));
                style.warnings++;
            }
            else if (exclPrefixPush(style, nsHref(ns)) >= 0)
            {
                nb++;
            }
            prefix = end;
        }
        return nb;
    }

    private static void deleteNode(XmlNode n)
    {
        nodeUnlink(n);
        nodeFree(n);
    }

    /** Port of xsltPreprocessStylesheet: strip whitespace and comments from the stylesheet. */
    private static void preprocessStylesheet(Stylesheet style, XmlNode cur)
    {
        if (style == null || cur == null) return;
        XmlNode styleelem = (isXsltElem(cur) && isXsltName(cur, "stylesheet")) ? cur : null;
        XmlNode doc_node = domDocument(style.doc);
        XmlNode delete_node = null;
        while (cur != null)
        {
            if (delete_node != null)
            {
                deleteNode(delete_node);
                delete_node = null;
            }
            int k = nodeKind(cur);
            boolean skip_children = false;
            if (k == ELEMENT)
            {
                int excl_prefixes = 0;
                if (isXsltElem(cur))
                {
                    if (isXsltName(cur, "text")) skip_children = true;
                }
                else
                {
                    excl_prefixes = parseStylesheetExcludePrefix(style, cur, 0);
                }
                if (!skip_children)
                {
                    if (!nodeNsDecls(cur).isEmpty() && style.excl_prefix_tab.size() > 0)
                    {
                        XmlNode root = domRootElement(style.doc);
                        if (root != null && root != cur)
                        {
                            for (XmlNs ns : new ArrayList<>(nodeNsDecls(cur)))
                            {
                                for (int i = 0; i < style.excl_prefix_tab.size(); i++)
                                {
                                    if (nsPrefix(ns) != null && streq(nsHref(ns), style.excl_prefix_tab.get(i)))
                                    {
                                        // Move the declaration to the root, to avoid duplicating it without losing it.
                                        nsMoveDeclTo(cur, ns, root);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    // Prefixes declared locally are popped when going back.
                    if (excl_prefixes > 0)
                    {
                        preprocessStylesheet(style, nodeFirstChild(cur));
                        for (; excl_prefixes > 0; excl_prefixes--) exclPrefixPop(style);
                        skip_children = true;
                    }
                }
            }
            else if (k == TEXT)
            {
                if (isBlankNode(cur))
                {
                    if (getSpacePreserve(nodeParent(cur)) != 1) delete_node = cur;
                }
            }
            else if (k != CDATA)
            {
                delete_node = cur;
                skip_children = true;
            }

            if (!skip_children)
            {
                // A namespaced child of the stylesheet that is neither xslt nor an extension: skip its content.
                if (k == ELEMENT && nodeNs(cur) != null && styleelem != null && nodeParent(cur) == styleelem &&
                    !streq(nodeNsHref(cur), XSLT_NAMESPACE) && checkExtUri(style, nodeNsHref(cur)) == 0)
                {
                    skip_children = true;
                }
                else if (nodeFirstChild(cur) != null)
                {
                    cur = nodeFirstChild(cur);
                    continue;
                }
            }
            // skip_children:
            if (nodeNext(cur) != null)
            {
                cur = nodeNext(cur);
                continue;
            }
            do
            {
                cur = nodeParent(cur);
                if (cur == null) break;
                if (cur == doc_node)
                {
                    cur = null;
                    break;
                }
                if (nodeNext(cur) != null)
                {
                    cur = nodeNext(cur);
                    break;
                }
            } while (cur != null);
        }
        if (delete_node != null) deleteNode(delete_node);
    }

    /** Port of xsltParseTemplateContent: precompute the instructions of a template body. */
    private static void parseTemplateContent(Stylesheet style, XmlNode templ)
    {
        if (style == null || templ == null || nodeKind(templ) == XmlDomApi.NAMESPACE) return;
        XmlNode cur = nodeFirstChild(templ);
        XmlNode del = null;
        while (cur != null)
        {
            style.principal.op_count += 1;
            if (del != null)
            {
                deleteNode(del);
                del = null;
            }
            boolean skip_children = false;
            if (isXsltElem(cur))
            {
                stylePreCompute(style, cur);
                if (isXsltName(cur, "text"))
                {
                    // TODO (libxslt): move the processing of xsl:text to xsltPreprocessStylesheet.
                    if (nodeFirstChild(cur) != null)
                    {
                        XmlNode text = nodeFirstChild(cur);
                        int noesc = 0;
                        String prop = getNsPropExact(cur, "disable-output-escaping", null);
                        if (prop != null)
                        {
                            if (prop.equals("yes")) noesc = 1;
                            else if (!prop.equals("no"))
                            {
                                transformError(null, style, cur, "xsl:text: disable-output-escaping allows only yes or no\n");
                                style.warnings++;
                            }
                        }
                        while (text != null)
                        {
                            if (nodeKind(text) == COMMENT)
                            {
                                text = nodeNext(text);
                                continue;
                            }
                            if (nodeKind(text) != TEXT && nodeKind(text) != CDATA)
                            {
                                transformError(null, style, cur, "xsltParseTemplateContent: xslt:text content problem\n");
                                style.errors++;
                                break;
                            }
                            // libxml2 xmlStringTextNoenc: the text is output without escaping.
                            if (noesc != 0 && nodeKind(text) != CDATA) nodeSetName(text, "textnoenc");
                            text = nodeNext(text);
                        }
                        // Replace xsl:text by its children.
                        if (text == null)
                        {
                            text = nodeFirstChild(cur);
                            while (text != null)
                            {
                                XmlNode next = nodeNext(text);
                                nodeUnlink(text);
                                nodeAddPrevSibling(cur, text);
                                text = next;
                            }
                        }
                    }
                    del = cur;
                    skip_children = true;
                }
            }
            else if (nodeKind(cur) == ELEMENT && nodeNs(cur) != null && style.ns_defs != null &&
                     checkExtPrefix(style, nsPrefix(nodeNs(cur))) != 0)
            {
                // An extension element, compile it too.
                stylePreCompute(style, cur);
            }
            else if (nodeKind(cur) == ELEMENT)
            {
                // A literal result element, precompile its attribute value templates.
                if (nodeNs(cur) == null && style.default_alias != null)
                {
                    nodeSetNs(cur, nsLookupHref(cur, style.default_alias));
                }
                for (XmlAttr attr : new ArrayList<>(nodeAttrs(cur))) compileAttr(style, attr);
            }
            if (!skip_children)
            {
                if (nodeFirstChild(cur) != null)
                {
                    cur = nodeFirstChild(cur);
                    continue;
                }
            }
            // skip_children:
            if (nodeNext(cur) != null)
            {
                cur = nodeNext(cur);
                continue;
            }
            do
            {
                cur = nodeParent(cur);
                if (cur == null) break;
                if (cur == templ)
                {
                    cur = null;
                    break;
                }
                if (nodeNext(cur) != null)
                {
                    cur = nodeNext(cur);
                    break;
                }
            } while (cur != null);
        }
        if (del != null) deleteNode(del);

        // Skip the first params, then report and remove misplaced params.
        cur = nodeFirstChild(templ);
        while (cur != null)
        {
            if (isXsltElem(cur) && !isXsltName(cur, "param")) break;
            cur = nodeNext(cur);
        }
        while (cur != null)
        {
            if (isXsltElem(cur) && isXsltName(cur, "param"))
            {
                XmlNode param = cur;
                transformError(null, style, cur, "xsltParseTemplateContent: ignoring misplaced param element\n");
                style.warnings++;
                cur = nodeNext(cur);
                deleteNode(param);
            }
            else
            {
                break;
            }
        }
    }

    // Keys, port of xsltAddKey (keys.c).

    private static int skipString(String cur, int end)
    {
        if (cur == null || end < 0) return -1;
        char limit;
        if (at(cur, end) == '\'' || at(cur, end) == '"') limit = at(cur, end);
        else return end;
        end++;
        while (at(cur, end) != 0)
        {
            if (at(cur, end) == limit) return end + 1;
            end++;
        }
        return -1;
    }

    private static int skipPredicate(String cur, int end)
    {
        if (cur == null || end < 0) return -1;
        if (at(cur, end) != '[') return end;
        end++;
        int level = 0;
        while (at(cur, end) != 0)
        {
            if (at(cur, end) == '\'' || at(cur, end) == '"')
            {
                end = skipString(cur, end);
                if (end <= 0) return -1;
                continue;
            }
            else if (at(cur, end) == '[')
            {
                level += 1;
            }
            else if (at(cur, end) == ']')
            {
                if (level == 0) return end + 1;
                level -= 1;
            }
            end++;
        }
        return -1;
    }

    /** Port of xsltAddKey. */
    private static int addKey(Stylesheet style, String name, String name_uri, String match, String use, XmlNode inst)
    {
        if (style == null || name == null || match == null || use == null) return -1;
        KeyDef key = new KeyDef();
        key.name = name;
        key.name_uri = name_uri;
        key.match = match;
        key.use = use;
        key.inst = inst;

        // Split the | and register it as as many keys.
        String pattern = null;
        int current = 0, end = 0, start;
        while (at(match, current) != 0)
        {
            start = current;
            while (isBlank(at(match, current))) current++;
            end = current;
            while (at(match, end) != 0 && at(match, end) != '|')
            {
                if (at(match, end) == '[')
                {
                    end = skipPredicate(match, end);
                    if (end <= 0)
                    {
                        transformError(null, style, inst, CFormat.format("xsl:key : 'match' pattern is malformed: %s", key.match));
                        style.errors++;
                        return 0;
                    }
                }
                else
                {
                    end++;
                }
            }
            if (current == end)
            {
                transformError(null, style, inst, "xsl:key : 'match' pattern is empty\n");
                style.errors++;
                return 0;
            }
            if (at(match, start) != '/') pattern = strncat(pattern, "//", 2);
            pattern = strncat(pattern, match.substring(start), end - start);
            if (at(match, end) == '|')
            {
                pattern = strncat(pattern, "|", 1);
                end++;
            }
            current = end;
        }
        if (pattern == null)
        {
            transformError(null, style, inst, "xsl:key : 'match' pattern is empty\n");
            style.errors++;
            return 0;
        }
        // XSLT 1.0: it is an error for use or match to contain a variable reference.
        key.comp = xpathCompile(style.settings, inst, pattern, true);
        if (key.comp == null)
        {
            transformError(null, style, inst, CFormat.format("xsl:key : 'match' pattern compilation failed '%s'\n", pattern));
            style.errors++;
        }
        key.usecomp = xpathCompile(style.settings, inst, use, true);
        if (key.usecomp == null)
        {
            transformError(null, style, inst, CFormat.format("xsl:key : 'use' expression compilation failed '%s'\n", use));
            style.errors++;
        }
        // Keep the provided order, add the new one at the end.
        if (style.keys == null)
        {
            style.keys = key;
        }
        else
        {
            KeyDef prev = style.keys;
            while (prev.next != null) prev = prev.next;
            prev.next = key;
        }
        key.next = null;
        return 0;
    }

    /** Port of xsltParseStylesheetKey. */
    private static void parseStylesheetKey(Stylesheet style, XmlNode key)
    {
        if (style == null || key == null || nodeKind(key) != ELEMENT) return;
        String name = null, name_uri = null, match = null, use = null;
        String prop = getNsPropExact(key, "name", null);
        error:
        {
            if (prop != null)
            {
                String[] p = { prop };
                String uri = getQnameUri(style.settings, key, p);
                prop = p[0];
                if (prop == null)
                {
                    style.errors++;
                    break error;
                }
                name = prop;
                if (uri != null) name_uri = uri;
            }
            else
            {
                transformError(null, style, key, "xsl:key : error missing name\n");
                style.errors++;
                break error;
            }
            match = getNsPropExact(key, "match", null);
            if (match == null)
            {
                transformError(null, style, key, "xsl:key : error missing match\n");
                style.errors++;
                break error;
            }
            use = getNsPropExact(key, "use", null);
            if (use == null)
            {
                transformError(null, style, key, "xsl:key : error missing use\n");
                style.errors++;
                break error;
            }
            addKey(style, name, name_uri, match, use, key);
        }
        if (nodeFirstChild(key) != null) parseContentError(style, nodeFirstChild(key));
    }

    /** Port of xsltParseStylesheetTemplate. */
    private static void parseStylesheetTemplate(Stylesheet style, XmlNode template_elem)
    {
        if (style == null || template_elem == null || nodeKind(template_elem) != ELEMENT) return;
        Template ret = newTemplate();
        ret.next = style.templates;
        style.templates = ret;
        ret.style = style;
        getInheritedNsList(style, ret, template_elem);

        String prop = getNsPropExact(template_elem, "mode", null);
        if (prop != null)
        {
            String[] p = { prop };
            String uri = getQnameUri(style.settings, template_elem, p);
            prop = p[0];
            if (prop == null)
            {
                style.errors++;
                return;
            }
            ret.mode = prop;
            ret.mode_uri = uri;
        }
        prop = getNsPropExact(template_elem, "match", null);
        if (prop != null) ret.match = prop;
        prop = getNsPropExact(template_elem, "priority", null);
        if (prop != null)
        {
            ret.priority = (float)XPath.stringToNumber(prop);
        }
        prop = getNsPropExact(template_elem, "name", null);
        if (prop != null)
        {
            String[] p = { prop };
            String uri = getQnameUri(style.settings, template_elem, p);
            prop = p[0];
            if (prop == null)
            {
                style.errors++;
                return;
            }
            if (validateNcname(prop) != 0)
            {
                transformError(null, style, template_elem, CFormat.format("xsl:template : error invalid name '%s'\n", prop));
                style.errors++;
                return;
            }
            ret.name = prop;
            ret.name_uri = uri;
        }
        // Parse the content and register the pattern.
        parseTemplateContent(style, template_elem);
        ret.elem = template_elem;
        ret.content = nodeFirstChild(template_elem);
        addTemplate(style, ret, ret.mode, ret.mode_uri);
    }

    // Attribute sets, port of libxslt attributes.c (the compilation).

    private static AttrSet findAttrSet(Stylesheet style, String ncname, String ns)
    {
        if (style.attribute_sets == null) return null;
        String key = key2(ncname, ns);
        return style.attribute_sets.get(key);
    }

    private static void addAttrSet(Stylesheet style, AttrSet set)
    {
        if (style.attribute_sets == null) style.attribute_sets = new HashMap<>();
        String key = key2(set.name, set.ns);
        style.attribute_sets.put(key, set);
        // Keep the declaration order.
        set.next_in_order = null;
        if (style.attribute_set_list == null)
        {
            style.attribute_set_list = set;
        }
        else
        {
            AttrSet p = style.attribute_set_list;
            while (p.next_in_order != null) p = p.next_in_order;
            p.next_in_order = set;
        }
    }

    private static void removeAttrSet(Stylesheet style, AttrSet set)
    {
        String key = key2(set.name, set.ns);
        style.attribute_sets.remove(key);
        if (style.attribute_set_list == set)
        {
            style.attribute_set_list = set.next_in_order;
            return;
        }
        AttrSet p = style.attribute_set_list;
        while (p != null && p.next_in_order != set) p = p.next_in_order;
        if (p != null) p.next_in_order = set.next_in_order;
    }

    /** Port of xsltParseStylesheetAttributeSet. */
    private static void parseStylesheetAttributeSet(Stylesheet style, XmlNode cur)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        String value = getNsPropExact(cur, "name", null);
        if (value == null || value.isEmpty())
        {
            genericError(style.settings, "xsl:attribute-set : name is missing\n");
            return;
        }
        if (validateQname(value) != 0)
        {
            transformError(null, style, cur, CFormat.format("xsl:attribute-set : The name '%s' is not a valid QName.\n", value));
            style.errors++;
            return;
        }
        String[] prefix = { null };
        String ncname = splitQname(value, prefix);
        String ns_uri = null;
        if (prefix[0] != null)
        {
            XmlNs ns = nsLookupPrefix(cur, prefix[0]);
            if (ns == null)
            {
                transformError(null, style, cur, CFormat.format("xsl:attribute-set : No namespace found for QName '%s:%s'\n", prefix[0], ncname));
                style.errors++;
                return;
            }
            ns_uri = nsHref(ns);
        }
        AttrSet set = findAttrSet(style, ncname, ns_uri);
        if (set == null)
        {
            set = new AttrSet();
            set.name = ncname;
            set.ns = ns_uri;
            addAttrSet(style, set);
        }

        // Only xsl:attribute elements are allowed.
        for (XmlNode child = nodeFirstChild(cur); child != null; child = nodeNext(child))
        {
            if (nodeKind(child) != ELEMENT || nodeNs(child) == null || !isXsltElem(child))
            {
                if (nodeKind(child) == ELEMENT)
                    transformError(null, style, child, CFormat.format("xsl:attribute-set : unexpected child %s\n", nodeName(child)));
                else
                    transformError(null, style, child, "xsl:attribute-set : child of unexpected type\n");
            }
            else if (!isXsltName(child, "attribute"))
            {
                transformError(null, style, child, CFormat.format("xsl:attribute-set : unexpected child xsl:%s\n", nodeName(child)));
            }
            else
            {
                stylePreCompute(style, child);
                if (nodeFirstChild(child) != null) parseTemplateContent(style, child);
                if (nodeUserData(child) == null)
                {
                    transformError(null, style, child, CFormat.format("xsl:attribute-set : internal error, attribute %s not compiled\n", nodeName(child)));
                }
                else
                {
                    AttrElem e = new AttrElem();
                    e.attr = child;
                    if (set.attrs == null)
                    {
                        set.attrs = e;
                    }
                    else
                    {
                        AttrElem p = set.attrs;
                        while (p.next != null) p = p.next;
                        p.next = e;
                    }
                }
            }
        }

        // The attribute use-attribute-sets.
        value = getNsPropExact(cur, "use-attribute-sets", null);
        if (value != null)
        {
            int curval = 0;
            while (at(value, curval) != 0)
            {
                while (isBlank(at(value, curval))) curval++;
                if (at(value, curval) == 0) break;
                int endval = curval;
                while (at(value, endval) != 0 && !isBlank(at(value, endval))) endval++;
                String qn = value.substring(curval, endval);
                if (validateQname(qn) != 0)
                {
                    transformError(null, style, cur, CFormat.format("xsl:attribute-set : The name '%s' in use-attribute-sets is not a valid QName.\n", qn));
                    style.errors++;
                    return;
                }
                String[] prefix2 = { null };
                String ncname2 = splitQname(qn, prefix2);
                String ns_uri2 = null;
                if (prefix2[0] != null)
                {
                    XmlNs ns2 = nsLookupPrefix(cur, prefix2[0]);
                    if (ns2 == null)
                    {
                        transformError(null, style, cur, CFormat.format("xsl:attribute-set : No namespace found for QName '%s:%s' in use-attribute-sets\n",
                                                                        prefix2[0], ncname2));
                        style.errors++;
                        return;
                    }
                    ns_uri2 = nsHref(ns2);
                }
                // xsltAddUseAttrSetList: no duplicates.
                boolean dup = false;
                UseAttrSet last = null;
                for (UseAttrSet u = set.use_attr_sets; u != null; u = u.next)
                {
                    if (streq(u.ncname, ncname2) && streq(u.ns, ns_uri2)) dup = true;
                    last = u;
                }
                if (!dup)
                {
                    UseAttrSet u = new UseAttrSet();
                    u.ncname = ncname2;
                    u.ns = ns_uri2;
                    if (last == null) set.use_attr_sets = u;
                    else last.next = u;
                }
                curval = endval;
            }
        }
    }

    /** Port of xsltMergeAttrSets. */
    private static void mergeAttrSets(AttrSet set, AttrSet other)
    {
        for (AttrElem old = other.attrs; old != null; old = old.next)
        {
            // Check that the attribute is not yet in the list.
            AttrElem cur = set.attrs;
            int add = 1;
            while (cur != null)
            {
                StylePreComp cur_comp = (StylePreComp)nodeUserData(cur.attr);
                StylePreComp old_comp = (StylePreComp)nodeUserData(old.attr);
                if (streq(cur_comp.name, old_comp.name) && streq(cur_comp.ns, old_comp.ns))
                {
                    add = 0;
                    break;
                }
                if (cur.next == null) break;
                cur = cur.next;
            }
            if (add != 0)
            {
                AttrElem e = new AttrElem();
                e.attr = old.attr;
                if (cur == null) set.attrs = e;
                else cur.next = e;
            }
        }
    }

    /** Port of xsltResolveUseAttrSets. */
    private static void resolveUseAttrSets(AttrSet set, Stylesheet top_style, int depth)
    {
        UseAttrSet use = set.use_attr_sets;
        while (use != null)
        {
            // Iterate the top stylesheet and all imports.
            for (Stylesheet cur = top_style; cur != null; cur = nextImport(cur))
            {
                AttrSet other = findAttrSet(cur, use.ncname, use.ns);
                if (other != null)
                {
                    resolveAttrSet(other, top_style, cur, use.ncname, use.ns, depth + 1);
                    mergeAttrSets(set, other);
                    break;
                }
            }
            use = use.next;
        }
        set.use_attr_sets = null;
    }

    /** Port of xsltResolveAttrSet. */
    private static void resolveAttrSet(AttrSet set, Stylesheet top_style, Stylesheet style, String name, String ns, int depth)
    {
        if (set.state == AttrSet.RESOLVED) return;
        if (set.state == AttrSet.RESOLVING)
        {
            transformError(null, top_style, null, CFormat.format("xsl:attribute-set : use-attribute-sets recursion detected on %s\n", name));
            top_style.errors++;
            set.state = AttrSet.RESOLVED;
            return;
        }
        if (depth > 100)
        {
            transformError(null, top_style, null, CFormat.format("xsl:attribute-set : use-attribute-sets maximum recursion depth exceeded on %s\n", name));
            top_style.errors++;
            return;
        }
        set.state = AttrSet.RESOLVING;
        resolveUseAttrSets(set, top_style, depth);
        // Merge the imported sets.
        for (Stylesheet cur = nextImport(style); cur != null; cur = nextImport(cur))
        {
            AttrSet other = findAttrSet(cur, name, ns);
            if (other != null)
            {
                resolveUseAttrSets(other, top_style, depth);
                mergeAttrSets(set, other);
                removeAttrSet(cur, other);
            }
        }
        set.state = AttrSet.RESOLVED;
    }

    /** Port of xsltResolveStylesheetAttributeSet: resolve all sets and move them to the top stylesheet.
        Resolving a set can free sets of later imports, so the sets are looked up by name. */
    private static void resolveStylesheetAttributeSet(Stylesheet style)
    {
        int error = 0;
        for (Stylesheet cur = style; cur != null; cur = nextImport(cur))
        {
            if (cur.attribute_set_list == null) continue;
            List<String> names = new ArrayList<>();
            for (AttrSet set = cur.attribute_set_list; set != null; set = set.next_in_order)
            {
                names.add(set.name);
                names.add(set.ns);
            }
            int n = names.size() / 2;
            for (int i = 0; i < n; i++)
            {
                String name = names.get(2 * i);
                String ns = names.get(2 * i + 1);
                AttrSet set = findAttrSet(cur, name, ns);
                if (set == null) continue;
                if (error != 0)
                {
                    if (cur != style)
                    {
                        removeAttrSet(cur, set);
                    }
                    continue;
                }
                resolveAttrSet(set, style, cur, name, ns, 1);
                if (cur != style)
                {
                    // Move the attribute set to the top stylesheet.
                    removeAttrSet(cur, set);
                    if (findAttrSet(style, name, ns) != null)
                    {
                        genericError(style.settings, CFormat.format("xsl:attribute-set : internal error, can't move imported  attribute set %s\n", name));
                        error = 1;
                    }
                    else
                    {
                        addAttrSet(style, set);
                    }
                }
            }
        }
    }

    // Namespace aliases, port of xsltNamespaceAlias (namespaces.c).

    private static void namespaceAlias(Stylesheet style, XmlNode node)
    {
        if (style == null || node == null) return;
        String style_prefix = getNsPropExact(node, "stylesheet-prefix", null);
        String result_prefix = null;
        if (style_prefix == null)
        {
            transformError(null, style, node, "namespace-alias: stylesheet-prefix attribute missing\n");
            return;
        }
        result_prefix = getNsPropExact(node, "result-prefix", null);
        if (result_prefix == null)
        {
            transformError(null, style, node, "namespace-alias: result-prefix attribute missing\n");
            return;
        }
        String literal_ns_name;
        String target_ns_name;
        XmlNs literal_ns, target_ns;
        if (style_prefix.equals("#default"))
        {
            literal_ns = nsLookupPrefix(node, null);
            literal_ns_name = literal_ns != null ? nsHref(literal_ns) : null;
        }
        else
        {
            literal_ns = nsLookupPrefix(node, style_prefix);
            if (literal_ns == null || nsHref(literal_ns) == null)
            {
                transformError(null, style, node, CFormat.format("namespace-alias: prefix %s not bound to any namespace\n", style_prefix));
                return;
            }
            literal_ns_name = nsHref(literal_ns);
        }
        // For #default without a default namespace, UNDEFINED_DEFAULT_NS is used.
        if (result_prefix.equals("#default"))
        {
            target_ns = nsLookupPrefix(node, null);
            target_ns_name = target_ns != null ? nsHref(target_ns) : UNDEFINED_DEFAULT_NS;
        }
        else
        {
            target_ns = nsLookupPrefix(node, result_prefix);
            if (target_ns == null || nsHref(target_ns) == null)
            {
                transformError(null, style, node, CFormat.format("namespace-alias: prefix %s not bound to any namespace\n", result_prefix));
                return;
            }
            target_ns_name = nsHref(target_ns);
        }
        if (literal_ns_name == null)
        {
            // #default without a default namespace in scope.
            if (target_ns != null) style.default_alias = nsHref(target_ns);
        }
        else
        {
            if (style.ns_aliases == null) style.ns_aliases = new HashMap<>();
            // xmlHashAddEntry: the first alias wins.
            if (style.ns_aliases.get(literal_ns_name) == null)
            {
                style.ns_aliases.put(literal_ns_name, target_ns_name != null ? target_ns_name : "");
            }
        }
    }

    // Global variables, port of xsltParseGlobalVariable/Param (variables.c).

    private static int registerGlobalVariable(Stylesheet style, String name, String ns_uri, String sel,
                                              XmlNode tree, StylePreComp comp, String value)
    {
        if (style == null || name == null || comp == null) return -1;
        StackElem elem = new StackElem();
        elem.comp = comp;
        elem.name = name;
        elem.select = sel;
        elem.name_uri = ns_uri;
        elem.tree = tree;
        for (StackElem tmp = style.variables; tmp != null; tmp = tmp.next)
        {
            if (elem.comp.type == StylePreComp.FUNC_VARIABLE && tmp.comp.type == StylePreComp.FUNC_VARIABLE &&
                streq(elem.name, tmp.name) && streq(elem.name_uri, tmp.name_uri))
            {
                transformError(null, style, comp.inst, CFormat.format("redefinition of global variable %s\n", elem.name));
                style.errors++;
            }
        }
        elem.next = style.variables;
        style.variables = elem;
        if (value != null)
        {
            elem.computed = 1;
            elem.value = XPathResult.newString(value);
        }
        return 0;
    }

    private static void parseGlobalVariable(Stylesheet style, XmlNode cur, boolean param)
    {
        if (cur == null || style == null || nodeKind(cur) != ELEMENT) return;
        String what = param ? "xsl:param" : "xsl:variable";
        stylePreCompute(style, cur);
        Object c = nodeUserData(cur);
        if (c == null || c == StylePreComp.EXT_MARKER || !(c instanceof StylePreComp))
        {
            transformError(null, style, cur, CFormat.format("%s : compilation failed\n", what));
            return;
        }
        StylePreComp comp = (StylePreComp)c;
        if (comp.name == null)
        {
            transformError(null, style, cur, CFormat.format("%s : missing name attribute\n", what));
            return;
        }
        // Parse the content (a sequence constructor).
        if (nodeFirstChild(cur) != null) parseTemplateContent(style, cur);
        registerGlobalVariable(style, comp.name, comp.ns, comp.select, nodeFirstChild(cur), comp, null);
    }

    // Imports and includes, port of libxslt imports.c.

    /** Port of xsltNormalizeCompSteps and xsltFixImportedCompSteps: only the first pattern of
        each name list of the template hash is renumbered, like libxslt. */
    private static void fixImportedCompSteps(Stylesheet master, Stylesheet style)
    {
        if (style.templates_hash != null)
        {
            for (Pattern comp : style.templates_hash.values())
            {
                if (comp == null) continue;
                for (int ix = 0; ix < comp.nb_step; ix++)
                {
                    comp.steps.get(ix).previous_extra += master.extras_nr;
                    comp.steps.get(ix).index_extra += master.extras_nr;
                    comp.steps.get(ix).len_extra += master.extras_nr;
                }
            }
        }
        master.extras_nr += style.extras_nr;
        for (Stylesheet res = style.imports; res != null; res = res.next) fixImportedCompSteps(master, res);
    }

    /** Port of xsltCheckCycle. */
    private static int checkCycle(Stylesheet style, XmlNode cur, String uri)
    {
        int depth = 0;
        for (Stylesheet ancestor = style; ancestor != null; ancestor = ancestor.parent)
        {
            if (++depth >= MAX_NESTING)
            {
                transformError(null, style, cur, CFormat.format("maximum nesting depth exceeded: %s\n", uri));
                return -1;
            }
            if (ancestor.doc != null && streq(domUrl(ancestor.doc), uri))
            {
                transformError(null, style, cur, CFormat.format("recursion detected on imported URL %s\n", uri));
                return -1;
            }
            for (XsltDocument docptr = ancestor.includes; docptr != null; docptr = docptr.includes)
            {
                if (++depth >= MAX_NESTING)
                {
                    transformError(null, style, cur, CFormat.format("maximum nesting depth exceeded: %s\n", uri));
                    return -1;
                }
                if (streq(domUrl(docptr.doc), uri))
                {
                    transformError(null, style, cur, CFormat.format("recursion detected on included URL %s\n", uri));
                    return -1;
                }
            }
        }
        return 0;
    }

    /** Port of xsltParseStylesheetImportedDoc. */
    private static Stylesheet parseStylesheetImportedDoc(XmlDocument doc, Stylesheet parent_style, Settings settings)
    {
        if (doc == null) return null;
        Stylesheet ret = newStylesheetInternal(parent_style, settings);
        if (parseStylesheetUser(ret, doc) != 0)
        {
            return null;
        }
        return ret;
    }

    /** Port of xsltParseStylesheetImport. */
    private static int parseStylesheetImport(Stylesheet style, XmlNode cur)
    {
        int ret = -1;
        String base = null, uri = null;
        String uri_ref = getNsPropExact(cur, "href", null);
        if (uri_ref == null)
        {
            transformError(null, style, cur, "xsl:import : missing href attribute\n");
            return ret;
        }
        base = nodeGetBase(style.doc, cur);
        uri = XmlUri.build(uri_ref, base);
        if (uri == null)
        {
            transformError(null, style, cur, CFormat.format("xsl:import : invalid URI reference %s\n", uri_ref));
            return ret;
        }
        if (checkCycle(style, cur, uri) < 0) return ret;
        XmlDocument imp = load(style, uri, Settings.LOAD_STYLESHEET);
        if (imp == null)
        {
            transformError(null, style, cur, CFormat.format("xsl:import : unable to load %s\n", uri));
            return ret;
        }
        Stylesheet res = parseStylesheetImportedDoc(imp, style, null);
        if (res != null)
        {
            res.next = style.imports;
            style.imports = res;
            if (style.parent == null) fixImportedCompSteps(style, res);
            ret = 0;
        }
        return ret;
    }

    /** Port of xsltLoadStyleDocument (documents.c). */
    private static XsltDocument loadStyleDocument(Stylesheet style, String uri)
    {
        for (XsltDocument ret = style.doc_list; ret != null; ret = ret.next)
        {
            if (ret.doc != null && domUrl(ret.doc) != null && streq(domUrl(ret.doc), uri)) return ret;
        }
        XmlDocument doc = load(style, uri, Settings.LOAD_STYLESHEET);
        if (doc == null) return null;
        XsltDocument ret = new XsltDocument();
        ret.doc = doc;
        ret.next = style.doc_list;
        style.doc_list = ret;
        return ret;
    }

    /** Port of xsltParseStylesheetInclude. */
    private static int parseStylesheetInclude(Stylesheet style, XmlNode cur)
    {
        int ret = -1;
        String base = null, uri = null;
        String uri_ref = getNsPropExact(cur, "href", null);
        if (uri_ref == null)
        {
            transformError(null, style, cur, "xsl:include : missing href attribute\n");
            return ret;
        }
        base = nodeGetBase(style.doc, cur);
        uri = XmlUri.build(uri_ref, base);
        if (uri == null)
        {
            transformError(null, style, cur, CFormat.format("xsl:include : invalid URI reference %s\n", uri_ref));
            return ret;
        }
        if (checkCycle(style, cur, uri) < 0) return ret;
        XsltDocument include = loadStyleDocument(style, uri);
        if (include == null)
        {
            transformError(null, style, cur, CFormat.format("xsl:include : unable to load %s\n", uri));
            return ret;
        }
        XmlDocument old_doc = style.doc;
        style.doc = include.doc;
        // Chain to the stylesheet for the recursion check.
        include.includes = style.includes;
        style.includes = include;
        int old_nopreproc = style.nopreproc;
        style.nopreproc = include.preproc;
        Stylesheet result = parseStylesheetProcess(style, include.doc);
        style.nopreproc = old_nopreproc;
        include.preproc = 1;
        style.includes = include.includes;
        style.doc = old_doc;
        ret = result != null ? 0 : -1;
        return ret;
    }

    /** Port of xsltParseStylesheetTop. */
    private static void parseStylesheetTop(Stylesheet style, XmlNode top)
    {
        if (top == null || nodeKind(top) != ELEMENT) return;
        String prop = getNsPropExact(top, "version", null);
        if (prop == null)
        {
            transformError(null, style, top, "xsl:version is missing: document may not be a stylesheet\n");
            style.warnings++;
        }
        else
        {
            if (!prop.equals("1.0") && !prop.equals("1.1"))
            {
                transformError(null, style, top, "xsl:version: only 1.1 features are supported\n");
                style.forwards_compatible = 1;
                style.warnings++;
            }
        }

        // The xsl:import elements.
        XmlNode cur = nodeFirstChild(top);
        while (cur != null)
        {
            style.principal.op_count += 1;
            if (isBlankNode(cur))
            {
                cur = nodeNext(cur);
                continue;
            }
            if (isXsltElem(cur) && isXsltName(cur, "import"))
            {
                if (parseStylesheetImport(style, cur) != 0) style.errors++;
            }
            else
            {
                break;
            }
            cur = nodeNext(cur);
        }

        // The other top level elements.
        while (cur != null)
        {
            style.principal.op_count += 1;
            if (isBlankNode(cur))
            {
                cur = nodeNext(cur);
                continue;
            }
            if (nodeKind(cur) == TEXT)
            {
                if (nodeContent(cur) != null) transformError(null, style, cur, CFormat.format("misplaced text node: '%s'\n", nodeContent(cur)));
                style.errors++;
                cur = nodeNext(cur);
                continue;
            }
            if (nodeKind(cur) == ELEMENT && nodeNs(cur) == null)
            {
                genericError(style.settings, CFormat.format("Found a top-level element %s with null namespace URI\n", nodeName(cur)));
                style.errors++;
                cur = nodeNext(cur);
                continue;
            }
            if (nodeKind(cur) == ELEMENT && !isXsltElem(cur))
            {
                // No extension module has top level elements.
                cur = nodeNext(cur);
                continue;
            }
            if (nodeKind(cur) != ELEMENT)
            {
                // libxslt compares the names of other nodes too.
            }
            if (isXsltName(cur, "import"))
            {
                transformError(null, style, cur, "xsltParseStylesheetTop: ignoring misplaced import element\n");
                style.errors++;
            }
            else if (isXsltName(cur, "include"))
            {
                if (parseStylesheetInclude(style, cur) != 0) style.errors++;
            }
            else if (isXsltName(cur, "strip-space")) parseStylesheetSpace(style, cur, true);
            else if (isXsltName(cur, "preserve-space")) parseStylesheetSpace(style, cur, false);
            else if (isXsltName(cur, "output")) parseStylesheetOutput(style, cur);
            else if (isXsltName(cur, "key")) parseStylesheetKey(style, cur);
            else if (isXsltName(cur, "decimal-format")) parseStylesheetDecimalFormat(style, cur);
            else if (isXsltName(cur, "attribute-set")) parseStylesheetAttributeSet(style, cur);
            else if (isXsltName(cur, "variable")) parseGlobalVariable(style, cur, false);
            else if (isXsltName(cur, "param")) parseGlobalVariable(style, cur, true);
            else if (isXsltName(cur, "template")) parseStylesheetTemplate(style, cur);
            else if (isXsltName(cur, "namespace-alias")) namespaceAlias(style, cur);
            else
            {
                if (style.forwards_compatible == 0)
                {
                    transformError(null, style, cur, CFormat.format("xsltParseStylesheetTop: unknown %s element\n",
                                                                    nodeName(cur) != null ? nodeName(cur) : "(null)"));
                    style.errors++;
                }
            }
            cur = nodeNext(cur);
        }
    }

    /** Port of xsltParseStylesheetProcess. */
    private static Stylesheet parseStylesheetProcess(Stylesheet ret, XmlDocument doc)
    {
        if (doc == null || ret == null) return null;
        XmlNode cur = domRootElement(doc);
        if (cur == null)
        {
            transformError(null, ret, domDocument(doc), "xsltParseStylesheetProcess : empty stylesheet\n");
            return null;
        }
        if (isXsltElem(cur) && (isXsltName(cur, "stylesheet") || isXsltName(cur, "transform")))
        {
            ret.literal_result = 0;
            parseStylesheetExcludePrefix(ret, cur, 1);
            parseStylesheetExtPrefix(ret, cur, 1);
        }
        else
        {
            parseStylesheetExcludePrefix(ret, cur, 0);
            parseStylesheetExtPrefix(ret, cur, 0);
            ret.literal_result = 1;
        }
        if (ret.nopreproc == 0) preprocessStylesheet(ret, cur);
        if (ret.literal_result == 0)
        {
            parseStylesheetTop(ret, cur);
        }
        else
        {
            // The document itself might be the template, check xsl:version.
            String prop = getNsPropExact(cur, "version", XSLT_NAMESPACE);
            if (prop == null)
            {
                transformError(null, ret, cur, "xsltParseStylesheetProcess : document is not a stylesheet\n");
                return null;
            }
            if (!prop.equals("1.0") && !prop.equals("1.1"))
            {
                transformError(null, ret, cur, "xsl:version: only 1.1 features are supported\n");
                ret.forwards_compatible = 1;
                ret.warnings++;
            }
            Template templ = newTemplate();
            templ.next = ret.templates;
            ret.templates = templ;
            templ.match = "/";
            // Parse the content and register the pattern.
            parseTemplateContent(ret, domDocument(doc));
            templ.elem = domDocument(doc);
            templ.content = nodeFirstChild(domDocument(doc));
            addTemplate(ret, templ, null, null);
            ret.literal_result = 1;
        }
        return ret;
    }

    /** Port of xsltParseStylesheetUser. */
    private static int parseStylesheetUser(Stylesheet style, XmlDocument doc)
    {
        if (style == null || doc == null) return -1;
        // libxslt calls xsltGatherNamespaces here, before style->doc is set, so it does nothing.
        style.doc = doc;
        if (parseStylesheetProcess(style, doc) == null)
        {
            style.doc = null;
            return -1;
        }
        if (style.parent == null) resolveStylesheetAttributeSet(style);
        if (style.errors != 0)
        {
            // Detach the doc, it is not freed with the stylesheet.
            style.doc = null;
            return -1;
        }
        return 0;
    }

    /**
     * Compile a stylesheet (libxslt xsltParseStylesheetDoc, xml_libxslt1_compile). The
     * stylesheet takes the document and modifies it (whitespace is stripped etc).
     * @param doc The stylesheet document.
     * @param settings The settings, null for the defaults.
     * @return The stylesheet, null after reporting errors.
     */
    public static Stylesheet compile(XmlDocument doc, Settings settings)
    {
        if (settings == null) settings = new Settings();
        return parseStylesheetImportedDoc(doc, null, settings);
    }

    // Description of a compiled stylesheet, for the differential tests.

    private static void dStr(StringBuilder mb, String label, String s)
    {
        mb.append(' ').append(label).append('=');
        if (s == null)
        {
            mb.append("(null)");
            return;
        }
        mb.append('"');
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (c == '\n') mb.append("\\n");
            else if (c == '"') mb.append("\\\"");
            else if (c == '\\') mb.append("\\\\");
            else if (c < 32) mb.append(CFormat.format("\\x%02x", (int)c));
            else mb.append(c);
        }
        mb.append('"');
    }

    private static void dIndent(StringBuilder mb, int depth)
    {
        for (int i = 0; i < depth; ++i) mb.append("  ");
    }

    private static final String[] STYLE_TYPE_NAMES = { "?", "copy", "sort", "text", "element", "attribute", "comment", "pi", "copy-of",
        "value-of", "number", "apply-imports", "call-template", "apply-templates", "choose", "if", "for-each",
        "document", "with-param", "param", "variable", "when", "extension" };

    private static String styleTypeName(int type)
    {
        if (type < 0 || type > StylePreComp.FUNC_EXTENSION) return "?";
        return STYLE_TYPE_NAMES[type];
    }

    /** The hash keys are printed with | between the parts and (null) for missing ones. */
    private static String dKey(String key)
    {
        StringBuilder mb = new StringBuilder();
        for (int i = 0; i < key.length(); i++)
        {
            char c = key.charAt(i);
            if (c == 1) mb.append('|');
            else if (c == 2) mb.append("(null)");
            else mb.append(c);
        }
        return mb.toString();
    }

    /** strcmp of the UTF-8 encodings: code point order. */
    private static int dCmp(String a, String b)
    {
        int i = 0, j = 0;
        while (i < a.length() && j < b.length())
        {
            int ca = a.codePointAt(i);
            int cb = b.codePointAt(j);
            if (ca != cb) return ca < cb ? -1 : 1;
            i += Character.charCount(ca);
            j += Character.charCount(cb);
        }
        if (i < a.length()) return 1;
        if (j < b.length()) return -1;
        return 0;
    }

    /** Print the lines collected, sorted. */
    private static void dSorted(StringBuilder mb, List<String> lines)
    {
        lines.sort(Xslt::dCmp);
        for (String l : lines) mb.append(l);
    }

    private static void dPrecomp(StringBuilder mb, Object o)
    {
        if (o == StylePreComp.EXT_MARKER || !(o instanceof StylePreComp))
        {
            mb.append(" precomp=ext-marker");
            return;
        }
        StylePreComp c = (StylePreComp)o;
        mb.append(" precomp=").append(styleTypeName(c.type));
        if (c.type == StylePreComp.FUNC_EXTENSION || c.type == StylePreComp.FUNC_DOCUMENT)
        {
            if (c.type == StylePreComp.FUNC_DOCUMENT)
            {
                mb.append(CFormat.format(" ver11=%d", c.ver11));
                dStr(mb, "filename", c.filename);
            }
            return;
        }
        if (c.stype != null) dStr(mb, "stype", c.stype);
        if (c.has_stype != 0) mb.append(CFormat.format(" has_stype=%d", c.has_stype));
        if (c.number != 0) mb.append(CFormat.format(" number=%d", c.number));
        if (c.order != null) dStr(mb, "order", c.order);
        if (c.has_order != 0) mb.append(CFormat.format(" has_order=%d", c.has_order));
        if (c.descending != 0) mb.append(CFormat.format(" descending=%d", c.descending));
        if (c.lang != null) dStr(mb, "lang", c.lang);
        if (c.has_lang != 0) mb.append(CFormat.format(" has_lang=%d", c.has_lang));
        if (c.case_order != null) dStr(mb, "case_order", c.case_order);
        if (c.lower_first != 0) mb.append(CFormat.format(" lower_first=%d", c.lower_first));
        if (c.use != null) dStr(mb, "use", c.use);
        if (c.has_use != 0) mb.append(CFormat.format(" has_use=%d", c.has_use));
        if (c.noescape != 0) mb.append(CFormat.format(" noescape=%d", c.noescape));
        if (c.name != null) dStr(mb, "name", c.name);
        if (c.has_name != 0) mb.append(CFormat.format(" has_name=%d", c.has_name));
        if (c.ns != null) dStr(mb, "ns", c.ns);
        if (c.has_ns != 0) mb.append(CFormat.format(" has_ns=%d", c.has_ns));
        if (c.mode != null) dStr(mb, "mode", c.mode);
        if (c.mode_uri != null) dStr(mb, "mode_uri", c.mode_uri);
        if (c.test != null) dStr(mb, "test", c.test);
        if (c.select != null) dStr(mb, "select", c.select);
        if (c.comp != null) mb.append(" comp");
        if (nsNr(c.ns_list) != 0) mb.append(CFormat.format(" ns_nr=%d", nsNr(c.ns_list)));
        if (c.type == StylePreComp.FUNC_NUMBER)
        {
            NumberData n = c.numdata;
            dStr(mb, "level", n.level);
            dStr(mb, "count", n.count);
            dStr(mb, "from", n.from);
            dStr(mb, "value", n.value);
            dStr(mb, "format", n.format);
            mb.append(CFormat.format(" has_format=%d digits_per_group=%d grouping_character=%d count_pat=%d from_pat=%d",
                                     n.has_format, n.digits_per_group, n.grouping_character, n.count_pat != null ? 1 : 0, n.from_pat != null ? 1 : 0));
        }
    }

    private static void dAvt(StringBuilder mb, AttrVT avt)
    {
        mb.append(CFormat.format(" avt=[strstart=%d", avt.strstart));
        boolean str = avt.strstart != 0;
        for (int i = 0; i < avt.nb_seg; ++i)
        {
            Object seg = avt.segments.get(i);
            if (str && seg != null) dStr(mb, "s", (String)seg);
            else if (str) mb.append(" s=(null)");
            else mb.append(seg != null ? " x" : " x=(null)");
            str = !str;
        }
        mb.append("]");
    }

    private static void dTree(StringBuilder mb, XmlNode node, int depth)
    {
        for (; node != null; node = nodeNext(node))
        {
            if (nodeKind(node) == DTD) continue;
            dIndent(mb, depth);
            switch (nodeKind(node))
            {
            case ELEMENT:
                mb.append("element");
                dStr(mb, "name", nodeName(node));
                if (nodeNs(node) != null) dStr(mb, "ns", nodeNsUri(node));
                if (nodeUserData(node) != null) dPrecomp(mb, nodeUserData(node));
                for (XmlNs ns : nodeNsDecls(node))
                {
                    mb.append(" xmlns");
                    dStr(mb, "p", nsPrefix(ns));
                    dStr(mb, "h", nsHref(ns));
                }
                mb.append('\n');
                for (XmlAttr a : nodeAttrs(node))
                {
                    dIndent(mb, depth + 1);
                    mb.append("attr");
                    dStr(mb, "name", attrName(a));
                    XmlNode an = attrNode(a);
                    if (nodeNs(an) != null) dStr(mb, "ns", nodeNsUri(an));
                    if (nodeUserData(an) instanceof AttrVT avt) dAvt(mb, avt);
                    mb.append('\n');
                }
                dTree(mb, nodeFirstChild(node), depth + 1);
                continue;
            case TEXT:
                mb.append("text");
                if (nodeName(node) != null && nodeName(node).equals("textnoenc")) mb.append(" noenc");
                dStr(mb, "content", nodeContent(node));
                break;
            case CDATA:
                mb.append("cdata");
                dStr(mb, "content", nodeContent(node));
                break;
            case COMMENT:
                mb.append("comment");
                break;
            case PI:
                mb.append("pi");
                break;
            default:
                mb.append(CFormat.format("node kind=%d", nodeKind(node)));
                break;
            }
            mb.append('\n');
        }
    }

    private static void dPatterns(StringBuilder mb, String label, Pattern p, int depth)
    {
        if (p == null) return;
        dIndent(mb, depth);
        mb.append(label);
        for (; p != null; p = p.next)
        {
            mb.append(" [");
            dStr(mb, "p", p.pattern);
            mb.append(CFormat.format(" prio=%g", (double)p.priority));
            if (p.mode != null) dStr(mb, "mode", p.mode);
            if (p.mode_uri != null) dStr(mb, "mode_uri", p.mode_uri);
            if (p.templ != null)
            {
                int ix = 0;
                Template t = p.templ.style != null ? p.templ.style.templates : null;
                while (t != null && t != p.templ)
                {
                    t = t.next;
                    ix++;
                }
                mb.append(CFormat.format(" tmpl=%d", t != null ? ix : -1));
            }
            mb.append(" ]");
        }
        mb.append('\n');
    }

    private static void dStyle(StringBuilder mb, Stylesheet style, int depth)
    {
        dIndent(mb, depth);
        mb.append(CFormat.format("stylesheet errors=%d warnings=%d literal_result=%d forwards_compatible=%d extras_nr=%d strip_all=%d\n",
                                 style.errors, style.warnings, style.literal_result, style.forwards_compatible, style.extras_nr, style.strip_all));
        dIndent(mb, depth);
        mb.append("output");
        dStr(mb, "method", style.method);
        dStr(mb, "method_uri", style.method_uri);
        dStr(mb, "version", style.version);
        dStr(mb, "encoding", style.encoding);
        mb.append(CFormat.format(" omit=%d standalone=%d indent=%d", style.omit_xml_declaration, style.standalone, style.indent));
        dStr(mb, "doctype_public", style.doctype_public);
        dStr(mb, "doctype_system", style.doctype_system);
        dStr(mb, "media_type", style.media_type);
        dStr(mb, "default_alias", style.default_alias);
        mb.append('\n');

        List<Map<String, String>> maps = new ArrayList<>();
        maps.add(style.strip_spaces);
        maps.add(style.cdata_section);
        maps.add(style.ns_aliases);
        String[] labels = { "space", "cdata", "alias" };
        for (int m = 0; m < 3; ++m)
        {
            if (maps.get(m) == null) continue;
            List<String> lines = new ArrayList<>();
            for (Map.Entry<String, String> e : maps.get(m).entrySet())
            {
                StringBuilder l = new StringBuilder();
                dIndent(l, depth);
                l.append(labels[m]);
                dStr(l, "key", dKey(e.getKey()));
                dStr(l, "value", e.getValue());
                l.append('\n');
                lines.add(l.toString());
            }
            dSorted(mb, lines);
        }
        for (int i = 0; i < style.excl_prefix_tab.size(); ++i)
        {
            dIndent(mb, depth);
            mb.append("exclude");
            dStr(mb, "uri", style.excl_prefix_tab.get(i));
            mb.append('\n');
        }
        for (ExtDef d = style.ns_defs; d != null; d = d.next)
        {
            dIndent(mb, depth);
            mb.append("extension");
            dStr(mb, "prefix", d.prefix);
            dStr(mb, "uri", d.uri);
            mb.append('\n');
        }
        for (DecimalFormat f = style.decimal_format; f != null; f = f.next)
        {
            dIndent(mb, depth);
            mb.append("decimal-format");
            dStr(mb, "name", f.name);
            dStr(mb, "ns", f.ns_uri);
            dStr(mb, "digit", f.digit);
            dStr(mb, "pattern_separator", f.pattern_separator);
            dStr(mb, "minus_sign", f.minus_sign);
            dStr(mb, "infinity", f.infinity);
            dStr(mb, "nan", f.no_number);
            dStr(mb, "decimal_point", f.decimal_point);
            dStr(mb, "grouping", f.grouping);
            dStr(mb, "percent", f.percent);
            dStr(mb, "permille", f.permille);
            dStr(mb, "zero_digit", f.zero_digit);
            mb.append('\n');
        }
        for (KeyDef k = style.keys; k != null; k = k.next)
        {
            dIndent(mb, depth);
            mb.append("key");
            dStr(mb, "name", k.name);
            dStr(mb, "name_uri", k.name_uri);
            dStr(mb, "match", k.match);
            dStr(mb, "use", k.use);
            mb.append(CFormat.format(" comp=%d usecomp=%d\n", k.comp != null ? 1 : 0, k.usecomp != null ? 1 : 0));
        }
        for (StackElem v = style.variables; v != null; v = v.next)
        {
            dIndent(mb, depth);
            mb.append(CFormat.format("global %s", styleTypeName(v.comp != null ? v.comp.type : 0)));
            dStr(mb, "name", v.name);
            dStr(mb, "name_uri", v.name_uri);
            dStr(mb, "select", v.select);
            mb.append(CFormat.format(" tree=%d computed=%d\n", v.tree != null ? 1 : 0, v.computed));
        }
        if (style.attribute_sets != null)
        {
            List<String> lines = new ArrayList<>();
            for (AttrSet set = style.attribute_set_list; set != null; set = set.next_in_order)
            {
                StringBuilder l = new StringBuilder();
                dIndent(l, depth);
                l.append("attribute-set");
                dStr(l, "name", set.name);
                dStr(l, "ns", set.ns);
                l.append(CFormat.format(" state=%d", set.state));
                for (AttrElem a = set.attrs; a != null; a = a.next)
                {
                    Object o = nodeUserData(a.attr);
                    StylePreComp c = o instanceof StylePreComp sc ? sc : null;
                    l.append(" [");
                    dStr(l, "name", c != null ? c.name : null);
                    dStr(l, "ns", c != null ? c.ns : null);
                    l.append(CFormat.format(" line=%d ]", nodeLine(a.attr)));
                }
                for (UseAttrSet u = set.use_attr_sets; u != null; u = u.next)
                {
                    l.append(" use");
                    dStr(l, "ncname", u.ncname);
                    dStr(l, "ns", u.ns);
                }
                l.append('\n');
                lines.add(l.toString());
            }
            dSorted(mb, lines);
        }
        for (Template t = style.templates; t != null; t = t.next)
        {
            dIndent(mb, depth);
            mb.append("template");
            dStr(mb, "match", t.match);
            dStr(mb, "name", t.name);
            dStr(mb, "name_uri", t.name_uri);
            dStr(mb, "mode", t.mode);
            dStr(mb, "mode_uri", t.mode_uri);
            mb.append(CFormat.format(" prio=%g position=%d content=%d inherited=%d", (double)t.priority, t.position,
                                     t.content != null ? 1 : 0, t.inherited_ns_nr));
            for (int i = 0; i < t.inherited_ns_nr; ++i) dStr(mb, "ns", nsPrefix(t.inherited_ns[i]));
            mb.append('\n');
        }
        dPatterns(mb, "root_match", style.root_match, depth);
        dPatterns(mb, "key_match", style.key_match, depth);
        dPatterns(mb, "elem_match", style.elem_match, depth);
        dPatterns(mb, "attr_match", style.attr_match, depth);
        dPatterns(mb, "parent_match", style.parent_match, depth);
        dPatterns(mb, "text_match", style.text_match, depth);
        dPatterns(mb, "pi_match", style.pi_match, depth);
        dPatterns(mb, "comment_match", style.comment_match, depth);
        if (style.templates_hash != null)
        {
            List<String> lines = new ArrayList<>();
            for (Map.Entry<String, Pattern> e : style.templates_hash.entrySet())
            {
                if (e.getValue() == null) continue;
                StringBuilder l = new StringBuilder();
                String k = dKey(e.getKey());
                StringBuilder label = new StringBuilder();
                label.append("hash");
                dStr(label, "key", k);
                dPatterns(l, label.toString(), e.getValue(), depth);
                lines.add(l.toString());
            }
            dSorted(mb, lines);
        }
        if (style.doc != null)
        {
            dIndent(mb, depth);
            mb.append("document");
            XmlNode doc_node = domDocument(style.doc);
            if (nodeUserData(doc_node) != null) dPrecomp(mb, nodeUserData(doc_node));
            mb.append('\n');
            dTree(mb, nodeFirstChild(doc_node), depth + 1);
        }
        for (XsltDocument d = style.doc_list; d != null; d = d.next)
        {
            dIndent(mb, depth);
            mb.append(CFormat.format("include preproc=%d\n", d.preproc));
            dTree(mb, nodeFirstChild(domDocument(d.doc)), depth + 1);
        }
        for (Stylesheet imp = style.imports; imp != null; imp = imp.next)
        {
            dIndent(mb, depth);
            mb.append("import\n");
            dStyle(mb, imp, depth + 1);
        }
    }

    /**
     * A description of the compiled stylesheet (output settings, templates, keys, global
     * variables, attribute sets, ...), for testing (xml_libxslt1_stylesheet_describe).
     * @param style The stylesheet.
     * @return The description.
     */
    public static String stylesheetDescribe(Stylesheet style)
    {
        StringBuilder mb = new StringBuilder();
        if (style != null) dStyle(mb, style, 0);
        return mb.toString();
    }
}
