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

#ifndef XML_SAVE_H
#define XML_SAVE_H

/*
    XML serializer for the native dom (xml_dom.h).

    The output is byte compatible with libxml2's xmlsave.c (the libxmq fork in
    3rdparty/libxml2): xml_save_doc corresponds to xmlDocDumpFormatMemoryEnc
    and xml_save_node to xmlNodeDump. Not yet supported: the XHTML
    serialization mode that libxml2 picks when the doctype is an XHTML DTD.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"membuffer.h"
#endif

#include<stdbool.h>
#include<stddef.h>

/** Serialize the document. encoding NULL means use the dom's encoding; if that is NULL too,
    no encoding is declared and all non-ascii characters are written as character references. */
void xml_save_doc_to(MemBuffer *mb, XmlDom *dom, const char *encoding, bool format);
/** Same as xml_save_doc_to but returns a malloced zero terminated string, length in *out_len (may be NULL). */
char *xml_save_doc(XmlDom *dom, const char *encoding, bool format, size_t *out_len);

/** Serialize a node and its subtree (UTF-8, no xml declaration). */
void xml_save_node_to(MemBuffer *mb, const XMQNode *node, int level, bool format);
char *xml_save_node(const XMQNode *node, int level, bool format, size_t *out_len);

/** Append s escaped for an xml attribute value (xmlSerializeText with XML_ESCAPE_ATTR),
    non ascii characters are written as character references if escape_non_ascii. */
void xml_save_escape_attr(MemBuffer *mb, const char *s, bool escape_non_ascii);

/** Append s quoted with " (or ' if s contains " but not '), like xmlOutputBufferWriteQuotedString. */
void xml_save_write_quoted(MemBuffer *mb, const char *s);

#define XML_SAVE_MODULE

#endif // XML_SAVE_H
