/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

package org.libxmq.imp;

import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;

/**
 * Print the xmq DOM as block style yaml, a port of xmq_print_yaml in yaml.c.
 * The id, ref, tag and ltag attributes become anchors, aliases and tags,
 * the S attribute forces a quoted string and A marks a sequence.
 */
final class YamlPrinter
{
    private final StringBuilder out = new StringBuilder();
    private final int indent;

    private YamlPrinter(int indent)
    {
        this.indent = indent < 1 ? 2 : indent;
    }

    /**
     * Print the document as yaml.
     * @param doc The document.
     * @param indent The indentation per level, less than 1 gives 2.
     * @param final_newline Add a final newline.
     * @return The yaml.
     */
    static String print(XmlDocument doc, int indent, boolean final_newline)
    {
        YamlPrinter p = new YamlPrinter(indent);
        boolean started = false;
        for (XmlNode r = doc.firstChild(); r != null; r = r.next())
        {
            // Yaml has no doctype, skip the dtd (and its entity declarations).
            if (r.type() == XmlNodeType.DTD) continue;
            if (r.type() == XmlNodeType.ELEMENT && r.name().equals("_") && !JsonPrinter.is_key_value_node(r))
            {
                // The standard root element: its children are the top level.
                XmlNode first = r.firstChild();
                if (first != null)
                {
                    p.children(0, first, r.lastChild(), has(r, "A"), !started);
                    started = true;
                }
            }
            else
            {
                if (started) p.out.append('\n');
                p.child(0, r);
                started = true;
            }
        }
        if (final_newline) p.out.append('\n');
        return p.out.toString();
    }

    static boolean has(XmlNode n, String attr)
    {
        return n instanceof XmlElement e && e.attribute(attr) != null;
    }

    static String attr(XmlNode n, String name)
    {
        if (!(n instanceof XmlElement e)) return null;
        XmlAttr a = e.attribute(name);
        return a != null && a.firstChild() != null ? a.textContent() : null;
    }

    static boolean ascii_alpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    /**
     * Does text that is a yaml string need quotes to keep its meaning, like yaml_needs_quotes.
     * @param s The text.
     * @return True if quotes are needed.
     */
    static boolean needs_quotes(String s)
    {
        if (s == null || s.isEmpty()) return true;
        if ("!&*?:{}[],#|>'\"%@`".indexOf(s.charAt(0)) >= 0) return true;
        for (int i = 0; i < s.length(); i++)
        {
            char c = s.charAt(i);
            if (!ascii_alpha(c) && !(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.' && c != '/' && c != ' ') return true;
        }
        return s.charAt(s.length() - 1) == ' ';
    }

    void comment(XmlNode node)
    {
        out.append("# ");
        String p = node.content() == null ? "" : node.content();
        for (int i = 0; i < p.length(); i++)
        {
            char c = p.charAt(i);
            out.append(c == '\n' || c == '\t' || c == '\r' ? ' ' : c);
        }
    }

    // The scalar value of the node range from..to, quoted if necessary.
    void value(XmlNode from, XmlNode to, boolean force_string)
    {
        if (from == to)
        {
            String content = JsonPrinter.element_content(from);
            if (content != null && !JsonPrinter.is_entity(from) && !force_string && !needs_quotes(content))
            {
                out.append(content);
                return;
            }
        }
        out.append('"');
        for (XmlNode i = from; i != null; i = i == to ? null : i.next())
        {
            if (JsonPrinter.is_entity(i))
            {
                out.append('&').append(i.name()).append(';');
                continue;
            }
            String content = JsonPrinter.element_content(i);
            if (content != null) out.append(JsonPrinter.quote(content, false));
        }
        out.append('"');
    }

    // Like xml_element_name(i) && strcmp(i->name, node->name) == 0, the libxml2 names.
    static boolean same_name(XmlNode i, XmlNode node)
    {
        String a = JsonPrinter.name(i);
        return a != null && a.equals(JsonPrinter.name(node));
    }

    static int dup_used(XmlNode node)
    {
        int used = 0;
        for (XmlNode i = node.prev(); i != null; i = i.prev()) if (same_name(i, node)) used++;
        return used;
    }

    static int dup_total(XmlNode node)
    {
        int total = 1;
        for (XmlNode i = node.prev(); i != null; i = i.prev()) if (same_name(i, node)) total++;
        for (XmlNode i = node.next(); i != null; i = i.next()) if (same_name(i, node)) total++;
        return total;
    }

    void key(String name, int total, int used)
    {
        if (!name.isEmpty() && !needs_quotes(name)) out.append(name);
        else out.append(JsonPrinter.quote(name, false));
        if (total > 1) out.append('[').append(used).append(']');
    }

    boolean one_affix(XmlNode node, String mark, String attrname, boolean need_space)
    {
        String v = attr(node, attrname);
        if (v == null) return false;
        if (need_space) out.append(' ');
        out.append(mark).append(v);
        return true;
    }

    // The anchor and/or tag of an element: " &anchor" " !!tag" or " !ltag".
    boolean affixes(XmlNode node, boolean need_space)
    {
        if (one_affix(node, "&", "id", need_space))
        {
            one_affix(node, "!!", "tag", true);
            one_affix(node, "!", "ltag", true);
            return true;
        }
        if (one_affix(node, "!!", "tag", need_space))
        {
            one_affix(node, "!", "ltag", true);
            return true;
        }
        return one_affix(node, "!", "ltag", need_space);
    }

    // The siblings first..last, one per line at col, prefixed with "- " when dashed.
    void children(int col, XmlNode first, XmlNode last, boolean dashed, boolean inline_first)
    {
        boolean inline_here = inline_first;
        for (XmlNode i = first; i != null; i = i == last ? null : i.next())
        {
            if (inline_here)
            {
                if (dashed) out.append("- ");
            }
            else
            {
                out.append('\n').append(" ".repeat(col));
                if (dashed) out.append("- ");
            }
            inline_here = false;
            child(col + (dashed ? 2 : 0), i);
        }
    }

    void child(int col, XmlNode node)
    {
        if (node.type() == XmlNodeType.COMMENT)
        {
            comment(node);
            return;
        }
        if (JsonPrinter.is_entity(node))
        {
            out.append("\"&").append(node.name()).append(";\"");
            return;
        }
        String name = JsonPrinter.name(node);
        if (name == null) name = "";
        boolean under = name.equals("_");
        String ref = attr(node, "ref");

        if (JsonPrinter.is_key_value_node(node))
        {
            boolean force_string = has(node, "S");
            if (!under)
            {
                key(name, dup_total(node), dup_used(node));
                out.append(": ");
            }
            if (ref != null)
            {
                // An alias, a reference carries no content.
                out.append('*').append(ref);
                return;
            }
            if (affixes(node, false)) out.append(' ');
            value(node.firstChild(), node.lastChild(), force_string);
            return;
        }

        // A container node.
        if (ref != null)
        {
            if (!under)
            {
                key(name, dup_total(node), dup_used(node));
                out.append(": ");
            }
            out.append('*').append(ref);
            return;
        }
        if (under)
        {
            // An anonymous container, a (possibly nested) sequence item.
            boolean a = has(node, "A");
            XmlNode first = node.firstChild();
            boolean affixed = affixes(node, false);
            if (first == null)
            {
                out.append("\"\"");
                return;
            }
            // With an anchor or tag, the first child starts on its own line.
            children(col, first, node.lastChild(), a, !affixed);
            return;
        }
        // A named container: "name:" followed by its children one level deeper.
        key(name, dup_total(node), dup_used(node));
        out.append(':');
        affixes(node, true);
        XmlNode first = node.firstChild();
        if (first == null) return;
        children(col + indent, first, node.lastChild(), has(node, "A"), false);
    }
}
