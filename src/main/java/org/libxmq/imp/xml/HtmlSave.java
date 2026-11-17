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

/* Portions of this file are ported from libxml2 (HTMLtree.c, HTMLparser.c, xmlsave.c and buf.c,
   version 2.16.0 as found in the libxmq fork in 3rdparty/libxml2), like src/main/c/parts/xml_html_save.c.
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


// Ported from libxml2 2.16.0 HTMLtree.c, HTMLparser.c, xmlsave.c and buf.c,
// Copyright (C) 1998-2012 Daniel Veillard, MIT license.

package org.libxmq.imp.xml;

import java.nio.charset.StandardCharsets;

/**
 * Serialize an html document or node exactly like the libxml2 (2.16)
 * html serializer htmlNodeDumpInternal does, the same as the C version
 * (src/main/c/parts/xml_html_save.c): text is escaped with &amp;amp; &amp;lt; &amp;gt;
 * only (non ascii is written as it is), attributes are always quoted with ".
 *
 * The code is ported from libxml2 HTMLtree.c, HTMLparser.c (the element table),
 * xmlsave.c (xmlSerializeText) and buf.c (xmlBufWriteQuotedString)
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
     * Dump a node and its children into a string builder, port of htmlNodeDumpInternal
     * (libxml2 2.16, encoding NULL so no meta charset handling), like xml_html_node in C.
     * @param out The output.
     * @param doc The document (not used, kept for the callers).
     * @param cur The node to dump.
     * @param format If true, then add newlines around block elements.
     */
    public static void nodeDumpFormatOutput(StringBuilder out, XmlDocument doc, XmlNode cur, boolean format)
    {
        if (cur == null) return;
        XmlNode root = cur;
        boolean is_raw = false;

        while (true)
        {
            switch (cur.type())
            {
            case HTML_DOCUMENT, DOCUMENT ->
            {
                XmlDocument d = (XmlDocument)cur;
                // The dtd (like xml_dom_dtd in C): the internal subset, or a dtd child, eg from xsl:output doctype-system.
                XmlDtd dtd = d.intSubset();
                for (XmlNode c = cur.children_; dtd == null && c != null; c = c.next_)
                {
                    if (c.type() == XmlNodeType.DTD) dtd = (XmlDtd)c;
                }
                if (dtd != null) dtdDumpOutput(out, dtd);
                if (cur.children_ != null)
                {
                    cur = cur.children_;
                    continue;
                }
                out.append('\n');
            }
            case ELEMENT ->
            {
                XmlElement e = (XmlElement)cur;
                XmlHtml.Element info = info(cur);
                out.append('<');
                appendQName(out, e.ns(), cur.name());
                for (XmlNs ns : e.nsDefs()) nsDumpOutput(out, ns);
                for (XmlAttr a : e.attributes()) attrDumpOutput(out, a);

                XmlNode first = cur.children_;
                if (info != null && info.empty())
                {
                    out.append('>');
                }
                else if (first == null)
                {
                    out.append("></");
                    appendQName(out, e.ns(), cur.name());
                    out.append('>');
                }
                else
                {
                    out.append('>');
                    if (format && info != null && !info.isinline() &&
                        !isTextOrRef(first) &&
                        first != cur.last_ &&
                        !nameStartsWithP(cur)) // p, pre, param
                    {
                        out.append('\n');
                    }
                    if (info != null && info.data_mode() >= XmlHtml.DATA_RAWTEXT) is_raw = true;
                    cur = first;
                    continue;
                }

                XmlNode next = cur.next_;
                XmlNode parent = cur.parent_;
                if (format && next != null && info != null && !info.isinline())
                {
                    if (!isTextOrRef(next) && parent != null && parent.type() == XmlNodeType.ELEMENT &&
                        !nameStartsWithP(parent))
                    {
                        out.append('\n');
                    }
                }
            }
            case ATTRIBUTE -> attrDumpOutput(out, (XmlAttr)cur);
            case TEXT ->
            {
                if (cur.content() != null)
                {
                    // Text marked "textnoenc" (xslt disable-output-escaping) is written as it is.
                    if (is_raw || "textnoenc".equals(cur.name())) out.append(cur.content());
                    else escape(out, cur.content(), false);
                }
            }
            case COMMENT ->
            {
                if (cur.content() != null) out.append("<!--").append(cur.content()).append("-->");
            }
            case PI ->
            {
                if (cur.name() != null)
                {
                    out.append("<?").append(cur.name());
                    if (cur.content() != null) out.append(' ').append(cur.content());
                    out.append('>');
                }
            }
            case ENTITY_REF -> out.append('&').append(cur.name()).append(';');
            case CDATA ->
            {
                // HTML_PRESERVE_NODE: raw text such as script content.
                if (cur.content() != null) out.append(cur.content());
            }
            default ->
            {
                // The dtd is written first by the document, declarations are not html.
            }
            }

            while (true)
            {
                if (cur == root) return;
                if (cur.next_ != null)
                {
                    cur = cur.next_;
                    break;
                }
                is_raw = false;
                cur = cur.parent_;
                if (cur.type() == XmlNodeType.HTML_DOCUMENT || cur.type() == XmlNodeType.DOCUMENT)
                {
                    out.append('\n');
                }
                else
                {
                    XmlHtml.Element info = format ? info(cur) : null;
                    XmlNode last = cur.last_;
                    if (format && info != null && !info.isinline() &&
                        !isTextOrRef(last) &&
                        cur.children_ != last &&
                        !nameStartsWithP(cur))
                    {
                        out.append('\n');
                    }
                    out.append("</");
                    appendQName(out, ((XmlElement)cur).ns(), cur.name());
                    out.append('>');

                    XmlNode cn = cur.next_;
                    XmlNode parent = cur.parent_;
                    if (format && info != null && !info.isinline() && cn != null)
                    {
                        if (!isTextOrRef(cn) && parent != null && parent.type() == XmlNodeType.ELEMENT &&
                            !nameStartsWithP(parent))
                        {
                            out.append('\n');
                        }
                    }
                }
            }
        }
    }

    private static boolean isTextOrRef(XmlNode n)
    {
        return n.type() == XmlNodeType.TEXT || n.type() == XmlNodeType.ENTITY_REF;
    }

    private static boolean nameStartsWithP(XmlNode n)
    {
        return n != null && n.name() != null && n.name().startsWith("p");
    }

    // Meta tags with the encoding, port of htmlSetMetaEncoding (HTMLtree.c).

    private static boolean is_ws(int c)
    {
        return c == 0x20 || (c >= 0x09 && c <= 0x0D && c != 0x0B);
    }

    private static XmlNode findFirstChild(XmlNode parent, String name)
    {
        for (XmlNode c = parent.firstChild(); c != null; c = c.next())
        {
            if (c.type() == XmlNodeType.ELEMENT && name.equalsIgnoreCase(c.name())) return c;
        }
        return null;
    }

    private static int at(String s, int i)
    {
        return i < s.length() ? s.charAt(i) : 0;
    }

    /** Port of htmlParseContentType: {start, end, size} of the charset in a content-type value. */
    private static int[] parseContentType(String val)
    {
        int p = 0;
        while (true)
        {
            int start, end;
            while (at(val, p) != 'c' && at(val, p) != 'C')
            {
                if (at(val, p) == 0) return null;
                p += 1;
            }
            p += 1;
            if (!val.regionMatches(true, p, "harset", 0, 6)) continue;
            p += 6;
            while (is_ws(at(val, p))) p += 1;
            if (at(val, p) != '=') continue;
            p += 1;
            while (is_ws(at(val, p))) p += 1;
            if (at(val, p) == 0) return null;
            if (at(val, p) == '"' || at(val, p) == '\'')
            {
                int quote = at(val, p);
                p += 1;
                while (is_ws(at(val, p))) p += 1;
                start = p;
                end = start;
                while (at(val, p) != quote)
                {
                    if (at(val, p) == 0) return null;
                    if (!is_ws(at(val, p))) end = p + 1;
                    p += 1;
                }
            }
            else
            {
                start = p;
                while (at(val, p) != 0 && at(val, p) != ';' && !is_ws(at(val, p))) p += 1;
                end = p;
            }
            return new int[] { start, end, val.length() };
        }
    }

    private static String simpleAttrValue(XmlAttr a)
    {
        XmlNode c = a.firstChild();
        if (c != null && c.type() == XmlNodeType.TEXT && c.next() == null && c.content() != null) return c.content();
        return null;
    }

    /**
     * Create or update the meta tags with the encoding in the head (libxml2 htmlSetMetaEncoding).
     * @param doc The document.
     * @param encoding The encoding.
     * @return 0, or 1 if there is no html/head.
     */
    public static int setMetaEncoding(XmlDocument doc, String encoding)
    {
        if (encoding == null) return 1;
        XmlNode html = findFirstChild(doc, "html");
        XmlNode head = html != null ? findFirstChild(html, "head") : null;
        if (head == null) return 1;
        boolean found = false;
        for (XmlNode meta = head.firstChild(); meta != null; meta = meta.next())
        {
            // htmlParseMetaEncoding and htmlFindMetaEncodingAttr.
            if (meta.type() != XmlNodeType.ELEMENT || !"meta".equalsIgnoreCase(meta.name())) continue;
            XmlAttr attr = null, content_attr = null;
            boolean ct = false, is_content_type = false;
            for (XmlAttr a : ((XmlElement)meta).attributes())
            {
                if (a.ns() != null) continue;
                if ("charset".equalsIgnoreCase(a.name()))
                {
                    attr = a;
                    break;
                }
                if ("content".equalsIgnoreCase(a.name())) content_attr = a;
                String v = simpleAttrValue(a);
                if ("http-equiv".equalsIgnoreCase(a.name()) && v != null && v.equalsIgnoreCase("Content-Type")) ct = true;
            }
            if (attr == null && ct && content_attr != null)
            {
                attr = content_attr;
                is_content_type = true;
            }
            if (attr == null) continue;
            String val = simpleAttrValue(attr);
            if (val == null) val = "";
            int[] off;
            if (!is_content_type)
            {
                int size = val.length(), start = 0, end = size;
                while (start < size && is_ws(val.charAt(start))) start += 1;
                while (end > 0 && is_ws(val.charAt(end - 1))) end -= 1;
                off = new int[] { start, end, size };
            }
            else
            {
                off = parseContentType(val);
                if (off == null) continue;
            }
            found = true;
            // The pseudo "HTML" encoding only produces ASCII.
            String enc = encoding.equalsIgnoreCase("HTML") ? "ASCII" : encoding;
            attr.setValue(val.substring(0, off[0]) + enc + val.substring(off[1], off[2]));
        }
        if (found) return 0;
        XmlElement meta = new XmlElement("meta");
        meta.addAttribute(new XmlAttr("charset", encoding));
        XmlNode first = head.firstChild();
        if (first == null) head.addChild(meta);
        else first.addPrevSibling(meta);
        return 0;
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

    /** xmlNsDumpOutput without a save context: attribute escaping plus non ascii as char refs. */
    private static void nsDumpOutput(StringBuilder out, XmlNs ns)
    {
        if (ns.href() == null) return;
        if ("xml".equals(ns.prefix())) return;
        out.append(ns.prefix() != null ? " xmlns:" : " xmlns");
        if (ns.prefix() != null) out.append(ns.prefix());
        out.append("=\"");
        // Like xmlSerializeText(XML_ESCAPE_ATTR | XML_ESCAPE_NON_ASCII), there is no save context.
        escapeAttrNonAscii(out, ns.href());
        out.append('"');
    }

    /** Port of htmlAttrDumpOutput. */
    private static void attrDumpOutput(StringBuilder out, XmlAttr a)
    {
        out.append(' ');
        appendQName(out, a.ns(), a.name());
        XmlNode first = a.children_;
        if (first == null || XmlHtml.isBooleanAttr(a.name())) return;

        out.append("=\"");
        XmlNode owner = a.parent_;
        String name = a.name();
        boolean is_uri = a.ns() == null && owner instanceof XmlElement oe && oe.ns() == null &&
            (name.equalsIgnoreCase("href") || name.equalsIgnoreCase("action") ||
             name.equalsIgnoreCase("src") ||
             (name.equalsIgnoreCase("name") && "a".equalsIgnoreCase(oe.name())));
        for (XmlNode c = first; c != null; c = c.next_)
        {
            if (c.type() == XmlNodeType.TEXT)
            {
                if (c.content() == null) continue;
                if (is_uri) serializeUri(out, c.content());
                else escape(out, c.content(), true);
            }
            else if (c.type() == XmlNodeType.ENTITY_REF)
            {
                out.append('&').append(c.name()).append(';');
            }
        }
        out.append('"');
    }

    /** xmlSerializeText with the htmlEscapeTab / htmlEscapeTabAttr tables: &amp; &lt; &gt; (and " in attributes). */
    private static void escape(StringBuilder out, String s, boolean attr)
    {
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            switch (c)
            {
            case '&' -> out.append("&amp;");
            case '<' -> out.append("&lt;");
            case '>' -> out.append("&gt;");
            case '"' -> out.append(attr ? "&quot;" : "\"");
            default -> out.append(c);
            }
        }
    }

    /** Port of htmlSerializeUri: %-escape spaces, controls and non ascii (each utf-8 byte), escape " and &amp;. */
    private static void serializeUri(StringBuilder out, String content)
    {
        byte[] b = content.getBytes(StandardCharsets.UTF_8);
        int i = 0;
        // Initial whitespace is kept as it is.
        while (i < b.length && (b[i] == 0x09 || b[i] == 0x0A || b[i] == 0x0C || b[i] == 0x0D || b[i] == 0x20))
        {
            out.append((char)b[i]);
            i++;
        }
        for (; i < b.length; i++)
        {
            int c = b[i] & 0xff;
            if (c > 0x20 && c < 0x7F && c != '"' && c != '&') out.append((char)c);
            else if (c == '"') out.append("&quot;");
            else if (c == '&') out.append("&amp;");
            else out.append('%').append(HEX.charAt(c >> 4)).append(HEX.charAt(c & 0xF));
        }
    }

    private static final String HEX = "0123456789ABCDEF";

    /** xml_save_escape_attr(.., true) in C: attribute escaping, non ascii as &amp;#xHEX; char refs. */
    private static void escapeAttrNonAscii(StringBuilder out, String s)
    {
        for (int i = 0; i < s.length(); )
        {
            int c = s.codePointAt(i);
            i += Character.charCount(c);
            switch (c)
            {
            case '&' -> out.append("&amp;");
            case '<' -> out.append("&lt;");
            case '>' -> out.append("&gt;");
            case '\r' -> out.append("&#13;");
            case '\t' -> out.append("&#9;");
            case '\n' -> out.append("&#10;");
            case '"' -> out.append("&quot;");
            default ->
            {
                if (c < 0x20) out.append("&#xFFFD;");
                else if (c < 0x80) out.append((char)c);
                else
                {
                    if (c == 0xFFFE || c == 0xFFFF || (c >= 0xD800 && c <= 0xDFFF)) c = 0xFFFD;
                    out.append("&#x").append(Integer.toHexString(c).toUpperCase()).append(';');
                }
            }
            }
        }
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

    /** The element description of an html element without a namespace, else null (xml_html_info). */
    private static XmlHtml.Element info(XmlNode n)
    {
        if (n.type() != XmlNodeType.ELEMENT || ((XmlElement)n).ns() != null) return null;
        return XmlHtml.element(n.name());
    }
}
