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
     * An html document is dumped as html.
     * @param doc The document.
     * @param encoding The encoding to put in the xml declaration, eg utf-8.
     * @param format True to indent the output.
     * @return The serialized document.
     */
    public static String docDump(XmlDocument doc, String encoding, boolean format)
    {
        if (doc.isHtml())
        {
            return HtmlSave.docContentDump(doc, format);
        }
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
            case ELEMENT_DECL, ATTRIBUTE_DECL, NOTATION_DECL -> buf.append(((XmlDtdDecl)cur).text());
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
                if (cur.content() != null) escape_content(buf, cur.content());
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
        buf.append('=');
        write_quoted(buf, ns.href());
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
