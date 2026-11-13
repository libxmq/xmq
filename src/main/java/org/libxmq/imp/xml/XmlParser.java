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

/**
 * An xml parser that builds the xmq DOM. It behaves like the libxml2
 * (Copyright (C) 1998-2012 Daniel Veillard, MIT license) parser called
 * by the C version of xmq: xmlReadMemory with XML_PARSE_NOCDATA |
 * XML_PARSE_NONET and optionally XML_PARSE_NOBLANKS.
 *
 * Most importantly, entity references are never expanded. A reference to a
 * declared entity becomes an XmlEntityRef node, both in content and in
 * attribute values. Character references and the predefined entities
 * (amp, lt, gt, quot, apos) are expanded into the surrounding text.
 * The internal subset of the doctype is parsed into declaration nodes,
 * so that it can be printed back exactly like libxml2 does.
 */
public final class XmlParser
{
    /** Remove ignorable blank text nodes, like XML_PARSE_NOBLANKS. */
    public static final int NOBLANKS = 1;
    /** Merge cdata sections into text nodes, like XML_PARSE_NOCDATA. */
    public static final int NOCDATA = 2;

    final String s;
    final String source_;
    final int options_;
    int i;
    XmlDocument doc_;
    XmlNode node_; // The current parent, the document or an element.
    boolean in_subset_;
    boolean has_external_subset_;
    boolean has_pe_refs_;
    final List<Integer> space_ = new ArrayList<>(); // xml:space stack, -1 default, 1 preserve, 0 default declared.
    // Attribute defaults from the dtd, element qname -> list of {attribute qname, value}, like libxml2 attsDefault.
    final java.util.Map<String, List<String[]>> att_defaults_ = new java.util.HashMap<>();

    XmlParser(String content, String source, int options)
    {
        String c = content;
        if (c.startsWith("﻿")) c = c.substring(1);
        if (c.indexOf('\r') >= 0)
        {
            // End of line handling, like libxml2 all \r\n and \r become \n.
            c = c.replace("\r\n", "\n").replace('\r', '\n');
        }
        s = c;
        source_ = source == null ? "noname.xml" : source;
        options_ = options;
    }

    /**
     * Parse xml into a document.
     * @param content The xml source.
     * @param source The source name used in error messages, eg the file name.
     * @param options NOBLANKS and/or NOCDATA.
     * @return The document.
     * @throws XmlParseException If the xml is not well formed.
     */
    public static XmlDocument parse(String content, String source, int options) throws XmlParseException
    {
        XmlParser p = new XmlParser(content, source, options);
        return p.parse_document();
    }

    // ---------------------------------------------------------------- errors

    int line_at(int pos)
    {
        int line = 1;
        int end = Math.min(pos, s.length());
        for (int k = 0; k < end; k++)
        {
            if (s.charAt(k) == '\n') line++;
        }
        return line;
    }

    /** Format a message with source context like libxml2 xmlParserPrintFileContextInternal. */
    String format_error(String kind, String msg, int pos)
    {
        int line = line_at(pos);
        StringBuilder out = new StringBuilder();
        out.append(source_).append(':').append(line).append(": ").append(kind).append(" : ").append(msg);
        if (!msg.endsWith("\n")) out.append('\n');
        if (pos > s.length()) pos = s.length();
        int cur = pos;
        // Skip backwards over any end of lines.
        while (cur > 0 && cur < s.length() && s.charAt(cur) == '\n') cur--;
        if (cur >= s.length() && cur > 0) cur = s.length() - 1;
        while (cur > 0 && s.charAt(cur) == '\n') cur--;
        int n = 0;
        while (n++ < 80 && cur > 0 && s.charAt(cur) != '\n') cur--;
        if (cur < s.length() && s.charAt(cur) == '\n') cur++;
        int col = pos - cur;
        StringBuilder content = new StringBuilder();
        int k = cur;
        n = 0;
        while (k < s.length() && s.charAt(k) != '\n' && n < 80)
        {
            content.append(s.charAt(k++));
            n++;
        }
        out.append(content).append('\n');
        StringBuilder caret = new StringBuilder();
        n = 0;
        int ci = 0;
        while (n < col && n++ < 79 && ci < content.length())
        {
            char c = content.charAt(ci++);
            caret.append(c == '\t' ? '\t' : ' ');
        }
        caret.append('^');
        out.append(caret).append('\n');
        return out.toString();
    }

    XmlParseException fatal(String msg)
    {
        return fatal(msg, i);
    }

    XmlParseException fatal(String msg, int pos)
    {
        return new XmlParseException(format_error("parser error", msg, pos), msg, line_at(pos));
    }

    void ns_error(String msg)
    {
        System.err.print(format_error("namespace error", msg, i));
    }

    void warning(String msg)
    {
        System.err.print(format_error("parser error", msg, i));
    }

    // ---------------------------------------------------------------- char classes

    static boolean is_blank(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static boolean is_name_start_char(int c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':'
            || (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF)
            || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D)
            || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF)
            || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
    }

    static boolean is_name_char(int c)
    {
        return is_name_start_char(c) || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == 0xB7
            || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
    }

    static boolean is_xml_char(int c)
    {
        return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF)
            || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
    }

    // ---------------------------------------------------------------- scanning helpers

    boolean eof()
    {
        return i >= s.length();
    }

    char cur()
    {
        return i < s.length() ? s.charAt(i) : 0;
    }

    char nxt(int n)
    {
        return i + n < s.length() ? s.charAt(i + n) : 0;
    }

    boolean at(String t)
    {
        return s.startsWith(t, i);
    }

    boolean skip_blanks()
    {
        int start = i;
        while (i < s.length() && is_blank(s.charAt(i))) i++;
        return i > start;
    }

    /** Parse a Name, returns null if no name starts here. */
    String parse_name()
    {
        if (eof()) return null;
        int c = s.codePointAt(i);
        if (!is_name_start_char(c)) return null;
        int start = i;
        i += Character.charCount(c);
        while (i < s.length())
        {
            c = s.codePointAt(i);
            if (!is_name_char(c)) break;
            i += Character.charCount(c);
        }
        return s.substring(start, i);
    }

    /** Parse an NCName, ie a name without colons. */
    String parse_ncname()
    {
        if (eof()) return null;
        int c = s.codePointAt(i);
        if (!is_name_start_char(c) || c == ':') return null;
        int start = i;
        i += Character.charCount(c);
        while (i < s.length())
        {
            c = s.codePointAt(i);
            if (!is_name_char(c) || c == ':') break;
            i += Character.charCount(c);
        }
        return s.substring(start, i);
    }

    /** Parse an Nmtoken. */
    String parse_nmtoken()
    {
        int start = i;
        while (i < s.length())
        {
            int c = s.codePointAt(i);
            if (!is_name_char(c)) break;
            i += Character.charCount(c);
        }
        if (i == start) return null;
        return s.substring(start, i);
    }

    /** Parse a QName, returns {prefix, local} where prefix can be null, or null if no name. */
    String[] parse_qname()
    {
        int start = i;
        String l = parse_ncname();
        if (l == null)
        {
            // Names starting with a colon are accepted by libxml2 as plain names.
            String n = parse_name();
            if (n == null) return null;
            return new String[] { null, n };
        }
        if (cur() == ':')
        {
            int save = i;
            i++;
            String l2 = parse_ncname();
            if (l2 == null || cur() == ':')
            {
                // Not a valid QName, treat the whole thing as a name.
                i = start;
                String n = parse_name();
                return new String[] { null, n };
            }
            return new String[] { l, l2 };
        }
        return new String[] { null, l };
    }

    // ---------------------------------------------------------------- document

    XmlDocument parse_document() throws XmlParseException
    {
        doc_ = new XmlDocument();
        doc_.setUrl(source_);
        node_ = doc_;

        if (eof())
        {
            throw fatal("Document is empty");
        }

        if (at("<?xml") && is_blank(nxt(5)))
        {
            parse_xml_decl();
        }

        parse_misc();

        if (at("<!DOCTYPE"))
        {
            parse_doctype_decl();
            parse_misc();
        }

        if (eof() || cur() != '<')
        {
            throw fatal("Start tag expected, '<' not found");
        }

        parse_element_tree();

        parse_misc();

        if (!eof())
        {
            throw fatal("Extra content at the end of the document");
        }
        return doc_;
    }

    void parse_xml_decl() throws XmlParseException
    {
        i += 5;
        skip_blanks();
        if (!at("version"))
        {
            throw fatal("Malformed declaration expecting version");
        }
        i += 7;
        String v = parse_eq_literal();
        if (v != null) doc_.setVersion(v);
        boolean blank = skip_blanks();
        if (at("encoding"))
        {
            if (!blank) throw fatal("Blank needed here");
            i += 8;
            String e = parse_eq_literal();
            doc_.setEncoding(e);
            blank = skip_blanks();
        }
        if (at("standalone"))
        {
            if (!blank) throw fatal("Blank needed here");
            i += 10;
            String sa = parse_eq_literal();
            if ("yes".equals(sa)) doc_.setStandalone(1);
            else if ("no".equals(sa)) doc_.setStandalone(0);
            else throw fatal("standalone accepts only 'yes' or 'no'");
            skip_blanks();
        }
        if (at("?>"))
        {
            i += 2;
        }
        else if (cur() == '>')
        {
            throw fatal("XML declaration must end-up with '?>'");
        }
        else
        {
            throw fatal("parsing XML declaration: '?>' expected");
        }
    }

    String parse_eq_literal() throws XmlParseException
    {
        skip_blanks();
        if (cur() != '=') throw fatal("Specification mandates value for attribute version");
        i++;
        skip_blanks();
        char q = cur();
        if (q != '"' && q != '\'') throw fatal("String not started expecting ' or \"");
        int end = s.indexOf(q, i + 1);
        if (end < 0) throw fatal("String not closed expecting \" or '");
        String v = s.substring(i + 1, end);
        i = end + 1;
        return v;
    }

    /** Comments, processing instructions and blanks outside of the root element. */
    void parse_misc() throws XmlParseException
    {
        while (!eof())
        {
            if (at("<?"))
            {
                parse_pi();
            }
            else if (at("<!--"))
            {
                parse_comment();
            }
            else if (is_blank(cur()))
            {
                skip_blanks();
            }
            else
            {
                return;
            }
        }
    }

    // ---------------------------------------------------------------- elements

    static class Open
    {
        Open(XmlElement e, String qname, int line)
        {
            this.e = e;
            this.qname = qname;
            this.line = line;
        }
        final XmlElement e;
        final String qname;
        final int line;
    }

    void parse_element_tree() throws XmlParseException
    {
        List<Open> stack = new ArrayList<>();
        Open root = parse_start_tag();
        if (root == null) return;
        stack.add(root);
        while (!stack.isEmpty())
        {
            Open top = stack.get(stack.size() - 1);
            if (eof())
            {
                throw fatal("Premature end of data in tag " + top.qname + " line " + top.line);
            }
            char c = cur();
            if (c == '<')
            {
                char n = nxt(1);
                if (n == '/')
                {
                    parse_end_tag(top);
                    stack.remove(stack.size() - 1);
                    node_ = stack.isEmpty() ? doc_ : stack.get(stack.size() - 1).e;
                    if (!space_.isEmpty()) space_.remove(space_.size() - 1);
                }
                else if (n == '?')
                {
                    parse_pi();
                }
                else if (at("<!--"))
                {
                    parse_comment();
                }
                else if (at("<![CDATA["))
                {
                    parse_cdata();
                }
                else if (n == '!')
                {
                    throw fatal("StartTag: invalid element name");
                }
                else
                {
                    Open child = parse_start_tag();
                    if (child != null) stack.add(child);
                }
            }
            else if (c == '&')
            {
                parse_reference();
            }
            else
            {
                parse_char_data();
            }
        }
    }

    static class RawAttr
    {
        String prefix;
        String local;
        List<Object> value = new ArrayList<>(); // String or EntityName
        int pos;
    }

    record EntityName(String name) { }

    /** Parse a start tag, returns the open element, or null if it was an empty element tag. */
    Open parse_start_tag() throws XmlParseException
    {
        int tag_start = i;
        int tag_line = line_at(i);
        i++; // Skip <
        String[] qn = parse_qname();
        if (qn == null)
        {
            throw fatal("StartTag: invalid element name");
        }
        String qname = qn[0] == null ? qn[1] : qn[0] + ":" + qn[1];
        List<RawAttr> attrs = new ArrayList<>();
        boolean empty = false;
        while (true)
        {
            boolean blank = skip_blanks();
            if (eof())
            {
                throw fatal("Couldn't find end of Start Tag " + qname + " line " + tag_line);
            }
            if (cur() == '>')
            {
                i++;
                break;
            }
            if (at("/>"))
            {
                i += 2;
                empty = true;
                break;
            }
            if (!blank)
            {
                throw fatal("attributes construct error");
            }
            int apos = i;
            String[] an = parse_qname();
            if (an == null)
            {
                throw fatal("attributes construct error");
            }
            skip_blanks();
            if (cur() != '=')
            {
                String aname = an[0] == null ? an[1] : an[0] + ":" + an[1];
                throw fatal("Specification mandates value for attribute " + aname);
            }
            i++;
            skip_blanks();
            RawAttr ra = new RawAttr();
            ra.prefix = an[0];
            ra.local = an[1];
            ra.pos = apos;
            parse_att_value(ra.value);
            for (RawAttr prev : attrs)
            {
                if (prev.local.equals(ra.local) && java.util.Objects.equals(prev.prefix, ra.prefix))
                {
                    throw fatal("Attribute " + (ra.prefix == null ? ra.local : ra.prefix + ":" + ra.local) + " redefined");
                }
            }
            attrs.add(ra);
        }

        XmlElement e = new XmlElement(qn[1]);

        // Namespace declarations first.
        List<String> declared = new ArrayList<>();
        for (RawAttr ra : attrs)
        {
            String prefix = null;
            if (ra.prefix == null && ra.local.equals("xmlns"))
            {
                prefix = null;
            }
            else if ("xmlns".equals(ra.prefix))
            {
                prefix = ra.local;
            }
            else
            {
                continue;
            }
            String href = value_to_string(ra.value);
            String key = prefix == null ? "" : prefix;
            if (declared.contains(key))
            {
                throw fatal("Attribute xmlns" + (prefix == null ? "" : ":" + prefix) + " redefined");
            }
            declared.add(key);
            if (prefix != null && href.isEmpty())
            {
                ns_error("xmlns:" + prefix + ": Empty XML namespace is not allowed");
                continue;
            }
            if ("xml".equals(prefix)) continue;
            e.nsDefs().add(new XmlNs(prefix, href));
        }

        // Namespace declarations defaulted from the dtd, like libxml2 xmlParseStartTag2.
        List<String[]> defaults = att_defaults_.get(qname);
        if (defaults != null)
        {
            for (String[] d : defaults)
            {
                String prefix;
                if (d[0].equals("xmlns")) prefix = null;
                else if (d[0].startsWith("xmlns:")) prefix = d[0].substring(6);
                else continue;
                String key = prefix == null ? "" : prefix;
                if (declared.contains(key)) continue;
                String in_scope = in_scope_namespace(prefix);
                String value = d[1];
                if (in_scope != null && in_scope.equals(value)) continue;
                declared.add(key);
                e.nsDefs().add(new XmlNs(prefix, value));
            }
        }

        // Attach the element so that namespace lookups see the ancestors.
        node_.appendRaw(e);

        // Resolve the element namespace.
        XmlNs ens = e.searchNs(qn[0]);
        if (qn[0] != null && ens == null)
        {
            ns_error("Namespace prefix " + qn[0] + " on " + qn[1] + " is not defined");
            e.setName(qn[0] + ":" + qn[1]);
        }
        else if (ens != null && !(qn[0] == null && ens.href().isEmpty()))
        {
            e.setNs(ens);
        }

        // Then the attributes.
        List<String> seen = new ArrayList<>();
        int space = space_.isEmpty() ? -1 : space_.get(space_.size() - 1);
        if (space == -2) space = -1;
        for (RawAttr ra : attrs)
        {
            if ((ra.prefix == null && ra.local.equals("xmlns")) || "xmlns".equals(ra.prefix)) continue;
            XmlAttr a = new XmlAttr(ra.local, null);
            String uri = null;
            if (ra.prefix != null)
            {
                XmlNs ans = e.searchNs(ra.prefix);
                if (ans == null)
                {
                    ns_error("Namespace prefix " + ra.prefix + " for " + ra.local + " on " + qn[1] + " is not defined");
                    a.setName(ra.prefix + ":" + ra.local);
                }
                else
                {
                    a.setNs(ans);
                    uri = ans.href();
                }
            }
            String key = (uri == null ? "" : uri) + "}" + a.name();
            if (seen.contains(key))
            {
 throw fatal("Attribute " + (ra.prefix == null ? ra.local : ra.prefix + ":" + ra.local) + " redefined");
            }
            seen.add(key);
            for (Object o : ra.value)
            {
                if (o instanceof String str) a.addChild(new XmlText(str));
                else a.appendRaw(new XmlEntityRef(((EntityName)o).name()));
            }
            e.addAttribute(a);
            if ("xml".equals(ra.prefix) && ra.local.equals("space"))
            {
                String v = value_to_string(ra.value);
                if (v.equals("preserve")) space = 1;
                else if (v.equals("default")) space = 0;
            }
        }

        if (empty)
        {
            return null;
        }
        space_.add(space);
        node_ = e;
        return new Open(e, qname, tag_line);
    }

    /** Like libxml2 xmlGetNamespace: the uri in scope for the prefix, an empty default namespace counts as none. */
    String in_scope_namespace(String prefix)
    {
        if (!(node_ instanceof XmlElement p)) return null;
        XmlNs ns = p.searchNs(prefix);
        if (ns == null) return null;
        if (prefix == null && (ns.href() == null || ns.href().isEmpty())) return null;
        return ns.href();
    }

    static String value_to_string(List<Object> value)
    {
        StringBuilder sb = new StringBuilder();
        for (Object o : value)
        {
            if (o instanceof String str) sb.append(str);
            else sb.append('&').append(((EntityName)o).name()).append(';');
        }
        return sb.toString();
    }

    void parse_end_tag(Open top) throws XmlParseException
    {
        i += 2;
        int name_pos = i;
        String[] qn = parse_qname();
        String name = qn == null ? "" : (qn[0] == null ? qn[1] : qn[0] + ":" + qn[1]);
        skip_blanks();
        if (cur() != '>')
        {
            throw fatal("expected '>'");
        }
        i++;
        if (!name.equals(top.qname))
        {
            throw fatal("Opening and ending tag mismatch: " + top.qname + " line " + top.line + " and " + name);
        }
    }

    /** Parse an attribute value into text and entity reference parts. */
    void parse_att_value(List<Object> out) throws XmlParseException
    {
        char q = cur();
        if (q != '"' && q != '\'')
        {
            throw fatal("AttValue: \" or ' expected");
        }
        i++;
        StringBuilder text = new StringBuilder();
        while (true)
        {
            if (eof())
            {
                throw fatal("AttValue: ' expected");
            }
            char c = cur();
            if (c == q)
            {
                i++;
                break;
            }
            if (c == '<')
            {
                throw fatal("Unescaped '<' not allowed in attributes values");
            }
            if (c == '&')
            {
                if (nxt(1) == '#')
                {
                    text.appendCodePoint(parse_char_ref());
                    continue;
                }
                i++;
                String name = parse_name();
                if (name == null)
                {
                    throw fatal("xmlParseEntityRef: no name");
                }
                if (cur() != ';')
                {
                    throw fatal("EntityRef: expecting ';'");
                }
                i++;
                String pre = predefined(name);
                if (pre != null)
                {
                    text.append(pre);
                    continue;
                }
                XmlEntityDecl ent = doc_.getEntity(name);
                if (ent == null)
                {
                    undeclared_entity(name);
                }
                else if (ent.etype() == XmlEntityDecl.EXTERNAL_GENERAL_PARSED || ent.etype() == XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED)
                {
                    throw fatal("Attribute references external entity '" + name + "'");
                }
                else if (ent.content() != null && ent.content().indexOf('<') >= 0)
                {
                    throw fatal("'<' in entity '" + name + "' is not allowed in attributes values");
                }
                if (text.length() > 0)
                {
                    out.add(text.toString());
                    text.setLength(0);
                }
                out.add(new EntityName(name));
                continue;
            }
            if (c == '\t' || c == '\n' || c == '\r')
            {
                text.append(' ');
            }
            else
            {
                text.append(c);
            }
            i++;
        }
        if (text.length() > 0) out.add(text.toString());
    }

    static String predefined(String name)
    {
        return switch (name)
        {
            case "lt" -> "<";
            case "gt" -> ">";
            case "amp" -> "&";
            case "apos" -> "'";
            case "quot" -> "\"";
            default -> null;
        };
    }

    void undeclared_entity(String name) throws XmlParseException
    {
        XmlDtd dtd = doc_.intSubset();
        if (dtd == null || doc_.standalone() == 1 || (!has_external_subset_ && !has_pe_refs_))
        {
            throw fatal("Entity '" + name + "' not defined");
        }
        warning("Entity '" + name + "' not defined");
    }

    /** Parse &amp;#123; or &amp;#x1F; and return the code point. */
    int parse_char_ref() throws XmlParseException
    {
        i += 2;
        int val = 0;
        boolean ok = false;
        if (cur() == 'x')
        {
            i++;
            while (!eof() && cur() != ';')
            {
                int d = Character.digit(cur(), 16);
                if (d < 0)
                {
                    throw fatal("xmlParseCharRef: invalid hexadecimal value");
                }
                val = val * 16 + d;
                if (val > 0x110000) val = 0x110000;
                ok = true;
                i++;
            }
        }
        else
        {
            while (!eof() && cur() != ';')
            {
                char c = cur();
                if (c < '0' || c > '9')
                {
                    throw fatal("xmlParseCharRef: invalid decimal value");
                }
                val = val * 10 + (c - '0');
                if (val > 0x110000) val = 0x110000;
                ok = true;
                i++;
            }
        }
        if (cur() != ';' || !ok)
        {
            throw fatal("xmlParseCharRef: invalid decimal value");
        }
        i++;
        if (!is_xml_char(val))
        {
            throw fatal("xmlParseCharRef: invalid xmlChar value " + val);
        }
        return val;
    }

    // ---------------------------------------------------------------- content

    /** Add character data to the current node, merging with a previous text node. */
    void characters(String t)
    {
        if (t.isEmpty()) return;
        node_.addChild(new XmlText(t));
    }

    void parse_reference() throws XmlParseException
    {
        if (nxt(1) == '#')
        {
            int start = i;
            int cp = parse_char_ref();
            characters(new String(Character.toChars(cp)));
            return;
        }
        i++;
        String name = parse_name();
        if (name == null)
        {
            throw fatal("xmlParseEntityRef: no name");
        }
        if (cur() != ';')
        {
            throw fatal("EntityRef: expecting ';'");
        }
        i++;
        String pre = predefined(name);
        if (pre != null)
        {
            characters(pre);
            return;
        }
        XmlEntityDecl ent = doc_.getEntity(name);
        if (ent == null)
        {
            undeclared_entity(name);
        }
        else if (ent.etype() == XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED)
        {
            throw fatal("Entity reference to unparsed entity " + name);
        }
        node_.appendRaw(new XmlEntityRef(name));
    }

    void parse_char_data() throws XmlParseException
    {
        int start = i;
        boolean all_blank = true;
        while (i < s.length())
        {
            char c = s.charAt(i);
            if (c == '<' || c == '&') break;
            if (c == ']' && s.startsWith("]]>", i))
            {
                throw fatal("Sequence ']]>' not allowed in content");
            }
            if (!is_blank(c)) all_blank = false;
            i++;
        }
        String t = s.substring(start, i);
        if (all_blank && are_blanks())
        {
            return;
        }
        characters(t);
        if (!all_blank && !space_.isEmpty() && space_.get(space_.size() - 1) == -1)
        {
            // Like libxml2, an element with non blank character data is mixed
            // content, after this no blanks are ignored in this element.
            space_.set(space_.size() - 1, -2);
        }
    }

    /** Port of the libxml2 areBlanks heuristic, called when a run of blanks has been read. */
    boolean are_blanks()
    {
        if ((options_ & NOBLANKS) == 0) return false;
        int space = space_.isEmpty() ? -1 : space_.get(space_.size() - 1);
        if (space == 1 || space == -2) return false;
        if (node_ == null || node_ == doc_) return false;
        // xmlIsMixedElement: only if the dtd declares the element.
        XmlDtd dtd = doc_.intSubset();
        if (dtd != null)
        {
            for (XmlNode d = dtd.firstChild(); d != null; d = d.next())
            {
                if (d.type() == XmlNodeType.ELEMENT_DECL && d.name().equals(((XmlElement)node_).fullName()))
                {
                    String text = ((XmlDtdDecl)d).text();
                    boolean mixed = text.contains("#PCDATA") || text.contains(" ANY>");
                    return !mixed;
                }
            }
        }
        char raw = cur();
        if (raw != '<' && raw != 0xD) return false;
        if (node_.firstChild() == null && raw == '<' && nxt(1) == '/') return false;
        XmlNode last = node_.lastChild();
        if (last == null)
        {
            // The node is an element, the libxml2 check on content does not apply.
        }
        else if (last.type() == XmlNodeType.TEXT)
        {
            return false;
        }
        else if (node_.firstChild() != null && node_.firstChild().type() == XmlNodeType.TEXT)
        {
            return false;
        }
        return true;
    }

    void parse_comment() throws XmlParseException
    {
        int start = i;
        i += 4;
        int content_start = i;
        while (true)
        {
            if (eof())
            {
                String head = s.substring(content_start, Math.min(s.length(), content_start + 50));
                throw fatal("Comment not terminated \n<!--" + head, start);
            }
            if (cur() == '-' && nxt(1) == '-')
            {
                if (nxt(2) == '>')
                {
                    break;
                }
                String head = s.substring(content_start, Math.min(i, content_start + 50));
                throw fatal("Double hyphen within comment: <!--" + head, i);
            }
            i++;
        }
        String content = s.substring(content_start, i);
        i += 3;
        XmlComment c = new XmlComment(content);
        if (in_subset_) doc_.intSubset().appendRaw(c);
        else node_.appendRaw(c);
    }

    void parse_pi() throws XmlParseException
    {
        int start = i;
        i += 2;
        String target = parse_name();
        if (target == null)
        {
            throw fatal("xmlParsePI : no target name");
        }
        if (target.equalsIgnoreCase("xml"))
        {
            if (target.equals("xml"))
            {
                throw fatal("XML declaration allowed only at the start of the document");
            }
            throw fatal("xmlParsePITarget: invalid name prefix 'xml'");
        }
        String data = null;
        if (at("?>"))
        {
            i += 2;
        }
        else
        {
            if (!skip_blanks())
            {
                throw fatal("ParsePI: PI " + target + " space expected");
            }
            int end = s.indexOf("?>", i);
            if (end < 0)
            {
                throw fatal("ParsePI: PI " + target + " never end ...", start);
            }
            data = s.substring(i, end);
            i = end + 2;
        }
        XmlPI pi = new XmlPI(target, data);
        if (in_subset_) doc_.intSubset().appendRaw(pi);
        else node_.appendRaw(pi);
    }

    void parse_cdata() throws XmlParseException
    {
        int start = i;
        i += 9;
        int end = s.indexOf("]]>", i);
        if (end < 0)
        {
            String head = s.substring(i, Math.min(s.length(), i + 50));
            throw fatal("CData section not finished\n" + head, start);
        }
        String content = s.substring(i, end);
        i = end + 3;
        if ((options_ & NOCDATA) != 0)
        {
            // Like libxml2, even an empty cdata section creates a (empty) text node.
            node_.addChild(new XmlText(content));
        }
        else
        {
            node_.appendRaw(new XmlCData(content));
        }
    }

    // ---------------------------------------------------------------- doctype

    void parse_doctype_decl() throws XmlParseException
    {
        i += 9;
        skip_blanks();
        String name = parse_name();
        if (name == null)
        {
            throw fatal("xmlParseDocTypeDecl : no DOCTYPE name !");
        }
        skip_blanks();
        String[] ids = parse_external_id(false);
        skip_blanks();
        XmlDtd dtd = new XmlDtd(name, ids[0], ids[1]);
        if (ids[0] != null || ids[1] != null) has_external_subset_ = true;
        doc_.setIntSubset(dtd);
        if (cur() == '[')
        {
            i++;
            in_subset_ = true;
            parse_internal_subset(dtd);
            in_subset_ = false;
            skip_blanks();
        }
        if (cur() != '>')
        {
            throw fatal("DOCTYPE improperly terminated");
        }
        i++;
    }

    /** Parse SYSTEM "x" or PUBLIC "p" "x", returns {publicId, systemId}. */
    String[] parse_external_id(boolean allow_public_only) throws XmlParseException
    {
        String pub = null;
        String sys = null;
        if (at("SYSTEM"))
        {
            i += 6;
            if (!skip_blanks()) throw fatal("Space required after 'SYSTEM'");
            sys = parse_quoted("SystemLiteral");
        }
        else if (at("PUBLIC"))
        {
            i += 6;
            if (!skip_blanks()) throw fatal("Space required after 'PUBLIC'");
            pub = parse_quoted("PubidLiteral");
            int save = i;
            boolean blank = skip_blanks();
            if (cur() == '"' || cur() == '\'')
            {
                if (!blank) throw fatal("Space required after the Public Identifier");
                sys = parse_quoted("SystemLiteral");
            }
            else
            {
                i = save;
                if (!allow_public_only) throw fatal("Space required after the Public Identifier");
            }
        }
        return new String[] { pub, sys };
    }

    String parse_quoted(String what) throws XmlParseException
    {
        char q = cur();
        if (q != '"' && q != '\'')
        {
            throw fatal(what + " \" or ' expected");
        }
        int end = s.indexOf(q, i + 1);
        if (end < 0)
        {
            throw fatal("Unfinished " + what);
        }
        String v = s.substring(i + 1, end);
        i = end + 1;
        return v;
    }

    void parse_internal_subset(XmlDtd dtd) throws XmlParseException
    {
        while (true)
        {
            skip_blanks();
            if (eof())
            {
                throw fatal("xmlParseInternalSubset: error detected in Markup declaration");
            }
            if (cur() == ']')
            {
                i++;
                return;
            }
            if (at("<!ENTITY"))
            {
                parse_entity_decl(dtd);
            }
            else if (at("<!ELEMENT"))
            {
                parse_element_decl(dtd);
            }
            else if (at("<!ATTLIST"))
            {
                parse_attlist_decl(dtd);
            }
            else if (at("<!NOTATION"))
            {
                parse_notation_decl(dtd);
            }
            else if (at("<!--"))
            {
                parse_comment();
            }
            else if (at("<?"))
            {
                parse_pi();
            }
            else if (cur() == '%')
            {
                i++;
                String n = parse_name();
                if (n == null || cur() != ';')
                {
                    throw fatal("PEReference: no name");
                }
                i++;
                has_pe_refs_ = true;
            }
            else
            {
                throw fatal("xmlParseInternalSubset: error detected in Markup declaration");
            }
        }
    }

    void expect_decl_end(String what) throws XmlParseException
    {
        skip_blanks();
        if (cur() != '>')
        {
            throw fatal(what);
        }
        i++;
    }

    void parse_entity_decl(XmlDtd dtd) throws XmlParseException
    {
        i += 8;
        if (!skip_blanks()) throw fatal("Space required after '<!ENTITY'");
        boolean pe = false;
        if (cur() == '%')
        {
            i++;
            if (!skip_blanks()) throw fatal("Space required after '%'");
            pe = true;
        }
        String name = parse_name();
        if (name == null)
        {
            throw fatal("xmlParseEntityDecl: no name");
        }
        if (!skip_blanks()) throw fatal("Space required after the entity name");
        XmlEntityDecl e;
        if (cur() == '"' || cur() == '\'')
        {
            char q = cur();
            int end = s.indexOf(q, i + 1);
            if (end < 0) throw fatal("EntityValue: \" expected");
            String orig = s.substring(i + 1, end);
            i = end + 1;
            String content = decode_entity_value(orig, dtd);
            e = new XmlEntityDecl(name, pe ? XmlEntityDecl.INTERNAL_PARAMETER : XmlEntityDecl.INTERNAL_GENERAL,
                                  null, null, content, orig);
        }
        else
        {
            String[] ids = parse_external_id(false);
            if (ids[0] == null && ids[1] == null)
            {
                throw fatal("Entity value required");
            }
            String ndata = null;
            if (!pe)
            {
                int save = i;
                boolean blank = skip_blanks();
                if (at("NDATA"))
                {
                    if (!blank) throw fatal("Space required before 'NDATA'");
                    i += 5;
                    skip_blanks();
                    ndata = parse_name();
                }
                else
                {
                    i = save;
                }
            }
            int type = pe ? XmlEntityDecl.EXTERNAL_PARAMETER
                : (ndata != null ? XmlEntityDecl.EXTERNAL_GENERAL_UNPARSED : XmlEntityDecl.EXTERNAL_GENERAL_PARSED);
            e = new XmlEntityDecl(name, type, ids[0], ids[1], ndata, null);
        }
        expect_decl_end("xmlParseEntityDecl: entity " + name + " not terminated");
        if (!pe && predefined(name) != null)
        {
            // Redefinition of a predefined entity is ignored.
            return;
        }
        dtd.addEntity(e);
    }

    /** Expand character references (and parameter entity references) in an entity value. */
    String decode_entity_value(String v, XmlDtd dtd) throws XmlParseException
    {
        StringBuilder out = new StringBuilder();
        int k = 0;
        while (k < v.length())
        {
            char c = v.charAt(k);
            if (c == '&' && k + 1 < v.length() && v.charAt(k + 1) == '#')
            {
                int end = v.indexOf(';', k);
                if (end < 0) throw fatal("xmlParseCharRef: invalid decimal value");
                String num = v.substring(k + 2, end);
                int cp;
                try
                {
                    cp = num.startsWith("x") ? Integer.parseInt(num.substring(1), 16) : Integer.parseInt(num);
                }
                catch (NumberFormatException ex)
                {
                    throw fatal("xmlParseCharRef: invalid decimal value");
                }
                out.appendCodePoint(cp);
                k = end + 1;
                continue;
            }
            if (c == '%')
            {
                int end = v.indexOf(';', k);
                if (end > k)
                {
                    XmlEntityDecl pe = dtd.getParameterEntity(v.substring(k + 1, end));
                    if (pe != null && pe.content() != null)
                    {
                        out.append(pe.content());
                        k = end + 1;
                        continue;
                    }
                }
            }
            out.append(c);
            k++;
        }
        return out.toString();
    }

    void parse_element_decl(XmlDtd dtd) throws XmlParseException
    {
        i += 9;
        if (!skip_blanks()) throw fatal("Space required after 'ELEMENT'");
        String name = parse_name();
        if (name == null) throw fatal("xmlParseElementDecl: no name for Element");
        if (!skip_blanks()) throw fatal("Space required after the element name");
        String spec;
        if (at("EMPTY"))
        {
            i += 5;
            spec = "EMPTY";
        }
        else if (at("ANY"))
        {
            i += 3;
            spec = "ANY";
        }
        else if (cur() == '(')
        {
            spec = new ContentModel(this).parse();
        }
        else
        {
            throw fatal("xmlParseElementDecl: 'EMPTY', 'ANY' or '(' expected");
        }
        expect_decl_end("expected '>' at the end of the element declaration");
        for (XmlNode d = dtd.firstChild(); d != null; d = d.next())
        {
            if (d.type() == XmlNodeType.ELEMENT_DECL && d.name().equals(name)) return;
        }
        dtd.addDecl(new XmlDtdDecl(XmlNodeType.ELEMENT_DECL, name, "<!ELEMENT " + name + " " + spec + ">\n"));
    }

    /** Parses a content model and prints it like libxml2 xmlDumpElementContent. */
    static class ContentModel
    {
        final XmlParser p;
        ContentModel(XmlParser p)
        {
            this.p = p;
        }

        // A node is either a name/#PCDATA leaf or a group.
        static class Node
        {
            String name;            // Leaf name.
            char op;                // ',' or '|' for a group with more than one child, 0 otherwise.
            List<Node> kids = new ArrayList<>();
            char occur;             // 0, '?', '*', '+'
        }

        String parse() throws XmlParseException
        {
            Node n = group();
            StringBuilder sb = new StringBuilder();
            print(sb, n, true);
            return sb.toString();
        }

        Node group() throws XmlParseException
        {
            p.i++; // (
            Node g = new Node();
            while (true)
            {
                p.skip_blanks();
                Node item;
                if (p.cur() == '(')
                {
                    item = group();
                }
                else if (p.at("#PCDATA"))
                {
                    p.i += 7;
                    item = new Node();
                    item.name = "#PCDATA";
                }
                else
                {
                    String n = p.parse_name();
                    if (n == null) throw p.fatal("xmlParseElementChildrenContentDecl : Name or '(' expected");
                    item = new Node();
                    item.name = n;
                    item.occur = occur();
                }
                g.kids.add(item);
                p.skip_blanks();
                char c = p.cur();
                if (c == ')')
                {
                    p.i++;
                    g.occur = occur();
                    break;
                }
                if (c == ',' || c == '|')
                {
                    if (g.op != 0 && g.op != c) throw p.fatal("xmlParseElementChildrenContentDecl : '" + g.op + "' expected");
                    g.op = c;
                    p.i++;
                    continue;
                }
                throw p.fatal("xmlParseElementChildrenContentDecl : ',' '|' or ')' expected");
            }
            return g;
        }

        char occur()
        {
            char c = p.cur();
            if (c == '?' || c == '*' || c == '+')
            {
                p.i++;
                return c;
            }
            return 0;
        }

        void print(StringBuilder sb, Node n, boolean top)
        {
            if (n.name != null)
            {
                sb.append(n.name);
                if (n.occur != 0) sb.append(n.occur);
                return;
            }
            if (n.kids.size() == 1 && n.kids.get(0).name != null)
            {
                // A single item group, eg (a) or (#PCDATA).
                Node k = n.kids.get(0);
                sb.append('(').append(k.name);
                if (k.occur != 0) sb.append(k.occur);
                sb.append(')');
                if (n.occur != 0) sb.append(n.occur);
                return;
            }
            sb.append('(');
            print_items(sb, n);
            sb.append(')');
            if (n.occur != 0) sb.append(n.occur);
        }

        void print_items(StringBuilder sb, Node n)
        {
            String sep = n.op == ',' ? " , " : " | ";
            boolean first = true;
            for (Node k : n.kids)
            {
                if (!first) sb.append(sep);
                first = false;
                if (k.name == null && k.op == n.op && k.occur == 0 && k.kids.size() > 1)
                {
                    // Nested group of the same type without occurrence, libxml2 prints it flat.
                    print_items(sb, k);
                }
                else
                {
                    print(sb, k, false);
                }
            }
        }
    }

    void parse_attlist_decl(XmlDtd dtd) throws XmlParseException
    {
        i += 9;
        if (!skip_blanks()) throw fatal("Space required after '<!ATTLIST'");
        String elem = parse_name();
        if (elem == null) throw fatal("ATTLIST: no name for Element");
        while (true)
        {
            skip_blanks();
            if (cur() == '>')
            {
                i++;
                return;
            }
            String attr = parse_name();
            if (attr == null) throw fatal("ATTLIST: no name for Attribute");
            if (!skip_blanks()) throw fatal("Space required after the attribute name");
            StringBuilder decl = new StringBuilder("<!ATTLIST ").append(elem).append(' ').append(attr);
            if (cur() == '(')
            {
                decl.append(" (").append(enumeration()).append(')');
            }
            else
            {
                String type = parse_name();
                if (type == null) throw fatal("attribute type expected");
                if (type.equals("NOTATION"))
                {
                    skip_blanks();
                    decl.append(" NOTATION (").append(enumeration()).append(')');
                }
                else
                {
                    decl.append(' ').append(type);
                }
            }
            if (!skip_blanks()) throw fatal("Space required after the attribute type");
            String default_value = null;
            if (at("#REQUIRED"))
            {
                i += 9;
                decl.append(" #REQUIRED");
            }
            else if (at("#IMPLIED"))
            {
                i += 8;
                decl.append(" #IMPLIED");
            }
            else
            {
                if (at("#FIXED"))
                {
                    i += 6;
                    decl.append(" #FIXED");
                    if (!skip_blanks()) throw fatal("Space required after '#FIXED'");
                }
                List<Object> v = new ArrayList<>();
                parse_att_value(v);
                default_value = value_to_string(v);
            }
            if (default_value != null)
            {
                decl.append(' ');
                XmlSave.write_quoted(decl, default_value);
            }
            decl.append(">\n");
            boolean dup = false;
            for (XmlNode d = dtd.firstChild(); d != null; d = d.next())
            {
                if (d.type() == XmlNodeType.ATTRIBUTE_DECL && d.name().equals(attr)
                    && ((XmlDtdDecl)d).text().startsWith("<!ATTLIST " + elem + " " + attr + " "))
                {
                    dup = true;
                }
            }
            if (!dup)
            {
                dtd.addDecl(new XmlDtdDecl(XmlNodeType.ATTRIBUTE_DECL, attr, decl.toString()));
                if (default_value != null)
                {
                    List<String[]> l = att_defaults_.computeIfAbsent(elem, k -> new ArrayList<>());
                    l.add(new String[] { attr, default_value });
                }
            }
        }
    }

    /** Parse (a|b|c) and return "a | b | c". */
    String enumeration() throws XmlParseException
    {
        if (cur() != '(') throw fatal("'(' required to start ATTLIST enumeration");
        i++;
        StringBuilder sb = new StringBuilder();
        while (true)
        {
            skip_blanks();
            String n = parse_nmtoken();
            if (n == null) throw fatal("NmToken expected in ATTLIST enumeration");
            if (sb.length() > 0) sb.append(" | ");
            sb.append(n);
            skip_blanks();
            if (cur() == ')')
            {
                i++;
                return sb.toString();
            }
            if (cur() != '|') throw fatal("')' required to finish ATTLIST enumeration");
            i++;
        }
    }

    void parse_notation_decl(XmlDtd dtd) throws XmlParseException
    {
        i += 10;
        if (!skip_blanks()) throw fatal("Space required after '<!NOTATION'");
        String name = parse_name();
        if (name == null) throw fatal("Name expected in NOTATION declaration");
        if (!skip_blanks()) throw fatal("Space required after the NOTATION name'");
        String[] ids = parse_external_id(true);
        expect_decl_end("'>' required to close NOTATION declaration");
        StringBuilder sb = new StringBuilder("<!NOTATION ").append(name);
        if (ids[0] != null)
        {
            sb.append(" PUBLIC ");
            XmlSave.write_quoted(sb, ids[0]);
            if (ids[1] != null)
            {
                sb.append(' ');
                XmlSave.write_quoted(sb, ids[1]);
            }
        }
        else
        {
            sb.append(" SYSTEM ");
            XmlSave.write_quoted(sb, ids[1]);
        }
        sb.append(" >\n");
        dtd.addNotation(new XmlDtdDecl(XmlNodeType.NOTATION_DECL, name, sb.toString()));
    }
}
