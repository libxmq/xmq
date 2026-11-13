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
 * Whitespace trimming of a DOM loaded from xml or html, exactly like the C
 * version of xmq does after reading xml/html (xmqTrimWhitespace and
 * xmq_fixup_comments_after_readin in xmq.c). All whitespace only text nodes
 * are removed and the incidental indentation is removed from the remaining
 * text and comment nodes.
 */
public final class XmlTrim
{
    private XmlTrim()
    {
    }

    /**
     * Trim the whitespace of all text and comment nodes in the document,
     * like the C function xmqTrimWhitespace.
     * @param doc The document to trim.
     */
    public static void trimWhitespace(XmlDocument doc)
    {
        XmlNode i = doc.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            trim_node(i);
            i = next;
        }
    }

    static void trim_node(XmlNode node)
    {
        if (node.type() == XmlNodeType.TEXT || node.type() == XmlNodeType.CDATA)
        {
            trim_text_node(node, false);
            return;
        }
        if (node.type() == XmlNodeType.COMMENT)
        {
            trim_text_node(node, true);
            return;
        }
        if (node.type() == XmlNodeType.ENTITY_DECL) return;
        XmlNode i = node.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            trim_node(i);
            i = next;
        }
    }

    static boolean is_all_xml_whitespace(String s)
    {
        for (int k = 0; k < s.length(); k++)
        {
            if (!is_xml_ws(s.charAt(k))) return false;
        }
        return true;
    }

    static void trim_text_node(XmlNode node, boolean is_comment)
    {
        String content = node.content();
        if (content == null) content = "";
        if (is_all_xml_whitespace(content))
        {
            node.unlink();
            return;
        }
        int start = 0;
        int stop = content.length();
        if (is_comment)
        {
            // For comments we always remove leading and ending spaces.
            while (start < stop && content.charAt(start) == ' ') start++;
            while (stop > start && content.charAt(stop - 1) == ' ') stop--;
        }
        String trimmed = trim_xml_text(content.substring(start, stop));
        if (trimmed.isEmpty())
        {
            node.unlink();
            return;
        }
        node.setContent(trimmed);
    }

    /**
     * Remove the dle escapes from comments, like the C function
     * xmq_fixup_comments_after_readin. An xml comment containing -\u2410-
     * (a dle escaped double dash) is replaced with --.
     * @param doc The document to fix.
     */
    public static void fixupCommentsAfterReadin(XmlDocument doc)
    {
        XmlNode i = doc.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            fixup_comments(i);
            i = next;
        }
    }

    static void fixup_comments(XmlNode node)
    {
        if (node.type() == XmlNodeType.COMMENT)
        {
            String c = node.content();
            if (c != null && c.contains("-\u2410"))
            {
                node.setContent(c.replace("-\u2410", "-"));
            }
            return;
        }
        if (node.type() == XmlNodeType.ENTITY_DECL) return;
        XmlNode i = node.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            fixup_comments(i);
            i = next;
        }
    }

    /**
     * Prepare a document for writing as xml or html, like the C function
     * xmq_fixup_html_before_writeout: comments containing -- are escaped
     * with the dle char, cdata sections become text and entity references
     * inside style and script elements are replaced by text.
     * @param doc The document to fix.
     */
    public static void fixupHtmlBeforeWriteout(XmlDocument doc)
    {
        XmlNode i = doc.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            fixup_html(doc, i, false);
            i = next;
        }
    }

    static void fixup_html(XmlDocument doc, XmlNode node, boolean inside_cdata_declared)
    {
        if (node.type() == XmlNodeType.COMMENT)
        {
            String c = node.content();
            if (c != null)
            {
                String e = escape_xml_comment(c);
                if (e != null) node.replaceWith(new XmlComment(e));
            }
            return;
        }
        else if (node.type() == XmlNodeType.CDATA)
        {
            XmlText t = new XmlText(node.content());
            node.replaceWith(t);
            node = t;
        }
        else if (node.type() == XmlNodeType.ENTITY_REF && inside_cdata_declared)
        {
            String content = null;
            XmlEntityDecl ent = doc.getEntity(node.name());
            if (ent != null) content = ent.content();
            if (content == null && node.name().startsWith("#"))
            {
                try
                {
                    content = String.valueOf((char)Integer.parseInt(node.name().substring(1)));
                }
                catch (NumberFormatException e)
                {
                    content = null;
                }
            }
            node.replaceWith(new XmlText(content == null ? "" : content));
            return;
        }

        XmlNode i = node.firstChild();
        while (i != null)
        {
            XmlNode next = i.next();
            boolean r = inside_cdata_declared;
            if (i.name() != null && (i.name().equalsIgnoreCase("style") || i.name().equalsIgnoreCase("script")))
            {
                // The html style and script nodes are declared as cdata nodes.
                r = true;
            }
            fixup_html(doc, i, r);
            i = next;
        }
    }

    /** When writing an xml comment we must replace -- with -\u2410-, an already existing -\u2410 becomes -\u2410\u2410. */
    static String escape_xml_comment(String c)
    {
        boolean needed = false;
        for (int k = 0; k + 1 < c.length(); k++)
        {
            if (c.charAt(k) == '-' && (c.charAt(k + 1) == '-' || c.charAt(k + 1) == '\u2410'))
            {
                needed = true;
                break;
            }
        }
        if (!needed) return null;
        StringBuilder sb = new StringBuilder();
        for (int k = 0; k < c.length(); k++)
        {
            char ch = c.charAt(k);
            sb.append(ch);
            if (ch == '-' && k + 1 < c.length() && (c.charAt(k + 1) == '-' || c.charAt(k + 1) == '\u2410'))
            {
                sb.append('\u2410');
            }
        }
        return sb.toString();
    }

    // ------------------------------------------------------------------
    //  Port of the C text-node trimming pipeline
    //  (xmq_trim_quote with is_xmq=false, is_comment=false, xmq.c),
    //  which the C xml parser applies to every text node after loading
    //  (trim_text_node -> xmq_un_quote).
    // ------------------------------------------------------------------

    static boolean is_xml_ws(char c)
    {
        return c == ' ' || c == '\n' || c == '\t' || c == '\r';
    }

    /** text.c has_leading_space_nl: does a leading whitespace run contain a newline? */
    static boolean has_leading_space_nl(String v, int start, int stop)
    {
        int i = start;
        while (i < stop && v.charAt(i) == '\n') i++;
        // Leading pure newlines count too (matches C: only_nls > 0 -> found_nl).
        boolean found = (i > start);
        while (i < stop)
        {
            if (v.charAt(i) == '\n') found = true;
            if (!is_xml_ws(v.charAt(i))) break;
            i++;
        }
        return found;
    }

    /** text.c has_ending_nl_space: does a trailing whitespace run contain a newline? */
    static boolean has_ending_nl_space(String v, int start, int stop)
    {
        int i = stop - 1;
        boolean found = false;
        while (i >= start && v.charAt(i) == '\n') { i--; found = true; }
        while (i >= start)
        {
            if (v.charAt(i) == '\n') found = true;
            if (!is_xml_ws(v.charAt(i))) break;
            i--;
        }
        return found;
    }

    /** xmq.c calculate_incidental_indent: -1 if no newline, else minimal leading spaces of content-lines. */
    static int calculate_incidental_indent(String v, int start, int stop)
    {
        int indent = -1;
        int i = start;
        while (i < stop && v.charAt(i) != '\n') i++;
        if (i >= stop) return -1;
        for (;;)
        {
            i++; // skip '\n'
            int line_start = i;
            while (i < stop && v.charAt(i) == ' ') i++;
            if (i >= stop) break;
            if (v.charAt(i) != '\n' && v.charAt(i) != '\r')
            {
                int ind = i - line_start;
                if (indent < 0 || ind < indent) indent = ind;
            }
            while (i < stop && v.charAt(i) != '\n') i++;
            if (i >= stop) break;
        }
        return indent;
    }

    static FLine find_line(String v, int start, int stop)
    {
        FLine r = new FLine();
        int i = start;
        int lnws = i;
        while (i < stop && (v.charAt(i) == ' ' || v.charAt(i) == '\t')) i++;
        while (i < stop)
        {
            char c = v.charAt(i);
            if (c == '\n' || c == '\r')
            {
                if (c == '\r' && i+1 < stop && v.charAt(i+1) == '\n') i++;
                i++;
                r.has_nl = true;
                break;
            }
            if (c != ' ' && c != '\t') lnws = i+1;
            i++;
        }
        r.after_last_ns = lnws;
        r.eol = i;
        return r;
    }

    /** xmq_trim_quote(v, is_xmq=false, is_comment=false): trim a text-node value. */
    /**
     * Trim a text value like the C xmq_trim_quote(v, is_xmq=false, is_comment=false).
     * @param v The text value.
     * @return The trimmed value.
     */
    public static String trim_xml_text(String v)
    {
        int start = 0;
        int stop = v.length();
        int append = 0;

        // Trailing whitespace run containing a newline: trim it.
        if (has_ending_nl_space(v, start, stop))
        {
            while (stop > start)
            {
                char c = v.charAt(stop-1);
                if (c == '\n') append++;
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
                stop--;
            }
        }
        // One ending newline is dropped, the rest are kept.
        if (append > 0) append--;

        if (stop == start)
        {
            // Value was all whitespace (caller drops the node anyway).
            StringBuilder b = new StringBuilder();
            for (int k = 0; k < append; k++) b.append('\n');
            return b.toString();
        }

        int incidental = calculate_incidental_indent(v, start, stop);
        if (incidental == -1)
        {
            // No newline anywhere: do not trim; re-add the ending newlines.
            StringBuilder b = new StringBuilder(v.substring(start, stop));
            for (int k = 0; k < append; k++) b.append('\n');
            return b.toString();
        }

        int prepend = 0;
        boolean first_line = true;

        // The first line's eol (used if the first line is all whitespace).
        FLine fl0 = find_line(v, start, stop);

        if (has_leading_space_nl(v, start, stop))
        {
            // No first line (leading space/newline): remove it and count blank lines.
            first_line = false;
            start = fl0.eol;
            int k = start;
            while (k < stop)
            {
                char c = v.charAt(k);
                if (c == '\n')
                {
                    start = k+1;
                    prepend++;
                }
                else if (c != '\r') break;
                k++;
            }
        }

        StringBuilder out = new StringBuilder();
        for (int k = 0; k < prepend; k++) out.append('\n');

        int i = start;
        while (i < stop)
        {
            FLine fl = find_line(v, i, stop);
            if (first_line)
            {
                first_line = false;
            }
            else
            {
                // Skip the incidental indentation (spaces only). Empty lines do not count.
                if (v.charAt(i) == ' ')
                {
                    int n = incidental;
                    while (n > 0)
                    {
                        if (v.charAt(i) != ' ') break;
                        i++;
                        n--;
                    }
                }
            }
            // Copy content but not the line-ending xml whitespace.
            while (i < fl.after_last_ns) out.append(v.charAt(i++));
            if (fl.has_nl)
            {
                out.append('\n');
            }
            else
            {
                // Final line without newline: any ending spaces are copied too.
                while (i < fl.eol) out.append(v.charAt(i++));
            }
            i = fl.eol;
        }
        for (int k = 0; k < append; k++) out.append('\n');
        return out.toString();
    }


}
