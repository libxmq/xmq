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
 * An entity declaration in a dtd, eg &lt;!ENTITY copy "&amp;#169;"&gt;
 */
public class XmlEntityDecl extends XmlNode
{
    /** libxml2 XML_INTERNAL_GENERAL_ENTITY */
    public static final int INTERNAL_GENERAL = 1;
    /** libxml2 XML_EXTERNAL_GENERAL_PARSED_ENTITY */
    public static final int EXTERNAL_GENERAL_PARSED = 2;
    /** libxml2 XML_EXTERNAL_GENERAL_UNPARSED_ENTITY */
    public static final int EXTERNAL_GENERAL_UNPARSED = 3;
    /** libxml2 XML_INTERNAL_PARAMETER_ENTITY */
    public static final int INTERNAL_PARAMETER = 4;
    /** libxml2 XML_EXTERNAL_PARAMETER_ENTITY */
    public static final int EXTERNAL_PARAMETER = 5;
    /** libxml2 XML_INTERNAL_PREDEFINED_ENTITY */
    public static final int INTERNAL_PREDEFINED = 6;

    int etype_;
    String content_; // The value with character references expanded.
    String orig_;    // The literal value as written in the source, without quotes.
    String externalId_;
    String systemId_;

    /**
     * Create an entity declaration.
     * @param name The entity name.
     * @param etype The entity type, eg INTERNAL_GENERAL.
     * @param externalId The public id or null.
     * @param systemId The system id or null.
     * @param content The (expanded) content, or the notation name for unparsed entities.
     * @param orig The literal value as written, or null.
     */
    public XmlEntityDecl(String name, int etype, String externalId, String systemId, String content, String orig)
    {
        super(name);
        etype_ = etype;
        externalId_ = externalId;
        systemId_ = systemId;
        content_ = content;
        orig_ = orig;
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.ENTITY_DECL;
    }

    /**
     * Returns the entity type.
     * @return The libxml2 entity type number.
     */
    public int etype()
    {
        return etype_;
    }

    @Override
    public String content()
    {
        return content_;
    }

    /**
     * Returns the value as written in the source.
     * @return The original literal or null.
     */
    public String orig()
    {
        return orig_;
    }

    /**
     * Returns the public id.
     * @return The external id or null.
     */
    public String externalId()
    {
        return externalId_;
    }

    /**
     * Returns the system id.
     * @return The system id or null.
     */
    public String systemId()
    {
        return systemId_;
    }

    @Override
    public String textContent()
    {
        return content_ == null ? "" : content_;
    }
}
