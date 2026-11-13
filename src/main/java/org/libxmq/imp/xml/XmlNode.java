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
 * Base class of all nodes in the xmq DOM.
 *
 * The DOM is modelled after libxml2 (Copyright (C) 1998-2012 Daniel Veillard,
 * MIT license), since the C version of xmq uses libxml2 and the java version
 * must behave identically. Like libxml2, the nodes are linked with
 * parent/children/last/next/prev pointers, entity references are kept as
 * nodes, and adding a text node next to another text node merges them.
 */
public abstract class XmlNode
{
    XmlNode parent_;
    XmlNode children_;
    XmlNode last_;
    XmlNode next_;
    XmlNode prev_;
    XmlDocument doc_;
    String name_;

    /**
     * Create a node with a name.
     * @param name The node name, null for nameless nodes.
     */
    protected XmlNode(String name)
    {
        name_ = name;
    }

    /**
     * Returns the type of this node.
     * @return The node type.
     */
    public abstract XmlNodeType type();

    /**
     * Returns the name of the node. For an element or attribute this is the
     * local name (without namespace prefix). For a pi it is the target, for
     * an entity reference or declaration it is the entity name.
     * @return The name or null.
     */
    public String name()
    {
        return name_;
    }

    /**
     * Set the name of the node.
     * @param name The new name.
     */
    public void setName(String name)
    {
        name_ = name;
    }

    /**
     * Returns the parent node.
     * @return The parent or null.
     */
    public XmlNode parent()
    {
        return parent_;
    }

    /**
     * Returns the first child.
     * @return The first child or null.
     */
    public XmlNode firstChild()
    {
        return children_;
    }

    /**
     * Returns the last child.
     * @return The last child or null.
     */
    public XmlNode lastChild()
    {
        return last_;
    }

    /**
     * Returns the next sibling.
     * @return The next sibling or null.
     */
    public XmlNode next()
    {
        return next_;
    }

    /**
     * Returns the previous sibling.
     * @return The previous sibling or null.
     */
    public XmlNode prev()
    {
        return prev_;
    }

    /**
     * Returns the document owning this node.
     * @return The document or null.
     */
    public XmlDocument doc()
    {
        return doc_;
    }

    /**
     * Returns true if this node has children.
     * @return True if there is at least one child.
     */
    public boolean hasChildren()
    {
        return children_ != null;
    }

    /**
     * Returns a new list with the children of this node.
     * @return The children, an empty list if none.
     */
    public List<XmlNode> children()
    {
        List<XmlNode> l = new ArrayList<>();
        for (XmlNode n = children_; n != null; n = n.next_) l.add(n);
        return l;
    }

    /**
     * Count the children.
     * @return The number of children.
     */
    public int numChildren()
    {
        int n = 0;
        for (XmlNode i = children_; i != null; i = i.next_) n++;
        return n;
    }

    /**
     * Returns the text content of text like nodes (text, cdata, comment, pi).
     * Returns null for other nodes, use {@link #textContent()} for the
     * concatenated text of an element.
     * @return The content or null.
     */
    public String content()
    {
        return null;
    }

    /**
     * Set the content of a text like node. For an element, all children are
     * replaced with a single text node, like libxml2 xmlNodeSetContent.
     * @param content The new content.
     */
    public void setContent(String content)
    {
        XmlNode i = children_;
        while (i != null)
        {
            XmlNode n = i.next_;
            i.unlink();
            i = n;
        }
        if (content != null && !content.isEmpty())
        {
            appendRaw(new XmlText(content));
        }
    }

    /**
     * Returns the string value of the node, like libxml2 xmlNodeGetContent.
     * For an element this is the concatenation of all descendant text,
     * where entity references contribute the content of the declared entity.
     * @return The text content, never null.
     */
    public String textContent()
    {
        StringBuilder sb = new StringBuilder();
        collectText(sb);
        return sb.toString();
    }

    void collectText(StringBuilder sb)
    {
        for (XmlNode i = children_; i != null; i = i.next_)
        {
            switch (i.type())
            {
            case TEXT, CDATA -> sb.append(i.content());
            case ELEMENT -> i.collectText(sb);
            case ENTITY_REF -> i.collectText(sb);
            default -> { }
            }
        }
    }

    void setDocRecursive(XmlDocument doc)
    {
        doc_ = doc;
        for (XmlNode i = children_; i != null; i = i.next_) i.setDocRecursive(doc);
        if (this instanceof XmlElement e)
        {
            for (XmlAttr a : e.attributes()) a.setDocRecursive(doc);
        }
    }

    /**
     * Append a child without merging adjacent text nodes.
     * @param child The child to append, it is unlinked first.
     * @return The child.
     */
    public XmlNode appendRaw(XmlNode child)
    {
        child.unlink();
        child.parent_ = this;
        child.setDocRecursive(doc_);
        if (last_ == null)
        {
            children_ = last_ = child;
        }
        else
        {
            last_.next_ = child;
            child.prev_ = last_;
            last_ = child;
        }
        return child;
    }

    /**
     * Add a child like libxml2 xmlAddChild. If the child is a text node and
     * the current last child is also a text node, then the text is merged
     * into the existing last child, and the existing node is returned.
     * @param child The child to add.
     * @return The node holding the child content.
     */
    public XmlNode addChild(XmlNode child)
    {
        if (child.type() == XmlNodeType.TEXT && last_ != null && last_.type() == XmlNodeType.TEXT
            && last_.name_ == null && child.name_ == null)
        {
            XmlText t = (XmlText)last_;
            t.setContent(t.content() + child.content());
            return t;
        }
        return appendRaw(child);
    }

    /**
     * Insert a sibling before this node, like libxml2 xmlAddPrevSibling
     * (but without text merging).
     * @param n The node to insert.
     * @return The inserted node.
     */
    public XmlNode addPrevSibling(XmlNode n)
    {
        n.unlink();
        n.parent_ = parent_;
        n.setDocRecursive(doc_);
        n.next_ = this;
        n.prev_ = prev_;
        if (prev_ != null) prev_.next_ = n;
        else if (parent_ != null) parent_.children_ = n;
        prev_ = n;
        return n;
    }

    /**
     * Insert a sibling after this node, like libxml2 xmlAddNextSibling
     * (but without text merging).
     * @param n The node to insert.
     * @return The inserted node.
     */
    public XmlNode addNextSibling(XmlNode n)
    {
        n.unlink();
        n.parent_ = parent_;
        n.setDocRecursive(doc_);
        n.prev_ = this;
        n.next_ = next_;
        if (next_ != null) next_.prev_ = n;
        else if (parent_ != null) parent_.last_ = n;
        next_ = n;
        return n;
    }

    /**
     * Remove this node from its parent and siblings, like libxml2 xmlUnlinkNode.
     */
    public void unlink()
    {
        if (parent_ != null)
        {
            if (parent_.children_ == this) parent_.children_ = next_;
            if (parent_.last_ == this) parent_.last_ = prev_;
            if (parent_ instanceof XmlDocument d && d.intSubset_ == this) d.intSubset_ = null;
        }
        if (prev_ != null) prev_.next_ = next_;
        if (next_ != null) next_.prev_ = prev_;
        parent_ = null;
        next_ = null;
        prev_ = null;
    }

    /**
     * Replace this node with another node, like libxml2 xmlReplaceNode.
     * @param n The replacement.
     */
    public void replaceWith(XmlNode n)
    {
        if (n == this) return;
        if (parent_ == null)
        {
            return;
        }
        addPrevSibling(n);
        unlink();
    }

    /**
     * Returns the full name, ie prefix:name if the node has a namespace with a prefix.
     * @return The full name.
     */
    public String fullName()
    {
        return name_;
    }

    @Override
    public String toString()
    {
        return type() + ":" + fullName();
    }
}
