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

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * A document type definition, ie the &lt;!DOCTYPE name ...&gt; declaration
 * with its internal subset. The declarations of the internal subset are
 * the children of this node.
 */
public class XmlDtd extends XmlNode
{
    String externalId_;
    String systemId_;
    final Map<String, XmlEntityDecl> entities_ = new LinkedHashMap<>();
    final Map<String, XmlEntityDecl> pentities_ = new LinkedHashMap<>();
    final java.util.List<XmlDtdDecl> notations_ = new java.util.ArrayList<>();
    int num_decls_;

    /**
     * Create a dtd.
     * @param name The doctype name, eg html.
     * @param externalId The public id or null.
     * @param systemId The system id or null.
     */
    public XmlDtd(String name, String externalId, String systemId)
    {
        super(name);
        externalId_ = externalId;
        systemId_ = systemId;
    }

    @Override
    public XmlNodeType type()
    {
        return XmlNodeType.DTD;
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

    /**
     * Add an entity declaration, like libxml2 xmlAddDocEntity. A redeclared
     * entity is ignored, the first declaration wins.
     * @param e The declaration.
     * @return The declaration, or null if it was already declared.
     */
    public XmlEntityDecl addEntity(XmlEntityDecl e)
    {
        Map<String, XmlEntityDecl> m = (e.etype() == XmlEntityDecl.INTERNAL_PARAMETER
                                        || e.etype() == XmlEntityDecl.EXTERNAL_PARAMETER) ? pentities_ : entities_;
        if (m.containsKey(e.name())) return null;
        m.put(e.name(), e);
        appendRaw(e);
        num_decls_++;
        return e;
    }

    /**
     * Add an element, attribute list or notation declaration.
     * @param d The declaration.
     */
    public void addDecl(XmlDtdDecl d)
    {
        appendRaw(d);
        num_decls_++;
    }

    /**
     * Add a notation declaration. Like in libxml2, notations are not
     * children of the dtd, they are kept in a separate table and are
     * printed before the other declarations.
     * @param d The notation declaration.
     */
    public void addNotation(XmlDtdDecl d)
    {
        for (XmlDtdDecl n : notations_)
        {
            if (n.name().equals(d.name())) return;
        }
        notations_.add(d);
        num_decls_++;
    }

    /**
     * Returns the notation declarations.
     * @return The live list of notations.
     */
    public java.util.List<XmlDtdDecl> notations()
    {
        return notations_;
    }

    /**
     * Find a general entity declaration.
     * @param name The entity name.
     * @return The declaration or null.
     */
    public XmlEntityDecl getEntity(String name)
    {
        return entities_.get(name);
    }

    /**
     * Find a parameter entity declaration.
     * @param name The entity name.
     * @return The declaration or null.
     */
    public XmlEntityDecl getParameterEntity(String name)
    {
        return pentities_.get(name);
    }

    /**
     * Returns true if the dtd has any declarations, ie an internal subset.
     * @return True if there are declarations.
     */
    public boolean hasDeclarations()
    {
        return num_decls_ > 0;
    }

    @Override
    void collectText(StringBuilder sb)
    {
    }
}
