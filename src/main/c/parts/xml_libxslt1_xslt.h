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

#ifndef XML_LIBXSLT1_XSLT_H
#define XML_LIBXSLT1_XSLT_H

/*
    XSLT 1.0, a port of libxslt onto the native dom (xml_dom) and xpath (xml_xpath).

    This part has the patterns (pattern.c: compilation, matching, template
    registration and lookup) and the compilation of stylesheets (xslt.c,
    preproc.c, imports.c, attributes.c); the transformation is in
    xml_libxslt1_transform.
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"xml_xpath.h"
#include"hashmap.h"
#endif

#include<stdbool.h>
#include<stddef.h>

/** Receives the error messages, formatted like libxslt prints them. */
typedef void (*XmlLibxslt1MessageCallback)(void *user_data, const char *message);

/** What a document is loaded for (libxslt xsltLoadType). */
typedef enum
{
    XML_LIBXSLT1_LOAD_START = 0,      // The main stylesheet.
    XML_LIBXSLT1_LOAD_STYLESHEET = 1, // xsl:import and xsl:include.
    XML_LIBXSLT1_LOAD_DOCUMENT = 2    // document().
} XmlLibxslt1LoadType;

/** Load and parse the document at uri, NULL if it cannot be loaded. The dom gets uri as its url. */
typedef XmlDom *(*XmlLibxslt1Loader)(void *user_data, const char *uri, XmlLibxslt1LoadType type);

typedef struct
{
    /** NULL prints the messages on stderr, like libxslt. */
    XmlLibxslt1MessageCallback message_callback;
    void *message_user_data;
    /** NULL reads files and parses them with entities expanded, like libxslt XSLT_PARSE_OPTIONS. */
    XmlLibxslt1Loader loader;
    void *loader_user_data;
} XmlLibxslt1Settings;

/** The default loader: read the file (a file: url or a path) and parse it as xml. */
XmlDom *xml_libxslt1_default_loader(void *user_data, const char *uri, XmlLibxslt1LoadType type);

void xml_libxslt1_settings_default(XmlLibxslt1Settings *settings);

/** A compiled stylesheet. */
struct XmlLibxslt1Stylesheet;
typedef struct XmlLibxslt1Stylesheet XmlLibxslt1Stylesheet;

/** A transformation of a source document with a stylesheet. */
struct XmlLibxslt1Transform;
typedef struct XmlLibxslt1Transform XmlLibxslt1Transform;

/** A compiled pattern (libxslt xsltCompMatch), a list of alternatives. */
struct XmlLibxslt1Pattern;
typedef struct XmlLibxslt1Pattern XmlLibxslt1Pattern;

/**
    Compile a stylesheet (libxslt xsltParseStylesheetDoc). The stylesheet takes
    ownership of doc and modifies it (whitespace is stripped etc) when compilation
    succeeds. Returns NULL after reporting errors, then doc is not taken (but may
    have been modified).
*/
XmlLibxslt1Stylesheet *xml_libxslt1_compile(XmlDom *doc, const XmlLibxslt1Settings *settings);

/** A description of the compiled stylesheet (output settings, templates, keys, global
    variables, attribute sets, ...), for testing. Malloced. */
char *xml_libxslt1_stylesheet_describe(const XmlLibxslt1Stylesheet *style);

/** An empty stylesheet whose document is style_dom (not owned), patterns can be
    compiled with namespaces from its elements. */
XmlLibxslt1Stylesheet *xml_libxslt1_stylesheet_new_empty(XmlDom *style_dom, const XmlLibxslt1Settings *settings);
void xml_libxslt1_stylesheet_free(XmlLibxslt1Stylesheet *style);

/** Compile a pattern, element (in the stylesheet document) gives the namespaces in
    scope. With no_variables, variable references in predicates are errors (template
    match patterns). Returns NULL after reporting an error. */
XmlLibxslt1Pattern *xml_libxslt1_pattern_compile(XmlLibxslt1Stylesheet *style, const char *pattern, XMQNode *element, bool no_variables);
void xml_libxslt1_pattern_free(XmlLibxslt1Pattern *pattern);
/** The default priority of the first alternative. */
double xml_libxslt1_pattern_priority(const XmlLibxslt1Pattern *pattern);

/** A transformation context for the source document (not owned). Compile the
    patterns before creating it, like libxslt allocates their caches. */
XmlLibxslt1Transform *xml_libxslt1_transform_new(XmlLibxslt1Stylesheet *style, XmlDom *source);
void xml_libxslt1_transform_free(XmlLibxslt1Transform *t);

/** Does the node match any alternative of the pattern (libxslt xsltTestCompMatchList)?
    Returns 1 or 0, -1 on an error. */
int xml_libxslt1_pattern_match(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Pattern *pattern);

// Internals, only seen by the xml_libxslt1 parts (and in dist/xmq.c).
#if defined(XML_LIBXSLT1_INTERNALS) || defined(BUILDING_DIST_XMQ)

// Internals, shared by the xml_libxslt1 parts (the stylesheet and transformation
// structures of libxslt xsltInternals.h). Not an API.

// The priority of a template without a priority attribute (XSLT_PAT_NO_PRIORITY).
#define XSLT_PAT_NO_PRIORITY -12345789.0f

typedef enum
{
    XSLT_STATE_OK = 0,
    XSLT_STATE_ERROR,
    XSLT_STATE_STOPPED
} XsltState;

#define XSLT_NAMESPACE "http://www.w3.org/1999/XSL/Transform"
#define XSLT_LIBXSLT_NAMESPACE "http://xmlsoft.org/XSLT/namespace"
#define XSLT_SAXON_NAMESPACE "http://icl.com/saxon"
#define XSLT_XT_NAMESPACE "http://www.jclark.com/xt"
#define XSLT_XALAN_NAMESPACE "org.apache.xalan.xslt.extensions.Redirect"
#define XSLT_UNDEFINED_DEFAULT_NS "#UNDEFINED_DEFAULT_NS"
#define XSLT_MAX_NESTING 40

typedef struct XsltTemplate XsltTemplate;
typedef struct XsltStylePreComp XsltStylePreComp;

/** A template (libxslt xsltTemplate). */
struct XsltTemplate
{
    XsltTemplate *next;     // The next template in the stylesheet.
    XmlLibxslt1Stylesheet *style;
    char *match;
    float priority;
    char *name;
    char *name_uri;
    char *mode;
    char *mode_uri;
    XMQNode *content;       // The first node of the template body.
    XMQNode *elem;          // The xsl:template element (or the document of a literal result stylesheet).
    XmlNs **inherited_ns;   // The namespaces in scope of the template, for literal result elements.
    int inherited_ns_nr;
    int position;
};

/** Runtime caches of the patterns, indexed by the extras allocated at compile time. */
typedef struct
{
    void *info;
    void (*deallocate)(void *);
    union
    {
        void *ptr;
        int ival;
    } val;
} XsltRuntimeExtra;

/** The kind of a precompiled instruction (libxslt xsltStyleType). */
typedef enum
{
    XSLT_FUNC_COPY = 1,
    XSLT_FUNC_SORT,
    XSLT_FUNC_TEXT,
    XSLT_FUNC_ELEMENT,
    XSLT_FUNC_ATTRIBUTE,
    XSLT_FUNC_COMMENT,
    XSLT_FUNC_PI,
    XSLT_FUNC_COPYOF,
    XSLT_FUNC_VALUEOF,
    XSLT_FUNC_NUMBER,
    XSLT_FUNC_APPLYIMPORTS,
    XSLT_FUNC_CALLTEMPLATE,
    XSLT_FUNC_APPLYTEMPLATES,
    XSLT_FUNC_CHOOSE,
    XSLT_FUNC_IF,
    XSLT_FUNC_FOREACH,
    XSLT_FUNC_DOCUMENT,
    XSLT_FUNC_WITHPARAM,
    XSLT_FUNC_PARAM,
    XSLT_FUNC_VARIABLE,
    XSLT_FUNC_WHEN,
    XSLT_FUNC_EXTENSION
} XsltStyleType;

/** The built in extension elements (extra.c): libxslt:debug and the document elements. */
typedef enum
{
    XSLT_EXT_NONE = 0,
    XSLT_EXT_DEBUG,
    XSLT_EXT_DOCUMENT
} XsltExtElement;

/** xsl:number data (libxslt xsltNumberData). */
typedef struct
{
    const char *level;
    const char *count;
    const char *from;
    const char *value;
    const char *format;
    int has_format;
    int digits_per_group;
    int grouping_character;
    int grouping_character_len;
    XmlDom *doc;
    XMQNode *node;
    XmlLibxslt1Pattern *count_pat;
    XmlLibxslt1Pattern *from_pat;
} XsltNumberData;

/** A precompiled instruction (libxslt xsltStylePreComp), kept as the user data of its element.
    The strings are owned by the precomputed instruction. */
struct XsltStylePreComp
{
    XsltStylePreComp *next; // All of them, in the stylesheet.
    XsltStyleType type;
    XsltExtElement ext;     // For XSLT_FUNC_EXTENSION and XSLT_FUNC_DOCUMENT.
    XMQNode *inst;

    char *stype;
    int has_stype;
    int number;
    char *order;
    int has_order;
    int descending;
    char *lang;
    int has_lang;
    char *case_order;
    int lower_first;
    char *use;
    int has_use;
    int noescape;
    char *name;
    int has_name;
    char *ns;
    int has_ns;
    char *mode;
    char *mode_uri;
    char *test;
    char *select;
    int ver11;
    char *filename;
    int has_filename;
    XsltTemplate *templ;    // The called template (call-template), found at runtime.
    XsltNumberData numdata;
    XmlXPathComp *comp;
    XmlXPathNamespace *ns_list;
    size_t ns_nr;
    // Strings of numdata, owned.
    char *num_strings[5];
};

/** The marker for elements of an unknown extension (libxslt xsltExtMarker). */
extern char xslt1_ext_marker_;
#define XSLT_EXT_MARKER ((void*)&xslt1_ext_marker_)

/** An attribute value template (attrvt.c). Segments alternate strings and expressions. */
typedef struct XsltAttrVT XsltAttrVT;
struct XsltAttrVT
{
    XsltAttrVT *next;
    int nb_seg;
    int max_seg;
    int strstart;           // Does it start with a string?
    XmlXPathNamespace *ns_list;
    size_t ns_nr;
    void **segments;        // char* or XmlXPathComp*.
};

/** A key declaration (libxslt xsltKeyDef). */
typedef struct XsltKeyDef XsltKeyDef;
struct XsltKeyDef
{
    XsltKeyDef *next;
    XMQNode *inst;
    char *name;
    char *name_uri;
    char *match;
    char *use;
    XmlXPathComp *comp;
    XmlXPathComp *usecomp;
};

/** A decimal format (libxslt xsltDecimalFormat). */
typedef struct XsltDecimalFormat XsltDecimalFormat;
struct XsltDecimalFormat
{
    XsltDecimalFormat *next;
    char *name;
    char *digit;
    char *pattern_separator;
    char *minus_sign;
    char *infinity;
    char *no_number;
    char *decimal_point;
    char *grouping;
    char *percent;
    char *permille;
    char *zero_digit;
    char *ns_uri;
};

/** A global variable or parameter (libxslt xsltStackElem, the compile time part). */
typedef struct XsltStackElem XsltStackElem;
struct XsltStackElem
{
    XsltStackElem *next;
    XsltStylePreComp *comp;
    int computed;
    char *name;
    char *name_uri;
    char *select;
    XMQNode *tree;          // The content, if there is no select.
    XmlXPathResult *value;
    // At runtime (the names are borrowed from comp, owns_strings is false).
    bool owns_strings;
    int level;              // The depth in the template, -1 for with-param, -2 persistent.
    int flags;              // XSLT_VAR_GLOBAL, XSLT_VAR_IN_SELECT.
    struct XsltRVT *fragment; // The result tree fragments bound to the variable.
};

/** A document loaded by the stylesheet: an included module (libxslt xsltDocument). */
typedef struct XsltDocument XsltDocument;
struct XsltDocument
{
    XsltDocument *next;
    int main;
    XmlDom *doc;
    XsltDocument *includes; // The chain of includes, for the recursion check.
    int preproc;
};

/** An extension namespace declared by extension-element-prefixes (libxslt xsltExtDef). */
typedef struct XsltExtDef XsltExtDef;
struct XsltExtDef
{
    XsltExtDef *next;
    char *prefix;
    char *uri;
};

/** An attribute set (attributes.c). */
typedef struct XsltAttrElem XsltAttrElem;
struct XsltAttrElem
{
    XsltAttrElem *next;
    XMQNode *attr;          // The xsl:attribute element.
};

typedef struct XsltUseAttrSet XsltUseAttrSet;
struct XsltUseAttrSet
{
    XsltUseAttrSet *next;
    char *ncname;
    char *ns;
};

typedef enum { XSLT_ATTRSET_UNRESOLVED = 0, XSLT_ATTRSET_RESOLVING, XSLT_ATTRSET_RESOLVED } XsltAttrSetState;

typedef struct XsltAttrSet XsltAttrSet;
struct XsltAttrSet
{
    XsltAttrSet *next_in_order; // The sets of a stylesheet in declaration order.
    char *name;
    char *ns;
    XsltAttrSetState state;
    XsltAttrElem *attrs;
    XsltUseAttrSet *use_attr_sets;
};

struct XmlLibxslt1Stylesheet
{
    XmlLibxslt1Settings settings;

    // The stylesheet import relation is kept as a tree.
    XmlLibxslt1Stylesheet *parent;
    XmlLibxslt1Stylesheet *next;
    XmlLibxslt1Stylesheet *imports;
    XsltDocument *doc_list;

    XmlDom *doc;
    HashMap *strip_spaces;      // "name\1uri" -> "strip" or "preserve".
    int strip_all;
    HashMap *cdata_section;     // "name\1uri" -> "cdata".

    XsltStackElem *variables;   // Global variables and parameters.

    XsltTemplate *templates;
    HashMap *templates_hash;    // Template rules by the name of their last step, mode and mode uri.
    XmlLibxslt1Pattern *root_match;
    XmlLibxslt1Pattern *key_match;
    XmlLibxslt1Pattern *elem_match;
    XmlLibxslt1Pattern *attr_match;
    XmlLibxslt1Pattern *parent_match;
    XmlLibxslt1Pattern *text_match;
    XmlLibxslt1Pattern *pi_match;
    XmlLibxslt1Pattern *comment_match;

    HashMap *ns_aliases;        // literal namespace -> target namespace (or XSLT_UNDEFINED_DEFAULT_NS).
    HashMap *attribute_sets;    // "name\1uri" -> XsltAttrSet.
    XsltAttrSet *attribute_set_list; // The same, in declaration order.
    XsltExtDef *ns_defs;
    XsltKeyDef *keys;

    // Output.
    char *method;
    char *method_uri;
    char *version;
    char *encoding;
    int omit_xml_declaration;
    XsltDecimalFormat *decimal_format;
    int standalone;
    char *doctype_public;
    char *doctype_system;
    int indent;
    char *media_type;

    XsltStylePreComp *pre_comps;
    int warnings;
    int errors;
    char *excl_prefix;
    char **excl_prefix_tab;
    int excl_prefix_nr;
    int excl_prefix_max;

    int extras_nr;              // The number of runtime extras allocated by the patterns.
    XsltDocument *includes;     // For keeping track of nested includes.
    XsltAttrVT *att_vts;
    char *default_alias;
    int nopreproc;
    int literal_result;
    XmlLibxslt1Stylesheet *principal;
    int forwards_compatible;
    HashMap *named_templates;   // name and uri -> XsltTemplate.
    unsigned long op_count;
    unsigned long op_limit;
    int owns_doc;               // Freed with the stylesheet.
};

struct XmlLibxslt1Transform
{
    XmlLibxslt1Stylesheet *style;
    XmlDom *source;
    XmlXPathContext *xpath;
    XsltState state;

    XMQNode *node;          // The current node (ctxt->node).
    XMQNode *inst;          // The current instruction (ctxt->inst).
    // The context position and size of the libxslt xpath context.
    long xp_position;
    long xp_size;
    const char *mode;
    const char *mode_uri;

    XsltRuntimeExtra *extras;
    int extras_nr;
    int extras_max;

    // The transformation (transform.c, variables.c).
    XmlDom *output;         // The tree being built (the result or a result tree fragment).
    XMQNode *insert;        // Where new nodes are added.
    int type;               // XSLT_OUTPUT_XML, XSLT_OUTPUT_HTML, XSLT_OUTPUT_TEXT.
    XsltTemplate *templ;    // The current template.
    XsltTemplate **templ_tab;
    int templ_nr;
    int templ_max;
    XsltStackElem **vars_tab; // The stack of local variables and parameters.
    int vars_nr;
    int vars_max;
    int vars_base;
    XsltStackElem *vars;
    HashMap *global_vars;   // "name\1uri" -> XsltStackElem
    XsltStackElem *global_vars_list; // The same, for freeing.
    XsltTemplate *current_template_rule;
    struct XsltNodeList *node_list; // The node list being processed (for xsl:sort).
    int depth;
    int max_template_depth;
    int max_template_vars;
    XMQNode *initial_context_node;
    XmlDom *initial_context_doc;
    XsltStackElem *context_variable;
    struct XsltRVT *tmp_rvt;    // Result tree fragments of the template (xsltRegisterTmpRVT).
    struct XsltRVT *local_rvt;  // Of the current instruction (xsltRegisterLocalRVT).
    struct XsltRVT *persist_rvt; // Kept until the end (global variables).
    unsigned long op_count;
    unsigned long op_limit;
    struct XsltDocInfo *doc_list; // Documents of the transformation (the source and document()), with their keys.
    struct XsltDocInfo *document; // The current document.
    int has_templ_key_patterns;
    int nb_keys;
    int key_init_level;
    const char **params;    // name, value pairs (xpath expressions), NULL terminated.
    void *keyed_nodes;      // The source nodes with keys (XSLT_SOURCE_NODE_HAS_KEY).
    HashMap *generated_ids; // node -> id of generate-id().
    const char *output_file; // The file the result goes to (ctxt->outputFile), for xsl:document.
    unsigned long current_id;
};

/** The output method of the result (libxslt xsltOutputType). */
#define XSLT_OUTPUT_XML 0
#define XSLT_OUTPUT_HTML 1
#define XSLT_OUTPUT_TEXT 2

/** The lifetime of a result tree fragment (libxslt XSLT_RVT_*). */
#define XSLT_RVT_LOCAL 1
#define XSLT_RVT_FUNC_RESULT 2
#define XSLT_RVT_GLOBAL 3

/** A result tree fragment (libxslt RVT, an xmlDoc marked as a fragment). The record is the
    user data of the document node of its tree. */
typedef struct XsltRVT XsltRVT;
struct XsltRVT
{
    unsigned magic;
    XmlDom *dom;
    XsltRVT *next;
    XsltRVT *prev;
    int flag;               // libxslt doc->compression: XSLT_RVT_LOCAL, ...
    struct XsltDocInfo *keys; // The key tables of the fragment (libxslt doc->_private).
};
#define XSLT_RVT_MAGIC 0x52565421u

/** A node list being processed by apply-templates or for-each, sorted in place. */
typedef struct XsltNodeList
{
    XMQNode **nodes;
    int nr;
} XsltNodeList;

/** A document of the transformation with its key tables (libxslt xsltDocument). */
typedef struct XsltDocInfo XsltDocInfo;
struct XsltDocInfo
{
    XsltDocInfo *next;
    int main;               // The source document.
    XmlDom *doc;
    int owned;              // Loaded by document(), freed with the transformation.
    struct XsltKeyTable *keys;
    int nb_keys_computed;
};


// Functions shared by the xml_libxslt1 parts.
/** Like xsltTransformError(NULL, NULL, node, ...) with the messages going to the settings. */
void xslt1_settings_error(const XmlLibxslt1Settings *s, XMQNode *node, const char *fmt, ...);
XsltDecimalFormat *xslt1_decimal_format_get_by_qname(XmlLibxslt1Stylesheet *style, const char *ns_uri, const char *name);
int xslt1_utf8_char(const char *s, int *len);
void xslt1_comp_match_clear_cache(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *comp);
bool xslt1_node_has_key(XmlLibxslt1Transform *t, XMQNode *node);
int xslt1_compute_all_keys(XmlLibxslt1Transform *t, XMQNode *context_node);
void xslt1_transform_error(XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style, XMQNode *node, const char *fmt, ...);
void xslt1_generic_error(const XmlLibxslt1Settings *s, const char *fmt, ...);
char *xslt1_strdup(const char *s);
char *xslt1_strndup(const char *s, size_t n);
bool xslt1_streq(const char *a, const char *b);
bool xslt1_is_blank(int c);
bool xslt1_is_xslt_elem(const XMQNode *n);
bool xslt1_is_xslt_name(const XMQNode *n, const char *name);
char *xslt1_get_ns_prop_exact(XMQNode *node, const char *name, const char *ns);
char *xslt1_get_ns_prop(XMQNode *node, const char *name, const char *ns);
XsltTemplate *xslt1_get_template(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Stylesheet *style);
XmlLibxslt1Stylesheet *xslt1_next_import(XmlLibxslt1Stylesheet *cur);
char *xslt1_hash3_key(const char *a, const char *b, const char *c);
char *xslt1_key2(const char *a, const char *b);
int xslt1_validate_qname(const char *value);
char *xslt1_split_qname(const char *name, char **prefix);
XmlXPathNamespace *xslt1_ns_list(XMQNode *node, size_t *num);
XmlDom *xslt1_load(XmlLibxslt1Stylesheet *style, const char *uri, XmlLibxslt1LoadType type);
char *xslt1_node_get_base(XmlDom *doc, XMQNode *cur);
XmlXPathComp *xslt1_xpath_compile(const XmlLibxslt1Settings *s, XMQNode *elem, const char *str, bool no_variables);
char *xslt1_strncat(char *a, const char *b, size_t n);
char *xslt1_strcat(char *a, const char *b);
int xslt1_allocate_extra_ctxt(XmlLibxslt1Transform *t);
void xslt1_report_xpath_error(const XmlLibxslt1Settings *s, const char *expr, const XmlXPathError *err);
bool xslt1_is_blank_str(const char *s);
const char *xslt1_get_qname_uri(const XmlLibxslt1Settings *s, XMQNode *node, char **name);
const XmlLibxslt1Settings *xslt1_settings_of(XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style);
const char *xslt1_libxml2_node_name(const XMQNode *node);
XmlXPathResult *xslt1_get_key(XmlLibxslt1Transform *t, const char *name, const char *uri, const char *value);
bool xslt1_is_rtf(XmlLibxslt1Transform *t, XmlDom *dom);

#endif // XML_LIBXSLT1_INTERNALS

#ifndef XMQ_NO_LIBXSLT
#define XML_LIBXSLT1_XSLT_MODULE
#endif

#endif // XML_LIBXSLT1_XSLT_H
