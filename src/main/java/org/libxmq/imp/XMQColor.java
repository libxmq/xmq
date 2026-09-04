/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq.imp;

/**
    XMQColor:

    Map token type into color index.
*/
public enum XMQColor {
    /** No color. */
    COLOR_none,
    /** Whitespace. */
    COLOR_whitespace,
    /** Unicode whitespace. */
    COLOR_unicode_whitespace,
    /** Indentation whitespace. */
    COLOR_indentation_whitespace,
    /** Equals sign. */
    COLOR_equals,
    /** Left brace. */
    COLOR_brace_left,
    /** Right brace. */
    COLOR_brace_right,
    /** Left parenthesis. */
    COLOR_apar_left,
    /** Right parenthesis. */
    COLOR_apar_right,
    /** Left bracket. */
    COLOR_cpar_left,
    /** Right bracket. */
    COLOR_cpar_right,
    /** Quote. */
    COLOR_quote,
    /** Entity. */
    COLOR_entity,
    /** Comment. */
    COLOR_comment,
    /** Comment continuation. */
    COLOR_comment_continuation,
    /** Namespace colon. */
    COLOR_ns_colon,
    /** Element namespace. */
    COLOR_element_ns,
    /** Element name. */
    COLOR_element_name,
    /** Element key. */
    COLOR_element_key,
    /** Element value text. */
    COLOR_element_value_text,
    /** Element value quote. */
    COLOR_element_value_quote,
    /** Element value entity. */
    COLOR_element_value_entity,
    /** Element value compound quote. */
    COLOR_element_value_compound_quote,
    /** Element value compound entity. */
    COLOR_element_value_compound_entity,
    /** Attribute namespace. */
    COLOR_attr_ns,
    /** Attribute key. */
    COLOR_attr_key,
    /** Attribute value text. */
    COLOR_attr_value_text,
    /** Attribute value quote. */
    COLOR_attr_value_quote,
    /** Attribute value entity. */
    COLOR_attr_value_entity,
    /** Attribute value compound quote. */
    COLOR_attr_value_compound_quote,
    /** Attribute value compound entity. */
    COLOR_attr_value_compound_entity,
    /** Namespace declaration. */
    COLOR_ns_declaration,
    /** Namespace override xsl. */
    COLOR_ns_override_xsl,
}
