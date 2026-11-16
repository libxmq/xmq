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

/* Portions of this file are ported from libxml2 (pattern.c, version 2.16.0 as found
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

#ifndef XML_PATTERN_H
#define XML_PATTERN_H

/* Streamable patterns, a port of libxml2 pattern.c: the XPath subset used for
   xs:selector and xs:field of XML Schema identity constraints (and plain
   patterns like "a/b", "//c", "/a/@b", alternatives with |).

   A compiled pattern can be matched against a node of the native dom
   (xml_pattern_match), or, when streamable, a stream context can be created
   that is fed with xml_stream_push/xml_stream_push_attr/xml_stream_pop while
   walking a tree in document order.

   Namespaces are given like libxml2 does: an array of href, prefix pairs
   terminated by a NULL href (or NULL for no namespaces). The prefix xml is
   always bound to the xml namespace.

   Node types for xml_stream_push_node are XmlNodeKind values (they are the
   same as the libxml2 xmlElementType values): XML_DOM_ELEMENT, XML_DOM_ATTRIBUTE,
   XML_DOM_TEXT, XML_DOM_CDATA, XML_DOM_COMMENT, XML_DOM_PI.

   Compilation errors are not printed: libxml2 2.16 compiles its error
   messages away in pattern.c (ERROR macros are empty), it only returns NULL.
   xml_pattern_compile_safe returns the error code. */

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#endif

/** Compilation flags, the values of libxml2 xmlPatternFlags (named differently, libxml2 is still linked). */
#define XML_PATTERN_FLAG_DEFAULT 0      /* simple pattern match */
#define XML_PATTERN_FLAG_XPATH   (1<<0) /* standard XPath pattern */
#define XML_PATTERN_FLAG_XSSEL   (1<<1) /* XPath subset for schema selector */
#define XML_PATTERN_FLAG_XSFIELD (1<<2) /* XPath subset for schema field */

struct XmlPattern;
typedef struct XmlPattern XmlPattern;
struct XmlStreamCtxt;
typedef struct XmlStreamCtxt XmlStreamCtxt;

/** Compile a pattern (libxml2 xmlPatternCompileSafe). Returns 0 on success, 1 on error,
    -1 if a memory allocation failed. *pattern_out is NULL on errors and for an empty pattern. */
int xml_pattern_compile_safe(const char *pattern, int flags, const char **namespaces, XmlPattern **pattern_out);
/** Compile a pattern, NULL on errors (libxml2 xmlPatterncompile). */
XmlPattern *xml_pattern_compile(const char *pattern, int flags, const char **namespaces);
/** Free a compiled pattern and all its | alternatives (libxml2 xmlFreePattern). */
void xml_pattern_free(XmlPattern *comp);
/** Same as xml_pattern_free (libxml2 xmlFreePatternList). */
void xml_pattern_free_list(XmlPattern *comp);
/** 1 if the node matches the pattern, 0 if not and -1 on failure (libxml2 xmlPatternMatch). */
int xml_pattern_match(XmlPattern *comp, XMQNode *node);
/** 1 if streamable, 0 if not and -1 on error (libxml2 xmlPatternStreamable). */
int xml_pattern_streamable(XmlPattern *comp);
/** -2 if no limit (using //), otherwise the depth, -1 on error (libxml2 xmlPatternMaxDepth). */
int xml_pattern_max_depth(XmlPattern *comp);
/** The minimum depth, 0 means / or . are part of the set, -1 on error (libxml2 xmlPatternMinDepth). */
int xml_pattern_min_depth(XmlPattern *comp);
/** 1 if the pattern must be looked at from the root, 0 if not, -1 on error (libxml2 xmlPatternFromRoot). */
int xml_pattern_from_root(XmlPattern *comp);
/** A streaming context for the pattern, NULL if not streamable (libxml2 xmlPatternGetStreamCtxt). */
XmlStreamCtxt *xml_pattern_get_stream_ctxt(XmlPattern *comp);
/** Free a stream context (libxml2 xmlFreeStreamCtxt). */
void xml_stream_ctxt_free(XmlStreamCtxt *stream);
/** Push an element, name and ns both NULL means the document node (a reset).
    Returns -1 on error, 1 if the current state is a match and 0 otherwise (libxml2 xmlStreamPush). */
int xml_stream_push(XmlStreamCtxt *stream, const char *name, const char *ns);
/** Push a node of the given type (libxml2 xmlStreamPushNode). */
int xml_stream_push_node(XmlStreamCtxt *stream, const char *name, const char *ns, int node_type);
/** Push an attribute (libxml2 xmlStreamPushAttr). */
int xml_stream_push_attr(XmlStreamCtxt *stream, const char *name, const char *ns);
/** Pop one level, -1 on error, 0 otherwise (libxml2 xmlStreamPop). */
int xml_stream_pop(XmlStreamCtxt *stream);
/** 1 if text, cdata, comment and pi nodes must be pushed as well, 0 if only elements and
    attributes are needed, -1 on error (libxml2 xmlStreamWantsAnyNode). */
int xml_stream_wants_any_node(XmlStreamCtxt *stream);

#ifndef XMQ_NO_XSD
#define XML_PATTERN_MODULE
#endif

#endif // XML_PATTERN_H
