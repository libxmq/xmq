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
 * The different token colors used when coloring (answering to the {@link
 * XMQTheme} pre/post strings) while printing XMQ. This mirrors the C
 * {@code XMQColor} enum: each color corresponds to one {@link XMQThemeStrings}
 * field of the {@link XMQTheme}.
 */
public enum XMQColor
{
    /** Plain whitespace. */
    COLOR_whitespace,
    /** Unicode whitespace other than space and newline, shown visibly. */
    COLOR_unicode_whitespace,
    /** The whitespace that indents a line. */
    COLOR_indentation_whitespace,
    /** The = between a key and its value. */
    COLOR_equals,
    /** The { that opens an element body. */
    COLOR_brace_left,
    /** The } that closes an element body. */
    COLOR_brace_right,
    /** The ( that opens the attributes. */
    COLOR_apar_left,
    /** The ) that closes the attributes. */
    COLOR_apar_right,
    /** The ( that opens a compound value. */
    COLOR_cpar_left,
    /** The ) that closes a compound value. */
    COLOR_cpar_right,
    /** A quoted text. */
    COLOR_quote,
    /** An entity, eg &amp;#10;. */
    COLOR_entity,
    /** A comment. */
    COLOR_comment,
    /** The continued part of a comment that is closed and then continued with an asterisk. */
    COLOR_comment_continuation,
    /** The colon between a namespace prefix and a name. */
    COLOR_ns_colon,
    /** The namespace prefix of an element. */
    COLOR_element_ns,
    /** The name of an element with children. */
    COLOR_element_name,
    /** The name of an element with a value. */
    COLOR_element_key,
    /** An unquoted element value. */
    COLOR_element_value_text,
    /** A quoted element value. */
    COLOR_element_value_quote,
    /** An entity as an element value. */
    COLOR_element_value_entity,
    /** A quote inside a compound element value. */
    COLOR_element_value_compound_quote,
    /** An entity inside a compound element value. */
    COLOR_element_value_compound_entity,
    /** The namespace prefix of an attribute. */
    COLOR_attr_ns,
    /** The name of an attribute. */
    COLOR_attr_key,
    /** An unquoted attribute value. */
    COLOR_attr_value_text,
    /** A quoted attribute value. */
    COLOR_attr_value_quote,
    /** An entity as an attribute value. */
    COLOR_attr_value_entity,
    /** A quote inside a compound attribute value. */
    COLOR_attr_value_compound_quote,
    /** An entity inside a compound attribute value. */
    COLOR_attr_value_compound_entity,
    /** A namespace declaration, xmlns or xmlns:prefix. */
    COLOR_ns_declaration,
    /** The xsl namespace prefix, which has its own color. */
    COLOR_ns_override_xsl
}
