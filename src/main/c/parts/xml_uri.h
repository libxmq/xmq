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

/* Portions of this file are ported from libxml2 (uri.c, version 2.16.0 as found
   in the libxmq fork in 3rdparty/libxml2).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef XML_URI_H
#define XML_URI_H

/* URI references (RFC 3986), parsing, saving and resolving against a base,
   a port of libxml2 uri.c. Used to resolve xml:base, xsl:import, xsl:include
   and document() the way libxml2 xmlBuildURI does. */

#include<stddef.h>

/** A parsed URI reference (libxml2 xmlURI). */
typedef struct XmlURI
{
    char *scheme;
    char *opaque;
    char *authority;
    char *server;
    char *user;
    int port;
    char *path;
    char *query;
    char *fragment;
    int cleanup;
    char *query_raw;
} XmlURI;

XmlURI *xml_uri_create(void);
/** Parse a URI reference, returns 0 or an error code (-1 for out of memory). */
int xml_uri_parse_safe(const char *str, XmlURI **uri_out);
XmlURI *xml_uri_parse(const char *str);
int xml_uri_parse_reference(XmlURI *uri, const char *str);
XmlURI *xml_uri_parse_raw(const char *str, int raw);
/** The uri as an escaped string, malloced. */
char *xml_uri_save(XmlURI *uri);
void xml_uri_free(XmlURI *uri);
int xml_uri_normalize_uri_path(char *path);
/** Unescape %XX sequences, len -1 for the whole string, target NULL to malloc. */
char *xml_uri_unescape_string(const char *str, int len, char *target);
/** Resolve uri against base (libxml2 xmlBuildURISafe): returns 0 and sets *out, or an error code
    (the uri or base are invalid) or -1 for out of memory. */
int xml_uri_build_safe(const char *uri, const char *base, char **out);
/** Escape str to hex except unreserved characters, "@" and those in list (libxml2 xmlURIEscapeStr). */
char *xml_uri_escape_str(const char *str, const char *list);
/** Like xml_uri_build_safe, NULL on errors (libxml2 xmlBuildURI). */
char *xml_uri_build(const char *uri, const char *base);

#define XML_URI_MODULE

#endif // XML_URI_H
