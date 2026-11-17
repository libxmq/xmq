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
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import org.libxmq.imp.xml.CFormat;
import org.libxmq.imp.xml.HtmlSave;
import org.libxmq.imp.xml.XPath;
import org.libxmq.imp.xml.XPathEvalContext;
import org.libxmq.imp.xml.XPathException;
import org.libxmq.imp.xml.XPathResult;
import org.libxmq.imp.xml.XPathSettings;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlChars;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlSave;
import org.libxmq.imp.xml.XmlUri;

/**
 * XSLT 1.0 transformations, a port of libxslt transform.c, variables.c, templates.c,
 * keys.c, functions.c, numbers.c and documents.c onto the dom and xpath of libxmq (the port
 * of src/main/c/parts/xml_libxslt1_transform.c). The stylesheet is compiled with
 * {@link Xslt}.
 */
public final class Transform
{
    private Transform()
    {
    }

    /** The output method xml of a result (the same numbers as libxslt xsltOutputType). */
    public static final int OUTPUT_XML = 0;
    /** The output method html. */
    public static final int OUTPUT_HTML = 1;
    /** The output method text. */
    public static final int OUTPUT_TEXT = 2;

    // The limits of libxslt (xsltMaxDepth, xsltMaxVars, XSLT_MAX_SORT).
    private static final int XSLT_MAX_DEPTH = 3000;
    private static final int XSLT_MAX_VARS = 15000;
    private static final int XSLT_MAX_SORT = 15;

    private static final int XSLT_VAR_GLOBAL = 1 << 0;
    private static final int XSLT_VAR_IN_SELECT = 1 << 1;

    private static final String XSLT_NAMESPACE = "http://www.w3.org/1999/XSL/Transform";
    private static final String XSLT_LIBXSLT_NAMESPACE = "http://xmlsoft.org/XSLT/namespace";
    private static final String XSLT_SAXON_NAMESPACE = "http://icl.com/saxon";
    private static final String XSLT_XT_NAMESPACE = "http://www.jclark.com/xt";
    private static final String XSLT_XALAN_NAMESPACE = "org.apache.xalan.xslt.extensions.Redirect";
    private static final String XSLT_UNDEFINED_DEFAULT_NS = "#UNDEFINED_DEFAULT_NS";

    // libxslt marks a global variable being computed by replacing its name (xsltComputingGlobalVarMarker).
    // Compared by identity.
    private static final String XSLT_COMPUTING_GLOBAL_VAR_MARKER = new String(" var/param being computed");

    private static boolean streq(String a, String b)
    {
        return Xslt.streq(a, b);
    }

    private static String fmt(String f, Object... args)
    {
        return CFormat.format(f, args);
    }

    private static void transformError(TransformCtx t, Stylesheet style, XmlNode node, String message)
    {
        Xslt.transformError(t, style, node, message);
    }

    private static void genericError(Settings s, String message)
    {
        Xslt.genericError(s, message);
    }

    /** The precomputed instruction of a node (its user data), null if none or the extension marker. */
    private static StylePreComp precomp(XmlNode n)
    {
        Object o = n == null ? null : nodeUserData(n);
        return o instanceof StylePreComp p ? p : null;
    }

    private static boolean isExtMarker(Object o)
    {
        return o == StylePreComp.EXT_MARKER;
    }

    // libxml2 tree operations, with the semantics the libxslt code relies on ///

    private static boolean lxIsNoenc(XmlNode n)
    {
        String name = nodeName(n);
        return name != null && name.equals("textnoenc");
    }

    private static boolean lxIsElement(XmlNode n)
    {
        return n != null && nodeKind(n) == ELEMENT;
    }

    /** The parent, null for nodes whose parent is not a node of the tree (attributes have their owner). */
    private static XmlNode lxParent(XmlNode n)
    {
        return n != null ? nodeParent(n) : null;
    }

    /** Port of xmlSearchNs: the declaration of prefix in scope of node, also looking at the
        namespaces of the ancestors (libxslt result trees can use undeclared namespaces). */
    private static XmlNs lxSearchNs(XmlNode node, String prefix)
    {
        if (node == null || nodeKind(node) == NAMESPACE) return null;
        if (prefix != null && prefix.equals("xml")) return nsLookupPrefix(node, "xml");
        XmlNode orig = node;
        while (nodeKind(node) != ELEMENT)
        {
            node = nodeParent(node);
            if (node == null) return null;
        }
        while (node != null && nodeKind(node) == ELEMENT)
        {
            for (XmlNs cur : nodeNsDecls(node))
            {
                if (streq(nsPrefix(cur), prefix) && nsHref(cur) != null) return cur;
            }
            if (orig != node)
            {
                XmlNs cur = nodeNs(node);
                if (cur != null && streq(nsPrefix(cur), prefix) && nsHref(cur) != null) return cur;
            }
            node = nodeParent(node);
        }
        return null;
    }

    /** Port of xmlNsInScope. */
    private static int lxNsInScope(XmlNode node, XmlNode ancestor, String prefix)
    {
        while (node != null && node != ancestor)
        {
            if (nodeKind(node) == ELEMENT)
            {
                for (XmlNs tst : nodeNsDecls(node))
                {
                    if (nsPrefix(tst) == null && prefix == null) return 0;
                    if (nsPrefix(tst) != null && prefix != null && nsPrefix(tst).equals(prefix)) return 0;
                }
            }
            node = nodeParent(node);
        }
        if (node != ancestor) return -1;
        return 1;
    }

    /** Port of xmlSearchNsByHref (for elements). */
    private static XmlNs lxSearchNsByHref(XmlNode node, String href)
    {
        if (node == null || nodeKind(node) == NAMESPACE) return null;
        if (streq(href, XML_NAMESPACE)) return nsLookupPrefix(node, "xml");
        boolean is_attr = nodeKind(node) == ATTRIBUTE;
        XmlNode orig = node;
        while (nodeKind(node) != ELEMENT)
        {
            node = nodeParent(node);
            if (node == null) return null;
        }
        while (node != null && nodeKind(node) == ELEMENT)
        {
            for (XmlNs cur : nodeNsDecls(node))
            {
                if (streq(nsHref(cur), href))
                {
                    if ((!is_attr || nsPrefix(cur) != null) && lxNsInScope(orig, node, nsPrefix(cur)) == 1) return cur;
                }
            }
            if (orig != node)
            {
                XmlNs cur = nodeNs(node);
                if (cur != null && streq(nsHref(cur), href))
                {
                    if ((!is_attr || nsPrefix(cur) != null) && lxNsInScope(orig, node, nsPrefix(cur)) == 1) return cur;
                }
            }
            node = nodeParent(node);
        }
        return null;
    }

    /** Port of xmlNewNs on an element: null if the prefix is already declared there. */
    private static XmlNs lxNewNs(XmlNode elem, String href, String prefix)
    {
        if (!lxIsElement(elem)) return null;
        return nsDeclare(elem, prefix, href);
    }

    /** Port of xmlAddChild: text is merged with a preceding text node of the same kind
        (escaped or not), then cur is freed and the text node is returned. */
    private static XmlNode lxAddChild(XmlNode parent, XmlNode cur)
    {
        if (cur == null) return null;
        if (parent == null)
        {
            nodeFree(cur);
            return null;
        }
        if (nodeKind(parent) == TEXT)
        {
            String c = nodeContent(cur);
            if (c != null) nodeAppendContent(parent, c);
            nodeFree(cur);
            return parent;
        }
        XmlNode prev = nodeLastChild(parent);
        if (cur == prev) return cur;
        if (nodeKind(cur) == TEXT && prev != null && nodeKind(prev) == TEXT && lxIsNoenc(prev) == lxIsNoenc(cur))
        {
            String c = nodeContent(cur);
            if (c != null) nodeAppendContent(prev, c);
            nodeFree(cur);
            return prev;
        }
        return nodeAppendChild(parent, cur);
    }

    /** libxml2 gives an attribute set to "" an empty text child (the html serializer writes it as a=""). */
    private static XmlAttr lxAttrTextChild(XmlAttr a, String value)
    {
        if (a != null && value != null && nodeFirstChild(attrNode(a)) == null)
        {
            XmlNode an = attrNode(a);
            nodeAppendChild(an, nodeNewText(nodeDom(an), ""));
        }
        return a;
    }

    /** Port of xmlSetNsProp: replace the value of an attribute with the same name and namespace or add one. */
    private static XmlAttr lxSetNsProp(XmlNode elem, XmlNs ns, String name, String value)
    {
        if (!lxIsElement(elem) || name == null) return null;
        return lxAttrTextChild(nodeSetAttr(elem, name, ns, value != null ? value : ""), value);
    }

    /** Port of xmlNewNsProp: add an attribute without looking for duplicates. */
    private static XmlAttr lxNewNsProp(XmlNode elem, XmlNs ns, String name, String value)
    {
        if (!lxIsElement(elem) || name == null) return null;
        return lxAttrTextChild(nodeAppendAttr(elem, name, ns, value != null ? value : ""), value);
    }

    /** The attribute value with the entity references substituted (xmlNodeListGetString). */
    private static String lxAttrValue(XmlNode attr)
    {
        return nodeStringValue(attr);
    }

    /** Port of xmlDocCopyNode(node, doc, 0): a copy of the node itself (no attributes, namespaces or children). */
    private static XmlNode lxShallowCopy(XmlDocument dom, XmlNode node)
    {
        XmlNode copy = null;
        switch (nodeKind(node))
        {
        case ELEMENT:
            copy = nodeNewElement(dom, nodeName(node), null);
            break;
        case PI:
            copy = nodeNewPi(dom, nodeName(node), nodeContent(node));
            break;
        case COMMENT:
            copy = nodeNewComment(dom, nodeContent(node));
            break;
        case ENTITY_REF:
            copy = nodeNewEntityRef(dom, nodeName(node));
            break;
        default:
            return null;
        }
        if (copy != null) nodeSetLine(copy, nodeLine(node));
        return copy;
    }

    private static boolean lxIsRealNode(XmlNode n)
    {
        if (n == null) return false;
        switch (nodeKind(n))
        {
        case ELEMENT:
        case TEXT:
        case CDATA:
        case ATTRIBUTE:
        case DOCUMENT:
        case COMMENT:
        case PI:
            return true;
        default:
            return false;
        }
    }

    private static boolean lxIsBlankNode(XmlNode n)
    {
        return nodeKind(n) == TEXT && Xslt.isBlankStr(nodeContent(n));
    }

    /** The first child, null for nodes without children in libxml2 (namespace nodes). */
    private static XmlNode lxChildren(XmlNode n)
    {
        if (n == null || nodeKind(n) == NAMESPACE || nodeKind(n) == ATTRIBUTE) return null;
        return nodeFirstChild(n);
    }

    private static boolean lxHasChildren(XmlNode n)
    {
        return lxChildren(n) != null;
    }

    /** Port of xmlNodeGetContent for an element: the concatenated text. */
    private static String lxNodeGetContent(XmlNode n)
    {
        return nodeStringValue(n);
    }

    // Result tree fragments, port of libxslt variables.c ///////////

    private static RVT xsltRvtOf(XmlDocument dom)
    {
        if (dom == null) return null;
        Object o = nodeUserData(domDocument(dom));
        if (o instanceof RVT rvt && rvt.dom == dom) return rvt;
        return null;
    }

    /**
     * Is the document a result tree fragment (libxslt XSLT_IS_RES_TREE_FRAG)?
     * @param t The transformation.
     * @param dom The document.
     * @return True for a result tree fragment.
     */
    static boolean isRtf(TransformCtx t, XmlDocument dom)
    {
        return xsltRvtOf(dom) != null;
    }

    /** Port of xsltCreateRVT. */
    private static RVT xsltCreateRvt(TransformCtx t)
    {
        RVT rvt = new RVT();
        rvt.dom = domNew();
        nodeSetUserData(domDocument(rvt.dom), rvt);
        return rvt;
    }

    /** Port of xsltReleaseRVT (without the cache). */
    private static void xsltReleaseRvt(TransformCtx t, RVT rvt)
    {
        if (rvt == null) return;
        if (rvt.keys != null)
        {
            xsltFreeDocKeys(rvt.keys);
        }
        t.xpath.forget(domDocument(rvt.dom));
    }

    /** Port of xsltRegisterTmpRVT. */
    private static void xsltRegisterTmpRvt(TransformCtx t, RVT rvt)
    {
        rvt.prev = null;
        rvt.flag = RVT.LOCAL;
        if (t.context_variable != null)
        {
            rvt.next = t.context_variable.fragment;
            t.context_variable.fragment = rvt;
            return;
        }
        rvt.next = t.tmp_rvt;
        if (t.tmp_rvt != null) t.tmp_rvt.prev = rvt;
        t.tmp_rvt = rvt;
    }

    /** Port of xsltRegisterLocalRVT. */
    private static void xsltRegisterLocalRvt(TransformCtx t, RVT rvt)
    {
        rvt.prev = null;
        rvt.flag = RVT.LOCAL;
        if (t.context_variable != null && (t.context_variable.flags & XSLT_VAR_IN_SELECT) != 0)
        {
            rvt.next = t.context_variable.fragment;
            t.context_variable.fragment = rvt;
            return;
        }
        rvt.next = t.local_rvt;
        if (t.local_rvt != null) t.local_rvt.prev = rvt;
        t.local_rvt = rvt;
    }

    /** Port of xsltRegisterPersistRVT. */
    private static void xsltRegisterPersistRvt(TransformCtx t, RVT rvt)
    {
        rvt.flag = RVT.GLOBAL;
        rvt.prev = null;
        rvt.next = t.persist_rvt;
        if (t.persist_rvt != null) t.persist_rvt.prev = rvt;
        t.persist_rvt = rvt;
    }

    /** Port of xsltFlagRVTs. */
    private static int xsltFlagRvts(TransformCtx t, XPathResult obj, int val)
    {
        if (obj == null || obj.type() != XPathResult.NODESET) return 0;
        int n = obj.size();
        for (int i = 0; i < n; i++)
        {
            XmlNode cur = obj.node(i);
            XmlDocument doc = nodeDom(cur);
            if (doc == null)
            {
                transformError(t, null, t.inst, "Internal error in xsltFlagRVTs(): Cannot retrieve the doc of a node.\n");
                return -1;
            }
            RVT rvt = xsltRvtOf(doc);
            if (rvt != null && rvt.flag != RVT.GLOBAL)
            {
                if (val == RVT.LOCAL)
                {
                    if (rvt.flag == RVT.FUNC_RESULT) rvt.flag = RVT.LOCAL;
                }
                else if (val == RVT.GLOBAL)
                {
                    if (rvt.flag != RVT.LOCAL)
                    {
                        genericError(t.style.settings, fmt("xsltFlagRVTs: Invalid transition %d => GLOBAL\n", rvt.flag));
                        rvt.flag = RVT.GLOBAL;
                        return -1;
                    }
                    // Registered as persistent in xsltReleaseLocalRVTs.
                    rvt.flag = RVT.GLOBAL;
                }
                else if (val == RVT.FUNC_RESULT)
                {
                    rvt.flag = val;
                }
            }
        }
        return 0;
    }

    /** Port of xsltReleaseLocalRVTs. */
    private static void xsltReleaseLocalRvts(TransformCtx t, RVT base)
    {
        RVT cur = t.local_rvt;
        if (cur == base) return;
        if (cur.prev != null) transformError(t, null, null, "localRVT not head of list\n");
        // Reset localRVT early because some RVTs might be registered again.
        t.local_rvt = base;
        if (base != null) base.prev = null;
        do
        {
            RVT tmp = cur;
            cur = cur.next;
            if (tmp.flag == RVT.LOCAL)
            {
                xsltReleaseRvt(t, tmp);
            }
            else if (tmp.flag == RVT.GLOBAL)
            {
                xsltRegisterPersistRvt(t, tmp);
            }
            else if (tmp.flag == RVT.FUNC_RESULT)
            {
                xsltRegisterLocalRvt(t, tmp);
                tmp.flag = RVT.FUNC_RESULT;
            }
            else
            {
                genericError(t.style.settings, "xsltReleaseLocalRVTs: Unexpected RVT flag (nil)\n");
            }
        } while (cur != base);
    }

    private static void xsltFreeRvtList(TransformCtx t, RVT cur)
    {
        while (cur != null)
        {
            RVT next = cur.next;
            xsltReleaseRvt(t, cur);
            cur = next;
        }
    }

    /** Port of xsltFreeRVTs. */
    private static void xsltFreeRvts(TransformCtx t)
    {
        xsltFreeRvtList(t, t.local_rvt);
        t.local_rvt = null;
        xsltFreeRvtList(t, t.tmp_rvt);
        t.tmp_rvt = null;
        xsltFreeRvtList(t, t.persist_rvt);
        t.persist_rvt = null;
    }

    /** A node set holding the document node of the fragment (xmlXPathNewValueTree). */
    private static XPathResult xsltNewValueTree(RVT rvt)
    {
        XPathResult r = XPathResult.newNodeSet();
        r.addNode(domDocument(rvt.dom));
        r.setTree(true);
        return r;
    }

    // XPath evaluation /////////////////////////////////////////////////

    /** Report an error of an evaluation like libxml2 (xmlXPathErr). */
    private static void xsltReportEvalError(TransformCtx t, XPath comp, XPathException err)
    {
        if (err == null || err.code() == 0) return;
        Xslt.reportXpathError(t.style.settings, comp != null ? comp.expression() : null, err);
    }

    /** Evaluate with the context position and size of the transformation (xmlXPathCompiledEval). */
    private static XPathResult xsltXpathEval(TransformCtx t, XPath comp, XmlNode node)
    {
        if (comp == null) return null;
        try
        {
            return t.xpath.eval(comp, node, t.xp_position, t.xp_size);
        }
        catch (XPathException err)
        {
            xsltReportEvalError(t, comp, err);
            return null;
        }
    }

    /** Port of xsltPreCompEval: the context position and size are restored afterwards. */
    private static XPathResult xsltPreCompEval(TransformCtx t, XmlNode node, StylePreComp comp)
    {
        long pos = t.xp_position, size = t.xp_size;
        XPathResult res = xsltXpathEval(t, comp.comp, node);
        t.xp_position = pos;
        t.xp_size = size;
        return res;
    }

    /** Port of xsltPreCompEvalToBoolean: -1 on an error. */
    private static int xsltPreCompEvalToBoolean(TransformCtx t, XmlNode node, StylePreComp comp)
    {
        XPathResult res = xsltPreCompEval(t, node, comp);
        if (res == null) return -1;
        return res.castToBoolean() ? 1 : 0;
    }

    // Variables, port of libxslt variables.c ////////////////////////////

    private static StackElem xsltNewStackElem()
    {
        return new StackElem();
    }

    /** Port of xsltCopyStackElem: a runtime copy of a global variable declaration. */
    private static StackElem xsltCopyStackElem(StackElem elem)
    {
        StackElem cur = xsltNewStackElem();
        cur.name = elem.name;
        cur.name_uri = elem.name_uri;
        cur.select = elem.select;
        cur.tree = elem.tree;
        cur.comp = elem.comp;
        return cur;
    }

    /** Port of xsltFreeStackElem (for runtime elements). */
    private static void xsltFreeRuntimeStackElem(TransformCtx t, StackElem elem)
    {
        if (elem == null) return;
        elem.value = null;
        while (elem.fragment != null)
        {
            RVT cur = elem.fragment;
            elem.fragment = cur.next;
            if (cur.flag == RVT.LOCAL)
            {
                xsltReleaseRvt(t, cur);
            }
            else if (cur.flag == RVT.FUNC_RESULT)
            {
                xsltRegisterLocalRvt(t, cur);
                cur.flag = RVT.FUNC_RESULT;
            }
            else
            {
                genericError(t.style.settings, fmt("xsltFreeStackElem: Unexpected RVT flag %d\n", cur.flag));
            }
        }
    }

    private static void xsltFreeRuntimeStackElemList(TransformCtx t, StackElem elem)
    {
        while (elem != null)
        {
            StackElem next = elem.next;
            xsltFreeRuntimeStackElem(t, elem);
            elem = next;
        }
    }

    private static StackElem varsAt(TransformCtx t, int i)
    {
        return t.vars_tab.get(i);
    }

    /** Port of xsltStackLookup. */
    private static StackElem xsltStackLookup(TransformCtx t, String name, String name_uri)
    {
        if (name == null || t.vars_nr == 0) return null;
        for (int i = t.vars_nr; i > t.vars_base; i--)
        {
            for (StackElem cur = varsAt(t, i - 1); cur != null; cur = cur.next)
            {
                if (streq(cur.name, name) && streq(cur.name_uri, name_uri)) return cur;
            }
        }
        return null;
    }

    /** Port of xsltCheckStackElem: 0 not found, 1 variable, 2 param, 3 with-param. */
    private static int xsltCheckStackElem(TransformCtx t, String name, String name_uri)
    {
        if (name == null) return -1;
        StackElem cur = xsltStackLookup(t, name, name_uri);
        if (cur == null) return 0;
        if (cur.comp != null)
        {
            if (cur.comp.type == StylePreComp.FUNC_WITHPARAM) return 3;
            else if (cur.comp.type == StylePreComp.FUNC_PARAM) return 2;
        }
        return 1;
    }

    private static void xsltVarsPush(TransformCtx t, StackElem elem)
    {
        if (t.vars_nr < t.vars_tab.size()) t.vars_tab.set(t.vars_nr, elem);
        else t.vars_tab.add(elem);
        t.vars_nr++;
        t.vars = elem;
    }

    /** Port of xsltAddStackElem: every element of the list is pushed. */
    private static int xsltAddStackElem(TransformCtx t, StackElem elem)
    {
        if (elem == null) return -1;
        do
        {
            xsltVarsPush(t, elem);
            elem = elem.next;
        } while (elem != null);
        return 0;
    }

    /** Port of xsltLocalVariablePush. */
    private static int xsltLocalVariablePush(TransformCtx t, StackElem variable, int level)
    {
        xsltVarsPush(t, variable);
        variable.level = level;
        return 0;
    }

    /** Port of xsltLocalVariablePop. */
    private static void xsltLocalVariablePop(TransformCtx t, int limit_nr, int level)
    {
        if (t.vars_nr <= 0) return;
        do
        {
            if (t.vars_nr <= limit_nr) break;
            StackElem variable = varsAt(t, t.vars_nr - 1);
            if (variable.level <= level) break;
            if (variable.level >= 0) xsltFreeRuntimeStackElemList(t, variable);
            t.vars_nr--;
        } while (t.vars_nr != 0);
        t.vars = t.vars_nr > 0 ? varsAt(t, t.vars_nr - 1) : null;
    }

    /** Port of xsltTemplateParamsCleanup: xsl:with-param items (level -1, -2) are not freed. */
    private static void xsltTemplateParamsCleanup(TransformCtx t)
    {
        for (; t.vars_nr > t.vars_base; t.vars_nr--)
        {
            StackElem param = varsAt(t, t.vars_nr - 1);
            if (param.level >= 0) xsltFreeRuntimeStackElemList(t, param);
        }
        t.vars = t.vars_nr > 0 ? varsAt(t, t.vars_nr - 1) : null;
    }

    /** Instantiate content into a new result tree fragment (the content part of xsltEvalVariable). */
    private static XPathResult xsltEvalContentInto(TransformCtx t, RVT container, XmlNode tree)
    {
        XmlDocument old_output = t.output;
        XmlNode old_insert = t.insert;
        t.output = container.dom;
        t.insert = domDocument(container.dom);
        xsltApplyOneTemplate(t, t.node, tree, null);
        t.insert = old_insert;
        t.output = old_output;
        return xsltNewValueTree(container);
    }

    /** Port of xsltEvalVariable. */
    private static XPathResult xsltEvalVariable(TransformCtx t, StackElem variable, StylePreComp comp)
    {
        XPathResult result = null;
        if (variable == null) return null;
        XmlNode old_inst = t.inst;
        if (variable.select != null)
        {
            XPath xp_expr = null;
            StackElem old_var = t.context_variable;
            if (comp != null && comp.comp != null)
            {
                xp_expr = comp.comp;
            }
            else
            {
                xp_expr = Xslt.xpathCompile(t.style.settings, null, variable.select, false);
            }
            if (xp_expr == null) return null;
            long pos = t.xp_position, size = t.xp_size;
            t.context_variable = variable;
            variable.flags |= XSLT_VAR_IN_SELECT;
            result = xsltXpathEval(t, xp_expr, t.node);
            variable.flags ^= XSLT_VAR_IN_SELECT;
            t.context_variable = old_var;
            t.xp_position = pos;
            t.xp_size = size;
            if (result == null)
            {
                transformError(t, null, comp != null ? comp.inst : null,
                               fmt("Failed to evaluate the expression of variable '%s'.\n", variable.name));
                t.state = TransformCtx.STATE_STOPPED;
            }
        }
        else
        {
            if (variable.tree == null)
            {
                result = XPathResult.newString("");
            }
            else
            {
                StackElem old_var = t.context_variable;
                RVT container = xsltCreateRvt(t);
                variable.fragment = container;
                container.flag = RVT.LOCAL;
                t.context_variable = variable;
                result = xsltEvalContentInto(t, container, variable.tree);
                t.context_variable = old_var;
                if (result == null) result = XPathResult.newString("");
            }
        }
        t.inst = old_inst;
        return result;
    }

    /** Port of xsltEvalGlobalVariable. */
    private static XPathResult xsltEvalGlobalVariable(StackElem elem, TransformCtx t)
    {
        XPathResult result = null;
        if (elem == null) return null;
        if (elem.computed != 0) return elem.value;
        XmlNode old_inst = t.inst;
        StylePreComp comp = elem.comp;
        String old_var_name = elem.name;
        elem.name = XSLT_COMPUTING_GLOBAL_VAR_MARKER;
        StackElem old_ctxt_var = t.context_variable;
        t.context_variable = null;
        error:
        {
            if (elem.select != null)
            {
                XPath xp_expr = null;
                if (comp != null && comp.comp != null)
                {
                    xp_expr = comp.comp;
                }
                else
                {
                    xp_expr = Xslt.xpathCompile(t.style.settings, null, elem.select, false);
                }
                if (xp_expr == null) break error;
                t.inst = comp != null ? comp.inst : null;
                long pos = t.xp_position, size = t.xp_size;
                t.xp_size = 1;
                t.xp_position = 1;
                result = xsltXpathEval(t, xp_expr, t.initial_context_node);
                t.xp_position = pos;
                t.xp_size = size;
                if (result == null)
                {
                    transformError(t, null, comp != null ? comp.inst : null, fmt("Evaluating global variable %s failed\n", elem.name));
                    t.state = TransformCtx.STATE_STOPPED;
                    break error;
                }
                xsltFlagRvts(t, result, RVT.GLOBAL);
            }
            else
            {
                if (elem.tree == null)
                {
                    result = XPathResult.newString("");
                }
                else
                {
                    RVT container = xsltCreateRvt(t);
                    xsltRegisterPersistRvt(t, container);
                    result = xsltEvalContentInto(t, container, elem.tree);
                    if (result == null) result = XPathResult.newString("");
                }
            }
        }
        t.context_variable = old_ctxt_var;
        elem.name = old_var_name;
        t.inst = old_inst;
        if (result != null)
        {
            elem.value = result;
            elem.computed = 1;
        }
        return result;
    }

    /** Port of xsltEvalGlobalVariables. */
    private static int xsltEvalGlobalVariables(TransformCtx t)
    {
        StackElem head = null;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            for (StackElem elem = style.variables; elem != null; elem = elem.next)
            {
                String key = Xslt.key2(elem.name, elem.name_uri);
                StackElem def = t.global_vars.get(key);
                if (def == null)
                {
                    def = xsltCopyStackElem(elem);
                    t.global_vars.put(key, def);
                    def.next = head;
                    head = def;
                    // Keep all of them for freeing.
                    def.fragment = null;
                }
                else if (elem.comp != null && elem.comp.type == StylePreComp.FUNC_VARIABLE)
                {
                    if (elem.comp.inst != null && def.comp != null && def.comp.inst != null &&
                        nodeDom(elem.comp.inst) == nodeDom(def.comp.inst))
                    {
                        transformError(t, style, elem.comp.inst, fmt("Global variable %s already defined\n", elem.name));
                        style.errors++;
                    }
                }
            }
        }
        // Evaluate them in declaration order (the list is reversed).
        StackElem elem = head;
        while (elem != null)
        {
            StackElem next = elem.next;
            xsltEvalGlobalVariable(elem, t);
            elem.next = null;
            elem = next;
        }
        return 0;
    }

    /** Port of xsltBuildVariable. */
    private static StackElem xsltBuildVariable(TransformCtx t, StylePreComp comp, XmlNode tree)
    {
        StackElem elem = xsltNewStackElem();
        elem.comp = comp;
        elem.name = comp.name;
        elem.select = comp.select;
        elem.name_uri = comp.ns;
        elem.tree = tree;
        elem.value = xsltEvalVariable(t, elem, comp);
        elem.computed = 1;
        return elem;
    }

    /** Port of xsltRegisterVariable. */
    private static int xsltRegisterVariable(TransformCtx t, StylePreComp comp, XmlNode tree, int is_param)
    {
        int present = xsltCheckStackElem(t, comp.name, comp.ns);
        if (is_param == 0)
        {
            if (present != 0 && present != 3)
            {
                transformError(t, null, comp.inst, fmt("XSLT-variable: Redefinition of variable '%s'.\n", comp.name));
                return 0;
            }
        }
        else if (present != 0)
        {
            if (present == 1 || present == 2)
            {
                transformError(t, null, comp.inst, fmt("XSLT-param: Redefinition of parameter '%s'.\n", comp.name));
                return 0;
            }
            return 0;
        }
        StackElem variable = xsltBuildVariable(t, comp, tree);
        xsltAddStackElem(t, variable);
        return 0;
    }

    /** Port of xsltGlobalVariableLookup (the value is borrowed). */
    private static XPathResult xsltGlobalVariableLookup(TransformCtx t, String name, String ns_uri)
    {
        if (t.global_vars == null) return null;
        String key = Xslt.key2(name, ns_uri);
        StackElem elem = t.global_vars.get(key);
        if (elem == null) return null;
        if (elem.computed == 0)
        {
            if (elem.name == XSLT_COMPUTING_GLOBAL_VAR_MARKER)
            {
                transformError(t, null, elem.comp != null ? elem.comp.inst : null, fmt("Recursive definition of %s\n", name));
                return null;
            }
            return xsltEvalGlobalVariable(elem, t);
        }
        return elem.value;
    }

    /** Port of xsltParseStylesheetCallerParam (xsl:with-param). */
    private static StackElem xsltParseStylesheetCallerParam(TransformCtx t, XmlNode inst)
    {
        if (inst == null || nodeKind(inst) != ELEMENT) return null;
        Object ud = nodeUserData(inst);
        StylePreComp comp = precomp(inst);
        if (ud == null)
        {
            transformError(t, null, inst, "Internal error in xsltParseStylesheetCallerParam(): " +
                           "The XSLT 'with-param' instruction was not compiled.\n");
            return null;
        }
        if (comp == null || comp.name == null)
        {
            transformError(t, null, inst, "Internal error in xsltParseStylesheetCallerParam(): " +
                           "XSLT 'with-param': The attribute 'name' was not compiled.\n");
            return null;
        }
        XmlNode tree = comp.select != null ? inst : nodeFirstChild(inst);
        return xsltBuildVariable(t, comp, tree);
    }

    /** Port of xsltParseStylesheetVariable (a local xsl:variable). */
    private static void xsltParseStylesheetVariable(TransformCtx t, XmlNode inst)
    {
        if (inst == null || nodeKind(inst) != ELEMENT) return;
        Object ud = nodeUserData(inst);
        StylePreComp comp = precomp(inst);
        if (ud == null)
        {
            transformError(t, null, inst, "Internal error in xsltParseStylesheetVariable(): " +
                           "The XSLT 'variable' instruction was not compiled.\n");
            return;
        }
        if (comp == null || comp.name == null)
        {
            transformError(t, null, inst, "Internal error in xsltParseStylesheetVariable(): " +
                           "The attribute 'name' was not compiled.\n");
            return;
        }
        xsltRegisterVariable(t, comp, nodeFirstChild(inst), 0);
    }

    /** Port of xsltParseStylesheetParam (a template xsl:param). */
    private static void xsltParseStylesheetParam(TransformCtx t, XmlNode cur)
    {
        if (cur == null || nodeKind(cur) != ELEMENT) return;
        StylePreComp comp = precomp(cur);
        if (comp == null || comp.name == null)
        {
            transformError(t, null, cur, "Internal error in xsltParseStylesheetParam(): " +
                           "The XSLT 'param' declaration was not compiled correctly.\n");
            return;
        }
        xsltRegisterVariable(t, comp, nodeFirstChild(cur), 1);
    }

    /** Port of xsltXPathVariableLookup, the variable callback of the xpath context. Only the
        first element of each stack entry is looked at, like libxslt. */
    private static XPathResult xsltXpathVariableLookup(TransformCtx t, String name, String ns_uri)
    {
        if (name == null) return null;
        if (t.vars_nr != 0)
        {
            StackElem variable = null;
            for (int i = t.vars_nr; i > t.vars_base; i--)
            {
                StackElem cur = varsAt(t, i - 1);
                if (streq(cur.name, name) && streq(cur.name_uri, ns_uri))
                {
                    variable = cur;
                    break;
                }
            }
            if (variable != null)
            {
                if (variable.computed == 0)
                {
                    variable.value = xsltEvalVariable(t, variable, null);
                    variable.computed = 1;
                }
                return variable.value;
            }
        }
        XPathResult value = null;
        if (t.global_vars != null) value = xsltGlobalVariableLookup(t, name, ns_uri);
        if (value == null)
        {
            if (ns_uri != null) transformError(t, null, t.inst, fmt("Variable '{%s}%s' has not been declared.\n", ns_uri, name));
            else transformError(t, null, t.inst, fmt("Variable '%s' has not been declared.\n", name));
        }
        return value;
    }

    // Namespaces at runtime, port of libxslt namespaces.c //////////////

    /** The alias of a literal namespace over the import cascade, null if none (XSLT_UNDEFINED_DEFAULT_NS for #default). */
    private static String xsltLookupAlias(TransformCtx t, String href)
    {
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            if (style.ns_aliases != null && href != null)
            {
                String uri = style.ns_aliases.get(href);
                if (uri != null) return uri;
            }
        }
        return null;
    }

    /** Port of xsltGetSpecialNamespace: find or declare a namespace binding for ns_name and
        nsPrefix in scope of target (an element of the result). */
    private static XmlNs xsltGetSpecialNamespace(TransformCtx t, XmlNode invoc_node, String ns_name,
                                                 String ns_prefix, XmlNode target)
    {
        if (!lxIsElement(target)) return null;
        XmlNs ns;
        int prefix_occupied = 0;
        if (ns_prefix == null && (ns_name == null || ns_name.isEmpty()))
        {
            // Ensure that the default namespace is not in scope (an undeclaration if needed).
            for (XmlNs d : nodeNsDecls(target))
            {
                if (nsPrefix(d) == null)
                {
                    if (nsHref(d) != null && !nsHref(d).isEmpty())
                    {
                        transformError(t, null, invoc_node, fmt("Namespace normalization error: Cannot undeclare " +
                            "the default namespace, since the default namespace '%s' is already declared on the result element " +
                            "'%s'.\n", nsHref(d), nodeName(target)));
                    }
                    return null;
                }
            }
            XmlNode parent = lxParent(target);
            if (lxIsElement(parent))
            {
                if (nodeNs(parent) == null) return null;
                ns = lxSearchNs(parent, null);
                if (ns == null || nsHref(ns) == null || nsHref(ns).isEmpty()) return null;
                lxNewNs(target, "", null);
                return null;
            }
            return null;
        }
        if (ns_prefix != null && ns_prefix.equals("xml")) return lxSearchNs(target, ns_prefix);
        for (XmlNs d : nodeNsDecls(target))
        {
            if ((nsPrefix(d) == null) == (ns_prefix == null))
            {
                if (streq(nsPrefix(d), ns_prefix))
                {
                    if (streq(nsHref(d), ns_name)) return d;
                    prefix_occupied = 1;
                    break;
                }
            }
        }
        XmlNode parent = lxParent(target);
        declareNewPrefix:
        {
            if (prefix_occupied != 0)
            {
                ns = lxSearchNsByHref(target, ns_name);
                if (ns != null) return ns;
            }
            else if (lxIsElement(parent))
            {
                XmlNs pns = nodeNs(parent);
                if (pns != null && ((nsPrefix(pns) != null) == (ns_prefix != null)))
                {
                    if (ns_prefix == null)
                    {
                        if (streq(nsHref(pns), ns_name)) return pns;
                    }
                    else if (streq(nsPrefix(pns), ns_prefix) && streq(nsHref(pns), ns_name))
                    {
                        return pns;
                    }
                }
                ns = lxSearchNs(parent, ns_prefix);
                if (ns != null)
                {
                    if (streq(nsHref(ns), ns_name)) return ns;
                    // The prefix is bound to another namespace, check that no attribute of the target uses it.
                    for (XmlAttr attr : nodeAttrs(target))
                    {
                        XmlNs ans = nodeNs(attrNode(attr));
                        if (ans != null && streq(nsPrefix(ans), ns_prefix))
                        {
                            ns = lxSearchNsByHref(target, ns_name);
                            if (ns != null) return ns;
                            break declareNewPrefix;
                        }
                    }
                }
                return lxNewNs(target, ns_name, ns_prefix);
            }
            else
            {
                return lxNewNs(target, ns_name, ns_prefix);
            }
        }
        // declare_new_prefix:
        String pref;
        int counter = 1;
        if (ns_prefix == null) ns_prefix = "ns";
        do
        {
            pref = fmt("%s_%d", ns_prefix, counter++);
            if (pref.length() > 29) pref = pref.substring(0, 29);
            ns = lxSearchNs(target, pref);
            if (counter > 1000)
            {
                transformError(t, null, invoc_node, "Internal error in xsltAcquireResultInScopeNs(): " +
                               "Failed to compute a unique ns-prefix for the generated element");
                return null;
            }
        } while (ns != null);
        return lxNewNs(target, ns_name, pref);
    }

    /** Port of xsltGetNamespace: the namespace of a literal result element or attribute, with aliases applied. */
    private static XmlNs xsltGetNamespace(TransformCtx t, XmlNode cur, XmlNs ns, XmlNode out)
    {
        if (ns == null || cur == null || out == null) return null;
        String uri = xsltLookupAlias(t, nsHref(ns));
        if (uri != null && uri.equals(XSLT_UNDEFINED_DEFAULT_NS)) return xsltGetSpecialNamespace(t, cur, null, null, out);
        if (uri == null) uri = nsHref(ns);
        return xsltGetSpecialNamespace(t, cur, uri, nsPrefix(ns), out);
    }

    /** Port of xsltCopyNamespaceList: copy the declarations of a literal result element (not the
        xslt namespace, with aliases applied, only those not in scope). */
    private static void xsltCopyNamespaceList(TransformCtx t, XmlNode node, List<XmlNs> list)
    {
        if (!lxIsElement(node)) node = null;
        for (XmlNs cur : new ArrayList<>(list))
        {
            if (node != null)
            {
                XmlNs nns = nodeNs(node);
                if (nns != null && streq(nsPrefix(nns), nsPrefix(cur)) && streq(nsHref(nns), nsHref(cur))) continue;
                XmlNs tmp = lxSearchNs(node, nsPrefix(cur));
                if (tmp != null && streq(nsHref(tmp), nsHref(cur))) continue;
            }
            if (!streq(nsHref(cur), XSLT_NAMESPACE))
            {
                // Only the aliases of the top stylesheet (TODO in libxslt: apply cascading).
                String uri = (t.style.ns_aliases != null && nsHref(cur) != null) ? t.style.ns_aliases.get(nsHref(cur)) : null;
                if (uri != null && uri.equals(XSLT_UNDEFINED_DEFAULT_NS)) continue;
                if (node != null) lxNewNs(node, uri != null ? uri : nsHref(cur), nsPrefix(cur));
            }
        }
    }

    /** Port of xsltCopyNamespaceListInternal: copy declarations that are not in scope. */
    private static void xsltCopyNamespaceListInternal(XmlNode elem, List<XmlNs> list)
    {
        if (!lxIsElement(elem)) elem = null;
        for (XmlNs ns : new ArrayList<>(list))
        {
            if (elem != null)
            {
                XmlNs ens = nodeNs(elem);
                if (ens != null && streq(nsPrefix(ens), nsPrefix(ns)) && streq(nsHref(ens), nsHref(ns))) continue;
                XmlNs lu = lxSearchNs(elem, nsPrefix(ns));
                if (lu != null && streq(nsHref(lu), nsHref(ns))) continue;
                lxNewNs(elem, nsHref(ns), nsPrefix(ns));
            }
        }
    }

    // Copying, port of libxslt transform.c //////////////////////////////

    /** Is target an element whose text goes into cdata sections (xsl:output cdata-section-elements)? */
    private static boolean xsltIsCdataTarget(TransformCtx t, XmlNode target)
    {
        if (t.type != OUTPUT_XML || t.style.cdata_section == null || !lxIsElement(target)) return false;
        XmlNs ns = nodeNs(target);
        String key = Xslt.key2(nodeName(target), ns != null ? nsHref(ns) : null);
        return t.style.cdata_section.get(key) != null;
    }

    /** Port of xsltAddTextString: append the string to the text (or cdata) node target. */
    private static XmlNode xsltAddTextString(XmlNode target, String string)
    {
        if (string == null || string.isEmpty() || target == null) return target;
        nodeAppendContent(target, string);
        return target;
    }

    /** Port of xsltCopyTextString. */
    private static XmlNode xsltCopyTextString(TransformCtx t, XmlNode target, String string, int noescape)
    {
        if (string == null) return null;
        XmlNode copy;
        XmlNode last = target != null ? nodeLastChild(target) : null;
        if (xsltIsCdataTarget(t, target))
        {
            // Coalesce with a preceding cdata section.
            if (last != null && nodeKind(last) == CDATA) return xsltAddTextString(last, string);
            copy = nodeNewCdata(t.output, string);
        }
        else if (noescape != 0)
        {
            if (last != null && nodeKind(last) == TEXT && lxIsNoenc(last)) return xsltAddTextString(last, string);
            copy = nodeNewText(t.output, string);
            if (copy != null) nodeSetName(copy, "textnoenc");
        }
        else
        {
            if (last != null && nodeKind(last) == TEXT && !lxIsNoenc(last)) return xsltAddTextString(last, string);
            copy = nodeNewText(t.output, string);
        }
        if (copy != null && target != null) copy = lxAddChild(target, copy);
        if (copy == null) transformError(t, null, target, "xsltCopyTextString: text copy failed\n");
        return copy;
    }

    /** Port of xsltCopyText: copy a text or cdata node. */
    private static XmlNode xsltCopyText(TransformCtx t, XmlNode target, XmlNode cur)
    {
        int k = nodeKind(cur);
        if (k != TEXT && k != CDATA) return null;
        String content = nodeContent(cur);
        if (content == null) return null;
        XmlNode copy = null;
        XmlNode last = target != null ? nodeLastChild(target) : null;
        exit:
        {
            if (xsltIsCdataTarget(t, target))
            {
                if (last != null && nodeKind(last) == CDATA)
                {
                    copy = xsltAddTextString(last, content);
                    break exit;
                }
                copy = nodeNewCdata(t.output, content);
            }
            else if (last != null &&
                     ((nodeKind(last) == TEXT && k == TEXT && lxIsNoenc(last) == lxIsNoenc(cur)) ||
                      // non-escaped text nodes and cdata sections
                      (nodeKind(last) == CDATA && k == TEXT && lxIsNoenc(cur))))
            {
                copy = xsltAddTextString(last, content);
                break exit;
            }
            else
            {
                copy = nodeNewText(t.output, content);
                if (copy != null && k == TEXT && lxIsNoenc(cur)) nodeSetName(copy, "textnoenc");
            }
            if (copy != null)
            {
                if (target != null) copy = lxAddChild(target, copy);
            }
            else
            {
                transformError(t, null, target, "xsltCopyText: text copy failed\n");
            }
        }
        if (copy == null)
        {
            transformError(t, null, target, "Internal error in xsltCopyText(): Failed to copy the string.\n");
            t.state = TransformCtx.STATE_STOPPED;
        }
        return copy;
    }

    /** Port of xsltShallowCopyAttr. */
    private static XmlAttr xsltShallowCopyAttr(TransformCtx t, XmlNode invoc_node, XmlNode target, XmlNode attr)
    {
        if (attr == null) return null;
        if (!lxIsElement(target))
        {
            transformError(t, null, invoc_node, "Cannot add an attribute node to a non-element node.\n");
            return null;
        }
        if (nodeFirstChild(target) != null)
        {
            transformError(t, null, invoc_node, "Attribute nodes must be added before any child nodes to an element.\n");
            return null;
        }
        String value = lxAttrValue(attr);
        XmlAttr copy;
        XmlNs ans = nodeNs(attr);
        if (ans != null)
        {
            XmlNs ns = xsltGetSpecialNamespace(t, invoc_node, nsHref(ans), nsPrefix(ans), target);
            if (ns == null)
            {
                transformError(t, null, invoc_node, fmt("Namespace fixup error: Failed to acquire an in-scope " +
                               "namespace binding of the copied attribute '{%s}%s'.\n", nsHref(ans), nodeName(attr)));
            }
            copy = lxSetNsProp(target, ns, nodeName(attr), value);
        }
        else
        {
            copy = lxSetNsProp(target, null, nodeName(attr), value);
        }
        return copy;
    }

    /** Port of xsltCopyAttrListNoOverwrite. */
    private static int xsltCopyAttrListNoOverwrite(TransformCtx t, XmlNode invoc_node, XmlNode target, List<XmlAttr> attrs)
    {
        XmlNs orig_ns = null, copy_ns = null;
        for (XmlAttr attr : new ArrayList<>(attrs))
        {
            XmlNode an = attrNode(attr);
            XmlNs ans = nodeNs(an);
            if (ans != orig_ns)
            {
                orig_ns = ans;
                if (ans != null)
                {
                    copy_ns = xsltGetSpecialNamespace(t, invoc_node, nsHref(ans), nsPrefix(ans), target);
                    if (copy_ns == null) return -1;
                }
                else
                {
                    copy_ns = null;
                }
            }
            String value = lxAttrValue(an);
            lxNewNsProp(target, copy_ns, attrName(attr), value);
        }
        return 0;
    }

    /** Port of xsltShallowCopyElem. */
    private static XmlNode xsltShallowCopyElem(TransformCtx t, XmlNode node, XmlNode insert, int is_lre)
    {
        int k = nodeKind(node);
        if (k == DTD || insert == null) return null;
        if (k == TEXT || k == CDATA) return xsltCopyText(t, insert, node);
        XmlNode copy = lxShallowCopy(t.output, node);
        if (copy != null)
        {
            copy = lxAddChild(insert, copy);
            if (copy == null)
            {
                transformError(t, null, node, "xsltShallowCopyElem: copy failed\n");
                return copy;
            }
            if (k == ELEMENT)
            {
                List<XmlNs> defs = nodeNsDecls(node);
                if (!defs.isEmpty())
                {
                    if (is_lre != 0) xsltCopyNamespaceList(t, copy, defs);
                    else xsltCopyNamespaceListInternal(copy, defs);
                }
                XmlNs nns = nodeNs(node);
                if (nns != null)
                {
                    if (is_lre != 0) nodeSetNs(copy, xsltGetNamespace(t, node, nns, copy));
                    else nodeSetNs(copy, xsltGetSpecialNamespace(t, node, nsHref(nns), nsPrefix(nns), copy));
                }
                else if (lxIsElement(insert) && nodeNs(insert) != null)
                {
                    xsltGetSpecialNamespace(t, node, null, null, copy);
                }
            }
        }
        else
        {
            transformError(t, null, node, fmt("xsltShallowCopyElem: copy %s failed\n", nodeName(node)));
        }
        return copy;
    }

    /** Port of xsltCopyTreeList. */
    private static XmlNode xsltCopyTreeList(TransformCtx t, XmlNode invoc_node, XmlNode list, XmlNode insert,
                                            int is_lre, int top_elem_visited)
    {
        XmlNode ret = null;
        while (list != null)
        {
            XmlNode next = nodeNext(list);
            XmlNode copy = xsltCopyTree(t, invoc_node, list, insert, is_lre, top_elem_visited);
            if (copy != null && ret == null) ret = copy;
            list = next;
        }
        return ret;
    }

    /** Port of xsltShallowCopyNsNode: copy a namespace node as a declaration on insert. */
    private static XmlNs xsltShallowCopyNsNode(TransformCtx t, XmlNode invoc_node, XmlNode insert, XmlNode ns)
    {
        if (!lxIsElement(insert)) return null;
        if (nodeFirstChild(insert) != null)
        {
            transformError(t, null, invoc_node, "Namespace nodes must be added before any child nodes are added to an element.\n");
            return null;
        }
        // A namespace node has the prefix as name and the uri as content.
        String prefix = nodeName(ns);
        String href = nodeContent(ns);
        if (prefix == null)
        {
            if (nodeNs(insert) == null) return null;
        }
        else if (prefix.equals("xml"))
        {
            return null;
        }
        for (XmlNs tmp : nodeNsDecls(insert))
        {
            if ((nsPrefix(tmp) == null) == (prefix == null))
            {
                if (streq(nsPrefix(tmp), prefix))
                {
                    if (streq(nsHref(tmp), href)) return null;
                    return null; // occupied
                }
            }
        }
        XmlNs tmp = lxSearchNs(insert, prefix);
        if (tmp != null && streq(nsHref(tmp), href)) return null;
        return lxNewNs(insert, href, prefix);
    }

    /** Collect the namespaces in scope of node like xmlGetNsList: innermost first, unique prefixes. */
    private static List<XmlNs> lxGetNsList(XmlNode node)
    {
        List<XmlNs> list = new ArrayList<>();
        for (; node != null; node = nodeParent(node))
        {
            if (nodeKind(node) != ELEMENT) continue;
            for (XmlNs cur : nodeNsDecls(node))
            {
                int i;
                int n = list.size();
                for (i = 0; i < n; i++)
                {
                    if (streq(nsPrefix(cur), nsPrefix(list.get(i)))) break;
                }
                if (i >= n) list.add(cur);
            }
        }
        return list;
    }

    /** Port of xsltCopyTree: a deep copy of node into insert. */
    private static XmlNode xsltCopyTree(TransformCtx t, XmlNode invoc_node, XmlNode node, XmlNode insert,
                                        int is_lre, int top_elem_visited)
    {
        if (node == null) return null;
        switch (nodeKind(node))
        {
        case ELEMENT:
        case ENTITY_REF:
        case PI:
        case COMMENT:
        case DOCUMENT:
            break;
        case TEXT:
            return xsltCopyTextString(t, insert, nodeContent(node), lxIsNoenc(node) ? 1 : 0);
        case CDATA:
            return xsltCopyTextString(t, insert, nodeContent(node), 0);
        case ATTRIBUTE:
            return xsltShallowCopyAttr(t, invoc_node, insert, node) != null ? insert : null;
        case NAMESPACE:
            return xsltShallowCopyNsNode(t, invoc_node, insert, node) != null ? insert : null;
        default:
            return null;
        }
        if (nodeKind(node) == DOCUMENT)
        {
            // libxslt only copies the children of result tree fragments, xmlDocCopyNode of a document fails.
            if (isRtf(t, nodeDom(node)))
            {
                XmlNode c = nodeFirstChild(node);
                return c != null ? xsltCopyTreeList(t, invoc_node, c, insert, 0, 0) : null;
            }
            transformError(t, null, invoc_node, fmt("xsltCopyTree: Copying of '%s' failed.\n", "(null)"));
            return null;
        }
        XmlNode copy = lxShallowCopy(t.output, node);
        if (copy != null)
        {
            copy = lxAddChild(insert, copy);
            if (copy == null)
            {
                transformError(t, null, invoc_node, fmt("xsltCopyTree: Copying of '%s' failed.\n", nodeName(node)));
                return copy;
            }
            if (nodeLastChild(insert) != copy) return nodeLastChild(insert);
            if (nodeKind(node) == ELEMENT)
            {
                XmlNode parent = nodeParent(node);
                XmlNs nns = nodeNs(node);
                if (top_elem_visited == 0 && parent != null && nodeKind(parent) != DOCUMENT)
                {
                    // The top element of a copied subtree gets all namespaces in scope.
                    List<XmlNs> ns_list = lxGetNsList(node);
                    for (XmlNs in_scope : ns_list)
                    {
                        XmlNs ns = lxSearchNs(insert, nsPrefix(in_scope));
                        if (ns == null || !streq(nsHref(ns), nsHref(in_scope))) ns = null;
                        if (ns == null) ns = lxNewNs(copy, nsHref(in_scope), nsPrefix(in_scope));
                        if (nns == in_scope) nodeSetNs(copy, ns);
                    }
                }
                else if (!nodeNsDecls(node).isEmpty())
                {
                    if (is_lre != 0) xsltCopyNamespaceList(t, copy, nodeNsDecls(node));
                    else xsltCopyNamespaceListInternal(copy, nodeNsDecls(node));
                }
                if (nns != null)
                {
                    if (nodeNs(copy) == null)
                    {
                        nodeSetNs(copy, xsltGetSpecialNamespace(t, invoc_node, nsHref(nns), nsPrefix(nns), copy));
                    }
                }
                else if (lxIsElement(insert) && nodeNs(insert) != null)
                {
                    xsltGetSpecialNamespace(t, invoc_node, null, null, copy);
                }
                if (!nodeAttrs(node).isEmpty()) xsltCopyAttrListNoOverwrite(t, invoc_node, copy, nodeAttrs(node));
                if (top_elem_visited == 0) top_elem_visited = 1;
            }
            if (lxChildren(node) != null) xsltCopyTreeList(t, invoc_node, lxChildren(node), copy, is_lre, top_elem_visited);
        }
        else
        {
            transformError(t, null, invoc_node, fmt("xsltCopyTree: Copying of '%s' failed.\n", nodeName(node)));
        }
        return copy;
    }

    // Templates and attribute value templates, port of libxslt templates.c and attrvt.c

    /** Port of xsltEvalXPathStringNs, with the namespaces compiled into the expression. */
    private static String xsltEvalXpathString(TransformCtx t, XPath comp)
    {
        if (t.inst == null)
        {
            transformError(t, null, null, "xsltEvalXPathStringNs: No context or instruction\n");
            return null;
        }
        XmlNode old_inst = t.inst;
        long pos = t.xp_position, size = t.xp_size;
        String ret = null;
        XPathResult res = xsltXpathEval(t, comp, t.node);
        if (res != null)
        {
            ret = res.castToString();
        }
        else
        {
            t.state = TransformCtx.STATE_STOPPED;
        }
        t.inst = old_inst;
        t.xp_position = pos;
        t.xp_size = size;
        return ret;
    }

    /** Port of xsltEvalTemplateString: instantiate the content of inst and return its text. */
    private static String xsltEvalTemplateString(TransformCtx t, XmlNode context_node, XmlNode inst)
    {
        if (context_node == null || inst == null || nodeKind(inst) != ELEMENT) return null;
        if (nodeFirstChild(inst) == null) return null;
        // A temporary element to add the resulting text content to.
        XmlNode insert = nodeNewElement(t.output, "fake", null);
        XmlNode old_insert = t.insert;
        t.insert = insert;
        xsltApplyOneTemplate(t, context_node, nodeFirstChild(inst), null);
        t.insert = old_insert;
        String ret = lxNodeGetContent(insert);
        nodeFree(insert);
        return ret;
    }

    /** xmlStrncat of the characters of str from start to end. */
    private static String strncat(String a, String str, int start, int end)
    {
        if (end <= start) return a;
        String b = str.substring(start, end);
        return a == null ? b : a + b;
    }

    /** xmlStrcat: null a gives a copy of b (also of ""). */
    private static String strcat(String a, String b)
    {
        if (b == null) return a;
        if (a == null) return b;
        return a + b;
    }

    /** Port of xsltAttrTemplateValueProcessNode: evaluate the attribute value template str at runtime. */
    private static String xsltAttrTemplateValueProcessNode(TransformCtx t, String s, XmlNode inst)
    {
        String ret = null;
        if (s == null) return null;
        if (s.isEmpty()) return "";
        int n = s.length();
        int str = 0;
        int cur = 0;
        while (cur < n)
        {
            char c = s.charAt(cur);
            if (c == '{')
            {
                if (cur + 1 < n && s.charAt(cur + 1) == '{')
                {
                    // An escaped {.
                    cur++;
                    ret = strncat(ret, s, str, cur);
                    cur++;
                    str = cur;
                    continue;
                }
                ret = strncat(ret, s, str, cur);
                str = cur;
                cur++;
                while (cur < n && s.charAt(cur) != '}')
                {
                    // Need to check for literals (bug539741).
                    if (s.charAt(cur) == '\'' || s.charAt(cur) == '"')
                    {
                        char delim = s.charAt(cur++);
                        while (cur < n && s.charAt(cur) != delim) cur++;
                        if (cur < n) cur++;
                    }
                    else
                    {
                        cur++;
                    }
                }
                if (cur >= n)
                {
                    transformError(t, null, inst, "xsltAttrTemplateValueProcessNode: unmatched '{'\n");
                    ret = strncat(ret, s, str, cur);
                    return ret;
                }
                str++;
                String expr = s.substring(str, cur);
                if (expr.startsWith("{"))
                {
                    ret = strcat(ret, expr);
                }
                else
                {
                    XPath comp = Xslt.xpathCompile(t.style.settings, inst, expr, false);
                    String val = comp != null ? xsltEvalXpathString(t, comp) : null;
                    if (comp == null) t.state = TransformCtx.STATE_STOPPED;
                    if (val != null)
                    {
                        ret = strcat(ret, val);
                    }
                }
                cur++;
                str = cur;
            }
            else if (c == '}')
            {
                cur++;
                if (cur < n && s.charAt(cur) == '}')
                {
                    // An escaped }.
                    ret = strncat(ret, s, str, cur);
                    cur++;
                    str = cur;
                    continue;
                }
                else
                {
                    transformError(t, null, inst, "xsltAttrTemplateValueProcessNode: unmatched '}'\n");
                }
            }
            else
            {
                cur++;
            }
        }
        if (cur != str) ret = strncat(ret, s, str, cur);
        return ret;
    }

    /** Port of xsltEvalAttrValueTemplate: the attribute name (in namespace ns) of inst as an AVT. */
    private static String xsltEvalAttrValueTemplate(TransformCtx t, XmlNode inst, String name, String ns)
    {
        if (inst == null || name == null || nodeKind(inst) != ELEMENT) return null;
        String expr = Xslt.getNsProp(inst, name, ns);
        if (expr == null) return null;
        return xsltAttrTemplateValueProcessNode(t, expr, inst);
    }

    /** Port of xsltEvalAVT: evaluate a precompiled attribute value template. */
    private static String xsltEvalAvt(TransformCtx t, AttrVT avt, XmlNode node)
    {
        if (avt == null || node == null) return null;
        String ret = null;
        boolean str = avt.strstart != 0;
        for (int i = 0; i < avt.nb_seg; i++)
        {
            if (str)
            {
                ret = strcat(ret, (String)avt.segments.get(i));
            }
            else
            {
                XPath comp = (XPath)avt.segments.get(i);
                String tmp = null;
                if (comp != null) tmp = xsltEvalXpathString(t, comp);
                else t.state = TransformCtx.STATE_STOPPED;
                if (tmp != null)
                {
                    if (ret != null)
                    {
                        ret = strcat(ret, tmp);
                    }
                    else
                    {
                        ret = tmp;
                    }
                }
            }
            str = !str;
        }
        return ret;
    }

    /** Port of xsltAttrListTemplateProcess: the attributes of a literal result element. */
    private static void xsltAttrListTemplateProcess(TransformCtx t, XmlNode target, List<XmlAttr> attrs)
    {
        if (!lxIsElement(target) || attrs.isEmpty()) return;
        XmlNode old_insert = t.insert;
        t.insert = target;
        // Apply attribute sets.
        for (XmlAttr attr : new ArrayList<>(attrs))
        {
            XmlNs ans = nodeNs(attrNode(attr));
            if (ans != null && attrName(attr).equals("use-attribute-sets") && streq(nsHref(ans), XSLT_NAMESPACE))
            {
                xsltApplyAttributeSet(t, t.node, attrNode(attr), null);
            }
        }
        boolean has_attr = !nodeAttrs(target).isEmpty();
        // Instantiate the literal result element attributes.
        XmlNs orig_ns = null, copy_ns = null;
        error:
        for (XmlAttr attr : new ArrayList<>(attrs))
        {
            XmlNode an = attrNode(attr);
            XmlNs ans = nodeNs(an);
            if (ans != null && streq(nsHref(ans), XSLT_NAMESPACE)) continue;
            XmlNode first = nodeFirstChild(an);
            String value = "";
            if (first != null)
            {
                if (nodeKind(first) != TEXT || nodeNext(first) != null)
                {
                    transformError(t, null, attrOwner(attr), "Internal error: The children of an attribute node of a " +
                                   "literal result element are not in the expected form.\n");
                    break error;
                }
                value = nodeContent(first) != null ? nodeContent(first) : "";
            }
            // Get the namespace, avoid lookups of the same namespaces.
            if (ans != orig_ns)
            {
                orig_ns = ans;
                if (ans != null)
                {
                    copy_ns = xsltGetNamespace(t, attrOwner(attr), ans, target);
                    if (copy_ns == null) break error;
                }
                else
                {
                    copy_ns = null;
                }
            }
            String val;
            Object ud = nodeUserData(an);
            AttrVT avt = ud instanceof AttrVT a ? a : null;
            if (avt != null)
            {
                val = xsltEvalAvt(t, avt, attrOwner(attr));
                if (val == null)
                {
                    if (ans != null) transformError(t, null, attrOwner(attr), fmt("Internal error: Failed to evaluate the AVT " +
                                                    "of attribute '{%s}%s'.\n", nsHref(ans), attrName(attr)));
                    else transformError(t, null, attrOwner(attr), fmt("Internal error: Failed to evaluate the AVT " +
                                        "of attribute '%s'.\n", attrName(attr)));
                    // libxslt has created the attribute with an empty value before failing.
                    if (has_attr) lxSetNsProp(target, copy_ns, attrName(attr), "");
                    else lxNewNsProp(target, copy_ns, attrName(attr), "");
                    break error;
                }
            }
            else
            {
                val = value;
            }
            if (has_attr) lxSetNsProp(target, copy_ns, attrName(attr), val);
            else lxNewNsProp(target, copy_ns, attrName(attr), val);
        }
        t.insert = old_insert;
    }

    // Attributes, port of libxslt attributes.c (xsltAttribute, xsltApplyAttributeSet).

    /** Port of xsltAttribute (xsl:attribute). */
    private static void xsltAttribute(TransformCtx t, XmlNode context_node, XmlNode inst, StylePreComp comp)
    {
        if (context_node == null || inst == null || nodeKind(inst) != ELEMENT) return;
        if (comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltAttribute(): The XSLT 'attribute' instruction was not compiled.\n");
            return;
        }
        if (comp.has_name == 0) return;
        if (t.insert == null) return;
        XmlNode target_elem = t.insert;
        if (nodeKind(target_elem) != ELEMENT) return;
        if (nodeFirstChild(target_elem) != null)
        {
            transformError(t, null, inst, "xsl:attribute: Cannot add attributes to an element if children have been already added " +
                           "to the element.\n");
            return;
        }
        String name = null;
        String[] prefix = { null };
        String ns_name = null;
        if (comp.name == null)
        {
            String prop = xsltEvalAttrValueTemplate(t, inst, "name", XSLT_NAMESPACE);
            if (prop == null)
            {
                transformError(t, null, inst, "xsl:attribute: The attribute 'name' is missing.\n");
                return;
            }
            if (Xslt.validateQname(prop) != 0)
            {
                transformError(t, null, inst, fmt("xsl:attribute: The effective name '%s' is not a valid QName.\n", prop));
                // Fall through to catch any further errors, if possible.
            }
            if (prop.equals("xmlns"))
            {
                transformError(t, null, inst, "xsl:attribute: The effective name 'xmlns' is not allowed.\n");
                return;
            }
            name = Xslt.splitQname(prop, prefix);
        }
        else
        {
            name = Xslt.splitQname(comp.name, prefix);
        }
        if (comp.has_ns != 0)
        {
            if (comp.ns != null)
            {
                if (!comp.ns.isEmpty()) ns_name = comp.ns;
            }
            else
            {
                String tmp = xsltEvalAttrValueTemplate(t, inst, "namespace", XSLT_NAMESPACE);
                if (tmp != null && !tmp.isEmpty()) ns_name = tmp;
            }
            if (streq(ns_name, "http://www.w3.org/2000/xmlns/"))
            {
                transformError(t, null, inst, "xsl:attribute: Namespace http://www.w3.org/2000/xmlns/ forbidden.\n");
                return;
            }
            if (streq(ns_name, XML_NAMESPACE))
            {
                prefix[0] = "xml";
            }
            else if (streq(prefix[0], "xml"))
            {
                prefix[0] = null;
            }
        }
        else if (prefix[0] != null)
        {
            XmlNs ns = lxSearchNs(inst, prefix[0]);
            if (ns == null)
            {
                transformError(t, null, inst, fmt("xsl:attribute: The QName '%s:%s' has no namespace binding in scope in the stylesheet; " +
                               "this is an error, since the namespace was not specified by the instruction itself.\n", prefix[0], name));
            }
            else
            {
                ns_name = nsHref(ns);
            }
        }
        XmlNs ns = null;
        if (ns_name != null)
        {
            if (prefix[0] == null || prefix[0].equals("xmlns")) ns = xsltGetSpecialNamespace(t, inst, ns_name, "ns_1", target_elem);
            else ns = xsltGetSpecialNamespace(t, inst, ns_name, prefix[0], target_elem);
            if (ns == null)
            {
                transformError(t, null, inst, fmt("Namespace fixup error: Failed to acquire an in-scope " +
                               "namespace binding for the generated attribute '{%s}%s'.\n", ns_name, name));
                return;
            }
        }
        XmlNode first = nodeFirstChild(inst);
        if (first == null)
        {
            lxSetNsProp(t.insert, ns, name, "");
        }
        else if (nodeNext(first) == null && (nodeKind(first) == TEXT || nodeKind(first) == CDATA))
        {
            lxSetNsProp(t.insert, ns, name, nodeContent(first));
        }
        else
        {
            String value = xsltEvalTemplateString(t, context_node, inst);
            lxSetNsProp(t.insert, ns, name, value != null ? value : "");
        }
    }

    /** Port of xsltApplyAttributeSet: the attribute sets named in attr_sets (or in the
        xsl:use-attribute-sets attribute inst). */
    private static void xsltApplyAttributeSet(TransformCtx t, XmlNode node, XmlNode inst, String attr_sets)
    {
        if (attr_sets == null)
        {
            if (inst == null) return;
            if (nodeKind(inst) == ATTRIBUTE)
            {
                XmlNode first = nodeFirstChild(inst);
                if (first != null) attr_sets = nodeContent(first);
            }
            if (attr_sets == null) return;
        }
        // xmlSearchNs from an attribute starts at its element (and looks at the element's namespace).
        XmlNode ns_node = inst;
        int n = attr_sets.length();
        int curstr = 0;
        while (curstr < n)
        {
            while (curstr < n && Xslt.isBlank(attr_sets.charAt(curstr))) curstr++;
            if (curstr >= n) break;
            int endstr = curstr;
            while (endstr < n && !Xslt.isBlank(attr_sets.charAt(endstr))) endstr++;
            String qname = attr_sets.substring(curstr, endstr);
            String ns_uri = null;
            if (Xslt.validateQname(qname) != 0)
            {
                transformError(t, null, inst, fmt("The name '%s' in use-attribute-sets is not a valid QName.\n", qname));
                return;
            }
            String[] prefix = { null };
            String ncname = Xslt.splitQname(qname, prefix);
            if (prefix[0] != null)
            {
                XmlNs ns = lxSearchNs(ns_node, prefix[0]);
                if (ns == null)
                {
                    transformError(t, null, inst, fmt("use-attribute-set : No namespace found for QName '%s:%s'\n", prefix[0], ncname));
                    return;
                }
                ns_uri = nsHref(ns);
            }
            AttrSet set = null;
            if (t.style.attribute_sets != null)
            {
                String key = Xslt.key2(ncname, ns_uri);
                set = t.style.attribute_sets.get(key);
            }
            if (set != null)
            {
                for (AttrElem cur = set.attrs; cur != null; cur = cur.next)
                {
                    if (cur.attr != null)
                    {
                        Object ud = nodeUserData(cur.attr);
                        xsltAttribute(t, node, cur.attr, ud instanceof StylePreComp p ? p : null);
                    }
                }
            }
            curstr = endstr;
        }
    }

    // Template application, port of libxslt transform.c ////////////////

    private static void templPush(TransformCtx t, Template value)
    {
        if (t.templ_nr < t.templ_tab.size()) t.templ_tab.set(t.templ_nr, value);
        else t.templ_tab.add(value);
        t.templ_nr++;
        t.templ = value;
    }

    private static void templPop(TransformCtx t)
    {
        if (t.templ_nr <= 0) return;
        t.templ_nr--;
        t.templ = t.templ_nr > 0 ? t.templ_tab.get(t.templ_nr - 1) : null;
        t.templ_tab.set(t.templ_nr, null);
    }

    /** Port of xsltApplyFallbacks. */
    private static int xsltApplyFallbacks(TransformCtx t, XmlNode node, XmlNode inst)
    {
        int ret = 0;
        if (node == null || inst == null || nodeFirstChild(inst) == null) return 0;
        for (XmlNode child = nodeFirstChild(inst); child != null; child = nodeNext(child))
        {
            if (Xslt.isXsltElem(child) && Xslt.isXsltName(child, "fallback"))
            {
                ret++;
                xsltApplySequenceConstructor(t, node, nodeFirstChild(child), null);
            }
        }
        return ret;
    }

    /** Count the children libxslt counts as real nodes. */
    private static int xsltCountRealChildren(XmlNode node)
    {
        int n = 0;
        for (XmlNode cur = lxChildren(node); cur != null; cur = nodeNext(cur))
        {
            if (lxIsRealNode(cur)) n++;
        }
        return n;
    }

    /** Port of xsltDefaultProcessOneNode: the built in template rules. */
    private static void xsltDefaultProcessOneNode(TransformCtx t, XmlNode node, StackElem params)
    {
        if (t.state == TransformCtx.STATE_STOPPED) return;
        switch (nodeKind(node))
        {
        case DOCUMENT:
        case ELEMENT:
            break;
        case CDATA:
            if (xsltCopyText(t, t.insert, node) == null) transformError(t, null, node, "xsltDefaultProcessOneNode: cdata copy failed\n");
            return;
        case TEXT:
            if (xsltCopyText(t, t.insert, node) == null) transformError(t, null, node, "xsltDefaultProcessOneNode: text copy failed\n");
            return;
        case ATTRIBUTE:
        {
            XmlNode cur = nodeFirstChild(node);
            while (cur != null && nodeKind(cur) != TEXT) cur = nodeNext(cur);
            if (cur == null)
            {
                transformError(t, null, node, "xsltDefaultProcessOneNode: no text for attribute\n");
            }
            else if (xsltCopyText(t, t.insert, cur) == null)
            {
                transformError(t, null, node, "xsltDefaultProcessOneNode: text copy failed\n");
            }
            return;
        }
        default:
            return;
        }
        int nbchild = xsltCountRealChildren(node);
        long old_size = t.xp_size, old_pos = t.xp_position;
        int childno = 0;
        for (XmlNode cur = lxChildren(node); cur != null; cur = nodeNext(cur))
        {
            childno++;
            Template templ;
            switch (nodeKind(cur))
            {
            case DOCUMENT:
            case ELEMENT:
                t.xp_size = nbchild;
                t.xp_position = childno;
                if (t.depth >= t.max_template_depth)
                {
                    transformError(t, null, cur, fmt("xsltDefaultProcessOneNode: Maximum template depth exceeded.\n" +
                        "You can adjust xsltMaxDepth (--maxdepth) in order to raise the maximum number of nested template calls and " +
                        "variables/params (currently set to %d).\n", t.max_template_depth));
                    t.state = TransformCtx.STATE_STOPPED;
                    return;
                }
                t.depth++;
                xsltProcessOneNode(t, cur, params);
                t.depth--;
                break;
            case CDATA:
                templ = Xslt.getTemplate(t, cur, null);
                if (templ != null)
                {
                    xsltApplyXsltTemplate(t, cur, templ.content, templ, params);
                }
                else if (xsltCopyText(t, t.insert, cur) == null)
                {
                    transformError(t, null, cur, "xsltDefaultProcessOneNode: cdata copy failed\n");
                }
                break;
            case TEXT:
                templ = Xslt.getTemplate(t, cur, null);
                if (templ != null)
                {
                    t.xp_size = nbchild;
                    t.xp_position = childno;
                    xsltApplyXsltTemplate(t, cur, templ.content, templ, params);
                }
                else if (xsltCopyText(t, t.insert, cur) == null)
                {
                    transformError(t, null, cur, "xsltDefaultProcessOneNode: text copy failed\n");
                }
                break;
            case PI:
            case COMMENT:
                templ = Xslt.getTemplate(t, cur, null);
                if (templ != null)
                {
                    t.xp_size = nbchild;
                    t.xp_position = childno;
                    xsltApplyXsltTemplate(t, cur, templ.content, templ, params);
                }
                break;
            default:
                break;
            }
        }
        t.xp_size = old_size;
        t.xp_position = old_pos;
    }

    /** Port of xsltProcessOneNode. */
    private static void xsltProcessOneNode(TransformCtx t, XmlNode context_node, StackElem with_params)
    {
        Template templ = Xslt.getTemplate(t, context_node, null);
        if (templ == null)
        {
            XmlNode old_node = t.node;
            t.node = context_node;
            xsltDefaultProcessOneNode(t, context_node, with_params);
            t.node = old_node;
            return;
        }
        Template old_rule = t.current_template_rule;
        t.current_template_rule = templ;
        xsltApplyXsltTemplate(t, context_node, templ.content, templ, with_params);
        t.current_template_rule = old_rule;
    }

    /** Port of xsltApplyXSLTTemplate. */
    private static void xsltApplyXsltTemplate(TransformCtx t, XmlNode context_node, XmlNode list,
                                              Template templ, StackElem with_params)
    {
        if (templ == null)
        {
            transformError(t, null, list, "xsltApplyXSLTTemplate: Bad arguments; @templ is mandatory.\n");
            return;
        }
        if (list == null) return;
        if (t.state == TransformCtx.STATE_STOPPED) return;
        if (t.vars_nr >= t.max_template_vars)
        {
            transformError(t, null, list, fmt("xsltApplyXSLTTemplate: A potential infinite template recursion was detected.\n" +
                "You can adjust maxTemplateVars (--maxvars) in order to raise the maximum number of variables/params (currently set to %d).\n",
                t.max_template_vars));
            xsltDebug(t, context_node, list);
            t.state = TransformCtx.STATE_STOPPED;
            return;
        }
        RVT old_user_fragment_top = t.tmp_rvt;
        t.tmp_rvt = null;
        int old_vars_base = t.vars_base;
        t.vars_base = t.vars_nr;
        t.node = context_node;
        templPush(t, templ);
        // The template parameters.
        XmlNode cur = list;
        do
        {
            if (nodeKind(cur) == TEXT)
            {
                cur = nodeNext(cur);
                continue;
            }
            Object iparam = nodeUserData(cur);
            if (nodeKind(cur) != ELEMENT || iparam == null || !streq(nodeName(cur), "param") || !Xslt.isXsltElem(cur))
            {
                break;
            }
            list = nodeNext(cur);
            StackElem tmp_param = null;
            if (with_params != null && iparam instanceof StylePreComp ip)
            {
                for (tmp_param = with_params; tmp_param != null; tmp_param = tmp_param.next)
                {
                    if (streq(tmp_param.name, ip.name) && streq(tmp_param.name_uri, ip.ns))
                    {
                        xsltLocalVariablePush(t, tmp_param, -1);
                        break;
                    }
                }
            }
            if (tmp_param == null) xsltParseStylesheetParam(t, cur);
            cur = nodeNext(cur);
        } while (cur != null);
        xsltApplySequenceConstructor(t, context_node, list, templ);
        if (t.vars_nr > t.vars_base) xsltTemplateParamsCleanup(t);
        t.vars_base = old_vars_base;
        // Fragments created by extension functions in the template.
        while (t.tmp_rvt != null)
        {
            RVT tmp = t.tmp_rvt;
            t.tmp_rvt = tmp.next;
            xsltReleaseRvt(t, tmp);
        }
        t.tmp_rvt = old_user_fragment_top;
        templPop(t);
    }

    /** Port of xsltApplyOneTemplate. */
    private static void xsltApplyOneTemplate(TransformCtx t, XmlNode context_node, XmlNode list, StackElem params)
    {
        if (list == null) return;
        if (t.state == TransformCtx.STATE_STOPPED) return;
        if (params != null)
        {
            int old_vars_nr = t.vars_nr;
            for (; params != null; params = params.next) xsltLocalVariablePush(t, params, -1);
            xsltApplySequenceConstructor(t, context_node, list, null);
            xsltLocalVariablePop(t, old_vars_nr, -2);
        }
        else
        {
            xsltApplySequenceConstructor(t, context_node, list, null);
        }
    }

    // The instructions.

    /** Port of xsltSort: xsl:sort outside apply-templates and for-each. */
    private static void xsltSort(TransformCtx t, XmlNode inst, StylePreComp comp)
    {
        if (comp == null)
        {
            transformError(t, null, inst, "xsl:sort : compilation failed\n");
            return;
        }
        transformError(t, null, inst, "xsl:sort : improper use this should not be reached\n");
    }

    /** Port of xsltCopy (xsl:copy). */
    private static void xsltCopy(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        XmlNode old_insert = t.insert;
        if (t.insert != null)
        {
            switch (nodeKind(node))
            {
            case TEXT:
            case CDATA:
                xsltCopyText(t, t.insert, node);
                break;
            case DOCUMENT:
                break;
            case ELEMENT:
            {
                XmlNode copy = xsltShallowCopyElem(t, node, t.insert, 0);
                t.insert = copy;
                if (comp.use != null) xsltApplyAttributeSet(t, node, inst, comp.use);
                break;
            }
            case ATTRIBUTE:
                xsltShallowCopyAttr(t, inst, t.insert, node);
                break;
            case PI:
                lxAddChild(t.insert, nodeNewPi(t.output, nodeName(node), nodeContent(node)));
                break;
            case COMMENT:
                lxAddChild(t.insert, nodeNewComment(t.output, nodeContent(node)));
                break;
            case NAMESPACE:
                xsltShallowCopyNsNode(t, inst, t.insert, node);
                break;
            default:
                break;
            }
        }
        switch (nodeKind(node))
        {
        case DOCUMENT:
        case ELEMENT:
            xsltApplySequenceConstructor(t, t.node, nodeFirstChild(inst), null);
            break;
        default:
            break;
        }
        t.insert = old_insert;
    }

    /** Port of xsltText (xsl:text, only reached when it could not be replaced by its content). */
    private static void xsltText(TransformCtx t, XmlNode inst, StylePreComp comp)
    {
        if (nodeFirstChild(inst) == null || comp == null) return;
        for (XmlNode text = nodeFirstChild(inst); text != null; text = nodeNext(text))
        {
            if (nodeKind(text) != TEXT && nodeKind(text) != CDATA)
            {
                transformError(t, null, inst, "xsl:text content problem\n");
                break;
            }
            XmlNode copy = nodeNewText(t.output, nodeContent(text));
            if (nodeKind(text) != CDATA) nodeSetName(copy, "textnoenc");
            lxAddChild(t.insert, copy);
        }
    }

    /** Port of xsltElement (xsl:element). */
    private static void xsltElement(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (t.insert == null) return;
        if (comp.has_name == 0) return;
        XmlNode old_insert = t.insert;
        String name = null;
        String[] prefix = { null };
        String ns_name = null;
        error:
        {
            if (comp.name == null)
            {
                String prop = xsltEvalAttrValueTemplate(t, inst, "name", XSLT_NAMESPACE);
                if (prop == null)
                {
                    transformError(t, null, inst, "xsl:element: The attribute 'name' is missing.\n");
                    break error;
                }
                if (Xslt.validateQname(prop) != 0)
                {
                    transformError(t, null, inst, fmt("xsl:element: The effective name '%s' is not a valid QName.\n", prop));
                    // Fall through to catch any further errors, if possible.
                }
                name = Xslt.splitQname(prop, prefix);
            }
            else
            {
                name = Xslt.splitQname(comp.name, prefix);
            }
            XmlNode copy = nodeNewElement(t.output, name, null);
            copy = lxAddChild(t.insert, copy);
            if (copy == null)
            {
                transformError(t, null, inst, "xsl:element : xsltAddChild failed\n");
                break error;
            }
            if (comp.has_ns != 0)
            {
                if (comp.ns != null)
                {
                    if (!comp.ns.isEmpty()) ns_name = comp.ns;
                }
                else
                {
                    String tmp = xsltEvalAttrValueTemplate(t, inst, "namespace", XSLT_NAMESPACE);
                    if (tmp != null && !tmp.isEmpty()) ns_name = tmp;
                }
                if (streq(ns_name, "http://www.w3.org/2000/xmlns/"))
                {
                    transformError(t, null, inst, "xsl:attribute: Namespace http://www.w3.org/2000/xmlns/ forbidden.\n");
                    break error;
                }
                if (streq(ns_name, XML_NAMESPACE))
                {
                    prefix[0] = "xml";
                }
                else if (streq(prefix[0], "xml"))
                {
                    prefix[0] = null;
                }
            }
            else
            {
                XmlNs ns = lxSearchNs(inst, prefix[0]);
                if (ns == null)
                {
                    if (prefix[0] != null)
                    {
                        transformError(t, null, inst, fmt("xsl:element: The QName '%s:%s' has no namespace binding in scope in the stylesheet; " +
                                       "this is an error, since the namespace was not specified by the instruction itself.\n", prefix[0], name));
                    }
                }
                else
                {
                    ns_name = nsHref(ns);
                }
            }
            if (ns_name != null)
            {
                // Do not use a prefix of "xmlns".
                if (streq(prefix[0], "xmlns")) nodeSetNs(copy, xsltGetSpecialNamespace(t, inst, ns_name, "ns_1", copy));
                else nodeSetNs(copy, xsltGetSpecialNamespace(t, inst, ns_name, prefix[0], copy));
            }
            else if (lxIsElement(nodeParent(copy)) && nodeNs(nodeParent(copy)) != null)
            {
                xsltGetSpecialNamespace(t, inst, null, null, copy);
            }
            t.insert = copy;
            if (comp.has_use != 0)
            {
                if (comp.use != null)
                {
                    xsltApplyAttributeSet(t, node, inst, comp.use);
                }
                else
                {
                    String attr_sets = xsltEvalAttrValueTemplate(t, inst, "use-attribute-sets", null);
                    if (attr_sets != null)
                    {
                        xsltApplyAttributeSet(t, node, inst, attr_sets);
                    }
                }
            }
            if (nodeFirstChild(inst) != null) xsltApplySequenceConstructor(t, t.node, nodeFirstChild(inst), null);
        }
        t.insert = old_insert;
    }

    /** Port of xsltComment (xsl:comment). */
    private static void xsltComment(TransformCtx t, XmlNode node, XmlNode inst)
    {
        String value = xsltEvalTemplateString(t, node, inst);
        int len = value != null ? value.length() : 0;
        if (len > 0)
        {
            if (value.charAt(len - 1) == '-' || value.contains("--"))
            {
                transformError(t, null, inst, "xsl:comment : '--' or ending '-' not allowed in comment\n");
                // Fall through to try to catch further errors.
            }
        }
        lxAddChild(t.insert, nodeNewComment(t.output, value));
    }

    /** Port of xsltProcessingInstruction (xsl:processing-instruction). */
    private static void xsltProcessingInstruction(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (t.insert == null) return;
        if (comp.has_name == 0) return;
        String name;
        String value = null;
        if (comp.name == null)
        {
            name = xsltEvalAttrValueTemplate(t, inst, "name", null);
            if (name == null)
            {
                transformError(t, null, inst, "xsl:processing-instruction : name is missing\n");
                return;
            }
        }
        else
        {
            name = comp.name;
        }
        value = xsltEvalTemplateString(t, node, inst);
        if (value != null && value.contains("?>"))
        {
            transformError(t, null, inst, "xsl:processing-instruction: '?>' not allowed within PI content\n");
            return;
        }
        lxAddChild(t.insert, nodeNewPi(t.output, name, value));
    }

    /** Port of xsltCopyOf (xsl:copy-of). */
    private static void xsltCopyOf(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (node == null || inst == null) return;
        if (comp == null || comp.select == null || comp.comp == null)
        {
            transformError(t, null, inst, "xsl:copy-of : compilation failed\n");
            return;
        }
        XPathResult res = xsltPreCompEval(t, node, comp);
        if (res != null)
        {
            if (res.type() == XPathResult.NODESET && !res.isTree())
            {
                int n = res.size();
                for (int i = 0; i < n; i++)
                {
                    XmlNode cur = res.node(i);
                    if (cur == null) continue;
                    if (nodeKind(cur) == DOCUMENT)
                    {
                        xsltCopyTreeList(t, inst, nodeFirstChild(cur), t.insert, 0, 0);
                    }
                    else if (nodeKind(cur) == ATTRIBUTE)
                    {
                        xsltShallowCopyAttr(t, inst, t.insert, cur);
                    }
                    else
                    {
                        xsltCopyTree(t, inst, cur, t.insert, 0, 0);
                    }
                }
            }
            else if (res.isTree())
            {
                XmlNode first = res.size() != 0 ? res.node(0) : null;
                if (first != null && lxIsRealNode(first)) xsltCopyTreeList(t, inst, lxChildren(first), t.insert, 0, 0);
            }
            else
            {
                String value = res.castToString();
                if (value == null)
                {
                    transformError(t, null, inst, "Internal error in xsltCopyOf(): failed to cast an XPath object to string.\n");
                    t.state = TransformCtx.STATE_STOPPED;
                }
                else
                {
                    if (!value.isEmpty()) xsltCopyTextString(t, t.insert, value, 0);
                }
            }
        }
        else
        {
            t.state = TransformCtx.STATE_STOPPED;
        }
    }

    /** Port of xsltValueOf (xsl:value-of). */
    private static void xsltValueOf(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (node == null || inst == null) return;
        if (comp == null || comp.select == null || comp.comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltValueOf(): The XSLT 'value-of' instruction was not compiled.\n");
            return;
        }
        String value = null;
        XPathResult res = xsltPreCompEval(t, node, comp);
        if (res != null)
        {
            value = res.castToString();
            if (value == null)
            {
                transformError(t, null, inst, "Internal error in xsltValueOf(): failed to cast an XPath object to string.\n");
                t.state = TransformCtx.STATE_STOPPED;
                return;
            }
            if (!value.isEmpty()) xsltCopyTextString(t, t.insert, value, comp.noescape);
        }
        else
        {
            transformError(t, null, inst, "XPath evaluation returned no result.\n");
            t.state = TransformCtx.STATE_STOPPED;
        }
    }

    /** Port of xsltNumber (xsl:number). */
    private static void xsltNumber(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (comp == null)
        {
            transformError(t, null, inst, "xsl:number : compilation failed\n");
            return;
        }
        if (node == null || inst == null) return;
        comp.numdata.doc = nodeDom(inst);
        comp.numdata.node = inst;
        xsltNumberFormat(t, comp.numdata, node);
    }

    /** Port of xsltApplyImports (xsl:apply-imports). */
    private static void xsltApplyImports(TransformCtx t, XmlNode context_node, XmlNode inst, StylePreComp comp)
    {
        if (inst == null) return;
        if (comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltApplyImports(): The XSLT 'apply-imports' instruction was not compiled.\n");
            return;
        }
        if (t.current_template_rule == null)
        {
            transformError(t, null, inst, "It is an error to call 'apply-imports' when there's no current template rule.\n");
            return;
        }
        Template templ = Xslt.getTemplate(t, context_node, t.current_template_rule.style);
        if (templ != null)
        {
            Template old_rule = t.current_template_rule;
            t.current_template_rule = templ;
            xsltApplyXsltTemplate(t, context_node, templ.content, templ, null);
            t.current_template_rule = old_rule;
        }
        else
        {
            // Use the built in templates.
            xsltDefaultProcessOneNode(t, context_node, null);
        }
    }

    /** Port of xsltFindTemplate: a named template over the import cascade. */
    private static Template xsltFindTemplate(TransformCtx t, String name, String name_uri)
    {
        if (name == null) return null;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            if (style.named_templates != null)
            {
                String key = Xslt.hash3Key(name, name_uri, null);
                Template cur = style.named_templates.get(key);
                if (cur != null) return cur;
            }
        }
        return null;
    }

    /** Port of xsltCallTemplate (xsl:call-template). */
    private static void xsltCallTemplate(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        StackElem with_params = null;
        if (t.insert == null) return;
        if (comp == null)
        {
            transformError(t, null, inst, "The XSLT 'call-template' instruction was not compiled.\n");
            return;
        }
        if (comp.templ == null)
        {
            comp.templ = xsltFindTemplate(t, comp.name, comp.ns);
            if (comp.templ == null)
            {
                if (comp.ns != null) transformError(t, null, inst, fmt("The called template '{%s}%s' was not found.\n", comp.ns, comp.name));
                else transformError(t, null, inst, fmt("The called template '%s' was not found.\n", comp.name));
                return;
            }
        }
        for (XmlNode cur = nodeFirstChild(inst); cur != null; cur = nodeNext(cur))
        {
            if (t.state == TransformCtx.STATE_STOPPED) break;
            if (Xslt.isXsltElem(cur))
            {
                if (Xslt.isXsltName(cur, "with-param"))
                {
                    StackElem param = xsltParseStylesheetCallerParam(t, cur);
                    if (param != null)
                    {
                        param.next = with_params;
                        with_params = param;
                    }
                }
                else
                {
                    genericError(t.style.settings, fmt("xsl:call-template: misplaced xsl:%s\n", nodeName(cur)));
                }
            }
            else
            {
                genericError(t.style.settings, fmt("xsl:call-template: misplaced %s element\n",
                             Xslt.libxml2NodeName(cur) != null ? Xslt.libxml2NodeName(cur) : "(null)"));
            }
        }
        xsltApplyXsltTemplate(t, node, comp.templ.content, comp.templ, with_params);
        if (with_params != null) xsltFreeRuntimeStackElemList(t, with_params);
    }

    private static NodeList xsltNodeListFrom(XPathResult res)
    {
        NodeList list = new NodeList();
        int n = res != null ? res.size() : 0;
        list.nodes = new XmlNode[n];
        for (int i = 0; i < n; ++i) list.nodes[i] = res.node(i);
        list.nr = n;
        return list;
    }

    /** Port of xsltApplyTemplates (xsl:apply-templates). */
    private static void xsltApplyTemplates(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (comp == null)
        {
            transformError(t, null, inst, "xsl:apply-templates : compilation failed\n");
            return;
        }
        if (node == null || inst == null) return;
        NodeList list = null;
        StackElem with_params = null;
        XmlNode old_context_node = t.node;
        String old_mode = t.mode, old_mode_uri = t.mode_uri;
        DocInfo old_doc_info = t.document;
        NodeList old_list = t.node_list;
        long old_size = t.xp_size, old_pos = t.xp_position;
        t.mode = comp.mode;
        t.mode_uri = comp.mode_uri;
        error:
        {
            if (comp.select != null)
            {
                if (comp.comp == null)
                {
                    transformError(t, null, inst, "xsl:apply-templates : compilation failed\n");
                    break error;
                }
                XPathResult res = xsltPreCompEval(t, node, comp);
                if (res != null)
                {
                    if (res.type() == XPathResult.NODESET && !res.isTree())
                    {
                        list = xsltNodeListFrom(res);
                    }
                    else
                    {
                        transformError(t, null, inst, "The 'select' expression did not evaluate to a node set.\n");
                        t.state = TransformCtx.STATE_STOPPED;
                        break error;
                    }
                }
                else
                {
                    transformError(t, null, inst, "Failed to evaluate the 'select' expression.\n");
                    t.state = TransformCtx.STATE_STOPPED;
                    break error;
                }
            }
            else
            {
                list = new NodeList();
                int n = 0;
                for (XmlNode cur = lxChildren(node); cur != null; cur = nodeNext(cur)) if (lxIsRealNode(cur)) n++;
                list.nodes = new XmlNode[n];
                for (XmlNode cur = lxChildren(node); cur != null; cur = nodeNext(cur)) if (lxIsRealNode(cur)) list.nodes[list.nr++] = cur;
            }
            if (list == null || list.nr == 0) break error;
            t.node_list = list;
            // Parameters and sorts.
            for (XmlNode cur = nodeFirstChild(inst); cur != null; )
            {
                if (t.state == TransformCtx.STATE_STOPPED) break;
                if (nodeKind(cur) == TEXT)
                {
                    cur = nodeNext(cur);
                    continue;
                }
                if (!Xslt.isXsltElem(cur)) break;
                if (Xslt.isXsltName(cur, "with-param"))
                {
                    StackElem param = xsltParseStylesheetCallerParam(t, cur);
                    if (param != null)
                    {
                        param.next = with_params;
                        with_params = param;
                    }
                }
                if (Xslt.isXsltName(cur, "sort"))
                {
                    Template old_rule = t.current_template_rule;
                    int nbsorts = 0;
                    XmlNode[] sorts = new XmlNode[XSLT_MAX_SORT];
                    sorts[nbsorts++] = cur;
                    cur = nodeNext(cur);
                    while (cur != null)
                    {
                        if (t.state == TransformCtx.STATE_STOPPED) break;
                        if (nodeKind(cur) == TEXT)
                        {
                            cur = nodeNext(cur);
                            continue;
                        }
                        if (!Xslt.isXsltElem(cur)) break;
                        if (Xslt.isXsltName(cur, "with-param"))
                        {
                            StackElem param = xsltParseStylesheetCallerParam(t, cur);
                            if (param != null)
                            {
                                param.next = with_params;
                                with_params = param;
                            }
                        }
                        if (Xslt.isXsltName(cur, "sort"))
                        {
                            if (nbsorts >= XSLT_MAX_SORT)
                            {
                                transformError(t, null, cur, fmt("The number (%d) of xsl:sort instructions exceeds the " +
                                               "maximum allowed by this processor's settings.\n", nbsorts));
                                t.state = TransformCtx.STATE_STOPPED;
                                break;
                            }
                            sorts[nbsorts++] = cur;
                        }
                        cur = nodeNext(cur);
                    }
                    t.current_template_rule = null;
                    xsltDoSortFunction(t, sorts, nbsorts);
                    t.current_template_rule = old_rule;
                    break;
                }
                cur = nodeNext(cur);
            }
            t.xp_size = list.nr;
            for (int i = 0; i < list.nr; i++)
            {
                XmlNode cur = list.nodes[i];
                t.node = cur;
                t.xp_position = i + 1;
                xsltProcessOneNode(t, cur, with_params);
            }
        }
        if (with_params != null) xsltFreeRuntimeStackElemList(t, with_params);
        t.xp_size = old_size;
        t.xp_position = old_pos;
        t.document = old_doc_info;
        t.node_list = old_list;
        t.node = old_context_node;
        t.mode = old_mode;
        t.mode_uri = old_mode_uri;
    }

    /** Port of xsltChoose (xsl:choose). */
    private static void xsltChoose(TransformCtx t, XmlNode context_node, XmlNode inst)
    {
        if (context_node == null || inst == null) return;
        XmlNode cur = nodeFirstChild(inst);
        if (cur == null)
        {
            transformError(t, null, inst, "xsl:choose: The instruction has no content.\n");
            return;
        }
        if (!Xslt.isXsltElem(cur) || !Xslt.isXsltName(cur, "when"))
        {
            transformError(t, null, inst, "xsl:choose: xsl:when expected first\n");
            return;
        }
        processSequence:
        {
            while (cur != null && Xslt.isXsltElem(cur) && Xslt.isXsltName(cur, "when"))
            {
                StylePreComp wcomp = precomp(cur);
                if (wcomp == null || wcomp.test == null || wcomp.comp == null)
                {
                    transformError(t, null, cur, "Internal error in xsltChoose(): The XSLT 'when' instruction was not compiled.\n");
                    return;
                }
                int res = xsltPreCompEvalToBoolean(t, context_node, wcomp);
                if (res == -1)
                {
                    t.state = TransformCtx.STATE_STOPPED;
                    return;
                }
                if (res == 1) break processSequence;
                cur = nodeNext(cur);
            }
            if (cur != null && Xslt.isXsltElem(cur) && Xslt.isXsltName(cur, "otherwise")) break processSequence;
            return;
        }
        xsltApplySequenceConstructor(t, t.node, nodeFirstChild(cur), null);
    }

    /** Port of xsltIf (xsl:if). */
    private static void xsltIf(TransformCtx t, XmlNode context_node, XmlNode inst, StylePreComp comp)
    {
        if (context_node == null || inst == null) return;
        if (comp == null || comp.test == null || comp.comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltIf(): The XSLT 'if' instruction was not compiled.\n");
            return;
        }
        RVT old_local_fragment_top = t.local_rvt;
        int res = xsltPreCompEvalToBoolean(t, context_node, comp);
        if (old_local_fragment_top != t.local_rvt) xsltReleaseLocalRvts(t, old_local_fragment_top);
        if (res == -1)
        {
            t.state = TransformCtx.STATE_STOPPED;
            return;
        }
        if (res == 1) xsltApplySequenceConstructor(t, context_node, nodeFirstChild(inst), null);
    }

    /** Port of xsltForEach (xsl:for-each). */
    private static void xsltForEach(TransformCtx t, XmlNode context_node, XmlNode inst, StylePreComp comp)
    {
        if (context_node == null || inst == null)
        {
            genericError(t.style.settings, "xsltForEach(): Bad arguments.\n");
            return;
        }
        if (comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltForEach(): The XSLT 'for-each' instruction was not compiled.\n");
            return;
        }
        if (comp.select == null || comp.comp == null)
        {
            transformError(t, null, inst, "Internal error in xsltForEach(): The selecting expression of the XSLT 'for-each' " +
                           "instruction was not compiled correctly.\n");
            return;
        }
        DocInfo old_doc_info = t.document;
        NodeList old_list = t.node_list;
        XmlNode old_context_node = t.node;
        Template old_rule = t.current_template_rule;
        t.current_template_rule = null;
        long old_size = t.xp_size, old_pos = t.xp_position;
        NodeList list = null;
        error:
        {
            XPathResult res = xsltPreCompEval(t, context_node, comp);
            if (res != null)
            {
                if (res.type() == XPathResult.NODESET && !res.isTree())
                {
                    list = xsltNodeListFrom(res);
                }
                else
                {
                    transformError(t, null, inst, "The 'select' expression does not evaluate to a node set.\n");
                    break error;
                }
            }
            else
            {
                transformError(t, null, inst, "Failed to evaluate the 'select' expression.\n");
                t.state = TransformCtx.STATE_STOPPED;
                break error;
            }
            if (list == null || list.nr <= 0) break error;
            t.node_list = list;
            XmlNode cur_inst = nodeFirstChild(inst);
            if (cur_inst != null && Xslt.isXsltElem(cur_inst) && Xslt.isXsltName(cur_inst, "sort"))
            {
                int nbsorts = 0;
                XmlNode[] sorts = new XmlNode[XSLT_MAX_SORT];
                sorts[nbsorts++] = cur_inst;
                cur_inst = nodeNext(cur_inst);
                while (cur_inst != null && Xslt.isXsltElem(cur_inst) && Xslt.isXsltName(cur_inst, "sort"))
                {
                    if (nbsorts >= XSLT_MAX_SORT)
                    {
                        transformError(t, null, cur_inst, fmt("The number of xsl:sort instructions exceeds the " +
                                       "maximum (%d) allowed by this processor.\n", XSLT_MAX_SORT));
                        break error;
                    }
                    sorts[nbsorts++] = cur_inst;
                    cur_inst = nodeNext(cur_inst);
                }
                xsltDoSortFunction(t, sorts, nbsorts);
            }
            t.xp_size = list.nr;
            for (int i = 0; i < list.nr; i++)
            {
                XmlNode cur = list.nodes[i];
                t.node = cur;
                t.xp_position = i + 1;
                xsltApplySequenceConstructor(t, cur, cur_inst, null);
            }
        }
        t.document = old_doc_info;
        t.node_list = old_list;
        t.node = old_context_node;
        t.current_template_rule = old_rule;
        t.xp_size = old_size;
        t.xp_position = old_pos;
    }

    /** Run the function of a precomputed instruction (comp->func in libxslt). Returns false if there is none. */
    private static boolean xsltCallFunc(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        switch (comp.type)
        {
        case StylePreComp.FUNC_COPY: xsltCopy(t, node, inst, comp); return true;
        case StylePreComp.FUNC_SORT: xsltSort(t, inst, comp); return true;
        case StylePreComp.FUNC_TEXT: xsltText(t, inst, comp); return true;
        case StylePreComp.FUNC_ELEMENT: xsltElement(t, node, inst, comp); return true;
        case StylePreComp.FUNC_ATTRIBUTE: xsltAttribute(t, node, inst, comp); return true;
        case StylePreComp.FUNC_COMMENT: xsltComment(t, node, inst); return true;
        case StylePreComp.FUNC_PI: xsltProcessingInstruction(t, node, inst, comp); return true;
        case StylePreComp.FUNC_COPYOF: xsltCopyOf(t, node, inst, comp); return true;
        case StylePreComp.FUNC_VALUEOF: xsltValueOf(t, node, inst, comp); return true;
        case StylePreComp.FUNC_NUMBER: xsltNumber(t, node, inst, comp); return true;
        case StylePreComp.FUNC_APPLYIMPORTS: xsltApplyImports(t, node, inst, comp); return true;
        case StylePreComp.FUNC_CALLTEMPLATE: xsltCallTemplate(t, node, inst, comp); return true;
        case StylePreComp.FUNC_APPLYTEMPLATES: xsltApplyTemplates(t, node, inst, comp); return true;
        case StylePreComp.FUNC_CHOOSE: xsltChoose(t, node, inst); return true;
        case StylePreComp.FUNC_IF: xsltIf(t, node, inst, comp); return true;
        case StylePreComp.FUNC_FOREACH: xsltForEach(t, node, inst, comp); return true;
        case StylePreComp.FUNC_DOCUMENT: xsltDocumentElem(t, node, inst, comp); return true;
        case StylePreComp.FUNC_EXTENSION:
            if (comp.ext == StylePreComp.EXT_DEBUG)
            {
                xsltDebug(t, node, inst);
                return true;
            }
            if (comp.ext == StylePreComp.EXT_DOCUMENT)
            {
                xsltDocumentElem(t, node, inst, comp);
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    /** Port of xsltApplySequenceConstructor: instantiate the nodes of list. */
    private static void xsltApplySequenceConstructor(TransformCtx t, XmlNode context_node, XmlNode list, Template templ)
    {
        if (list == null) return;
        if (t.state == TransformCtx.STATE_STOPPED) return;
        if (t.depth >= t.max_template_depth)
        {
            transformError(t, null, list, fmt("xsltApplySequenceConstructor: A potential infinite template recursion was detected.\n" +
                "You can adjust xsltMaxDepth (--maxdepth) in order to raise the maximum number of nested template calls and " +
                "variables/params (currently set to %d).\n", t.max_template_depth));
            xsltDebug(t, context_node, list);
            t.state = TransformCtx.STATE_STOPPED;
            return;
        }
        t.depth++;
        RVT old_local_fragment_top = t.local_rvt;
        XmlNode old_insert = t.insert, insert = t.insert;
        XmlNode old_inst = t.inst, old_cur_inst;
        XmlNode old_context_node = t.node;
        int old_vars_nr = t.vars_nr;
        int level = 0;
        XmlNode list_parent = nodeParent(list);
        XmlNode cur = list;
        XmlNode copy = null;
        error:
        while (cur != null)
        {
            if (t.op_limit != 0)
            {
                if (Long.compareUnsigned(t.op_count, t.op_limit) >= 0)
                {
                    transformError(t, null, cur, "xsltApplySequenceConstructor: Operation limit exceeded\n");
                    t.state = TransformCtx.STATE_STOPPED;
                    break error;
                }
                t.op_count += 1;
            }
            t.inst = cur;
            if (insert == null) break error;
            copy = null;
            skipChildren:
            {
                if (Xslt.isXsltElem(cur))
                {
                    Object ud = nodeUserData(cur);
                    if (ud == null)
                    {
                        if (Xslt.isXsltName(cur, "message"))
                        {
                            xsltMessage(t, context_node, cur);
                        }
                        else
                        {
                            // Something really went wrong, try the fallbacks.
                            t.insert = insert;
                            if (xsltApplyFallbacks(t, context_node, cur) == 0)
                            {
                                genericError(t.style.settings, fmt("xsltApplySequenceConstructor: %s was not compiled\n", nodeName(cur)));
                            }
                            t.insert = old_insert;
                        }
                        break skipChildren;
                    }
                    if (ud instanceof StylePreComp info)
                    {
                        old_cur_inst = t.inst;
                        t.inst = cur;
                        t.insert = insert;
                        boolean done = xsltCallFunc(t, context_node, cur, info);
                        if (done)
                        {
                            if (old_local_fragment_top != t.local_rvt) xsltReleaseLocalRvts(t, old_local_fragment_top);
                            t.insert = old_insert;
                            t.inst = old_cur_inst;
                            break skipChildren;
                        }
                        t.insert = old_insert;
                        t.inst = old_cur_inst;
                    }
                    if (Xslt.isXsltName(cur, "variable"))
                    {
                        StackElem tmpvar = t.vars;
                        old_cur_inst = t.inst;
                        t.inst = cur;
                        xsltParseStylesheetVariable(t, cur);
                        t.inst = old_cur_inst;
                        if (tmpvar != t.vars)
                        {
                            // The variable was registered, it lives until the end of its parent.
                            t.vars.level = level;
                        }
                    }
                    else if (Xslt.isXsltName(cur, "message"))
                    {
                        xsltMessage(t, context_node, cur);
                    }
                    else
                    {
                        transformError(t, null, cur, fmt("Unexpected XSLT element '%s'.\n", nodeName(cur)));
                    }
                    break skipChildren;
                }
                else if (nodeKind(cur) == TEXT || nodeKind(cur) == CDATA)
                {
                    if (xsltCopyText(t, insert, cur) == null) break error;
                }
                else if (nodeKind(cur) == ELEMENT && nodeNs(cur) != null && nodeUserData(cur) != null)
                {
                    // An extension element.
                    old_cur_inst = t.inst;
                    t.inst = cur;
                    Object ud = nodeUserData(cur);
                    boolean done = false;
                    if (ud instanceof StylePreComp info)
                    {
                        t.insert = insert;
                        done = xsltCallFunc(t, context_node, cur, info);
                        if (done && old_local_fragment_top != t.local_rvt) xsltReleaseLocalRvts(t, old_local_fragment_top);
                        t.insert = old_insert;
                    }
                    if (!done)
                    {
                        int found = 0;
                        t.insert = insert;
                        for (XmlNode child = nodeFirstChild(cur); child != null; child = nodeNext(child))
                        {
                            if (Xslt.isXsltElem(child) && Xslt.isXsltName(child, "fallback"))
                            {
                                found = 1;
                                xsltApplySequenceConstructor(t, context_node, nodeFirstChild(child), null);
                            }
                        }
                        t.insert = old_insert;
                        if (found == 0)
                        {
                            transformError(t, null, cur, fmt("xsltApplySequenceConstructor: failed to find extension %s\n", nodeName(cur)));
                        }
                    }
                    t.inst = old_cur_inst;
                    break skipChildren;
                }
                else if (nodeKind(cur) == ELEMENT)
                {
                    // A literal result element.
                    old_cur_inst = t.inst;
                    t.inst = cur;
                    copy = xsltShallowCopyElem(t, cur, insert, 1);
                    if (copy == null) break error;
                    // Add the namespaces in scope of the template (only for its top level elements).
                    if (templ != null && old_insert == insert && t.templ != null && t.templ.inherited_ns != null)
                    {
                        for (int i = 0; i < t.templ.inherited_ns_nr; i++)
                        {
                            XmlNs ns = t.templ.inherited_ns[i];
                            String uri = xsltLookupAlias(t, nsHref(ns));
                            if (uri != null && uri.equals(XSLT_UNDEFINED_DEFAULT_NS)) continue;
                            if (uri == null) uri = nsHref(ns);
                            XmlNs ret = lxSearchNs(copy, nsPrefix(ns));
                            if (ret == null || !streq(nsHref(ret), uri)) lxNewNs(copy, uri, nsPrefix(ns));
                        }
                        if (nodeNs(copy) != null) nodeSetNs(copy, xsltGetNamespace(t, cur, nodeNs(copy), copy));
                    }
                    if (!nodeAttrs(cur).isEmpty()) xsltAttrListTemplateProcess(t, copy, nodeAttrs(cur));
                    t.inst = old_cur_inst;
                }
                // Descend into the content.
                if (nodeFirstChild(cur) != null && nodeKind(nodeFirstChild(cur)) != ENTITY_DECL)
                {
                    cur = nodeFirstChild(cur);
                    level++;
                    if (copy != null) insert = copy;
                    continue;
                }
            }
            // skip_children:
            if (t.state == TransformCtx.STATE_STOPPED) break;
            if (nodeNext(cur) != null)
            {
                cur = nodeNext(cur);
                continue;
            }
            do
            {
                cur = nodeParent(cur);
                level--;
                // Pop the variables of the leaving level.
                if (t.vars_nr > old_vars_nr && t.vars.level > level) xsltLocalVariablePop(t, old_vars_nr, level);
                insert = lxParent(insert);
                if (cur == null) break;
                if (cur == list_parent)
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
        // error:
        if (t.vars_nr > old_vars_nr) xsltLocalVariablePop(t, old_vars_nr, -1);
        t.node = old_context_node;
        t.inst = old_inst;
        t.insert = old_insert;
        t.depth--;
    }

    // Sorting, port of libxslt xsltutils.c xsltDefaultSortFunction ////

    /** A sort key (XsltSortKey). */
    private static final class SortKey
    {
        boolean valid;
        double num;
        String str;
        int index;              // The original position, for duplicates.
    }

    /** Port of xsltComputeSortResultInternal: the sort keys of the current node list. */
    private static SortKey[] xsltComputeSortResult(TransformCtx t, XmlNode sort, int number)
    {
        StylePreComp comp = precomp(sort);
        if (comp == null)
        {
            genericError(t.style.settings, "xsl:sort : compilation failed\n");
            return null;
        }
        if (comp.select == null || comp.comp == null) return null;
        NodeList list = t.node_list;
        if (list == null || list.nr <= 1) return null;
        int len = list.nr;
        SortKey[] results = new SortKey[len];
        for (int i = 0; i < len; i++) results[i] = new SortKey();
        XmlNode old_inst = t.inst;
        XmlNode old_node = t.node;
        long old_pos = t.xp_position, old_size = t.xp_size;
        for (int i = 0; i < len; i++)
        {
            t.inst = sort;
            t.xp_size = len;
            t.xp_position = i + 1;
            t.node = list.nodes[i];
            XPathResult res = xsltXpathEval(t, comp.comp, t.node);
            if (res != null)
            {
                results[i].index = i;
                results[i].valid = true;
                String s = res.castToString();
                if (number != 0)
                {
                    results[i].num = XPath.stringToNumber(s);
                }
                else
                {
                    results[i].str = s;
                }
            }
            else
            {
                t.state = TransformCtx.STATE_STOPPED;
                results[i].valid = false;
            }
        }
        t.inst = old_inst;
        t.node = old_node;
        t.xp_size = old_size;
        t.xp_position = old_pos;
        return results;
    }

    /** strcmp of the UTF-8 encodings: code point order. */
    private static int strcmpUtf8(String a, String b)
    {
        int i = 0, j = 0;
        int na = a.length(), nb = b.length();
        while (i < na && j < nb)
        {
            int ca = a.codePointAt(i), cb = b.codePointAt(j);
            if (ca != cb) return ca < cb ? -1 : 1;
            i += Character.charCount(ca);
            j += Character.charCount(cb);
        }
        if (i < na) return 1;
        if (j < nb) return -1;
        return 0;
    }

    private static int xsltSortCompare(SortKey a, SortKey b, int number)
    {
        if (number != 0)
        {
            // NaN is smaller than numbers, like the XSLT spec says.
            if (Double.isNaN(a.num)) return Double.isNaN(b.num) ? 0 : -1;
            if (Double.isNaN(b.num)) return 1;
            if (a.num == b.num) return 0;
            return a.num > b.num ? 1 : -1;
        }
        return strcmpUtf8(a.str != null ? a.str : "", b.str != null ? b.str : "");
    }

    /** Port of xsltDefaultSortFunction: a shell sort of the node list by the sort keys. */
    private static void xsltDoSortFunction(TransformCtx t, XmlNode[] sorts, int nbsorts)
    {
        SortKey[][] results_tab = new SortKey[XSLT_MAX_SORT][];
        int[] number = new int[XSLT_MAX_SORT];
        int[] desc = new int[XSLT_MAX_SORT];
        if (sorts == null || nbsorts <= 0 || nbsorts >= XSLT_MAX_SORT) return;
        if (sorts[0] == null) return;
        StylePreComp comp = precomp(sorts[0]);
        if (nodeUserData(sorts[0]) == null) return;
        NodeList list = t.node_list;
        if (list == null || list.nr <= 1) return;
        for (int j = 0; j < nbsorts; j++)
        {
            comp = precomp(sorts[j]);
            if (comp == null) continue;
            if (comp.stype == null && comp.has_stype != 0)
            {
                String stype = xsltEvalAttrValueTemplate(t, sorts[j], "data-type", null);
                number[j] = 0;
                if (stype != null)
                {
                    if (stype.equals("text")) { }
                    else if (stype.equals("number")) number[j] = 1;
                    else transformError(t, null, sorts[j], fmt("xsltDoSortFunction: no support for data-type = %s\n", stype));
                }
            }
            else
            {
                number[j] = comp.number;
            }
            if (comp.order == null && comp.has_order != 0)
            {
                String order = xsltEvalAttrValueTemplate(t, sorts[j], "order", null);
                desc[j] = 0;
                if (order != null)
                {
                    if (order.equals("ascending")) { }
                    else if (order.equals("descending")) desc[j] = 1;
                    else transformError(t, null, sorts[j], fmt("xsltDoSortFunction: invalid value %s for order\n", order));
                }
            }
            else
            {
                desc[j] = comp.descending;
            }
            if (comp.lang == null && comp.has_lang != 0)
            {
                // TODO: locale specific collation (xsltNewLocale, xsltStrxfrm).
                xsltEvalAttrValueTemplate(t, sorts[j], "lang", null);
            }
        }
        int len = list.nr;
        results_tab[0] = xsltComputeSortResult(t, sorts[0], number[0]);
        for (int i = 1; i < XSLT_MAX_SORT; i++) results_tab[i] = null;
        SortKey[] results = results_tab[0];
        if (results == null) return;
        // Shell's sort of the node set.
        for (int incr = len / 2; incr > 0; incr /= 2)
        {
            for (int i = incr; i < len; i++)
            {
                int j = i - incr;
                if (!results[i].valid) continue;
                while (j >= 0)
                {
                    int tst;
                    if (!results[j].valid)
                    {
                        tst = 1;
                    }
                    else
                    {
                        tst = xsltSortCompare(results[j], results[j + incr], number[0]);
                        if (desc[0] != 0) tst = -tst;
                    }
                    if (tst == 0)
                    {
                        // Multi level sorts.
                        int depth = 1;
                        while (depth < nbsorts)
                        {
                            if (sorts[depth] == null) break;
                            if (nodeUserData(sorts[depth]) == null) break;
                            if (results_tab[depth] == null) results_tab[depth] = xsltComputeSortResult(t, sorts[depth], number[depth]);
                            SortKey[] res = results_tab[depth];
                            if (res == null) break;
                            if (!res[j].valid)
                            {
                                if (res[j + incr].valid) tst = 1;
                            }
                            else if (!res[j + incr].valid)
                            {
                                tst = -1;
                            }
                            else
                            {
                                tst = xsltSortCompare(res[j], res[j + incr], number[depth]);
                                if (desc[depth] != 0) tst = -tst;
                            }
                            if (tst != 0) break;
                            depth++;
                        }
                    }
                    if (tst == 0) tst = results[j].index > results[j + incr].index ? 1 : 0;
                    if (tst > 0)
                    {
                        SortKey tmp = results[j];
                        results[j] = results[j + incr];
                        results[j + incr] = tmp;
                        XmlNode node = list.nodes[j];
                        list.nodes[j] = list.nodes[j + incr];
                        list.nodes[j + incr] = node;
                        for (int depth = 1; depth < nbsorts; depth++)
                        {
                            if (sorts[depth] == null || results_tab[depth] == null) break;
                            SortKey[] res = results_tab[depth];
                            SortKey tmp2 = res[j];
                            res[j] = res[j + incr];
                            res[j + incr] = tmp2;
                        }
                        j -= incr;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    /** Port of xsltMessage (xsl:message). */
    private static void xsltMessage(TransformCtx t, XmlNode node, XmlNode inst)
    {
        if (inst == null) return;
        int terminate = 0;
        String prop = Xslt.getNsPropExact(inst, "terminate", null);
        if (prop != null)
        {
            if (prop.equals("yes")) terminate = 1;
            else if (prop.equals("no")) terminate = 0;
            else transformError(t, null, inst, "xsl:message : terminate expecting 'yes' or 'no'\n");
        }
        String message = xsltEvalTemplateString(t, node, inst);
        if (message != null)
        {
            int len = message.length();
            genericError(t.style.settings, message);
            if (len > 0 && message.charAt(len - 1) != '\n') genericError(t.style.settings, "\n");
        }
        if (terminate != 0) t.state = TransformCtx.STATE_STOPPED;
    }

    /** Port of xsltDebug (libxslt:debug and potential infinite recursions): dump the template and variable stacks. */
    private static void xsltDebug(TransformCtx t, XmlNode node, XmlNode inst)
    {
        Settings s = t.style.settings;
        genericError(s, "Templates:\n");
        for (int i = 0, j = t.templ_nr - 1; i < 15 && j >= 0; i++, j--)
        {
            Template tj = t.templ_tab.get(j);
            genericError(s, fmt("#%d ", i));
            if (tj.name != null) genericError(s, fmt("name %s ", tj.name));
            if (tj.match != null) genericError(s, fmt("name %s ", tj.match));
            if (tj.mode != null) genericError(s, fmt("name %s ", tj.mode));
            genericError(s, "\n");
        }
        genericError(s, "Variables:\n");
        for (int i = 0, j = t.vars_nr - 1; i < 15 && j >= 0; i++, j--)
        {
            if (varsAt(t, j) == null) continue;
            genericError(s, fmt("#%d\n", i));
            for (StackElem cur = varsAt(t, j); cur != null; cur = cur.next)
            {
                if (cur.comp == null) genericError(s, "corrupted !!!\n");
                else if (cur.comp.type == StylePreComp.FUNC_PARAM) genericError(s, "param ");
                else if (cur.comp.type == StylePreComp.FUNC_VARIABLE) genericError(s, "var ");
                if (cur.name != null) genericError(s, fmt("%s ", cur.name));
                else genericError(s, "noname !!!!");
                // The value is only dumped when the debug output is stdout or stderr (not by default).
                if (cur.value == null) genericError(s, "NULL !!!!");
                genericError(s, "\n");
            }
        }
    }

    /** Port of xsltSaveResultToFilename: save a result document (xsl:document), -1 on failure. */
    private static int xsltSaveResultToFilename(TransformCtx t, String filename, XmlDocument res, Stylesheet style, boolean append)
    {
        if (filename == null || res == null || style == null) return -1;
        if (nodeFirstChild(domDocument(res)) == null) return 0;
        byte[] out = saveResultToString(res, style, t.type);
        String path = filename;
        if (path.startsWith("file://"))
        {
            path = path.substring(7);
            if (path.startsWith("localhost/")) path = path.substring(9);
        }
        int ret = -1;
        try (OutputStream f = append
             ? Files.newOutputStream(Paths.get(path), StandardOpenOption.CREATE, StandardOpenOption.APPEND, StandardOpenOption.WRITE)
             : Files.newOutputStream(Paths.get(path), StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING, StandardOpenOption.WRITE))
        {
            f.write(out);
            ret = out.length;
        }
        catch (IOException | RuntimeException e)
        {
            ret = -1;
        }
        return ret;
    }

    /** Port of xsltDocumentElem: xsl:document (XSLT 1.1 draft), saxon:output, xalan:write and xt:document. */
    private static void xsltDocumentElem(TransformCtx t, XmlNode node, XmlNode inst, StylePreComp comp)
    {
        if (node == null || inst == null || comp == null) return;
        String url = null, filename = null;
        XmlDocument res = null;
        Stylesheet style = null;
        String name = nodeName(inst);
        if (comp.filename == null)
        {
            if (name.equals("output"))
            {
                // saxon:output, the file attribute is in no namespace.
                url = xsltEvalAttrValueTemplate(t, inst, "file", XSLT_SAXON_NAMESPACE);
                if (url == null) url = xsltEvalAttrValueTemplate(t, inst, "href", XSLT_SAXON_NAMESPACE);
            }
            else if (name.equals("write"))
            {
                url = xsltEvalAttrValueTemplate(t, inst, "select", XSLT_XALAN_NAMESPACE);
                if (url != null)
                {
                    // The value of the select attribute is an XPath expression (bug #59212).
                    XPath cmp = Xslt.xpathCompile(t.style.settings, null, url, false);
                    String val = cmp != null ? xsltEvalXpathString(t, cmp) : null;
                    if (cmp == null) t.state = TransformCtx.STATE_STOPPED;
                    url = val;
                }
                if (url == null) url = xsltEvalAttrValueTemplate(t, inst, "file", XSLT_XALAN_NAMESPACE);
                if (url == null) url = xsltEvalAttrValueTemplate(t, inst, "href", XSLT_XALAN_NAMESPACE);
            }
            else if (name.equals("document"))
            {
                url = xsltEvalAttrValueTemplate(t, inst, "href", null);
            }
        }
        else
        {
            url = comp.filename;
        }
        if (url == null)
        {
            transformError(t, null, inst, "xsltDocumentElem: href/URI-Reference not found\n");
            return;
        }
        // If the computation failed, the url was probably not escaped.
        filename = XmlUri.build(url, t.output_file);
        if (filename == null)
        {
            String esc = XmlUri.escapeStr(url, ":/.?,");
            if (esc != null)
            {
                filename = XmlUri.build(esc, t.output_file);
            }
        }
        if (filename == null)
        {
            transformError(t, null, inst, fmt("xsltDocumentElem: URL computation failed for %s\n", url));
            return;
        }
        String old_output_file = t.output_file;
        XmlDocument old_output = t.output;
        XmlNode old_insert = t.insert;
        int old_type = t.type;
        t.output_file = filename;
        style = Xslt.stylesheetNewEmpty(null, t.style.settings);
        error:
        {
            // The 1.1 draft allows full parameterization of the output.
            String prop;
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "version", null)) != null)
            {
                style.version = prop;
            }
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "encoding", null)) != null)
            {
                style.encoding = prop;
            }
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "method", null)) != null)
            {
                style.method = null;
                style.method_uri = null;
                String[] pname = { prop };
                String uri = Xslt.getQnameUri(t.style.settings, inst, pname);
                prop = pname[0];
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
                        transformError(t, null, inst, fmt("invalid value for method: %s\n", prop));
                        style.warnings++;
                    }
                }
                else
                {
                    style.method = prop;
                    style.method_uri = uri;
                }
            }
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "doctype-system", null)) != null)
            {
                style.doctype_system = prop;
            }
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "doctype-public", null)) != null)
            {
                style.doctype_public = prop;
            }
            String[] yesno = { "standalone", "indent", "omit-xml-declaration" };
            for (int i = 0; i < 3; i++)
            {
                if ((prop = xsltEvalAttrValueTemplate(t, inst, yesno[i], null)) != null)
                {
                    int v = -2;
                    if (prop.equals("yes")) v = 1;
                    else if (prop.equals("no")) v = 0;
                    else
                    {
                        transformError(t, null, inst, fmt("invalid value for %s: %s\n", yesno[i], prop));
                        style.warnings++;
                    }
                    if (v != -2)
                    {
                        if (i == 0) style.standalone = v;
                        else if (i == 1) style.indent = v;
                        else style.omit_xml_declaration = v;
                    }
                }
            }
            String elements = xsltEvalAttrValueTemplate(t, inst, "cdata-section-elements", null);
            if (elements != null)
            {
                // Like libxslt they go into the strip-space table.
                if (style.strip_spaces == null) style.strip_spaces = new HashMap<>();
                int n = elements.length();
                int element = 0;
                while (element < n)
                {
                    while (element < n && Xslt.isBlank(elements.charAt(element))) element++;
                    if (element >= n) break;
                    int end = element;
                    while (end < n && !Xslt.isBlank(elements.charAt(end))) end++;
                    String[] e = { elements.substring(element, end) };
                    String uri = Xslt.getQnameUri(t.style.settings, inst, e);
                    if (e[0] != null)
                    {
                        String key = Xslt.key2(e[0], uri);
                        style.strip_spaces.putIfAbsent(key, "cdata");
                    }
                    element = end;
                }
            }
            // Create a new document tree and process the element template.
            String method = xsltImportMethod(style);
            String[] doctype_public = { xsltImportDoctypePublic(style) };
            String[] doctype_system = { xsltImportDoctypeSystem(style) };
            String version = xsltImportVersion(style);
            String encoding = xsltImportEncoding(style);
            if (method != null && !method.equals("xml"))
            {
                if (method.equals("html"))
                {
                    t.type = OUTPUT_HTML;
                    if (doctype_public[0] != null || doctype_system[0] != null)
                    {
                        res = xsltHtmlNewDoc(doctype_system[0], doctype_public[0]);
                    }
                    else
                    {
                        if (version != null) xsltGetHtmlIds(version, doctype_public, doctype_system);
                        // htmlNewDocNoDtD.
                        res = domNew();
                        if (doctype_public[0] != null || doctype_system[0] != null) domSetDtd(res, "html", doctype_public[0], doctype_system[0]);
                    }
                }
                else if (method.equals("xhtml"))
                {
                    transformError(t, null, inst, "xsltDocumentElem: unsupported method xhtml\n");
                    t.type = OUTPUT_HTML;
                    res = domNew();
                    if (doctype_public[0] != null || doctype_system[0] != null) domSetDtd(res, "html", doctype_public[0], doctype_system[0]);
                }
                else if (method.equals("text"))
                {
                    t.type = OUTPUT_TEXT;
                    res = domNew();
                    domSetVersion(res, style.version != null ? style.version : "1.0");
                }
                else
                {
                    transformError(t, null, inst, fmt("xsltDocumentElem: unsupported method (%s)\n", method));
                    break error;
                }
            }
            else
            {
                t.type = OUTPUT_XML;
                res = domNew();
                domSetVersion(res, style.version != null ? style.version : "1.0");
            }
            if (encoding != null) domSetEncoding(res, encoding);
            t.output = res;
            t.insert = domDocument(res);
            xsltApplySequenceConstructor(t, node, nodeFirstChild(inst), null);
            // Post processing depending on the generated output.
            XmlNode root = domRootElement(res);
            if (root != null)
            {
                String doctype;
                XmlNs rns = nodeNs(root);
                if (rns != null && nsPrefix(rns) != null)
                {
                    doctype = nsPrefix(rns) + ":" + nodeName(root);
                }
                else
                {
                    doctype = nodeName(root);
                }
                if (method == null && rns == null && nodeName(root).equalsIgnoreCase("html"))
                {
                    XmlNode tmp = nodeFirstChild(domDocument(res));
                    while (tmp != null && tmp != root)
                    {
                        if (nodeKind(tmp) == ELEMENT) break;
                        if (nodeKind(tmp) == TEXT && !xsltIsBlankTextNode(tmp)) break;
                        tmp = nodeNext(tmp);
                    }
                    if (tmp == root)
                    {
                        t.type = OUTPUT_HTML;
                        if (doctype_public[0] != null || doctype_system[0] != null)
                        {
                            domSetDtd(res, doctype, doctype_public[0], doctype_system[0]);
                        }
                        else if (version != null)
                        {
                            xsltGetHtmlIds(version, doctype_public, doctype_system);
                            if (doctype_public[0] != null || doctype_system[0] != null) domSetDtd(res, doctype, doctype_public[0], doctype_system[0]);
                        }
                    }
                }
                if (t.type == OUTPUT_XML)
                {
                    doctype_public[0] = xsltImportDoctypePublic(style);
                    doctype_system[0] = xsltImportDoctypeSystem(style);
                    if (doctype_public[0] != null || doctype_system[0] != null) domSetDtd(res, doctype, doctype_public[0], doctype_system[0]);
                }
            }
            // redirect:write can append to the file (append="true|yes").
            boolean append = false;
            if ((prop = xsltEvalAttrValueTemplate(t, inst, "append", null)) != null)
            {
                if (prop.equals("true") || prop.equals("yes"))
                {
                    style.omit_xml_declaration = 1;
                    append = true;
                }
                else
                {
                    style.omit_xml_declaration = 0;
                }
            }
            if (xsltSaveResultToFilename(t, filename, res, style, append) < 0)
            {
                transformError(t, null, inst, fmt("xsltDocumentElem: unable to save to %s\n", filename));
            }
        }
        t.output = old_output;
        t.insert = old_insert;
        t.type = old_type;
        t.output_file = old_output_file;
        if (style != null) Xslt.stylesheetFree(style);
    }

    // Numbers, port of libxslt numbers.c ////////////////////////////
    //
    // Like libxslt, the formatting works on the UTF-8 bytes of the strings: a C string is a
    // byte array with a terminating zero here and a pointer is an index into it.

    private static final int XSLT_NUM_MAX_TOKENS = 1024;

    /** A format token (XsltFormatToken). */
    private static final class FormatToken
    {
        byte[] separator;
        int token;
        int width;

        FormatToken()
        {
        }

        FormatToken(byte[] separator, int token, int width)
        {
            this.separator = separator;
            this.token = token;
            this.width = width;
        }
    }

    /** The tokens of a format (XsltFormat). */
    private static final class Format
    {
        byte[] start;
        FormatToken[] tokens = new FormatToken[XSLT_NUM_MAX_TOKENS];
        int n_tokens;
        byte[] end;

        Format()
        {
            for (int i = 0; i < XSLT_NUM_MAX_TOKENS; i++) tokens[i] = new FormatToken();
        }
    }

    private static final byte[] XSLT_ALPHA_UPPER_LIST = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".getBytes(StandardCharsets.US_ASCII);
    private static final byte[] XSLT_ALPHA_LOWER_LIST = "abcdefghijklmnopqrstuvwxyz".getBytes(StandardCharsets.US_ASCII);

    /** The zero terminated UTF-8 bytes of s, null for null. */
    private static byte[] cstr(String s)
    {
        if (s == null) return null;
        byte[] b = s.getBytes(StandardCharsets.UTF_8);
        byte[] r = new byte[b.length + 1];
        System.arraycopy(b, 0, r, 0, b.length);
        return r;
    }

    /** The byte at i, 0 past the end. */
    private static int at(byte[] s, int i)
    {
        return i >= 0 && i < s.length ? s[i] & 0xff : 0;
    }

    /** strlen from p. */
    private static int strlenB(byte[] s, int p)
    {
        int n = 0;
        while (at(s, p + n) != 0) n++;
        return n;
    }

    /** strndup of n bytes from p. */
    private static byte[] strndupB(byte[] s, int p, int n)
    {
        byte[] r = new byte[n + 1];
        for (int i = 0; i < n; i++) r[i] = (byte)at(s, p + i);
        return r;
    }

    /** The string of the bytes of s from p (to the terminating zero). */
    private static String strB(byte[] s, int p)
    {
        return new String(s, p, strlenB(s, p), StandardCharsets.UTF_8);
    }

    private static void appendB(java.io.ByteArrayOutputStream out, byte[] s, int p)
    {
        if (s == null) return;
        out.write(s, p, strlenB(s, p));
    }

    private static void appendB(java.io.ByteArrayOutputStream out, String s)
    {
        byte[] b = s.getBytes(StandardCharsets.UTF_8);
        out.write(b, 0, b.length);
    }

    /** membuffer_append_region(start, start + len). */
    private static void appendRegionB(java.io.ByteArrayOutputStream out, byte[] s, int p, int len)
    {
        if (s == null || len <= 0) return;
        for (int i = 0; i < len; i++) out.write(at(s, p + i));
    }

    private static int strncmpB(byte[] a, int ai, byte[] b, int bi, int n)
    {
        for (int k = 0; k < n; k++)
        {
            int ca = at(a, ai + k), cb = at(b, bi + k);
            if (ca != cb) return ca - cb;
            if (ca == 0) return 0;
        }
        return 0;
    }

    /** Port of xsltGetUTF8CharZ on bytes (xslt1_utf8_char). */
    private static int utf8CharB(byte[] s, int p, int[] len)
    {
        int c = at(s, p);
        if ((c & 0x80) != 0)
        {
            if ((at(s, p + 1) & 0xc0) != 0x80)
            {
                len[0] = 0;
                return -1;
            }
            if ((c & 0xe0) == 0xe0)
            {
                if ((at(s, p + 2) & 0xc0) != 0x80)
                {
                    len[0] = 0;
                    return -1;
                }
                if ((c & 0xf0) == 0xf0)
                {
                    if ((c & 0xf8) != 0xf0 || (at(s, p + 3) & 0xc0) != 0x80)
                    {
                        len[0] = 0;
                        return -1;
                    }
                    len[0] = 4;
                    c = (at(s, p) & 0x7) << 18;
                    c |= (at(s, p + 1) & 0x3f) << 12;
                    c |= (at(s, p + 2) & 0x3f) << 6;
                    c |= at(s, p + 3) & 0x3f;
                }
                else
                {
                    len[0] = 3;
                    c = (at(s, p) & 0xf) << 12;
                    c |= (at(s, p + 1) & 0x3f) << 6;
                    c |= at(s, p + 2) & 0x3f;
                }
            }
            else
            {
                len[0] = 2;
                c = (at(s, p) & 0x1f) << 6;
                c |= at(s, p + 1) & 0x3f;
            }
        }
        else
        {
            len[0] = 1;
        }
        return c;
    }

    /** xmlUTF8Strsize(utf, 1): the bytes of the first character. */
    private static int xsltUtf8Size1(byte[] utf, int p)
    {
        if (utf == null) return 0;
        if (at(utf, p) == 0) return 0;
        int ptr = p;
        int ch = at(utf, ptr++);
        if ((ch & 0x80) != 0)
        {
            while (((ch <<= 1) & 0x80) != 0)
            {
                if (at(utf, ptr) == 0) break;
                ptr++;
            }
        }
        return ptr - p;
    }

    /** Port of xsltUTF8Charcmp: compare the first character of utf1 with utf2. */
    private static int xsltUtf8Charcmp(byte[] utf1, int p1, byte[] utf2)
    {
        int len = xsltUtf8Size1(utf1, p1);
        if (len < 1) return -1;
        if (utf2 == null) return 1;
        return strncmpB(utf1, p1, utf2, 0, len);
    }

    /** xmlUTF8Strloc: the index (in characters) of utfchar in utf, -1 if not found. */
    private static int xsltUtf8Strloc(byte[] utf, byte[] utfchar)
    {
        if (utf == null || utfchar == null) return -1;
        int size = xsltUtf8Size1(utfchar, 0);
        int p = 0;
        for (int i = 0; at(utf, p) != 0; i++)
        {
            if (strncmpB(utf, p, utfchar, 0, size) == 0) return i;
            int ch = at(utf, p++);
            if ((ch & 0x80) != 0)
            {
                if ((ch & 0xc0) != 0xc0) return -1;
                while (((ch <<= 1) & 0x80) != 0)
                {
                    if ((at(utf, p++) & 0xc0) != 0x80) return -1;
                }
            }
        }
        return -1;
    }

    /** xmlUTF8Strpos: the index of the pos-th character, -1 if none. */
    private static int xsltUtf8Strpos(byte[] utf, int pos)
    {
        if (utf == null || pos < 0) return -1;
        int p = 0;
        while (pos-- != 0)
        {
            int ch = at(utf, p++);
            if (ch == 0) return -1;
            if ((ch & 0x80) != 0)
            {
                if ((ch & 0xc0) != 0xc0) return -1;
                while (((ch <<= 1) & 0x80) != 0)
                {
                    if ((at(utf, p++) & 0xc0) != 0x80) return -1;
                }
            }
        }
        return p;
    }

    private static void putB(byte[] out, int i, int v)
    {
        if (i >= 0 && i < out.length) out[i] = (byte)v;
    }

    /** Port of xsltCopyCharMultiByte. */
    private static int xsltCopyCharMultiByte(byte[] out, int p, int val)
    {
        if (out == null || val < 0) return 0;
        if (val >= 0x80)
        {
            int saved = p;
            int bits;
            if (val < 0x800) { putB(out, p++, (val >> 6) | 0xC0); bits = 0; }
            else if (val < 0x10000) { putB(out, p++, (val >> 12) | 0xE0); bits = 6; }
            else if (val < 0x110000) { putB(out, p++, (val >> 18) | 0xF0); bits = 12; }
            else return 0;
            for (; bits >= 0; bits -= 6) putB(out, p++, ((val >> bits) & 0x3F) | 0x80);
            return p - saved;
        }
        putB(out, p, val);
        return 1;
    }

    /** The length of the UTF-8 encoding of a character. */
    private static int utf8Len(int c)
    {
        if (c < 0x80) return 1;
        if (c < 0x800) return 2;
        if (c < 0x10000) return 3;
        return 4;
    }

    private static boolean xsltIsLetterDigit(int val)
    {
        return XmlChars.isBaseChar(val) || XmlChars.isIdeographic(val) || XmlChars.isDigit(val);
    }

    private static boolean xsltIsDigitZero(int ch)
    {
        switch (ch)
        {
        case 0x0030: case 0x0660: case 0x06F0: case 0x0966:
        case 0x09E6: case 0x0A66: case 0x0AE6: case 0x0B66:
        case 0x0C66: case 0x0CE6: case 0x0D66: case 0x0E50:
        case 0x0ED0: case 0x0F20:
            return true;
        default:
            return false;
        }
    }

    private static boolean xsltIsDigitOne(int ch)
    {
        return xsltIsDigitZero(ch - 1);
    }

    /** Port of xsltNumberFormatDecimal. */
    private static void xsltNumberFormatDecimal(java.io.ByteArrayOutputStream buffer, double number, int digit_zero, int width,
                                                int digits_per_group, int grouping_character, int grouping_character_len,
                                                Settings settings)
    {
        byte[] temp_string = new byte[500];
        byte[] temp_char = new byte[6];
        // Build the buffer from the back.
        int pointer = temp_string.length - 1;
        temp_string[pointer] = 0;
        int i = 0;
        while (pointer > 0)
        {
            if (i >= width && Math.abs(number) < 1.0) break;
            if (i > 0 && grouping_character != 0 && digits_per_group > 0 && (i % digits_per_group) == 0)
            {
                if (pointer - grouping_character_len < 0)
                {
                    i = -1;
                    break;
                }
                pointer -= grouping_character_len;
                xsltCopyCharMultiByte(temp_string, pointer, grouping_character);
            }
            int val = digit_zero + (int)(number % 10.0);
            if (val < 0x80)
            {
                if (pointer <= 0)
                {
                    i = -1;
                    break;
                }
                temp_string[--pointer] = (byte)val;
            }
            else
            {
                int len = xsltCopyCharMultiByte(temp_char, 0, val);
                if (pointer - len < 0)
                {
                    i = -1;
                    break;
                }
                pointer -= len;
                System.arraycopy(temp_char, 0, temp_string, pointer, len);
            }
            number /= 10.0;
            ++i;
        }
        if (i < 0) genericError(settings, "xsltNumberFormatDecimal: Internal buffer size exceeded\n");
        appendB(buffer, temp_string, pointer);
    }

    /** Port of xsltNumberFormatAlpha. */
    private static void xsltNumberFormatAlpha(NumberData data, java.io.ByteArrayOutputStream buffer, double number, int is_upper,
                                              Settings settings)
    {
        byte[] temp_string = new byte[8 * 8 + 1];
        double alpha_size = (double)XSLT_ALPHA_UPPER_LIST.length;
        // The "a" token has an intrinsic lower limit of 1.
        if (number < 1.0)
        {
            xsltNumberFormatDecimal(buffer, number, '0', 1, data.digits_per_group, data.grouping_character,
                                    utf8Len(data.grouping_character), settings);
            return;
        }
        int pointer = temp_string.length;
        temp_string[--pointer] = 0;
        byte[] alpha_list = is_upper != 0 ? XSLT_ALPHA_UPPER_LIST : XSLT_ALPHA_LOWER_LIST;
        for (int i = 1; i < temp_string.length; i++)
        {
            number--;
            temp_string[--pointer] = alpha_list[(int)(number % alpha_size)];
            number /= alpha_size;
            if (number < 1.0) break;
        }
        appendB(buffer, temp_string, pointer);
    }

    private static final double[] ROMAN_V = { 1000.0, 900.0, 500.0, 400.0, 100.0, 90.0, 50.0, 40.0, 10.0, 9.0, 5.0, 4.0, 1.0 };
    private static final String[] ROMAN_U = { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" };
    private static final String[] ROMAN_L = { "m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i" };
    private static final boolean[] ROMAN_REPEAT = { true, false, true, false, true, false, true, false, true, false, true, false, true };

    /** Port of xsltNumberFormatRoman. */
    private static void xsltNumberFormatRoman(NumberData data, java.io.ByteArrayOutputStream buffer, double number, int is_upper,
                                              Settings settings)
    {
        // A reasonable upper bound to avoid denial of service.
        if (number < 1.0 || number > 5000.0)
        {
            xsltNumberFormatDecimal(buffer, number, '0', 1, data.digits_per_group, data.grouping_character,
                                    utf8Len(data.grouping_character), settings);
            return;
        }
        for (int k = 0; k < ROMAN_V.length; k++)
        {
            if (ROMAN_REPEAT[k])
            {
                while (number >= ROMAN_V[k])
                {
                    appendB(buffer, is_upper != 0 ? ROMAN_U[k] : ROMAN_L[k]);
                    number -= ROMAN_V[k];
                }
            }
            else if (number >= ROMAN_V[k])
            {
                appendB(buffer, is_upper != 0 ? ROMAN_U[k] : ROMAN_L[k]);
                number -= ROMAN_V[k];
            }
        }
    }

    /** Port of xsltNumberFormatTokenize. */
    private static void xsltNumberFormatTokenize(byte[] format, Format tokens)
    {
        int ix = 0;
        int val;
        int[] len = { 0 };
        tokens.start = null;
        tokens.tokens[0].separator = null;
        tokens.end = null;
        // The initial non-alphanumeric token (always there, even if NULL).
        while (!xsltIsLetterDigit(val = utf8CharB(format, ix, len)))
        {
            if (at(format, ix) == 0) break;
            ix += len[0];
        }
        if (ix > 0) tokens.start = strndupB(format, 0, ix);
        for (tokens.n_tokens = 0; tokens.n_tokens < XSLT_NUM_MAX_TOKENS; tokens.n_tokens++)
        {
            if (at(format, ix) == 0) break;
            FormatToken tok = tokens.tokens[tokens.n_tokens];
            // The separator has already been parsed (except for the first number) in tokens->end.
            if (tokens.n_tokens > 0)
            {
                tok.separator = tokens.end;
                tokens.end = null;
            }
            val = utf8CharB(format, ix, len);
            if (xsltIsDigitOne(val) || xsltIsDigitZero(val))
            {
                tok.width = 1;
                while (xsltIsDigitZero(val))
                {
                    tok.width++;
                    ix += len[0];
                    val = utf8CharB(format, ix, len);
                }
                if (xsltIsDigitOne(val))
                {
                    tok.token = val - 1;
                    ix += len[0];
                    val = utf8CharB(format, ix, len);
                }
                else
                {
                    tok.token = '0';
                    tok.width = 1;
                }
            }
            else if (val == 'A' || val == 'a' || val == 'I' || val == 'i')
            {
                tok.token = val;
                ix += len[0];
                val = utf8CharB(format, ix, len);
            }
            else
            {
                // Any other format token: a format token of 1 (XSLT 7.7).
                tok.token = '0';
                tok.width = 1;
            }
            // Skip the remaining alphanumeric characters.
            while (xsltIsLetterDigit(val))
            {
                ix += len[0];
                val = utf8CharB(format, ix, len);
            }
            // The temporary non-alphanumeric final token.
            int j = ix;
            while (!xsltIsLetterDigit(val))
            {
                if (val == 0) break;
                ix += len[0];
                val = utf8CharB(format, ix, len);
            }
            if (ix > j) tokens.end = strndupB(format, j, ix - j);
        }
    }

    private static final FormatToken DEFAULT_TOKEN = new FormatToken(new byte[] { '.', 0 }, '0', 1);

    /** Port of xsltNumberFormatInsertNumbers. */
    private static void xsltNumberFormatInsertNumbers(TransformCtx t, NumberData data, double[] numbers, int numbers_max,
                                                      Format tokens, java.io.ByteArrayOutputStream buffer)
    {
        Settings settings = t.style.settings;
        if (tokens.start != null) appendB(buffer, tokens.start, 0);
        for (int i = 0; i < numbers_max; i++)
        {
            double number = numbers[(numbers_max - 1) - i];
            // Round to nearest like XSLT 2.0.
            number = Math.floor(number + 0.5);
            if (number < 0.0)
            {
                Xslt.settingsError(settings, null, "xsl-number : negative value\n");
                // Recover by treating negative values as zero.
                number = 0.0;
            }
            FormatToken token;
            if (i < tokens.n_tokens) token = tokens.tokens[i];
            else if (tokens.n_tokens > 0) token = tokens.tokens[tokens.n_tokens - 1];
            else token = DEFAULT_TOKEN;
            // The separator, except for the first number.
            if (i > 0)
            {
                if (token.separator != null) appendB(buffer, token.separator, 0);
                else appendB(buffer, ".");
            }
            if (Double.isInfinite(number))
            {
                appendB(buffer, number < 0 ? "-Infinity" : "Infinity");
            }
            else if (Double.isNaN(number))
            {
                appendB(buffer, "NaN");
            }
            else
            {
                switch (token.token)
                {
                case 'A': xsltNumberFormatAlpha(data, buffer, number, 1, settings); break;
                case 'a': xsltNumberFormatAlpha(data, buffer, number, 0, settings); break;
                case 'I': xsltNumberFormatRoman(data, buffer, number, 1, settings); break;
                case 'i': xsltNumberFormatRoman(data, buffer, number, 0, settings); break;
                default:
                    if (xsltIsDigitZero(token.token))
                    {
                        xsltNumberFormatDecimal(buffer, number, token.token, token.width, data.digits_per_group,
                                                data.grouping_character, utf8Len(data.grouping_character), settings);
                    }
                    break;
                }
            }
        }
        if (tokens.end != null) appendB(buffer, tokens.end, 0);
    }

    /** Port of xsltTestCompMatchCount. */
    private static int xsltTestCompMatchCount(TransformCtx t, XmlNode node, Pattern count_pat, XmlNode cur)
    {
        if (count_pat != null) return Xslt.patternMatch(t, node, count_pat);
        // The default: the same node type and expanded name as the current node.
        if (nodeKind(node) != nodeKind(cur)) return 0;
        if (nodeKind(node) == NAMESPACE) return 1;
        if (!streq(nodeName(node), nodeName(cur))) return 0;
        XmlNs a = nodeNs(node), b = nodeNs(cur);
        if (a == b) return 1;
        if (a == null || b == null) return 0;
        return streq(nsHref(a), nsHref(b)) ? 1 : 0;
    }

    /** The previous sibling as libxslt sees it (skipping nothing). */
    private static XmlNode xsltPrev(XmlNode n)
    {
        if (nodeKind(n) == NAMESPACE || nodeKind(n) == ATTRIBUTE) return null;
        return nodePrev(n);
    }

    /** Port of xsltNumberFormatGetAnyLevel. */
    private static int xsltNumberFormatGetAnyLevel(TransformCtx t, XmlNode node, Pattern count_pat,
                                                   Pattern from_pat, double[] array)
    {
        int amount = 0, cnt = 0;
        XmlNode cur = node;
        while (cur != null)
        {
            if (xsltTestCompMatchCount(t, cur, count_pat, node) != 0) cnt++;
            if (from_pat != null && Xslt.patternMatch(t, cur, from_pat) == 1) break;
            // Skip to the next preceding or ancestor.
            if (nodeKind(cur) == DOCUMENT) break;
            if (nodeKind(cur) == NAMESPACE || nodeKind(cur) == ATTRIBUTE)
            {
                cur = nodeParent(cur);
            }
            else
            {
                while (xsltPrev(cur) != null && nodeKind(xsltPrev(cur)) == DTD) cur = xsltPrev(cur);
                if (xsltPrev(cur) != null)
                {
                    for (cur = xsltPrev(cur); lxChildren(cur) != null && nodeLastChild(cur) != null; cur = nodeLastChild(cur)) { }
                }
                else
                {
                    cur = nodeParent(cur);
                }
            }
        }
        array[amount++] = (double)cnt;
        return amount;
    }

    /** Port of xsltNumberFormatGetMultipleLevel. */
    private static int xsltNumberFormatGetMultipleLevel(TransformCtx t, XmlNode node, Pattern count_pat,
                                                        Pattern from_pat, double[] array, int max)
    {
        int amount = 0;
        // ancestor-or-self::*[count]
        XmlNode ancestor = node;
        while (ancestor != null && nodeKind(ancestor) != DOCUMENT)
        {
            if (from_pat != null && Xslt.patternMatch(t, ancestor, from_pat) == 1) break;
            if (xsltTestCompMatchCount(t, ancestor, count_pat, node) != 0)
            {
                // count(preceding-sibling::*)
                int cnt = 1;
                XmlNode preceding = nodeKind(ancestor) != NAMESPACE ? xsltPrev(ancestor) : null;
                while (preceding != null)
                {
                    if (xsltTestCompMatchCount(t, preceding, count_pat, node) != 0) cnt++;
                    preceding = xsltPrev(preceding);
                }
                array[amount++] = (double)cnt;
                if (amount >= max) break;
            }
            ancestor = nodeParent(ancestor);
        }
        return amount;
    }

    /** Port of xsltNumberFormatGetValue: number(value) evaluated with node as the context node. */
    private static int xsltNumberFormatGetValue(TransformCtx t, XmlNode node, String value, XmlNode inst, double[] number)
    {
        int amount = 0;
        String expr = "number(" + value + ")";
        XPath comp = Xslt.xpathCompile(t.style.settings, inst, expr, false);
        if (comp != null)
        {
            XPathResult obj = xsltXpathEval(t, comp, node);
            if (obj != null)
            {
                number[0] = obj.castToNumber();
                amount++;
            }
        }
        return amount;
    }

    /** Port of xsltNumberFormat (xsl:number). */
    private static void xsltNumberFormat(TransformCtx t, NumberData data, XmlNode node)
    {
        Format tokens = new Format();
        if (data.format != null)
        {
            xsltNumberFormatTokenize(cstr(data.format), tokens);
        }
        else
        {
            // The format needs to be recomputed each time.
            if (data.has_format == 0) return;
            String format = xsltEvalAttrValueTemplate(t, data.node, "format", XSLT_NAMESPACE);
            if (format == null) return;
            xsltNumberFormatTokenize(cstr(format), tokens);
        }
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        double[] number = { 0 };
        int amount;
        if (data.value != null)
        {
            amount = xsltNumberFormatGetValue(t, node, data.value, data.node, number);
            if (amount == 1) xsltNumberFormatInsertNumbers(t, data, number, 1, tokens, output);
        }
        else if (data.level != null)
        {
            if (data.level.equals("single"))
            {
                amount = xsltNumberFormatGetMultipleLevel(t, node, data.count_pat, data.from_pat, number, 1);
                if (amount == 1) xsltNumberFormatInsertNumbers(t, data, number, 1, tokens, output);
            }
            else if (data.level.equals("multiple"))
            {
                double[] numarray = new double[1024];
                int max = numarray.length;
                amount = xsltNumberFormatGetMultipleLevel(t, node, data.count_pat, data.from_pat, numarray, max);
                if (amount > 0) xsltNumberFormatInsertNumbers(t, data, numarray, amount, tokens, output);
            }
            else if (data.level.equals("any"))
            {
                amount = xsltNumberFormatGetAnyLevel(t, node, data.count_pat, data.from_pat, number);
                if (amount > 0) xsltNumberFormatInsertNumbers(t, data, number, 1, tokens, output);
            }
            // count and from patterns can contain variable references, clear the cache of direct matches.
            if (data.count_pat != null) Xslt.compMatchClearCache(t, data.count_pat);
            if (data.from_pat != null) Xslt.compMatchClearCache(t, data.from_pat);
        }
        // Insert the number as a text node.
        String text = new String(output.toByteArray(), StandardCharsets.UTF_8);
        xsltCopyTextString(t, t.insert, text, 0);
    }

    /** XsltFormatNumberInfo. */
    private static final class FormatNumberInfo
    {
        int integer_hash;
        int integer_digits;
        int frac_digits;
        int frac_hash;
        int group;
        int multiplier;
        int add_decimal;
        int is_multiplier_set;
        int is_negative_pattern;
    }

    /** The UTF-8 bytes of the strings of a decimal format. */
    private static final class DecimalFormatBytes
    {
        byte[] digit;
        byte[] pattern_separator;
        byte[] minus_sign;
        byte[] infinity;
        byte[] no_number;
        byte[] decimal_point;
        byte[] grouping;
        byte[] percent;
        byte[] permille;
        byte[] zero_digit;

        DecimalFormatBytes(DecimalFormat f)
        {
            digit = cstr(f.digit);
            pattern_separator = cstr(f.pattern_separator);
            minus_sign = cstr(f.minus_sign);
            infinity = cstr(f.infinity);
            no_number = cstr(f.no_number);
            decimal_point = cstr(f.decimal_point);
            grouping = cstr(f.grouping);
            percent = cstr(f.percent);
            permille = cstr(f.permille);
            zero_digit = cstr(f.zero_digit);
        }
    }

    private static boolean xsltIsSpecial(DecimalFormatBytes self, byte[] letter, int p)
    {
        return xsltUtf8Charcmp(letter, p, self.zero_digit) == 0 ||
               xsltUtf8Charcmp(letter, p, self.digit) == 0 ||
               xsltUtf8Charcmp(letter, p, self.decimal_point) == 0 ||
               xsltUtf8Charcmp(letter, p, self.grouping) == 0 ||
               xsltUtf8Charcmp(letter, p, self.pattern_separator) == 0;
    }

    /** Port of xsltFormatNumberPreSuffix. format[0] is the index into f. */
    private static int xsltFormatNumberPreSuffix(DecimalFormatBytes self, byte[] f, int[] format, FormatNumberInfo info)
    {
        int count = 0;
        int len;
        while (true)
        {
            if (at(f, format[0]) == 0) return count;
            if (at(f, format[0]) == '\'')
            {
                // An escaped character, just count it.
                if (at(f, ++format[0]) == 0) return -1;
            }
            else if (xsltIsSpecial(self, f, format[0]))
            {
                return count;
            }
            else
            {
                // Percent and per-mille, only one of them.
                if (xsltUtf8Charcmp(f, format[0], self.percent) == 0)
                {
                    if (info.is_multiplier_set != 0) return -1;
                    info.multiplier = 100;
                    info.is_multiplier_set = 1;
                }
                else if (xsltUtf8Charcmp(f, format[0], self.permille) == 0)
                {
                    if (info.is_multiplier_set != 0) return -1;
                    info.multiplier = 1000;
                    info.is_multiplier_set = 1;
                }
            }
            if ((len = xsltUtf8Size1(f, format[0])) < 1) return -1;
            count += len;
            format[0] += len;
        }
    }

    private static final int DBL_MAX_10_EXP = 308;

    /** Port of xsltFormatNumberConversion: format-number(). */
    private static String xsltFormatNumberConversion(DecimalFormat self_format, String format_str, double number, Settings settings)
    {
        if (format_str.isEmpty())
        {
            Xslt.settingsError(settings, null, "xsltFormatNumberConversion : Invalid format (0-length)\n");
        }
        if (Double.isNaN(number))
        {
            return (self_format == null || self_format.no_number == null) ? "NaN" : self_format.no_number;
        }
        DecimalFormatBytes self = new DecimalFormatBytes(self_format);
        byte[] format = cstr(format_str);
        int[] the_format = { 0 };
        int prefix = -1, suffix = -1;
        int nprefix = -1, nsuffix = -1;
        int prefix_length, suffix_length = 0, nprefix_length, nsuffix_length;
        int j, len = 0;
        FormatNumberInfo format_info = new FormatNumberInfo();
        int delayed_multiplier = 0;
        int default_sign = 0;
        int found_error = 0;
        format_info.integer_hash = 0;
        format_info.integer_digits = 0;
        format_info.frac_digits = 0;
        format_info.frac_hash = 0;
        format_info.group = -1;
        format_info.multiplier = 1;
        format_info.add_decimal = 0;
        format_info.is_multiplier_set = 0;
        format_info.is_negative_pattern = 0;
        the_format[0] = 0;
        prefix_length = 0;
        outputNumber:
        {
            // First the positive pattern, for percent and per-mille and the main format.
            prefix = the_format[0];
            prefix_length = xsltFormatNumberPreSuffix(self, format, the_format, format_info);
            if (prefix_length < 0)
            {
                found_error = 1;
                break outputNumber;
            }
            // The number part, a trailing percent or per-mille may be part of the suffix.
            int self_grouping_len = self.grouping != null ? strlenB(self.grouping, 0) : 0;
            while (at(format, the_format[0]) != 0 && xsltUtf8Charcmp(format, the_format[0], self.decimal_point) != 0 &&
                   xsltUtf8Charcmp(format, the_format[0], self.pattern_separator) != 0)
            {
                if (delayed_multiplier != 0)
                {
                    format_info.multiplier = delayed_multiplier;
                    format_info.is_multiplier_set = 1;
                    delayed_multiplier = 0;
                }
                if (xsltUtf8Charcmp(format, the_format[0], self.digit) == 0)
                {
                    if (format_info.integer_digits > 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    format_info.integer_hash++;
                    if (format_info.group >= 0) format_info.group++;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.zero_digit) == 0)
                {
                    format_info.integer_digits++;
                    if (format_info.group >= 0) format_info.group++;
                }
                else if (self_grouping_len > 0 && strncmpB(format, the_format[0], self.grouping, 0, self_grouping_len) == 0)
                {
                    // Reset the group count.
                    format_info.group = 0;
                    the_format[0] += self_grouping_len;
                    continue;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.percent) == 0)
                {
                    if (format_info.is_multiplier_set != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    delayed_multiplier = 100;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.permille) == 0)
                {
                    if (format_info.is_multiplier_set != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    delayed_multiplier = 1000;
                }
                else
                {
                    break;
                }
                if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                {
                    found_error = 1;
                    break outputNumber;
                }
                the_format[0] += len;
            }
            // The fraction.
            if (at(format, the_format[0]) != 0 && xsltUtf8Charcmp(format, the_format[0], self.decimal_point) == 0)
            {
                format_info.add_decimal = 1;
                if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                {
                    found_error = 1;
                    break outputNumber;
                }
                the_format[0] += len;
            }
            while (at(format, the_format[0]) != 0)
            {
                if (xsltUtf8Charcmp(format, the_format[0], self.zero_digit) == 0)
                {
                    if (format_info.frac_hash != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    format_info.frac_digits++;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.digit) == 0)
                {
                    format_info.frac_hash++;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.percent) == 0)
                {
                    if (format_info.is_multiplier_set != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    delayed_multiplier = 100;
                    if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    the_format[0] += len;
                    continue;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.permille) == 0)
                {
                    if (format_info.is_multiplier_set != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    delayed_multiplier = 1000;
                    if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    the_format[0] += len;
                    continue;
                }
                else if (xsltUtf8Charcmp(format, the_format[0], self.grouping) != 0)
                {
                    break;
                }
                if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                {
                    found_error = 1;
                    break outputNumber;
                }
                the_format[0] += len;
                if (delayed_multiplier != 0)
                {
                    format_info.multiplier = delayed_multiplier;
                    delayed_multiplier = 0;
                    format_info.is_multiplier_set = 1;
                }
            }
            // A delayed multiplier after the number part belongs to the suffix.
            if (delayed_multiplier != 0)
            {
                the_format[0] -= len;
                delayed_multiplier = 0;
            }
            suffix = the_format[0];
            suffix_length = xsltFormatNumberPreSuffix(self, format, the_format, format_info);
            if (suffix_length < 0 || (at(format, the_format[0]) != 0 && xsltUtf8Charcmp(format, the_format[0], self.pattern_separator) != 0))
            {
                found_error = 1;
                break outputNumber;
            }
            // A negative number uses the negative prefix and suffix.
            if (number < 0)
            {
                // j is the number of utf-8 characters before the separator (bug 151975).
                j = xsltUtf8Strloc(format, self.pattern_separator);
                if (j < 0)
                {
                    // No negative pattern, use the default sign.
                    default_sign = 1;
                }
                else
                {
                    the_format[0] = xsltUtf8Strpos(format, j + 1);
                    format_info.is_negative_pattern = 1;
                    format_info.is_multiplier_set = 0;
                    // The negative prefix.
                    nprefix = the_format[0];
                    nprefix_length = xsltFormatNumberPreSuffix(self, format, the_format, format_info);
                    if (nprefix_length < 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    while (at(format, the_format[0]) != 0)
                    {
                        if (xsltUtf8Charcmp(format, the_format[0], self.percent) == 0 ||
                            xsltUtf8Charcmp(format, the_format[0], self.permille) == 0)
                        {
                            if (format_info.is_multiplier_set != 0)
                            {
                                found_error = 1;
                                break outputNumber;
                            }
                            format_info.is_multiplier_set = 1;
                            delayed_multiplier = 1;
                        }
                        else if (xsltIsSpecial(self, format, the_format[0]))
                        {
                            delayed_multiplier = 0;
                        }
                        else
                        {
                            break;
                        }
                        if ((len = xsltUtf8Size1(format, the_format[0])) < 1)
                        {
                            found_error = 1;
                            break outputNumber;
                        }
                        the_format[0] += len;
                    }
                    if (delayed_multiplier != 0)
                    {
                        format_info.is_multiplier_set = 0;
                        the_format[0] -= len;
                    }
                    // The negative suffix.
                    if (at(format, the_format[0]) != 0)
                    {
                        nsuffix = the_format[0];
                        nsuffix_length = xsltFormatNumberPreSuffix(self, format, the_format, format_info);
                        if (nsuffix_length < 0)
                        {
                            found_error = 1;
                            break outputNumber;
                        }
                    }
                    else
                    {
                        nsuffix_length = 0;
                    }
                    if (at(format, the_format[0]) != 0)
                    {
                        found_error = 1;
                        break outputNumber;
                    }
                    // A Java peculiarity: if the negative prefix and suffix are the positive ones, use the default.
                    if (nprefix_length != prefix_length || nsuffix_length != suffix_length ||
                        (nprefix_length > 0 && strncmpB(format, nprefix, format, prefix, prefix_length) != 0) ||
                        (nsuffix_length > 0 && strncmpB(format, nsuffix, format, suffix, suffix_length) != 0))
                    {
                        prefix = nprefix;
                        prefix_length = nprefix_length;
                        suffix = nsuffix;
                        suffix_length = nsuffix_length;
                    }
                }
            }
        }
        // OUTPUT_NUMBER:
        if (found_error != 0)
        {
            Xslt.settingsError(settings, null, fmt("xsltFormatNumberConversion : error in format string '%s', using default\n", format_str));
            default_sign = (number < 0.0) ? 1 : 0;
            prefix_length = suffix_length = 0;
            format_info.integer_hash = 0;
            format_info.integer_digits = 1;
            format_info.frac_digits = 1;
            format_info.frac_hash = 4;
            format_info.group = -1;
            format_info.multiplier = 1;
            format_info.add_decimal = 1;
        }
        // Apply the multiplier.
        number *= (double)format_info.multiplier;
        if (Double.isInfinite(number))
        {
            java.io.ByteArrayOutputStream mb = new java.io.ByteArrayOutputStream();
            if (number < 0)
            {
                if (self.minus_sign != null) appendB(mb, self.minus_sign, 0);
                else appendB(mb, "-");
            }
            if (self.infinity == null) appendB(mb, "Infinity");
            else appendB(mb, self.infinity, 0);
            return new String(mb.toByteArray(), StandardCharsets.UTF_8);
        }
        java.io.ByteArrayOutputStream buffer = new java.io.ByteArrayOutputStream();
        // The default sign.
        if (default_sign != 0) appendRegionB(buffer, self.minus_sign, 0, xsltUtf8Size1(self.minus_sign, 0));
        // The prefix.
        for (j = 0; j < prefix_length;)
        {
            if (at(format, prefix) == '\'') prefix++;
            len = xsltUtf8Size1(format, prefix);
            appendRegionB(buffer, format, prefix, len);
            prefix += len;
            j += len;
        }
        // Round to n digits.
        number = Math.abs(number);
        int exp10 = format_info.frac_digits + format_info.frac_hash;
        if (exp10 > DBL_MAX_10_EXP)
        {
            if (format_info.frac_digits > DBL_MAX_10_EXP)
            {
                format_info.frac_digits = DBL_MAX_10_EXP;
                format_info.frac_hash = 0;
            }
            else
            {
                format_info.frac_hash = DBL_MAX_10_EXP - format_info.frac_digits;
            }
            exp10 = DBL_MAX_10_EXP;
        }
        // pow(10.0, exp10), correctly rounded.
        double scale = Double.parseDouble("1e" + exp10);
        number += .5 / scale;
        number -= number % (1 / scale);
        int zero_digit0 = at(self.zero_digit, 0);
        // The integer part.
        if (self.grouping != null && at(self.grouping, 0) != 0)
        {
            int[] glen = { 0 };
            int gchar = utf8CharB(self.grouping, 0, glen);
            len = strlenB(self.grouping, 0);
            if (glen[0] > 0) len = glen[0];
            xsltNumberFormatDecimal(buffer, Math.floor(number), zero_digit0, format_info.integer_digits,
                                    format_info.group, gchar, len, settings);
        }
        else
        {
            xsltNumberFormatDecimal(buffer, Math.floor(number), zero_digit0, format_info.integer_digits,
                                    format_info.group, ',', 1, settings);
        }
        // Special case: java treats '.#' like '.0', '.##' like '.0#', etc.
        if (format_info.integer_digits + format_info.integer_hash + format_info.frac_digits == 0 && format_info.frac_hash > 0)
        {
            ++format_info.frac_digits;
            --format_info.frac_hash;
        }
        // A leading zero, if required.
        if (Math.floor(number) == 0 && format_info.integer_digits + format_info.frac_digits == 0)
        {
            appendRegionB(buffer, self.zero_digit, 0, xsltUtf8Size1(self.zero_digit, 0));
        }
        // The fractional part, if required.
        if (format_info.frac_digits + format_info.frac_hash == 0)
        {
            if (format_info.add_decimal != 0)
                appendRegionB(buffer, self.decimal_point, 0, xsltUtf8Size1(self.decimal_point, 0));
        }
        else
        {
            number -= Math.floor(number);
            if (number != 0 || format_info.frac_digits != 0)
            {
                appendRegionB(buffer, self.decimal_point, 0, xsltUtf8Size1(self.decimal_point, 0));
                number = Math.floor(scale * number + 0.5);
                for (j = format_info.frac_hash; j > 0; j--)
                {
                    if (number % 10.0 >= 1.0) break;
                    number /= 10.0;
                }
                xsltNumberFormatDecimal(buffer, Math.floor(number), zero_digit0, format_info.frac_digits + j, 0, 0, 0, settings);
            }
        }
        // The suffix.
        for (j = 0; j < suffix_length;)
        {
            if (at(format, suffix) == '\'') suffix++;
            len = xsltUtf8Size1(format, suffix);
            appendRegionB(buffer, format, suffix, len);
            suffix += len;
            j += len;
        }
        return new String(buffer.toByteArray(), StandardCharsets.UTF_8);
    }

    /** Port of xsltFormatNumberFunction: format-number(). */
    private static XPathResult xsltFormatNumberFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        // Without an instruction libxslt returns without a value, which is a stack error.
        if (t.inst == null)
        {
            ctx.setError(23);
            return null;
        }
        Stylesheet sheet = t.style;
        DecimalFormat format_values = sheet.decimal_format;
        int nargs = args.length;
        if (nargs != 2 && nargs != 3)
        {
            ctx.setError(XPathException.INVALID_ARITY);
            return null;
        }
        if (nargs == 3)
        {
            String qname = args[2].castToString();
            String[] prefix = { null };
            String ncname = Xslt.splitQname(qname, prefix);
            String ns_uri = null;
            if (prefix[0] != null)
            {
                XmlNs ns = lxSearchNs(t.inst, prefix[0]);
                if (ns == null)
                {
                    transformError(t, null, null, fmt("format-number : No namespace found for QName '%s:%s'\n", prefix[0], ncname));
                    sheet.errors++;
                    ncname = null;
                }
                else
                {
                    ns_uri = nsHref(ns);
                }
            }
            if (ncname != null) format_values = Xslt.decimalFormatGetByQname(sheet, ns_uri, ncname);
            if (format_values == null) transformError(t, null, null, fmt("format-number() : undeclared decimal format '%s'\n", qname));
        }
        if (format_values == null)
        {
            // Nothing is pushed, a stack error in libxml2.
            ctx.setError(23);
            return null;
        }
        String format = args[1].castToString();
        double number = args[0].castToNumber();
        String result = xsltFormatNumberConversion(format_values, format, number, sheet.settings);
        return XPathResult.newString(result != null ? result : "");
    }

    // Keys, port of libxslt keys.c //////////////////////////////////

    /** Port of xsltFreeDocumentKeys. */
    private static void xsltFreeDocKeys(DocInfo info)
    {
        if (info == null) return;
        info.keys = null;
    }

    /** Port of xsltInitCtxtKey: compute the key table of key_def for the document idoc. */
    private static int xsltInitCtxtKey(TransformCtx t, DocInfo idoc, KeyDef key_def)
    {
        if (key_def.comp == null || key_def.usecomp == null) return -1;
        // Detect recursive keys.
        if (t.key_init_level > t.nb_keys)
        {
            transformError(t, null, key_def.inst, fmt("Key definition for %s is recursive\n", key_def.name));
            t.state = TransformCtx.STATE_STOPPED;
            return -1;
        }
        t.key_init_level++;
        idoc.nb_keys_computed++;
        XmlNode old_inst = t.inst;
        DocInfo old_doc_info = t.document;
        XmlNode old_context_node = t.node;
        long old_pos = t.xp_position, old_size = t.xp_size;
        XPathResult use_res = null;
        t.document = idoc;
        t.node = domDocument(idoc.doc);
        t.inst = key_def.inst;
        error:
        {
            // Evaluate the match expression of the xsl:key (with the context size and position as they are).
            XPathResult match_res = xsltXpathEval(t, key_def.comp, domDocument(idoc.doc));
            if (match_res == null)
            {
                transformError(t, null, key_def.inst, "Failed to evaluate the 'match' expression.\n");
                t.state = TransformCtx.STATE_STOPPED;
                break error;
            }
            if (match_res.type() != XPathResult.NODESET || match_res.isTree())
            {
                transformError(t, null, key_def.inst, "The 'match' expression did not evaluate to a node set.\n");
                t.state = TransformCtx.STATE_STOPPED;
                break error;
            }
            int nmatch = match_res.size();
            if (nmatch == 0) break error;
            // Multiple key definitions for the same name are allowed.
            KeyTable table = idoc.keys;
            while (table != null)
            {
                if (streq(table.name, key_def.name) && streq(table.name_uri, key_def.name_uri)) break;
                table = table.next;
            }
            if (table == null)
            {
                table = new KeyTable();
                table.name = key_def.name;
                table.name_uri = key_def.name_uri;
                table.keys = new HashMap<>();
                table.next = idoc.keys;
                idoc.keys = table;
            }
            // The use expression is evaluated with a node list containing just the node.
            t.xp_size = 1;
            t.xp_position = 1;
            for (int i = 0; i < nmatch; i++)
            {
                XmlNode cur = match_res.node(i);
                if (!lxIsRealNode(cur)) continue;
                t.node = cur;
                use_res = xsltXpathEval(t, key_def.usecomp, cur);
                if (use_res == null)
                {
                    transformError(t, null, key_def.inst, "Failed to evaluate the 'use' expression.\n");
                    t.state = TransformCtx.STATE_STOPPED;
                    break;
                }
                int len;
                boolean nodes = use_res.type() == XPathResult.NODESET;
                if (nodes)
                {
                    len = use_res.size();
                    if (len == 0) continue;
                }
                else
                {
                    len = 1;
                }
                for (int k = 0; k < len; k++)
                {
                    String str = nodes ? nodeStringValue(use_res.node(k)) : use_res.castToString();
                    if (str == null) continue;
                    XPathResult keylist = table.keys.get(str);
                    if (keylist == null)
                    {
                        keylist = XPathResult.newNodeSet();
                        keylist.addNode(cur);
                        table.keys.put(str, keylist);
                    }
                    else
                    {
                        // xmlXPathNodeSetAdd checks for duplicates.
                        int n = keylist.size();
                        boolean dup = false;
                        for (int jj = 0; jj < n && !dup; jj++) dup = keylist.node(jj) == cur;
                        if (!dup) keylist.addNode(cur);
                    }
                    if (t.keyed_nodes == null) t.keyed_nodes = new IdentityHashMap<>();
                    t.keyed_nodes.put(cur, Boolean.TRUE);
                }
            }
        }
        t.key_init_level--;
        t.xp_position = old_pos;
        t.xp_size = old_size;
        t.node = old_context_node;
        t.document = old_doc_info;
        t.inst = old_inst;
        return 0;
    }

    /** Port of xsltInitDocKeyTable. */
    private static int xsltInitDocKeyTable(TransformCtx t, String name, String name_uri)
    {
        KeyDef keyd = null;
        int found = 0;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            for (keyd = style.keys; keyd != null; keyd = keyd.next)
            {
                if (streq(keyd.name, name) && streq(keyd.name_uri, name_uri))
                {
                    xsltInitCtxtKey(t, t.document, keyd);
                    if (t.document.nb_keys_computed == t.nb_keys) return 0;
                    found = 1;
                }
            }
        }
        if (found == 0)
        {
            transformError(t, null, keyd != null ? keyd.inst : null, fmt("Failed to find key definition for %s\n", name));
            t.state = TransformCtx.STATE_STOPPED;
            return -1;
        }
        return 0;
    }

    /** Port of xsltInitAllDocKeys. */
    private static int xsltInitAllDocKeys(TransformCtx t)
    {
        if (t.document.nb_keys_computed == t.nb_keys) return 0;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            for (KeyDef keyd = style.keys; keyd != null; keyd = keyd.next)
            {
                // Check if keys with this QName have been already computed.
                KeyTable table = t.document.keys;
                while (table != null)
                {
                    if (streq(keyd.name, table.name) && streq(keyd.name_uri, table.name_uri)) break;
                    table = table.next;
                }
                if (table == null) xsltInitDocKeyTable(t, keyd.name, keyd.name_uri);
            }
        }
        return 0;
    }

    /**
     * Port of xsltGetKey: the nodes of the current document with the key value (borrowed).
     * @param t The transformation.
     * @param name The name of the key.
     * @param name_uri The namespace of the name.
     * @param value The key value.
     * @return The nodes or null.
     */
    static XPathResult getKey(TransformCtx t, String name, String name_uri, String value)
    {
        int init_table = 0;
        if (name == null || value == null || t.document == null) return null;
        // Keys are computed on demand, on the first key access for a document.
        if (t.document.nb_keys_computed < t.nb_keys && t.key_init_level == 0)
        {
            if (xsltInitAllDocKeys(t) != 0) return null;
        }
        for (;;)
        {
            for (KeyTable table = t.document.keys; table != null; table = table.next)
            {
                if (streq(table.name, name) && streq(table.name_uri, name_uri))
                {
                    return table.keys.get(value);
                }
            }
            if (t.key_init_level != 0 && init_table == 0)
            {
                // A recursive key, initialize just this one and retry.
                xsltInitDocKeyTable(t, name, name_uri);
                init_table = 1;
                continue;
            }
            return null;
        }
    }

    /**
     * Does the node have a key (libxslt XSLT_SOURCE_NODE_HAS_KEY)?
     * @param t The transformation.
     * @param node The node.
     * @return True if a key table holds the node.
     */
    static boolean nodeHasKey(TransformCtx t, XmlNode node)
    {
        return t.keyed_nodes != null && t.keyed_nodes.containsKey(node);
    }

    /**
     * Port of xsltComputeAllKeys (for the keyed template patterns).
     * @param t The transformation.
     * @param context_node The context node.
     * @return 0 or -1.
     */
    static int computeAllKeys(TransformCtx t, XmlNode context_node)
    {
        if (context_node == null)
        {
            transformError(t, null, t.inst, "Internal error in xsltComputeAllKeys(): Bad arguments.\n");
            return -1;
        }
        if (t.document == null) return -1;
        return xsltInitAllDocKeys(t);
    }

    /** Port of xsltFindDocument. */
    private static DocInfo xsltFindDocument(TransformCtx t, XmlDocument doc)
    {
        for (DocInfo ret = t.doc_list; ret != null; ret = ret.next)
        {
            if (ret.doc == doc) return ret;
        }
        if (doc == t.style.doc) return t.document;
        return null;
    }

    // Documents, port of libxslt documents.c (xsltLoadDocument) /////

    private static DocInfo xsltNewDocument(TransformCtx t, XmlDocument doc, int owned)
    {
        DocInfo cur = new DocInfo();
        cur.doc = doc;
        cur.owned = owned;
        if (!isRtf(t, doc))
        {
            cur.next = t.doc_list;
            t.doc_list = cur;
        }
        return cur;
    }

    /** Port of xsltLoadDocument. */
    private static DocInfo xsltLoadDocument(TransformCtx t, String uri)
    {
        if (uri == null) return null;
        for (DocInfo ret = t.doc_list; ret != null; ret = ret.next)
        {
            if (ret.doc != null && domUrl(ret.doc) != null && domUrl(ret.doc).equals(uri)) return ret;
        }
        XmlDocument doc = Xslt.load(t.style, uri, Settings.LOAD_DOCUMENT);
        if (doc == null) return null;
        // Apply white-space stripping if asked for.
        if (xsltNeedElemSpaceHandling(t) != 0) xsltApplyStripSpaces(t, domRootElement(doc));
        return xsltNewDocument(t, doc, 1);
    }

    /** A copy of the stylesheet document (xmlCopyDoc), for document(''). */
    private static XmlDocument xsltCopyDoc(XmlDocument src)
    {
        XmlDocument doc = domNew();
        domSetUrl(doc, domUrl(src));
        if (domVersion(src) != null) domSetVersion(doc, domVersion(src));
        if (domEncoding(src) != null) domSetEncoding(doc, domEncoding(src));
        for (XmlNode c = nodeFirstChild(domDocument(src)); c != null; c = nodeNext(c))
        {
            if (nodeKind(c) == DTD)
            {
                domSetDtd(doc, nodeName(c), dtdPublicId(c), dtdSystemId(c));
                continue;
            }
            XmlNode copy = nodeCopy(doc, c, true);
            if (copy != null) nodeAppendChild(domDocument(doc), copy);
        }
        // The copied namespaces are references, make every subtree self contained.
        for (XmlNode c = nodeFirstChild(domDocument(doc)); c != null; c = nodeNext(c))
        {
            if (nodeKind(c) == ELEMENT) nodeReconcileNs(c);
        }
        return doc;
    }

    /**
     * A copy of the (compiled) stylesheet document, like the xmlCopyDoc loaders give for document('').
     * @param style The stylesheet.
     * @return The copy or null.
     */
    public static XmlDocument copyStylesheetDoc(Stylesheet style)
    {
        return (style != null && style.doc != null) ? xsltCopyDoc(style.doc) : null;
    }

    // Functions, port of libxslt functions.c and extra.c ////////////

    // XPointer, port of the parts of libxml2 xpointer.c used by document() /////

    /** The state of an XPointer evaluation (XsltXPtr), cur is a char index into str. */
    private static final class XPtr
    {
        XmlDocument doc;
        String str;
        int cur;
        boolean error;
        LinkedHashMap<String, String> ns = new LinkedHashMap<>(); // From xmlns() parts.

        int ch(int i)
        {
            return cur + i < str.length() ? str.charAt(cur + i) : 0;
        }

        XPtr copy()
        {
            XPtr x = new XPtr();
            x.doc = doc;
            x.str = str;
            x.cur = cur;
            x.error = error;
            x.ns = ns;
            return x;
        }
    }

    private static boolean xsltXptrIsNameStart(int c)
    {
        return XmlChars.isLetter(c) || c == '_' || c == ':';
    }

    private static boolean xsltXptrIsNameChar(int c)
    {
        return XmlChars.isLetter(c) || XmlChars.isDigit(c) || c == '.' || c == '-' || c == '_' || c == ':' ||
               XmlChars.isCombining(c) || XmlChars.isExtender(c);
    }

    /** The code point at the position, 0 at the end. */
    private static int xptrChar(XPtr x, int[] len)
    {
        if (x.cur >= x.str.length())
        {
            len[0] = 1;
            return 0;
        }
        int c = x.str.codePointAt(x.cur);
        len[0] = Character.charCount(c);
        return c;
    }

    /** xmlXPathParseName: a QName at cur, null if there is none. */
    private static String xsltXptrParseName(XPtr x)
    {
        int[] len = { 0 };
        int start = x.cur;
        int c = xptrChar(x, len);
        if (c <= 0 || !xsltXptrIsNameStart(c)) return null;
        while (c > 0 && xsltXptrIsNameChar(c))
        {
            x.cur += len[0];
            c = xptrChar(x, len);
        }
        return x.str.substring(start, x.cur);
    }

    private static void xsltXptrSkipBlanks(XPtr x)
    {
        while (x.cur < x.str.length() && Xslt.isBlank(x.str.charAt(x.cur))) x.cur++;
    }

    /** xmlXPtrGetNthChild: the no-th element child (counting from 1). */
    private static XmlNode xsltXptrNthChild(XmlNode cur, int no)
    {
        if (cur == null || nodeKind(cur) == NAMESPACE) return cur;
        int i = 0;
        for (cur = nodeFirstChild(cur); i <= no; cur = nodeNext(cur))
        {
            if (cur == null) return cur;
            if (nodeKind(cur) == ELEMENT || nodeKind(cur) == DOCUMENT)
            {
                i++;
                if (i == no) break;
            }
        }
        return cur;
    }

    /** xmlXPtrEvalChildSeq: a bare name (an id) and/or /n/m child steps, from the node set. */
    private static XPathResult xsltXptrChildSeq(XPtr x, XPathEvalContext ctx, String name, XPathResult set)
    {
        if (name != null)
        {
            set = XPathResult.newNodeSet();
            XmlNode e = ctx.getId(domDocument(x.doc), name);
            if (e != null) set.addNode(e);
        }
        while (x.ch(0) == '/')
        {
            int child = 0;
            boolean overflow = false;
            x.cur++;
            while (x.ch(0) >= '0' && x.ch(0) <= '9')
            {
                int d = x.ch(0) - '0';
                if (child > Integer.MAX_VALUE / 10) overflow = true;
                else child *= 10;
                if (child > Integer.MAX_VALUE - d) overflow = true;
                else child += d;
                x.cur++;
            }
            if (overflow) child = 0;
            // xmlXPtrGetChildNo.
            XmlNode cur = null;
            if (child > 0 && set != null && set.size() == 1) cur = xsltXptrNthChild(set.node(0), child);
            set = XPathResult.newNodeSet();
            if (cur != null) set.addNode(cur);
        }
        return set;
    }

    /** xmlXPtrEvalXPtrPart: one scheme(...) part, null if it gives nothing. */
    private static XPathResult xsltXptrPart(XPtr x, XPathEvalContext ctx, String name)
    {
        if (x.ch(0) != '(')
        {
            x.error = true;
            return null;
        }
        x.cur++;
        int level = 1;
        StringBuilder mb = new StringBuilder();
        while (x.ch(0) != 0)
        {
            if (x.ch(0) == ')')
            {
                level--;
                if (level == 0)
                {
                    x.cur++;
                    break;
                }
            }
            else if (x.ch(0) == '(')
            {
                level++;
            }
            else if (x.ch(0) == '^')
            {
                if (x.ch(1) == ')' || x.ch(1) == '(' || x.ch(1) == '^') x.cur++;
            }
            mb.append((char)x.ch(0));
            x.cur++;
        }
        String buffer = mb.toString();
        if (level != 0 && x.ch(0) == 0)
        {
            x.error = true;
            return null;
        }
        XPathResult res = null;
        if (name.equals("xpointer") || name.equals("xpath1"))
        {
            // Evaluated with the document as context node, position and size 1.
            XPathSettings st = new XPathSettings();
            st.namespaces = new LinkedHashMap<>(x.ns);
            try
            {
                XPath comp = XPath.compile(buffer, st);
                res = ctx.eval(comp, domDocument(x.doc), 1, 1);
            }
            catch (XPathException e)
            {
                res = null;
            }
            if (res == null) x.error = true;
        }
        else if (name.equals("element"))
        {
            XPtr sub = x.copy();
            sub.str = buffer;
            sub.cur = 0;
            if (buffer.startsWith("/"))
            {
                XPathResult root = XPathResult.newNodeSet();
                root.addNode(domDocument(x.doc));
                res = xsltXptrChildSeq(sub, ctx, null, root);
            }
            else
            {
                String name2 = xsltXptrParseName(sub);
                if (name2 == null) x.error = true;
                else res = xsltXptrChildSeq(sub, ctx, name2, null);
            }
        }
        else if (name.equals("xmlns"))
        {
            XPtr sub = x.copy();
            sub.str = buffer;
            sub.cur = 0;
            String prefix = xsltXptrParseName(sub);
            if (prefix != null && prefix.indexOf(':') < 0)
            {
                xsltXptrSkipBlanks(sub);
                if (sub.ch(0) == '=')
                {
                    sub.cur++;
                    xsltXptrSkipBlanks(sub);
                    x.ns.putIfAbsent(prefix, sub.str.substring(sub.cur));
                }
                else
                {
                    x.error = true;
                }
            }
            else
            {
                x.error = true;
            }
        }
        // Unknown schemes are skipped (XML_XPTR_UNKNOWN_SCHEME).
        return res;
    }

    /** Port of xmlXPtrEval: the nodes the fragment identifier selects in doc, null on errors. */
    private static XPathResult xsltXptrEval(XmlDocument doc, String str)
    {
        XPtr x = new XPtr();
        x.doc = doc;
        x.str = str;
        x.cur = 0;
        XPathEvalContext ctx = new XPathEvalContext();
        XPathResult res = null;
        xsltXptrSkipBlanks(x);
        done:
        {
            if (x.ch(0) == '/')
            {
                XPathResult root = XPathResult.newNodeSet();
                root.addNode(domDocument(doc));
                res = xsltXptrChildSeq(x, ctx, null, root);
            }
            else
            {
                String name = xsltXptrParseName(x);
                if (name == null)
                {
                    x.error = true;
                }
                else if (x.ch(0) == '(')
                {
                    // xmlXPtrEvalFullXPtr: the parts until one gives a non empty node set.
                    while (name != null)
                    {
                        res = xsltXptrPart(x, ctx, name);
                        if (x.error) break;
                        if (res != null && res.type() == XPathResult.NODESET && res.size() > 0) break;
                        res = null;
                        xsltXptrSkipBlanks(x);
                        name = xsltXptrParseName(x);
                    }
                    break done;
                }
                else
                {
                    // Bare names and child sequences.
                    res = xsltXptrChildSeq(x, ctx, name, null);
                }
            }
            xsltXptrSkipBlanks(x);
            if (x.ch(0) != 0) x.error = true;
        }
        if (x.error || (res != null && res.type() != XPathResult.NODESET))
        {
            res = null;
        }
        return res;
    }

    /** Port of xsltDocumentFunctionLoadDocument: add the nodes of document uri (with fragment) to ret. */
    private static void xsltDocumentFunctionLoadDocument(TransformCtx t, XPathResult ret, String uri, String fragment)
    {
        DocInfo idoc = xsltLoadDocument(t, uri);
        XmlDocument doc;
        if (idoc == null)
        {
            if (uri == null || uri.startsWith("#") || (t.style.doc != null && streq(domUrl(t.style.doc), uri)))
            {
                // This selects the stylesheet's document itself (a copy without the precomputed data).
                doc = xsltCopyDoc(t.style.doc);
                idoc = xsltNewDocument(t, doc, 1);
            }
            else
            {
                return;
            }
        }
        else
        {
            doc = idoc.doc;
        }
        if (fragment == null)
        {
            ret.addNode(domDocument(doc));
            return;
        }
        // The fragment identifier is an XPointer.
        XPathResult res_obj = xsltXptrEval(doc, fragment);
        if (res_obj != null)
        {
            xsltNodesetMerge(ret, res_obj);
        }
    }

    /** xmlXPathNodeSetMerge: add the nodes of from that are not in to. */
    private static void xsltNodesetMerge(XPathResult to, XPathResult from)
    {
        int nf = from != null ? from.size() : 0;
        for (int i = 0; i < nf; i++)
        {
            XmlNode n = from.node(i);
            int nt = to.size();
            boolean dup = false;
            for (int j = 0; j < nt && !dup; j++) dup = to.node(j) == n;
            if (!dup) to.addNode(n);
        }
    }

    private static boolean xsltIsStrictNodeset(XPathResult r)
    {
        return r != null && r.type() == XPathResult.NODESET && !r.isTree();
    }

    /** document() for one uri string with an optional base node set (obj2). */
    private static XPathResult xsltDocumentString(TransformCtx t, String url_in, XPathResult obj2)
    {
        XPathResult ret = XPathResult.newNodeSet();
        XmlUri uri = XmlUri.parse(url_in);
        if (uri == null)
        {
            transformError(t, null, null, fmt("document() : failed to parse URI '%s'\n", url_in));
            return ret;
        }
        // Check for and remove the fragment identifier.
        String fragment = uri.fragment;
        String url = url_in;
        if (fragment != null)
        {
            uri.fragment = null;
            url = uri.save();
        }
        String base = null;
        if (obj2 != null && obj2.size() > 0 && lxIsRealNode(obj2.node(0)))
        {
            XmlNode target = obj2.node(0);
            if (nodeKind(target) == ATTRIBUTE || nodeKind(target) == PI) target = nodeParent(target);
            base = Xslt.nodeGetBase(nodeDom(target), target);
        }
        else if (t.inst != null)
        {
            base = Xslt.nodeGetBase(nodeDom(t.inst), t.inst);
        }
        else if (t.style.doc != null)
        {
            base = Xslt.nodeGetBase(t.style.doc, domDocument(t.style.doc));
        }
        String full = XmlUri.build(url, base);
        if (full != null)
        {
            xsltDocumentFunctionLoadDocument(t, ret, full, fragment);
        }
        return ret;
    }

    /** Port of xsltDocumentFunction: document(). */
    private static XPathResult xsltDocumentFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        int nargs = args.length;
        if (nargs < 1 || nargs > 2)
        {
            transformError(t, null, null, fmt("document() : invalid number of args %d\n", nargs));
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        XPathResult obj2 = null;
        if (nargs == 2)
        {
            if (!xsltIsStrictNodeset(args[1]))
            {
                transformError(t, null, null, "document() : invalid arg expecting a nodeset\n");
                ctx.setErrorQuiet(XPathException.INVALID_TYPE);
                return null;
            }
            obj2 = args[1];
        }
        if (xsltIsStrictNodeset(args[0]))
        {
            XPathResult ret = XPathResult.newNodeSet();
            int n = args[0].size();
            for (int i = 0; i < n; i++)
            {
                XmlNode node = args[0].node(i);
                String s = nodeStringValue(node);
                XPathResult base = XPathResult.newNodeSet();
                if (nargs == 2)
                {
                    for (int j = 0; j < obj2.size(); j++) base.addNode(obj2.node(j));
                }
                else
                {
                    base.addNode(node);
                }
                XPathResult newobj = xsltDocumentString(t, s, base);
                xsltNodesetMerge(ret, newobj);
            }
            return ret;
        }
        String s = args[0].castToString();
        return xsltDocumentString(t, s, obj2);
    }

    /** Port of xsltKeyFunction: key(). */
    private static XPathResult xsltKeyFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        int nargs = args.length;
        if (nargs != 2)
        {
            transformError(t, null, null, "key() : expects two arguments\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        String qname = args[0].castToString();
        XPathResult ret = XPathResult.newNodeSet();
        XPathResult obj2 = args[1];
        String[] values;
        int nvalues;
        if (obj2.type() == XPathResult.NODESET)
        {
            nvalues = obj2.size();
            values = new String[nvalues];
            for (int i = 0; i < nvalues; i++) values[i] = nodeStringValue(obj2.node(i));
        }
        else
        {
            nvalues = 1;
            values = new String[] { obj2.castToString() };
        }
        for (int vi = 0; vi < nvalues; vi++)
        {
            DocInfo old_doc_info = t.document;
            XmlNode node = ctx.contextNode();
            if (node == null)
            {
                transformError(t, null, t.inst, "Internal error in xsltKeyFunction(): The context node is not set on the XPath context.\n");
                t.state = TransformCtx.STATE_STOPPED;
                t.document = old_doc_info;
                continue;
            }
            // The name, with the namespace of a prefix.
            String prefix = null;
            String key = null;
            String key_uri = null;
            int colon = qname.indexOf(':');
            if (colon > 0 && colon + 1 < qname.length())
            {
                prefix = qname.substring(0, colon);
                key = qname.substring(colon + 1);
                key_uri = ctx.lookupNamespace(prefix);
                if (key_uri == null) transformError(t, null, t.inst, fmt("key() : prefix %s is not bound\n", prefix));
            }
            else
            {
                key = qname;
            }
            XmlNode tmp_node = node;
            if (nodeKind(node) == NAMESPACE) tmp_node = nodeParent(node);
            XPathResult nodelist = null;
            if (tmp_node == null || nodeDom(tmp_node) == null)
            {
                transformError(t, null, t.inst, "Internal error in xsltKeyFunction(): Couldn't get the doc of the XPath context node.\n");
            }
            else
            {
                XmlDocument doc = nodeDom(tmp_node);
                boolean ok = true;
                if (t.document == null || t.document.doc != doc)
                {
                    RVT rvt = xsltRvtOf(doc);
                    if (rvt != null)
                    {
                        // A result tree fragment.
                        if (rvt.keys == null) rvt.keys = xsltNewDocument(t, doc, 0);
                        t.document = rvt.keys;
                    }
                    else
                    {
                        // The source document or one from document().
                        t.document = xsltFindDocument(t, doc);
                    }
                    if (t.document == null)
                    {
                        transformError(t, null, t.inst, "Internal error in xsltKeyFunction(): Could not get the document info of a context doc.\n");
                        t.state = TransformCtx.STATE_STOPPED;
                        ok = false;
                    }
                }
                if (ok) nodelist = getKey(t, key, key_uri, values[vi]);
            }
            t.document = old_doc_info;
            if (nodelist != null) xsltNodesetMerge(ret, nodelist);
        }
        return ret;
    }

    /** Port of xsltUnparsedEntityURIFunction: unparsed-entity-uri(). */
    private static XPathResult xsltUnparsedEntityUriFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 1)
        {
            genericError(t.style.settings, "unparsed-entity-uri() : expects one string arg\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        String str = args[0].castToString();
        XmlNode node = ctx.contextNode();
        XmlDocument doc = node != null ? nodeDom(node) : null;
        XmlNode entity = (doc != null && str != null) ? domGetEntity(doc, str) : null;
        XPathResult ret;
        if (entity != null && entityDeclSystemId(entity) != null)
        {
            // libxml2 entity->URI: the system id resolved against the document.
            String uri = XmlUri.build(entityDeclSystemId(entity), domUrl(doc));
            ret = XPathResult.newString(uri != null ? uri : entityDeclSystemId(entity));
        }
        else
        {
            ret = XPathResult.newString("");
        }
        return ret;
    }

    /** Port of xsltGenerateIdFunction: generate-id() gives id1, id2, ... in the order of the calls. */
    private static XPathResult xsltGenerateIdFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        XmlNode cur = null;
        int nargs = args.length;
        if (nargs == 0)
        {
            cur = ctx.contextNode();
        }
        else if (nargs == 1)
        {
            if (!xsltIsStrictNodeset(args[0]))
            {
                ctx.setErrorQuiet(XPathException.INVALID_TYPE);
                transformError(t, null, null, "generate-id() : invalid arg expecting a node-set\n");
                return null;
            }
            int n = args[0].size();
            if (n == 0) return XPathResult.newString("");
            // The first node in document order.
            cur = args[0].node(0);
            for (int i = 1; i < n; i++)
            {
                XmlNode o = args[0].node(i);
                if (nodeCompareOrder(o, cur) < 0) cur = o;
            }
        }
        else
        {
            transformError(t, null, null, fmt("generate-id() : invalid number of args %d\n", nargs));
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        String ns_prefix = null;
        boolean is_ns = false;
        if (nodeKind(cur) == NAMESPACE)
        {
            is_ns = true;
            ns_prefix = nodeName(cur) != null ? nodeName(cur) : "";
            cur = nodeParent(cur);
        }
        if (t.generated_ids == null) t.generated_ids = new IdentityHashMap<>();
        String id = t.generated_ids.get(cur);
        if (id == null)
        {
            id = Long.toUnsignedString(++t.current_id);
            t.generated_ids.put(cur, id);
        }
        StringBuilder mb = new StringBuilder();
        if (!is_ns)
        {
            mb.append("id").append(id);
        }
        else
        {
            // Only ascii alphanumerics are allowed, the prefix is hex encoded.
            mb.append("id").append(id).append("ns");
            for (byte b : ns_prefix.getBytes(StandardCharsets.UTF_8))
            {
                int p = b & 0xff;
                int v = p >> 4;
                mb.append((char)(v < 10 ? '0' + v : 'A' + (v - 10)));
                v = p & 15;
                mb.append((char)(v < 10 ? '0' + v : 'A' + (v - 10)));
            }
        }
        return XPathResult.newString(mb.toString());
    }

    /** Split a QName like xmlSplitQName2: null if there is no (valid) prefix. */
    private static String xsltSplitQname2(String qname, String[] prefix)
    {
        prefix[0] = null;
        if (qname == null || qname.startsWith(":")) return null;
        int colon = qname.indexOf(':');
        if (colon < 0 || colon + 1 >= qname.length()) return null;
        prefix[0] = qname.substring(0, colon);
        return qname.substring(colon + 1);
    }

    /** Port of xsltSystemPropertyFunction: system-property(). */
    private static XPathResult xsltSystemPropertyFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 1)
        {
            transformError(t, null, null, "system-property() : expects one string arg\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        if (args[0].type() != XPathResult.STRING)
        {
            transformError(t, null, null, "system-property() : invalid arg expecting a string\n");
            ctx.setErrorQuiet(XPathException.INVALID_TYPE);
            return null;
        }
        String s = args[0].stringValue();
        String[] prefix = { null };
        String ns_uri = null;
        String name = xsltSplitQname2(s, prefix);
        if (name == null)
        {
            name = s;
        }
        else
        {
            ns_uri = ctx.lookupNamespace(prefix[0]);
            if (ns_uri == null) transformError(t, null, null, fmt("system-property() : prefix %s is not bound\n", prefix[0]));
        }
        String value = "";
        if (streq(ns_uri, XSLT_NAMESPACE))
        {
            if (name.equals("vendor")) value = "libxslt";
            else if (name.equals("version")) value = "1.0";
            else if (name.equals("vendor-url")) value = "http://xmlsoft.org/XSLT/";
        }
        return XPathResult.newString(value);
    }

    // The xslt elements registered by xsltRegisterAllElement.
    private static final String[] XSLT_ELEMENTS = { "apply-templates", "apply-imports", "call-template", "element", "attribute",
        "text", "processing-instruction", "comment", "copy", "value-of", "number", "for-each", "if", "choose", "sort",
        "copy-of", "message", "variable", "param", "with-param", "decimal-format", "when", "otherwise", "fallback" };

    /** The extension elements libxslt knows (xsltExtElementLookup with the elements of extra.c). */
    private static boolean xsltExtElementAvailable(String name, String uri)
    {
        if (name == null || uri == null) return false;
        if (uri.equals(XSLT_NAMESPACE))
        {
            for (String e : XSLT_ELEMENTS) if (name.equals(e)) return true;
        }
        if (uri.equals(XSLT_LIBXSLT_NAMESPACE) && name.equals("debug")) return true;
        if (uri.equals(XSLT_SAXON_NAMESPACE) && name.equals("output")) return true;
        if (uri.equals(XSLT_XALAN_NAMESPACE) && name.equals("write")) return true;
        if (uri.equals(XSLT_XT_NAMESPACE) && name.equals("document")) return true;
        if (uri.equals(XSLT_NAMESPACE) && name.equals("document")) return true;
        return false;
    }

    /** Port of xsltElementAvailableFunction: element-available(). */
    private static XPathResult xsltElementAvailableFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 1)
        {
            transformError(t, null, null, "element-available() : expects one string arg\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        String s = args[0].castToString();
        if (t.inst == null)
        {
            transformError(t, null, null, "element-available() : internal error tctxt == NULL\n");
            return XPathResult.newBoolean(false);
        }
        String[] prefix = { null };
        String ns_uri = null;
        String name = xsltSplitQname2(s, prefix);
        if (name == null)
        {
            name = s;
            XmlNs ns = lxSearchNs(t.inst, null);
            if (ns != null) ns_uri = nsHref(ns);
        }
        else
        {
            ns_uri = ctx.lookupNamespace(prefix[0]);
            if (ns_uri == null) transformError(t, null, null, fmt("element-available() : prefix %s is not bound\n", prefix[0]));
        }
        boolean r = xsltExtElementAvailable(name, ns_uri);
        return XPathResult.newBoolean(r);
    }

    private static final String[] CORE_FUNCTIONS = { "boolean", "ceiling", "count", "concat", "contains", "id", "false", "floor", "last",
        "lang", "local-name", "not", "name", "namespace-uri", "normalize-space", "number", "position", "round", "string",
        "string-length", "starts-with", "substring", "substring-before", "substring-after", "sum", "true", "translate" };

    /** Is the function a core XPath 1.0 function of libxml2? */
    private static boolean xsltIsCoreFunction(String name, String ns_uri)
    {
        if (ns_uri != null) return ns_uri.equals("http://www.w3.org/2002/08/xquery-functions") && name.equals("escape-uri");
        for (String c : CORE_FUNCTIONS) if (c.equals(name)) return true;
        return false;
    }

    /** Port of xsltFunctionAvailableFunction: function-available(). */
    private static XPathResult xsltFunctionAvailableFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 1)
        {
            transformError(t, null, null, "function-available() : expects one string arg\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        String s = args[0].castToString();
        String[] prefix = { null };
        String ns_uri = null;
        String name = xsltSplitQname2(s, prefix);
        if (name == null)
        {
            name = s;
        }
        else
        {
            ns_uri = ctx.lookupNamespace(prefix[0]);
            if (ns_uri == null) transformError(t, null, null, fmt("function-available() : prefix %s is not bound\n", prefix[0]));
        }
        boolean r = xsltIsCoreFunction(name, ns_uri) || xsltFunctionLookup(t, name, ns_uri) != null;
        return XPathResult.newBoolean(r);
    }

    /** Port of xsltCurrentFunction: current(). */
    private static XPathResult xsltCurrentFunction(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 0)
        {
            transformError(t, null, null, "current() : function uses no argument\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        XPathResult r = XPathResult.newNodeSet();
        if (t.node != null) r.addNode(t.node);
        return r;
    }

    /** Port of xsltFunctionNodeSet (extra.c): node-set() in the libxslt, saxon and xt namespaces. */
    private static XPathResult xsltFunctionNodeSet(XPathEvalContext ctx, TransformCtx t, XPathResult[] args)
    {
        if (args.length != 1)
        {
            transformError(t, null, null, "node-set() : expects one result-tree arg\n");
            ctx.setErrorQuiet(XPathException.INVALID_ARITY);
            return null;
        }
        if (args[0].type() != XPathResult.NODESET)
        {
            transformError(t, null, null, "node-set() invalid arg expecting a result tree\n");
            ctx.setErrorQuiet(XPathException.INVALID_TYPE);
            return null;
        }
        XPathResult r = args[0].copy();
        r.setTree(false);
        return r;
    }

    /** An xslt function: the C function with its func_data (the transformation). */
    private interface XsltFunction
    {
        XPathResult call(XPathEvalContext ctx, TransformCtx t, XPathResult[] args);
    }

    /** The function lookup of the xpath context: the XSLT functions (xsltRegisterAllFunctions) and
        the extension functions libxslt registers by default (extra.c). */
    private static XsltFunction xsltFunctionLookup(TransformCtx t, String name, String ns_uri)
    {
        if (ns_uri == null)
        {
            if (name.equals("current")) return Transform::xsltCurrentFunction;
            if (name.equals("document")) return Transform::xsltDocumentFunction;
            if (name.equals("key")) return Transform::xsltKeyFunction;
            if (name.equals("unparsed-entity-uri")) return Transform::xsltUnparsedEntityUriFunction;
            if (name.equals("format-number")) return Transform::xsltFormatNumberFunction;
            if (name.equals("generate-id")) return Transform::xsltGenerateIdFunction;
            if (name.equals("system-property")) return Transform::xsltSystemPropertyFunction;
            if (name.equals("element-available")) return Transform::xsltElementAvailableFunction;
            if (name.equals("function-available")) return Transform::xsltFunctionAvailableFunction;
            return null;
        }
        if (name.equals("node-set") &&
            (ns_uri.equals(XSLT_LIBXSLT_NAMESPACE) || ns_uri.equals(XSLT_SAXON_NAMESPACE) || ns_uri.equals(XSLT_XT_NAMESPACE)))
        {
            return Transform::xsltFunctionNodeSet;
        }
        return null;
    }

    // The transformation context ////////////////////////////////////////

    /**
     * A transformation context for the source document (not owned). Compile the patterns
     * before creating it, like libxslt allocates their caches (xml_libxslt1_transform_new).
     * @param style The stylesheet.
     * @param source The source document.
     * @return The transformation.
     */
    public static TransformCtx transformNew(Stylesheet style, XmlDocument source)
    {
        TransformCtx t = new TransformCtx();
        t.style = style;
        t.source = source;
        t.xpath = new XPathEvalContext();
        // Like a fresh libxml2 xpath context, the position and size are unknown.
        t.xp_position = -1;
        t.xp_size = -1;
        int extras_max = style.extras_nr + 20;
        for (int i = 0; i < extras_max; i++) t.extras.add(new RuntimeExtra());
        t.extras_nr = style.extras_nr;
        t.max_template_depth = XSLT_MAX_DEPTH;
        t.max_template_vars = XSLT_MAX_VARS;
        t.op_limit = style.op_limit;
        final TransformCtx tc = t;
        t.xpath.setVariableLookup((name, ns_uri) -> xsltXpathVariableLookup(tc, name, ns_uri));
        t.xpath.setFunctionLookup((name, ns_uri) ->
        {
            XsltFunction f = xsltFunctionLookup(tc, name, ns_uri);
            if (f == null) return null;
            return (ctx, args) -> f.call(ctx, tc, args);
        });
        DocInfo docu = new DocInfo();
        docu.doc = source;
        docu.main = 1;
        t.doc_list = docu;
        t.document = docu;
        return t;
    }

    /**
     * Free a transformation (xml_libxslt1_transform_free): the result tree fragments and the
     * documents loaded by document() are released.
     * @param t The transformation.
     */
    public static void transformFree(TransformCtx t)
    {
        if (t == null) return;
        t.extras.clear();
        if (t.global_vars != null)
        {
            for (StackElem val : t.global_vars.values()) xsltFreeRuntimeStackElem(t, val);
            t.global_vars = null;
        }
        xsltFreeRvts(t);
        DocInfo d = t.doc_list;
        while (d != null)
        {
            DocInfo next = d.next;
            xsltFreeDocKeys(d);
            if (d.owned != 0)
            {
                t.xpath.forget(domDocument(d.doc));
            }
            d = next;
        }
        t.keyed_nodes = null;
        t.generated_ids = null;
    }

    /** Port of xsltFindElemSpaceHandling: are blank text children of node stripped? */
    private static int xsltFindElemSpaceHandling(TransformCtx t, XmlNode node)
    {
        if (node == null) return 0;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            String val = null;
            if (style.strip_spaces != null && nodeKind(node) == ELEMENT)
            {
                XmlNs ns = nodeNs(node);
                if (ns != null)
                {
                    String key = Xslt.key2(nodeName(node), nsHref(ns));
                    val = style.strip_spaces.get(key);
                    if (val == null)
                    {
                        key = Xslt.key2("*", nsHref(ns));
                        val = style.strip_spaces.get(key);
                    }
                }
                else
                {
                    String key = Xslt.key2(nodeName(node), null);
                    val = style.strip_spaces.get(key);
                }
            }
            if (val != null)
            {
                if (val.equals("strip")) return 1;
                if (val.equals("preserve")) return 0;
            }
            if (style.strip_all == 1) return 1;
            if (style.strip_all == -1) return 0;
        }
        return 0;
    }

    /** Port of xsltNeedElemSpaceHandling. */
    private static int xsltNeedElemSpaceHandling(TransformCtx t)
    {
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            if (style.strip_spaces != null) return 1;
        }
        return 0;
    }

    /** Port of xsltApplyStripSpaces: strip the blank text nodes of the source document. */
    private static void xsltApplyStripSpaces(TransformCtx t, XmlNode node)
    {
        XmlNode current = node;
        while (current != null)
        {
            if (lxIsRealNode(current) && lxHasChildren(current) && xsltFindElemSpaceHandling(t, current) != 0)
            {
                XmlNode del = null, cur = lxChildren(current);
                while (cur != null)
                {
                    if (lxIsBlankNode(cur)) del = cur;
                    cur = nodeNext(cur);
                    if (del != null)
                    {
                        nodeFree(del);
                        del = null;
                    }
                }
            }
            if (lxHasChildren(current) && nodeKind(current) != ENTITY_REF)
            {
                current = lxChildren(current);
            }
            else if (nodeNext(current) != null)
            {
                current = nodeNext(current);
            }
            else
            {
                do
                {
                    current = nodeParent(current);
                    if (current == null) break;
                    if (current == node) return;
                    if (nodeNext(current) != null)
                    {
                        current = nodeNext(current);
                        break;
                    }
                } while (current != null);
            }
        }
    }

    /** Port of xsltCountKeys. */
    private static int xsltCountKeys(TransformCtx t)
    {
        t.has_templ_key_patterns = 0;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            if (style.key_match != null)
            {
                t.has_templ_key_patterns = 1;
                break;
            }
        }
        t.nb_keys = 0;
        for (Stylesheet style = t.style; style != null; style = Xslt.nextImport(style))
        {
            for (KeyDef keyd = style.keys; keyd != null; keyd = keyd.next) t.nb_keys++;
        }
        return t.nb_keys;
    }

    private static String xsltImportMethod(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.method != null) return s.method; return null; }
    private static String xsltImportDoctypePublic(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.doctype_public != null) return s.doctype_public; return null; }
    private static String xsltImportDoctypeSystem(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.doctype_system != null) return s.doctype_system; return null; }
    private static String xsltImportVersion(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.version != null) return s.version; return null; }
    private static String xsltImportEncoding(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.encoding != null) return s.encoding; return null; }

    private static final String[][] HTML_VERSIONS = {
        { "5", null, "about:legacy-compat" },
        { "4.01frame", "-//W3C//DTD HTML 4.01 Frameset//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/frameset.dtd" },
        { "4.01strict", "-//W3C//DTD HTML 4.01//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/strict.dtd" },
        { "4.01trans", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd" },
        { "4.01", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd" },
        { "4.0strict", "-//W3C//DTD HTML 4.01//EN", "http://www.w3.org/TR/html4/strict.dtd" },
        { "4.0trans", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/html4/loose.dtd" },
        { "4.0frame", "-//W3C//DTD HTML 4.01 Frameset//EN", "http://www.w3.org/TR/html4/frameset.dtd" },
        { "4.0", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/html4/loose.dtd" },
        { "3.2", "-//W3C//DTD HTML 3.2//EN", null }
    };

    /** strcasecmp == 0 (ascii case folding). */
    private static boolean strcaseeq(String a, String b)
    {
        if (a.length() != b.length()) return false;
        for (int i = 0; i < a.length(); i++)
        {
            char x = a.charAt(i), y = b.charAt(i);
            if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
            if (x != y) return false;
        }
        return true;
    }

    /** Port of xsltGetHTMLIDs. */
    private static int xsltGetHtmlIds(String version, String[] public_id, String[] system_id)
    {
        if (version == null) return -1;
        for (String[] v : HTML_VERSIONS)
        {
            if (strcaseeq(version, v[0]))
            {
                if (public_id != null) public_id[0] = v[1];
                if (system_id != null) system_id[0] = v[2];
                return 0;
            }
        }
        return -1;
    }

    /** htmlNewDoc: an html document with a doctype (the html 4.0 transitional one if none is given). */
    private static XmlDocument xsltHtmlNewDoc(String system_id, String public_id)
    {
        XmlDocument res = domNew();
        if (system_id == null && public_id == null)
        {
            system_id = "http://www.w3.org/TR/REC-html40/loose.dtd";
            public_id = "-//W3C//DTD HTML 4.0 Transitional//EN";
        }
        domSetDtd(res, "html", public_id, system_id);
        return res;
    }

    private static boolean xsltIsBlankTextNode(XmlNode n)
    {
        return nodeKind(n) == TEXT && Xslt.isBlankStr(nodeContent(n));
    }

    /** Put the dtd first among the children of the document (libxslt's "hack" before comments). */
    private static void xsltSetDtdFirst(XmlDocument res, String name, String public_id, String system_id)
    {
        XmlNode dtd = domSetDtd(res, name, public_id, system_id);
        XmlNode doc = domDocument(res);
        XmlNode first = nodeFirstChild(doc);
        if (dtd != null && first != null && first != dtd)
        {
            nodeUnlink(dtd);
            nodeAddPrevSibling(first, dtd);
        }
    }

    /** Port of xsltProcessUserParamInternal (with eval): a parameter given to the transformation. */
    private static int xsltProcessUserParam(TransformCtx t, String qname, String value)
    {
        Stylesheet style = t.style;
        if (qname == null || value == null) return 0;
        String name = null, href = null;
        if (qname.startsWith("{"))
        {
            int len = 0;
            while (len < qname.length() && qname.charAt(len) != '}') len++;
            if (len >= qname.length())
            {
                transformError(t, style, null, fmt("user param : malformed parameter name : %s\n", qname));
                name = qname;
            }
            else
            {
                href = qname.substring(1, len);
                name = qname.substring(len + 1);
            }
        }
        else
        {
            String[] prefix = { null };
            name = Xslt.splitQname(qname, prefix);
            if (prefix[0] != null)
            {
                XmlNs ns = lxSearchNs(domRootElement(style.doc), prefix[0]);
                if (ns == null) transformError(t, style, null, fmt("user param : no namespace bound to prefix %s\n", prefix[0]));
                else href = nsHref(ns);
            }
        }
        if (name == null)
        {
            return -1;
        }
        String key = Xslt.key2(name, href);
        if (t.global_vars.get(key) != null) transformError(t, style, null, fmt("Global parameter %s already defined\n", name));
        // Like libxslt, only the variables of the top stylesheet are looked at (for every import).
        for (Stylesheet st = style; st != null; st = Xslt.nextImport(st))
        {
            for (StackElem elem = t.style.variables; elem != null; elem = elem.next)
            {
                if (elem.comp != null && elem.comp.type == StylePreComp.FUNC_VARIABLE && streq(elem.name, name) && streq(elem.name_uri, href))
                {
                    return 0;
                }
            }
        }
        XPathResult result = null;
        XPath xp_expr = Xslt.xpathCompile(style.settings, null, value, false);
        if (xp_expr != null)
        {
            long pos = t.xp_position, size = t.xp_size;
            t.xp_size = 1;
            t.xp_position = 1;
            result = xsltXpathEval(t, xp_expr, t.initial_context_node);
            t.xp_position = pos;
            t.xp_size = size;
        }
        if (result == null)
        {
            transformError(t, style, null, fmt("Evaluating user parameter %s failed\n", name));
            t.state = TransformCtx.STATE_STOPPED;
            return -1;
        }
        StackElem elem = xsltNewStackElem();
        elem.name = name;
        elem.select = value;
        elem.name_uri = href;
        elem.computed = 1;
        elem.value = result;
        if (t.global_vars.get(key) != null)
        {
            transformError(t, style, null, fmt("Global parameter %s already defined\n", name));
            xsltFreeRuntimeStackElem(t, elem);
        }
        else
        {
            t.global_vars.put(key, elem);
        }
        return 0;
    }

    /** Port of xsltEvalUserParams. */
    private static int xsltEvalUserParams(TransformCtx t, String[] params)
    {
        if (params == null) return 0;
        for (int i = 0; i < params.length && params[i] != null; i += 2)
        {
            if (xsltProcessUserParam(t, params[i], i + 1 < params.length ? params[i + 1] : null) != 0) return -1;
        }
        return 0;
    }

    // Output, port of libxslt xsltutils.c xsltSaveResultTo ///////////////

    private static int xsltImportIntIndent(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.indent != -1) return s.indent; return -1; }
    private static int xsltImportIntOmit(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.omit_xml_declaration != -1) return s.omit_xml_declaration; return -1; }
    private static int xsltImportIntStandalone(Stylesheet s) { for (; s != null; s = Xslt.nextImport(s)) if (s.standalone != -1) return s.standalone; return -1; }

    /** xml_html_save_doc_to: the html serialization of the document in UTF-8. The C serializer
        writes non ascii characters as they are (the output encoding is applied afterwards),
        HtmlSave does so when the document has an encoding. */
    private static void xsltHtmlSaveDocTo(StringBuilder buf, XmlDocument result, boolean format)
    {
        String old = domEncoding(result);
        if (old == null) domSetEncoding(result, "UTF-8");
        try
        {
            HtmlSave.nodeDumpFormatOutput(buf, result, domDocument(result), format);
        }
        finally
        {
            if (old == null) domSetEncoding(result, null);
        }
    }

    /** Port of xsltSaveResultTo: serialize the result in UTF-8 (the encoding is applied by the caller). */
    private static boolean xsltSaveResultTo(StringBuilder buf, XmlDocument result, Stylesheet style, int type)
    {
        if (buf == null || result == null || style == null) return false;
        XmlNode doc_node = domDocument(result);
        XmlNode first = nodeFirstChild(doc_node);
        if (first == null || (nodeKind(first) == DTD && nodeNext(first) == null)) return true;
        if (style.method_uri != null && (style.method == null || !style.method.equals("xhtml")))
        {
            genericError(style.settings, "xsltSaveResultTo : unknown output method\n");
            return false;
        }
        String method = xsltImportMethod(style);
        String encoding = xsltImportEncoding(style);
        int indent = xsltImportIntIndent(style);
        if (method == null && type == OUTPUT_HTML) method = "html";
        if (method != null && method.equals("html"))
        {
            HtmlSave.setMetaEncoding(result, encoding != null ? encoding : "UTF-8");
            if (indent == -1) indent = 1;
            xsltHtmlSaveDocTo(buf, result, indent != 0);
        }
        else if (method != null && method.equals("xhtml"))
        {
            HtmlSave.setMetaEncoding(result, encoding != null ? encoding : "UTF-8");
            xsltHtmlSaveDocTo(buf, result, true);
        }
        else if (method != null && method.equals("text"))
        {
            XmlNode cur = first;
            while (cur != null)
            {
                if (nodeKind(cur) == TEXT && nodeContent(cur) != null) buf.append(nodeContent(cur));
                XmlNode child = lxChildren(cur);
                if (child != null && nodeKind(child) != ENTITY_DECL && nodeKind(child) != ENTITY_REF)
                {
                    cur = child;
                    continue;
                }
                if (nodeNext(cur) != null)
                {
                    cur = nodeNext(cur);
                    continue;
                }
                do
                {
                    cur = nodeParent(cur);
                    if (cur == null || cur == doc_node)
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
        }
        else
        {
            int omit_xml_decl = xsltImportIntOmit(style);
            int standalone = xsltImportIntStandalone(style);
            if (omit_xml_decl != 1)
            {
                buf.append("<?xml version=");
                String version = domVersion(result);
                buf.append("\"");
                buf.append(version != null ? version : "1.0");
                buf.append("\"");
                if (encoding == null) encoding = domEncoding(result);
                if (encoding != null)
                {
                    buf.append(" encoding=\"");
                    buf.append(encoding);
                    buf.append("\"");
                }
                if (standalone == 0) buf.append(" standalone=\"no\"");
                else if (standalone == 1) buf.append(" standalone=\"yes\"");
                buf.append("?>\n");
            }
            for (XmlNode child = first; child != null; child = nodeNext(child))
            {
                XmlSave.nodeDumpOutput(buf, result, child, 0, indent == 1, encoding);
                if (indent != 0 && (nodeKind(child) == DTD || (nodeKind(child) == COMMENT && nodeNext(child) != null)))
                    buf.append("\n");
            }
            if (indent != 0) buf.append("\n");
        }
        return true;
    }

    /**
     * Serialize a result like libxslt xsltSaveResultToString: the output method, encoding,
     * indentation, xml declaration and doctype of the stylesheet (xml_libxslt1_save_result_to_string).
     * @param result The result document.
     * @param style The stylesheet.
     * @param output_type What applyStylesheet gave.
     * @return The bytes in the output encoding.
     */
    public static byte[] saveResultToString(XmlDocument result, Stylesheet style, int output_type)
    {
        StringBuilder utf8 = new StringBuilder();
        if (result != null && nodeFirstChild(domDocument(result)) != null) xsltSaveResultTo(utf8, result, style, output_type);
        String encoding = xsltImportEncoding(style);
        // Like libxslt, no encoder for UTF-8; an encoding without an encoder is written as UTF-8.
        if (encoding != null && !encoding.equalsIgnoreCase("UTF-8") && !encoding.equalsIgnoreCase("UTF8") && XmlSave.encodingSupported(encoding))
        {
            byte[] r = XmlSave.transcode(utf8.toString(), encoding);
            if (r != null) return r;
        }
        return utf8.toString().getBytes(StandardCharsets.UTF_8);
    }

    /**
     * Apply the stylesheet to the document (libxslt xsltApplyStylesheet, port of
     * xsltApplyStylesheetInternal). The document is not owned but whitespace may be stripped
     * from it (xsl:strip-space).
     * @param style The stylesheet.
     * @param doc The source document.
     * @param params Name, value pairs (the values are xpath expressions) or null.
     * @param output_type If not null, output_type[0] gets OUTPUT_XML, OUTPUT_HTML or OUTPUT_TEXT.
     * @return The result, null after reporting errors.
     */
    public static XmlDocument applyStylesheet(Stylesheet style, XmlDocument doc, String[] params, int[] output_type)
    {
        if (style == null || doc == null) return null;
        XmlDocument res = null;
        TransformCtx t = transformNew(style, doc);
        t.params = params;
        t.initial_context_doc = doc;
        t.initial_context_node = domDocument(doc);
        String method = xsltImportMethod(style);
        String[] doctype_public = { xsltImportDoctypePublic(style) };
        String[] doctype_system = { xsltImportDoctypeSystem(style) };
        String version = xsltImportVersion(style);
        String encoding = xsltImportEncoding(style);
        if (method != null && !method.equals("xml"))
        {
            if (method.equals("html"))
            {
                t.type = OUTPUT_HTML;
                if (doctype_public[0] != null || doctype_system[0] != null)
                {
                    res = xsltHtmlNewDoc(doctype_system[0], doctype_public[0]);
                }
                else if (version == null)
                {
                    // htmlNewDoc then the dtd is removed.
                    res = domNew();
                }
                else
                {
                    xsltGetHtmlIds(version, doctype_public, doctype_system);
                    res = xsltHtmlNewDoc(doctype_system[0], doctype_public[0]);
                }
            }
            else if (method.equals("xhtml"))
            {
                transformError(t, null, domDocument(doc), "xsltApplyStylesheetInternal: unsupported method xhtml, using html\n");
                t.type = OUTPUT_HTML;
                res = xsltHtmlNewDoc(doctype_system[0], doctype_public[0]);
            }
            else if (method.equals("text"))
            {
                t.type = OUTPUT_TEXT;
                res = domNew();
                domSetVersion(res, style.version != null ? style.version : "1.0");
            }
            else
            {
                transformError(t, null, domDocument(doc), fmt("xsltApplyStylesheetInternal: unsupported method (%s)\n", method));
                transformFree(t);
                return null;
            }
        }
        else
        {
            t.type = OUTPUT_XML;
            res = domNew();
            domSetVersion(res, style.version != null ? style.version : "1.0");
        }
        if (encoding != null) domSetEncoding(res, encoding);
        t.node = domDocument(doc);
        t.output = res;
        t.xp_size = 1;
        t.xp_position = 1;
        if (xsltNeedElemSpaceHandling(t) != 0) xsltApplyStripSpaces(t, domRootElement(doc));
        t.global_vars = new HashMap<>();
        if (params != null) xsltEvalUserParams(t, params);
        xsltCountKeys(t);
        xsltEvalGlobalVariables(t);
        // Clean up any unused fragments.
        if (t.local_rvt != null) xsltReleaseLocalRvts(t, null);
        t.insert = domDocument(res);
        t.vars_base = t.vars_nr - 1;
        xsltProcessOneNode(t, t.node, null);
        xsltLocalVariablePop(t, 0, -2);

        XmlNode root = domRootElement(res);
        if (root != null)
        {
            String doctype = null;
            XmlNs rns = nodeNs(root);
            if (rns != null && nsPrefix(rns) != null)
            {
                doctype = nsPrefix(rns) + ":" + nodeName(root);
            }
            else
            {
                doctype = nodeName(root);
            }
            // Apply the default html output method when the root element is html.
            if (method == null && rns == null && nodeName(root).equalsIgnoreCase("html"))
            {
                XmlNode tmp = nodeFirstChild(domDocument(res));
                while (tmp != null && tmp != root)
                {
                    if (nodeKind(tmp) == ELEMENT) break;
                    if (nodeKind(tmp) == TEXT && !xsltIsBlankTextNode(tmp)) break;
                    tmp = nodeNext(tmp);
                }
                if (tmp == root)
                {
                    t.type = OUTPUT_HTML;
                    if (doctype_public[0] != null || doctype_system[0] != null)
                    {
                        domSetDtd(res, doctype, doctype_public[0], doctype_system[0]);
                    }
                    else if (version != null)
                    {
                        xsltGetHtmlIds(version, doctype_public, doctype_system);
                        if (doctype_public[0] != null || doctype_system[0] != null) domSetDtd(res, doctype, doctype_public[0], doctype_system[0]);
                    }
                }
            }
            if (t.type == OUTPUT_XML)
            {
                doctype_public[0] = xsltImportDoctypePublic(style);
                doctype_system[0] = xsltImportDoctypeSystem(style);
                if (doctype_public[0] != null || doctype_system[0] != null) xsltSetDtdFirst(res, doctype, doctype_public[0], doctype_system[0]);
            }
        }
        if (t.state != TransformCtx.STATE_OK)
        {
            res = null;
        }
        if (output_type != null) output_type[0] = t.type;
        transformFree(t);
        return res;
    }
}
