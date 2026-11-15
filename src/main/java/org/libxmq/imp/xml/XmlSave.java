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

/* Portions of this file are ported from libxml2 (xmlsave.c).
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

/**
 * Serializes the xmq DOM as xml. This is a port of the libxml2
 * (Copyright (C) 1998-2012 Daniel Veillard, MIT license) xmlsave.c
 * serializer, so that the output is identical to the C version of xmq,
 * which calls xmlDocDumpMemoryEnc and xmlNodeDump.
 */
public final class XmlSave
{
    static final String INDENT = "  ";
    static final int INDENT_SIZE = 2;
    static final int INDENT_NR = 60 / INDENT_SIZE;

    final StringBuilder buf;
    int level;
    int format;

    XmlSave(StringBuilder buf, int level, int format)
    {
        this.buf = buf;
        this.level = level;
        this.format = format;
    }

    /**
     * Dump a document like libxml2 xmlDocDumpMemoryEnc(doc, ..., encoding),
     * ie without formatting. An html document is dumped as html.
     * @param doc The document.
     * @param encoding The encoding to put in the xml declaration, eg utf-8.
     * @return The serialized document.
     */
    public static String docDump(XmlDocument doc, String encoding)
    {
        return docDump(doc, encoding, false);
    }

    /**
     * Dump a document like libxml2 xmlDocDumpFormatMemoryEnc(doc, ..., encoding, format).
     * Like the C version, which has one dom for xml and html, an html document is
     * dumped as xml too, use HtmlSave.docContentDump for html.
     * @param doc The document.
     * @param encoding The encoding to put in the xml declaration, eg utf-8.
     * @param format True to indent the output.
     * @return The serialized document.
     */
    public static String docDump(XmlDocument doc, String encoding, boolean format)
    {
        StringBuilder sb = new StringBuilder();
        XmlSave ctxt = new XmlSave(sb, 0, format ? 1 : 0);
        ctxt.doc_content(doc, encoding);
        return sb.toString();
    }

    /**
     * Dump a node like libxml2 xmlNodeDump(buf, doc, node, level, format).
     * @param doc The document, can be null.
     * @param node The node to dump, its children are dumped too.
     * @param level The initial indentation level.
     * @param format True to add formatting whitespace.
     * @return The serialized node.
     */
    public static String nodeDump(XmlDocument doc, XmlNode node, int level, boolean format)
    {
        StringBuilder sb = new StringBuilder();
        XmlSave ctxt = new XmlSave(sb, level, format ? 1 : 0);
        ctxt.node(node);
        return sb.toString();
    }

    void doc_content(XmlDocument doc, String encoding)
    {
        buf.append("<?xml version=");
        write_quoted(buf, doc.version() != null ? doc.version() : "1.0");
        if (encoding != null)
        {
            buf.append(" encoding=");
            write_quoted(buf, encoding);
        }
        switch (doc.standalone())
        {
        case 0 -> buf.append(" standalone=\"no\"");
        case 1 -> buf.append(" standalone=\"yes\"");
        default -> { }
        }
        buf.append("?>\n");
        for (XmlNode child = doc.firstChild(); child != null; child = child.next())
        {
            level = 0;
            node(child);
            buf.append('\n');
        }
    }

    void indent()
    {
        int n = level > INDENT_NR ? INDENT_NR : level;
        for (int k = 0; k < n; k++) buf.append(INDENT);
    }

    /** Port of xmlNodeDumpOutputInternal. */
    void node(XmlNode cur)
    {
        int saved_format = format;
        XmlNode root = cur;
        XmlNode unformatted = null;
        while (true)
        {
            boolean descended = false;
            switch (cur.type())
            {
            case DOCUMENT, HTML_DOCUMENT -> doc_content((XmlDocument)cur, "UTF-8");
            case DTD -> dtd((XmlDtd)cur);
            case ELEMENT_DECL, ATTRIBUTE_DECL, NOTATION_DECL, PE_REF -> buf.append(((XmlDtdDecl)cur).text());
            case ENTITY_DECL -> entity_decl(buf, (XmlEntityDecl)cur);
            case ELEMENT ->
            {
                XmlElement e = (XmlElement)cur;
                if (cur != root && format == 1) indent();
                buf.append('<').append(e.fullName());
                for (XmlNs ns : e.nsDefs()) ns(ns);
                for (XmlAttr a : e.attributes()) attr(a);
                if (cur.firstChild() == null)
                {
                    buf.append("/>");
                }
                else
                {
                    if (format == 1)
                    {
                        for (XmlNode t = cur.firstChild(); t != null; t = t.next())
                        {
                            if (t.type() == XmlNodeType.TEXT || t.type() == XmlNodeType.CDATA
                                || t.type() == XmlNodeType.ENTITY_REF)
                            {
                                format = 0;
                                unformatted = cur;
                                break;
                            }
                        }
                    }
                    buf.append('>');
                    if (format == 1) buf.append('\n');
                    if (level >= 0) level++;
                    cur = cur.firstChild();
                    descended = true;
                }
            }
            case TEXT ->
            {
                // Text marked "textnoenc" (xslt disable-output-escaping) is written as it is.
                if (cur.content() != null)
                {
                    if ("textnoenc".equals(cur.name())) buf.append(cur.content());
                    else escape_content(buf, cur.content());
                }
            }
            case NAMESPACE ->
            {
                // An xpath namespace node, like libxml2 xmlNsDumpOutput.
                buf.append(" xmlns");
                if (cur.name() != null) buf.append(':').append(cur.name());
                buf.append('=');
                write_quoted(buf, cur.content() != null ? cur.content() : "");
            }
            case PI ->
            {
                if (cur != root && format == 1) indent();
                buf.append("<?").append(cur.name());
                if (cur.content() != null)
                {
                    buf.append(' ').append(cur.content());
                }
                buf.append("?>");
            }
            case COMMENT ->
            {
                if (cur != root && format == 1) indent();
                if (cur.content() != null)
                {
                    buf.append("<!--").append(cur.content()).append("-->");
                }
            }
            case ENTITY_REF -> buf.append('&').append(cur.name()).append(';');
            case CDATA -> cdata(cur.content());
            case ATTRIBUTE -> attr((XmlAttr)cur);
            default -> { }
            }
            if (descended) continue;

            while (true)
            {
                if (cur == root)
                {
                    format = saved_format;
                    return;
                }
                if (format == 1) buf.append('\n');
                if (cur.next() != null)
                {
                    cur = cur.next();
                    break;
                }
                cur = cur.parent();
                if (cur.type() == XmlNodeType.ELEMENT)
                {
                    if (level > 0) level--;
                    if (format == 1) indent();
                    buf.append("</").append(cur.fullName()).append('>');
                    if (cur == unformatted)
                    {
                        format = saved_format;
                        unformatted = null;
                    }
                }
            }
        }
    }

    // XHTML, port of xhtmlNodeDumpOutput (used when the dtd is an XHTML 1.0 dtd).

    static final String XHTML_NS = "http://www.w3.org/1999/xhtml";

    /** Port of xmlIsXHTML: 1 for the XHTML 1.0 strict, frameset and transitional dtds, -1 if no ids. */
    static int is_xhtml(String system_id, String public_id)
    {
        if (system_id == null && public_id == null) return -1;
        if (public_id != null)
        {
            if (public_id.equals("-//W3C//DTD XHTML 1.0 Strict//EN")) return 1;
            if (public_id.equals("-//W3C//DTD XHTML 1.0 Frameset//EN")) return 1;
            if (public_id.equals("-//W3C//DTD XHTML 1.0 Transitional//EN")) return 1;
        }
        if (system_id != null)
        {
            if (system_id.equals("http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd")) return 1;
            if (system_id.equals("http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd")) return 1;
            if (system_id.equals("http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd")) return 1;
        }
        return 0;
    }

    private static final java.util.Set<String> XHTML_EMPTY = java.util.Set.of(
        "area", "br", "base", "basefont", "col", "frame", "hr", "img", "input", "isindex", "link", "meta", "param");

    /** Port of xhtmlIsEmpty: an element with an empty content model. */
    static int xhtml_is_empty(XmlNode node)
    {
        if (node == null) return -1;
        if (node.type() != XmlNodeType.ELEMENT) return 0;
        XmlNs ns = ((XmlElement)node).ns();
        if (ns != null && !XHTML_NS.equals(ns.href())) return 0;
        if (node.firstChild() != null) return 0;
        String name = node.name();
        if (name == null) return 0;
        return XHTML_EMPTY.contains(name) ? 1 : 0;
    }

    private static boolean attr_is(XmlAttr a, String name, boolean with_ns)
    {
        if (with_ns != (a.ns() != null)) return false;
        return name.equals(a.name());
    }

    private void attr_content(XmlAttr a)
    {
        for (XmlNode c = a.firstChild(); c != null; c = c.next())
        {
            if (c.type() == XmlNodeType.TEXT) attr_serialize_text(buf, c.content());
            else if (c.type() == XmlNodeType.ENTITY_REF) buf.append('&').append(c.name()).append(';');
        }
    }

    private static final java.util.Set<String> XHTML_ID_NAMES = java.util.Set.of(
        "a", "p", "div", "img", "map", "applet", "form", "frame", "iframe");

    /** Port of xhtmlAttrListDumpOutput (with the C.7 and C.8 compatibility attributes). */
    private void xhtml_attrs(XmlElement parent)
    {
        XmlAttr xml_lang = null, lang = null, name = null, id = null;
        for (XmlAttr a : parent.attributes())
        {
            if (attr_is(a, "id", false)) id = a;
            else if (attr_is(a, "name", false)) name = a;
            else if (attr_is(a, "lang", false)) lang = a;
            else if (attr_is(a, "lang", true) && "xml".equals(a.ns().prefix())) xml_lang = a;
            attr(a);
        }
        // C.8
        if (name != null && id == null && parent.name() != null && XHTML_ID_NAMES.contains(parent.name()))
        {
            buf.append(" id=\"");
            attr_content(name);
            buf.append('"');
        }
        // C.7.
        if (lang != null && xml_lang == null)
        {
            buf.append(" xml:lang=\"");
            attr_content(lang);
            buf.append('"');
        }
        else if (xml_lang != null && lang == null)
        {
            buf.append(" lang=\"");
            attr_content(xml_lang);
            buf.append('"');
        }
    }

    private void meta_content_type(String encoding)
    {
        buf.append("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=");
        buf.append(encoding != null ? encoding : "UTF-8");
        buf.append("\" />");
    }

    /** Port of xhtmlNodeDumpOutput. */
    void xhtml_node(XmlNode cur, String encoding)
    {
        int saved_format = format;
        XmlNode root = cur;
        XmlNode unformatted = null;
        while (true)
        {
            boolean descended = false;
            switch (cur.type())
            {
            case DOCUMENT, HTML_DOCUMENT -> doc_content((XmlDocument)cur, encoding);
            case DTD -> dtd((XmlDtd)cur);
            case ELEMENT_DECL, ATTRIBUTE_DECL, NOTATION_DECL -> buf.append(((XmlDtdDecl)cur).text());
            case ENTITY_DECL -> entity_decl(buf, (XmlEntityDecl)cur);
            case ELEMENT ->
            {
                XmlElement e = (XmlElement)cur;
                boolean addmeta = false;
                if (cur != root && format == 1) indent();
                buf.append('<').append(e.fullName());
                for (XmlNs ns : e.nsDefs()) ns(ns);
                if ("html".equals(e.name()) && e.ns() == null && e.nsDefs().isEmpty())
                {
                    // 3.1.1. Strictly Conforming Documents A.3.1.1 3/
                    buf.append(" xmlns=\"http://www.w3.org/1999/xhtml\"");
                }
                if (!e.attributes().isEmpty()) xhtml_attrs(e);
                XmlNode parent = cur.parent();
                if (parent != null && parent.type() == XmlNodeType.ELEMENT && parent.parent() != null
                    && (parent.parent().type() == XmlNodeType.DOCUMENT || parent.parent().type() == XmlNodeType.HTML_DOCUMENT)
                    && "head".equals(e.name()) && "html".equals(parent.name()))
                {
                    XmlNode tmp;
                    for (tmp = cur.firstChild(); tmp != null; tmp = tmp.next())
                    {
                        if ("meta".equals(tmp.name()) && tmp.type() == XmlNodeType.ELEMENT)
                        {
                            XmlAttr he = XmlParser.findAttr((XmlElement)tmp, "http-equiv", null);
                            if (he != null && he.value().equalsIgnoreCase("Content-Type")) break;
                        }
                    }
                    if (tmp == null) addmeta = true;
                }
                XmlNode first = cur.firstChild();
                if (first == null)
                {
                    XmlNs ns = e.ns();
                    if ((ns == null || ns.prefix() == null) && xhtml_is_empty(cur) == 1 && !addmeta)
                    {
                        // C.2. Empty Elements
                        buf.append(" />");
                    }
                    else
                    {
                        buf.append('>');
                        if (addmeta)
                        {
                            if (format == 1)
                            {
                                buf.append('\n');
                                level++;
                                indent();
                                level--;
                            }
                            meta_content_type(encoding);
                            if (format == 1) buf.append('\n');
                        }
                        // C.3. Element Minimization and Empty Element Content
                        buf.append("</").append(e.fullName()).append('>');
                    }
                }
                else
                {
                    buf.append('>');
                    if (addmeta)
                    {
                        if (format == 1)
                        {
                            buf.append('\n');
                            level++;
                            indent();
                            level--;
                        }
                        meta_content_type(encoding);
                    }
                    if (format == 1)
                    {
                        for (XmlNode t = first; t != null; t = t.next())
                        {
                            if (t.type() == XmlNodeType.TEXT || t.type() == XmlNodeType.ENTITY_REF)
                            {
                                unformatted = cur;
                                format = 0;
                                break;
                            }
                        }
                    }
                    if (format == 1) buf.append('\n');
                    if (level >= 0) level++;
                    cur = first;
                    descended = true;
                }
            }
            case TEXT ->
            {
                if (cur.content() != null)
                {
                    if ("textnoenc".equals(cur.name())) buf.append(cur.content());
                    else escape_content(buf, cur.content());
                }
            }
            case PI ->
            {
                buf.append("<?").append(cur.name());
                if (cur.content() != null) buf.append(' ').append(cur.content());
                buf.append("?>");
            }
            case COMMENT ->
            {
                if (cur.content() != null) buf.append("<!--").append(cur.content()).append("-->");
            }
            case ENTITY_REF -> buf.append('&').append(cur.name()).append(';');
            case CDATA -> cdata(cur.content());
            case ATTRIBUTE -> attr((XmlAttr)cur);
            default -> { }
            }
            if (descended) continue;
            while (true)
            {
                if (cur == root)
                {
                    format = saved_format;
                    return;
                }
                if (format == 1) buf.append('\n');
                if (cur.next() != null)
                {
                    cur = cur.next();
                    break;
                }
                cur = cur.parent();
                if (cur.type() == XmlNodeType.ELEMENT)
                {
                    if (level > 0) level--;
                    if (format == 1) indent();
                    buf.append("</").append(cur.fullName()).append('>');
                    if (cur == unformatted)
                    {
                        format = saved_format;
                        unformatted = null;
                    }
                }
            }
        }
    }

    /**
     * Serialize a node like libxml2 xmlNodeDumpOutput: in the XHTML mode if the dtd of doc is an
     * XHTML 1.0 dtd. Text is written as a String (transcode afterwards for other encodings);
     * encoding (null is UTF-8) is only used for the XHTML meta tag.
     * @param out Where to append the output.
     * @param doc The document whose dtd decides the XHTML mode, can be null.
     * @param node The node.
     * @param level The indentation level.
     * @param format True to indent.
     * @param encoding The encoding for the meta tag, or null.
     */
    public static void nodeDumpOutput(StringBuilder out, XmlDocument doc, XmlNode node, int level, boolean format, String encoding)
    {
        if (node == null) return;
        if (level < 0) level = 0;
        else if (level > 100) level = 100;
        if (encoding == null) encoding = "UTF-8";
        XmlSave ctxt = new XmlSave(out, level, format ? 1 : 0);
        int xhtml = 0;
        XmlDtd dtd = doc != null ? doc.intSubset() : null;
        if (dtd != null)
        {
            xhtml = is_xhtml(dtd.systemId(), dtd.externalId());
            if (xhtml < 0) xhtml = 0;
        }
        if (xhtml != 0) ctxt.xhtml_node(node, encoding);
        else ctxt.node(node);
    }

    // Output encodings, like libxml2 xmlCharEncOutput (characters that the encoding
    // cannot represent become decimal character references).

    private static final int ENC_UTF8 = 0, ENC_LATIN1 = 1, ENC_ASCII = 2, ENC_UTF16 = 3, ENC_UTF16LE = 4,
                             ENC_UTF16BE = 5, ENC_UNKNOWN = 6;

    private static int find_encoding(String encoding)
    {
        if (encoding == null) return ENC_UTF8;
        switch (encoding.toUpperCase(java.util.Locale.ROOT))
        {
        case "UTF-8": case "UTF8": return ENC_UTF8;
        case "ISO-8859-1": case "ISO-LATIN-1": case "ISO_8859-1": case "LATIN1": case "L1": case "ISO8859-1":
            return ENC_LATIN1;
        case "US-ASCII": case "ASCII": return ENC_ASCII;
        case "UTF-16": case "UTF16": return ENC_UTF16;
        case "UTF-16LE": return ENC_UTF16LE;
        case "UTF-16BE": return ENC_UTF16BE;
        default: return ENC_UNKNOWN;
        }
    }

    /**
     * Is the output encoding supported by transcode (UTF-8, ISO-8859-1, ASCII, UTF-16)?
     * @param encoding The encoding name, null is UTF-8.
     * @return True if supported.
     */
    public static boolean encodingSupported(String encoding)
    {
        return find_encoding(encoding) != ENC_UNKNOWN;
    }

    /**
     * The text converted to the encoding, characters the encoding cannot represent become
     * decimal character references (like libxml2 output encoders).
     * @param text The text.
     * @param encoding The encoding, null is UTF-8.
     * @return The bytes, or null if the encoding is not supported.
     */
    public static byte[] transcode(String text, String encoding)
    {
        int enc = find_encoding(encoding);
        if (enc == ENC_UNKNOWN) return null;
        if (enc == ENC_UTF8) return text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        // The UTF-16 encoder writes a byte order mark (little endian) with the
        // first converted bytes, nothing when there is nothing to convert.
        if (enc == ENC_UTF16 && !text.isEmpty())
        {
            out.write(0xFF);
            out.write(0xFE);
        }
        for (int i = 0; i < text.length(); )
        {
            int c = text.codePointAt(i);
            i += Character.charCount(c);
            switch (enc)
            {
            case ENC_LATIN1, ENC_ASCII ->
            {
                if (c < (enc == ENC_LATIN1 ? 0x100 : 0x80))
                {
                    out.write(c);
                }
                else
                {
                    byte[] ref = ("&#" + c + ";").getBytes(java.nio.charset.StandardCharsets.US_ASCII);
                    out.write(ref, 0, ref.length);
                }
            }
            default ->
            {
                boolean be = enc == ENC_UTF16BE;
                int[] units = c >= 0x10000
                    ? new int[] { 0xD800 | ((c - 0x10000) >> 10), 0xDC00 | ((c - 0x10000) & 0x3FF) }
                    : new int[] { c };
                for (int u : units)
                {
                    int hi = (u >> 8) & 0xFF, lo = u & 0xFF;
                    out.write(be ? hi : lo);
                    out.write(be ? lo : hi);
                }
            }
            }
        }
        return out.toByteArray();
    }

    void cdata(String content)
    {
        if (content == null || content.isEmpty())
        {
            buf.append("<![CDATA[]]>");
            return;
        }
        int start = 0;
        int end = 0;
        while (end < content.length())
        {
            if (content.startsWith("]]>", end))
            {
                end = end + 2;
                buf.append("<![CDATA[").append(content, start, end).append("]]>");
                start = end;
            }
            end++;
        }
        if (start != end)
        {
            buf.append("<![CDATA[").append(content.substring(start)).append("]]>");
        }
    }

    /** Port of xmlNsDumpOutput. */
    void ns(XmlNs ns)
    {
        if (ns.href() == null) return;
        if ("xml".equals(ns.prefix())) return;
        buf.append(' ');
        if (ns.prefix() != null)
        {
            buf.append("xmlns:").append(ns.prefix());
        }
        else
        {
            buf.append("xmlns");
        }
        // Escaped as an attribute value, like libxml2 2.16 and the C version.
        buf.append("=\"");
        attr_serialize_text(buf, ns.href());
        buf.append('"');
    }

    /** Port of xmlAttrDumpOutput. */
    void attr(XmlAttr a)
    {
        buf.append(' ').append(a.fullName()).append("=\"");
        for (XmlNode c = a.firstChild(); c != null; c = c.next())
        {
            if (c.type() == XmlNodeType.TEXT)
            {
                attr_serialize_text(buf, c.content());
            }
            else if (c.type() == XmlNodeType.ENTITY_REF)
            {
                buf.append('&').append(c.name()).append(';');
            }
        }
        buf.append('"');
    }

    /** Port of xmlDtdDumpOutput. */
    void dtd(XmlDtd dtd)
    {
        buf.append("<!DOCTYPE ").append(dtd.name());
        if (dtd.externalId() != null)
        {
            buf.append(" PUBLIC ");
            write_quoted(buf, dtd.externalId());
            buf.append(' ');
            write_quoted(buf, dtd.systemId());
        }
        else if (dtd.systemId() != null)
        {
            buf.append(" SYSTEM ");
            write_quoted(buf, dtd.systemId());
        }
        if (!dtd.hasDeclarations())
        {
            buf.append('>');
            return;
        }
        buf.append(" [\n");
        for (XmlDtdDecl n : dtd.notations()) buf.append(n.text());
        int f = format;
        int l = level;
        format = 0;
        level = -1;
        for (XmlNode c = dtd.firstChild(); c != null; c = c.next())
        {
            node(c);
        }
        format = f;
        level = l;
        buf.append("]>");
    }

    /** Port of xmlDumpEntityDecl. */
    static void entity_decl(StringBuilder buf, XmlEntityDecl ent)
    {
        switch (ent.etype())
        {
        case XmlEntityDecl.INTERNAL_GENERAL ->
        {
            buf.append("<!ENTITY ").append(ent.name()).append(' ');
            if (ent.orig() != null) write_quoted(buf, ent.orig());
            else entity_content(buf, ent.content());
            buf.append(">\n");
        }
        case XmlEntityDecl.EXTERNAL_GENERAL_PARSED, XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED ->
        {
            buf.append("<!ENTITY ").append(ent.name());
            external_ids(buf, ent);
            if (ent.etype() == XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED && ent.content() != null)
            {
                buf.append(" NDATA ").append(ent.orig() != null ? ent.orig() : ent.content());
            }
            buf.append(">\n");
        }
        case XmlEntityDecl.INTERNAL_PARAMETER ->
        {
            buf.append("<!ENTITY % ").append(ent.name()).append(' ');
            if (ent.orig() == null) entity_content(buf, ent.content());
            else write_quoted(buf, ent.orig());
            buf.append(">\n");
        }
        case XmlEntityDecl.EXTERNAL_PARAMETER ->
        {
            buf.append("<!ENTITY % ").append(ent.name());
            external_ids(buf, ent);
            buf.append(">\n");
        }
        default -> { }
        }
    }

    static void external_ids(StringBuilder buf, XmlEntityDecl ent)
    {
        if (ent.externalId() != null)
        {
            buf.append(" PUBLIC ");
            write_quoted(buf, ent.externalId());
            buf.append(' ');
            write_quoted(buf, ent.systemId());
        }
        else
        {
            buf.append(" SYSTEM ");
            write_quoted(buf, ent.systemId());
        }
    }

    /** Port of xmlDumpEntityContent. */
    static void entity_content(StringBuilder buf, String content)
    {
        if (content.indexOf('%') >= 0)
        {
            buf.append('"');
            for (int k = 0; k < content.length(); k++)
            {
                char c = content.charAt(k);
                if (c == '"') buf.append("&quot;");
                else if (c == '%') buf.append("&#x25;");
                else buf.append(c);
            }
            buf.append('"');
        }
        else
        {
            write_quoted(buf, content);
        }
    }

    /**
     * Port of xmlBufWriteQuotedString: quote with double quotes, unless the
     * string contains double quotes, then use single quotes, unless it
     * contains both, then use double quotes and escape them as &amp;quot;
     * @param buf The output.
     * @param s The string to quote.
     */
    public static void write_quoted(StringBuilder buf, String s)
    {
        if (s == null) s = "";
        if (s.indexOf('"') >= 0)
        {
            if (s.indexOf('\'') >= 0)
            {
                buf.append('"');
                for (int k = 0; k < s.length(); k++)
                {
                    char c = s.charAt(k);
                    if (c == '"') buf.append("&quot;");
                    else buf.append(c);
                }
                buf.append('"');
            }
            else
            {
                buf.append('\'').append(s).append('\'');
            }
        }
        else
        {
            buf.append('"').append(s).append('"');
        }
    }

    /**
     * Port of xmlEscapeContent, used for text when the output encoding is utf-8.
     * @param buf The output.
     * @param s The text to escape.
     */
    public static void escape_content(StringBuilder buf, String s)
    {
        for (int k = 0; k < s.length(); k++)
        {
            char c = s.charAt(k);
            switch (c)
            {
            case '<' -> buf.append("&lt;");
            case '>' -> buf.append("&gt;");
            case '&' -> buf.append("&amp;");
            case '\r' -> buf.append("&#13;");
            default -> buf.append(c);
            }
        }
    }

    /**
     * Port of xmlBufAttrSerializeTxtContent for an utf-8 document.
     * @param buf The output.
     * @param s The attribute text to escape.
     */
    public static void attr_serialize_text(StringBuilder buf, String s)
    {
        if (s == null) return;
        for (int k = 0; k < s.length(); k++)
        {
            char c = s.charAt(k);
            switch (c)
            {
            case '\n' -> buf.append("&#10;");
            case '\r' -> buf.append("&#13;");
            case '\t' -> buf.append("&#9;");
            case '"' -> buf.append("&quot;");
            case '<' -> buf.append("&lt;");
            case '>' -> buf.append("&gt;");
            case '&' -> buf.append("&amp;");
            default -> buf.append(c);
            }
        }
    }
}
