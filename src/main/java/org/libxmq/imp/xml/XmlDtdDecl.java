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
 * An element, attribute list or notation declaration in a dtd. The
 * declaration is stored as normalized source text, eg &lt;!ELEMENT a (b|c)*&gt;
 */
public class XmlDtdDecl extends XmlNode
{
    final XmlNodeType type_;
    final String text_;

    /**
     * Create a declaration.
     * @param type ELEMENT_DECL, ATTRIBUTE_DECL or NOTATION_DECL.
     * @param name The declared name.
     * @param text The full normalized declaration text.
     */
    public XmlDtdDecl(XmlNodeType type, String name, String text)
    {
        super(name);
        type_ = type;
        text_ = text;
    }

    @Override
    public XmlNodeType type()
    {
        return type_;
    }

    /**
     * Returns the declaration text.
     * @return The declaration, eg &lt;!ELEMENT a EMPTY&gt;
     */
    public String text()
    {
        return text_;
    }
}
