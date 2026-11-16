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
 * Explicit entity expansion, like xml_expand_entities in the C version
 * (parts/xml_parser.c). Entities are never expanded when parsing: a
 * reference to a declared entity stays as an XmlEntityRef. This replaces
 * every reference to a declared internal general entity, also inside
 * attribute values, with its replacement text parsed as content at the
 * position of the reference (so the namespaces in scope there are used).
 * References inside the replacement text are expanded too. Character
 * references, undeclared entities and external entities (never loaded) are
 * kept. Adjacent text nodes are merged.
 *
 * The limits of libxml2 are enforced: loops, 19 nested entities (20 inputs
 * counting the document) and the amplification factor 5 once more than 1MB
 * has been expanded. On an error the references expanded so far stay
 * expanded, the one being expanded is kept.
 */
public final class XmlEntityExpander
{
    static final int MAX_NESTING = 20;
    static final long ALLOWED_EXPANSION = 1000000;
    static final long MAX_AMPLIFICATION = 5;

    final XmlDocument doc_;
    final String only_;
    final long input_size_;
    final int options_;
    final List<XmlEntityDecl> active_ = new ArrayList<>();
    long expanded_size_;

    private XmlEntityExpander(XmlDocument doc, String only, long input_size, int options)
    {
        doc_ = doc;
        only_ = only;
        input_size_ = input_size;
        options_ = options;
    }

    /**
     * Expand the entities of a document.
     * @param doc The document.
     * @param only Expand only references to this entity (and everything inside it), or null for all.
     * @param input_size The size of the parsed source, for the amplification limit.
     * @param options XmlParser options used when parsing the replacement text, eg NOCDATA.
     * @throws XmlParseException If an entity cannot be expanded, the error field has the message.
     */
    public static void expand(XmlDocument doc, String only, long input_size, int options) throws XmlParseException
    {
        if (doc.intSubset() == null) return; // No declared entities.
        XmlEntityExpander x = new XmlEntityExpander(doc, only, input_size, options);
        x.collectAttrTypes();
        x.expandChildren(doc);
        mergeText(doc);
    }

    /** The replacement text parsed last, errors outside of a parse are reported at its start
        like the C version (and libxml2) report them in the current input, "" at first. */
    String buf_ = "";

    /** An error, the full message has the source context of the text parsed last, the error
        field the message itself. */
    XmlParseException error(String msg)
    {
        return new XmlParser(buf_, doc_.url(), options_).fatal(msg, 0);
    }

    /** The internal general entity to expand for a reference, null if the reference is kept. */
    XmlEntityDecl expandable(String name)
    {
        if (name == null || name.startsWith("#")) return null; // Character reference node (from xmq).
        if (only_ != null && active_.isEmpty() && !name.equals(only_)) return null;
        XmlEntityDecl decl = doc_.getEntity(name);
        if (decl == null) return null; // Undeclared, kept.
        if (decl.etype() != XmlEntityDecl.INTERNAL_GENERAL) return null; // External entities are never loaded.
        return decl;
    }

    void enter(XmlEntityDecl decl, int value_len) throws XmlParseException
    {
        for (XmlEntityDecl a : active_)
        {
            if (a == decl) throw error("Detected an entity reference loop");
        }
        // libxml2 limits the input stack, the document is one of the inputs.
        if (active_.size() + 1 >= MAX_NESTING) throw error("Maximum entity nesting depth exceeded");
        expanded_size_ += value_len;
        if (expanded_size_ > ALLOWED_EXPANSION && expanded_size_ / MAX_AMPLIFICATION > input_size_)
        {
            throw error("Maximum entity amplification factor exceeded, see xmlCtxtSetMaxAmplification.");
        }
        active_.add(decl);
    }

    void leave()
    {
        active_.remove(active_.size() - 1);
    }

    void expandChildren(XmlNode node) throws XmlParseException
    {
        if (node instanceof XmlElement e)
        {
            for (XmlAttr a : e.attributes()) expandAttr(e, a);
        }
        XmlNode c = node.firstChild();
        while (c != null)
        {
            XmlNode next = c.next();
            if (c.type() == XmlNodeType.ENTITY_REF)
            {
                XmlEntityDecl decl = expandable(c.name());
                if (decl != null) expandRef(c, decl);
            }
            else if (c.type() == XmlNodeType.ELEMENT)
            {
                expandChildren(c);
            }
            c = next;
        }
    }

    void expandRef(XmlNode ref, XmlEntityDecl decl) throws XmlParseException
    {
        String v = decl.content() == null ? "" : decl.content();
        enter(decl, v.length());
        // Parse into a temporary container at the position of the reference, so that
        // the namespaces in scope there are used.
        XmlElement container = new XmlElement("entity");
        ref.addPrevSibling(container);
        try
        {
            XmlParser p = new XmlParser(v, doc_.url(), options_);
            buf_ = v;
            p.parse_content_chunk(doc_, container);
            expandChildren(container);
        }
        catch (XmlParseException e)
        {
            container.unlink();
            throw e;
        }
        leave();
        XmlNode c = container.firstChild();
        while (c != null)
        {
            XmlNode next = c.next();
            ref.addPrevSibling(c);
            c = next;
        }
        container.unlink();
        ref.unlink();
    }

    // Declared attribute types, "element\u0001attr" -> true for CDATA, false for other types
    // (first declaration wins, libxml2 attsSpecial).
    final java.util.Map<String, Boolean> attr_cdata_ = new java.util.HashMap<>();

    /** Remember the attribute types declared in the dtd. */
    void collectAttrTypes()
    {
        for (XmlNode d = doc_.intSubset().firstChild(); d != null; d = d.next())
        {
            if (d.type() != XmlNodeType.ATTRIBUTE_DECL) continue;
            // The declaration is "<!ATTLIST element attr type ...".
            String t = ((XmlDtdDecl)d).text();
            if (t == null || !t.startsWith("<!ATTLIST ")) continue;
            int es = t.indexOf(' ', 10);
            if (es < 0) continue;
            int as = t.indexOf(' ', es + 1);
            if (as < 0) continue;
            String ty = t.substring(as + 1);
            boolean cdata = ty.equals("CDATA") || ty.startsWith("CDATA ");
            attr_cdata_.putIfAbsent(t.substring(10, es) + "\u0001" + t.substring(es + 1, as), cdata);
        }
    }

    /**
     * Like libxml2 xmlAttrNormalizeSpace2 for the expanded value of an attribute declared
     * with a non CDATA type. Kept entity references count as non space.
     */
    static void normalizeAttr(XmlAttr a)
    {
        boolean started = false;
        boolean pending_space = false;
        XmlNode c = a.firstChild();
        while (c != null)
        {
            XmlNode next = c.next();
            if (c.type() != XmlNodeType.TEXT)
            {
                if (pending_space) c.addPrevSibling(new XmlText(" "));
                pending_space = false;
                started = true;
                c = next;
                continue;
            }
            String t = c.content() == null ? "" : c.content();
            StringBuilder b = new StringBuilder();
            for (int k = 0; k < t.length(); k++)
            {
                char ch = t.charAt(k);
                if (ch == ' ')
                {
                    if (started) pending_space = true;
                    continue;
                }
                if (pending_space) b.append(' ');
                pending_space = false;
                b.append(ch);
                started = true;
            }
            if (b.length() > 0) c.addPrevSibling(new XmlText(b.toString()));
            c.unlink();
            c = next;
        }
    }

    void expandAttr(XmlElement e, XmlAttr a) throws XmlParseException
    {
        boolean any = false;
        for (XmlNode c = a.firstChild(); c != null; c = c.next())
        {
            if (c.type() == XmlNodeType.ENTITY_REF && expandable(c.name()) != null) any = true;
        }
        if (!any) return;
        StringBuilder out = new StringBuilder();
        XmlNode c = a.firstChild();
        while (c != null)
        {
            XmlNode next = c.next();
            XmlEntityDecl decl = null;
            if (c.type() == XmlNodeType.TEXT)
            {
                if (c.content() != null) out.append(c.content());
                c.unlink();
            }
            else if (c.type() == XmlNodeType.ENTITY_REF && (decl = expandable(c.name())) != null)
            {
                c.unlink();
                expandAttrEntity(out, decl);
            }
            else if (out.length() > 0)
            {
                // A kept reference: the text before it becomes a text node.
                c.addPrevSibling(new XmlText(out.toString()));
                out.setLength(0);
            }
            c = next;
        }
        if (out.length() > 0) a.appendRaw(new XmlText(out.toString()));
        if (Boolean.FALSE.equals(attr_cdata_.get(e.fullName() + "\u0001" + a.fullName()))) normalizeAttr(a);
    }

    /** Append the expanded replacement text of an entity referenced from an attribute value. */
    void expandAttrEntity(StringBuilder out, XmlEntityDecl decl) throws XmlParseException
    {
        String v = decl.content() == null ? "" : decl.content();
        if (v.indexOf('<') >= 0) throw error("'<' in entity '" + decl.name() + "' is not allowed in attributes values");
        enter(decl, v.length());
        // Not an input of its own, the end of lines are not normalized (each becomes a space).
        XmlParser p = XmlParser.prepared(v, doc_.url(), options_);
        p.doc_ = doc_;
        buf_ = v;
        {
            while (!p.eof())
            {
                char ch = p.cur();
                if (ch == '&')
                {
                    if (p.nxt(1) == '#')
                    {
                        out.appendCodePoint(p.parse_char_ref());
                        continue;
                    }
                    p.i++;
                    String name = p.parse_name();
                    if (name == null) throw p.fatal("xmlParseEntityRef: no name");
                    if (p.cur() != ';') throw p.fatal("EntityRef: expecting ';'");
                    p.i++;
                    String pre = XmlParser.predefined(name);
                    if (pre != null)
                    {
                        out.append(pre);
                        continue;
                    }
                    XmlEntityDecl d = doc_.getEntity(name);
                    if (d == null) throw p.fatal("Entity '" + name + "' not defined");
                    if (d.etype() != XmlEntityDecl.INTERNAL_GENERAL)
                    {
                        throw p.fatal("Attribute references external entity '" + name + "'");
                    }
                    expandAttrEntity(out, d);
                    // Restore the scan of this value.
                    buf_ = v;
                    continue;
                }
                if (ch == '\t' || ch == '\n' || ch == '\r') out.append(' ');
                else out.append(ch);
                p.i++;
            }
        }
        leave();
    }

    /** Merge adjacent text nodes, created where expanded text meets existing text. */
    static void mergeText(XmlNode node)
    {
        XmlNode c = node.firstChild();
        while (c != null)
        {
            XmlNode next = c.next();
            if (c.type() == XmlNodeType.TEXT)
            {
                while (next != null && next.type() == XmlNodeType.TEXT)
                {
                    c.setContent((c.content() == null ? "" : c.content()) + (next.content() == null ? "" : next.content()));
                    XmlNode nn = next.next();
                    next.unlink();
                    next = nn;
                }
            }
            else if (c.type() == XmlNodeType.ELEMENT)
            {
                mergeText(c);
            }
            c = next;
        }
    }
}
