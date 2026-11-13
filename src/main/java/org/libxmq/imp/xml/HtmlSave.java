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

// Ported from libxml2 2.9.14 HTMLtree.c, entities.c, uri.c and buf.c,
// Copyright (C) 1998-2012 Daniel Veillard, MIT license.

package org.libxmq.imp.xml;

import java.nio.charset.StandardCharsets;

/**
 * Serialize an html document or node exactly like the libxml2 (2.9.14)
 * html serializer htmlNodeDumpFormatOutput does.
 *
 * The code is ported from libxml2 HTMLtree.c, entities.c (xmlEncodeEntitiesInternal),
 * uri.c (xmlURIEscapeStr) and buf.c (xmlBufWriteQuotedString)
 * (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 */
public final class HtmlSave
{
    private HtmlSave()
    {
    }

    /**
     * Dump a whole html document, like libxml2 htmlDocContentDumpFormatOutput.
     * The C version of xmq calls htmlDocContentDumpOutput(out, doc, "utf8") which
     * is the same as using format=true.
     * @param doc The document.
     * @param format If true, then add newlines around block elements.
     * @return The serialized document.
     */
    public static String docContentDump(XmlDocument doc, boolean format)
    {
        StringBuilder out = new StringBuilder();
        nodeDumpFormatOutput(out, doc, doc, format);
        return out.toString();
    }

    /**
     * Dump a node and its children, like libxml2 htmlNodeDumpFormatOutput.
     * Note that libxml2 htmlNodeDump always uses format=1.
     * @param doc The document, used to decide if the escaping should be html or xml style.
     * @param node The node to dump.
     * @param format If true, then add newlines around block elements.
     * @return The serialized node.
     */
    public static String nodeDump(XmlDocument doc, XmlNode node, boolean format)
    {
        StringBuilder out = new StringBuilder();
        nodeDumpFormatOutput(out, doc, node, format);
        return out.toString();
    }

    /**
     * Dump a node and its children into a string builder, like libxml2 htmlNodeDumpFormatOutput.
     * @param out The output.
     * @param doc The document, used to decide if the escaping should be html or xml style.
     * @param cur The node to dump.
     * @param format If true, then add newlines around block elements.
     */
    public static void nodeDumpFormatOutput(StringBuilder out, XmlDocument doc, XmlNode cur, boolean format)
    {
        if (cur == null) return;
        XmlNode root = cur;
        XmlNode parent = cur.parent_;
        HtmlElementTable.Desc info;
        for (;;)
        {
            switch (cur.type())
            {
            case HTML_DOCUMENT, DOCUMENT ->
            {
                XmlDocument d = (XmlDocument)cur;
                if (d.intSubset() != null)
                {
                    dtdDumpOutput(out, d.intSubset());
                }
                if (cur.children_ != null)
                {
                    if (cur.parent_ == parent)
                    {
                        parent = cur;
                        cur = cur.children_;
                        continue;
                    }
                }
                else
                {
                    out.append('\n');
                }
            }
            case ELEMENT ->
            {
                XmlElement e = (XmlElement)cur;
                if (cur.parent_ != parent && cur.children_ != null)
                {
                    nodeDumpFormatOutput(out, doc, cur, format);
                    break;
                }
                if (e.ns() == null) info = HtmlElementTable.lookup(cur.name());
                else info = null;
                out.append('<');
                appendQName(out, e.ns(), cur.name());
                for (XmlNs ns : e.nsDefs()) nsDumpOutput(out, ns);
                for (XmlAttr a : e.attributes()) attrDumpOutput(out, doc, a);
                if (info != null && info.empty != 0)
                {
                    out.append('>');
                }
                else if (cur.children_ == null)
                {
                    if (info != null && info.saveEndTag != 0 && !info.name.equals("html") && !info.name.equals("body"))
                    {
                        out.append('>');
                    }
                    else
                    {
                        out.append("></");
                        appendQName(out, e.ns(), cur.name());
                        out.append('>');
                    }
                }
                else
                {
                    out.append('>');
                    if (format && info != null && info.isinline == 0 &&
                        cur.children_.type() != XmlNodeType.TEXT &&
                        cur.children_.type() != XmlNodeType.ENTITY_REF &&
                        cur.children_ != cur.last_ &&
                        cur.name() != null && !cur.name().startsWith("p"))
                    {
                        out.append('\n');
                    }
                    parent = cur;
                    cur = cur.children_;
                    continue;
                }
                if (format && cur.next_ != null && info != null && info.isinline == 0)
                {
                    if (cur.next_.type() != XmlNodeType.TEXT &&
                        cur.next_.type() != XmlNodeType.ENTITY_REF &&
                        parent != null && parent.name() != null && !parent.name().startsWith("p"))
                    {
                        out.append('\n');
                    }
                }
            }
            case ATTRIBUTE -> attrDumpOutput(out, doc, (XmlAttr)cur);
            case TEXT ->
            {
                if (cur.content() == null) break;
                if (parent == null ||
                    (!"script".equalsIgnoreCase(parent.name()) && !"style".equalsIgnoreCase(parent.name())))
                {
                    out.append(encodeEntities(doc, cur.content(), false));
                }
                else
                {
                    out.append(cur.content());
                }
            }
            case COMMENT ->
            {
                if (cur.content() != null)
                {
                    out.append("<!--").append(cur.content()).append("-->");
                }
            }
            case PI ->
            {
                if (cur.name() != null)
                {
                    out.append("<?").append(cur.name());
                    if (cur.content() != null)
                    {
                        out.append(' ').append(cur.content());
                    }
                    out.append('>');
                }
            }
            case ENTITY_REF -> out.append('&').append(cur.name()).append(';');
            case CDATA ->
            {
                if (cur.content() != null) out.append(cur.content());
            }
            default ->
            {
            }
            }

            for (;;)
            {
                if (cur == root) return;
                if (cur.next_ != null)
                {
                    cur = cur.next_;
                    break;
                }
                cur = parent;
                parent = cur.parent_;
                if (cur.type() == XmlNodeType.HTML_DOCUMENT || cur.type() == XmlNodeType.DOCUMENT)
                {
                    out.append('\n');
                }
                else
                {
                    XmlNs ns = (cur instanceof XmlElement ce) ? ce.ns() : null;
                    if (format && ns == null) info = HtmlElementTable.lookup(cur.name());
                    else info = null;
                    if (format && info != null && info.isinline == 0 &&
                        cur.last_.type() != XmlNodeType.TEXT &&
                        cur.last_.type() != XmlNodeType.ENTITY_REF &&
                        cur.children_ != cur.last_ &&
                        cur.name() != null && !cur.name().startsWith("p"))
                    {
                        out.append('\n');
                    }
                    out.append("</");
                    appendQName(out, ns, cur.name());
                    out.append('>');
                    if (format && info != null && info.isinline == 0 && cur.next_ != null)
                    {
                        if (cur.next_.type() != XmlNodeType.TEXT &&
                            cur.next_.type() != XmlNodeType.ENTITY_REF &&
                            parent != null && parent.name() != null && !parent.name().startsWith("p"))
                        {
                            out.append('\n');
                        }
                    }
                }
            }
        }
    }

    private static void appendQName(StringBuilder out, XmlNs ns, String name)
    {
        if (ns != null && ns.prefix() != null)
        {
            out.append(ns.prefix()).append(':');
        }
        if (name != null) out.append(name);
    }

    /** htmlDtdDumpOutput */
    private static void dtdDumpOutput(StringBuilder out, XmlDtd dtd)
    {
        out.append("<!DOCTYPE ");
        if (dtd.name() != null) out.append(dtd.name());
        if (dtd.externalId() != null)
        {
            out.append(" PUBLIC ");
            writeQuotedString(out, dtd.externalId());
            if (dtd.systemId() != null)
            {
                out.append(' ');
                writeQuotedString(out, dtd.systemId());
            }
        }
        else if (dtd.systemId() != null && !dtd.systemId().equals("about:legacy-compat"))
        {
            out.append(" SYSTEM ");
            writeQuotedString(out, dtd.systemId());
        }
        out.append(">\n");
    }

    /** xmlNsDumpOutput */
    private static void nsDumpOutput(StringBuilder out, XmlNs ns)
    {
        if (ns.href() == null) return;
        if ("xml".equals(ns.prefix())) return;
        out.append(' ');
        if (ns.prefix() != null)
        {
            out.append("xmlns:").append(ns.prefix());
        }
        else
        {
            out.append("xmlns");
        }
        out.append('=');
        writeQuotedString(out, ns.href());
    }

    /** htmlAttrDumpOutput */
    private static void attrDumpOutput(StringBuilder out, XmlDocument doc, XmlAttr a)
    {
        out.append(' ');
        appendQName(out, a.ns(), a.name());
        if (a.children_ != null && !HtmlParser.isBooleanAttr(a.name()))
        {
            String value = nodeListGetString(doc, a);
            if (value != null)
            {
                out.append('=');
                XmlNode p = a.parent_;
                if (a.ns() == null && p instanceof XmlElement pe && pe.ns() == null &&
                    (a.name().equalsIgnoreCase("href") ||
                     a.name().equalsIgnoreCase("action") ||
                     a.name().equalsIgnoreCase("src") ||
                     (a.name().equalsIgnoreCase("name") && "a".equalsIgnoreCase(pe.name()))))
                {
                    int i = 0;
                    while (i < value.length() && isBlank(value.charAt(i))) i++;
                    writeQuotedString(out, uriEscapeStr(value.substring(i), "@/:=?;#%&,+<>"));
                }
                else
                {
                    writeQuotedString(out, value);
                }
            }
            else
            {
                out.append("=\"\"");
            }
        }
    }

    private static boolean isBlank(int c)
    {
        return c == 0x20 || c == 0x9 || c == 0xa || c == 0xd;
    }

    /** xmlNodeListGetString(doc, attr->children, 0) */
    private static String nodeListGetString(XmlDocument doc, XmlAttr a)
    {
        StringBuilder sb = null;
        for (XmlNode n = a.children_; n != null; n = n.next_)
        {
            if (n.type() == XmlNodeType.TEXT || n.type() == XmlNodeType.CDATA)
            {
                if (sb == null) sb = new StringBuilder();
                if (n.content() != null) sb.append(encodeEntities(doc, n.content(), true));
            }
            else if (n.type() == XmlNodeType.ENTITY_REF)
            {
                if (sb == null) sb = new StringBuilder();
                sb.append('&').append(n.name()).append(';');
            }
        }
        return sb == null ? null : sb.toString();
    }

    /** xmlBufWriteQuotedString */
    private static void writeQuotedString(StringBuilder out, String s)
    {
        if (s.indexOf('"') >= 0)
        {
            if (s.indexOf('\'') >= 0)
            {
                out.append('"');
                out.append(s.replace("\"", "&quot;"));
                out.append('"');
            }
            else
            {
                out.append('\'').append(s).append('\'');
            }
        }
        else
        {
            out.append('"').append(s).append('"');
        }
    }

    /** xmlURIEscapeStr, escapes each UTF-8 byte that is not allowed. */
    private static String uriEscapeStr(String s, String list)
    {
        if (s.isEmpty()) return s;
        byte[] b = s.getBytes(StandardCharsets.UTF_8);
        ByteArrayOut bo = new ByteArrayOut();
        for (byte x : b)
        {
            int ch = x & 0xff;
            if (ch != '@' && !isUnreserved(ch) && (ch >= 0x80 || list.indexOf(ch) < 0))
            {
                bo.add('%');
                bo.add(hex(ch >> 4));
                bo.add(hex(ch & 0xf));
            }
            else
            {
                bo.add(ch);
            }
        }
        return bo.str();
    }

    private static final class ByteArrayOut
    {
        byte[] b = new byte[64];
        int n;

        void add(int c)
        {
            if (n == b.length) b = java.util.Arrays.copyOf(b, n * 2);
            b[n++] = (byte)c;
        }

        String str()
        {
            return new String(b, 0, n, StandardCharsets.UTF_8);
        }
    }

    private static int hex(int v)
    {
        return v <= 9 ? '0' + v : 'A' + v - 0xA;
    }

    private static boolean isUnreserved(int x)
    {
        return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || (x >= '0' && x <= '9') ||
            x == '-' || x == '_' || x == '.' || x == '!' || x == '~' || x == '*' || x == '\'' ||
            x == '(' || x == ')';
    }

    /**
     * xmlEncodeEntitiesInternal, escape text for html or xml output.
     * @param doc The document, if it is an html document then html rules are used.
     * @param input The text.
     * @param attr True if escaping an attribute value.
     * @return The escaped text.
     */
    static String encodeEntities(XmlDocument doc, String input, boolean attr)
    {
        boolean html = doc != null && doc.isHtml();
        boolean hasEncoding = doc != null && doc.encoding() != null;
        StringBuilder out = new StringBuilder(input.length() + 16);
        int len = input.length();
        int i = 0;
        while (i < len)
        {
            char c = input.charAt(i);
            if (c == '<')
            {
                int end;
                if (html && attr && input.startsWith("<!--", i) && (end = input.indexOf("-->", i)) >= 0)
                {
                    out.append(input, i, end + 3);
                    i = end + 3;
                    continue;
                }
                out.append("&lt;");
            }
            else if (c == '>')
            {
                out.append("&gt;");
            }
            else if (c == '&')
            {
                int end;
                if (html && attr && i + 1 < len && input.charAt(i + 1) == '{' && (end = input.indexOf('}', i)) >= 0)
                {
                    out.append(input, i, end + 1);
                    i = end + 1;
                    continue;
                }
                out.append("&amp;");
            }
            else if ((c >= 0x20 && c < 0x80) || c == '\n' || c == '\t' || (html && c == '\r'))
            {
                out.append(c);
            }
            else if (c >= 0x80)
            {
                int cp = input.codePointAt(i);
                int n = Character.charCount(cp);
                if (hasEncoding || html)
                {
                    out.append(input, i, i + n);
                }
                else if (!HtmlCharClasses.isChar(cp))
                {
                    // Invalid char, libxml2 outputs the first byte as a char ref.
                    byte[] bs = new String(Character.toChars(cp)).getBytes(StandardCharsets.UTF_8);
                    out.append("&#").append(bs[0] & 0xff).append(';');
                }
                else
                {
                    out.append("&#x").append(Integer.toHexString(cp).toUpperCase()).append(';');
                }
                i += n;
                continue;
            }
            else if (c == '\r')
            {
                out.append("&#13;");
            }
            // Other control characters are dropped.
            i++;
        }
        return out.toString();
    }
}
