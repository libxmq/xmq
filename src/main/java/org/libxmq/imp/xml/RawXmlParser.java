/* libxmq - Copyright (C) 2025-2026 Fredrik Öhrström (spdx: MIT)

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

import org.jdom2.Attribute;
import org.jdom2.Comment;
import org.jdom2.Content;
import org.jdom2.DocType;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.EntityRef;
import org.jdom2.ProcessingInstruction;
import org.jdom2.Text;

import java.util.ArrayList;
import java.util.List;
import java.util.Stack;

/**
 * A SAX-style XML parser that never resolves entity references.
 *
 * The standard SAX API does not allow a driver to report an unresolved
 * entity reference: SAX mandates that character references and predefined
 * entities are delivered as characters, and that general entity references
 * are expanded into the entity's text. xmq needs the opposite: the entity
 * reference must survive parsing so that it can be printed back exactly as
 * source, ie. '&copy;' must not be replaced with the '©' character.
 *
 * This class therefore provides its own SAX-like handler interface (with
 * the same callback style: document events, element start/end, attributes,
 * character data, comments, processing instructions) plus a small scanner
 * that drives the handler from the raw source text. Numeric character
 * references like '&#169;' are still expanded to characters, exactly as SAX
 * does for them. Named references of predefined entities (amp, lt, gt,
 * quot, apos) are likewise expanded to characters anywhere. All other
 * named references are passed to the handler as entity-reference events.
 *
 * A handler that builds a JDOM2 document (JdomHandler) is included. It
 * deliberately does not use the JDOM2 SAX machinery: namespace
 * declarations like 'xmlns = "..."' or prefixed names like 'xml:lang' are
 * kept as plain strings, and entity references in element content become
 * EntityRef nodes.
 *
 * Whitespace-only text in element content is dropped, matching the C
 * parser which reads XML with XML_PARSE_NOBLANKS. CDATA sections are
 * treated as ordinary text, matching XML_PARSE_NOCDATA.
 */
public class RawXmlParser
{
    /** Default constructor. This is a static utility class. */
    RawXmlParser() {}

    /** Parse an XML string into a JDOM2 document without resolving
     *  general entity references. This is the XML to XMQ direction.
     *  HTML is auto-detected the same way C does: the first word after
     *  a '\u0026lt;' (or after a '\u0026lt;!doctype') is 'html'.
     *  @param content the XML (or HTML) source text
     *  @return the parsed JDOM2 document
     *  @throws Exception on malformed input */
    public static Document parse(String content) throws Exception
    {
        JdomHandler h = new JdomHandler(is_html(content));
        new Scanner(content, h, h.html).document();
        return h.finish();
    }

    /** The five predefined XML entities. */
    static char predefined_char(String name)
    {
        return switch (name)
        {
            case "amp" -> '&';
            case "lt" -> '<';
            case "gt" -> '>';
            case "quot" -> '"';
            case "apos" -> '\'';
            default -> 0;
        };
    }

    static boolean is_name_char(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || c == '_' || c == '.' || c == '-' || c == ':';
    }

    static boolean is_ws(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    /** Expand a numeric character reference, ie. '#169' or '#xa9'. */
    static char numeric_char(String digits) throws Exception
    {
        int code;
        if (digits.charAt(0) == 'x' || digits.charAt(0) == 'X')
        {
            code = Integer.parseInt(digits.substring(1), 16);
        }
        else
        {
            code = Integer.parseInt(digits);
        }
        if (code < 1)
        {
            throw err("Invalid character reference &#" + digits + ";", 0);
        }
        return (char)code;
    }

    /** Split a value into text and entity-reference parts. A reference is
     *  an '&lt;name&gt;;' sequence where name starts with a letter or
     *  underscore and otherwise consists of name chars. Preceding
     *  character references have already been expanded to characters by
     *  the parser, so the remaining reference text is a named reference.
     *  @param value the decoded value text
     *  @return the list of text and entity-reference parts */
    public static List<ValuePart> value_parts(String value)
    {
        List<ValuePart> parts = new ArrayList<>();
        if (value == null)
        {
            return parts;
        }

        StringBuilder text = new StringBuilder();
        for (int k = 0; k < value.length(); k++)
        {
            char c = value.charAt(k);
            boolean entity = false;
            int end = -1;
            if (c == '&' && k+1 < value.length() && is_name_char(value.charAt(k+1)))
            {
                for (int e = k+1; e < value.length(); e++)
                {
                    char d = value.charAt(e);
                    if (d == ';')
                    {
                        end = e;
                        entity = true;
                        break;
                    }
                    if (!is_name_char(d))
                    {
                        entity = false;
                        break;
                    }
                }
            }
            if (entity)
            {
                if (text.length() > 0)
                {
                    parts.add(new ValuePart(false, text.toString()));
                    text.setLength(0);
                }
                String name = value.substring(k+1, end);
                parts.add(new ValuePart(true, name));
                k = end;
                continue;
            }
            text.append(c);
        }
        if (text.length() > 0)
        {
            parts.add(new ValuePart(false, text.toString()));
        }
        return parts;
    }

    /** Make an error with a source offset for diagnostics. */
    static RuntimeException err(String message, int index)
    {
        return new IllegalStateException(message + " (at offset " + index + ")");
    }

    // ------------------------------------------------------------------
    //  Exact port of the C text-node trimming pipeline
    //  (xmq_trim_quote with is_xmq=false, is_comment=false, xmq.c:1526),
    //  which the C xml parser applies to every text node at parse time
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
        boolean found = false;
        while (i < stop && v.charAt(i) == '\n') i++;
        boolean middle_found = (i > start);
        while (i < stop)
        {
            if (v.charAt(i) == '\n') found = true;
            if (!is_xml_ws(v.charAt(i))) break;
            i++;
        }
        if (!found) return false;
        return true; // (middle_found/only_newlines not needed: caller passes NULL)
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
    static String trim_xml_text(String v)
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


    /** Detect html content like C's xmq content detection (find_word_ignore_case).
     *  @param content the source text
     *  @return true if the text is recognized as html */
    public static boolean is_html(String content)
    {
        int i = skip_ws(content, 0);
        if (i >= content.length() || content.charAt(i) != '<')
        {
            return false;
        }
        String lower = content.toLowerCase(java.util.Locale.ROOT);
        if (lower.startsWith("<!doctype", i))
        {
            return first_word(lower, i + "<!doctype".length()).equalsIgnoreCase("html");
        }
        return first_word(lower, i + 1).equalsIgnoreCase("html");
    }

    static int skip_ws(String s, int i)
    {
        while (i < s.length() && is_ws(s.charAt(i))) i++;
        return i;
    }

    static String first_word(String s, int i)
    {
        int start = skip_ws(s, i);
        if (start >= s.length() || !is_name_char(s.charAt(start)))
        {
            return "";
        }
        int e = start;
        while (e < s.length() && is_name_char(s.charAt(e))) e++;
        return s.substring(start, e);
    }
}
