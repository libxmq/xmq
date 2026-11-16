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

#ifndef XML_HTML_PARSER_H
#define XML_HTML_PARSER_H

/*
    Html parser building the native dom (xml_dom.h), the same dom as the xml
    parser builds: there is no html specific node or document type.

    It builds the same tree as libxml2 htmlReadMemory (the libxmq fork in
    3rdparty/libxml2) does with the options libxmq uses (HTML_PARSE_NOERROR |
    HTML_PARSE_NOWARNING | HTML_PARSE_NONET, optionally HTML_PARSE_NOBLANKS,
    encoding UTF-8): html5 tokenization, libxml2's tree construction with
    implied html/head/body elements and implicit closing of elements, a default
    html 4.0 transitional doctype if none is given.

    Element names are lowercased, there are no namespaces. Character references
    and html named character references (like &nbsp;) are decoded: in html they
    are character references, not declared entities. Raw text (script, style...)
    is stored as cdata nodes, like libxml2.

    Html parsing never fails, errors are reported through the callback in the
    settings and the parser recovers. There is no global state.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"xml_parser.h"
#endif

#include<stdbool.h>
#include<stddef.h>

typedef struct
{
    /** Name used in messages, eg the file name. NULL gives "noname.html". */
    const char *source_name;
    /** Drop ignorable blank text nodes (libxml2 HTML_PARSE_NOBLANKS heuristic). */
    bool no_blanks;
    /** Maximum length in bytes of an attribute value, comment or doctype literal (10000000). */
    size_t max_text_length;
    /** Receives recoverable errors, may be NULL. */
    XmlParseMessageCallback message_callback;
    void *message_user_data;
} XmlHtmlParseSettings;

void xml_html_parse_settings_default(XmlHtmlParseSettings *settings);

/** Parse html (UTF-8; like libxml2 a bom is skipped if the input is at least four bytes, a lone
    bom is the character U+FEFF). Always returns a dom (it may lack a root element). */
XmlDom *xml_html_parse_buffer(const char *start, const char *stop, const XmlHtmlParseSettings *settings);

#ifndef XMQ_NO_HTML
#define XML_HTML_PARSER_MODULE
#endif

#endif // XML_HTML_PARSER_H
