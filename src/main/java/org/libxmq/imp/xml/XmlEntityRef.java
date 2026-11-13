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
 * An entity reference, eg &amp;copy; The reference is kept as a node in the
 * DOM, the entity is never expanded into the document.
 */
public class XmlEntityRef extends XmlNode
{
    /**
     * Create an entity reference.
     * @param name The entity name, eg copy for &amp;copy;
     */
    public XmlEntityRef(String name)
    {
        super(name);
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.ENTITY_REF;
    }

    /**
     * Find the declaration of the referenced entity in the document dtd.
     * @return The declaration or null.
     */
    public XmlEntityDecl entity()
    {
        if (doc_ == null) return null;
        return doc_.getEntity(name_);
    }

    @Override
    void collectText(StringBuilder sb)
    {
        XmlEntityDecl e = entity();
        if (e != null && e.content() != null) sb.append(e.content());
    }
}
