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
 * An element. Attributes are kept in source order. Namespace declarations
 * (xmlns and xmlns:prefix) are not attributes, they are stored in the
 * nsDef list, like in libxml2.
 */
public class XmlElement extends XmlNode
{
    XmlNs ns_;
    final List<XmlAttr> attributes_ = new ArrayList<>();
    final List<XmlNs> nsDefs_ = new ArrayList<>();

    /**
     * Create an element.
     * @param name The local name, ie without a namespace prefix.
     */
    public XmlElement(String name)
    {
        super(name);
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.ELEMENT;
    }

    /**
     * Returns the namespace of the element.
     * @return The namespace or null.
     */
    public XmlNs ns()
    {
        return ns_;
    }

    /**
     * Set the namespace of the element.
     * @param ns The namespace or null.
     */
    public void setNs(XmlNs ns)
    {
        ns_ = ns;
    }

    /**
     * Returns the namespace declarations on this element, in source order.
     * @return The live list of declarations.
     */
    public List<XmlNs> nsDefs()
    {
        return nsDefs_;
    }

    /**
     * Declare a namespace on this element, like libxml2 xmlNewNs.
     * If the prefix is already declared on this element, the existing
     * declaration is returned.
     * @param prefix The prefix, null for the default namespace.
     * @param href The uri.
     * @return The declared namespace.
     */
    public XmlNs declareNs(String prefix, String href)
    {
        for (XmlNs n : nsDefs_)
        {
            if (java.util.Objects.equals(n.prefix(), prefix)) return n;
        }
        XmlNs n = new XmlNs(prefix, href);
        nsDefs_.add(n);
        return n;
    }

    /**
     * Search the namespace declared for the prefix on this element or its
     * ancestors, like libxml2 xmlSearchNs.
     * @param prefix The prefix, null for the default namespace.
     * @return The namespace or null.
     */
    public XmlNs searchNs(String prefix)
    {
        if ("xml".equals(prefix)) return XML_NS;
        for (XmlNode n = this; n instanceof XmlElement e; n = n.parent_)
        {
            for (XmlNs ns : e.nsDefs_)
            {
                if (java.util.Objects.equals(ns.prefix(), prefix)) return ns;
            }
        }
        return null;
    }

    /** The predefined xml namespace. */
    public static final XmlNs XML_NS = new XmlNs("xml", "http://www.w3.org/XML/1998/namespace");

    /**
     * Returns the attributes, in source order.
     * @return The live list of attributes.
     */
    public List<XmlAttr> attributes()
    {
        return attributes_;
    }

    /**
     * Add an attribute last, like libxml2 xmlNewProp.
     * @param a The attribute.
     * @return The attribute.
     */
    public XmlAttr addAttribute(XmlAttr a)
    {
        if (a.parent_ instanceof XmlElement old) old.attributes_.remove(a);
        a.parent_ = this;
        a.setDocRecursive(doc_);
        attributes_.add(a);
        return a;
    }

    /**
     * Add or replace an attribute with a text value.
     * @param name The attribute name.
     * @param value The value.
     * @return The attribute.
     */
    public XmlAttr setAttribute(String name, String value)
    {
        XmlAttr a = attribute(name);
        if (a != null)
        {
            a.setValue(value);
            return a;
        }
        return addAttribute(new XmlAttr(name, value));
    }

    /**
     * Remove an attribute.
     * @param a The attribute to remove.
     */
    public void removeAttribute(XmlAttr a)
    {
        if (attributes_.remove(a)) a.parent_ = null;
    }

    /**
     * Find an attribute without a namespace by its name.
     * @param name The name.
     * @return The attribute or null.
     */
    public XmlAttr attribute(String name)
    {
        for (XmlAttr a : attributes_)
        {
            if (a.name().equals(name) && (a.ns() == null || a.ns().prefix() == null)) return a;
        }
        return null;
    }

    /**
     * Find an attribute by its full name, ie prefix:name.
     * @param full_name The full name.
     * @return The attribute or null.
     */
    public XmlAttr attributeByFullName(String full_name)
    {
        for (XmlAttr a : attributes_)
        {
            if (a.fullName().equals(full_name)) return a;
        }
        return null;
    }

    /**
     * Returns the value of an attribute.
     * @param name The attribute name.
     * @return The value or null if the attribute does not exist.
     */
    public String attributeValue(String name)
    {
        XmlAttr a = attribute(name);
        return a == null ? null : a.value();
    }

    /**
     * Returns the element children.
     * @return A new list of the child elements.
     */
    public List<XmlElement> elements()
    {
        List<XmlElement> l = new ArrayList<>();
        for (XmlNode i = children_; i != null; i = i.next_)
        {
            if (i instanceof XmlElement e) l.add(e);
        }
        return l;
    }

    /**
     * Returns the first child element with the given name.
     * @param name The local name.
     * @return The element or null.
     */
    public XmlElement element(String name)
    {
        for (XmlNode i = children_; i != null; i = i.next_)
        {
            if (i instanceof XmlElement e && e.name().equals(name)) return e;
        }
        return null;
    }

    @Override
    public String fullName()
    {
        if (ns_ != null && ns_.prefix() != null) return ns_.prefix() + ":" + name_;
        return name_;
    }

    @Override
    void setDocRecursive(XmlDocument doc)
    {
        super.setDocRecursive(doc);
    }
}
