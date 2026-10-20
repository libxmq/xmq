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

/** SAX-style handler. All methods are optional in the sense that a
 *  handler is free to ignore events it does not need. */
public interface Handler
{
    /** A doctype declaration. Subset is the raw internal subset with
     *  each line trimmed and empty lines dropped, joined by newlines,
     *  or null if there is no internal subset.
     *  @param name the doctype name
     *  @param subset the internal subset, or null
     *  @throws Exception for handler errors */
    void doctype(String name, String subset) throws Exception;

    /** A processing instruction. Data has leading whitespace removed.
     *  @param target the instruction target
     *  @param data the instruction data
     *  @throws Exception for handler errors */
    void processingInstruction(String target, String data) throws Exception;

    /** A comment, without the surrounding comment marks.
     *  @param text the comment text
     *  @throws Exception for handler errors */
    void comment(String text) throws Exception;

    /** Start of an element. Attribute names and values are in source
     *  order. Values are decoded: character references and predefined
     *  entity references are expanded to characters, but other named
     *  references are kept as literal '&lt;name&gt;;' text.
     *  @param name the element name
     *  @param attributeNames the attribute names
     *  @param attributeValues the attribute values
     *  @throws Exception for handler errors */
    void startElement(String name, String[] attributeNames, String[] attributeValues) throws Exception;

    /** End of an element.
     *  @param name the element name
     *  @throws Exception for handler errors */
    void endElement(String name) throws Exception;

    /** Character data. Character references are already expanded.
     *  @param t the character data
     *  @throws Exception for handler errors */
    void text(String t) throws Exception;

    /** An unresolved entity reference in content.
     *  @param name the entity name
     *  @throws Exception for handler errors */
    void entityReference(String name) throws Exception;
}
