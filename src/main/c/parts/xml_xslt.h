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

#ifndef XML_XSLT_H
#define XML_XSLT_H

/*
    XSLT 1.0, a port of libxslt onto the native dom (xml_dom) and xpath (xml_xpath).

    Work in progress (step 7 of the native xml plan): so far the patterns
    (pattern.c: compilation, matching, template registration and lookup), on an
    empty stylesheet and a minimal transformation context for testing.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"xml_xpath.h"
#endif

#include<stdbool.h>
#include<stddef.h>

/** Receives the error messages, formatted like libxslt prints them. */
typedef void (*XmlXsltMessageCallback)(void *user_data, const char *message);

typedef struct
{
    /** NULL prints the messages on stderr, like libxslt. */
    XmlXsltMessageCallback message_callback;
    void *message_user_data;
} XmlXsltSettings;

void xml_xslt_settings_default(XmlXsltSettings *settings);

/** A compiled stylesheet. */
struct XmlXsltStylesheet;
typedef struct XmlXsltStylesheet XmlXsltStylesheet;

/** A transformation of a source document with a stylesheet. */
struct XmlXsltTransform;
typedef struct XmlXsltTransform XmlXsltTransform;

/** A compiled pattern (libxslt xsltCompMatch), a list of alternatives. */
struct XmlXsltPattern;
typedef struct XmlXsltPattern XmlXsltPattern;

/** An empty stylesheet whose document is style_dom (not owned), patterns can be
    compiled with namespaces from its elements. */
XmlXsltStylesheet *xml_xslt_stylesheet_new_empty(XmlDom *style_dom, const XmlXsltSettings *settings);
void xml_xslt_stylesheet_free(XmlXsltStylesheet *style);

/** Compile a pattern, element (in the stylesheet document) gives the namespaces in
    scope. With no_variables, variable references in predicates are errors (template
    match patterns). Returns NULL after reporting an error. */
XmlXsltPattern *xml_xslt_pattern_compile(XmlXsltStylesheet *style, const char *pattern, XMQNode *element, bool no_variables);
void xml_xslt_pattern_free(XmlXsltPattern *pattern);
/** The default priority of the first alternative. */
double xml_xslt_pattern_priority(const XmlXsltPattern *pattern);

/** A transformation context for the source document (not owned). Compile the
    patterns before creating it, like libxslt allocates their caches. */
XmlXsltTransform *xml_xslt_transform_new(XmlXsltStylesheet *style, XmlDom *source);
void xml_xslt_transform_free(XmlXsltTransform *t);

/** Does the node match any alternative of the pattern (libxslt xsltTestCompMatchList)?
    Returns 1 or 0, -1 on an error. */
int xml_xslt_pattern_match(XmlXsltTransform *t, XMQNode *node, XmlXsltPattern *pattern);

#define XML_XSLT_MODULE

#endif // XML_XSLT_H
