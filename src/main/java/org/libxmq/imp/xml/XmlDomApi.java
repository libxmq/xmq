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
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Objects;

/**
 * The C dom api (src/main/c/parts/xml_dom.h) on the java dom, with the same semantics, so
 * that C code using it can be ported line by line (eg the xslt engine). The C name
 * xml_node_first_child is nodeFirstChild here, XmlDom is XmlDocument, XMQNode is XmlNode,
 * XMQAttr is XmlAttr (attributes are nodes) and XmlNs is XmlNs.
 */
public final class XmlDomApi
{
    private XmlDomApi()
    {
    }

    // The node kinds, the values of XmlNodeKind in the C version (libxml2 xmlElementType).
    /** An element. */
    public static final int ELEMENT = 1;
    /** An attribute. */
    public static final int ATTRIBUTE = 2;
    /** A text node. */
    public static final int TEXT = 3;
    /** A CDATA section. */
    public static final int CDATA = 4;
    /** An entity reference. */
    public static final int ENTITY_REF = 5;
    /** A processing instruction. */
    public static final int PI = 7;
    /** A comment. */
    public static final int COMMENT = 8;
    /** The document. */
    public static final int DOCUMENT = 9;
    /** A document type declaration. */
    public static final int DTD = 14;
    /** An element declaration in a DTD. */
    public static final int ELEMENT_DECL = 15;
    /** An attribute declaration in a DTD. */
    public static final int ATTRIBUTE_DECL = 16;
    /** An entity declaration in a DTD. */
    public static final int ENTITY_DECL = 17;
    /** A namespace declaration. */
    public static final int NAMESPACE = 18;
    /** A notation declaration in a DTD. */
    public static final int NOTATION_DECL = 20;
    /** A parameter entity reference in a DTD. */
    public static final int PE_REF = 21;

    /** The xml namespace uri. */
    public static final String XML_NAMESPACE = "http://www.w3.org/XML/1998/namespace";

    // Documents ///////////////////////////////////////////////////

    /**
     * A new empty document (xml_dom_new).
     * @return The document.
     */
    public static XmlDocument domNew()
    {
        return new XmlDocument();
    }

    /**
     * The document node of a dom, the same object in java.
     * @param dom The dom.
     * @return The document node.
     */
    public static XmlNode domDocument(XmlDocument dom)
    {
        return dom;
    }

    /**
     * The root element.
     * @param dom The dom.
     * @return The root element or null.
     */
    public static XmlNode domRootElement(XmlDocument dom)
    {
        for (XmlNode c = dom.firstChild(); c != null; c = c.next())
        {
            if (c.type() == XmlNodeType.ELEMENT) return c;
        }
        return null;
    }

    /**
     * Replace the root element (or add it).
     * @param dom The dom.
     * @param root The new root element.
     */
    public static void domSetRootElement(XmlDocument dom, XmlNode root)
    {
        XmlNode old = domRootElement(dom);
        if (old == root) return;
        if (old != null)
        {
            old.addPrevSibling(root);
            old.unlink();
        }
        else
        {
            nodeAppendChild(dom, root);
        }
    }

    /**
     * The version of the xml declaration.
     * @param dom The dom.
     * @return The version or null.
     */
    public static String domVersion(XmlDocument dom)
    {
        return dom.version();
    }

    /**
     * Set the version.
     * @param dom The dom.
     * @param v The version.
     */
    public static void domSetVersion(XmlDocument dom, String v)
    {
        dom.setVersion(v);
    }

    /**
     * The encoding of the xml declaration.
     * @param dom The dom.
     * @return The encoding or null.
     */
    public static String domEncoding(XmlDocument dom)
    {
        return dom.encoding();
    }

    /**
     * Set the encoding.
     * @param dom The dom.
     * @param e The encoding.
     */
    public static void domSetEncoding(XmlDocument dom, String e)
    {
        dom.setEncoding(e);
    }

    /**
     * Where the document was loaded from (libxml2 doc->URL).
     * @param dom The dom.
     * @return The url or null.
     */
    public static String domUrl(XmlDocument dom)
    {
        return dom.url();
    }

    /**
     * Set the url.
     * @param dom The dom.
     * @param url The url.
     */
    public static void domSetUrl(XmlDocument dom, String url)
    {
        dom.setUrl(url);
    }

    /**
     * The standalone flag: -1 not given, 0 no, 1 yes.
     * @param dom The dom.
     * @return The flag.
     */
    public static int domStandalone(XmlDocument dom)
    {
        return dom.standalone();
    }

    /**
     * Set the standalone flag.
     * @param dom The dom.
     * @param s The flag.
     */
    public static void domSetStandalone(XmlDocument dom, int s)
    {
        dom.setStandalone(s);
    }

    /**
     * The internal subset.
     * @param dom The dom.
     * @return The dtd or null.
     */
    public static XmlNode domDtd(XmlDocument dom)
    {
        return dom.intSubset();
    }

    /**
     * Replace the dtd with a new one without declarations, before the root element
     * (xml_dom_set_dtd).
     * @param dom The dom.
     * @param name The name.
     * @param public_id The public id or null.
     * @param system_id The system id or null.
     * @return The dtd.
     */
    public static XmlNode domSetDtd(XmlDocument dom, String name, String public_id, String system_id)
    {
        XmlDtd old = dom.intSubset();
        if (old != null) old.unlink();
        dom.setIntSubsetRef(null);
        XmlDtd dtd = new XmlDtd(name, public_id, system_id);
        XmlNode root = domRootElement(dom);
        if (root != null) root.addPrevSibling(dtd);
        else dom.appendRaw(dtd);
        dom.setIntSubsetRef(dtd);
        dtd.setDocRecursive(dom);
        return dtd;
    }

    /**
     * The public id of a dtd.
     * @param dtd The dtd.
     * @return The public id or null.
     */
    public static String dtdPublicId(XmlNode dtd)
    {
        return ((XmlDtd)dtd).externalId();
    }

    /**
     * The system id of a dtd.
     * @param dtd The dtd.
     * @return The system id or null.
     */
    public static String dtdSystemId(XmlNode dtd)
    {
        return ((XmlDtd)dtd).systemId();
    }

    /**
     * A general entity declared in the internal subset.
     * @param dom The dom.
     * @param name The name.
     * @return The declaration or null.
     */
    public static XmlNode domGetEntity(XmlDocument dom, String name)
    {
        return dom.getEntity(name);
    }

    /**
     * The system id of an entity declaration.
     * @param decl The declaration.
     * @return The system id or null.
     */
    public static String entityDeclSystemId(XmlNode decl)
    {
        return ((XmlEntityDecl)decl).systemId();
    }

    // Nodes ///////////////////////////////////////////////////////

    private static <T extends XmlNode> T owned(XmlDocument dom, T n)
    {
        n.doc_ = dom;
        return n;
    }

    /**
     * A new element (xml_node_new_element).
     * @param dom The dom.
     * @param name The local name.
     * @param ns The namespace or null.
     * @return The element.
     */
    public static XmlNode nodeNewElement(XmlDocument dom, String name, XmlNs ns)
    {
        XmlElement e = owned(dom, new XmlElement(name));
        e.setNs(ns);
        return e;
    }

    /**
     * A new text node.
     * @param dom The dom.
     * @param text The text, null is "".
     * @return The node.
     */
    public static XmlNode nodeNewText(XmlDocument dom, String text)
    {
        return owned(dom, new XmlText(text == null ? "" : text));
    }

    /**
     * A new cdata section.
     * @param dom The dom.
     * @param text The content.
     * @return The node.
     */
    public static XmlNode nodeNewCdata(XmlDocument dom, String text)
    {
        return owned(dom, new XmlCData(text == null ? "" : text));
    }

    /**
     * A new comment.
     * @param dom The dom.
     * @param text The content.
     * @return The node.
     */
    public static XmlNode nodeNewComment(XmlDocument dom, String text)
    {
        return owned(dom, new XmlComment(text));
    }

    /**
     * A new processing instruction.
     * @param dom The dom.
     * @param target The target.
     * @param data The data or null.
     * @return The node.
     */
    public static XmlNode nodeNewPi(XmlDocument dom, String target, String data)
    {
        return owned(dom, new XmlPI(target, data));
    }

    /**
     * A new entity reference.
     * @param dom The dom.
     * @param name The entity name.
     * @return The node.
     */
    public static XmlNode nodeNewEntityRef(XmlDocument dom, String name)
    {
        return owned(dom, new XmlEntityRef(name));
    }

    /**
     * An xpath namespace node of element, not linked into the element.
     * @param element The element.
     * @param prefix The prefix or null.
     * @param href The uri.
     * @param index The position on the namespace axis (stored as its line in C).
     * @return The node.
     */
    public static XmlNode nodeNewNamespaceNode(XmlNode element, String prefix, String href, int index)
    {
        return new XPathNamespaceNode((XmlElement)element, prefix, href, index);
    }

    /**
     * Unlink a node (the garbage collector frees it).
     * @param n The node.
     */
    public static void nodeFree(XmlNode n)
    {
        if (n != null) nodeUnlink(n);
    }

    /**
     * The kind of a node, the C XmlNodeKind values.
     * @param n The node.
     * @return The kind.
     */
    public static int nodeKind(XmlNode n)
    {
        switch (n.type())
        {
        case ELEMENT: return ELEMENT;
        case ATTRIBUTE: return ATTRIBUTE;
        case TEXT: return TEXT;
        case CDATA: return CDATA;
        case ENTITY_REF: return ENTITY_REF;
        case PI: return PI;
        case COMMENT: return COMMENT;
        case DOCUMENT: case HTML_DOCUMENT: return DOCUMENT;
        case DTD: return DTD;
        case ELEMENT_DECL: return ELEMENT_DECL;
        case ATTRIBUTE_DECL: return ATTRIBUTE_DECL;
        case ENTITY_DECL: return ENTITY_DECL;
        case NOTATION_DECL: return NOTATION_DECL;
        case PE_REF: return PE_REF;
        case NAMESPACE: return NAMESPACE;
        default: return 0;
        }
    }

    /**
     * The dom of a node.
     * @param n The node.
     * @return The document.
     */
    public static XmlDocument nodeDom(XmlNode n)
    {
        if (n instanceof XmlDocument d) return d;
        if (n.doc_ != null) return n.doc_;
        XmlNode i = n;
        while (i.parent_ != null) i = i.parent_;
        return i instanceof XmlDocument d ? d : null;
    }

    /**
     * The parent (the owner element of an attribute or namespace node).
     * @param n The node.
     * @return The parent or null.
     */
    public static XmlNode nodeParent(XmlNode n)
    {
        return n.parent_;
    }

    /**
     * The first child.
     * @param n The node.
     * @return The child or null.
     */
    public static XmlNode nodeFirstChild(XmlNode n)
    {
        return n.children_;
    }

    /**
     * The last child.
     * @param n The node.
     * @return The child or null.
     */
    public static XmlNode nodeLastChild(XmlNode n)
    {
        return n.last_;
    }

    /**
     * The next sibling (for an attribute the next attribute).
     * @param n The node.
     * @return The sibling or null.
     */
    public static XmlNode nodeNext(XmlNode n)
    {
        if (n instanceof XmlAttr a) return attrNext(a);
        return n.next_;
    }

    /**
     * The previous sibling (for an attribute the previous attribute).
     * @param n The node.
     * @return The sibling or null.
     */
    public static XmlNode nodePrev(XmlNode n)
    {
        if (n instanceof XmlAttr a)
        {
            if (!(a.parent_ instanceof XmlElement e)) return null;
            int i = indexOf(e.attributes_, a);
            return i > 0 ? e.attributes_.get(i - 1) : null;
        }
        return n.prev_;
    }

    private static int indexOf(List<XmlAttr> l, XmlAttr a)
    {
        for (int i = 0; i < l.size(); i++) if (l.get(i) == a) return i;
        return -1;
    }

    /**
     * The name: local name of elements and attributes, the target of a pi, the entity name
     * of a reference, the prefix of a namespace node, null for text ("textnoenc" for text
     * written without escaping), comments and documents.
     * @param n The node.
     * @return The name or null.
     */
    public static String nodeName(XmlNode n)
    {
        if (n instanceof XmlDocument) return null;
        return n.name_;
    }

    /**
     * Set the name.
     * @param n The node.
     * @param name The name.
     */
    public static void nodeSetName(XmlNode n, String name)
    {
        n.name_ = name;
    }

    /**
     * The content of text, cdata, comments, pis (the data) and namespace nodes (the uri),
     * null for other nodes.
     * @param n The node.
     * @return The content or null.
     */
    public static String nodeContent(XmlNode n)
    {
        switch (n.type())
        {
        case TEXT: case CDATA: case COMMENT: case PI: case NAMESPACE:
            return n.content();
        case ELEMENT_DECL: case ATTRIBUTE_DECL: case NOTATION_DECL: case PE_REF:
            return ((XmlDtdDecl)n).text();
        case ENTITY_DECL:
            return n.content();
        default:
            return null;
        }
    }

    private static boolean hasChildContent(XmlNode n)
    {
        switch (n.type())
        {
        case ELEMENT: case ATTRIBUTE: case DOCUMENT: case HTML_DOCUMENT: case DTD: case ENTITY_REF:
            return true;
        default:
            return false;
        }
    }

    /**
     * Set the content, for elements and attributes the children are replaced with one
     * text node (none for "").
     * @param n The node.
     * @param content The content.
     */
    public static void nodeSetContent(XmlNode n, String content)
    {
        if (hasChildContent(n))
        {
            XmlNode c = n.children_;
            while (c != null)
            {
                XmlNode next = c.next_;
                c.unlink();
                c = next;
            }
            if (content != null && !content.isEmpty())
            {
                nodeAppendChild(n, nodeNewText(nodeDom(n), content));
            }
            return;
        }
        n.setContent(content);
    }

    /**
     * Append to the content of a text like node.
     * @param n The node.
     * @param content The text to append.
     */
    public static void nodeAppendContent(XmlNode n, String content)
    {
        String old = n.content();
        n.setContent(old == null ? content : old + content);
    }

    /**
     * The string value: text content of elements, attributes and documents, the character
     * of a character reference (other entity references contribute nothing).
     * @param n The node.
     * @return The string value, never null.
     */
    public static String nodeStringValue(XmlNode n)
    {
        if (hasChildContent(n) || n.type() == XmlNodeType.ENTITY_REF) return n.textContent();
        String c = n.content();
        return c == null ? "" : c;
    }

    /**
     * The namespace of an element or attribute.
     * @param n The node.
     * @return The namespace or null.
     */
    public static XmlNs nodeNs(XmlNode n)
    {
        if (n instanceof XmlElement e) return e.ns();
        if (n instanceof XmlAttr a) return a.ns();
        return null;
    }

    /**
     * The namespace uri of the node, null if none (or "").
     * @param n The node.
     * @return The uri or null.
     */
    public static String nodeNsUri(XmlNode n)
    {
        XmlNs ns = nodeNs(n);
        if (ns == null || ns.href() == null || ns.href().isEmpty()) return null;
        return ns.href();
    }

    /**
     * The prefix of the namespace of the node.
     * @param n The node.
     * @return The prefix or null.
     */
    public static String nodeNsPrefix(XmlNode n)
    {
        XmlNs ns = nodeNs(n);
        return ns == null ? null : ns.prefix();
    }

    /**
     * Set the namespace of an element or attribute.
     * @param n The node.
     * @param ns The namespace.
     */
    public static void nodeSetNs(XmlNode n, XmlNs ns)
    {
        if (n instanceof XmlElement e) e.setNs(ns);
        else if (n instanceof XmlAttr a) a.setNs(ns);
    }

    /**
     * The source line.
     * @param n The node.
     * @return The line.
     */
    public static int nodeLine(XmlNode n)
    {
        if (n instanceof XPathNamespaceNode ns) return ns.index();
        return n.line_;
    }

    /**
     * Set the source line.
     * @param n The node.
     * @param line The line.
     */
    public static void nodeSetLine(XmlNode n, int line)
    {
        n.line_ = line;
    }

    /**
     * The user data.
     * @param n The node.
     * @return The data or null.
     */
    public static Object nodeUserData(XmlNode n)
    {
        return n.user_data_;
    }

    /**
     * Set the user data.
     * @param n The node.
     * @param data The data.
     */
    public static void nodeSetUserData(XmlNode n, Object data)
    {
        n.user_data_ = data;
    }

    /**
     * Append a child without merging text (xml_node_append_child), the child is unlinked first.
     * @param parent The parent.
     * @param child The child.
     * @return The child.
     */
    public static XmlNode nodeAppendChild(XmlNode parent, XmlNode child)
    {
        child.unlink();
        if (parent instanceof XmlDocument d && child instanceof XmlDtd dtd && d.intSubset() == null)
        {
            d.setIntSubsetRef(dtd);
        }
        return parent.appendRaw(child);
    }

    /**
     * Append a child, text is merged into a last text child (xml_node_add_child), then the
     * child is dropped and the last child returned.
     * @param parent The parent.
     * @param child The child.
     * @return The node holding the child.
     */
    public static XmlNode nodeAddChild(XmlNode parent, XmlNode child)
    {
        XmlNode last = parent.last_;
        if (child.type() == XmlNodeType.TEXT && last != null && last != child && last.type() == XmlNodeType.TEXT)
        {
            String c = child.content();
            nodeAppendContent(last, c == null ? "" : c);
            child.unlink();
            return last;
        }
        return nodeAppendChild(parent, child);
    }

    /**
     * Insert n before node.
     * @param node The node.
     * @param n The new sibling.
     * @return n.
     */
    public static XmlNode nodeAddPrevSibling(XmlNode node, XmlNode n)
    {
        if (n == node) return n;
        return node.addPrevSibling(n);
    }

    /**
     * Insert n after node.
     * @param node The node.
     * @param n The new sibling.
     * @return n.
     */
    public static XmlNode nodeAddNextSibling(XmlNode node, XmlNode n)
    {
        if (n == node) return n;
        return node.addNextSibling(n);
    }

    /**
     * Unlink a node from its parent (an attribute from its element).
     * @param n The node.
     */
    public static void nodeUnlink(XmlNode n)
    {
        if (n instanceof XPathNamespaceNode)
        {
            n.parent_ = null;
            return;
        }
        if (n instanceof XmlAttr a)
        {
            if (a.parent_ instanceof XmlElement e) e.removeAttribute(a);
            a.parent_ = null;
            return;
        }
        if (n.parent_ instanceof XmlDocument d && d.intSubset() == n) d.setIntSubsetRef(null);
        n.unlink();
    }

    /**
     * Replace node with n.
     * @param node The node.
     * @param n The replacement.
     * @return node.
     */
    public static XmlNode nodeReplace(XmlNode node, XmlNode n)
    {
        if (n == node || node.parent_ == null) return node;
        nodeAddPrevSibling(node, n);
        nodeUnlink(node);
        return node;
    }

    /**
     * The number of children.
     * @param n The node.
     * @return The count.
     */
    public static int nodeNumChildren(XmlNode n)
    {
        int k = 0;
        for (XmlNode c = n.children_; c != null; c = c.next_) k++;
        return k;
    }

    /**
     * Copy a node (not a document) into dom, deep with the children, the namespaces used are
     * the copies of the declarations copied with it or the original ones (xml_node_copy).
     * @param dom The dom of the copy.
     * @param node The node.
     * @param deep Copy the children too.
     * @return The copy.
     */
    public static XmlNode nodeCopy(XmlDocument dom, XmlNode node, boolean deep)
    {
        return copyInternal(dom, node, deep, new IdentityHashMap<>());
    }

    private static XmlNs mapNs(IdentityHashMap<XmlNs, XmlNs> map, XmlNs ns)
    {
        if (ns == null) return null;
        XmlNs m = map.get(ns);
        return m != null ? m : ns;
    }

    private static XmlNode copyInternal(XmlDocument dom, XmlNode node, boolean deep, IdentityHashMap<XmlNs, XmlNs> map)
    {
        XmlNode n;
        switch (node.type())
        {
        case ELEMENT ->
        {
            XmlElement from = (XmlElement)node;
            XmlElement to = new XmlElement(from.name_);
            for (XmlNs d : from.nsDefs())
            {
                XmlNs nd = new XmlNs(d.prefix(), d.href());
                map.put(d, nd);
                to.nsDefs().add(nd);
            }
            to.setNs(mapNs(map, from.ns()));
            to.doc_ = dom;
            for (XmlAttr a : from.attributes())
            {
                XmlAttr na = (XmlAttr)copyInternal(dom, a, true, map);
                na.parent_ = to;
                to.attributes_.add(na);
            }
            n = to;
        }
        case ATTRIBUTE ->
        {
            XmlAttr from = (XmlAttr)node;
            XmlAttr to = new XmlAttr(from.name_, null);
            to.setNs(mapNs(map, from.ns()));
            n = to;
            deep = true; // The value is stored as children.
        }
        case TEXT -> n = new XmlText(node.content());
        case CDATA -> n = new XmlCData(node.content());
        case COMMENT -> n = new XmlComment(node.content());
        case PI -> n = new XmlPI(node.name_, node.content());
        case ENTITY_REF -> n = new XmlEntityRef(node.name_);
        case DTD ->
        {
            XmlDtd from = (XmlDtd)node;
            XmlDtd to = new XmlDtd(from.name_, from.externalId(), from.systemId());
            for (XmlDtdDecl d : from.notations()) to.addNotation(new XmlDtdDecl(d.type(), d.name_, d.text()));
            n = to;
            deep = true;
        }
        case ENTITY_DECL ->
        {
            XmlEntityDecl from = (XmlEntityDecl)node;
            n = new XmlEntityDecl(from.name_, from.etype(), from.externalId(), from.systemId(), from.content(), from.orig());
        }
        case ELEMENT_DECL, ATTRIBUTE_DECL, NOTATION_DECL, PE_REF ->
        {
            XmlDtdDecl from = (XmlDtdDecl)node;
            n = new XmlDtdDecl(from.type(), from.name_, from.text());
        }
        case NAMESPACE ->
        {
            XPathNamespaceNode from = (XPathNamespaceNode)node;
            n = new XPathNamespaceNode(from.element(), from.prefix(), from.href(), from.index());
        }
        default -> throw new IllegalArgumentException("Cannot copy " + node.type());
        }
        if (n.name_ == null || node.type() == XmlNodeType.TEXT) n.name_ = node.name_;
        n.line_ = node.line_;
        n.doc_ = dom;
        if (deep)
        {
            for (XmlNode c = node.children_; c != null; c = c.next_)
            {
                XmlNode nc = copyInternal(dom, c, true, map);
                n.appendRaw(nc);
            }
        }
        return n;
    }

    private static int depth(XmlNode n)
    {
        int d = 0;
        while (n.parent_ != null)
        {
            n = n.parent_;
            d++;
        }
        return d;
    }

    /**
     * Compare two nodes in document order (xml_node_compare_order): -1, 0 or 1. Nodes of
     * different trees are ordered by the creation of their roots.
     * @param a A node.
     * @param b Another node.
     * @return The order.
     */
    public static int nodeCompareOrder(XmlNode a, XmlNode b)
    {
        if (a == b) return 0;
        int da = depth(a), db = depth(b);
        XmlNode ia = a, ib = b;
        while (da > db)
        {
            ia = ia.parent_;
            da--;
        }
        while (db > da)
        {
            ib = ib.parent_;
            db--;
        }
        // One is an ancestor (or owner element) of the other, the ancestor comes first.
        if (ia == ib) return a == ia ? -1 : 1;
        while (ia.parent_ != ib.parent_)
        {
            ia = ia.parent_;
            ib = ib.parent_;
        }
        if (ia.parent_ == null) return ia.seq_ < ib.seq_ ? -1 : 1;
        boolean aa = ia instanceof XmlAttr, ba = ib instanceof XmlAttr;
        if (aa && !ba) return -1; // Attributes come before children.
        if (ba && !aa) return 1;
        if (aa)
        {
            List<XmlAttr> l = ((XmlElement)ia.parent_).attributes_;
            return indexOf(l, (XmlAttr)ia) < indexOf(l, (XmlAttr)ib) ? -1 : 1;
        }
        for (XmlNode i = ia.next_; i != null; i = i.next_)
        {
            if (i == ib) return -1;
        }
        return 1;
    }

    // Attributes //////////////////////////////////////////////////

    private static boolean attrMatches(XmlAttr a, String name, String ns_uri)
    {
        if (!Objects.equals(a.name_, name)) return false;
        return Objects.equals(nodeNsUri(a), ns_uri);
    }

    /**
     * The attributes of an element (the live list, copy it to change attributes while
     * iterating).
     * @param e The element.
     * @return The attributes.
     */
    public static List<XmlAttr> nodeAttrs(XmlNode e)
    {
        if (e instanceof XmlElement el) return el.attributes_;
        return List.of();
    }

    /**
     * The first attribute.
     * @param e The element.
     * @return The attribute or null.
     */
    public static XmlAttr nodeFirstAttr(XmlNode e)
    {
        List<XmlAttr> l = nodeAttrs(e);
        return l.isEmpty() ? null : l.get(0);
    }

    /**
     * The next attribute.
     * @param a The attribute.
     * @return The next or null.
     */
    public static XmlAttr attrNext(XmlAttr a)
    {
        if (!(a.parent_ instanceof XmlElement e)) return null;
        int i = indexOf(e.attributes_, a);
        return i >= 0 && i + 1 < e.attributes_.size() ? e.attributes_.get(i + 1) : null;
    }

    /**
     * The name of an attribute.
     * @param a The attribute.
     * @return The local name.
     */
    public static String attrName(XmlAttr a)
    {
        return a.name_;
    }

    /**
     * The element of an attribute.
     * @param a The attribute.
     * @return The element or null.
     */
    public static XmlNode attrOwner(XmlAttr a)
    {
        return a.parent_;
    }

    /**
     * The attribute as a node, the same object in java.
     * @param a The attribute.
     * @return The node.
     */
    public static XmlNode attrNode(XmlAttr a)
    {
        return a;
    }

    /**
     * The node as an attribute.
     * @param n The node.
     * @return The attribute or null.
     */
    public static XmlAttr nodeAsAttr(XmlNode n)
    {
        return n instanceof XmlAttr a ? a : null;
    }

    /**
     * The attribute with this name and namespace uri (null or "" for none).
     * @param e The element.
     * @param name The local name.
     * @param ns_uri The uri.
     * @return The attribute or null.
     */
    public static XmlAttr nodeGetAttr(XmlNode e, String name, String ns_uri)
    {
        if (ns_uri != null && ns_uri.isEmpty()) ns_uri = null;
        for (XmlAttr a : nodeAttrs(e))
        {
            if (attrMatches(a, name, ns_uri)) return a;
        }
        return null;
    }

    /**
     * Append an attribute (without checking for an existing one).
     * @param e The element.
     * @param name The local name.
     * @param ns The namespace or null.
     * @param value The value, null or "" for no text child.
     * @return The attribute.
     */
    public static XmlAttr nodeAppendAttr(XmlNode e, String name, XmlNs ns, String value)
    {
        XmlElement el = (XmlElement)e;
        XmlAttr a = new XmlAttr(name, null);
        a.setNs(ns);
        a.doc_ = el.doc_;
        a.parent_ = el;
        nodeSetContent(a, value);
        el.attributes_.add(a);
        return a;
    }

    /**
     * Set an attribute, replacing the value (and namespace) of an existing one.
     * @param e The element.
     * @param name The local name.
     * @param ns The namespace or null.
     * @param value The value.
     * @return The attribute.
     */
    public static XmlAttr nodeSetAttr(XmlNode e, String name, XmlNs ns, String value)
    {
        String uri = ns != null && ns.href() != null && !ns.href().isEmpty() ? ns.href() : null;
        XmlAttr a = nodeGetAttr(e, name, uri);
        if (a != null)
        {
            nodeSetContent(a, value);
            a.setNs(ns);
            return a;
        }
        return nodeAppendAttr(e, name, ns, value);
    }

    /**
     * Move an attribute to element, an attribute with the same name and namespace is
     * replaced (xml_node_move_attr).
     * @param e The element.
     * @param a The attribute.
     * @return The attribute.
     */
    public static XmlAttr nodeMoveAttr(XmlNode e, XmlAttr a)
    {
        if (a.parent_ == e) return a;
        nodeRemoveAttr(e, a.name_, a.ns() != null ? a.ns().href() : null);
        nodeUnlink(a);
        a.parent_ = e;
        a.setDocRecursive(((XmlElement)e).doc_);
        ((XmlElement)e).attributes_.add(a);
        return a;
    }

    /**
     * Remove the attribute with this name and namespace uri.
     * @param e The element.
     * @param name The local name.
     * @param ns_uri The uri or null.
     * @return True if one was removed.
     */
    public static boolean nodeRemoveAttr(XmlNode e, String name, String ns_uri)
    {
        XmlAttr a = nodeGetAttr(e, name, ns_uri);
        if (a == null) return false;
        nodeUnlink(a);
        return true;
    }

    // Namespaces //////////////////////////////////////////////////

    /**
     * The prefix of a namespace.
     * @param ns The namespace.
     * @return The prefix or null.
     */
    public static String nsPrefix(XmlNs ns)
    {
        return ns.prefix();
    }

    /**
     * The uri of a namespace, "" for an undeclaration, null for a prefix declared without uri.
     * @param ns The namespace.
     * @return The uri.
     */
    public static String nsHref(XmlNs ns)
    {
        return ns.href();
    }

    /**
     * Set the uri.
     * @param ns The namespace.
     * @param href The uri.
     */
    public static void nsSetHref(XmlNs ns, String href)
    {
        ns.setHref(href);
    }

    /**
     * The namespace declarations of an element (the live list, copy it to change it while
     * iterating).
     * @param e The element.
     * @return The declarations.
     */
    public static List<XmlNs> nodeNsDecls(XmlNode e)
    {
        if (e instanceof XmlElement el) return el.nsDefs();
        return List.of();
    }

    /**
     * Declare a namespace on element, null if the prefix is already declared there
     * (xml_ns_declare).
     * @param e The element.
     * @param prefix The prefix or null.
     * @param href The uri.
     * @return The declaration or null.
     */
    public static XmlNs nsDeclare(XmlNode e, String prefix, String href)
    {
        List<XmlNs> l = ((XmlElement)e).nsDefs();
        for (XmlNs d : l)
        {
            if (Objects.equals(d.prefix(), prefix)) return null;
        }
        XmlNs ns = new XmlNs(prefix, href);
        l.add(ns);
        return ns;
    }

    /**
     * Move the declaration ns from element from to the front of the declarations of to.
     * @param from The element declaring ns.
     * @param ns The declaration.
     * @param to The new element.
     */
    public static void nsMoveDeclTo(XmlNode from, XmlNs ns, XmlNode to)
    {
        List<XmlNs> l = ((XmlElement)from).nsDefs();
        int i = -1;
        for (int k = 0; k < l.size(); k++) if (l.get(k) == ns) i = k;
        if (i < 0) return;
        l.remove(i);
        ((XmlElement)to).nsDefs().add(0, ns);
    }

    /**
     * Move the declaration ns last among the declarations of element.
     * @param e The element.
     * @param ns The declaration.
     */
    public static void nsMoveDeclLast(XmlNode e, XmlNs ns)
    {
        List<XmlNs> l = ((XmlElement)e).nsDefs();
        int i = -1;
        for (int k = 0; k < l.size(); k++) if (l.get(k) == ns) i = k;
        if (i < 0 || i == l.size() - 1) return;
        l.remove(i);
        l.add(ns);
    }

    private static void repointNs(XmlNode node, XmlNs from, XmlNs to)
    {
        if (!(node instanceof XmlElement e)) return;
        if (e.ns() == from) e.setNs(to);
        for (XmlAttr a : e.attributes_) if (a.ns() == from) a.setNs(to);
        for (XmlNode c = node.children_; c != null; c = c.next_) repointNs(c, from, to);
    }

    /**
     * Remove the declaration ns from element, the nodes of the subtree using it use
     * replacement instead (xml_ns_remove_decl).
     * @param e The element.
     * @param ns The declaration.
     * @param replacement The namespace to use instead, or null.
     */
    public static void nsRemoveDecl(XmlNode e, XmlNs ns, XmlNs replacement)
    {
        List<XmlNs> l = ((XmlElement)e).nsDefs();
        int i = -1;
        for (int k = 0; k < l.size(); k++) if (l.get(k) == ns) i = k;
        if (i < 0) return;
        l.remove(i);
        repointNs(e, ns, replacement);
    }

    /**
     * Find the declaration of prefix in scope of node (xmlSearchNs), the xml prefix is
     * always declared.
     * @param node The node.
     * @param prefix The prefix, null for the default namespace.
     * @return The declaration or null.
     */
    public static XmlNs nsLookupPrefix(XmlNode node, String prefix)
    {
        if ("xml".equals(prefix)) return XmlElement.XML_NS;
        if (node instanceof XmlAttr) node = node.parent_;
        for (XmlNode i = node; i != null; i = i.parent_)
        {
            if (!(i instanceof XmlElement e)) continue;
            for (XmlNs d : e.nsDefs())
            {
                if (Objects.equals(d.prefix(), prefix)) return d;
            }
        }
        return null;
    }

    /**
     * Find a declaration of href in scope of node whose prefix is not shadowed
     * (xmlSearchNsByHref).
     * @param node The node.
     * @param href The uri.
     * @return The declaration or null.
     */
    public static XmlNs nsLookupHref(XmlNode node, String href)
    {
        if (href == null) return null;
        if (href.equals(XML_NAMESPACE)) return XmlElement.XML_NS;
        XmlNode start = node instanceof XmlAttr ? node.parent_ : node;
        for (XmlNode i = start; i != null; i = i.parent_)
        {
            if (!(i instanceof XmlElement e)) continue;
            for (XmlNs d : e.nsDefs())
            {
                if (!Objects.equals(d.href(), href)) continue;
                if (nsLookupPrefix(start, d.prefix()) == d) return d;
            }
        }
        return null;
    }

    /** Declare href with the first free prefix ns1, ns2, ... on decl (xml_ns_declare_unique). */
    private static XmlNs declareUnique(XmlNode check, XmlNode decl, String href)
    {
        for (long i = 1; ; i++)
        {
            String prefix = "ns" + i;
            if (nsLookupPrefix(check, prefix) == null && nsLookupPrefix(decl, prefix) == null)
            {
                return nsDeclare(decl, prefix, href);
            }
        }
    }

    private static void reconcileOne(XmlNode root, XmlNode n)
    {
        XmlNs ns = nodeNs(n);
        if (ns == null || ns == XmlElement.XML_NS) return;
        boolean is_attr = n instanceof XmlAttr;
        // An attribute cannot use the default namespace, it needs a prefix.
        if (!(is_attr && ns.prefix() == null))
        {
            XmlNs found = nsLookupPrefix(n, ns.prefix());
            if (found == ns) return;
            if (found != null && Objects.equals(found.href(), ns.href()))
            {
                nodeSetNs(n, found);
                return;
            }
        }
        else
        {
            XmlNs found = nsLookupHref(n, ns.href());
            if (found != null && found.prefix() != null)
            {
                nodeSetNs(n, found);
                return;
            }
        }
        // Not in scope. Declare it on the root of the subtree, if the prefix is free there.
        XmlNs nd = null;
        if (!(is_attr && ns.prefix() == null) && nsLookupPrefix(root, ns.prefix()) == null)
        {
            nd = nsDeclare(root, ns.prefix(), ns.href());
        }
        if (nd == null)
        {
            XmlNs existing = nsLookupHref(root, ns.href());
            if (existing != null && (existing.prefix() != null || !is_attr)) nd = existing;
            else nd = declareUnique(n, root, ns.href());
        }
        nodeSetNs(n, nd);
    }

    private static void reconcileRecursive(XmlNode root, XmlNode n)
    {
        if (n instanceof XmlElement e)
        {
            reconcileOne(root, n);
            for (XmlAttr a : new ArrayList<>(e.attributes_)) reconcileOne(root, a);
        }
        for (XmlNode c = n.children_; c != null; c = c.next_) reconcileRecursive(root, c);
    }

    /**
     * Make every namespace used in the subtree of node declared in scope (xml_node_reconcile_ns).
     * @param node The element.
     */
    public static void nodeReconcileNs(XmlNode node)
    {
        if (!(node instanceof XmlElement)) return;
        reconcileRecursive(node, node);
    }
}
