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

#ifndef XML_XPATH_H
#define XML_XPATH_H

/*
    XPath 1.0 on the native dom (xml_dom.h), with the same results as libxml2
    xmlXPathEval (the libxmq fork in 3rdparty/libxml2): the same core function
    library, number formatting, node set ordering and the same streaming
    evaluation of simple paths.

    Entity references are never expanded (see xml_parser.h), so unlike libxml2
    the replacement text of an entity is not visible to xpath.

    There is no global state: namespaces and options are given per call.
*/

#include"xml_dom.h"

#include<stdbool.h>
#include<stddef.h>

typedef enum
{
    XML_XPATH_NODESET = 1,
    XML_XPATH_BOOLEAN = 2,
    XML_XPATH_NUMBER = 3,
    XML_XPATH_STRING = 4,
} XmlXPathType;

/** Error codes, the same numbers as libxml2 xmlXPathError. */
typedef enum
{
    XML_XPATH_ERR_OK = 0,
    XML_XPATH_ERR_NUMBER = 1,
    XML_XPATH_ERR_UNFINISHED_LITERAL = 2,
    XML_XPATH_ERR_START_LITERAL = 3,
    XML_XPATH_ERR_VARIABLE_REF = 4,
    XML_XPATH_ERR_UNDEF_VARIABLE = 5,
    XML_XPATH_ERR_INVALID_PREDICATE = 6,
    XML_XPATH_ERR_EXPR = 7,
    XML_XPATH_ERR_UNCLOSED = 8,
    XML_XPATH_ERR_UNKNOWN_FUNC = 9,
    XML_XPATH_ERR_INVALID_OPERAND = 10,
    XML_XPATH_ERR_INVALID_TYPE = 11,
    XML_XPATH_ERR_INVALID_ARITY = 12,
    XML_XPATH_ERR_INVALID_CTXT_SIZE = 13,
    XML_XPATH_ERR_INVALID_CTXT_POSITION = 14,
    XML_XPATH_ERR_MEMORY = 15,
    XML_XPATH_ERR_UNDEF_PREFIX = 19,
    XML_XPATH_ERR_RECURSION_LIMIT_EXCEEDED = 26,
} XmlXPathErrorCode;

typedef struct
{
    const char *prefix;
    const char *uri;
} XmlXPathNamespace;

typedef struct
{
    /** Prefixes that may be used in expressions (the xml prefix is always known). */
    const XmlXPathNamespace *namespaces;
    size_t num_namespaces;
    /** id() also finds id attributes (and name on a), like libxml2 for html documents. */
    bool html_ids;
    /** Maximum nesting of the expression (libxml2 XPATH_MAX_RECURSION_DEPTH). */
    size_t max_depth;
} XmlXPathSettings;

typedef struct
{
    XmlXPathErrorCode code;
    /** Static libxml2 error message, eg "Invalid expression". */
    const char *message;
    /** Byte offset in the expression where a compile error was found. */
    size_t offset;
} XmlXPathError;

struct XmlXPathResult;
typedef struct XmlXPathResult XmlXPathResult;

void xml_xpath_settings_default(XmlXPathSettings *settings);

/** Evaluate expr with context as the context node. Returns NULL on error, then error (if not NULL)
    is filled in. settings may be NULL for the defaults. */
XmlXPathResult *xml_xpath_eval(const char *expr, XMQNode *context, const XmlXPathSettings *settings, XmlXPathError *error);

XmlXPathType xml_xpath_result_type(const XmlXPathResult *r);
/** The number of nodes for a node set, 0 for other results. The nodes are in document order. */
size_t xml_xpath_result_size(const XmlXPathResult *r);
XMQNode *xml_xpath_result_node(const XmlXPathResult *r, size_t i);
bool xml_xpath_result_boolean(const XmlXPathResult *r);
double xml_xpath_result_number(const XmlXPathResult *r);
/** The string result (for a string result), NULL otherwise. */
const char *xml_xpath_result_string(const XmlXPathResult *r);
/** Frees the result, including any namespace nodes it contains. */
void xml_xpath_result_free(XmlXPathResult *r);

/** Format a number like xpath string(), eg 1, 0.5, NaN, Infinity. Malloced. */
char *xml_xpath_format_number(double d);
/** Convert a string to a number like xpath number(). */
double xml_xpath_string_to_number(const char *s);

#define XML_XPATH_MODULE

#endif // XML_XPATH_H
