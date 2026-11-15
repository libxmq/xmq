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

package org.libxmq.imp;

import static org.libxmq.imp.xml.XmlDomApi.*;

import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlParseException;
import org.libxmq.imp.xml.XmlParser;

/**
 * Support for xslt in the command line, like the C version: the character entities of
 * stylesheets written in xmq are replaced with their characters (substitute_entity in
 * xmq-cli.c), and the transform markers are replaced after a transform
 * (xmq_replace_transform_markers in xmq.c).
 */
final class XsltSupport
{
    private XsltSupport()
    {
    }

    /** Like decode_entity_ref in parts/xml.c. */
    static int decodeEntityRef(String name)
    {
        switch (name)
        {
        case "amp": return '&';
        case "apos": return '\'';
        case "gt": return '>';
        case "lt": return '<';
        case "quot": return '"';
        case "nbsp": return 160;
        default: break;
        }
        if (!name.startsWith("#")) return 0;
        if (name.length() > 1 && name.charAt(1) == 'x')
        {
            return (int)strtol(name.substring(2), 16);
        }
        return atoi(name.substring(1));
    }

    /** C atoi: leading blanks, sign, digits, 0 if none (overflow is undefined in C, wraps here). */
    static int atoi(String s)
    {
        int i = 0, n = s.length();
        while (i < n && (s.charAt(i) == ' ' || (s.charAt(i) >= 9 && s.charAt(i) <= 13))) i++;
        boolean neg = false;
        if (i < n && (s.charAt(i) == '+' || s.charAt(i) == '-'))
        {
            neg = s.charAt(i) == '-';
            i++;
        }
        long v = 0;
        while (i < n && s.charAt(i) >= '0' && s.charAt(i) <= '9')
        {
            v = v * 10 + (s.charAt(i++) - '0');
            if (v > Integer.MAX_VALUE + 1L) v = Integer.MAX_VALUE + 1L;
        }
        return (int)(neg ? -v : v);
    }

    /** The utf8 bytes of encode_utf8 as a string (a 0 code point is an empty string in C). */
    static String encodeUtf8(int uc)
    {
        if (uc <= 0 || uc > 0x10FFFF || (uc >= 0xD800 && uc <= 0xDFFF)) return "";
        return new String(Character.toChars(uc));
    }

    /**
     * Replace the character entity references (names starting with #) below node with text
     * nodes, without merging them into adjacent text (substitute_entity(doc, node, NULL, true)).
     */
    static void substituteCharEntities(XmlDocument doc, XmlNode node)
    {
        if (node == null) return;
        if (nodeKind(node) == ENTITY_REF)
        {
            if (nodeName(node).startsWith("#"))
            {
                XmlNode n = nodeNewText(doc, encodeUtf8(decodeEntityRef(nodeName(node))));
                nodeReplace(node, n);
                nodeFree(node);
            }
            return;
        }
        XmlNode i = nodeFirstChild(node);
        while (i != null)
        {
            XmlNode next = nodeNext(i);
            substituteCharEntities(doc, i);
            i = next;
        }
    }

    // Transform markers, see the comment in xmq.c.

    static final String MARKERS_NS = "https://libxmq.org/xmq/markers";

    private static boolean isMarker(XmlNode n, String name)
    {
        return nodeKind(n) == ELEMENT && nodeNs(n) != null && nodeNs(n).href() != null
            && nodeNs(n).href().equals(MARKERS_NS) && (name == null || name.equals(nodeName(n)));
    }

    private static String markerProp(XmlNode n, String name)
    {
        var a = nodeGetAttr(n, name, null);
        if (a == null) return "";
        return nodeStringValue(a);
    }

    private static String markerContent(XmlNode n)
    {
        return nodeStringValue(n);
    }

    /** strtol(s, NULL, base) for the leading digits. */
    static long strtol(String s, int base)
    {
        int i = 0, n = s.length();
        while (i < n && (s.charAt(i) == ' ' || (s.charAt(i) >= 9 && s.charAt(i) <= 13))) i++;
        boolean neg = false;
        if (i < n && (s.charAt(i) == '+' || s.charAt(i) == '-'))
        {
            neg = s.charAt(i) == '-';
            i++;
        }
        if (base == 16 && i + 1 < n && s.charAt(i) == '0' && (s.charAt(i + 1) == 'x' || s.charAt(i + 1) == 'X')) i += 2;
        long v = 0;
        while (i < n)
        {
            int d = Character.digit(s.charAt(i), base);
            if (d < 0 || s.charAt(i) > 0x7f) break;
            if (v < Long.MAX_VALUE / 32) v = v * base + d;
            i++;
        }
        return neg ? -v : v;
    }

    /** An entity marker becomes a text node (character entity) or an entity reference. */
    private static XmlNode markerEntityNode(XmlDocument doc, XmlNode marker)
    {
        String name = markerProp(marker, "name");
        XmlNode n = null;
        if (name.startsWith("#"))
        {
            long uc = (name.length() > 1 && name.charAt(1) == 'x') ? strtol(name.substring(2), 16) : strtol(name.substring(1), 10);
            if (uc > 0 && uc <= 0x10ffff)
            {
                n = nodeNewText(doc, encodeUtf8Marker((int)uc));
            }
        }
        if (n == null) n = nodeNewEntityRef(doc, name);
        return n;
    }

    /** encode_utf8 encodes surrogate code points too (as 3 bytes), java strings hold them as is. */
    private static String encodeUtf8Marker(int uc)
    {
        if (uc >= 0xD800 && uc <= 0xDFFF) return String.valueOf((char)uc);
        return new String(Character.toChars(uc));
    }

    private static void markerReplace(XmlNode marker, XmlNode n)
    {
        // Insert without merging text, adjacent text is merged by cleanupAfterMarkers.
        if (n != null) nodeAddPrevSibling(marker, n);
        nodeFree(marker);
    }

    private static XmlNs markerFindNsDef(XmlNode e, String prefix)
    {
        for (XmlNs ns : nodeNsDecls(e))
        {
            if (prefix == null && ns.prefix() == null) return ns;
            if (prefix != null && prefix.equals(ns.prefix())) return ns;
        }
        return null;
    }

    private static void markerXmlns(Set<XmlNs> explicit, XmlNode parent, XmlNode marker)
    {
        String prefix = markerProp(marker, "prefix");
        String href = markerContent(marker);
        String p = prefix.isEmpty() ? null : prefix;
        if (nodeKind(parent) == ELEMENT)
        {
            XmlNs ns = markerFindNsDef(parent, p);
            if (ns == null) ns = nsDeclare(parent, p, href);
            if (ns != null)
            {
                explicit.add(ns);
                // Move the declaration last, the markers come in source order.
                nsMoveDeclLast(parent, ns);
            }
        }
    }

    private static void markerAttribute(Set<XmlNs> explicit, XmlDocument doc, XmlNode parent, XmlNode marker)
    {
        if (nodeKind(parent) != ELEMENT) return;
        String name = markerProp(marker, "name");
        String uri = markerProp(marker, "ns");
        org.libxmq.imp.xml.XmlAttr a;
        int colon = name.indexOf(':');
        if (colon >= 0)
        {
            String pfx = name.substring(0, colon);
            XmlNs ns;
            if (!uri.isEmpty())
            {
                ns = nsLookupPrefix(parent, pfx);
                if (ns == null || ns.href() == null || !ns.href().equals(uri)) ns = nsDeclare(parent, pfx, uri);
            }
            else
            {
                ns = markerFindNsDef(parent, pfx);
                if (ns == null) ns = nsDeclare(parent, pfx, null);
            }
            if (ns != null) explicit.add(ns);
            a = nodeAppendAttr(parent, name.substring(colon + 1), ns, null);
        }
        else
        {
            a = nodeAppendAttr(parent, name, null, null);
        }
        for (XmlNode c = nodeFirstChild(marker); c != null; c = nodeNext(c))
        {
            XmlNode n = null;
            if (nodeKind(c) == TEXT) n = nodeNewText(doc, nodeContent(c));
            else if (isMarker(c, "entity")) n = markerEntityNode(doc, c);
            if (n != null) nodeAddChild(a, n);
        }
    }

    private static void markerDoctype(XmlDocument doc, String source, XmlNode marker)
    {
        String buf = "<!DOCTYPE " + markerContent(marker) + "><foo></foo>";
        XmlNode dtd = null;
        try
        {
            XmlDocument d = XmlParser.parse(buf, source, 0);
            XmlNode dd = domDtd(d);
            if (dd != null) dtd = nodeCopy(doc, dd, true);
        }
        catch (XmlParseException e)
        {
            System.err.print(e.getMessage());
        }
        if (dtd != null && domDtd(doc) == null)
        {
            nodeAddPrevSibling(marker, dtd);
            doc.setIntSubsetRef((org.libxmq.imp.xml.XmlDtd)dtd);
        }
    }

    private static XmlNode markerXml(XmlDocument doc, XmlNode marker)
    {
        String value = markerContent(marker);
        try
        {
            XmlDocument fdoc = XmlParser.parse(value, "noname.xml", XmlParser.NOCDATA | XmlParser.NOBLANKS);
            XmlNode root = domRootElement(fdoc);
            return root != null ? nodeCopy(doc, root, true) : null;
        }
        catch (XmlParseException e)
        {
            System.err.print(e.getMessage());
            return null;
        }
    }

    private static void replaceMarkers(Set<XmlNs> explicit, XmlDocument doc, String source, XmlNode parent)
    {
        XmlNode next;
        for (XmlNode i = nodeFirstChild(parent); i != null; i = next)
        {
            next = nodeNext(i);
            if (!isMarker(i, null))
            {
                if (nodeKind(i) == ELEMENT) replaceMarkers(explicit, doc, source, i);
                continue;
            }
            String m = nodeName(i);
            switch (m)
            {
            case "entity" -> markerReplace(i, markerEntityNode(doc, i));
            case "empty" -> markerReplace(i, nodeNewText(doc, ""));
            case "comment" -> markerReplace(i, nodeNewComment(doc, markerContent(i)));
            case "xml" -> markerReplace(i, markerXml(doc, i));
            case "doctype" ->
            {
                markerDoctype(doc, source, i);
                markerReplace(i, null);
            }
            case "xmlns" ->
            {
                markerXmlns(explicit, parent, i);
                markerReplace(i, null);
            }
            case "attribute" ->
            {
                markerAttribute(explicit, doc, parent, i);
                markerReplace(i, null);
            }
            case "element" ->
            {
                // An element with an undeclared prefix, declare the prefix
                // without uri on it like the xmq parser does.
                String name = markerProp(i, "name");
                nodeRemoveAttr(i, "name", null);
                XmlNs ns = null;
                int colon = name.indexOf(':');
                if (colon >= 0)
                {
                    ns = nsDeclare(i, name.substring(0, colon), null);
                    if (ns != null) explicit.add(ns);
                }
                nodeSetName(i, colon >= 0 ? name.substring(colon + 1) : name);
                nodeSetNs(i, ns);
                replaceMarkers(explicit, doc, source, i);
            }
            default -> replaceMarkers(explicit, doc, source, i);
            }
        }
    }

    /** Remove the marker namespace declarations and the declarations that repeat an identical
        declaration of an ancestor. Merge adjacent text nodes created when markers were replaced. */
    private static void cleanupAfterMarkers(Set<XmlNs> explicit, XmlNode node)
    {
        if (nodeKind(node) == ELEMENT)
        {
            for (XmlNs ns : new java.util.ArrayList<>(nodeNsDecls(node)))
            {
                boolean remove = false;
                XmlNs replacement = null;
                if (ns.href() != null && ns.href().equals(MARKERS_NS))
                {
                    remove = true;
                }
                else if (!explicit.contains(ns) && nodeParent(node) != null && nodeKind(nodeParent(node)) == ELEMENT)
                {
                    XmlNs anc = nsLookupPrefix(nodeParent(node), ns.prefix());
                    if (anc != null && anc.href() != null && ns.href() != null && anc.href().equals(ns.href()))
                    {
                        remove = true;
                        replacement = anc;
                    }
                }
                if (remove) nsRemoveDecl(node, ns, replacement);
            }
        }
        for (XmlNode c = nodeFirstChild(node); c != null; c = nodeNext(c))
        {
            while (nodeKind(c) == TEXT && nodeNext(c) != null && nodeKind(nodeNext(c)) == TEXT)
            {
                XmlNode n = nodeNext(c);
                nodeAppendContent(c, nodeContent(n));
                nodeFree(n);
            }
            if (nodeKind(c) == ELEMENT) cleanupAfterMarkers(explicit, c);
        }
    }

    private static boolean hasMarkers(XmlNode node)
    {
        for (XmlNode c = nodeFirstChild(node); c != null; c = nodeNext(c))
        {
            if (isMarker(c, null)) return true;
            if (nodeKind(c) == ELEMENT && hasMarkers(c)) return true;
        }
        return false;
    }

    /** Replace the transform markers in doc (xmq_replace_transform_markers). */
    static void replaceTransformMarkers(XmlDocument doc, String source)
    {
        if (doc == null || !hasMarkers(doc)) return;
        Set<XmlNs> explicit = Collections.newSetFromMap(new IdentityHashMap<>());
        replaceMarkers(explicit, doc, source, doc);
        cleanupAfterMarkers(explicit, doc);
    }
}
