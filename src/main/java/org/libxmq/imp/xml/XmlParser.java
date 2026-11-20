/* This file is ported from libxml2 (parser.c and SAX2.c).
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
    /** Add the attributes defaulted in the dtd, like XML_PARSE_DTDATTR. */
    public static final int DTDATTR = 4;

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
    // Declared attribute types, "element\u0001attr" -> true for CDATA, false for other types
    // whose values are normalized (first declaration wins, libxml2 attsSpecial).
    final java.util.Map<String, Boolean> attr_cdata_ = new java.util.HashMap<>();

    // The UTF-8 bytes of s, for error messages whose positions are in bytes like the C version.
    byte[] u8_;

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
        return parse(content.getBytes(java.nio.charset.StandardCharsets.UTF_8), source, options);
    }

    /**
     * Parse xml bytes into a document, the bytes are UTF-8, UTF-16 (bom or "&lt;?"
     * pattern) or ISO-8859-1 (declared), like the C version.
     * @param content The xml source.
     * @param source The source name used in error messages, eg the file name.
     * @param options NOBLANKS and/or NOCDATA.
     * @return The document.
     * @throws XmlParseException If the xml is not well formed.
     */
    public static XmlDocument parse(byte[] content, String source, int options) throws XmlParseException
    {
        return parse(content, source, options, null);
    }

    /**
     * Parse xml bytes into a document, the warnings and recoverable errors (eg namespace
     * errors) go to messages instead of stderr, like the message callback of the C parser.
     * @param content The xml source.
     * @param source The source name used in error messages, eg the file name.
     * @param options NOBLANKS, NOCDATA and/or DTDATTR.
     * @param messages Receives the formatted messages, null for stderr.
     * @return The document.
     * @throws XmlParseException If the xml is not well formed.
     */
    public static XmlDocument parse(byte[] content, String source, int options,
                                    java.util.function.Consumer<String> messages) throws XmlParseException
    {
        int[] info = new int[3];
        byte[] u8 = XmlInput.prepare(source == null ? "noname.xml" : source, content, info);
        XmlParser p = new XmlParser(new String(u8, java.nio.charset.StandardCharsets.UTF_8), u8, source, options);
        p.messages_ = messages;
        if (info[0] >= 0)
        {
            p.has_bad_ = true;
            p.bad_pos_ = new String(u8, 0, info[0], java.nio.charset.StandardCharsets.UTF_8).length();
            p.bad_cp_ = info[1];
        }
        p.io_error_at_end_ = info[2] != 0;
        return p.parse_document();
    }

    /** Where the messages go, null for stderr. */
    java.util.function.Consumer<String> messages_;

    /** A parser of text used as it is, without end of line normalization. */
    static XmlParser prepared(String content, String source, int options)
    {
        return new XmlParser(content, null, source, options);
    }

    /** A parser of prepared input: valid UTF-8 with normalized line ends. */
    private XmlParser(String prepared, byte[] u8, String source, int options)
    {
        s = prepared;
        u8_ = u8;
        source_ = source == null ? "noname.xml" : source;
        options_ = options;
    }

    // ---------------------------------------------------------------- errors

    // Cache for line_at, like the C version, so that line numbers are not quadratic.
    int line_pos_;
    int line_no_ = 1;

    int line_at(int pos)
    {
        int end = Math.min(pos, s.length());
        if (end < line_pos_)
        {
            line_pos_ = 0;
            line_no_ = 1;
        }
        for (int k = line_pos_; k < end; k++)
        {
            if (s.charAt(k) == '\n') line_no_++;
        }
        line_pos_ = end;
        return line_no_;
    }

    /** The byte offset in u8_ of the char index pos in s. */
    int byte_pos(int pos)
    {
        if (u8_ == null) u8_ = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        int end = Math.min(pos, s.length());
        int n = 0;
        for (int k = 0; k < end; k++)
        {
            char c = s.charAt(k);
            if (c < 0x80) n++;
            else if (c < 0x800) n += 2;
            else if (Character.isHighSurrogate(c) && k + 1 < end) { n += 4; k++; }
            else n += 3;
        }
        return n;
    }

    /** Format a message with source context like libxml2 xmlParserPrintFileContextInternal, as bytes. */
    byte[] format_error(String kind, String msg, int pos, int[] out_line)
    {
        int bpos = byte_pos(pos);
        return XmlInput.format(source_, kind, msg, u8_, u8_.length, bpos, out_line);
    }

    /** Print a non fatal message, like the parse message callback of the C version. */
    void message(String kind, String msg, int pos)
    {
        byte[] m = format_error(kind, msg, pos, null);
        if (messages_ != null)
        {
            messages_.accept(new String(m, java.nio.charset.StandardCharsets.UTF_8));
            return;
        }
        System.err.write(m, 0, m.length);
        System.err.flush();
    }

    XmlParseException fatal(String msg)
    {
        return fatal(msg, i);
    }

    XmlParseException fatal(String msg, int pos)
    {
        return fatal("parser error", msg, pos);
    }

    XmlParseException fatal(String kind, String msg, int pos)
    {
        if (has_bad_ && pos > bad_pos_)
        {
            // The invalid character was passed by a construct that does not check for it.
            has_bad_ = false;
            kind = "parser error";
            msg = bad_message();
            pos = bad_pos_;
        }
        int[] line = new int[1];
        byte[] full = format_error(kind, msg, pos, line);
        return new XmlParseException(full, msg, line[0]);
    }

    // The first character of the input that is not an xml char. Like libxml2 the parser
    // reports it where it finds it, in the words of the construct it is in.
    boolean has_bad_;
    int bad_pos_;
    int bad_cp_;
    // The input ends with an incomplete UTF-16 code unit.
    boolean io_error_at_end_;

    String bad_message()
    {
        return "Char 0x" + Integer.toHexString(bad_cp_).toUpperCase(java.util.Locale.ROOT) + " out of allowed range";
    }

    void warning(String msg)
    {
        message("parser error", msg, i);
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

    /** Do not report malformed QNames (end tags report them themselves). */
    boolean qname_quiet_;
    /** The last QName parsed was malformed. */
    boolean qname_malformed_;

    void qname_malformed(String name)
    {
        // Like libxml2 xmlParseQNameHashed, reported where the name ends.
        qname_malformed_ = true;
        if (!qname_quiet_) message("namespace error", "Failed to parse QName '" + name + "'", i);
    }

    /** Parse a QName, returns {prefix, local} where prefix can be null, or null if no name. */
    String[] parse_qname()
    {
        int start = i;
        qname_malformed_ = false;
        String l = parse_ncname();
        if (l == null)
        {
            // Names starting with a colon are accepted by libxml2 as plain names.
            String n = parse_name();
            if (n == null) return null;
            qname_malformed(n);
            return new String[] { null, n };
        }
        if (cur() == ':')
        {
            i++;
            String l2 = parse_ncname();
            if (l2 == null || cur() == ':')
            {
                // Not a valid QName, treat the whole thing as a name.
                i = start;
                String n = parse_name();
                qname_malformed(n);
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
        if (has_bad_) throw fatal(bad_message(), bad_pos_);
        if (io_error_at_end_) throw fatal("I/O error", "Invalid bytes in character encoding", s.length());
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
        boolean prefixed; // The name has a namespace prefix.
    }

    void parse_element_tree() throws XmlParseException
    {
        List<Open> stack = new ArrayList<>();
        open_depth_ = 0;
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
                    open_depth_ = stack.size();
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

    /**
     * Parse content (text, references, elements, comments, pis, cdata) into
     * parent, used for the replacement text of an entity, see XmlEntityExpander.
     * Namespaces are resolved where parent is in the tree.
     */
    void parse_content_chunk(XmlDocument doc, XmlNode parent) throws XmlParseException
    {
        doc_ = doc;
        node_ = parent;
        XmlDtd dtd = doc.intSubset();
        has_external_subset_ = dtd != null && (dtd.systemId() != null || dtd.externalId() != null);
        has_pe_refs_ = dtd != null && dtd.has_pe_refs_;
        List<Open> stack = new ArrayList<>();
        while (!eof())
        {
            char c = cur();
            if (c == '<')
            {
                char n = nxt(1);
                if (n == '/')
                {
                    if (stack.isEmpty()) throw fatal("chunk is not well balanced");
                    Open top = stack.get(stack.size() - 1);
                    parse_end_tag(top);
                    stack.remove(stack.size() - 1);
                    node_ = stack.isEmpty() ? parent : stack.get(stack.size() - 1).e;
                    if (!space_.isEmpty()) space_.remove(space_.size() - 1);
                }
                else if (n == '?') parse_pi();
                else if (at("<!--")) parse_comment();
                else if (at("<![CDATA[")) parse_cdata();
                else if (n == '!') throw fatal("StartTag: invalid element name");
                else
                {
                    open_depth_ = stack.size();
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
        if (!stack.isEmpty())
        {
            Open top = stack.get(stack.size() - 1);
            throw fatal("Premature end of data in tag " + top.qname + " line " + top.line);
        }
    }

    static class RawAttr
    {
        String prefix;
        String local;
        List<Object> value = new ArrayList<>(); // String or EntityName
        int pos;
        int end; // After the value, where libxml2 reports problems with the value.
        boolean ns_skip; // A namespace declaration that is not kept (an error was reported).
        String qname()
        {
            return prefix == null ? local : prefix + ":" + local;
        }
    }

    static final String XML_NS_URI = "http://www.w3.org/XML/1998/namespace";
    static final String XMLNS_NS_URI = "http://www.w3.org/2000/xmlns/";

    /** The checks of a namespace declaration of libxml2 xmlParseStartTag2, reported right
        after the attribute value. Returns false if the declaration is not kept. */
    boolean check_ns_decl(RawAttr ra)
    {
        String pfx;
        if (ra.prefix == null && ra.local.equals("xmlns")) pfx = null;
        else if ("xmlns".equals(ra.prefix)) pfx = ra.local;
        else return true;
        for (Object o : ra.value) if (o instanceof EntityName) return true; // Rejected later.
        String href = value_to_string(ra.value);
        if (pfx == null)
        {
            if (!href.isEmpty())
            {
                XmlUri[] uri = new XmlUri[1];
                XmlUri.parseSafe(href, uri);
                if (uri[0] == null) message("namespace error", "xmlns: '" + href + "' is not a valid URI", ra.end);
                else if (uri[0].scheme == null) message("namespace warning", "xmlns: URI " + href + " is not absolute", ra.end);
                if (href.equals(XML_NS_URI))
                {
                    message("namespace error", "xml namespace URI cannot be the default namespace", ra.end);
                    return false;
                }
                if (href.equals(XMLNS_NS_URI))
                {
                    message("namespace error", "reuse of the xmlns namespace name is forbidden", ra.end);
                    return false;
                }
            }
            return true;
        }
        if (pfx.equals("xml"))
        {
            if (!href.equals(XML_NS_URI)) message("namespace error", "xml namespace prefix mapped to wrong URI", ra.end);
            return false;
        }
        if (href.equals(XML_NS_URI))
        {
            message("namespace error", "xml namespace URI mapped to wrong prefix", ra.end);
            return false;
        }
        if (pfx.equals("xmlns"))
        {
            message("namespace error", "redefinition of the xmlns prefix is forbidden", ra.end);
            return false;
        }
        if (href.equals(XMLNS_NS_URI))
        {
            message("namespace error", "reuse of the xmlns namespace name is forbidden", ra.end);
            return false;
        }
        if (href.isEmpty())
        {
            message("namespace error", "xmlns:" + pfx + ": Empty XML namespace is not allowed", ra.end);
            return false;
        }
        XmlUri[] uri = new XmlUri[1];
        XmlUri.parseSafe(href, uri);
        if (uri[0] == null) message("namespace error", "xmlns:" + pfx + ": '" + href + "' is not a valid URI", ra.end);
        return true;
    }

    record EntityName(String name) { }

    /** At most this many nested elements, like the max_depth of xml_parser.c (libxml2 has 256). */
    static final int MAX_DEPTH = 10000;

    /** The number of open elements when a start tag is parsed. */
    int open_depth_;

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
        if (open_depth_ >= MAX_DEPTH)
        {
            throw fatal("Excessive depth in document: " + MAX_DEPTH, tag_start);
        }
        String qname = qn[0] == null ? qn[1] : qn[0] + ":" + qn[1];
        List<RawAttr> attrs = new ArrayList<>();
        boolean empty = false;
        // The end of the start tag (the > or />), where libxml2 2.16 reports duplicate
        // attributes and undefined prefixes, since it checks them after reading all attributes.
        int tag_end = i;
        while (true)
        {
            boolean blank = skip_blanks();
            if (eof())
            {
                throw fatal("Couldn't find end of Start Tag " + qname + " line " + tag_line);
            }
            if (cur() == '>')
            {
                tag_end = i;
                i++;
                break;
            }
            if (at("/>"))
            {
                tag_end = i;
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
            ra.end = i;
            ra.ns_skip = !check_ns_decl(ra);
            attrs.add(ra);
        }

        // Duplicate attribute names as written, reported after the namespaced duplicates
        // like libxml2 does.
        RawAttr raw_dup = null;
        for (int k = 0; k < attrs.size() && raw_dup == null; k++)
        {
            RawAttr ra = attrs.get(k);
            for (int j = 0; j < k; j++)
            {
                RawAttr prev = attrs.get(j);
                if (prev.local.equals(ra.local) && java.util.Objects.equals(prev.prefix, ra.prefix))
                {
                    raw_dup = ra;
                    break;
                }
            }
        }
        if (raw_dup != null && ((raw_dup.prefix == null && raw_dup.local.equals("xmlns")) || "xmlns".equals(raw_dup.prefix)))
        {
            throw fatal("Attribute " + raw_dup.qname() + " redefined", tag_end);
        }

        // Values of attributes declared with a type other than CDATA are normalized.
        if (!attr_cdata_.isEmpty())
        {
            for (RawAttr ra : attrs)
            {
                String an = ra.prefix == null ? ra.local : ra.prefix + ":" + ra.local;
                if (Boolean.FALSE.equals(attr_cdata_.get(qname + "\u0001" + an)))
                {
                    ra.value = normalize_parts(ra.value);
                }
            }
        }

        XmlElement e = new XmlElement(qn[1]);
        // Like libxml2 the element gets the line where its start tag ends.
        e.setLine(line_at(tag_end));

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
            for (Object o : ra.value)
            {
                if (o instanceof EntityName)
                {
                    // Entities are never expanded implicitly and a namespace uri is a plain string,
                    // so a reference cannot be kept. Rejected like the C version.
                    throw fatal("Entity references in namespace uris are not supported (xmlns"
                                + (prefix == null ? "" : ":" + prefix) + ")", ra.end);
                }
            }
            String href = value_to_string(ra.value);
            String key = prefix == null ? "" : prefix;
            if (declared.contains(key))
            {
                throw fatal("Attribute xmlns" + (prefix == null ? "" : ":" + prefix) + " redefined", tag_end);
            }
            declared.add(key);
            if (ra.ns_skip) continue;
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
                if ("xml".equals(prefix)) continue;
                String value = d[1];
                declared.add(key);
                e.nsDefs().add(new XmlNs(prefix, value));
            }
        }

        // Attach the element so that namespace lookups see the ancestors.
        node_.appendRaw(e);

        // Resolve the element namespace.
        XmlNs ens = e.searchNs(qn[0]);
        boolean eprefix_undefined = qn[0] != null && ens == null;
        if (eprefix_undefined)
        {
            // Reported after the attributes, like libxml2.
            e.setName(qn[0] + ":" + qn[1]);
        }
        else if (ens != null && !(qn[0] == null && ens.href().isEmpty()))
        {
            e.setNs(ens);
        }

        // Then the attributes.
        java.util.Map<String, RawAttr> seen = new java.util.HashMap<>();
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
                    message("namespace error", "Namespace prefix " + ra.prefix + " for " + ra.local + " on " + qn[1] + " is not defined", tag_end);
                    a.setName(ra.prefix + ":" + ra.local);
                }
                else
                {
                    a.setNs(ans);
                    uri = ans.href();
                }
            }
            // The first earlier attribute with the same expanded name, like the attribute hash of
            // libxml2 xmlParseStartTag2. The same prefix is a duplicate, another prefix
            // bound to the same uri a recoverable namespace error, both attributes are kept.
            String key = (uri == null ? "" : uri) + "}" + a.name();
            RawAttr prev = seen.get(key);
            if (prev != null)
            {
                if (java.util.Objects.equals(prev.prefix, ra.prefix))
                {
                    throw fatal("Attribute " + ra.qname() + " redefined", tag_end);
                }
                message("namespace error", "Namespaced Attribute " + ra.local + " in '" + uri + "' redefined", tag_end);
            }
            else
            {
                seen.put(key, ra);
            }
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

        if (raw_dup != null) throw fatal("Attribute " + raw_dup.qname() + " redefined", tag_end);

        // The attributes defaulted from the dtd (libxml2 XML_PARSE_DTDATTR).
        if ((options_ & DTDATTR) != 0 && defaults != null)
        {
            for (String[] d : defaults)
            {
                if (d[0].equals("xmlns") || d[0].startsWith("xmlns:")) continue;
                int colon = d[0].indexOf(':');
                String local = colon >= 0 ? d[0].substring(colon + 1) : d[0];
                XmlNs ans = null;
                if (colon >= 0)
                {
                    String apfx = d[0].substring(0, colon);
                    ans = e.searchNs(apfx);
                    if (ans == null)
                    {
                        message("namespace error", "Namespace prefix " + apfx + " for " + local + " on " + qn[1] + " is not defined", tag_end);
                    }
                }
                String uri = ans != null ? ans.href() : null;
                if (findAttr(e, local, uri) != null) continue;
                XmlAttr a = new XmlAttr(local, null);
                if (ans != null) a.setNs(ans);
                a.appendRaw(new XmlText(d[1]));
                e.addAttribute(a);
            }
        }

        if (eprefix_undefined)
        {
            message("namespace error", "Namespace prefix " + qn[0] + " on " + qn[1] + " is not defined", tag_end);
        }

        if (empty)
        {
            return null;
        }
        space_.add(space);
        node_ = e;
        Open o = new Open(e, qname, tag_line);
        o.prefixed = qn[0] != null;
        return o;
    }


    /** The attribute with this local name and namespace uri (null for none), like xml_node_get_attr. */
    static XmlAttr findAttr(XmlElement e, String local, String uri)
    {
        for (XmlAttr a : e.attributes())
        {
            if (!local.equals(a.name())) continue;
            String au = a.ns() != null ? a.ns().href() : null;
            if (uri == null ? au == null : uri.equals(au)) return a;
        }
        return null;
    }

    /**
     * Port of libxml2 xmlAttrNormalizeSpace2: remove leading and trailing spaces and
     * collapse runs of spaces into one. Entity references count as non space.
     */
    static List<Object> normalize_parts(List<Object> value)
    {
        List<Object> out = new ArrayList<>();
        StringBuilder text = new StringBuilder();
        boolean started = false;
        boolean pending_space = false;
        for (Object o : value)
        {
            if (o instanceof EntityName en)
            {
                if (pending_space) text.append(' ');
                pending_space = false;
                if (text.length() > 0)
                {
                    out.add(text.toString());
                    text.setLength(0);
                }
                out.add(en);
                started = true;
                continue;
            }
            String str = (String)o;
            for (int k = 0; k < str.length(); k++)
            {
                char c = str.charAt(k);
                if (c == ' ')
                {
                    if (started) pending_space = true;
                    continue;
                }
                if (pending_space) text.append(' ');
                pending_space = false;
                text.append(c);
                started = true;
            }
        }
        if (text.length() > 0) out.add(text.toString());
        return out;
    }

    /** Normalize a default value like libxml2 xmlAttrNormalizeSpace. */
    static String normalize_space(String v)
    {
        StringBuilder b = new StringBuilder();
        int k = 0;
        while (k < v.length() && v.charAt(k) == ' ') k++;
        while (k < v.length())
        {
            if (v.charAt(k) == ' ')
            {
                while (k < v.length() && v.charAt(k) == ' ') k++;
                if (k < v.length()) b.append(' ');
                continue;
            }
            b.append(v.charAt(k++));
        }
        return b.toString();
    }

    /**
     * The value of an attribute default as libxml2 stores it: entity references written
     * as &amp;name; and a &amp; that came from a reference (&amp;amp; or &amp;#38;)
     * written as &amp;#38;.
     */
    static String default_to_string(List<Object> value)
    {
        StringBuilder sb = new StringBuilder();
        for (Object o : value)
        {
            if (o instanceof String str) sb.append(str.replace("&", "&#38;"));
            else sb.append('&').append(((EntityName)o).name()).append(';');
        }
        return sb.toString();
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
        // libxml2 xmlParseQNameAndCompare (used if the start tag has a prefix) only parses the
        // QName (and reports it) if it differs, otherwise a plain name is parsed.
        qname_quiet_ = true;
        String[] qn = parse_qname();
        qname_quiet_ = false;
        String name = qn == null ? "" : (qn[0] == null ? qn[1] : qn[0] + ":" + qn[1]);
        if (qn != null && qname_malformed_ && top.prefixed && !name.equals(top.qname)) qname_malformed(qn[1]);
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
            if (has_bad_ && i == bad_pos_)
            {
                has_bad_ = false;
                throw fatal("invalid character in attribute value");
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
        // Recoverable when the entity could be declared in an external subset, a warning in libxml2.
        message("parser warning", "Entity '" + name + "' not defined", i);
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
        // Like libxml2 these are reported after the reference.
        if (val >= 0x110000)
        {
            throw fatal("xmlParseCharRef: character reference out of bounds");
        }
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
        characters(t, i);
    }

    /**
     * Add character data, line_pos is where the first chunk of a new text node ends
     * (libxml2 gives a text node the line where its first chunk was handed over).
     */
    void characters(String t, int line_pos)
    {
        if (t.isEmpty()) return;
        XmlNode last = node_.lastChild();
        boolean merge = last != null && last.type() == XmlNodeType.TEXT;
        XmlText n = new XmlText(t);
        if (!merge) n.setLine(line_at(line_pos));
        node_.addChild(n);
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
            if (has_bad_ && i == bad_pos_)
            {
                has_bad_ = false;
                // libxml2 xmlCurrentChar reports a nul byte itself.
                if (bad_cp_ == 0) message("parser error", "Invalid character: Char 0x0 out of allowed range\n\n", i);
                throw fatal("PCDATA invalid Char value " + bad_cp_);
            }
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
        // libxml2 hands character data over in chunks of 300 bytes.
        int chunk_end = start;
        int bytes = 0;
        while (chunk_end < i && bytes < 300)
        {
            int cp = s.codePointAt(chunk_end);
            bytes += cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            chunk_end += Character.charCount(cp);
        }
        characters(t, chunk_end);
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
            if (has_bad_ && i == bad_pos_)
            {
                has_bad_ = false;
                if (bad_cp_ == 0)
                {
                    // libxml2 xmlCurrentChar reports a nul byte, which then ends the comment.
                    message("parser error", "Invalid character: Char 0x0 out of allowed range\n\n", i);
                    throw fatal("Comment not terminated", i);
                }
                throw fatal("xmlParseComment: invalid xmlChar value " + bad_cp_, i);
            }
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
        // Like libxml2, the line where the comment ends.
        c.setLine(line_at(i));
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
            if (has_bad_ && bad_pos_ >= i && bad_pos_ < (end < 0 ? s.length() : end))
            {
                // Like libxml2 xmlParsePI, the pi ends at a character that is not an xml char.
                has_bad_ = false;
                throw fatal("ParsePI: PI " + target + " never end ...", bad_pos_);
            }
            if (end < 0)
            {
                throw fatal("ParsePI: PI " + target + " never end ...", start);
            }
            data = s.substring(i, end);
            i = end + 2;
        }
        XmlPI pi = new XmlPI(target, data);
        // Like libxml2, the line where the pi ends.
        pi.setLine(line_at(i));
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
            // Like libxml2, even an empty cdata section creates a (empty) text node,
            // with the line where the section ends.
            XmlNode last = node_.lastChild();
            XmlText t = new XmlText(content);
            if (last == null || last.type() != XmlNodeType.TEXT) t.setLine(line_at(i));
            node_.addChild(t);
        }
        else
        {
            XmlCData cd = new XmlCData(content);
            cd.setLine(line_at(i));
            node_.appendRaw(cd);
        }
    }

    // ---------------------------------------------------------------- doctype

    void parse_doctype_decl() throws XmlParseException
    {
        i += 9;
        if (!skip_blanks()) throw fatal("Space required after 'DOCTYPE'");
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
                throw fatal("Content error in the internal subset");
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
                dtd.has_pe_refs_ = true;
                // Kept as written, parameter entities are never expanded implicitly.
                dtd.addDecl(new XmlDtdDecl(XmlNodeType.PE_REF, n, "%" + n + ";\n"));
            }
            else
            {
                // Like libxml2 xmlParseMarkupDecl, an unknown declaration starting with
                // <!E <!A <!N or <!- is skipped by two characters before the error.
                char k = nxt(2);
                boolean skip = cur() == '<' && nxt(1) == '!' && (k == 'E' || k == 'A' || k == 'N' || k == '-');
                throw fatal("Content error in the internal subset", i + (skip ? 2 : 0));
            }
        }
    }

    /**
     * An error in an element content declaration. libxml2 reports it and returns from the
     * declaration at the error position, its internal subset loop then reports
     * "Content error in the internal subset" there unless a declaration, a parameter entity
     * reference or the end of the subset follows. The parser stops at the first error it
     * cannot recover from like that.
     */
    XmlParseException cm_error(String msg)
    {
        int pos = i;
        if (in_subset_)
        {
            int k = pos;
            while (k < s.length() && is_blank(s.charAt(k))) k++;
            char c = k < s.length() ? s.charAt(k) : 0;
            char n = k + 1 < s.length() ? s.charAt(k + 1) : 0;
            boolean goes_on = k < s.length() && ((c == '<' && (n == '!' || n == '?')) || c == '%' || c == ']');
            if (!goes_on)
            {
                message("parser error", msg, pos);
                return fatal("Content error in the internal subset", k);
            }
        }
        return fatal(msg, pos);
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
            int vpos = i + 1;
            if (has_bad_ && bad_pos_ >= vpos && bad_pos_ < end)
            {
                // libxml2 xmlExpandPEsInEntityValue checks the literal once it is read.
                has_bad_ = false;
                throw fatal("invalid character in entity value", end);
            }
            i = end + 1;
            String content = decode_entity_value(orig, dtd, vpos);
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
        String pre = pe ? null : predefined(name);
        if (pre != null && !valid_predefined_redecl(pre.charAt(0), e.etype(), e.content()))
        {
            // Port of the libxml2 xmlAddEntity check (4.6 Predefined Entities), reported and ignored.
            // libxml2 adds the entity (and warns) before the closing '>' is consumed.
            message("parser warning", "Invalid redeclaration of predefined entity '" + name + "'", i - 1);
            return;
        }
        // A valid redeclaration of a predefined entity is kept in the dtd like libxml2
        // does, but references still use the predefined value.
        dtd.addEntity(e);
    }

    /** Is this a valid redeclaration of the predefined entity for c, like libxml2 xmlAddEntity? */
    static boolean valid_predefined_redecl(char c, int etype, String content)
    {
        if (etype != XmlEntityDecl.INTERNAL_GENERAL || content == null) return false;
        if (content.length() == 1 && content.charAt(0) == c && (c == '>' || c == '\'' || c == '"')) return true;
        if (content.startsWith("&#x"))
        {
            String hex = "0123456789ABCDEF";
            String ref = "" + hex.charAt(c / 16 % 16) + hex.charAt(c % 16) + ";";
            return content.substring(3).toUpperCase(java.util.Locale.ROOT).equals(ref);
        }
        if (content.startsWith("&#"))
        {
            String ref = "" + (char)('0' + c / 10 % 10) + (char)('0' + c % 10) + ";";
            return content.substring(2).equals(ref);
        }
        return false;
    }

    /** Expand character references (and parameter entity references) in an entity value. */
    /** Like xml_decode_entity_value in the C version: the character references of an entity
        value, errors are reported at pos (the start of the value). */
    String decode_entity_value(String v, XmlDtd dtd, int pos) throws XmlParseException
    {
        StringBuilder out = new StringBuilder();
        int k = 0;
        while (k < v.length())
        {
            char c = v.charAt(k);
            if (c == '&' && k + 1 < v.length() && v.charAt(k + 1) == '#')
            {
                int semi = v.indexOf(';', k);
                if (semi < 0) throw fatal("xmlParseCharRef: invalid decimal value", pos);
                boolean hex = k + 2 < v.length() && v.charAt(k + 2) == 'x';
                int d = k + 2 + (hex ? 1 : 0);
                if (d == semi) throw fatal("xmlParseCharRef: invalid decimal value", pos);
                int cp = 0;
                for (; d < semi; ++d)
                {
                    char ch = v.charAt(d);
                    int digit;
                    if (ch >= '0' && ch <= '9') digit = ch - '0';
                    else if (hex && ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
                    else if (hex && ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
                    else throw fatal(hex ? "xmlParseCharRef: invalid hexadecimal value" : "xmlParseCharRef: invalid decimal value", pos);
                    cp = cp * (hex ? 16 : 10) + digit;
                    if (cp > 0x110000) cp = 0x110000;
                }
                if (!is_xml_char(cp)) throw fatal("xmlParseCharRef: invalid xmlChar value " + cp, pos);
                out.appendCodePoint(cp);
                k = semi + 1;
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

    /**
     * Parses a content model like libxml2 xmlParseElementMixedContentDecl and
     * xmlParseElementChildrenContentDeclPriv (including their normalizations
     * of occurrences) and prints it like xmlBufDumpElementContent. A port of
     * the content model code in the C version, parts/xml_parser.c.
     */
    static class ContentModel
    {
        static final int PCDATA = 1, ELEMENT = 2, SEQ = 3, OR = 4;
        static final int ONCE = 1, OPT = 2, MULT = 3, PLUS = 4;

        static class Node
        {
            int type;
            int ocur = ONCE;
            String name;
            Node c1, c2, parent;
            Node(int type)
            {
                this.type = type;
            }
        }

        final XmlParser p;
        ContentModel(XmlParser p)
        {
            this.p = p;
        }

        static void setC1(Node parent, Node child)
        {
            parent.c1 = child;
            if (child != null) child.parent = parent;
        }

        static void setC2(Node parent, Node child)
        {
            parent.c2 = child;
            if (child != null) child.parent = parent;
        }

        static Node element(String name)
        {
            Node n = new Node(ELEMENT);
            n.name = name;
            return n;
        }

        /** Parse the content model starting at '(' and return it printed. */
        String parse() throws XmlParseException
        {
            p.i++;
            p.skip_blanks();
            Node root = p.at("#PCDATA") ? parseMixed() : parseChildren(1);
            StringBuilder sb = new StringBuilder();
            print(sb, root);
            return sb.toString();
        }

        int parseOccur()
        {
            char c = p.cur();
            if (c == '?') { p.i++; return OPT; }
            if (c == '*') { p.i++; return MULT; }
            if (c == '+') { p.i++; return PLUS; }
            return ONCE;
        }

        /** Port of xmlParseElementMixedContentDecl, the '(' and blanks have been skipped. */
        Node parseMixed() throws XmlParseException
        {
            Node ret = null, cur = null;
            String elem = null;
            p.i += 7; // #PCDATA
            p.skip_blanks();
            if (p.cur() == ')')
            {
                p.i++;
                ret = new Node(PCDATA);
                if (p.cur() == '*')
                {
                    ret.ocur = MULT;
                    p.i++;
                }
                return ret;
            }
            if (p.cur() == '(' || p.cur() == '|')
            {
                ret = cur = new Node(PCDATA);
            }
            while (p.cur() == '|')
            {
                p.i++;
                Node n = new Node(OR);
                if (elem == null)
                {
                    setC1(n, cur);
                    ret = cur = n;
                }
                else
                {
                    setC2(cur, n);
                    setC1(n, element(elem));
                    cur = n;
                }
                p.skip_blanks();
                elem = p.parse_name();
                if (elem == null) throw p.cm_error("xmlParseElementMixedContentDecl : Name expected");
                p.skip_blanks();
            }
            if (p.cur() == ')' && p.nxt(1) == '*')
            {
                if (elem != null) setC2(cur, element(elem));
                if (ret != null) ret.ocur = MULT;
                p.i += 2;
            }
            else
            {
                throw p.cm_error("xmlParseElementMixedContentDecl : '|' or ')*' expected");
            }
            return ret;
        }

        /** Port of xmlParseElementChildrenContentDeclPriv, the '(' has been skipped. */
        Node parseChildren(int depth) throws XmlParseException
        {
            Node ret, cur, last = null;
            char type = 0;
            if (depth > 256)
            {
                throw p.fatal("xmlParseElementChildrenContentDecl : depth " + depth + " too deep, use XML_PARSE_HUGE");
            }
            p.skip_blanks();
            if (p.cur() == '(')
            {
                p.i++;
                cur = ret = parseChildren(depth + 1);
            }
            else
            {
                String elem = p.parse_name();
                if (elem == null) throw p.cm_error("ContentDecl : Name or '(' expected");
                cur = ret = element(elem);
                cur.ocur = parseOccur();
            }
            while (true)
            {
                p.skip_blanks();
                char c = p.cur();
                if (c == ')') break;
                if (c == ',' || c == '|')
                {
                    if (type == 0) type = c;
                    else if (type != c) throw p.cm_error("xmlParseElementChildrenContentDecl : '" + type + "' expected");
                    p.i++;
                    Node op = new Node(c == ',' ? SEQ : OR);
                    if (last == null)
                    {
                        setC1(op, ret);
                        ret = cur = op;
                    }
                    else
                    {
                        setC2(cur, op);
                        setC1(op, last);
                        cur = op;
                        last = null;
                    }
                }
                else
                {
                    throw p.cm_error("ContentDecl : ',' '|' or ')' expected");
                }
                p.skip_blanks();
                if (p.cur() == '(')
                {
                    p.i++;
                    last = parseChildren(depth + 1);
                }
                else
                {
                    String elem = p.parse_name();
                    if (elem == null) throw p.cm_error("ContentDecl : Name or '(' expected");
                    last = element(elem);
                    last.ocur = parseOccur();
                }
            }
            if (cur != null && last != null) setC2(cur, last);
            p.i++; // )

            char o = p.cur();
            if (o == '?')
            {
                ret.ocur = (ret.ocur == PLUS || ret.ocur == MULT) ? MULT : OPT;
                p.i++;
            }
            else if (o == '*')
            {
                ret.ocur = MULT;
                // Some normalization: (a | b* | c?)* == (a | b | c)*
                for (Node k = ret; k != null && k.type == OR; k = k.c2)
                {
                    if (k.c1 != null && (k.c1.ocur == OPT || k.c1.ocur == MULT)) k.c1.ocur = ONCE;
                    if (k.c2 != null && (k.c2.ocur == OPT || k.c2.ocur == MULT)) k.c2.ocur = ONCE;
                }
                p.i++;
            }
            else if (o == '+')
            {
                boolean found = false;
                ret.ocur = (ret.ocur == OPT || ret.ocur == MULT) ? MULT : PLUS;
                // Some normalization: (a | b*)+ == (a | b)* and (a | b?)+ == (a | b)*
                for (Node k = cur; k != null && k.type == OR; k = k.c2)
                {
                    if (k.c1 != null && (k.c1.ocur == OPT || k.c1.ocur == MULT)) { k.c1.ocur = ONCE; found = true; }
                    if (k.c2 != null && (k.c2.ocur == OPT || k.c2.ocur == MULT)) { k.c2.ocur = ONCE; found = true; }
                }
                if (found) ret.ocur = MULT;
                p.i++;
            }
            return ret;
        }

        static void printOccur(StringBuilder out, Node n)
        {
            switch (n.ocur)
            {
            case OPT -> out.append('?');
            case MULT -> out.append('*');
            case PLUS -> out.append('+');
            default -> { }
            }
        }

        /** Port of xmlBufDumpElementContent. */
        static void print(StringBuilder out, Node content)
        {
            out.append('(');
            Node cur = content;
            do
            {
                if (cur.type == SEQ || cur.type == OR)
                {
                    if (cur != content && cur.parent != null && (cur.type != cur.parent.type || cur.ocur != ONCE))
                    {
                        out.append('(');
                    }
                    cur = cur.c1;
                    continue;
                }
                out.append(cur.type == PCDATA ? "#PCDATA" : cur.name);
                while (cur != content)
                {
                    Node parent = cur.parent;
                    if ((cur.type == OR || cur.type == SEQ) && (cur.type != parent.type || cur.ocur != ONCE))
                    {
                        out.append(')');
                    }
                    printOccur(out, cur);
                    if (cur == parent.c1)
                    {
                        if (parent.type == SEQ) out.append(" , ");
                        else if (parent.type == OR) out.append(" | ");
                        cur = parent.c2;
                        break;
                    }
                    cur = parent;
                }
            }
            while (cur != content);
            out.append(')');
            printOccur(out, content);
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
            boolean cdata = false;
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
                    cdata = type.equals("CDATA");
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
                default_value = default_to_string(v);
                if (!cdata) default_value = normalize_space(default_value);
            }
            attr_cdata_.putIfAbsent(elem + "\u0001" + attr, cdata);
            if (default_value != null)
            {
                // Like libxml2, the default value (entity references written as &name;)
                // is escaped as an attribute value.
                decl.append(" \"");
                XmlSave.attr_serialize_text(decl, default_value);
                decl.append('"');
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
