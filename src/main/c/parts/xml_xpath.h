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
    the replacement text of an entity is not visible to xpath: entity reference
    nodes are not selected by any node test and contribute nothing to string values.

    There is no global state: namespaces and options are given per call.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#endif

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
    XML_XPATH_ERR_FORBID_VARIABLE = 24,
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
    /** The context node given to xml_xpath_eval only selects the document, there is no context node
        (like a fresh libxml2 xmlXPathNewContext): relative paths select nothing. */
    bool no_context_node;
    /** Variable references are compile errors (libxml2 XML_XPATH_NOVAR, xslt patterns). */
    bool no_variables;
} XmlXPathSettings;

typedef struct
{
    XmlXPathErrorCode code;
    /** Static libxml2 error message, eg "Invalid expression". */
    const char *message;
    /** Byte offset in the expression where a compile error was found. */
    size_t offset;
    /** The error happened while evaluating a compiled expression (libxml2 has no expression
        context for such errors). */
    bool evaluating;
    /** An extension function failed without a message (like setting ctxt->error in libxml2),
        nothing should be reported. */
    bool quiet;
    /** The message with details (libxml2 xmlXPathErrFmt), eg "Unregistered function: f", or "". */
    char detail[256];
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

// Values created by the caller, eg variable values and extension function results.
XmlXPathResult *xml_xpath_result_new_string(const char *s);
XmlXPathResult *xml_xpath_result_new_number(double d);
XmlXPathResult *xml_xpath_result_new_boolean(bool b);
/** An empty node set, add nodes with xml_xpath_result_add_node. */
XmlXPathResult *xml_xpath_result_new_nodeset(void);
/** Append a node to a node set (no check for duplicates or order, see xml_xpath_context_sort). */
void xml_xpath_result_add_node(XmlXPathResult *r, XMQNode *n);
/** Mark a node set as a result tree fragment (libxml2 XPATH_XSLT_TREE, the value of an xslt
    variable with content): it behaves like a node set except that path steps, predicates
    and unions on it are type errors. Copies keep the mark. */
void xml_xpath_result_set_tree(XmlXPathResult *r, bool tree);
bool xml_xpath_result_is_tree(const XmlXPathResult *r);
/** A copy, namespace nodes owned by r are copied too. */
XmlXPathResult *xml_xpath_result_copy(const XmlXPathResult *r);
/** The XPath string(), number() and boolean() of a value. The string is malloced. */
char *xml_xpath_result_to_string(const XmlXPathResult *r);
double xml_xpath_result_to_number(const XmlXPathResult *r);
bool xml_xpath_result_to_boolean(const XmlXPathResult *r);

// Compiled expressions and evaluation contexts, used by xslt //////////////

/** A compiled expression, evaluated any number of times with xml_xpath_eval_comp. */
struct XmlXPathComp;
typedef struct XmlXPathComp XmlXPathComp;

/**
    An evaluation context: the document order index and the id tables of the trees
    seen so far are kept between evaluations, and variables and extension functions
    are resolved through callbacks. The trees must not change while the context
    knows them: call xml_xpath_context_forget before a tree is changed or freed.
*/
struct XmlXPathContext;
typedef struct XmlXPathContext XmlXPathContext;

/** Return the value of the variable (borrowed, it must live until the evaluation
    ends), or NULL if it is not defined. ns_uri is NULL for an unprefixed name. */
typedef const XmlXPathResult *(*XmlXPathVariableLookup)(void *user_data, const char *name, const char *ns_uri);

/** An extension function. Returns a new value, or NULL after calling
    xml_xpath_context_set_error. The arguments are owned by the caller. */
typedef XmlXPathResult *(*XmlXPathFunction)(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs);

/** Find an extension function (called for names that are not XPath core functions),
    returns NULL if there is none. *func_data is passed to the function. */
typedef XmlXPathFunction (*XmlXPathFunctionLookup)(void *user_data, const char *name, const char *ns_uri, void **func_data);

XmlXPathComp *xml_xpath_compile(const char *expr, const XmlXPathSettings *settings, XmlXPathError *error);
void xml_xpath_comp_free(XmlXPathComp *comp);
/** The expression as given to xml_xpath_compile. */
const char *xml_xpath_comp_source(const XmlXPathComp *comp);

XmlXPathContext *xml_xpath_context_new(void);
void xml_xpath_context_free(XmlXPathContext *ctx);
void xml_xpath_context_set_variable_lookup(XmlXPathContext *ctx, XmlXPathVariableLookup lookup, void *user_data);
void xml_xpath_context_set_function_lookup(XmlXPathContext *ctx, XmlXPathFunctionLookup lookup, void *user_data);
/** Drop what the context knows about the tree with this root, before it is changed or freed. */
void xml_xpath_context_forget(XmlXPathContext *ctx, XMQNode *root);

/**
    Evaluate a compiled expression with node as context node, position and size
    as the context position and size (-1 if unknown, then position() and last()
    are errors). Returns NULL on an error, then error (if not NULL) is filled in.
*/
XmlXPathResult *xml_xpath_eval_comp(XmlXPathContext *ctx, const XmlXPathComp *comp, XMQNode *node,
                                    long position, long size, XmlXPathError *error);

/** Sort a node set into document order and remove duplicates, like the result of a union. */
void xml_xpath_context_sort(XmlXPathContext *ctx, XmlXPathResult *r);

// For extension functions, during an evaluation.
XMQNode *xml_xpath_context_node(const XmlXPathContext *ctx);
long xml_xpath_context_position(const XmlXPathContext *ctx);
long xml_xpath_context_size(const XmlXPathContext *ctx);
/** The namespace uri of a prefix in the namespaces of the expression being evaluated, or NULL. */
const char *xml_xpath_context_lookup_namespace(const XmlXPathContext *ctx, const char *prefix);
/** The element with this id (like id() and libxml2 xmlGetID) in the document of node, or NULL. */
XMQNode *xml_xpath_context_get_id(XmlXPathContext *ctx, XMQNode *node, const char *id);
/** Make the current extension function fail with this error. */
void xml_xpath_context_set_error(XmlXPathContext *ctx, XmlXPathErrorCode code);
/** Make the current extension function fail with this error without a message (libxml2 functions
    that set ctxt->error directly). */
void xml_xpath_context_set_error_quiet(XmlXPathContext *ctx, XmlXPathErrorCode code);

/** Format a number like xpath string(), eg 1, 0.5, NaN, Infinity. Malloced. */
char *xml_xpath_format_number(double d);
/** Convert a string to a number like xpath number(). */
double xml_xpath_string_to_number(const char *s);

#define XML_XPATH_MODULE

#endif // XML_XPATH_H
