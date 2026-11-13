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

// Ported from libxml2 2.9.14 HTMLparser.c and SAX2.c,
// Copyright (C) 1998-2012 Daniel Veillard, MIT license.

package org.libxmq.imp.xml;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * An html parser that builds exactly the same tree as the libxml2 (2.9.14)
 * html parser does when invoked by xmq as:
 * htmlReadMemory(buf, len, NULL, "UTF-8", HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING
 * | HTML_PARSE_NONET [| HTML_PARSE_NOBLANKS]).
 *
 * The code is ported from libxml2 HTMLparser.c and SAX2.c
 * (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 * Like libxml2 the parser works on the UTF-8 bytes of the input,
 * this is necessary to get the same chunking of text as libxml2,
 * which affects the blank detection.
 * Errors are never reported (NOERROR/NOWARNING), the parser always recovers.
 */
public final class HtmlParser
{
    private static final int HTML_PARSER_BUFFER_SIZE = 100;
    private static final int HTML_PARSER_BIG_BUFFER_SIZE = 1000;
    private static final int MAX_DEPTH = 256;
    private static final int PAD = 16;

    private static final String[] ALLOW_PCDATA = {
        "a", "abbr", "acronym", "address", "applet", "b", "bdo", "big",
        "blockquote", "body", "button", "caption", "center", "cite", "code",
        "dd", "del", "dfn", "div", "dt", "em", "font", "form", "h1", "h2",
        "h3", "h4", "h5", "h6", "i", "iframe", "ins", "kbd", "label", "legend",
        "li", "noframes", "noscript", "object", "p", "pre", "q", "s", "samp",
        "small", "span", "strike", "strong", "td", "th", "tt", "u", "var"
    };

    private static final String[] BOOLEAN_ATTRS = {
        "checked", "compact", "declare", "defer", "disabled", "ismap",
        "multiple", "nohref", "noresize", "noshade", "nowrap", "readonly",
        "selected"
    };

    private byte[] in_;
    private int pos_;
    private int end_;
    private int curLen_;

    private boolean eof_;
    private boolean disableSAX_;
    private final boolean keepBlanks_;
    private int html_;
    private int depth_;

    private final XmlDocument myDoc_;
    private final List<String> nameTab_ = new ArrayList<>();
    private String name_;
    private final List<XmlNode> nodeTab_ = new ArrayList<>();
    private XmlNode node_;

    private XmlText pendingText_;
    private StringBuilder pendingSb_;

    private HtmlParser(String content, boolean noblanks)
    {
        byte[] b = content.getBytes(StandardCharsets.UTF_8);
        in_ = new byte[b.length + PAD];
        System.arraycopy(b, 0, in_, 0, b.length);
        end_ = b.length;
        pos_ = 0;
        keepBlanks_ = !noblanks;
        myDoc_ = new XmlDocument(true);
    }

    /**
     * Parse html content into a document, like libxml2 htmlReadMemory with the
     * options HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET.
     * @param content The html source.
     * @param noblanks If true then also HTML_PARSE_NOBLANKS is used, ie ignorable
     *        whitespace text nodes are dropped.
     * @return The html document, or null if the content is empty
     *         (like htmlReadMemory which returns NULL for a zero length buffer).
     */
    public static XmlDocument parse(String content, boolean noblanks)
    {
        if (content == null || content.isEmpty()) return null;
        HtmlParser p = new HtmlParser(content, noblanks);
        p.parseDocument();
        return p.myDoc_;
    }

    /**
     * Check if an attribute is an html boolean attribute, like libxml2 htmlIsBooleanAttr.
     * @param name The attribute name.
     * @return True if the attribute is boolean (checked, selected...).
     */
    public static boolean isBooleanAttr(String name)
    {
        if (name == null) return false;
        for (String b : BOOLEAN_ATTRS)
        {
            if (b.equals(HtmlElementTable.asciiLower(name))) return true;
        }
        return false;
    }

    // -----------------------------------------------------------------
    // Input access, corresponds to the CUR/NXT/NEXT/SKIP macros.

    private int cur()
    {
        return in_[pos_] & 0xff;
    }

    private int nxt(int n)
    {
        int p = pos_ + n;
        if (p >= in_.length) return 0;
        return in_[p] & 0xff;
    }

    private int upp(int n)
    {
        int c = nxt(n);
        if (c >= 'a' && c <= 'z') c -= 0x20;
        return c;
    }

    private void skip(int n)
    {
        pos_ += n;
    }

    private int utf8Len(int c)
    {
        if (c < 0x80) return 1;
        if ((c & 0xe0) == 0xc0) return 2;
        if ((c & 0xf0) == 0xe0) return 3;
        if ((c & 0xf8) == 0xf0) return 4;
        return 1;
    }

    /** xmlNextChar */
    private void next()
    {
        if (eof_) return;
        if (cur() == 0) return;
        pos_ += utf8Len(cur());
        if (pos_ > end_) pos_ = end_;
    }

    private void nextl(int l)
    {
        pos_ += l;
    }

    /** htmlCurrentChar, sets curLen_. */
    private int curChar()
    {
        if (eof_) return 0;
        int c = cur();
        if (c >= 0x80)
        {
            int l = utf8Len(c);
            int val;
            switch (l)
            {
            case 2 -> val = ((c & 0x1f) << 6) | (nxt(1) & 0x3f);
            case 3 -> val = ((c & 0xf) << 12) | ((nxt(1) & 0x3f) << 6) | (nxt(2) & 0x3f);
            case 4 -> val = ((c & 0x7) << 18) | ((nxt(1) & 0x3f) << 12) | ((nxt(2) & 0x3f) << 6) | (nxt(3) & 0x3f);
            default -> val = c;
            }
            curLen_ = l;
            return val;
        }
        curLen_ = 1;
        if (c == 0 && pos_ < end_) return ' ';
        return c;
    }

    /** htmlSkipBlankChars */
    private int skipBlanks()
    {
        int res = 0;
        while (isBlankCh(cur()))
        {
            pos_++;
            res++;
        }
        return res;
    }

    private static boolean isBlankCh(int c)
    {
        return c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d;
    }

    private static boolean isAsciiLetter(int c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    private static boolean isAsciiDigit(int c)
    {
        return c >= '0' && c <= '9';
    }

    /** xmlHaltParser */
    private void haltParser()
    {
        eof_ = true;
        disableSAX_ = true;
        in_ = new byte[PAD];
        pos_ = 0;
        end_ = 0;
    }

    // -----------------------------------------------------------------
    // A growable byte buffer, used where libxml2 collects UTF-8 bytes.

    private static final class ByteBuf
    {
        byte[] b = new byte[128];
        int n;

        void add(int c)
        {
            if (n == b.length) b = java.util.Arrays.copyOf(b, n * 2);
            b[n++] = (byte)c;
        }

        void addChar(int c)
        {
            if (c < 0x80)
            {
                add(c);
            }
            else if (c < 0x800)
            {
                add(((c >> 6) & 0x1F) | 0xC0);
                add((c & 0x3F) | 0x80);
            }
            else if (c < 0x10000)
            {
                add(((c >> 12) & 0x0F) | 0xE0);
                add(((c >> 6) & 0x3F) | 0x80);
                add((c & 0x3F) | 0x80);
            }
            else
            {
                add(((c >> 18) & 0x07) | 0xF0);
                add(((c >> 12) & 0x3F) | 0x80);
                add(((c >> 6) & 0x3F) | 0x80);
                add((c & 0x3F) | 0x80);
            }
        }

        /** Returns the content as a string, a nul byte terminates the string like in C. */
        String str()
        {
            int len = 0;
            while (len < n && b[len] != 0) len++;
            return new String(b, 0, len, StandardCharsets.UTF_8);
        }
    }

    private String str(int from, int len)
    {
        return new String(in_, from, len, StandardCharsets.UTF_8);
    }

    // -----------------------------------------------------------------
    // Name and node stacks.

    private void namePush(String value)
    {
        if (html_ < 3 && value.equals("head")) html_ = 3;
        if (html_ < 10 && value.equals("body")) html_ = 10;
        nameTab_.add(value);
        name_ = value;
    }

    private String namePop()
    {
        if (nameTab_.isEmpty()) return null;
        String ret = nameTab_.remove(nameTab_.size() - 1);
        name_ = nameTab_.isEmpty() ? null : nameTab_.get(nameTab_.size() - 1);
        return ret;
    }

    private int nameNr()
    {
        return nameTab_.size();
    }

    private boolean nodePush(XmlNode n)
    {
        if (nodeTab_.size() > MAX_DEPTH)
        {
            haltParser();
            return false;
        }
        nodeTab_.add(n);
        node_ = n;
        return true;
    }

    private void nodePop()
    {
        if (nodeTab_.isEmpty()) return;
        nodeTab_.remove(nodeTab_.size() - 1);
        node_ = nodeTab_.isEmpty() ? null : nodeTab_.get(nodeTab_.size() - 1);
    }

    private static boolean strEqual(String a, String b)
    {
        if (a == null) return b == null;
        return a.equals(b);
    }

    // -----------------------------------------------------------------
    // The SAX2 tree building callbacks (SAX2.c).

    private void flushText()
    {
        if (pendingText_ != null)
        {
            pendingText_.setContent(pendingSb_.toString());
            pendingText_ = null;
            pendingSb_ = null;
        }
    }

    /** xmlSAX2Text */
    private void saxText(String s, boolean cdata)
    {
        if (node_ == null) return;
        XmlNodeType type = cdata ? XmlNodeType.CDATA : XmlNodeType.TEXT;
        XmlNode last = node_.last_;
        if (last != null && last.type() == type)
        {
            if (pendingText_ != last)
            {
                flushText();
                pendingText_ = (XmlText)last;
                pendingSb_ = new StringBuilder(last.content() == null ? "" : last.content());
            }
            pendingSb_.append(s);
            return;
        }
        flushText();
        XmlText t = cdata ? new XmlCData(s) : new XmlText(s);
        node_.appendRaw(t);
        pendingText_ = t;
        pendingSb_ = new StringBuilder(s);
    }

    private void characters(String s)
    {
        saxText(s, false);
    }

    private void cdataBlock(String s)
    {
        saxText(s, true);
    }

    /** Add a node like the SAX2 comment and pi callbacks do. */
    private void addMisc(XmlNode n)
    {
        flushText();
        XmlNode parent = node_;
        if (parent == null)
        {
            myDoc_.appendRaw(n);
        }
        else if (parent.type() == XmlNodeType.ELEMENT)
        {
            parent.appendRaw(n);
        }
        else
        {
            addSibling(parent, n);
        }
    }

    /** xmlAddSibling, add at the end of the sibling list. */
    private static void addSibling(XmlNode sibling, XmlNode n)
    {
        if (sibling.parent_ != null)
        {
            sibling.parent_.appendRaw(n);
        }
        else
        {
            XmlNode last = sibling;
            while (last.next_ != null) last = last.next_;
            last.addNextSibling(n);
        }
    }

    /** xmlSAX2StartElement */
    private void startElement(String fullname, List<String[]> atts)
    {
        flushText();
        XmlNode parent = node_;
        XmlElement ret = new XmlElement(localName(fullname));
        if (myDoc_.children_ == null)
        {
            myDoc_.appendRaw(ret);
        }
        else if (parent == null)
        {
            parent = myDoc_.children_;
        }
        if (!nodePush(ret))
        {
            ret.unlink();
            return;
        }
        if (parent != null)
        {
            if (parent.type() == XmlNodeType.ELEMENT)
            {
                parent.appendRaw(ret);
            }
            else
            {
                addSibling(parent, ret);
            }
        }
        if (atts != null)
        {
            for (String[] a : atts)
            {
                String value = a[1];
                if (value == null && isBooleanAttr(a[0])) value = a[0];
                XmlAttr attr = new XmlAttr(a[0], null);
                if (value != null) attr.appendRaw(new XmlText(value));
                ret.addAttribute(attr);
            }
        }
    }

    /**
     * The local part of xmlSplitQName, the html parser drops the prefix
     * of an element name (but keeps it for attributes).
     */
    private static String localName(String name)
    {
        if (name.startsWith(":")) return name;
        int i = name.indexOf(':');
        if (i < 0) return name;
        if (i == name.length() - 1) return name;
        return name.substring(i + 1);
    }

    /** xmlSAX2EndElement */
    private void endElement()
    {
        flushText();
        nodePop();
    }

    /** xmlSAX2InternalSubset */
    private void internalSubset(String name, String externalId, String systemId)
    {
        flushText();
        if (myDoc_.intSubset() != null) return;
        myDoc_.setIntSubset(new XmlDtd(name, externalId, systemId));
    }

    // -----------------------------------------------------------------
    // Auto close and implied elements.

    private static int endPriority(String name)
    {
        if (name == null) return 100;
        return switch (name)
        {
        case "div" -> 150;
        case "td", "th" -> 160;
        case "tr" -> 170;
        case "thead", "tbody", "tfoot" -> 180;
        case "table" -> 190;
        case "head", "body" -> 200;
        case "html" -> 220;
        default -> 100;
        };
    }

    /** htmlAutoCloseOnClose */
    private void autoCloseOnClose(String newtag)
    {
        int priority = endPriority(newtag);
        int i;
        for (i = nameNr() - 1; i >= 0; i--)
        {
            if (strEqual(newtag, nameTab_.get(i))) break;
            if (endPriority(nameTab_.get(i)) > priority) return;
        }
        if (i < 0) return;
        while (!strEqual(newtag, name_))
        {
            endElement();
            namePop();
        }
    }

    /** htmlAutoCloseOnEnd */
    private void autoCloseOnEnd()
    {
        if (nameNr() == 0) return;
        for (int i = nameNr() - 1; i >= 0; i--)
        {
            endElement();
            namePop();
        }
    }

    /** htmlAutoClose */
    private void autoClose(String newtag)
    {
        while (newtag != null && name_ != null && HtmlElementTable.checkAutoClose(newtag, name_))
        {
            endElement();
            namePop();
        }
        if (newtag == null)
        {
            autoCloseOnEnd();
        }
    }

    /** htmlCheckImplied */
    private void checkImplied(String newtag)
    {
        if (newtag.equals("html")) return;
        if (nameNr() <= 0)
        {
            namePush("html");
            startElement("html", null);
        }
        if (newtag.equals("body") || newtag.equals("head")) return;
        if (nameNr() <= 1 &&
            (newtag.equals("script") || newtag.equals("style") || newtag.equals("meta") ||
             newtag.equals("link") || newtag.equals("title") || newtag.equals("base")))
        {
            if (html_ >= 3) return;
            namePush("head");
            startElement("head", null);
        }
        else if (!newtag.equals("noframes") && !newtag.equals("frame") && !newtag.equals("frameset"))
        {
            if (html_ >= 10) return;
            for (int i = 0; i < nameNr(); i++)
            {
                if (nameTab_.get(i).equals("body")) return;
                if (nameTab_.get(i).equals("head")) return;
            }
            namePush("body");
            startElement("body", null);
        }
    }

    /** htmlCheckParagraph */
    private void checkParagraph()
    {
        String tag = name_;
        if (tag == null)
        {
            autoClose("p");
            checkImplied("p");
            namePush("p");
            startElement("p", null);
            return;
        }
        if (tag.equals("html") || tag.equals("head"))
        {
            autoClose("p");
            checkImplied("p");
            namePush("p");
            startElement("p", null);
        }
    }

    /** areBlanks */
    private boolean areBlanks(ByteBuf buf)
    {
        for (int j = 0; j < buf.n; j++)
        {
            if (!isBlankCh(buf.b[j] & 0xff)) return false;
        }
        if (cur() == 0) return true;
        if (cur() != '<') return false;
        if (name_ == null) return true;
        if (name_.equals("html")) return true;
        if (name_.equals("head")) return true;
        if (name_.equals("body"))
        {
            XmlDtd dtd = myDoc_.intSubset();
            if (dtd != null && dtd.externalId() != null)
            {
                if (dtd.externalId().equalsIgnoreCase("-//W3C//DTD HTML 4.01//EN") ||
                    dtd.externalId().equalsIgnoreCase("-//W3C//DTD HTML 4//EN"))
                {
                    return true;
                }
            }
        }
        if (node_ == null) return false;
        XmlNode lastChild = node_.last_;
        while (lastChild != null && lastChild.type() == XmlNodeType.COMMENT) lastChild = lastChild.prev_;
        if (lastChild == null)
        {
            for (String s : ALLOW_PCDATA)
            {
                if (s.equals(name_)) return false;
            }
        }
        else if (lastChild.type() == XmlNodeType.TEXT)
        {
            return false;
        }
        else
        {
            for (String s : ALLOW_PCDATA)
            {
                if (s.equals(lastChild.name())) return false;
            }
        }
        return true;
    }

    // -----------------------------------------------------------------
    // The parser itself.

    /** htmlParseHTMLName, returns a lowercase tag or attribute name. */
    private String parseHTMLName()
    {
        int c = cur();
        if (!isAsciiLetter(c) && c != '_' && c != ':' && c != '.') return null;
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < HTML_PARSER_BUFFER_SIZE)
        {
            c = cur();
            if (!(isAsciiLetter(c) || isAsciiDigit(c) || c == ':' || c == '-' || c == '_' || c == '.')) break;
            if (c >= 'A' && c <= 'Z') c += 0x20;
            sb.append((char)c);
            i++;
            next();
        }
        return sb.toString();
    }

    /** htmlParseHTMLName_nonInvasive */
    private String parseHTMLNameNonInvasive()
    {
        int c = nxt(1);
        if (!isAsciiLetter(c) && c != '_' && c != ':') return null;
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < HTML_PARSER_BUFFER_SIZE)
        {
            c = nxt(1 + i);
            if (!(isAsciiLetter(c) || isAsciiDigit(c) || c == ':' || c == '-' || c == '_')) break;
            if (c >= 'A' && c <= 'Z') c += 0x20;
            sb.append((char)c);
            i++;
        }
        return sb.toString();
    }

    /** htmlParseName, case sensitive. */
    private String parseName()
    {
        int p = pos_;
        int c = in_[p] & 0xff;
        if ((c >= 0x61 && c <= 0x7A) || (c >= 0x41 && c <= 0x5A) || c == '_' || c == ':')
        {
            p++;
            for (;;)
            {
                c = in_[p] & 0xff;
                if ((c >= 0x61 && c <= 0x7A) || (c >= 0x41 && c <= 0x5A) || (c >= 0x30 && c <= 0x39) ||
                    c == '_' || c == '-' || c == ':' || c == '.')
                {
                    p++;
                }
                else
                {
                    break;
                }
            }
            if (p == end_) return null;
            c = in_[p] & 0xff;
            if (c > 0 && c < 0x80)
            {
                String ret = str(pos_, p - pos_);
                pos_ = p;
                return ret;
            }
        }
        return parseNameComplex();
    }

    /** htmlParseNameComplex */
    private String parseNameComplex()
    {
        int len = 0;
        int c = curChar();
        int l = curLen_;
        if (c == ' ' || c == '>' || c == '/' ||
            (!HtmlCharClasses.isLetter(c) && c != '_' && c != ':'))
        {
            return null;
        }
        while (c != ' ' && c != '>' && c != '/' &&
               (HtmlCharClasses.isLetter(c) || HtmlCharClasses.isDigit(c) ||
                c == '.' || c == '-' || c == '_' || c == ':' ||
                HtmlCharClasses.isCombining(c) || HtmlCharClasses.isExtender(c)))
        {
            len += l;
            nextl(l);
            c = curChar();
            l = curLen_;
        }
        return str(pos_ - len, len);
    }

    /** htmlParseHTMLAttribute */
    private String parseHTMLAttribute(int stop)
    {
        ByteBuf out = new ByteBuf();
        while (cur() != 0 && cur() != stop)
        {
            if (stop == 0 && cur() == '>') break;
            if (stop == 0 && isBlankCh(cur())) break;
            if (cur() == '&')
            {
                if (nxt(1) == '#')
                {
                    int c = parseCharRef();
                    out.addChar(c);
                }
                else
                {
                    String[] nameOut = new String[1];
                    int ent = parseEntityRef(nameOut);
                    String name = nameOut[0];
                    if (name == null)
                    {
                        out.add('&');
                    }
                    else if (ent < 0)
                    {
                        out.add('&');
                        byte[] nb = name.getBytes(StandardCharsets.UTF_8);
                        for (byte x : nb) out.add(x);
                    }
                    else
                    {
                        out.addChar(ent);
                    }
                }
            }
            else
            {
                int c = curChar();
                out.addChar(c);
                next();
            }
        }
        return out.str();
    }

    /**
     * htmlParseEntityRef, returns the code point of the entity or -1 if
     * not found. The name (if any) is stored in nameOut[0].
     */
    private int parseEntityRef(String[] nameOut)
    {
        nameOut[0] = null;
        int ent = -1;
        if (cur() == '&')
        {
            next();
            String name = parseName();
            if (name != null)
            {
                if (cur() == ';')
                {
                    nameOut[0] = name;
                    ent = HtmlEntityTable.lookup(name);
                    if (ent >= 0) next();
                }
                else
                {
                    nameOut[0] = name;
                }
            }
        }
        return ent;
    }

    /** htmlParseAttValue */
    private String parseAttValue()
    {
        String ret;
        if (cur() == '"')
        {
            next();
            ret = parseHTMLAttribute('"');
            if (cur() == '"') next();
        }
        else if (cur() == '\'')
        {
            next();
            ret = parseHTMLAttribute('\'');
            if (cur() == '\'') next();
        }
        else
        {
            ret = parseHTMLAttribute(0);
        }
        return ret;
    }

    /** htmlParseSystemLiteral */
    private String parseSystemLiteral()
    {
        int len = 0;
        boolean err = false;
        if (cur() != '"' && cur() != '\'') return null;
        int quote = cur();
        next();
        int startPosition = pos_;
        while (cur() != 0 && cur() != quote)
        {
            int c = cur();
            if (!(c == 0x9 || c == 0xa || c == 0xd || c >= 0x20)) err = true;
            next();
            len++;
        }
        String ret = null;
        if (cur() == quote)
        {
            next();
            if (!err) ret = str(startPosition, len);
        }
        return ret;
    }

    /** htmlParsePubidLiteral */
    private String parsePubidLiteral()
    {
        int len = 0;
        boolean err = false;
        if (cur() != '"' && cur() != '\'') return null;
        int quote = cur();
        next();
        int startPosition = pos_;
        while (cur() != 0 && cur() != quote)
        {
            if (!HtmlCharClasses.isPubidChar(cur())) err = true;
            len++;
            next();
        }
        String ret = null;
        if (cur() == quote)
        {
            next();
            if (!err) ret = str(startPosition, len);
        }
        return ret;
    }

    /** htmlParseScript */
    private void parseScript()
    {
        ByteBuf buf = new ByteBuf();
        int c = curChar();
        int l = curLen_;
        while (c != 0)
        {
            if (c == '<' && nxt(1) == '/')
            {
                if (isAsciiLetter(nxt(2))) break;
            }
            if (HtmlCharClasses.isChar(c))
            {
                copyBuf(buf, l, c);
            }
            if (buf.n >= HTML_PARSER_BIG_BUFFER_SIZE)
            {
                cdataBlock(buf.str());
                buf.n = 0;
            }
            nextl(l);
            c = curChar();
            l = curLen_;
        }
        if (buf.n != 0 && !disableSAX_)
        {
            cdataBlock(buf.str());
        }
    }

    /** COPY_BUF, copy the current character (l bytes) as is or encode c. */
    private void copyBuf(ByteBuf buf, int l, int c)
    {
        if (l == 1)
        {
            buf.add(c);
        }
        else
        {
            buf.addChar(c);
        }
    }

    /** Deliver a chunk of character data, used by htmlParseCharDataInternal. */
    private void deliverCharData(ByteBuf buf)
    {
        if (disableSAX_) return;
        if (areBlanks(buf))
        {
            if (keepBlanks_)
            {
                characters(buf.str());
            }
            // else ignorableWhitespace which is a no-op.
        }
        else
        {
            checkParagraph();
            characters(buf.str());
        }
    }

    /** htmlParseCharDataInternal */
    private void parseCharData()
    {
        ByteBuf buf = new ByteBuf();
        int c = curChar();
        int l = curLen_;
        while (c != '<' && c != '&' && c != 0)
        {
            if (HtmlCharClasses.isChar(c))
            {
                copyBuf(buf, l, c);
            }
            if (buf.n >= HTML_PARSER_BIG_BUFFER_SIZE)
            {
                deliverCharData(buf);
                buf.n = 0;
            }
            nextl(l);
            c = curChar();
            l = curLen_;
        }
        if (buf.n != 0)
        {
            deliverCharData(buf);
        }
        else
        {
            if (c == 0) eof_ = true;
        }
    }

    /** htmlParseExternalID, returns {uri, publicId} */
    private String[] parseExternalID()
    {
        String uri = null;
        String publicId = null;
        if (upp(0) == 'S' && upp(1) == 'Y' && upp(2) == 'S' && upp(3) == 'T' && upp(4) == 'E' && upp(5) == 'M')
        {
            skip(6);
            skipBlanks();
            uri = parseSystemLiteral();
        }
        else if (upp(0) == 'P' && upp(1) == 'U' && upp(2) == 'B' && upp(3) == 'L' && upp(4) == 'I' && upp(5) == 'C')
        {
            skip(6);
            skipBlanks();
            publicId = parsePubidLiteral();
            skipBlanks();
            if (cur() == '"' || cur() == '\'')
            {
                uri = parseSystemLiteral();
            }
        }
        return new String[] { uri, publicId };
    }

    /** htmlParsePI */
    private void parsePI()
    {
        if (cur() == '<' && nxt(1) == '?')
        {
            skip(2);
            String target = parseName();
            if (target != null)
            {
                if (cur() == '>')
                {
                    skip(1);
                    if (!disableSAX_) addMisc(new XmlPI(target, null));
                    return;
                }
                ByteBuf buf = new ByteBuf();
                skipBlanks();
                int c = curChar();
                int l = curLen_;
                while (c != 0 && c != '>')
                {
                    if (HtmlCharClasses.isChar(c))
                    {
                        copyBuf(buf, l, c);
                    }
                    nextl(l);
                    c = curChar();
                    l = curLen_;
                }
                if (c == '>')
                {
                    skip(1);
                    if (!disableSAX_) addMisc(new XmlPI(target, buf.str()));
                }
            }
        }
    }

    /** htmlParseComment */
    private void parseComment()
    {
        if (cur() != '<' || nxt(1) != '!' || nxt(2) != '-' || nxt(3) != '-') return;
        skip(4);
        ByteBuf buf = new ByteBuf();
        int q = curChar();
        int ql = curLen_;
        if (q == 0) return;
        nextl(ql);
        int r = curChar();
        int rl = curLen_;
        if (r == 0) return;
        nextl(rl);
        int c = curChar();
        int l = curLen_;
        while (c != 0 && (c != '>' || r != '-' || q != '-'))
        {
            nextl(l);
            int nx = curChar();
            int nl = curLen_;
            if (q == '-' && r == '-' && c == '!' && nx == '>')
            {
                c = '>';
                break;
            }
            if (HtmlCharClasses.isChar(q))
            {
                copyBuf(buf, ql, q);
            }
            q = r;
            ql = rl;
            r = c;
            rl = l;
            c = nx;
            l = nl;
        }
        if (c == '>')
        {
            next();
            if (!disableSAX_) addMisc(new XmlComment(buf.str()));
        }
    }

    /** htmlParseCharRef */
    private int parseCharRef()
    {
        int val = 0;
        if (cur() == '&' && nxt(1) == '#' && (nxt(2) == 'x' || nxt(2) == 'X'))
        {
            skip(3);
            while (cur() != ';')
            {
                int c = cur();
                if (c >= '0' && c <= '9')
                {
                    if (val < 0x110000) val = val * 16 + (c - '0');
                }
                else if (c >= 'a' && c <= 'f')
                {
                    if (val < 0x110000) val = val * 16 + (c - 'a') + 10;
                }
                else if (c >= 'A' && c <= 'F')
                {
                    if (val < 0x110000) val = val * 16 + (c - 'A') + 10;
                }
                else
                {
                    break;
                }
                next();
            }
            if (cur() == ';') next();
        }
        else if (cur() == '&' && nxt(1) == '#')
        {
            skip(2);
            while (cur() != ';')
            {
                int c = cur();
                if (c >= '0' && c <= '9')
                {
                    if (val < 0x110000) val = val * 10 + (c - '0');
                }
                else
                {
                    break;
                }
                next();
            }
            if (cur() == ';') next();
        }
        if (HtmlCharClasses.isChar(val)) return val;
        return 0;
    }

    /** htmlParseDocTypeDecl */
    private void parseDocTypeDecl()
    {
        skip(9);
        skipBlanks();
        String name = parseName();
        skipBlanks();
        String[] ids = parseExternalID();
        skipBlanks();
        if (cur() != '>')
        {
            while (cur() != 0 && cur() != '>') next();
        }
        if (cur() == '>') next();
        if (!disableSAX_) internalSubset(name, ids[1], ids[0]);
    }

    /** htmlParseAttribute, returns {name, value} or null. */
    private String[] parseAttribute()
    {
        String name = parseHTMLName();
        if (name == null) return null;
        String val = null;
        skipBlanks();
        if (cur() == '=')
        {
            next();
            skipBlanks();
            val = parseAttValue();
        }
        return new String[] { name, val };
    }

    /** htmlParseStartTag, returns 0 on success, -1 on error and 1 if discarded. */
    private int parseStartTag()
    {
        boolean discardtag = false;
        if (eof_) return -1;
        if (cur() != '<') return -1;
        next();
        String name = parseHTMLName();
        if (name == null)
        {
            while (cur() != 0 && cur() != '>' && !eof_) next();
            return -1;
        }
        autoClose(name);
        checkImplied(name);
        if (nameNr() > 0 && name.equals("html"))
        {
            discardtag = true;
            depth_++;
        }
        if (nameNr() != 1 && name.equals("head"))
        {
            discardtag = true;
            depth_++;
        }
        if (name.equals("body"))
        {
            for (int indx = 0; indx < nameNr(); indx++)
            {
                if (nameTab_.get(indx).equals("body"))
                {
                    discardtag = true;
                    depth_++;
                }
            }
        }
        List<String[]> atts = new ArrayList<>();
        skipBlanks();
        while (cur() != 0 && cur() != '>' && (cur() != '/' || nxt(1) != '>'))
        {
            String[] att = parseAttribute();
            if (att != null)
            {
                boolean dup = false;
                for (String[] a : atts)
                {
                    if (a[0].equals(att[0]))
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup) atts.add(att);
            }
            else
            {
                while (cur() != 0 && !isBlankCh(cur()) && cur() != '>' && (cur() != '/' || nxt(1) != '>'))
                {
                    next();
                }
            }
            skipBlanks();
        }
        if (!discardtag)
        {
            namePush(name);
            startElement(name, atts.isEmpty() ? null : atts);
        }
        return discardtag ? 1 : 0;
    }

    /** htmlParseEndTag, returns true if the current level should be closed. */
    private boolean parseEndTag()
    {
        if (cur() != '<' || nxt(1) != '/') return false;
        skip(2);
        String name = parseHTMLName();
        if (name == null) return false;
        skipBlanks();
        if (cur() != '>')
        {
            while (cur() != 0 && cur() != '>') next();
        }
        if (cur() == '>') next();
        if (depth_ > 0 && (name.equals("html") || name.equals("body") || name.equals("head")))
        {
            depth_--;
            return false;
        }
        int i;
        for (i = nameNr() - 1; i >= 0; i--)
        {
            if (name.equals(nameTab_.get(i))) break;
        }
        if (i < 0) return false;
        autoCloseOnClose(name);
        String oldname = name_;
        if (oldname != null && oldname.equals(name))
        {
            endElement();
            namePop();
            return true;
        }
        return false;
    }

    /** htmlParseReference */
    private void parseReference()
    {
        if (cur() != '&') return;
        if (nxt(1) == '#')
        {
            int c = parseCharRef();
            if (c == 0) return;
            checkParagraph();
            characters(new String(Character.toChars(c)));
        }
        else
        {
            String[] nameOut = new String[1];
            int ent = parseEntityRef(nameOut);
            String name = nameOut[0];
            if (name == null)
            {
                checkParagraph();
                characters("&");
                return;
            }
            if (ent <= 0)
            {
                checkParagraph();
                characters("&");
                characters(name);
            }
            else
            {
                checkParagraph();
                characters(new String(Character.toChars(ent)));
            }
        }
    }

    /** htmlParserFinishElementParsing */
    private void finishElementParsing()
    {
        if (cur() == 0) autoCloseOnEnd();
    }

    /** htmlParseElementInternal */
    private void parseElementInternal()
    {
        if (eof_) return;
        int failed = parseStartTag();
        String name = name_;
        if (failed == -1 || name == null)
        {
            if (cur() == '>') next();
            return;
        }
        HtmlElementTable.Desc info = HtmlElementTable.lookup(name);
        if (cur() == '/' && nxt(1) == '>')
        {
            skip(2);
            endElement();
            namePop();
            return;
        }
        if (cur() == '>')
        {
            next();
        }
        else
        {
            if (strEqual(name, name_))
            {
                flushText();
                nodePop();
                namePop();
            }
            finishElementParsing();
            return;
        }
        if (info != null && info.empty != 0)
        {
            endElement();
            namePop();
        }
    }

    private boolean isDoctypeStart()
    {
        return cur() == '<' && nxt(1) == '!' &&
            upp(2) == 'D' && upp(3) == 'O' && upp(4) == 'C' && upp(5) == 'T' &&
            upp(6) == 'Y' && upp(7) == 'P' && upp(8) == 'E';
    }

    /** htmlParseContentInternal */
    private void parseContentInternal()
    {
        String currentNode = name_;
        int depth = nameNr();
        for (;;)
        {
            if (eof_) break;
            if (cur() == '<' && nxt(1) == '/')
            {
                if (parseEndTag() && (currentNode != null || nameNr() == 0))
                {
                    currentNode = name_;
                    depth = nameNr();
                }
                continue;
            }
            else if (cur() == '<' && (isAsciiLetter(nxt(1)) || nxt(1) == '_' || nxt(1) == ':'))
            {
                String name = parseHTMLNameNonInvasive();
                if (name == null)
                {
                    finishElementParsing();
                    currentNode = name_;
                    depth = nameNr();
                    continue;
                }
                if (name_ != null)
                {
                    if (HtmlElementTable.checkAutoClose(name, name_))
                    {
                        autoClose(name);
                        continue;
                    }
                }
            }
            if (nameNr() > 0 && depth >= nameNr() && !strEqual(currentNode, name_))
            {
                finishElementParsing();
                currentNode = name_;
                depth = nameNr();
                continue;
            }
            if (cur() != 0 && ("script".equals(currentNode) || "style".equals(currentNode)))
            {
                parseScript();
            }
            else
            {
                if (isDoctypeStart())
                {
                    parseDocTypeDecl();
                }
                if (cur() == '<' && nxt(1) == '!' && nxt(2) == '-' && nxt(3) == '-')
                {
                    parseComment();
                }
                else if (cur() == '<' && nxt(1) == '?')
                {
                    parsePI();
                }
                else if (cur() == '<' && isAsciiLetter(nxt(1)))
                {
                    parseElementInternal();
                    currentNode = name_;
                    depth = nameNr();
                }
                else if (cur() == '<')
                {
                    if (!disableSAX_) characters("<");
                    next();
                }
                else if (cur() == '&')
                {
                    parseReference();
                }
                else if (cur() == 0)
                {
                    autoCloseOnEnd();
                    break;
                }
                else
                {
                    parseCharData();
                }
            }
        }
    }

    /** htmlParseDocument */
    private void parseDocument()
    {
        html_ = 1;
        // Skip an UTF-8 byte order mark.
        if (end_ >= 3 && cur() == 0xEF && nxt(1) == 0xBB && nxt(2) == 0xBF) pos_ += 3;
        skipBlanks();
        while ((cur() == '<' && nxt(1) == '!' && nxt(2) == '-' && nxt(3) == '-') ||
               (cur() == '<' && nxt(1) == '?'))
        {
            parseComment();
            parsePI();
            skipBlanks();
        }
        if (isDoctypeStart())
        {
            parseDocTypeDecl();
        }
        skipBlanks();
        while ((cur() == '<' && nxt(1) == '!' && nxt(2) == '-' && nxt(3) == '-') ||
               (cur() == '<' && nxt(1) == '?'))
        {
            parseComment();
            parsePI();
            skipBlanks();
        }
        parseContentInternal();
        if (cur() == 0) autoCloseOnEnd();
        flushText();
        if (myDoc_.intSubset() == null)
        {
            myDoc_.setIntSubset(new XmlDtd("html",
                                           "-//W3C//DTD HTML 4.0 Transitional//EN",
                                           "http://www.w3.org/TR/REC-html40/loose.dtd"));
        }
    }
}
