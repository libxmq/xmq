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
 * The node types of the xmq DOM. They mirror the libxml2 xmlElementType
 * values that xmq uses, since the xmq DOM is modelled after libxml2.
 */
public enum XmlNodeType
{
    /** An element, eg &lt;a&gt;. */
    ELEMENT,
    /** An attribute, eg x="1". */
    ATTRIBUTE,
    /** A text node. */
    TEXT,
    /** A cdata section. */
    CDATA,
    /** An entity reference, eg &amp;copy; */
    ENTITY_REF,
    /** A processing instruction, eg &lt;?target data?&gt; */
    PI,
    /** A comment. */
    COMMENT,
    /** An xml document. */
    DOCUMENT,
    /** An html document. */
    HTML_DOCUMENT,
    /** A document type definition, the internal subset. */
    DTD,
    /** An entity declaration inside a dtd. */
    ENTITY_DECL,
    /** An element declaration inside a dtd. */
    ELEMENT_DECL,
    /** An attribute list declaration inside a dtd. */
    ATTRIBUTE_DECL,
    /** A notation declaration inside a dtd. */
    NOTATION_DECL,
    /** A namespace node, only created by xpath namespace axis evaluation. */
    NAMESPACE
}
