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

package org.libxmq.imp;

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
    /** SAX-style handler. All methods are optional in the sense that a
     *  handler is free to ignore events it does not need. */
    public interface Handler
    {
        /** A doctype declaration. Subset is the raw internal subset with
         *  each line trimmed and empty lines dropped, joined by newlines,
         *  or null if there is no internal subset. */
        void doctype(String name, String subset) throws Exception;

        /** A processing instruction. Data has leading whitespace removed. */
        void processingInstruction(String target, String data) throws Exception;

        /** A comment, without the surrounding comment marks. */
        void comment(String text) throws Exception;

        /** Start of an element. Attribute names and values are in source
         *  order. Values are decoded: character references and predefined
         *  entity references are expanded to characters, but other named
         *  references are kept as literal '&lt;name&gt;;' text. */
        void startElement(String name, String[] attributeNames, String[] attributeValues) throws Exception;

        /** End of an element. */
        void endElement(String name) throws Exception;

        /** Character data. Character references are already expanded. */
        void text(String t) throws Exception;

        /** An unresolved entity reference in content. */
        void entityReference(String name) throws Exception;
    }

    /** One part of a decoded attribute value or a value printed by the
     *  printer: either entity-reference text (like '&lt;name&gt;;') or
     *  plain text. */
    public static class ValuePart
    {
        public final boolean is_entity;
        public final String text;

        ValuePart(boolean is_entity, String text)
        {
            this.is_entity = is_entity;
            this.text = text;
        }
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
     *  the parser, so the remaining reference text is a named reference. */
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

    static class Scanner
    {
        final String s;
        final Handler h;
        int i = 0;

        Scanner(String s, Handler h)
        {
            this.s = s;
            this.h = h;
        }

        boolean done()
        {
            return i >= s.length();
        }

        char ch()
        {
            if (done())
            {
                throw err("Unexpected end of input", i);
            }
            return s.charAt(i);
        }

        void skip_ws()
        {
            while (!done() && is_ws(ch())) i++;
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
            if (done() || !(is_name_char(ch()) || is_digit(ch())))
            {
                throw err("Expecting a name, got '" + (done() ? "" : ch() + "") + "'", i);
            }
            int start = i;
            while (!done() && (is_name_char(ch()) || is_digit(ch()) || ch() == '.' || ch() == '+'))
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
                        throw err("Unterminated XML declaration", i);
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
                throw err("Invalid character '" + ch() + "' in document", i);
            }
        }

        void processing_instruction() throws Exception
        {
            int end = find("?>", i + 2);
            if (end < 0)
            {
                throw err("Unterminated processing instruction", i);
            }
            String body = s.substring(i + 2, end).strip();
            int sp = 0;
            while (sp < body.length() && !is_ws(body.charAt(sp))) sp++;
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
                throw err("Unterminated comment", i);
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
                    throw err("Unterminated internal subset in doctype", i);
                }
                subset = s.substring(start, close);
                // Skip the ']' and any trailing whitespace and the '>'.
                int k = close + 1;
                while (k < s.length() && is_ws(s.charAt(k))) k++;
                if (k >= s.length() || s.charAt(k) != '>')
                {
                    throw err("Expecting '>' after doctype internal subset", close);
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
                    throw err("Expecting '>' in doctype declaration", i);
                }
                i++;
                h.doctype(name, null);
            }
        }

        void element() throws Exception
        {
            if (ch() != '<')
            {
                throw err("Expecting '<' before element name", i);
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
                    throw err("Expecting '=' after attribute name " + aname, i);
                }
                i++;
                skip_ws();
                if (done() || (ch() != '\'' && ch() != '"'))
                {
                    throw err("Expecting a quoted attribute value for " + aname, i);
                }
                char quote = ch();
                i++;
                int end = s.indexOf(quote, i);
                if (end < 0)
                {
                    throw err("Unterminated attribute value for " + aname, i);
                }
                anames.add(aname);
                avalues.add(decode_value(s.substring(i, end)));
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
                    throw err("Unexpected end of input, expecting </" + name + ">", i);
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
                    if (k < s.length() && (is_name_char(s.charAt(k)) || is_digit(s.charAt(k))))
                    {
                        int e = k;
                        while (e < s.length() && (is_name_char(s.charAt(e)) || is_digit(s.charAt(e))
                            || s.charAt(e) == '.' || s.charAt(e) == '+'))
                        {
                            e++;
                        }
                        cname = s.substring(k, e);
                        k = e;
                        while (k < s.length() && is_ws(s.charAt(k))) k++;
                        if (k >= s.length() || s.charAt(k) != '>')
                        {
                            throw err("Expecting '>' in end tag", k);
                        }
                        if (!cname.equals(name))
                        {
                            throw err("Closing tag </" + cname + "> does not match opening <" + name + ">", i);
                        }
                        i = k + 1;
                        return;
                    }
                    throw err("Expecting element name in end tag", i);
                }
                if (starts_with("<![CDATA["))
                {
                    int end = find("]]>", i + 9);
                    if (end < 0)
                    {
                        throw err("Unterminated CDATA section", i);
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
            if (j >= s.length() || !is_name_char(s.charAt(j)) && !(s.charAt(j) == '#') )
            {
                throw err("Invalid entity reference", i);
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
                    throw err("Invalid character reference", start);
                }
                String digits = s.substring(j, end);
                h.text(String.valueOf(numeric_char(digits)));
                i = end + 1;
                return;
            }
            while (j < s.length() && (is_name_char(s.charAt(j)) || s.charAt(j) == '.' || s.charAt(j) == '+' || s.charAt(j) == '/'))
            {
                j++;
            }
            if (j >= s.length() || s.charAt(j) != ';')
            {
                throw err("Invalid entity reference", start);
            }
            String name = s.substring(i + 1, j);
            char pre = predefined_char(name);
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
            StringBuilder out = new StringBuilder();
            int i = 0;
            while (i < v.length())
            {
                char c = v.charAt(i);
                if (c != '&')
                {
                    out.append(c);
                    i++;
                    continue;
                }
                int j = i + 1;
                String name = null;
                if (j < v.length() && v.charAt(j) == '#')
                {
                    j++;
                    int start = j;
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
                        throw err("Invalid character reference in attribute value", i);
                    }
                    out.append(numeric_char(v.substring(start, j)));
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
                    if (!is_name_char(d))
                    {
                        break;
                    }
                }
                if (name != null)
                {
                    char pre = predefined_char(name);
                    if (pre != 0)
                    {
                        out.append(pre);
                    }
                    else
                    {
                        // Keep the reference intact.
                        out.append('&').append(name).append(';');
                    }
                    i = j + 1;
                    continue;
                }
                throw err("Invalid '&' in attribute value", i);
            }
            return out.toString();
        }
    }

    /** A handler that builds a JDOM2 document. */
    static class JdomHandler implements Handler
    {
        final Document doc = new Document();
        final Stack<Element> elements = new Stack<>();
        Element root = null;
        final StringBuilder pending_text = new StringBuilder();

        public void doctype(String name, String subset) throws Exception
        {
            DocType d = new DocType(name);
            if (subset != null)
            {
                d.setInternalSubset(subset);
            }
            doc.setDocType(d);
        }

        public void processingInstruction(String target, String data) throws Exception
        {
            ProcessingInstruction pi = new ProcessingInstruction(target, data);
            add(pi);
        }

        public void comment(String text) throws Exception
        {
            add(new Comment(text));
        }

        Element top()
        {
            return elements.isEmpty() ? null : elements.peek();
        }

        void add(Content c)
        {
            Element e = top();
            if (e == null)
            {
                doc.addContent(c);
            }
            else
            {
                e.addContent(c);
            }
        }

        void flush_text()
        {
            if (pending_text.length() == 0)
            {
                return;
            }
            String t = pending_text.toString();
            pending_text.setLength(0);
            Element e = top();
            if (e == null)
            {
                // Text directly under the document is dropped, like
                // whitespace between the root element and the end tag.
                return;
            }
            // Whitespace-only text nodes are dropped, matching the C
            // parser's XML_PARSE_NOBLANKS. See also the printer, which
            // ignores whitespace-only values.
            if (t.trim().isEmpty())
            {
                return;
            }
            e.addContent(new Text(t));
        }

        public void text(String t) throws Exception
        {
            pending_text.append(t);
        }

        public void entityReference(String name) throws Exception
        {
            flush_text();
            EntityRef er = new EntityRef(name);
            Element e = top();
            if (e == null)
            {
                throw err("Entity reference &" + name + "; outside of an element", 0);
            }
            e.addContent(er);
        }

        public void startElement(String name, String[] anames, String[] avalues) throws Exception
        {
            flush_text();
            Element e = new Element(name);
            for (int k = 0; k < anames.length; k++)
            {
                e.setAttribute(new Attribute(anames[k], avalues[k]));
            }
            if (top() == null)
            {
                if (root != null)
                {
                    throw err("Document has more than one root element", 0);
                }
                root = e;
                doc.addContent(e);
            }
            elements.push(e);
        }

        public void endElement(String name) throws Exception
        {
            flush_text();
            Element e = top();
            if (e == null || !e.getName().equals(name))
            {
                throw err("Element end " + name + " does not match", 0);
            }
            elements.pop();
        }

        Document finish()
        {
            return doc;
        }
    }

    /** Parse an XML string into a JDOM2 document without resolving
     *  general entity references. This is the XML to XMQ direction. */
    public static Document parse(String content) throws Exception
    {
        JdomHandler h = new JdomHandler();
        new Scanner(content, h).document();
        return h.finish();
    }
}
