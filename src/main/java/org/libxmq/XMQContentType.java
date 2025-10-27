/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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

/** Specify the file/buffer content type.

    XMQContentType:
    @XMQ_CONTENT_UNKNOWN: a failed content detect will mark the content type as unknown
    @XMQ_CONTENT_DETECT: auto detect the content type
    @XMQ_CONTENT_XMQ: xmq detected
    @XMQ_CONTENT_HTMQ: htmq detected
    @XMQ_CONTENT_XML: xml detected
    @XMQ_CONTENT_HTML: html detected
    @XMQ_CONTENT_JSON: json detected
    @XMQ_CONTENT_IXML: ixml selected
    @XMQ_CONTENT_TEXT: valid utf8 text input/output is selected
    @XMQ_CONTENT_CLINES: xpath="c-escaped string"

    Specify the file/buffer content type.
*/
public enum XMQContentType
{
    XMQ_CONTENT_UNKNOWN,
    XMQ_CONTENT_DETECT,
    XMQ_CONTENT_XMQ,
    XMQ_CONTENT_HTMQ,
    XMQ_CONTENT_XML,
    XMQ_CONTENT_HTML,
    XMQ_CONTENT_JSON,
    XMQ_CONTENT_TEXT,
    XMQ_CONTENT_CLINES;
}
