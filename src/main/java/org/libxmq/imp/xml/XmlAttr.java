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
 * An attribute. Like in libxml2 the value is stored as children, text nodes
 * and entity references, so that entity references in attribute values
 * survive parsing and printing.
 */
public class XmlAttr extends XmlNode
{
    XmlNs ns_;

    /**
     * Create an attribute.
     * @param name The local name.
     * @param value The value, or null for no value.
     */
    public XmlAttr(String name, String value)
    {
        super(name);
        if (value != null && !value.isEmpty()) appendRaw(new XmlText(value));
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.ATTRIBUTE;
    }

    /**
     * Returns the namespace of the attribute.
     * @return The namespace or null.
     */
    public XmlNs ns()
    {
        return ns_;
    }

    /**
     * Set the namespace of the attribute.
     * @param ns The namespace or null.
     */
    public void setNs(XmlNs ns)
    {
        ns_ = ns;
    }

    /**
     * Returns the value of the attribute, where entity references contribute
     * the content of their declared entity, like libxml2 xmlGetProp.
     * @return The value, never null.
     */
    public String value()
    {
        return textContent();
    }

    /**
     * Returns the value of the attribute where entity references are written
     * as &amp;name; ie the raw value as it would be written in xmq.
     * @return The raw value, never null.
     */
    public String rawValue()
    {
        StringBuilder sb = new StringBuilder();
        for (XmlNode i = children_; i != null; i = i.next_)
        {
            if (i.type() == XmlNodeType.ENTITY_REF) sb.append('&').append(i.name()).append(';');
            else if (i.content() != null) sb.append(i.content());
        }
        return sb.toString();
    }

    /**
     * Replace the value of the attribute with a text value.
     * @param value The new value.
     */
    public void setValue(String value)
    {
        setContent(value);
    }

    /**
     * Returns the element owning this attribute.
     * @return The element or null.
     */
    public XmlElement element()
    {
        return (XmlElement)parent_;
    }

    @Override
    public String fullName()
    {
        if (ns_ != null && ns_.prefix() != null) return ns_.prefix() + ":" + name_;
        return name_;
    }
}
