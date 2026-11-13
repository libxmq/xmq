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
 * A namespace node, created when the xpath namespace axis is traversed.
 *
 * Like libxml2 (which copies the xmlNs and points its next field to the
 * owning element), a new namespace node is created for every traversal.
 * Two namespace nodes are equal if they belong to the same element and
 * have the same prefix, which is how libxml2 deduplicates them when
 * merging node sets. The semantics are ported from libxml2 xpath.c
 * (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 */
public final class XPathNamespaceNode extends XmlNode
{
    private final String href_;
    private final int index_;

    /**
     * Create a namespace node.
     * @param element The element owning the namespace node, becomes the parent.
     * @param prefix The prefix or null for the default namespace.
     * @param href The namespace uri.
     * @param index The position of this node on the namespace axis of the element.
     */
    public XPathNamespaceNode(XmlElement element, String prefix, String href, int index)
    {
        super(prefix);
        href_ = href;
        index_ = index;
        parent_ = element;
        doc_ = element == null ? null : element.doc();
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.NAMESPACE;
    }

    /**
     * Returns the prefix of the namespace, null for the default namespace.
     * @return The prefix.
     */
    public String prefix()
    {
        return name_;
    }

    /**
     * Returns the namespace uri.
     * @return The uri.
     */
    public String href()
    {
        return href_;
    }

    /**
     * Returns the element owning this namespace node.
     * @return The element.
     */
    public XmlElement element()
    {
        return (XmlElement)parent_;
    }

    /**
     * Returns the position of this node on the namespace axis of its element.
     * @return The index, the xml namespace has index 0.
     */
    public int index()
    {
        return index_;
    }

    @Override
    public String content()
    {
        return href_;
    }

    @Override
    public String textContent()
    {
        return href_ == null ? "" : href_;
    }

    @Override
    public String fullName()
    {
        return name_ == null ? "xmlns" : "xmlns:" + name_;
    }

    @Override
    public boolean equals(Object o)
    {
        if (this == o) return true;
        if (!(o instanceof XPathNamespaceNode)) return false;
        XPathNamespaceNode n = (XPathNamespaceNode)o;
        return n.parent_ == parent_ && java.util.Objects.equals(n.name_, name_);
    }

    @Override
    public int hashCode()
    {
        return System.identityHashCode(parent_) * 31 + (name_ == null ? 0 : name_.hashCode());
    }
}
