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
import java.util.Stack;

/** Low-level scanner that drives a Handler from raw XML source text:
 *  elements, attributes, character data, comments, processing
 *  instructions, doctypes and entity references. Html mode relaxes the
 *  checks that differ between xml and html (see org.libxmq.imp.xml). */
public class Scanner
{
    final String s;
    final Handler h;
    int i = 0;
    boolean html;

    Scanner(String s, Handler h, boolean html)
    {
        this.s = s;
        this.h = h;
        this.html = html;
    }

    boolean done()
    {
        return i >= s.length();
    }

    char ch()
    {
        if (done())
        {
            throw RawXmlParser.err("Unexpected end of input", i);
        }
        return s.charAt(i);
    }

    void skip_ws()
    {
        while (!done() && RawXmlParser.is_ws(ch())) i++;
    }

    boolean starts_with(String t)
    {
        return s.startsWith(t, i);
    }

    int find(String t, int from)
    {
        return s.indexOf(t, from);
    }

    String name() throws Exception
    {
        if (done() || !(RawXmlParser.is_name_char(ch()) || is_digit(ch())))
        {
            throw RawXmlParser.err("Expecting a name, got '" + (done() ? "" : ch() + "") + "'", i);
        }
        int start = i;
        while (!done() && (RawXmlParser.is_name_char(ch()) || is_digit(ch()) || ch() == '.' || ch() == '+'))
        {
            i++;
        }
        return s.substring(start, i);
    }

    static boolean is_digit(char c)
    {
        return c >= '0' && c <= '9';
    }

    void document() throws Exception
    {
        while (true)
        {
            skip_ws();
            if (done())
            {
                return;
            }
            if (starts_with("<?xml"))
            {
                // The XML declaration, not a processing instruction.
                int end = find("?>", i);
                if (end < 0)
                {
                    throw RawXmlParser.err("Unterminated XML declaration", i);
                }
                i = end + 2;
                continue;
            }
            if (starts_with("<?"))
            {
                processing_instruction();
                continue;
            }
            if (starts_with("<!--"))
            {
                comment();
                continue;
            }
            if (starts_with("<!DOCTYPE"))
            {
                doctype();
                continue;
            }
            if (starts_with("<"))
            {
                element();
                continue;
            }
            throw RawXmlParser.err("Invalid character '" + ch() + "' in document", i);
        }
    }

    void processing_instruction() throws Exception
    {
        int end = find("?>", i + 2);
        if (end < 0)
        {
            throw RawXmlParser.err("Unterminated processing instruction", i);
        }
        String body = s.substring(i + 2, end).strip();
        int sp = 0;
        while (sp < body.length() && !RawXmlParser.is_ws(body.charAt(sp))) sp++;
        String target = body.substring(0, sp);
        String data = sp < body.length() ? body.substring(sp + 1) : "";
        h.processingInstruction(target, data);
        i = end + 2;
    }

    void comment() throws Exception
    {
        int end = find("-->", i + 4);
        if (end < 0)
        {
            throw RawXmlParser.err("Unterminated comment", i);
        }
        h.comment(s.substring(i + 4, end));
        i = end + 3;
    }

    static String normalize_subset(String subset)
    {
        StringBuilder v = new StringBuilder();
        for (String line : subset.split("\n"))
        {
            String t = line.strip();
            if (t.isEmpty()) continue;
            if (v.length() > 0) v.append('\n');
            v.append(t);
        }
        return v.toString();
    }

    void doctype() throws Exception
    {
        i += "<!DOCTYPE".length();
        skip_ws();
        String name = this.name();
        skip_ws();
        String subset = null;
        if (!done() && ch() == '[')
        {
            int start = i + 1;
            char quote = 0;
            int close = -1;
            for (int k = start; k < s.length(); k++)
            {
                char c = s.charAt(k);
                if (quote != 0)
                {
                    if (c == quote) quote = 0;
                    continue;
                }
                if (c == '\'' || c == '"')
                {
                    quote = c;
                    continue;
                }
                if (c == ']')
                {
                    close = k;
                    break;
                }
            }
            if (close < 0)
            {
                throw RawXmlParser.err("Unterminated internal subset in doctype", i);
            }
            subset = s.substring(start, close);
            // Skip the ']' and any trailing whitespace and the '>'.
            int k = close + 1;
            while (k < s.length() && RawXmlParser.is_ws(s.charAt(k))) k++;
            if (k >= s.length() || s.charAt(k) != '>')
            {
                throw RawXmlParser.err("Expecting '>' after doctype internal subset", close);
            }
            i = k + 1;
            String n = normalize_subset(subset);
            h.doctype(name, n.length() > 0 ? n : null);
        }
        else
        {
            skip_ws();
            if (done() || ch() != '>')
            {
                throw RawXmlParser.err("Expecting '>' in doctype declaration", i);
            }
            i++;
            h.doctype(name, null);
        }
    }

    void element() throws Exception
    {
        if (ch() != '<')
        {
            throw RawXmlParser.err("Expecting '<' before element name", i);
        }
        i++;
        String name = this.name();
        List<String> anames = new ArrayList<>();
        List<String> avalues = new ArrayList<>();
        while (true)
        {
            skip_ws();
            if (starts_with("/>"))
            {
                i += 2;
                break_self_close(name, anames, avalues);
                return;
            }
            if (!done() && ch() == '>')
            {
                i++;
                break;
            }
            String aname = this.name();
            skip_ws();
            if (done() || ch() != '=')
            {
                throw RawXmlParser.err("Expecting '=' after attribute name " + aname, i);
            }
            i++;
            skip_ws();
            if (done() || (ch() != '\'' && ch() != '"'))
            {
                throw RawXmlParser.err("Expecting a quoted attribute value for " + aname, i);
            }
            char quote = ch();
            i++;
            int end = s.indexOf(quote, i);
            if (end < 0)
            {
                throw RawXmlParser.err("Unterminated attribute value for " + aname, i);
            }
            anames.add(aname);
            avalues.add(Scanner.decode_value(s.substring(i, end)));
            i = end + 1;
        }
        h.startElement(name, anames.toArray(new String[0]), avalues.toArray(new String[0]));
        element_content(name);
        h.endElement(name);
    }

    void break_self_close(String name, List<String> anames, List<String> avalues) throws Exception
    {
        h.startElement(name, anames.toArray(new String[0]), avalues.toArray(new String[0]));
        h.endElement(name);
    }

    void element_content(String name) throws Exception
    {
        while (true)
        {
            if (done())
            {
                throw RawXmlParser.err("Unexpected end of input, expecting </" + name + ">", i);
            }
            if (starts_with("<!--"))
            {
                comment();
                continue;
            }
            if (starts_with("<?"))
            {
                processing_instruction();
                continue;
            }
            if (starts_with("</"))
            {
                int k = i + 2;
                String cname = "";
                if (k < s.length() && (RawXmlParser.is_name_char(s.charAt(k)) || is_digit(s.charAt(k))))
                {
                    int e = k;
                    while (e < s.length() && (RawXmlParser.is_name_char(s.charAt(e)) || is_digit(s.charAt(e))
                                              || s.charAt(e) == '.' || s.charAt(e) == '+'))
                    {
                        e++;
                    }
                    cname = s.substring(k, e);
                    k = e;
                    while (k < s.length() && RawXmlParser.is_ws(s.charAt(k))) k++;
                    if (k >= s.length() || s.charAt(k) != '>')
                    {
                        throw RawXmlParser.err("Expecting '>' in end tag", k);
                    }
                    if (!cname.equals(name))
                    {
                        throw RawXmlParser.err("Closing tag </" + cname + "> does not match opening <" + name + ">", i);
                    }
                    i = k + 1;
                    return;
                }
                throw RawXmlParser.err("Expecting element name in end tag", i);
            }
            if (starts_with("<![CDATA["))
            {
                int end = find("]]>", i + 9);
                if (end < 0)
                {
                    throw RawXmlParser.err("Unterminated CDATA section", i);
                }
                h.text(s.substring(i + 9, end));
                i = end + 3;
                continue;
            }
            if (!done() && ch() == '<')
            {
                element();
                continue;
            }
            if (!done() && ch() == '&')
            {
                entity_reference();
                continue;
            }
            h.text(String.valueOf(ch()));
            i++;
        }
    }

    void entity_reference() throws Exception
    {
        int start = i;
        int j = i + 1;
        if (j >= s.length() || !RawXmlParser.is_name_char(s.charAt(j)) && !(s.charAt(j) == '#') )
        {
            throw RawXmlParser.err("Invalid entity reference", i);
        }
        char first = s.charAt(j);
        if (first == '#')
        {
            j++;
            boolean valid = false;
            int end = j;
            boolean hex = false;
            if (end < s.length() && (s.charAt(end) == 'x' || s.charAt(end) == 'X'))
            {
                hex = true;
                end++;
            }
            for (; end < s.length(); end++)
            {
                char c = s.charAt(end);
                boolean ok = hex ? Character.digit(c, 16) >= 0
                    : (c >= '0' && c <= '9');
                if (!ok) break;
                valid = true;
            }
            if (!valid || end >= s.length() || s.charAt(end) != ';')
            {
                throw RawXmlParser.err("Invalid character reference", start);
            }
            String digits = s.substring(j, end);
            h.text(String.valueOf(RawXmlParser.numeric_char(digits)));
            i = end + 1;
            return;
        }
        while (j < s.length() && (RawXmlParser.is_name_char(s.charAt(j)) || s.charAt(j) == '.' || s.charAt(j) == '+' || s.charAt(j) == '/'))
        {
            j++;
        }
        if (j >= s.length() || s.charAt(j) != ';')
        {
            throw RawXmlParser.err("Invalid entity reference", start);
        }
        String name = s.substring(i + 1, j);
        char pre = RawXmlParser.predefined_char(name);
        if (pre != 0)
        {
            h.text(String.valueOf(pre));
        }
        else
        {
            h.entityReference(name);
        }
        i = j + 1;
    }

    /** Decode an attribute value. Named references of predefined
     *  entities and character references are expanded to characters,
     *  other named references are kept as literal '&lt;name&gt;;' text. */
    static String decode_value(String v) throws Exception
    {
        boolean is_html = false;
        StringBuilder out = new StringBuilder();
        int i = 0;
        while (i < v.length())
        {
            char c = v.charAt(i);
            if (c != '&')
            {
                // XML attribute-value normalization happens on the raw
                // value, so tabs/linefeeds/carriage returns become spaces
                // here.  Characters produced by references (such as
                // '&#10;') are appended below verbatim. In html mode no
                // normalization is done at all.
                if (/*!html && */ (c == '\t' || c == '\n' || c == '\r'))
                {
                    out.append(' ');
                }
                else
                {
                    out.append(c);
                }
                i++;
                continue;
            }
            int j = i + 1;
            String name = null;
            if (j < v.length() && v.charAt(j) == '#')
            {
                j++;
                int nstart = j;
                boolean hex = false;
                if (j < v.length() && (v.charAt(j) == 'x' || v.charAt(j) == 'X'))
                {
                    hex = true;
                    j++;
                }
                boolean valid = false;
                for (; j < v.length(); j++)
                {
                    boolean ok = hex ? Character.digit(v.charAt(j), 16) >= 0
                        : (v.charAt(j) >= '0' && v.charAt(j) <= '9');
                    if (!ok) break;
                    valid = true;
                }
                if (!valid || j >= v.length() || v.charAt(j) != ';')
                {
                    throw RawXmlParser.err("Invalid character reference in attribute value", i);
                }
                out.append(RawXmlParser.numeric_char(v.substring(nstart, j)));
                i = j + 1;
                continue;
            }
            for (; j < v.length(); j++)
            {
                char d = v.charAt(j);
                if (d == ';')
                {
                    name = v.substring(i + 1, j);
                    break;
                }
                if (!RawXmlParser.is_name_char(d))
                {
                    break;
                }
            }
            if (name != null)
            {
                char pre = RawXmlParser.predefined_char(name);
                if (pre != 0)
                {
                    out.append(pre);
                }
                else if (is_html)
                {
                    Integer ch = HtmlEntities.get(name);
                    if (ch != null)
                    {
                        out.append((char) ch.intValue());
                    }
                    else
                    {
                        out.append('&').append(name).append(';');
                    }
                }
                else
                {
                    // Keep the reference intact.
                    out.append('&').append(name).append(';');
                }
                i = j + 1;
                continue;
            }
            throw RawXmlParser.err("Invalid '&' in attribute value", i);
        }
        return out.toString();
    }
}
