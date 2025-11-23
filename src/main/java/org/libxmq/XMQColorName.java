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

package org.libxmq;

/**
    XMQSyntax:
    SYNTAX_C: Comments
    SYNTAX_Q: Standalone quote.
    SYNTAX_E: Entity
    SYNTAX_ENS: Element Namespace
    SYNTAX_EN: Element Name
    SYNTAX_EK: Element Key
    SYNTAX_EKV: Element Key Value
    SYNTAX_ANS: Attribute NameSpace
    SYNTAX_AK: Attribute Key
    SYNTAX_AKV: Attribute Key Value
    SYNTAX_CP: Compound Parentheses
    SYNTAX_NDC: Namespace declaration
    SYNTAX_UW: Unicode Whitespace
*/
public enum XMQColorName {
    /** Comment */
    XMQ_COLOR_C,
    /** Quote */
    XMQ_COLOR_Q,
    /** Entity */
    XMQ_COLOR_E,
    /** Name Space (both for element and attribute) */
    XMQ_COLOR_NS,
    /** Element Name */
    XMQ_COLOR_EN,
    /** Element Key */
    XMQ_COLOR_EK,
    /** Element Key Value */
    XMQ_COLOR_EKV,
    /** Attribute Key */
    XMQ_COLOR_AK,
    /** Attribute Key Value */
    XMQ_COLOR_AKV,
    /** Compound Parentheses */
    XMQ_COLOR_CP,
    /** Name Space Declaration xmlns */
    XMQ_COLOR_NSD,
    /** Unicode whitespace */
    XMQ_COLOR_UW,
    /** Override XLS element names with this color. */
    XMQ_COLOR_XLS;
}
