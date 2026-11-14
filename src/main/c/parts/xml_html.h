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

#ifndef XML_HTML_H
#define XML_HTML_H

/*
    Html knowledge shared by the html parser (xml_html_parser) and the html
    serializer (xml_html_save): element properties, implicit closing rules and
    the html5 named character references. The tables in xml_html.c are
    generated from libxml2 by scripts/generate_xml_html_tables.py.

    There is no html specific dom: the html parser builds the same XmlDom as the
    xml parser, these tables are only used while reading and writing html.
*/

#include<stdbool.h>
#include<stddef.h>

/** Raw text modes of elements, like libxml2 private/html.h. */
#define XML_HTML_DATA_RCDATA 1
#define XML_HTML_DATA_RAWTEXT 2
#define XML_HTML_DATA_PLAINTEXT 3
#define XML_HTML_DATA_SCRIPT 4
#define XML_HTML_DATA_SCRIPT_ESC1 5
#define XML_HTML_DATA_SCRIPT_ESC2 6

typedef struct
{
    const char *name;
    char end_tag;   // 1 the end tag can be implied, 2 forbidden (empty), 3 stylistic, closed easily.
    char empty;     // An empty element (br, img, ...).
    char isinline;  // An inline (1) or block (0) element.
    char data_mode; // 0 or one of XML_HTML_DATA_*.
} XmlHtmlElementDesc;

/** Look up an html element (case insensitive), NULL if unknown. */
const XmlHtmlElementDesc *xml_html_element(const char *name);
/** Does a start tag new_tag implicitly close an open element old_tag? (lowercase names) */
bool xml_html_start_closes(const char *new_tag, const char *old_tag);
/** The end tag priority: an end tag only closes elements with lower or equal priority. */
int xml_html_end_priority(const char *name);
/** Elements where ignorable whitespace is kept since they allow pcdata. */
bool xml_html_allows_pcdata(const char *name);
/** Attributes written in minimized form, eg checked or selected (case insensitive). */
bool xml_html_is_boolean_attr(const char *name);
/**
    Find the longest html5 named character reference that is a prefix of string (which
    starts after the &). Returns the utf8 replacement (length in *rlen) and the number
    of bytes of the reference (excluding the &, including a ; if present) in *nlen,
    or NULL if there is none.
*/
const unsigned char *xml_html_find_entity_prefix(const unsigned char *string, size_t slen, bool is_attr,
                                                 size_t *nlen, size_t *rlen);

#define XML_HTML_MODULE

#endif // XML_HTML_H
