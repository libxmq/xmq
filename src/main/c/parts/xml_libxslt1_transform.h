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

/* Portions of this file are ported from libxslt (version 1.1.45 as found in the
   libxmq fork in 3rdparty/libxslt). libxslt is covered by the following notice
   (from libxslt/Copyright):

 Copyright (C) 2001-2002 Daniel Veillard.  All Rights Reserved.

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
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.
*/

#ifndef XML_LIBXSLT1_TRANSFORM_H
#define XML_LIBXSLT1_TRANSFORM_H

/*
    XSLT 1.0 transformations, a port of libxslt transform.c, variables.c,
    templates.c, keys.c, functions.c, numbers.c and documents.c onto the
    native dom and xpath. The stylesheet is compiled with xml_libxslt1_xslt.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"xml_libxslt1_xslt.h"
#endif

/** The output method of a result (the same numbers as libxslt xsltOutputType). */
#define XML_LIBXSLT1_OUTPUT_XML 0
#define XML_LIBXSLT1_OUTPUT_HTML 1
#define XML_LIBXSLT1_OUTPUT_TEXT 2

/**
    Apply the stylesheet to the document (libxslt xsltApplyStylesheet). params are
    name, value pairs (the values are xpath expressions) ending with NULL, or NULL.
    The document is not owned but whitespace may be stripped from it (xsl:strip-space).
    Returns the result, NULL after reporting errors. *output_type (if not NULL) gets
    XML_LIBXSLT1_OUTPUT_XML, _HTML or _TEXT.
*/
XmlDom *xml_libxslt1_apply_stylesheet(XmlLibxslt1Stylesheet *style, XmlDom *doc, const char **params, int *output_type);

#define XML_LIBXSLT1_TRANSFORM_MODULE

#endif // XML_LIBXSLT1_TRANSFORM_H
