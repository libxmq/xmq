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

#ifndef XML_HTML_SAVE_H
#define XML_HTML_SAVE_H

/*
    Html serializer for the native dom (xml_dom.h). Any dom can be written as
    html, whether it was read by the html parser, the xml parser or built by
    xmq: there is only one dom.

    The output is byte compatible with libxml2 htmlDocContentDumpFormatOutput
    and htmlNodeDumpFormatOutput (the libxmq fork in 3rdparty/libxml2), as
    called by libxmq: utf8 output, no meta charset handling.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"membuffer.h"
#endif

#include<stdbool.h>
#include<stddef.h>

/** Serialize the document as html (doctype first, then the content). */
void xml_html_save_doc_to(MemBuffer *mb, XmlDom *dom, bool format);
char *xml_html_save_doc(XmlDom *dom, bool format, size_t *out_len);

/** Serialize a node and its subtree as html. */
void xml_html_save_node_to(MemBuffer *mb, const XMQNode *node, bool format);
char *xml_html_save_node(const XMQNode *node, bool format, size_t *out_len);

#define XML_HTML_SAVE_MODULE

#endif // XML_HTML_SAVE_H
