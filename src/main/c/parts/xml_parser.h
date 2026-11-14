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

#ifndef XML_PARSER_H
#define XML_PARSER_H

/*
    XML 1.0 + Namespaces parser building the native dom (xml_dom.h).

    It builds the same tree as libxml2 xmlReadMemory does with the options libxmq
    uses (XML_PARSE_NOCDATA | XML_PARSE_NONET, optionally XML_PARSE_NOBLANKS):
    entity references are kept as nodes (also in attribute values), character
    references and the predefined entities are expanded, the internal subset is
    parsed into declaration nodes, external subsets/entities are never loaded.

    There is no global state: everything is configured through XmlParseSettings
    and messages are reported through the callback in the settings.

    Declared entities (&name; and %name;) are never expanded, they stay as reference
    nodes until an explicit xmqExpandEntities, which is where entity loops, nesting
    and amplification limits are enforced. Only character references and the five
    predefined entities (lt gt amp apos quot) are decoded while parsing. Entity
    references in namespace uris are rejected, since a uri is a plain string.

    The input is UTF-8, UTF-16 (bom or "<?" pattern) or ISO-8859-1 (declared),
    a UTF-8 BOM is skipped; the dom gets UTF-8.
*/

#include"xml_dom.h"

#include<stdbool.h>
#include<stddef.h>

/** Called for non-fatal problems (eg namespace errors) that libxml2 reports but recovers from.
    message is a complete libxml2 style message including the source context lines. */
typedef void (*XmlParseMessageCallback)(void *user_data, const char *message);

typedef struct
{
    /** Name used in messages, eg the file name. NULL gives "noname.xml". */
    const char *source_name;
    /** Drop ignorable blank text nodes (libxml2 XML_PARSE_NOBLANKS heuristic). */
    bool no_blanks;
    /** Merge cdata sections into text (XML_PARSE_NOCDATA). */
    bool no_cdata;
    /** Maximum element nesting depth. */
    size_t max_depth;
    /** Maximum length in bytes of a name. */
    size_t max_name_length;
    /** Maximum length in bytes of a single text node, attribute value, comment or pi. */
    size_t max_text_length;
    /** Maximum ratio between expanded entity values and the input size (and at least 1MB). */
    size_t max_amplification;
    /** Receives non-fatal messages, may be NULL. */
    XmlParseMessageCallback message_callback;
    void *message_user_data;
} XmlParseSettings;

/** Fill in the defaults: libxml2's default limits (depth 256, names 50000 bytes,
    text 10000000 bytes, amplification 5), no_blanks and no_cdata false. */
void xml_parse_settings_default(XmlParseSettings *settings);

typedef struct
{
    /** Malloced libxml2 style message with source context, NULL if no error. */
    char *message;
    /** The message without location and context, malloced, NULL if no error. */
    char *short_message;
    /** 1-based line and column (in bytes) of the error. */
    size_t line;
    size_t col;
} XmlParseError;

/** Parse a complete document. Returns NULL on a fatal error, then error (if not NULL)
    is filled in; free it with xml_parse_error_free. */
XmlDom *xml_parse_buffer(const char *start, const char *stop, const XmlParseSettings *settings, XmlParseError *error);

void xml_parse_error_free(XmlParseError *error);

#define XML_PARSER_MODULE

#endif // XML_PARSER_H
