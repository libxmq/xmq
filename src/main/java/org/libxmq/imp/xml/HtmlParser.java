/* This file is ported from libxml2 (HTMLparser.c and SAX2.c, version 2.16.0
   as found in the libxmq fork in 3rdparty/libxml2), like src/main/c/parts/xml_html_parser.c.
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

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An html parser that builds the same tree as libxml2 (2.16) htmlReadMemory does with
 * the options libxmq uses (HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET,
 * optionally HTML_PARSE_NOBLANKS, encoding UTF-8): html5 tokenization, libxml2's tree
 * construction with implied html/head/body elements and implicit closing of elements,
 * a default html 4.0 transitional doctype if none is given. A port of the C version
 * src/main/c/parts/xml_html_parser.c.
 *
 * Element names are lowercased, there are no namespaces. Character references and html
 * named character references (like &amp;nbsp;) are decoded. Raw text (script, style...)
 * is stored as cdata nodes, like libxml2. Like libxml2 the parser works on the UTF-8
 * bytes of the input, which gives the same chunking of text, which affects the blank
 * detection. Errors are never reported (like xmq which parses without a message
 * callback), the parser always recovers.
 */
public final class HtmlParser
{
    // Insertion modes, libxml2 ctxt->html.
    private static final int INSERT_INITIAL = 1;
    private static final int INSERT_IN_HEAD = 3;
    private static final int INSERT_IN_BODY = 10;

    // Like libxml2 HTML_PARSER_BUFFER_SIZE: longer names are truncated.
    private static final int MAX_NAME = 100;

    // Like the C version xml_html_parse_settings_default.
    private static final int MAX_TEXT_LENGTH = 10000000;

    private static final byte[] REPLACEMENT = { (byte)0xEF, (byte)0xBF, (byte)0xBD };
    private static final byte[] NEWLINE = { '\n' };
    private static final byte[] NOTHING = { };

    /** A growable byte buffer. */
    private static final class Bytes
    {
        byte[] b = new byte[64];
        int len;

        void add(byte[] s, int off, int n)
        {
            if (len + n > b.length) b = java.util.Arrays.copyOf(b, Math.max(b.length * 2, len + n));
            System.arraycopy(s, off, b, len, n);
            len += n;
        }

        void add(int c)
        {
            if (len == b.length) b = java.util.Arrays.copyOf(b, b.length * 2);
            b[len++] = (byte)c;
        }

        void clear()
        {
            len = 0;
        }

        String str()
        {
            return new String(b, 0, len, StandardCharsets.UTF_8);
        }
    }

    /** An attribute of a start tag, value null if it has no value. */
    private record Attr(String name, String value) { }

    private final byte[] buf;
    private final int len;
    private int i;
    private final boolean no_blanks;

    private final XmlDocument doc;
    // The name stack (libxml2 nameTab) and node stack (nodeTab).
    private final List<XmlElement> nodes = new ArrayList<>();
    private final List<String> names = new ArrayList<>();
    private int insert_mode = INSERT_INITIAL;
    private int depth;            // Misplaced html/head/body start tags that were discarded.
    private int end_check_state;  // The raw text mode of the current element.
    private boolean in_misc;      // Before the content, a doctype is only accepted here.

    // Pending character data, merged like libxml2 xmlSAX2Text does.
    private final Bytes text = new Bytes();
    private boolean has_text;
    private boolean text_is_cdata;
    private XmlElement text_parent;

    private final Bytes data = new Bytes();  // Result of parseData.
    private final Bytes name = new Bytes();  // Result of parseName.

    private HtmlParser(byte[] buf, boolean no_blanks)
    {
        this.buf = buf;
        this.len = buf.length;
        this.no_blanks = no_blanks;
        doc = new XmlDocument(true);
        // Like htmlNewDocNoDtD (and the C version), printed as standalone="yes" when saved as xml.
        doc.setStandalone(1);
    }

    /**
     * Parse html content into a document, like libxml2 htmlReadMemory with the
     * options HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET.
     * @param content The html source.
     * @param noblanks If true then also HTML_PARSE_NOBLANKS is used, ie ignorable
     *        whitespace text nodes are dropped.
     * @return The html document, without a root element if there was no content.
     */
    public static XmlDocument parse(String content, boolean noblanks)
    {
        return parse(content == null ? new byte[0] : content.getBytes(StandardCharsets.UTF_8), noblanks);
    }

    /**
     * Parse html content (UTF-8 bytes, invalid sequences become U+FFFD) into a document.
     * @param content The html source.
     * @param noblanks If true then ignorable whitespace text nodes are dropped.
     * @return The html document, without a root element if there was no content.
     */
    public static XmlDocument parse(byte[] content, boolean noblanks)
    {
        HtmlParser p = new HtmlParser(content, noblanks);
        p.parseDocument();
        return p.doc;
    }

    /**
     * Check if an attribute is an html boolean attribute, like libxml2 htmlIsBooleanAttr.
     * @param name The attribute name.
     * @return True if the attribute is boolean (checked, selected...).
     */
    public static boolean isBooleanAttr(String name)
    {
        return XmlHtml.isBooleanAttr(name);
    }

    // Character classes ///////////////////////////////////////////////

    private static boolean isWs(int c)
    {
        return c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == 0x20;
    }

    private static boolean isLetter(int c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    private static boolean isDigit(int c)
    {
        return c >= '0' && c <= '9';
    }

    private static boolean isHex(int c)
    {
        return isDigit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
    }

    private static int upper(int c)
    {
        return (c >= 'a' && c <= 'z') ? c - 32 : c;
    }

    /** Port of htmlValidateUtf8: the length of a valid sequence at off, or -1. */
    private static int validateUtf8(byte[] str, int off, int avail)
    {
        int c = str[off] & 0xff;
        if (c < 0xC2) return -1;
        if (c < 0xE0)
        {
            if (avail < 2 || (str[off + 1] & 0xC0) != 0x80) return -1;
            return 2;
        }
        if (c < 0xF0)
        {
            if (avail < 3) return -1;
            int v = c << 16 | (str[off + 1] & 0xff) << 8 | (str[off + 2] & 0xff);
            if (((v & 0x00C0C0) != 0x008080) || ((v & 0x0F2000) == 0x000000) || ((v & 0x0F2000) == 0x0D2000)) return -1;
            return 3;
        }
        if (avail < 4) return -1;
        long v = (long)c << 24 | (str[off + 1] & 0xff) << 16 | (str[off + 2] & 0xff) << 8 | (str[off + 3] & 0xff);
        if (((v & 0x00C0C0C0L) != 0x00808080L) || (v < 0xF0900000L) || (v >= 0xF4900000L)) return -1;
        return 4;
    }

    private static final int[] C1_REMAP = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
    };

    /** Port of htmlCodePointToUtf8. */
    private static byte[] codePointToUtf8(int c)
    {
        if (c >= 0x80 && c < 0xA0) c = C1_REMAP[c - 0x80];
        else if (c <= 0 || (c >= 0xD800 && c < 0xE000) || c > 0x10FFFF) c = 0xFFFD;
        return new String(Character.toChars(c)).getBytes(StandardCharsets.UTF_8);
    }

    // Input helpers ///////////////////////////////////////////////////

    private int cur()
    {
        return i < len ? buf[i] & 0xff : 0;
    }

    private int nxt(int n)
    {
        return i + n < len ? buf[i + n] & 0xff : 0;
    }

    private int upp(int n)
    {
        return upper(nxt(n));
    }

    private void skipBlanks()
    {
        while (i < len && isWs(buf[i] & 0xff)) i++;
    }

    // Tree building (the libxml2 SAX2 callbacks for html) ////////////

    private XmlElement current()
    {
        return nodes.isEmpty() ? null : nodes.get(nodes.size() - 1);
    }

    private String currentName()
    {
        return names.isEmpty() ? null : names.get(names.size() - 1);
    }

    private void flushText()
    {
        if (!has_text) return;
        String s = text.str();
        text_parent.appendRaw(text_is_cdata ? new XmlCData(s) : new XmlText(s));
        has_text = false;
        text.clear();
    }

    private static boolean isKind(XmlNode n, boolean cdata)
    {
        return n.type() == (cdata ? XmlNodeType.CDATA : XmlNodeType.TEXT);
    }

    /** xmlSAX2Text: append text or cdata to the current node, merging with the previous node of the same kind. */
    private void addText(byte[] s, int off, int n, boolean cdata)
    {
        XmlElement parent = current();
        if (parent == null) return; // Text outside of any element is dropped.
        if (has_text && (text_parent != parent || text_is_cdata != cdata)) flushText();
        if (!has_text)
        {
            text.clear();
            // libxml2 also merges with an existing last child of the same kind.
            XmlNode last = parent.lastChild();
            if (last != null && isKind(last, cdata))
            {
                if (last.content() != null)
                {
                    byte[] c = last.content().getBytes(StandardCharsets.UTF_8);
                    text.add(c, 0, c.length);
                }
                last.unlink();
            }
            has_text = true;
            text_parent = parent;
            text_is_cdata = cdata;
        }
        text.add(s, off, n);
    }

    private void characters(byte[] s, int off, int n)
    {
        addText(s, off, n, false);
    }

    private void append(XmlNode n)
    {
        flushText();
        XmlNode parent = current();
        if (parent == null) parent = doc;
        parent.appendRaw(n);
    }

    private void comment(String s)
    {
        append(new XmlComment(s));
    }

    /** htmlnamePush + startElement (xmlSAX2StartHtmlElement). */
    private void startElement(String tag, List<Attr> atts)
    {
        if (insert_mode < INSERT_IN_HEAD && tag.equals("head")) insert_mode = INSERT_IN_HEAD;
        if (insert_mode < INSERT_IN_BODY && tag.equals("body")) insert_mode = INSERT_IN_BODY;

        XmlElement e = new XmlElement(tag);
        append(e);

        if (atts != null)
        {
            for (Attr a : atts)
            {
                String value = a.value();
                if (value == null && XmlHtml.isBooleanAttr(a.name())) value = a.name();
                XmlAttr attr = new XmlAttr(a.name(), null);
                // An empty value is an empty text node, which is written as name="".
                if (value != null) attr.appendRaw(new XmlText(value));
                e.addAttribute(attr);
            }
        }

        nodes.add(e);
        names.add(tag);
    }

    /** endElement + htmlnamePop. */
    private void endElement()
    {
        if (nodes.isEmpty()) return;
        flushText();
        nodes.remove(nodes.size() - 1);
        names.remove(names.size() - 1);
    }

    /** xmlSAX2InternalSubset: only before the content, the dtd is put first like in a libxml2 html document. */
    private void internalSubset(String dtd_name, String pub, String sys)
    {
        if (!in_misc) return;
        doc.setIntSubset(new XmlDtd(dtd_name, pub, sys));
    }

    // Implied elements and implicit closing /////////////////////////

    /** Port of htmlAutoClose. */
    private void autoClose(String newtag)
    {
        while (currentName() != null && XmlHtml.startCloses(newtag, currentName()))
        {
            endElement();
        }
    }

    /** Port of htmlAutoCloseOnClose. */
    private void autoCloseOnClose(String newtag)
    {
        int priority = XmlHtml.endPriority(newtag);
        int k;
        for (k = names.size(); k > 0; --k)
        {
            if (newtag.equals(names.get(k - 1))) break;
            // A misplaced endtag can only close elements with lower or equal priority.
            if (XmlHtml.endPriority(names.get(k - 1)) > priority) return;
        }
        if (k == 0) return;
        while (!newtag.equals(currentName()))
        {
            endElement();
        }
    }

    private void autoCloseOnEnd()
    {
        while (!nodes.isEmpty()) endElement();
    }

    /** Port of htmlCheckImplied. */
    private void checkImplied(String newtag)
    {
        if (newtag.equals("html")) return;
        if (nodes.isEmpty()) startElement("html", null);
        if (newtag.equals("body") || newtag.equals("head")) return;
        if (nodes.size() <= 1 &&
            (newtag.equals("script") || newtag.equals("style") || newtag.equals("meta") ||
             newtag.equals("link") || newtag.equals("title") || newtag.equals("base")))
        {
            if (insert_mode >= INSERT_IN_HEAD) return; // We already saw or generated a head.
            startElement("head", null);
        }
        else if (!newtag.equals("noframes") && !newtag.equals("frame") && !newtag.equals("frameset"))
        {
            if (insert_mode >= INSERT_IN_BODY) return; // We already saw or generated a body.
            for (String n : names)
            {
                if (n.equals("body")) return;
                if (n.equals("head")) return;
            }
            startElement("body", null);
        }
    }

    /** Port of htmlStartCharData: prepare for non whitespace character data. */
    private void startCharData()
    {
        String n = currentName();
        if (n != null && n.equals("head")) autoClose("p");
        checkImplied("p");
    }

    // Character data ////////////////////////////////////////////////

    /** Port of the html areBlanks: 1 if ignorable, 0 if whitespace to keep, -1 if not whitespace. */
    private int areBlanks(byte[] s, int off, int n)
    {
        for (int j = 0; j < n; ++j) if (!isWs(s[off + j] & 0xff)) return -1;
        int c = cur();
        if (c == 0) return 1;
        if (c != '<') return 0;
        String cn = currentName();
        if (cn == null) return 1;
        if (cn.equals("html")) return 1;
        if (cn.equals("head")) return 1;

        // Only strip CDATA children of the body tag for strict HTML DTDs.
        if (cn.equals("body"))
        {
            XmlDtd dtd = doc.intSubset();
            String pub = dtd != null ? dtd.externalId() : null;
            if (pub != null)
            {
                if (pub.equalsIgnoreCase("-//W3C//DTD HTML 4.01//EN") || pub.equalsIgnoreCase("-//W3C//DTD HTML 4//EN")) return 1;
            }
        }

        XmlElement node = current();
        if (node == null) return 0;
        // The last child as libxml2 sees it, pending text included, comments skipped.
        XmlNodeType last;
        String last_name = null;
        if (has_text && text_parent == node)
        {
            last = text_is_cdata ? XmlNodeType.CDATA : XmlNodeType.TEXT;
        }
        else
        {
            XmlNode l = node.lastChild();
            while (l != null && l.type() == XmlNodeType.COMMENT) l = l.prev();
            last = l == null ? null : l.type();
            if (l != null) last_name = l.name();
        }
        if (last == null)
        {
            // Keep ws in constructs like ...<b> </b>... for all tags "b" allowing PCDATA.
            if (XmlHtml.allowsPcdata(cn)) return 0;
        }
        else if (last == XmlNodeType.TEXT)
        {
            return 0;
        }
        else
        {
            // Keep ws in constructs like <p><b>xy</b> <i>z</i><p> for all tags "p" allowing PCDATA.
            if (last_name != null && XmlHtml.allowsPcdata(last_name)) return 0;
        }
        return 1;
    }

    /** Port of htmlCharDataSAXCallback. i is after the data. */
    private void charDataCallback(byte[] s, int off, int size, int mode)
    {
        if (mode == 0 || mode == XmlHtml.DATA_RCDATA)
        {
            String cn = currentName();
            if (cn == null || cn.equals("html") || cn.equals("head"))
            {
                // Add leading whitespace to html or head elements before calling htmlStartCharData.
                int k;
                for (k = 0; k < size; ++k) if (!isWs(s[off + k] & 0xff)) break;
                if (k > 0)
                {
                    if (!no_blanks) characters(s, off, k);
                    off += k;
                    size -= k;
                }
                if (size == 0) return;
                startCharData();
            }
            if (mode == 0 && no_blanks && areBlanks(s, off, size) > 0)
            {
                return; // Ignorable whitespace.
            }
            characters(s, off, size);
        }
        else
        {
            // Raw text is inserted as cdata (HTML_PRESERVE_NODE).
            addText(s, off, size, true);
        }
    }

    /** The replacement of a character reference or entity found while scanning data. */
    private static final class Scan
    {
        byte[] repl = NOTHING;
        int skip;
        int ncr;       // 10 or 16 while reading a numeric character reference.
        int ncr_size;
        int cp;
    }

    /** Start a numeric character reference or find a named one at in (an &amp;). Returns the
     * size to advance, or -1 if a named reference was found (scan.repl and scan.skip are set). */
    private int scanRef(int in, int avail, boolean is_attr, Scan scan)
    {
        int j = 1;
        if (j < avail && buf[in + j] == '#')
        {
            j += 1;
            if (j < avail)
            {
                if ((buf[in + j] | 0x20) == 'x')
                {
                    j += 1;
                    if (j < avail && isHex(buf[in + j] & 0xff))
                    {
                        scan.ncr = 16;
                        scan.ncr_size = 3;
                        scan.cp = 0;
                        return 3;
                    }
                }
                else if (isDigit(buf[in + j] & 0xff))
                {
                    scan.ncr = 10;
                    scan.ncr_size = 2;
                    scan.cp = 0;
                    return 2;
                }
            }
        }
        else
        {
            int[] nlen = new int[1];
            byte[] r = XmlHtml.findEntityPrefix(buf, in + j, avail - j, is_attr, nlen);
            if (r != null)
            {
                scan.repl = r;
                scan.skip = nlen[0] + 1;
                return -1;
            }
        }
        return 1;
    }

    /** Port of htmlParseCharData (partial == 0). */
    private void parseCharData()
    {
        int mode = end_check_state;
        boolean complete = false;
        boolean done = false;

        while (true)
        {
            int chunk = i;
            int in = chunk;
            Scan scan = new Scan();

            scanning:
            while (true)
            {
                if (in >= len)
                {
                    done = true;
                    break;
                }
                int c = buf[in] & 0xff;
                int size = 1;
                int avail = len - in;

                if (scan.ncr != 0)
                {
                    int lc = c | 0x20;
                    int digit;
                    if (c >= '0' && c <= '9') digit = c - '0';
                    else if (scan.ncr == 16 && lc >= 'a' && lc <= 'f') digit = lc - 'a' + 10;
                    else
                    {
                        if (c == ';')
                        {
                            in += 1;
                            scan.ncr_size += 1;
                        }
                        break;
                    }
                    scan.cp = scan.cp * scan.ncr + digit;
                    if (scan.cp >= 0x110000) scan.cp = 0x110000;
                    scan.ncr_size += 1;
                    in += size;
                    continue;
                }

                switch (c)
                {
                case '<':
                    if (mode == 0)
                    {
                        done = true;
                        complete = true;
                        break scanning;
                    }
                    if (mode == XmlHtml.DATA_PLAINTEXT) break;
                    {
                        int j = 1;
                        if (j < avail)
                        {
                            if (mode == XmlHtml.DATA_SCRIPT && buf[in + j] == '!')
                            {
                                // Check for comment start.
                                j += 1;
                                if (j < avail && buf[in + j] == '-')
                                {
                                    j += 1;
                                    if (j < avail && buf[in + j] == '-') mode = XmlHtml.DATA_SCRIPT_ESC1;
                                }
                            }
                            else
                            {
                                String cn = currentName();
                                boolean solidus = false;
                                // Check for tag.
                                if (buf[in + j] == '/')
                                {
                                    j += 1;
                                    solidus = true;
                                }
                                if ((solidus || mode == XmlHtml.DATA_SCRIPT_ESC1) && cn != null)
                                {
                                    byte[] nb = cn.getBytes(StandardCharsets.UTF_8);
                                    int ni = 0;
                                    while (j < avail && ni < nb.length && (nb[ni] & 0xff) == ((buf[in + j] & 0xff) | 0x20))
                                    {
                                        ni++;
                                        j++;
                                    }
                                    if (ni == nb.length && j < avail)
                                    {
                                        int t = buf[in + j] & 0xff;
                                        if (t == '>' || t == '/' || isWs(t))
                                        {
                                            if (mode == XmlHtml.DATA_SCRIPT_ESC1 && !solidus)
                                            {
                                                mode = XmlHtml.DATA_SCRIPT_ESC2;
                                            }
                                            else if (mode == XmlHtml.DATA_SCRIPT_ESC2)
                                            {
                                                mode = XmlHtml.DATA_SCRIPT_ESC1;
                                            }
                                            else
                                            {
                                                complete = true;
                                                done = true;
                                                break scanning;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;

                case '-':
                    if (mode != XmlHtml.DATA_SCRIPT_ESC1 && mode != XmlHtml.DATA_SCRIPT_ESC2) break;
                    // Check for comment end.
                    if (1 < avail && buf[in + 1] == '-')
                    {
                        if (2 < avail && buf[in + 2] == '>') mode = XmlHtml.DATA_SCRIPT;
                    }
                    break;

                case '&':
                    if (mode != 0 && mode != XmlHtml.DATA_RCDATA) break;
                    size = scanRef(in, avail, false, scan);
                    if (size < 0) break scanning;
                    break;

                case 0:
                    scan.skip = 1;
                    // The tokenizer strips U+0000 in normal data mode.
                    scan.repl = mode == 0 ? NOTHING : REPLACEMENT;
                    break scanning;

                case '\r':
                    scan.skip = 1;
                    if (in + 1 >= len || buf[in + 1] != 0x0A) scan.repl = NEWLINE;
                    break scanning;

                default:
                    if (c < 0x80) break;
                    {
                        int vs = validateUtf8(buf, in, avail);
                        if (vs <= 0)
                        {
                            scan.skip = 1;
                            scan.repl = REPLACEMENT;
                            break scanning;
                        }
                        size = vs;
                    }
                    break;
                }
                in += size;
            }

            // next_chunk:
            if (scan.ncr_size > 0)
            {
                scan.skip = scan.ncr_size;
                in -= scan.ncr_size;
                scan.repl = codePointToUtf8(scan.cp);
            }
            if (in > chunk)
            {
                i = in;
                charDataCallback(buf, chunk, in - chunk, mode);
            }
            i = in + scan.skip;
            if (scan.repl.length > 0) charDataCallback(scan.repl, 0, scan.repl.length, mode);
            if (done) break;
        }

        end_check_state = complete ? 0 : mode;
    }

    // Data: attribute values, comments, doctype literals /////////////

    private static final int MASK_DQ = 1;
    private static final int MASK_SQ = 2;
    private static final int MASK_GT = 4;
    private static final int MASK_DASH = 8;
    private static final int MASK_WS_GT = 16;
    private static final int MASK_DQ_GT = 32;
    private static final int MASK_SQ_GT = 64;

    private static boolean maskMatch(int mask, int c)
    {
        switch (mask)
        {
        case MASK_DQ: return c == '"';
        case MASK_SQ: return c == '\'';
        case MASK_GT: return c == '>';
        case MASK_DASH: return c == '-';
        case MASK_WS_GT: return c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == ' ' || c == '>';
        case MASK_DQ_GT: return c == '"' || c == '>';
        case MASK_SQ_GT: return c == '\'' || c == '>';
        default: return false;
        }
    }

    /** Port of htmlParseData. The result is in data. Returns false if the value is too long. */
    private boolean parseData(int mask, boolean is_comment, boolean refs, int max_length)
    {
        data.clear();
        int term_skip = -1;

        while (true)
        {
            int chunk = i;
            int in = chunk;
            Scan scan = new Scan();

            scanning:
            while (true)
            {
                if (in >= len)
                {
                    term_skip = 0;
                    break;
                }
                int avail = len - in;
                int c = buf[in] & 0xff;
                int size = 1;

                if (maskMatch(mask, c))
                {
                    if (is_comment)
                    {
                        if (avail < 2) term_skip = 1;
                        else if (buf[in + 1] == '-')
                        {
                            if (avail < 3) term_skip = 2;
                            else if (buf[in + 2] == '>') term_skip = 3;
                            else if (buf[in + 2] == '!')
                            {
                                if (avail < 4) term_skip = 3;
                                else if (buf[in + 3] == '>') term_skip = 4;
                            }
                        }
                        if (term_skip >= 0) break;
                    }
                    else
                    {
                        term_skip = 0;
                        break;
                    }
                }

                if (scan.ncr != 0)
                {
                    int lc = c | 0x20;
                    int digit;
                    if (c >= '0' && c <= '9') digit = c - '0';
                    else if (scan.ncr == 16 && lc >= 'a' && lc <= 'f') digit = lc - 'a' + 10;
                    else
                    {
                        if (c == ';')
                        {
                            in += 1;
                            scan.ncr_size += 1;
                        }
                        break;
                    }
                    scan.cp = scan.cp * scan.ncr + digit;
                    if (scan.cp >= 0x110000) scan.cp = 0x110000;
                    scan.ncr_size += 1;
                    in += size;
                    continue;
                }

                switch (c)
                {
                case '&':
                    if (!refs) break;
                    size = scanRef(in, avail, true, scan);
                    if (size < 0) break scanning;
                    break;
                case 0:
                    scan.skip = 1;
                    scan.repl = REPLACEMENT;
                    break scanning;
                case '\r':
                    scan.skip = 1;
                    if (in + 1 >= len || buf[in + 1] != 0x0A) scan.repl = NEWLINE;
                    break scanning;
                default:
                    if (c < 0x80) break;
                    {
                        int vs = validateUtf8(buf, in, avail);
                        if (vs <= 0)
                        {
                            scan.skip = 1;
                            scan.repl = REPLACEMENT;
                            break scanning;
                        }
                        size = vs;
                    }
                    break;
                }
                in += size;
            }

            // next_chunk:
            if (scan.ncr_size > 0)
            {
                scan.skip = scan.ncr_size;
                in -= scan.ncr_size;
                scan.repl = codePointToUtf8(scan.cp);
            }
            int chunk_size = in - chunk;
            if ((long)chunk_size + scan.repl.length > max_length - data.len)
            {
                return false; // value too long
            }
            if (chunk_size > 0) data.add(buf, chunk, chunk_size);
            i = in + scan.skip;
            if (scan.repl.length > 0) data.add(scan.repl, 0, scan.repl.length);
            if (term_skip >= 0) break;
        }
        if (term_skip > 0) i += term_skip;
        return true;
    }

    /** Port of htmlParseHTMLName: lowercased, truncated to 100 bytes. */
    private String parseName(boolean attr)
    {
        int stop = attr ? '=' : ' ';
        name.clear();
        int nbchar = 0;
        while (i < len)
        {
            int c = buf[i] & 0xff;
            int size = 1;
            if (nbchar != 0 && (c == '/' || c == '>' || c == stop || isWs(c))) break;
            if (c == 0)
            {
                if (nbchar + 3 <= MAX_NAME)
                {
                    name.add(REPLACEMENT, 0, 3);
                    nbchar += 3;
                }
            }
            else if (c < 0x80)
            {
                if (nbchar < MAX_NAME)
                {
                    name.add((c >= 'A' && c <= 'Z') ? c + 0x20 : c);
                    nbchar++;
                }
            }
            else
            {
                int vs = validateUtf8(buf, i, len - i);
                if (vs > 0)
                {
                    size = vs;
                    if (nbchar + size <= MAX_NAME)
                    {
                        name.add(buf, i, size);
                        nbchar += size;
                    }
                }
                else
                {
                    size = 1;
                    if (nbchar + 3 <= MAX_NAME)
                    {
                        name.add(REPLACEMENT, 0, 3);
                        nbchar += 3;
                    }
                }
            }
            i += size;
        }
        return name.str();
    }

    /** Port of htmlParseAttValue. Returns the value, or null if too long. */
    private String parseAttValue()
    {
        boolean ok;
        if (cur() == '"')
        {
            i++;
            ok = parseData(MASK_DQ, false, true, MAX_TEXT_LENGTH);
            if (cur() == '"') i++;
        }
        else if (cur() == '\'')
        {
            i++;
            ok = parseData(MASK_SQ, false, true, MAX_TEXT_LENGTH);
            if (cur() == '\'') i++;
        }
        else
        {
            ok = parseData(MASK_WS_GT, false, true, MAX_TEXT_LENGTH);
        }
        return ok ? data.str() : null;
    }

    /** Port of htmlParseComment. */
    private void parseComment(boolean bogus)
    {
        if (bogus)
        {
            boolean ok = parseData(MASK_GT, false, false, MAX_TEXT_LENGTH);
            if (cur() == '>') i++;
            if (ok) comment(data.str());
            return;
        }
        if (cur() == '>')
        {
            i++;
            comment("");
        }
        else if (cur() == '-' && nxt(1) == '>')
        {
            i += 2;
            comment("");
        }
        else
        {
            boolean ok = parseData(MASK_DASH, true, false, MAX_TEXT_LENGTH);
            if (ok) comment(data.str());
        }
    }

    /** Port of htmlParseDoctypeLiteral. Returns the literal or null. */
    private String parseDoctypeLiteral()
    {
        boolean ok;
        if (cur() == '"')
        {
            i++;
            ok = parseData(MASK_DQ_GT, false, false, 50000);
            if (cur() == '"') i++;
        }
        else if (cur() == '\'')
        {
            i++;
            ok = parseData(MASK_SQ_GT, false, false, 50000);
            if (cur() == '\'') i++;
        }
        else
        {
            return null;
        }
        return ok ? data.str() : null;
    }

    /** Port of htmlParseDocTypeDecl. */
    private void parseDoctype()
    {
        String dtd_name = null, pub = null, sys = null;
        i += 9;
        skipBlanks();
        if (i < len && cur() != '>')
        {
            if (parseData(MASK_WS_GT, false, false, 50000)) dtd_name = data.str();
            skipBlanks();
        }
        if (upp(0) == 'P' && upp(1) == 'U' && upp(2) == 'B' && upp(3) == 'L' && upp(4) == 'I' && upp(5) == 'C')
        {
            i += 6;
            skipBlanks();
            pub = parseDoctypeLiteral();
            if (pub != null)
            {
                skipBlanks();
                sys = parseDoctypeLiteral();
            }
        }
        else if (upp(0) == 'S' && upp(1) == 'Y' && upp(2) == 'S' && upp(3) == 'T' && upp(4) == 'E' && upp(5) == 'M')
        {
            i += 6;
            skipBlanks();
            sys = parseDoctypeLiteral();
        }
        // htmlSkipBogusDoctype: skip to and over the next >.
        while (i < len)
        {
            if (buf[i++] == '>') break;
        }
        internalSubset(dtd_name, pub, sys);
    }

    // Tags ///////////////////////////////////////////////////////////

    /** Port of htmlParseAttribute. */
    private Attr parseAttribute()
    {
        String attname = parseName(true);
        String value = null;
        skipBlanks();
        if (cur() == '=')
        {
            i++;
            skipBlanks();
            value = parseAttValue();
        }
        return new Attr(attname, value);
    }

    /** Port of htmlParseStartTag. */
    private void parseStartTag()
    {
        boolean discard = false;
        end_check_state = 0;
        i++; // <
        String tag = parseName(false);

        // Check for auto-closure of HTML elements.
        autoClose(tag);
        // Check for implied HTML elements.
        checkImplied(tag);

        // Avoid html at any level > 0, head at any level != 1 or any attempt to recurse body.
        if (!nodes.isEmpty() && tag.equals("html"))
        {
            discard = true;
            depth++;
        }
        if (nodes.size() != 1 && tag.equals("head"))
        {
            discard = true;
            depth++;
        }
        if (tag.equals("body"))
        {
            for (String n : names)
            {
                if (n.equals("body"))
                {
                    discard = true;
                    depth++;
                }
            }
        }

        // Parse the attributes.
        List<Attr> atts = new ArrayList<>();
        skipBlanks();
        while (i < len && cur() != '>' && !(cur() == '/' && nxt(1) == '>'))
        {
            // unexpected-solidus-in-tag
            if (cur() == '/')
            {
                i++;
                skipBlanks();
                continue;
            }
            atts.add(parseAttribute());
            skipBlanks();
        }

        if (i >= len)
        {
            discard = true;
        }
        else if (atts.size() > 1)
        {
            // Attribute names must be unique, the first one wins.
            Set<String> seen = new HashSet<>();
            List<Attr> unique = new ArrayList<>();
            for (Attr a : atts)
            {
                if (seen.add(a.name())) unique.add(a);
            }
            atts = unique;
        }

        if (!discard) startElement(tag, atts);
    }

    /** Port of htmlParseEndTag. */
    private void parseEndTag()
    {
        end_check_state = 0;
        i += 2;
        if (i >= len)
        {
            startCharData();
            byte[] lt = { '<', '/' };
            characters(lt, 0, 2);
            return;
        }
        if (cur() == '>')
        {
            i++;
            return;
        }
        if (!isLetter(cur()))
        {
            parseComment(true);
            return;
        }

        String tag = parseName(false);

        // Parse and ignore attributes.
        skipBlanks();
        while (i < len && cur() != '>' && !(cur() == '/' && nxt(1) == '>'))
        {
            if (cur() == '/')
            {
                i++;
                skipBlanks();
                continue;
            }
            parseAttribute();
            skipBlanks();
        }

        if (cur() == '>' && i < len)
        {
            i++;
        }
        else if (cur() == '/' && nxt(1) == '>')
        {
            i += 2;
        }
        else
        {
            return;
        }

        // If we ignored misplaced tags in htmlParseStartTag don't pop them out now.
        if (depth > 0 && (tag.equals("html") || tag.equals("body") || tag.equals("head")))
        {
            depth--;
            return;
        }

        // If the name read is not one of the element in the parsing stack then return, it's just an error.
        if (!names.contains(tag)) return;

        // Check for auto-closure of HTML elements.
        autoCloseOnClose(tag);

        String cn = currentName();
        if (cn != null && cn.equals(tag)) endElement();
    }

    /** Port of htmlParseElementInternal. */
    private void parseElement()
    {
        parseStartTag();
        // Note: like libxml2 this is the current name, which is the parent if the start tag was discarded.
        String cn = currentName();
        if (cn == null) return;

        // Check for an Empty Element labeled the XML/SGML way.
        if (cur() == '/' && nxt(1) == '>' && i < len)
        {
            i += 2;
            endElement();
            return;
        }
        if (cur() != '>' || i >= len) return;
        i++;

        // Check for an Empty Element from DTD definition.
        XmlHtml.Element info = XmlHtml.element(cn);
        if (info != null && info.empty())
        {
            endElement();
            return;
        }
        if (info != null) end_check_state = info.data_mode();
    }

    private boolean atDoctype()
    {
        return upp(2) == 'D' && upp(3) == 'O' && upp(4) == 'C' && upp(5) == 'T' && upp(6) == 'Y' && upp(7) == 'P' && upp(8) == 'E';
    }

    /** Port of htmlParseContent. */
    private void parseContent()
    {
        while (i < len)
        {
            int mode = end_check_state;
            if (mode == 0 && cur() == '<')
            {
                int n = nxt(1);
                if (n == '/')
                {
                    parseEndTag();
                }
                else if (n == '!')
                {
                    // Sometimes DOCTYPE arrives in the middle of the document.
                    if (atDoctype())
                    {
                        parseDoctype();
                    }
                    else if (nxt(2) == '-' && nxt(3) == '-')
                    {
                        i += 4;
                        parseComment(false);
                    }
                    else
                    {
                        i += 2;
                        parseComment(true);
                    }
                }
                else if (n == '?')
                {
                    i += 1;
                    parseComment(true);
                }
                else if (isLetter(n))
                {
                    parseElement();
                }
                else
                {
                    startCharData();
                    byte[] lt = { '<' };
                    characters(lt, 0, 1);
                    i += 1;
                }
            }
            else
            {
                parseCharData();
            }
        }
        autoCloseOnEnd();
    }

    /** Port of htmlParseDocument. */
    private void parseDocument()
    {
        // Like libxml2 xmlDetectEncoding with the encoding already set (libxmq gives UTF-8): a bom
        // is skipped, but only if the input has at least four bytes. Otherwise it is the
        // character U+FEFF in the content.
        if (len >= 4 && (buf[0] & 0xff) == 0xEF && (buf[1] & 0xff) == 0xBB && (buf[2] & 0xff) == 0xBF) i = 3;

        // Wipe out everything which is before the first '<'.
        skipBlanks();

        // Parse possible comments, PIs or doctype declarations before any content.
        in_misc = true;
        while (cur() == '<' && i < len)
        {
            if (nxt(1) == '!')
            {
                if (nxt(2) == '-' && nxt(3) == '-')
                {
                    i += 4;
                    parseComment(false);
                }
                else if (atDoctype())
                {
                    parseDoctype();
                    in_misc = false;
                }
                else
                {
                    i += 2;
                    parseComment(true);
                }
            }
            else if (nxt(1) == '?')
            {
                i += 1;
                parseComment(true);
            }
            else
            {
                break;
            }
            skipBlanks();
        }
        in_misc = false;

        // Time to start parsing the tree itself.
        parseContent();
        flushText();

        // Like xmlSAX2EndDocument: a default doctype if none was given.
        if (doc.intSubset() == null)
        {
            in_misc = true;
            internalSubset("html", "-//W3C//DTD HTML 4.0 Transitional//EN", "http://www.w3.org/TR/REC-html40/loose.dtd");
            in_misc = false;
        }
    }
}
