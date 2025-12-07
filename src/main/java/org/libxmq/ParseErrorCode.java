/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq;

/** These are the potential parse errors that can be returned from libxmq.
 */
public enum ParseErrorCode
{
    /** No error. */
    XMQ_ERROR_NONE(0),
    /** Cannot read file. */
    XMQ_ERROR_CANNOT_READ_FILE(1),
    /** Out of memory. */
    XMQ_ERROR_OOM(2),
    /** Not XMQ. */
    XMQ_ERROR_NOT_XMQ(3),
    /** Quote not closed. */
    XMQ_ERROR_QUOTE_NOT_CLOSED(4),
    /** Entity not closed. */
    XMQ_ERROR_ENTITY_NOT_CLOSED(5),
    /** Comment not closed. */
    XMQ_ERROR_COMMENT_NOT_CLOSED(6),
    /** Comment closed with too many slashes. */
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES(7),
    /** Body not closed. */
    XMQ_ERROR_BODY_NOT_CLOSED(8),
    /** Attributes not closed. */
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED(9),
    /** Compound not closed. */
    XMQ_ERROR_COMPOUND_NOT_CLOSED(10),
    /** Compound may not contain. */
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN(11),
    /** Quote closed with too many quotes. */
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES(12),
    /** Unexpected closing brace. */
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE(13),
    /** Expected content after equals. */
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS(14),
    /** Unexpected tab. */
    XMQ_ERROR_UNEXPECTED_TAB(15),
    /** Invalid char. */
    XMQ_ERROR_INVALID_CHAR(16),
    /** Bad doctype. */
    XMQ_ERROR_BAD_DOCTYPE(17),
    /** JSON invalid escape. */
    XMQ_ERROR_JSON_INVALID_ESCAPE(18),
    /** JSON invalid char. */
    XMQ_ERROR_JSON_INVALID_CHAR(19),
    /** Cannot handle XML. */
    XMQ_ERROR_CANNOT_HANDLE_XML(20),
    /** Cannot handle HTML. */
    XMQ_ERROR_CANNOT_HANDLE_HTML(21),
    /** Cannot handle JSON. */
    XMQ_ERROR_CANNOT_HANDLE_JSON(22),
    /** Expected XMQ. */
    XMQ_ERROR_EXPECTED_XMQ(23),
    /** Expected HTMQ. */
    XMQ_ERROR_EXPECTED_HTMQ(24),
    /** Expected XML. */
    XMQ_ERROR_EXPECTED_XML(25),
    /** Expected HTML. */
    XMQ_ERROR_EXPECTED_HTML(26),
    /** Expected JSON. */
    XMQ_ERROR_EXPECTED_JSON(27),
    /** Error parsing XML. */
    XMQ_ERROR_PARSING_XML(28),
    /** Error parsing HTML. */
    XMQ_ERROR_PARSING_HTML(29),
    /** Value cannot start with. */
    XMQ_ERROR_VALUE_CANNOT_START_WITH(30),
    /** IXML expected rule. */
    XMQ_ERROR_IXML_EXPECTED_RULE(50),
    /** IXML expected name. */
    XMQ_ERROR_IXML_EXPECTED_NAME(51),
    /** IXML expected equal or colon. */
    XMQ_ERROR_IXML_EXPECTED_EQUAL_OR_COLON(52),
    /** IXML expected dot. */
    XMQ_ERROR_IXML_EXPECTED_DOT(53),
    /** IXML unexpected end. */
    XMQ_ERROR_IXML_UNEXPECTED_END(54),
    /** IXML syntax error. */
    XMQ_ERROR_IXML_SYNTAX_ERROR(55),
    /** Warning quotes needed. */
    XMQ_WARNING_QUOTES_NEEDED(100);

    ParseErrorCode(int v) { value = v; }
    /** Return error code. */
    public int value;
}
