/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;

import org.libxmq.OutputSettings;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDtd;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlSave;
import org.libxmq.imp.xml.XmlText;

/**
 * Print the xmq DOM as json, a port of the printer in json.c (xmq_print_json).
 * See JsonParser for the conventions used to store xml in json.
 */
public class JsonPrinter extends XMQPrinter
{
    /**
     * Creates the json printer.
     */
    public JsonPrinter()
    {
    }

    /**
     * Print the document as json, like xmq_print_json. The document is first
     * fixed up (text and entities in an element are collapsed into one text).
     * @param doc The document.
     * @param os The output settings.
     * @param final_newline Add a final newline.
     * @return The json.
     */
    public String toJson(XmlDocument doc, OutputSettings os, boolean final_newline)
    {
        fixup_json(doc);
        XmlNode first = doc.firstChild();
        if (first == null) return "";
        XmlNode last = doc.lastChild();
        XMQPrintState ps = new XMQPrintState();
        // Compact json always escapes newlines.
        ps.output_settings = os;
        pre = new ArrayDeque<>();
        post = new ArrayDeque<>();

        // Leading doctype/comments and ending comments are printed inside the root object.
        XmlNode node;
        for (node = first; node != null && node != last; node = node.next())
        {
            if (node instanceof XmlDtd || node.type() == XmlNodeType.COMMENT)
            {
                pre.add(node);
                if (node.type() == XmlNodeType.COMMENT) ps.pre_post_num_comments_total++;
                continue;
            }
            break;
        }
        first = node;
        for (node = last; node != null && node != first; node = node.prev())
        {
            if (node.type() == XmlNodeType.COMMENT)
            {
                post.add(node);
                ps.pre_post_num_comments_total++;
                continue;
            }
            break;
        }
        last = node;

        print_object_nodes(ps, null, first, last);
        if (final_newline) ps.buffer.append('\n');
        return ps.buffer.toString();
    }

    // The leading and ending nodes, taken from the front like stack_rock in C.
    ArrayDeque<XmlNode> pre, post;

    // A raw write, does not change last_char (the C write).
    static void write(XMQPrintState ps, String s)
    {
        ps.buffer.append(s);
    }

    // The C print_utf8: writes and remembers the last char.
    static void print(XMQPrintState ps, String s)
    {
        print_string(ps, s);
    }

    static boolean compact(XMQPrintState ps)
    {
        return ps.output_settings.compact();
    }

    static int indent(XMQPrintState ps)
    {
        return ps.output_settings.indentAmount();
    }

    static boolean is_content(XmlNode n)
    {
        return n.type() == XmlNodeType.TEXT || n.type() == XmlNodeType.CDATA;
    }

    static boolean is_entity(XmlNode n)
    {
        return n.type() == XmlNodeType.ENTITY_REF;
    }

    static String content(XmlNode n)
    {
        return is_content(n) ? n.content() : null;
    }

    static XmlAttr attr(XmlNode n, String name)
    {
        return n instanceof XmlElement e ? e.attribute(name) : null;
    }

    // Like xml_get_attribute != NULL.
    static boolean has_attr(XmlNode n, String name)
    {
        return attr(n, name) != null;
    }

    static String attr_value(XmlAttr a)
    {
        return a.textContent();
    }

    static boolean is_key_value_node(XmlNode n)
    {
        XmlNode from = n.firstChild(), to = n.lastChild();
        if (from != null && from == to && (is_content(from) || is_entity(from))) return true;
        if (from == null) return false;
        for (XmlNode i = from; i != null; i = i.next())
        {
            if (i.type() != XmlNodeType.TEXT && i.type() != XmlNodeType.ENTITY_REF) return false;
        }
        return true;
    }

    static boolean has_attr_other_than_AS_(XmlNode n)
    {
        if (!(n instanceof XmlElement e)) return false;
        for (XmlAttr a : e.attributes())
        {
            String name = a.name();
            if (!name.equals("A") && !name.equals("S") && !name.equals("_")) return true;
        }
        return false;
    }

    static boolean has_attributes(XmlNode n)
    {
        return n instanceof XmlElement e && !e.attributes().isEmpty();
    }

    void print_object_nodes(XMQPrintState ps, XmlNode container, XmlNode from, XmlNode to)
    {
        Map<String, int[]> counters = new HashMap<>(); // name -> {total, used}
        for (XmlNode i = from; i != null; i = i.next())
        {
            String name = name(i);
            if (name != null && !name.equals("_")) counters.computeIfAbsent(name, k -> new int[2])[0]++;
            if (i == to) break;
        }
        for (XmlNode i = from; i != null; i = i.next())
        {
            String name = name(i);
            if (name != null && !name.equals("_"))
            {
                int[] c = counters.get(name);
                print_node(ps, container, i, c[0], c[1]);
                c[1]++;
            }
            else
            {
                print_node(ps, container, i, 1, 0);
            }
            if (i == to) break;
        }
    }

    // The libxml2 node name: elements and entity references have names, text is "text"
    // and comments "comment" (these are counted like names in json.c).
    static String name(XmlNode n)
    {
        return switch (n.type())
        {
        case ELEMENT, ENTITY_REF -> n.name();
        case TEXT -> "text";
        case CDATA -> null;
        case COMMENT -> "comment";
        default -> n.name();
        };
    }

    void print_array_nodes(XMQPrintState ps, XmlNode container, XmlNode from, XmlNode to)
    {
        for (XmlNode i = from; i != null; i = i.next())
        {
            check_comma(ps);
            boolean force_string = has_attr(i, "S");
            String c = element_content(i);
            boolean is_number = c != null && JsonParser.is_number(c);
            boolean is_keyword = c != null && JsonParser.is_keyword(c);
            if (force_string || is_number || is_keyword)
            {
                print_value(ps, i.firstChild(), i.lastChild(), force_string);
            }
            else
            {
                print_node(ps, null, i, 1, 0);
            }
        }
    }

    // Like xml_element_content: the content of the node itself, ie null for an element.
    static String element_content(XmlNode n)
    {
        return n != null && is_content(n) ? n.content() : null;
    }

    /**
     * Quote a string like xmq_quote_as_c in text.c: backslash escapes for
     * \\ \" \a \b \t \n \v \f \r and \xHH for other control characters.
     * @param s The string.
     * @param add_quotes Add surrounding double quotes.
     * @return The quoted string.
     */
    public static String quote(String s, boolean add_quotes)
    {
        StringBuilder b = new StringBuilder();
        if (add_quotes) b.append('"');
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            switch (c)
            {
            case '\\' -> b.append("\\\\");
            case '"' -> b.append("\\\"");
            case 7 -> b.append("\\a");
            case '\b' -> b.append("\\b");
            case '\t' -> b.append("\\t");
            case '\n' -> b.append("\\n");
            case 11 -> b.append("\\v");
            case '\f' -> b.append("\\f");
            case '\r' -> b.append("\\r");
            default -> {
                if (c < ' ' || c == 127) b.append(String.format("\\x%02x", (int)c));
                else b.append(c);
            }
            }
        }
        if (add_quotes) b.append('"');
        return b.toString();
    }

    void print_node(XMQPrintState ps, XmlNode container, XmlNode node, int total, int used)
    {
        if (node.type() == XmlNodeType.COMMENT)
        {
            print_comment_node(ps, node, false, total, used);
            return;
        }
        if (is_content(node))
        {
            print_standalone_quote(ps, node, total, used);
            return;
        }
        if (is_entity(node))
        {
            print_entity_node(ps, node);
            return;
        }
        if (node.firstChild() == null && container != null)
        {
            print_leaf_node(ps, container, node, total, used);
            return;
        }
        if (is_key_value_node(node) && (!has_attributes(node) || !has_attr_other_than_AS_(node)))
        {
            print_key_node(ps, container, node, total, used, has_attr(node, "S"));
            return;
        }
        if (has_attr(node, "A"))
        {
            String name = node.name();
            if (!name.equals("_") && container == null)
            {
                System.err.print("xmq: Warning! The element name \"" + name + "\" is lost when converted to an unnamed json array!\n");
            }
            if (has_attr_other_than_AS_(node))
            {
                System.err.print("xmq: Warning! The element \"" + name + "\" loses its attributes when converted to a json array!\n");
            }
            print_array_with_children(ps, container, node);
            return;
        }
        print_element_with_children(ps, container, node, total, used);
    }

    void print_value(XMQPrintState ps, XmlNode from, XmlNode to, boolean force_string)
    {
        XmlNode node = from;
        String content = element_content(node);
        if (node.next() == null && !force_string && content != null
            && (JsonParser.is_number(content) || JsonParser.is_keyword(content)))
        {
            // A number or a keyword, 123 true false null
            write(ps, content);
            ps.last_char = content.charAt(content.length() - 1);
        }
        else if (node.next() == null && content != null && content.isEmpty())
        {
            write(ps, "\"\"");
            ps.last_char = '"';
        }
        else
        {
            print(ps, "\"");
            if (is_entity(node))
            {
                write(ps, "&" + node.name() + ";");
            }
            else
            {
                do
                {
                    if (is_entity(node))
                    {
                        print(ps, "&" + node.name() + ";");
                    }
                    else
                    {
                        String value = element_content(node);
                        if (value != null) print(ps, quote(value, false));
                    }
                    if (node == to) break;
                    node = node.next();
                }
                while (node != null);
            }
            print(ps, "\"");
            ps.last_char = '"';
        }
    }

    void print_array_with_children(XMQPrintState ps, XmlNode container, XmlNode node)
    {
        check_comma(ps);
        if (container != null)
        {
            print_element_name(ps, node, 1, 0);
            print(ps, ":");
            if (!compact(ps)) print(ps, " ");
        }
        XmlNode from = node.firstChild(), to = node.lastChild();
        print(ps, "[");
        ps.last_char = '[';
        ps.line_indent += indent(ps);
        if (!compact(ps)) print_nl_and_indent(ps, null, null);
        print_array_nodes(ps, null, from, to);
        ps.line_indent -= indent(ps);
        if (!compact(ps)) print_nl_and_indent(ps, null, null);
        print(ps, "]");
        ps.last_char = ']';
    }

    void print_attribute(XMQPrintState ps, XmlAttr a)
    {
        String key = a.name();
        String prefix = a.ns() != null ? a.ns().prefix() : null;
        // The _ attribute holds the element name when it is not a valid xml name.
        if (key.equals("_")) return;
        check_comma(ps);
        print(ps, "\"_");
        if (prefix != null)
        {
            print(ps, prefix);
            print(ps, ":");
        }
        print(ps, quote(key, false));
        print(ps, "\":");
        if (!compact(ps)) print(ps, " ");
        if (a.firstChild() != null)
        {
            print(ps, quote(attr_value(a), true));
        }
        else
        {
            print(ps, "null");
        }
    }

    void print_namespace_declaration(XMQPrintState ps, XmlNs ns)
    {
        check_comma(ps);
        print(ps, "\"_xmlns");
        if (ns.prefix() != null)
        {
            print(ps, ":");
            print(ps, ns.prefix());
        }
        print(ps, "\":");
        if (!compact(ps)) print(ps, " ");
        String v = ns.href();
        if (v != null) print(ps, "\"" + v + "\"");
        else print(ps, "null");
    }

    void print_attributes(XMQPrintState ps, XmlNode node)
    {
        if (!(node instanceof XmlElement e)) return;
        for (XmlAttr a : e.attributes()) print_attribute(ps, a);
        for (XmlNs ns : e.nsDefs()) print_namespace_declaration(ps, ns);
    }

    void print_element_with_children(XMQPrintState ps, XmlNode container, XmlNode node, int total, int used)
    {
        check_comma(ps);
        if (container != null)
        {
            print_element_name(ps, node, total, used);
            print(ps, ":");
            if (!compact(ps)) print(ps, " ");
        }
        XmlNode from = node.firstChild(), to = node.lastChild();
        print(ps, "{");
        ps.last_char = '{';
        ps.line_indent += indent(ps);
        if (!compact(ps)) print_nl_and_indent(ps, null, null);

        while (container == null && !pre.isEmpty())
        {
            XmlNode n = pre.poll();
            if (n instanceof XmlDtd d) print_doctype_node(ps, d);
            else print_comment_node(ps, n, true, ps.pre_post_num_comments_total, ps.pre_post_num_comments_used++);
        }

        String name = node.name();
        if (container == null && name != null && !name.equals("_"))
        {
            // The name of a top level object, or an object in an array, is stored
            // inside the object with the key _, x{a=1} -> {"_":"x","a":1}
            check_comma(ps);
            print(ps, "\"_\":");
            if (!compact(ps)) print(ps, " ");
            ps.last_char = ' ';
            print_element_name(ps, node, total, used);
        }

        print_attributes(ps, node);
        if (from != null) print_object_nodes(ps, node, from, to);

        while (container == null && !post.isEmpty())
        {
            XmlNode n = post.poll();
            print_comment_node(ps, n, true, ps.pre_post_num_comments_total, ps.pre_post_num_comments_used++);
        }

        ps.line_indent -= indent(ps);
        if (!compact(ps)) print_nl_and_indent(ps, null, null);
        print(ps, "}");
        ps.last_char = '}';
    }

    void print_element_name(XMQPrintState ps, XmlNode node, int total, int used)
    {
        String name = node.name();
        String prefix = node instanceof XmlElement e && e.ns() != null ? e.ns().prefix() : null;
        print(ps, "\"");
        if (prefix != null)
        {
            print(ps, prefix);
            print(ps, ":");
        }
        if (!name.equals("_"))
        {
            print(ps, name);
        }
        else
        {
            XmlAttr a = attr(node, "_");
            if (a != null)
            {
                // The key was stored in the attribute _ since it is not a valid element name.
                print(ps, quote(attr_value(a), false));
                ps.last_char = '"';
            }
        }
        if (total > 1) print(ps, "[" + used + "]");
        print(ps, "\"");
        ps.last_char = '"';
    }

    void print_key_node(XMQPrintState ps, XmlNode container, XmlNode node, int total, int used, boolean force_string)
    {
        check_comma(ps);
        if (container != null)
        {
            print_element_name(ps, node, total, used);
            print(ps, ":");
            if (!compact(ps)) print(ps, " ");
            ps.last_char = ':';
        }
        print_value(ps, node.firstChild(), node.lastChild(), force_string);
    }

    void check_comma(XMQPrintState ps)
    {
        char c = ps.last_char;
        if (c == 0) return;
        if (c != '{' && c != '[' && c != ',') print_comma(ps);
    }

    void print_comma(XMQPrintState ps)
    {
        write(ps, ",");
        ps.last_char = ',';
        ps.current_indent++;
        if (!compact(ps)) print_nl_and_indent(ps, null, null);
    }

    void print_comment_node(XMQPrintState ps, XmlNode node, boolean prefix_ul, int total, int used)
    {
        check_comma(ps);
        print(ps, prefix_ul ? "\"_//" : "\"//");
        if (total > 1) print(ps, "[" + used + "]\":");
        else print(ps, "\":");
        if (!compact(ps)) print(ps, " ");
        ps.last_char = ':';
        // The comment is printed as a forced string.
        print(ps, "\"");
        String value = node.content();
        if (value != null) print(ps, quote(value, false));
        print(ps, "\"");
        ps.last_char = '"';
    }

    void print_doctype_node(XMQPrintState ps, XmlDtd node)
    {
        check_comma(ps);
        print(ps, "\"!DOCTYPE\":");
        ps.last_char = ':';
        String c = XmlSave.nodeDump(node.doc(), node, 0, false);
        print(ps, quote(c.substring(10, c.length() - 1), true));
        ps.last_char = '"';
    }

    void print_entity_node(XMQPrintState ps, XmlNode node)
    {
        check_comma(ps);
        print(ps, "\"&\":\"&" + node.name() + ";\"");
        ps.last_char = '"';
    }

    void print_standalone_quote(XMQPrintState ps, XmlNode node, int total, int used)
    {
        check_comma(ps);
        String quoted = quote(node.content(), false);
        if (total == 1) print(ps, "\"|\":\"" + quoted + "\"");
        else print(ps, "\"|[" + used + "]\":\"" + quoted + "\"");
        ps.last_char = '"';
    }

    void print_leaf_node(XMQPrintState ps, XmlNode container, XmlNode node, int total, int used)
    {
        String name = node.name();
        check_comma(ps);
        if (name != null && !name.equals("_"))
        {
            print_element_name(ps, node, total, used);
            write(ps, ":");
            if (!compact(ps)) print(ps, " ");
        }
        if (has_attr(node, "A"))
        {
            write(ps, "[]");
            ps.last_char = ']';
        }
        else if (has_attributes(node))
        {
            write(ps, "{");
            ps.last_char = '{';
            print_attributes(ps, node);
            write(ps, "}");
            ps.last_char = '}';
        }
        else
        {
            write(ps, "{}");
            ps.last_char = '}';
        }
    }

    // ------------------------------------------------------------ fixup

    // Collapse the text and entity children of elements into a single text,
    // like fixup_json and xml_collapse_text in C.
    static void fixup_json(XmlNode node)
    {
        if (node.type() == XmlNodeType.ELEMENT)
        {
            String collapsed = collapse_text(node);
            if (collapsed != null)
            {
                for (XmlNode i = node.firstChild(); i != null;)
                {
                    XmlNode next = i.next();
                    i.unlink();
                    i = next;
                }
                node.appendRaw(new XmlText(collapsed));
                return;
            }
        }
        for (XmlNode i = node.firstChild(); i != null;)
        {
            XmlNode next = i.next();
            fixup_json(i);
            i = next;
        }
    }

    static String collapse_text(XmlNode node)
    {
        int num_text = 0, num_entities = 0;
        for (XmlNode i = node.firstChild(); i != null; i = i.next())
        {
            if (i.type() != XmlNodeType.TEXT && i.type() != XmlNodeType.ENTITY_REF) return null;
            if (i.type() == XmlNodeType.TEXT) num_text++; else num_entities++;
        }
        if (num_text <= 1 && num_entities == 0) return null;
        StringBuilder b = new StringBuilder();
        for (XmlNode i = node.firstChild(); i != null; i = i.next())
        {
            if (i.type() == XmlNodeType.TEXT)
            {
                b.append(i.content());
                continue;
            }
            int uc = decode_entity_ref(i.name());
            // A zero (an unknown entity) ends the C string.
            if (uc == 0) break;
            b.appendCodePoint(uc);
        }
        return b.toString();
    }

    // Like decode_entity_ref in xml.c, including its quirks: an unknown named
    // entity and a hex reference (strtol on "#x..") give 0.
    static int decode_entity_ref(String name)
    {
        switch (name)
        {
        case "apos": return '\'';
        case "gt": return '>';
        case "lt": return '<';
        case "quot": return '"';
        case "nbsp": return 160;
        default: break;
        }
        if (name.isEmpty() || name.charAt(0) != '#') return 0;
        if (name.length() > 1 && name.charAt(1) == 'x') return 0;
        int i = 1, v = 0;
        boolean neg = false;
        if (i < name.length() && (name.charAt(i) == '-' || name.charAt(i) == '+')) neg = name.charAt(i++) == '-';
        while (i < name.length() && Character.isDigit(name.charAt(i))) v = v * 10 + (name.charAt(i++) - '0');
        return neg ? -v : v;
    }
}
