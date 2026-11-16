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

/* Portions of this file are ported from libxml2 (xmlschemas.c, xmlschemas.h,
   schemasInternals.h, version 2.16.0 as found in the libxmq fork in 3rdparty/libxml2).
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

#ifndef XML_XSD10_SCHEMAS_H
#define XML_XSD10_SCHEMAS_H

/*
    XML Schema 1.0 structures: parsing schemas and validating documents,
    a port of libxml2 xmlschemas.c on the native dom (xml_dom), the native
    regular expressions and automata (xml_regexp), streaming patterns
    (xml_pattern) and built in types (xml_xsd10_types).

    The port is split in four parts that share this header:
    - xml_xsd10_schemas: helpers, error reporting, allocation, the context
      lifecycle, the dict and hash tables (and the public setters).
    - xml_xsd10_parse:   parsing the schema documents into components.
    - xml_xsd10_build:   content models, fixup and the component constraints.
    - xml_xsd10_valid:   simple type validation, dom validation and the
                         identity constraints.

    The messages (text, order and the fields of the structured errors) are
    the ones of libxml2. The plain error/warning callbacks are called like
    libxml2 calls them: callback(ctx, "%s", message).
*/

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#include"xml_regexp.h"
#include"xml_pattern.h"
#include"xml_xsd10_types.h"
#endif

#include<stdarg.h>
#include<stdio.h>

// Errors /////////////////////////////////////////////////////////

/** The severity of an error (libxml2 xmlErrorLevel). */
typedef enum {
    XML_XSD10_ERR_NONE = 0,
    XML_XSD10_ERR_WARNING = 1, /* A simple warning */
    XML_XSD10_ERR_ERROR = 2,   /* A recoverable error */
    XML_XSD10_ERR_FATAL = 3    /* A fatal error */
} XmlXsd10ErrorLevel;

/* The libxml2 error domains (xmlErrorDomain) that this part reports. */
#define XML_XSD10_FROM_PARSER 1    /* The XML parser (a schema document that does not parse) */
#define XML_XSD10_FROM_MEMORY 6    /* The memory allocator */
#define XML_XSD10_FROM_IO 8        /* The Input/Output stack (a schema document that cannot be loaded) */
#define XML_XSD10_FROM_SCHEMASP 16 /* The W3C XML Schemas parser module */
#define XML_XSD10_FROM_SCHEMASV 17 /* The W3C XML Schemas validation module */

/* Other libxml2 error codes (xmlParserErrors) used besides the schema codes below. */
#define XML_XSD10_ERR_OK 0
#define XML_XSD10_ERR_INTERNAL_ERROR 1
#define XML_XSD10_ERR_NO_MEMORY 2
#define XML_XSD10_IO_UNKNOWN 1500
#define XML_XSD10_IO_EACCES 1501
#define XML_XSD10_IO_ENOENT 1524
#define XML_XSD10_IO_LOAD_ERROR 1549

/**
    An error (libxml2 xmlError). Passed to the structured error callback, it is only
    valid during the call (libxml2 passes its thread local last error).
*/
typedef struct XmlXsd10Error {
    int domain;               /* What part of the library raised this error */
    int code;                 /* The error code, e.g. an xmlParserErrors */
    char *message;            /* human-readable informative error message */
    XmlXsd10ErrorLevel level; /* how consequent is the error */
    char *file;               /* the filename */
    int line;                 /* the line number if available */
    char *str1;               /* extra string information */
    char *str2;               /* extra string information */
    char *str3;               /* extra string information */
    int int1;                 /* extra number information */
    int int2;                 /* error column # or 0 if N/A */
    void *ctxt;               /* libxml2: the parser context if available. Native: NULL, or the context of
                                 the parser input as text (a const char*, eg "\n^\n") that
                                 xml_xsd10_format_error prints after the message like libxml2. */
    XMQNode *node;            /* the node in the tree */
} XmlXsd10Error;

/** printf like error and warning callbacks (libxml2 xmlSchemaValidityErrorFunc/WarningFunc).
    They are called as callback(ctx, "%s", message), exactly like libxml2 does. */
typedef void (*XmlXsd10ValidityErrorFunc)(void *ctx, const char *msg, ...);
typedef void (*XmlXsd10ValidityWarningFunc)(void *ctx, const char *msg, ...);
/** The structured error callback (libxml2 xmlStructuredErrorFunc). */
typedef void (*XmlXsd10StructuredErrorFunc)(void *user_data, const XmlXsd10Error *error);

/** Port of xmlFormatError: report an error to a printf like channel in the libxml2 format
    "file:line: element name: Schemas parser error : message". */
void xml_xsd10_format_error(const XmlXsd10Error *err, XmlXsd10ValidityErrorFunc channel, void *data);
/** The text xml_xsd10_format_error produces, malloced. */
char *xml_xsd10_format_error_string(const XmlXsd10Error *err);
/** Port of xmlGenericErrorDefaultFunc: print on stderr. This channel (and no callbacks at all)
    gives the libxml2 default output: the error formatted by xml_xsd10_format_error. */
void xml_xsd10_generic_error(void *ctx, const char *msg, ...);

// Schemas and contexts ///////////////////////////////////////////

/** A compiled schema (libxml2 xmlSchema). */
struct XmlXsd10Schema;
typedef struct XmlXsd10Schema XmlXsd10Schema;
/** A schema parser context (libxml2 xmlSchemaParserCtxt). */
struct XmlXsd10ParserCtxt;
typedef struct XmlXsd10ParserCtxt XmlXsd10ParserCtxt;
/** A validation context (libxml2 xmlSchemaValidCtxt). */
struct XmlXsd10ValidCtxt;
typedef struct XmlXsd10ValidCtxt XmlXsd10ValidCtxt;

/** Load the schema document at uri (an absolute uri or a path, as resolved against the
    including document), NULL if it cannot be loaded. The dom should get uri as its url
    (xml_dom_set_url). Used for xs:import, xs:include, xs:redefine, xsi:schemaLocation
    and the main schema of xml_xsd10_new_parser_ctxt. Replaces libxml2 xmlResourceLoader. */
typedef XmlDom *(*XmlXsd10Loader)(void *user_data, const char *uri);

/** The default loader: read the file (a file: url or a path) and parse it with the native
    parser, entities substituted (libxml2 XML_PARSE_NOENT). Problems are printed on stderr
    the way libxml2 prints them. user_data is not used. */
XmlDom *xml_xsd10_default_loader(void *user_data, const char *uri);

/** Validation options (libxml2 xmlSchemaValidOption). */
typedef enum {
    XML_XSD10_SCHEMA_VAL_VC_I_CREATE = 1<<0 /* Default/fixed: create an attribute node or an element's text node on the instance. */
} XmlXsd10ValidOption;

/** Port of xmlSchemaNewParserCtxt: the schema is loaded from URL (through the loader) when parsed. */
XmlXsd10ParserCtxt *xml_xsd10_new_parser_ctxt(const char *URL);
/** Port of xmlSchemaNewMemParserCtxt: the buffer is parsed with the native parser (not copied, keep it alive). */
XmlXsd10ParserCtxt *xml_xsd10_new_mem_parser_ctxt(const char *buffer, int size);
/** Port of xmlSchemaNewDocParserCtxt: the document is owned by the caller and must live as long as the
    schema (the components point into it). It is not modified by the native port. */
XmlXsd10ParserCtxt *xml_xsd10_new_doc_parser_ctxt(XmlDom *doc);
/** Port of xmlSchemaFreeParserCtxt. */
void xml_xsd10_free_parser_ctxt(XmlXsd10ParserCtxt *ctxt);
/** Port of xmlSchemaSetParserErrors. */
void xml_xsd10_set_parser_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10ValidityErrorFunc err, XmlXsd10ValidityWarningFunc warn, void *ctx);
/** Port of xmlSchemaSetParserStructuredErrors (takes precedence over the plain callbacks). */
void xml_xsd10_set_parser_structured_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10StructuredErrorFunc serror, void *ctx);
/** Port of xmlSchemaGetParserErrors. */
int xml_xsd10_get_parser_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10ValidityErrorFunc *err, XmlXsd10ValidityWarningFunc *warn, void **ctx);
/** Port of xmlSchemaSetResourceLoader: NULL loader restores xml_xsd10_default_loader. */
void xml_xsd10_set_resource_loader(XmlXsd10ParserCtxt *ctxt, XmlXsd10Loader loader, void *data);
/** Port of xmlSchemaParse: NULL on errors (they have been reported). */
XmlXsd10Schema *xml_xsd10_parse(XmlXsd10ParserCtxt *ctxt);
/** Port of xmlSchemaFree. */
void xml_xsd10_free(XmlXsd10Schema *schema);

/** Port of xmlSchemaNewValidCtxt. */
XmlXsd10ValidCtxt *xml_xsd10_new_valid_ctxt(XmlXsd10Schema *schema);
/** Port of xmlSchemaFreeValidCtxt. */
void xml_xsd10_free_valid_ctxt(XmlXsd10ValidCtxt *ctxt);
/** Port of xmlSchemaSetValidErrors. */
void xml_xsd10_set_valid_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10ValidityErrorFunc err, XmlXsd10ValidityWarningFunc warn, void *ctx);
/** Port of xmlSchemaSetValidStructuredErrors. */
void xml_xsd10_set_valid_structured_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10StructuredErrorFunc serror, void *ctx);
/** Port of xmlSchemaGetValidErrors. */
int xml_xsd10_get_valid_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10ValidityErrorFunc *err, XmlXsd10ValidityWarningFunc *warn, void **ctx);
/** Port of xmlSchemaSetValidOptions: options is a combination of XmlXsd10ValidOption. */
int xml_xsd10_set_valid_options(XmlXsd10ValidCtxt *ctxt, int options);
/** Port of xmlSchemaValidCtxtGetOptions. */
int xml_xsd10_valid_ctxt_get_options(XmlXsd10ValidCtxt *ctxt);
/** Port of xmlSchemaIsValid: 1 if no error happened in the context (and its schema is usable). */
int xml_xsd10_is_valid(XmlXsd10ValidCtxt *ctxt);
/** Port of xmlSchemaValidateSetFilename: the file name used in messages. */
void xml_xsd10_validate_set_filename(XmlXsd10ValidCtxt *vctxt, const char *filename);
/** Port of xmlSchemaValidateDoc: 0 if the document is valid, a positive error code otherwise,
    -1 on internal or API errors. */
int xml_xsd10_validate_doc(XmlXsd10ValidCtxt *ctxt, XmlDom *doc);
/** Port of xmlSchemaValidateOneElement: validate a subtree. */
int xml_xsd10_validate_one_element(XmlXsd10ValidCtxt *ctxt, XMQNode *elem);

// Internals, only seen by the xml_xsd10 parts (and in dist/xmq.c).
#if defined(XML_XSD10_INTERNALS) || defined(BUILDING_DIST_XMQ)

/*
    Internals shared by the four xml_xsd10 schema parts. Not an API.

    NAMING. Everything is named mechanically after libxml2 xmlschemas.c, so that the
    parts can be written independently and still call each other:
    - Functions: xmlSchemaFooBar -> xml_xsd10_foo_bar (static or not, all are extern
      here). Strip "xmlSchema" (or "xml" for the few xmlFoo functions of xmlschemas.c),
      put '_' before every capital that follows a lower case letter or digit and before
      the last capital of a run of capitals followed by a lower case letter, lower case:
      xmlSchemaPErrMemory -> xml_xsd10_p_err_memory, xmlSchemaVErrFull -> xml_xsd10_v_err_full,
      xmlSchemaIDCNewBinding -> xml_xsd10_idc_new_binding, xmlSchemaFormatQName ->
      xml_xsd10_format_q_name, xmlSchemaCheckCOSSTDerivedOK -> xml_xsd10_check_cosst_derived_ok,
      xmlSchemaErr4Line -> xml_xsd10_err4_line, xmlFreeIDCHashEntry -> xml_xsd10_free_idc_hash_entry.
      Every prototype below has the libxml2 name in a comment, grep for it.
      Exceptions: xmlSchemaCheckFacet -> xml_xsd10_check_facet_ctxt (xml_xsd10_check_facet is the
      reduced port in xml_xsd10_types), xmlSchemaNewFacet/xmlSchemaFreeFacet are xml_xsd10_new_facet
      and xml_xsd10_free_facet of xml_xsd10_types.
    - Types: struct _xmlSchemaFoo / xmlSchemaFoo -> struct XmlXsd10Foo / XmlXsd10Foo,
      xmlSchemaFooPtr -> XmlXsd10Foo *, xmlSchema -> XmlXsd10Schema, xmlIDCHashEntry ->
      XmlXsd10IDCHashEntry. The schemasInternals.h component structs are in xml_xsd10_types.h.
      Fields keep their libxml2 names.
    - Translated types: xmlChar -> char, xmlNodePtr -> XMQNode *, xmlAttrPtr -> XMQNode * (an
      attribute node, kind XML_DOM_ATTRIBUTE: convert with xml_node_as_attr/xml_attr_node),
      xmlDocPtr -> XmlDom *, xmlNsPtr -> XmlNs *, xmlDictPtr -> XmlXsd10Dict *, xmlHashTablePtr
      -> XmlXsd10Hash *, xmlRegexpPtr, xmlRegExecCtxtPtr, xmlAutomataPtr, xmlAutomataStatePtr ->
      XmlRegexp *, XmlRegExecCtxt *, XmlAutomata *, XmlAutomataState * (xml_regexp), xmlPatternPtr,
      xmlStreamCtxtPtr -> XmlPattern *, XmlStreamCtxt * (xml_pattern), xmlSchemaValPtr ->
      XmlXsd10Val *, xmlParserErrors -> int, xmlErrorLevel -> XmlXsd10ErrorLevel,
      xmlElementType -> XmlNodeKind (same values as libxml2: XML_ELEMENT_NODE -> XML_DOM_ELEMENT,
      XML_ATTRIBUTE_NODE -> XML_DOM_ATTRIBUTE, XML_TEXT_NODE -> XML_DOM_TEXT, XML_CDATA_SECTION_NODE
      -> XML_DOM_CDATA, XML_ENTITY_REF_NODE -> XML_DOM_ENTITY_REF, XML_ENTITY_DECL ->
      XML_DOM_ENTITY_DECL), xmlStructuredErrorFunc -> XmlXsd10StructuredErrorFunc,
      xmlResourceLoader -> XmlXsd10Loader.
    - Macros: XML_SCHEMA_FOO -> XML_XSD10_SCHEMA_FOO, XML_SCHEMAS_FOO -> XML_XSD10_SCHEMAS_FOO,
      XML_SCHEMAP_FOO / XML_SCHEMAV_FOO -> XML_XSD10_SCHEMAP_FOO / XML_XSD10_SCHEMAV_FOO, other
      XML_FOO -> XML_XSD10_FOO, the macros without an XML_ prefix (WXS_*, IS_SCHEMA, FREE_AND_NULL,
      VERROR, PERROR_INT, HFAILURE, ...) -> XSD10_ + name (XSD10_WXS_IS_COMPLEX, XSD10_IS_SCHEMA,
      ...), UNBOUNDED -> XML_XSD10_UNBOUNDED. Function like macros named like functions
      (xmlSchemaGetQNameRefName) are named like functions. Macros used by one part only are
      defined in that part's .c with the same rule and #undef'd at its end (dist/xmq.c is one
      translation unit, also compiled as C++).
    - Helpers replacing libxml2 tree/string functions: xmlStrEqual -> xml_xsd10_str_equal,
      xmlStrdup -> xml_xsd10_strdup, xmlStrndup, xmlStrcat, xmlStrncat, xmlStrncatNew, xmlStrlen,
      xmlCharStrdup, xmlEscapeFormatString, xmlGrowCapacity likewise (xml_xsd10_strcat ...),
      the tree.c functions get xml_xsd10_tree_ (xml_xsd10_tree_validate_qname, _split_qname2,
      _search_ns, _get_no_ns_prop, _node_get_content, _node_get_base, _get_ns_list, ...), see below.
      xmlMalloc/xmlRealloc/xmlFree are malloc/realloc/free (cast the result of malloc for C++).

    DESIGN DECISIONS for the whole port.
    - Dict. libxml2 interns the component strings in an xmlDict and compares them by pointer
      in places (WXS_ARE_DEFAULT_STR_EQUAL, "ns == pctxt->targetNamespace", ...). XmlXsd10Dict
      is a real interning dict with the same api (create/reference/free/lookup/qlookup): every
      string from xml_xsd10_dict_lookup lives until the dict is freed, equal strings from the
      same dict are the same pointer, so those pointer comparisons stay valid. The schema, the
      parser context and the construction context share one reference counted dict exactly
      like libxml2 (xmlDictReference). Component strings are never freed individually.
      BUT: libxml2 parses the schema documents into the same dict, the native dom does not
      intern. Any comparison of a string from a node (xml_node_name, xml_node_ns_uri,
      xml_ns_href, attribute values) with a dict string must use xml_xsd10_str_equal, or the
      node string must be interned first with xml_xsd10_dict_lookup.
    - Hash. libxml2 hash tables (xmlHashCreate) are seeded with a random value per table
      (xmlRandom, seeded from getentropy), so libxml2's xmlHashScan order differs between
      runs and no libxml2 output can depend on it (it only orders the augmentation of the
      identity constraints of several imported schemas). hash.c is therefore not ported:
      XmlXsd10Hash has the xmlHash api for 1-3 string keys (NULL keys allowed, NULL and ""
      differ) and scans in insertion order (deterministic). Keys are copied.
    - Ids. libxml2 registers xs:ID attributes in doc->ids (xmlAddIDSafe, attr->atype). The
      schema documents get an XmlXsd10Ids per bucket (XmlXsd10Bucket.ids, created when needed),
      the instance document one in the validation context (XmlXsd10ValidCtxt.ids, pass it to the
      *_ids functions of xml_xsd10_types).
    - Nodes. "node->ns != NULL ... node->ns->href" is xml_node_ns_uri(node) (NULL when the element
      or attribute has no namespace), node->name is xml_node_name, attr->parent and node->parent
      are xml_node_parent, node->properties are xml_node_first_attr/xml_attr_next (xml_attr_node
      gives the attribute node), xmlNodeGetContent is xml_xsd10_tree_node_get_content (malloced).
    - Node lines are xml_node_line. psvi, _private and xmlGetNodePath are not used by
      xmlschemas.c. The libxml2 65535 line truncation does not exist in the native dom.
    - Regexps. The schema parts call xml_regexp_* and xml_automata_* directly. The pattern facets
      of xml_xsd10_types are evaluated and freed through its regexp hooks, which the parser context
      creation installs (xml_xsd10_install_regexp_hooks) to call xml_regexp: facet->regexp is an
      XmlRegexp * compiled with xml_regexp_compile.
    - Documents. Schema documents are loaded through the loader of the parser context
      (xml_xsd10_p_read_file/xml_xsd10_p_read_memory replace xmlCtxtReadFile/xmlCtxtReadMemory).
      The native parse never modifies the schema documents of the user (xmlSchemaCleanupDoc
      removes nodes in libxml2: skip such nodes instead of unlinking them in a preserved doc).
    - Streaming. The reader (xmlSchemaVReaderWalk), the SAX handlers, xmlSchemaSAXPlug and the
      stream/file validation functions are not ported: the validation context has no input,
      sax, parserCtxt, reader, locFunc, locCtxt fields. xmlSchemaValidateDoc and
      xmlSchemaValidateOneElement on a dom are ported.
    - Errors. xmlVRaiseError, xmlFormatError and xmlRaiseMemoryError are ported here
      (xml_xsd10_v_raise_error, xml_xsd10_format_error, xml_xsd10_raise_memory_error) with the
      libxml2 semantics: structured callback first, else the plain callback with "%s", else the
      libxml2 formatted text on stderr. Messages are formatted with vsnprintf and truncated at
      MAX_ERR_MSG_SIZE (64000) like xmlStrVASPrintf. NULL %s arguments print "(null)" (glibc).
*/

// The libxml2 constants that libxml2 does not prefix with XML_SCHEMA.
#define XML_XSD10_MAX_ITEMS 1000000000 /* 1 billion, libxml2 XML_MAX_ITEMS */
#define XML_XSD10_MAX_ERR_MSG_SIZE 64000
#define XML_XSD10_XML_XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"

/* The XML Schemas namespaces (libxml2 xmlSchemaNs, xmlSchemaInstanceNs, xmlNamespaceNs). */
extern const char xml_xsd10_ns[];
extern const char xml_xsd10_instance_ns[];
extern const char xml_xsd10_namespace_ns[];

/** The schema error codes (libxml2 xmlParserErrors XML_SCHEMAP_* and XML_SCHEMAV_*). */
typedef enum {
    XML_XSD10_SCHEMAP_PREFIX_UNDEFINED = 1700,
    XML_XSD10_SCHEMAP_ATTRFORMDEFAULT_VALUE = 1701,
    XML_XSD10_SCHEMAP_ATTRGRP_NONAME_NOREF = 1702,
    XML_XSD10_SCHEMAP_ATTR_NONAME_NOREF = 1703,
    XML_XSD10_SCHEMAP_COMPLEXTYPE_NONAME_NOREF = 1704,
    XML_XSD10_SCHEMAP_ELEMFORMDEFAULT_VALUE = 1705,
    XML_XSD10_SCHEMAP_ELEM_NONAME_NOREF = 1706,
    XML_XSD10_SCHEMAP_EXTENSION_NO_BASE = 1707,
    XML_XSD10_SCHEMAP_FACET_NO_VALUE = 1708,
    XML_XSD10_SCHEMAP_FAILED_BUILD_IMPORT = 1709,
    XML_XSD10_SCHEMAP_GROUP_NONAME_NOREF = 1710,
    XML_XSD10_SCHEMAP_IMPORT_NAMESPACE_NOT_URI = 1711,
    XML_XSD10_SCHEMAP_IMPORT_REDEFINE_NSNAME = 1712,
    XML_XSD10_SCHEMAP_IMPORT_SCHEMA_NOT_URI = 1713,
    XML_XSD10_SCHEMAP_INVALID_BOOLEAN = 1714,
    XML_XSD10_SCHEMAP_INVALID_ENUM = 1715,
    XML_XSD10_SCHEMAP_INVALID_FACET = 1716,
    /* XML_XSD10_SCHEMAP_INVALID_FACET_VALUE = 1717 is in xml_xsd10_types.h */
    XML_XSD10_SCHEMAP_INVALID_MAXOCCURS = 1718,
    XML_XSD10_SCHEMAP_INVALID_MINOCCURS = 1719,
    XML_XSD10_SCHEMAP_INVALID_REF_AND_SUBTYPE = 1720,
    XML_XSD10_SCHEMAP_INVALID_WHITE_SPACE = 1721,
    XML_XSD10_SCHEMAP_NOATTR_NOREF = 1722,
    XML_XSD10_SCHEMAP_NOTATION_NO_NAME = 1723,
    XML_XSD10_SCHEMAP_NOTYPE_NOREF = 1724,
    XML_XSD10_SCHEMAP_REF_AND_SUBTYPE = 1725,
    XML_XSD10_SCHEMAP_RESTRICTION_NONAME_NOREF = 1726,
    XML_XSD10_SCHEMAP_SIMPLETYPE_NONAME = 1727,
    XML_XSD10_SCHEMAP_TYPE_AND_SUBTYPE = 1728,
    XML_XSD10_SCHEMAP_UNKNOWN_ALL_CHILD = 1729,
    XML_XSD10_SCHEMAP_UNKNOWN_ANYATTRIBUTE_CHILD = 1730,
    XML_XSD10_SCHEMAP_UNKNOWN_ATTR_CHILD = 1731,
    XML_XSD10_SCHEMAP_UNKNOWN_ATTRGRP_CHILD = 1732,
    XML_XSD10_SCHEMAP_UNKNOWN_ATTRIBUTE_GROUP = 1733,
    XML_XSD10_SCHEMAP_UNKNOWN_BASE_TYPE = 1734,
    XML_XSD10_SCHEMAP_UNKNOWN_CHOICE_CHILD = 1735,
    XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXCONTENT_CHILD = 1736,
    XML_XSD10_SCHEMAP_UNKNOWN_COMPLEXTYPE_CHILD = 1737,
    XML_XSD10_SCHEMAP_UNKNOWN_ELEM_CHILD = 1738,
    XML_XSD10_SCHEMAP_UNKNOWN_EXTENSION_CHILD = 1739,
    XML_XSD10_SCHEMAP_UNKNOWN_FACET_CHILD = 1740,
    XML_XSD10_SCHEMAP_UNKNOWN_FACET_TYPE = 1741,
    XML_XSD10_SCHEMAP_UNKNOWN_GROUP_CHILD = 1742,
    XML_XSD10_SCHEMAP_UNKNOWN_IMPORT_CHILD = 1743,
    XML_XSD10_SCHEMAP_UNKNOWN_LIST_CHILD = 1744,
    XML_XSD10_SCHEMAP_UNKNOWN_NOTATION_CHILD = 1745,
    XML_XSD10_SCHEMAP_UNKNOWN_PROCESSCONTENT_CHILD = 1746,
    XML_XSD10_SCHEMAP_UNKNOWN_REF = 1747,
    XML_XSD10_SCHEMAP_UNKNOWN_RESTRICTION_CHILD = 1748,
    XML_XSD10_SCHEMAP_UNKNOWN_SCHEMAS_CHILD = 1749,
    XML_XSD10_SCHEMAP_UNKNOWN_SEQUENCE_CHILD = 1750,
    XML_XSD10_SCHEMAP_UNKNOWN_SIMPLECONTENT_CHILD = 1751,
    XML_XSD10_SCHEMAP_UNKNOWN_SIMPLETYPE_CHILD = 1752,
    XML_XSD10_SCHEMAP_UNKNOWN_TYPE = 1753,
    XML_XSD10_SCHEMAP_UNKNOWN_UNION_CHILD = 1754,
    XML_XSD10_SCHEMAP_ELEM_DEFAULT_FIXED = 1755,
    /* XML_XSD10_SCHEMAP_REGEXP_INVALID = 1756 is in xml_xsd10_types.h */
    XML_XSD10_SCHEMAP_FAILED_LOAD = 1757,
    XML_XSD10_SCHEMAP_NOTHING_TO_PARSE = 1758,
    XML_XSD10_SCHEMAP_NOROOT = 1759,
    XML_XSD10_SCHEMAP_REDEFINED_GROUP = 1760,
    XML_XSD10_SCHEMAP_REDEFINED_TYPE = 1761,
    XML_XSD10_SCHEMAP_REDEFINED_ELEMENT = 1762,
    XML_XSD10_SCHEMAP_REDEFINED_ATTRGROUP = 1763,
    XML_XSD10_SCHEMAP_REDEFINED_ATTR = 1764,
    XML_XSD10_SCHEMAP_REDEFINED_NOTATION = 1765,
    XML_XSD10_SCHEMAP_FAILED_PARSE = 1766,
    XML_XSD10_SCHEMAP_UNKNOWN_PREFIX = 1767,
    XML_XSD10_SCHEMAP_DEF_AND_PREFIX = 1768,
    XML_XSD10_SCHEMAP_UNKNOWN_INCLUDE_CHILD = 1769,
    XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NOT_URI = 1770,
    XML_XSD10_SCHEMAP_INCLUDE_SCHEMA_NO_URI = 1771,
    XML_XSD10_SCHEMAP_NOT_SCHEMA = 1772,
    XML_XSD10_SCHEMAP_UNKNOWN_MEMBER_TYPE = 1773,
    XML_XSD10_SCHEMAP_INVALID_ATTR_USE = 1774,
    XML_XSD10_SCHEMAP_RECURSIVE = 1775,
    XML_XSD10_SCHEMAP_SUPERNUMEROUS_LIST_ITEM_TYPE = 1776,
    XML_XSD10_SCHEMAP_INVALID_ATTR_COMBINATION = 1777,
    XML_XSD10_SCHEMAP_INVALID_ATTR_INLINE_COMBINATION = 1778,
    XML_XSD10_SCHEMAP_MISSING_SIMPLETYPE_CHILD = 1779,
    XML_XSD10_SCHEMAP_INVALID_ATTR_NAME = 1780,
    XML_XSD10_SCHEMAP_REF_AND_CONTENT = 1781,
    XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_1 = 1782,
    XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_2 = 1783,
    XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_3 = 1784,
    XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_4 = 1785,
    XML_XSD10_SCHEMAP_CT_PROPS_CORRECT_5 = 1786,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_1 = 1787,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_1 = 1788,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_2 = 1789,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_2 = 1790,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_3 = 1791,
    XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER = 1792,
    XML_XSD10_SCHEMAP_INTERSECTION_NOT_EXPRESSIBLE = 1793,
    XML_XSD10_SCHEMAP_UNION_NOT_EXPRESSIBLE = 1794,
    XML_XSD10_SCHEMAP_SRC_IMPORT_3_1 = 1795,
    XML_XSD10_SCHEMAP_SRC_IMPORT_3_2 = 1796,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_1 = 1797,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_2 = 1798,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_4_3 = 1799,
    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_3 = 1800,
    XML_XSD10_SCHEMAV_NOROOT = 1801,
    XML_XSD10_SCHEMAV_UNDECLAREDELEM = 1802,
    XML_XSD10_SCHEMAV_NOTTOPLEVEL = 1803,
    XML_XSD10_SCHEMAV_MISSING = 1804,
    XML_XSD10_SCHEMAV_WRONGELEM = 1805,
    XML_XSD10_SCHEMAV_NOTYPE = 1806,
    XML_XSD10_SCHEMAV_NOROLLBACK = 1807,
    XML_XSD10_SCHEMAV_ISABSTRACT = 1808,
    XML_XSD10_SCHEMAV_NOTEMPTY = 1809,
    XML_XSD10_SCHEMAV_ELEMCONT = 1810,
    XML_XSD10_SCHEMAV_HAVEDEFAULT = 1811,
    XML_XSD10_SCHEMAV_NOTNILLABLE = 1812,
    XML_XSD10_SCHEMAV_EXTRACONTENT = 1813,
    XML_XSD10_SCHEMAV_INVALIDATTR = 1814,
    XML_XSD10_SCHEMAV_INVALIDELEM = 1815,
    XML_XSD10_SCHEMAV_NOTDETERMINIST = 1816,
    XML_XSD10_SCHEMAV_CONSTRUCT = 1817,
    XML_XSD10_SCHEMAV_INTERNAL = 1818,
    XML_XSD10_SCHEMAV_NOTSIMPLE = 1819,
    XML_XSD10_SCHEMAV_ATTRUNKNOWN = 1820,
    XML_XSD10_SCHEMAV_ATTRINVALID = 1821,
    XML_XSD10_SCHEMAV_VALUE = 1822,
    XML_XSD10_SCHEMAV_FACET = 1823,
    /* XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1 = 1824 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2 = 1825 is in xml_xsd10_types.h */
    XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3 = 1826,
    XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1 = 1827,
    XML_XSD10_SCHEMAV_CVC_TYPE_3_1_2 = 1828,
    XML_XSD10_SCHEMAV_CVC_FACET_VALID = 1829,
    /* XML_XSD10_SCHEMAV_CVC_LENGTH_VALID = 1830 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID = 1831 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID = 1832 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID = 1833 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID = 1834 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID = 1835 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID = 1836 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID = 1837 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID = 1838 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_PATTERN_VALID = 1839 is in xml_xsd10_types.h */
    /* XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID = 1840 is in xml_xsd10_types.h */
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1 = 1841,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_2 = 1842,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_3 = 1843,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_4 = 1844,
    XML_XSD10_SCHEMAV_CVC_ELT_1 = 1845,
    XML_XSD10_SCHEMAV_CVC_ELT_2 = 1846,
    XML_XSD10_SCHEMAV_CVC_ELT_3_1 = 1847,
    XML_XSD10_SCHEMAV_CVC_ELT_3_2_1 = 1848,
    XML_XSD10_SCHEMAV_CVC_ELT_3_2_2 = 1849,
    XML_XSD10_SCHEMAV_CVC_ELT_4_1 = 1850,
    XML_XSD10_SCHEMAV_CVC_ELT_4_2 = 1851,
    XML_XSD10_SCHEMAV_CVC_ELT_4_3 = 1852,
    XML_XSD10_SCHEMAV_CVC_ELT_5_1_1 = 1853,
    XML_XSD10_SCHEMAV_CVC_ELT_5_1_2 = 1854,
    XML_XSD10_SCHEMAV_CVC_ELT_5_2_1 = 1855,
    XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_1 = 1856,
    XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_1 = 1857,
    XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_2 = 1858,
    XML_XSD10_SCHEMAV_CVC_ELT_6 = 1859,
    XML_XSD10_SCHEMAV_CVC_ELT_7 = 1860,
    XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_1 = 1861,
    XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_2 = 1862,
    XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_3 = 1863,
    XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_4 = 1864,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_1 = 1865,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_1 = 1866,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_2 = 1867,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_4 = 1868,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_1 = 1869,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_5_2 = 1870,
    XML_XSD10_SCHEMAV_ELEMENT_CONTENT = 1871,
    XML_XSD10_SCHEMAV_DOCUMENT_ELEMENT_MISSING = 1872,
    XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_1 = 1873,
    XML_XSD10_SCHEMAV_CVC_AU = 1874,
    XML_XSD10_SCHEMAV_CVC_TYPE_1 = 1875,
    XML_XSD10_SCHEMAV_CVC_TYPE_2 = 1876,
    XML_XSD10_SCHEMAV_CVC_IDC = 1877,
    XML_XSD10_SCHEMAV_CVC_WILDCARD = 1878,
    XML_XSD10_SCHEMAV_MISC = 1879,
    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1 = 3000,
    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_2 = 3001,
    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_3 = 3002,
    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_4 = 3003,
    XML_XSD10_SCHEMAP_SRC_RESOLVE = 3004,
    XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE = 3005,
    XML_XSD10_SCHEMAP_SRC_LIST_ITEMTYPE_OR_SIMPLETYPE = 3006,
    XML_XSD10_SCHEMAP_SRC_UNION_MEMBERTYPES_OR_SIMPLETYPES = 3007,
    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_1 = 3008,
    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_2 = 3009,
    XML_XSD10_SCHEMAP_ST_PROPS_CORRECT_3 = 3010,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_1 = 3011,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_2 = 3012,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_1 = 3013,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_1_3_2 = 3014,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_1 = 3015,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_1 = 3016,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_1_2 = 3017,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_1 = 3018,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_2 = 3019,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_3 = 3020,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_4 = 3021,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_2_3_2_5 = 3022,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_1 = 3023,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1 = 3024,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_1_2 = 3025,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_2 = 3026,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_1 = 3027,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_3 = 3028,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_4 = 3029,
    XML_XSD10_SCHEMAP_COS_ST_RESTRICTS_3_3_2_5 = 3030,
    XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_1 = 3031,
    XML_XSD10_SCHEMAP_COS_ST_DERIVED_OK_2_2 = 3032,
    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED = 3033,
    XML_XSD10_SCHEMAP_S4S_ELEM_MISSING = 3034,
    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED = 3035,
    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING = 3036,
    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE = 3037,
    XML_XSD10_SCHEMAP_SRC_ELEMENT_1 = 3038,
    XML_XSD10_SCHEMAP_SRC_ELEMENT_2_1 = 3039,
    XML_XSD10_SCHEMAP_SRC_ELEMENT_2_2 = 3040,
    XML_XSD10_SCHEMAP_SRC_ELEMENT_3 = 3041,
    XML_XSD10_SCHEMAP_P_PROPS_CORRECT_1 = 3042,
    XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1 = 3043,
    XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2 = 3044,
    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_2 = 3045,
    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_3 = 3046,
    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_4 = 3047,
    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_5 = 3048,
    XML_XSD10_SCHEMAP_E_PROPS_CORRECT_6 = 3049,
    XML_XSD10_SCHEMAP_SRC_INCLUDE = 3050,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1 = 3051,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_2 = 3052,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_1 = 3053,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_2 = 3054,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4 = 3055,
    XML_XSD10_SCHEMAP_NO_XMLNS = 3056,
    XML_XSD10_SCHEMAP_NO_XSI = 3057,
    XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_1 = 3058,
    XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1 = 3059,
    XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_1 = 3060,
    XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_2_2 = 3061,
    XML_XSD10_SCHEMAP_CVC_SIMPLE_TYPE = 3062,
    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_1 = 3063,
    XML_XSD10_SCHEMAP_SRC_IMPORT_1_1 = 3064,
    XML_XSD10_SCHEMAP_SRC_IMPORT_1_2 = 3065,
    XML_XSD10_SCHEMAP_SRC_IMPORT_2 = 3066,
    XML_XSD10_SCHEMAP_SRC_IMPORT_2_1 = 3067,
    XML_XSD10_SCHEMAP_SRC_IMPORT_2_2 = 3068,
    XML_XSD10_SCHEMAP_INTERNAL = 3069,
    XML_XSD10_SCHEMAP_NOT_DETERMINISTIC = 3070,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_1 = 3071,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_2 = 3072,
    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_GROUP_3 = 3073,
    XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_1 = 3074,
    XML_XSD10_SCHEMAP_MG_PROPS_CORRECT_2 = 3075,
    XML_XSD10_SCHEMAP_SRC_CT_1 = 3076,
    XML_XSD10_SCHEMAP_DERIVATION_OK_RESTRICTION_2_1_3 = 3077,
    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT_2 = 3078,
    XML_XSD10_SCHEMAP_A_PROPS_CORRECT_2 = 3079,
    XML_XSD10_SCHEMAP_C_PROPS_CORRECT = 3080,
    XML_XSD10_SCHEMAP_SRC_REDEFINE = 3081,
    XML_XSD10_SCHEMAP_SRC_IMPORT = 3082,
    XML_XSD10_SCHEMAP_WARN_SKIP_SCHEMA = 3083,
    XML_XSD10_SCHEMAP_WARN_UNLOCATED_SCHEMA = 3084,
    XML_XSD10_SCHEMAP_WARN_ATTR_REDECL_PROH = 3085,
    XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH = 3086,
    XML_XSD10_SCHEMAP_AG_PROPS_CORRECT = 3087,
    XML_XSD10_SCHEMAP_COS_CT_EXTENDS_1_2 = 3088,
    XML_XSD10_SCHEMAP_AU_PROPS_CORRECT = 3089,
    XML_XSD10_SCHEMAP_A_PROPS_CORRECT_3 = 3090,
    XML_XSD10_SCHEMAP_COS_ALL_LIMITED = 3091
} XmlXsd10ParserErrors;

/** libxml2 xmlSchemaValidError, only XML_SCHEMAS_ERR_NOTDETERMINIST is referenced (in a comment). */
typedef enum {
    XML_XSD10_SCHEMAS_ERR_OK = 0,
    XML_XSD10_SCHEMAS_ERR_NOROOT = 1,
    XML_XSD10_SCHEMAS_ERR_UNDECLAREDELEM,
    XML_XSD10_SCHEMAS_ERR_NOTTOPLEVEL,
    XML_XSD10_SCHEMAS_ERR_MISSING,
    XML_XSD10_SCHEMAS_ERR_WRONGELEM,
    XML_XSD10_SCHEMAS_ERR_NOTYPE,
    XML_XSD10_SCHEMAS_ERR_NOROLLBACK,
    XML_XSD10_SCHEMAS_ERR_ISABSTRACT,
    XML_XSD10_SCHEMAS_ERR_NOTEMPTY,
    XML_XSD10_SCHEMAS_ERR_ELEMCONT,
    XML_XSD10_SCHEMAS_ERR_HAVEDEFAULT,
    XML_XSD10_SCHEMAS_ERR_NOTNILLABLE,
    XML_XSD10_SCHEMAS_ERR_EXTRACONTENT,
    XML_XSD10_SCHEMAS_ERR_INVALIDATTR,
    XML_XSD10_SCHEMAS_ERR_INVALIDELEM,
    XML_XSD10_SCHEMAS_ERR_NOTDETERMINIST,
    XML_XSD10_SCHEMAS_ERR_CONSTRUCT,
    XML_XSD10_SCHEMAS_ERR_INTERNAL,
    XML_XSD10_SCHEMAS_ERR_NOTSIMPLE,
    XML_XSD10_SCHEMAS_ERR_ATTRUNKNOWN,
    XML_XSD10_SCHEMAS_ERR_ATTRINVALID,
    XML_XSD10_SCHEMAS_ERR_VALUE,
    XML_XSD10_SCHEMAS_ERR_FACET,
    XML_XSD10_SCHEMAS_ERR_,
    XML_XSD10_SCHEMAS_ERR_XXX
} XmlXsd10ValidError;

/* libxml2 xmlSchemaValidOption XML_SCHEMA_VAL_XSI_ASSEMBLE (obsolete, not in xmlschemas.h's enum anymore). */
#define XML_XSD10_SCHEMA_VAL_XSI_ASSEMBLE (1<<1)

/* The flags of XmlXsd10Schema (schemasInternals.h). */
/* Reflects elementFormDefault == qualified in an XML schema document. */
#define XML_XSD10_SCHEMAS_QUALIF_ELEM (1 << 0)
/* Reflects attributeFormDefault == qualified in an XML schema document. */
#define XML_XSD10_SCHEMAS_QUALIF_ATTR (1 << 1)
/* the schema has "extension" in the set of finalDefault. */
#define XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION (1 << 2)
/* the schema has "restriction" in the set of finalDefault. */
#define XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION (1 << 3)
/* the schema has "list" in the set of finalDefault. */
#define XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST (1 << 4)
/* the schema has "union" in the set of finalDefault. */
#define XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION (1 << 5)
/* the schema has "extension" in the set of blockDefault. */
#define XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION (1 << 6)
/* the schema has "restriction" in the set of blockDefault. */
#define XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION (1 << 7)
/* the schema has "substitution" in the set of blockDefault. */
#define XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION (1 << 8)
/* the schema is currently including an other schema with no target namespace. */
#define XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS (1 << 9)
/* The type redefines a type (schemasInternals.h, obsolete). */
#define XML_XSD10_SCHEMAS_TYPE_REDEFINING ((int)(1U << 31))

// Dict (libxml2 xmlDict) /////////////////////////////////////////

/** An interning string dictionary, reference counted (libxml2 xmlDict). */
struct XmlXsd10Dict;
typedef struct XmlXsd10Dict XmlXsd10Dict;

/** Port of xmlDictCreate: a dict with one reference. */
XmlXsd10Dict *xml_xsd10_dict_create(void);
/** Port of xmlDictReference: add a reference, 0 or -1 if dict is NULL. */
int xml_xsd10_dict_reference(XmlXsd10Dict *dict);
/** Port of xmlDictFree: drop a reference, frees the dict and all its strings with the last one. */
void xml_xsd10_dict_free(XmlXsd10Dict *dict);
/** Port of xmlDictLookup: the interned copy of the first len bytes of name (len -1: strlen),
    NULL if name is NULL or on out of memory. */
const char *xml_xsd10_dict_lookup(XmlXsd10Dict *dict, const char *name, int len);
/** Port of xmlDictQLookup: the interned "prefix:name" (name if prefix is NULL). */
const char *xml_xsd10_dict_qlookup(XmlXsd10Dict *dict, const char *prefix, const char *name);
/** Port of xmlDictOwns: 1 if str is a string of the dict, 0 if not, -1 on API errors. */
int xml_xsd10_dict_owns(XmlXsd10Dict *dict, const char *str);
/** Port of xmlDictSize: the number of strings. */
int xml_xsd10_dict_size(XmlXsd10Dict *dict);

// Hash tables (libxml2 xmlHashTable) /////////////////////////////

/** A hash table with one to three string keys (libxml2 xmlHashTable), scanned in insertion order. */
struct XmlXsd10Hash;
typedef struct XmlXsd10Hash XmlXsd10Hash;

/** libxml2 xmlHashDeallocator. */
typedef void (*XmlXsd10HashDeallocator)(void *payload, const char *name);
/** libxml2 xmlHashScanner. */
typedef void (*XmlXsd10HashScanner)(void *payload, void *data, const char *name);
/** libxml2 xmlHashScannerFull. */
typedef void (*XmlXsd10HashScannerFull)(void *payload, void *data, const char *name, const char *name2, const char *name3);

/** Port of xmlHashCreate (size is a hint). */
XmlXsd10Hash *xml_xsd10_hash_create(int size);
/** Port of xmlHashCreateDict: the dict is not used, keys are always copied. */
XmlXsd10Hash *xml_xsd10_hash_create_dict(int size, XmlXsd10Dict *dict);
/** Port of xmlHashFree: dealloc (may be NULL) is called for every payload. */
void xml_xsd10_hash_free(XmlXsd10Hash *hash, XmlXsd10HashDeallocator dealloc);
/** Port of xmlHashAddEntry: 0 if added, -1 if the key exists or on errors. */
int xml_xsd10_hash_add_entry(XmlXsd10Hash *hash, const char *name, void *payload);
/** Port of xmlHashAddEntry2. */
int xml_xsd10_hash_add_entry2(XmlXsd10Hash *hash, const char *name, const char *name2, void *payload);
/** Port of xmlHashAddEntry3. */
int xml_xsd10_hash_add_entry3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3, void *payload);
/** Port of xmlHashUpdateEntry: add or replace (dealloc is called on the replaced payload). 0 or -1. */
int xml_xsd10_hash_update_entry(XmlXsd10Hash *hash, const char *name, void *payload, XmlXsd10HashDeallocator dealloc);
/** Port of xmlHashLookup. */
void *xml_xsd10_hash_lookup(XmlXsd10Hash *hash, const char *name);
/** Port of xmlHashLookup2. */
void *xml_xsd10_hash_lookup2(XmlXsd10Hash *hash, const char *name, const char *name2);
/** Port of xmlHashLookup3. */
void *xml_xsd10_hash_lookup3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3);
/** Port of xmlHashRemoveEntry: 0 if removed, -1 if not found. */
int xml_xsd10_hash_remove_entry(XmlXsd10Hash *hash, const char *name, XmlXsd10HashDeallocator dealloc);
/** Port of xmlHashRemoveEntry2. */
int xml_xsd10_hash_remove_entry2(XmlXsd10Hash *hash, const char *name, const char *name2, XmlXsd10HashDeallocator dealloc);
/** Port of xmlHashRemoveEntry3. */
int xml_xsd10_hash_remove_entry3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3, XmlXsd10HashDeallocator dealloc);
/** Port of xmlHashSize: the number of entries, -1 if hash is NULL. */
int xml_xsd10_hash_size(XmlXsd10Hash *hash);
/** Port of xmlHashScan, in insertion order. Removing the scanned entry in the callback is allowed. */
void xml_xsd10_hash_scan(XmlXsd10Hash *hash, XmlXsd10HashScanner scan, void *data);
/** Port of xmlHashScanFull, in insertion order. */
void xml_xsd10_hash_scan_full(XmlXsd10Hash *hash, XmlXsd10HashScannerFull scan, void *data);

// String and tree helpers (libxml2 xmlstring.c, tree.c) //////////

/** xmlStrEqual: 1 if equal (two NULL are equal), 0 otherwise. */
int xml_xsd10_str_equal(const char *a, const char *b);
/** xmlStrdup: NULL for NULL. */
char *xml_xsd10_strdup(const char *s);
/** xmlStrndup: the first len bytes (len < 0: NULL). */
char *xml_xsd10_strndup(const char *s, int len);
/** xmlCharStrdup. */
char *xml_xsd10_char_strdup(const char *s);
/** xmlStrlen: 0 for NULL. */
int xml_xsd10_strlen(const char *s);
/** xmlStrcat: append add to the malloced cur (which may be NULL), returns the new string. */
char *xml_xsd10_strcat(char *cur, const char *add);
/** xmlStrncat: append len bytes of add. */
char *xml_xsd10_strncat(char *cur, const char *add, int len);
/** xmlStrncatNew: a new string str1 + len bytes of str2 (len < 0: strlen). */
char *xml_xsd10_strncat_new(const char *str1, const char *str2, int len);
/** xmlEscapeFormatString: double every '%' of the malloced *msg (which may be replaced), returns *msg. */
char *xml_xsd10_escape_format_string(char **msg);
/** xmlGrowCapacity: the new capacity (grow by 50%, min if capacity <= 0), -1 if too large. */
int xml_xsd10_grow_capacity(int capacity, size_t elemSize, int min, int max);

/** xmlValidateNCName: 0 if value is an NCName, a positive number if not, -1 on API errors. */
int xml_xsd10_tree_validate_ncname(const char *value, int space);
/** xmlValidateQName. */
int xml_xsd10_tree_validate_qname(const char *value, int space);
/** xmlSplitQName2: NULL if no prefix, otherwise the malloced local name and *prefix the malloced prefix. */
char *xml_xsd10_tree_split_qname2(const char *name, char **prefix);
/** xmlSplitQName3: NULL if no prefix, otherwise a pointer to the local name in name and *len the prefix length. */
const char *xml_xsd10_tree_split_qname3(const char *name, int *len);
/** xmlSearchNs: the in scope declaration of prefix (NULL: the default namespace) from node (an element
    or attribute or text, the search starts at the element), "xml" is predefined. NULL if none. */
XmlNs *xml_xsd10_tree_search_ns(XMQNode *node, const char *prefix);
/** xmlSearchNsByHref. */
XmlNs *xml_xsd10_tree_search_ns_by_href(XMQNode *node, const char *href);
/** xmlGetNsList: a malloced NULL terminated array of the namespaces in scope (the closest first), NULL if none. */
XmlNs **xml_xsd10_tree_get_ns_list(XMQNode *node);
/** xmlGetNoNsProp: the malloced value of the attribute without namespace, NULL if absent. */
char *xml_xsd10_tree_get_no_ns_prop(XMQNode *node, const char *name);
/** xmlHasProp: the attribute (any namespace) as a node, NULL if absent. */
XMQNode *xml_xsd10_tree_has_prop(XMQNode *node, const char *name);
/** xmlNodeGetContent: the malloced text of an element, attribute or text node (string value). */
char *xml_xsd10_tree_node_get_content(XMQNode *node);
/** xmlNodeGetBase: the malloced base uri of node (xml:base and the url of its dom), NULL if none. */
char *xml_xsd10_tree_node_get_base(XmlDom *doc, XMQNode *node);
/** xmlNodeGetSpacePreserve: 1 xml:space="preserve", 0 "default", -1 not specified. */
int xml_xsd10_tree_node_get_space_preserve(XMQNode *node);
/** xmlGetLineNo (the native dom has no line truncation): -1 if unknown. */
long xml_xsd10_tree_get_line_no(XMQNode *node);

// Error raising (libxml2 error.c) ////////////////////////////////

/** Port of xmlVRaiseError (without the global handlers): 0 or -1 on out of memory. */
int xml_xsd10_v_raise_error(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data,
                            void *ctx, XMQNode *node, int domain, int code, XmlXsd10ErrorLevel level,
                            const char *file, int line, const char *str1, const char *str2, const char *str3,
                            int int1, int col, const char *msg, va_list ap);
/** Port of xmlRaiseMemoryError. */
void xml_xsd10_raise_memory_error(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data, int domain);
/** Install the regexp hooks of xml_xsd10_types (xml_regexp_compile, xml_regexp_exec, xml_regexp_free). */
void xml_xsd10_install_regexp_hooks(void);

// Loading schema documents ///////////////////////////////////////

/** Replaces xmlCtxtReadFile in xmlSchemaAddSchemaDoc: load url through the loader of pctxt.
    Returns NULL if it failed, then *notLocated (if not NULL) is 1 when the resource could
    not be found/read (libxml2: the last error is in the XML_FROM_IO domain) and 0 when it
    did not parse. The problems are reported like libxml2's xml parser reports them: to the
    structured callback of pctxt if set, otherwise on stderr. The dom gets url as its url. */
XmlDom *xml_xsd10_p_read_file(XmlXsd10ParserCtxt *pctxt, const char *url, int *notLocated);
/** Replaces xmlCtxtReadMemory: parse the buffer with the native parser (NOENT), the dom gets
    the url "in_memory_buffer" like libxml2 sets it. */
XmlDom *xml_xsd10_p_read_memory(XmlXsd10ParserCtxt *pctxt, const char *buffer, int size, int *notLocated);

// Macros of xmlschemas.c /////////////////////////////////////////

#define XML_XSD10_SCHEMAS_NO_NAMESPACE "##"

/* Come casting macros. */
#define XSD10_ACTXT_CAST (XmlXsd10AbstractCtxt *)
#define XSD10_PCTXT_CAST (XmlXsd10ParserCtxt *)
#define XSD10_VCTXT_CAST (XmlXsd10ValidCtxt *)
#define XSD10_WXS_BASIC_CAST (XmlXsd10BasicItem *)
#define XSD10_WXS_TREE_CAST (XmlXsd10TreeItem *)
#define XSD10_WXS_PTC_CAST (XmlXsd10Particle *)
#define XSD10_WXS_TYPE_CAST (XmlXsd10Type *)
#define XSD10_WXS_ELEM_CAST (XmlXsd10Element *)
#define XSD10_WXS_ATTR_GROUP_CAST (XmlXsd10AttributeGroup *)
#define XSD10_WXS_ATTR_CAST (XmlXsd10Attribute *)
#define XSD10_WXS_ATTR_USE_CAST (XmlXsd10AttributeUse *)
#define XSD10_WXS_ATTR_PROHIB_CAST (XmlXsd10AttributeUseProhib *)
#define XSD10_WXS_MODEL_GROUPDEF_CAST (XmlXsd10ModelGroupDef *)
#define XSD10_WXS_MODEL_GROUP_CAST (XmlXsd10ModelGroup *)
#define XSD10_WXS_IDC_CAST (XmlXsd10IDC *)
#define XSD10_WXS_QNAME_CAST (XmlXsd10QNameRef *)
#define XSD10_WXS_LIST_CAST (XmlXsd10ItemList *)

/* Macros to query common properties of components. */
#define XSD10_WXS_ITEM_NODE(i) xml_xsd10_get_component_node(XSD10_WXS_BASIC_CAST (i))

#define XSD10_WXS_ITEM_TYPE_NAME(i) xml_xsd10_get_component_type_str(XSD10_WXS_BASIC_CAST (i))
/* Macros for element declarations. */
#define XSD10_WXS_ELEM_TYPEDEF(e) (e)->subtypes

#define XSD10_WXS_SUBST_HEAD(item) (item)->refDecl
/* Macros for attribute declarations. */
#define XSD10_WXS_ATTR_TYPEDEF(a) (a)->subtypes
/* Macros for attribute uses. */
#define XSD10_WXS_ATTRUSE_DECL(au) (XSD10_WXS_ATTR_USE_CAST (au))->attrDecl

#define XSD10_WXS_ATTRUSE_TYPEDEF(au) XSD10_WXS_ATTR_TYPEDEF(XSD10_WXS_ATTRUSE_DECL( XSD10_WXS_ATTR_USE_CAST au))

#define XSD10_WXS_ATTRUSE_DECL_NAME(au) (XSD10_WXS_ATTRUSE_DECL(au))->name

#define XSD10_WXS_ATTRUSE_DECL_TNS(au) (XSD10_WXS_ATTRUSE_DECL(au))->targetNamespace
/* Macros for attribute groups. */
#define XSD10_WXS_ATTR_GROUP_HAS_REFS(ag) ((XSD10_WXS_ATTR_GROUP_CAST (ag))->flags & XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS)
#define XSD10_WXS_ATTR_GROUP_EXPANDED(ag) ((XSD10_WXS_ATTR_GROUP_CAST (ag))->flags & XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED)
/* Macros for particles. */
#define XSD10_WXS_PARTICLE(p) XSD10_WXS_PTC_CAST (p)

#define XSD10_WXS_PARTICLE_TERM(p) (XSD10_WXS_PARTICLE(p))->children

#define XSD10_WXS_PARTICLE_TERM_AS_ELEM(p) (XSD10_WXS_ELEM_CAST XSD10_WXS_PARTICLE_TERM(p))

#define XSD10_WXS_PARTICLE_MODEL(p) XSD10_WXS_MODEL_GROUP_CAST XSD10_WXS_PARTICLE(p)->children
/* Macros for model groups definitions. */
#define XSD10_WXS_MODELGROUPDEF_MODEL(mgd) (XSD10_WXS_MODEL_GROUP_CAST (mgd))->children
/* Macros for model groups. */
#define XSD10_WXS_IS_MODEL_GROUP(i) \
    (((i)->type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) || \
     ((i)->type == XML_XSD10_SCHEMA_TYPE_CHOICE) || \
     ((i)->type == XML_XSD10_SCHEMA_TYPE_ALL))

#define XSD10_WXS_MODELGROUP_PARTICLE(mg) XSD10_WXS_PTC_CAST (mg)->children
/* Macros for schema buckets. */
#define XSD10_WXS_IS_BUCKET_INCREDEF(t) (((t) == XML_XSD10_SCHEMA_SCHEMA_INCLUDE) || \
    ((t) == XML_XSD10_SCHEMA_SCHEMA_REDEFINE))

#define XSD10_WXS_IS_BUCKET_IMPMAIN(t) (((t) == XML_XSD10_SCHEMA_SCHEMA_MAIN) || \
    ((t) == XML_XSD10_SCHEMA_SCHEMA_IMPORT))

#define XSD10_WXS_IMPBUCKET(b) ((XmlXsd10Import *) (b))

#define XSD10_WXS_INCBUCKET(b) ((XmlXsd10Include *) (b))
/* Macros for complex/simple types. */
#define XSD10_WXS_IS_ANYTYPE(i) \
     (( (i)->type == XML_XSD10_SCHEMA_TYPE_BASIC) && \
      ( (XSD10_WXS_TYPE_CAST (i))->builtInType == XML_XSD10_SCHEMAS_ANYTYPE))

#define XSD10_WXS_IS_COMPLEX(i) \
    (((i)->type == XML_XSD10_SCHEMA_TYPE_COMPLEX) || \
     ((i)->builtInType == XML_XSD10_SCHEMAS_ANYTYPE))

#define XSD10_WXS_IS_SIMPLE(item) \
    ((item->type == XML_XSD10_SCHEMA_TYPE_SIMPLE) || \
     ((item->type == XML_XSD10_SCHEMA_TYPE_BASIC) && \
      (item->builtInType != XML_XSD10_SCHEMAS_ANYTYPE)))

#define XSD10_WXS_IS_ANY_SIMPLE_TYPE(i) \
    (((i)->type == XML_XSD10_SCHEMA_TYPE_BASIC) && \
      ((i)->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))

#define XSD10_WXS_IS_RESTRICTION(t) \
    ((t)->flags & XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION)

#define XSD10_WXS_IS_EXTENSION(t) \
    ((t)->flags & XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION)

#define XSD10_WXS_IS_TYPE_NOT_FIXED(i) \
    (((i)->type != XML_XSD10_SCHEMA_TYPE_BASIC) && \
     (((i)->flags & XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED) == 0))

#define XSD10_WXS_IS_TYPE_NOT_FIXED_1(item) \
    (((item)->type != XML_XSD10_SCHEMA_TYPE_BASIC) && \
     (((item)->flags & XML_XSD10_SCHEMAS_TYPE_FIXUP_1) == 0))

#define XSD10_WXS_TYPE_IS_GLOBAL(t) ((t)->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL)

#define XSD10_WXS_TYPE_IS_LOCAL(t) (((t)->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) == 0)
/* Macros for exclusively for complex types. */
#define XSD10_WXS_HAS_COMPLEX_CONTENT(item) \
    ((item->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) || \
     (item->contentType == XML_XSD10_SCHEMA_CONTENT_EMPTY) || \
     (item->contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS))

#define XSD10_WXS_HAS_SIMPLE_CONTENT(item) \
    ((item->contentType == XML_XSD10_SCHEMA_CONTENT_SIMPLE) || \
     (item->contentType == XML_XSD10_SCHEMA_CONTENT_BASIC))

#define XSD10_WXS_HAS_MIXED_CONTENT(item) \
    (item->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED)

#define XSD10_WXS_EMPTIABLE(t) \
    (xml_xsd10_is_particle_emptiable(XSD10_WXS_PTC_CAST (t)->subtypes))

#define XSD10_WXS_TYPE_CONTENTTYPE(t) (t)->subtypes

#define XSD10_WXS_TYPE_PARTICLE(t) XSD10_WXS_PTC_CAST (t)->subtypes

#define XSD10_WXS_TYPE_PARTICLE_TERM(t) XSD10_WXS_PARTICLE_TERM(XSD10_WXS_TYPE_PARTICLE(t))
/* Macros for exclusively for simple types. */
#define XSD10_WXS_LIST_ITEMTYPE(t) (t)->subtypes

#define XSD10_WXS_IS_ATOMIC(t) (t->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC)

#define XSD10_WXS_IS_LIST(t) (t->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST)

#define XSD10_WXS_IS_UNION(t) (t->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION)
/* Misc parser context macros. */
#define XSD10_WXS_CONSTRUCTOR(ctx) (ctx)->constructor

#define XSD10_WXS_HAS_BUCKETS(ctx) \
( (XSD10_WXS_CONSTRUCTOR((ctx))->buckets != NULL) && \
(XSD10_WXS_CONSTRUCTOR((ctx))->buckets->nbItems > 0) )

#define XSD10_WXS_SUBST_GROUPS(ctx) XSD10_WXS_CONSTRUCTOR((ctx))->substGroups

#define XSD10_WXS_BUCKET(ctx) XSD10_WXS_CONSTRUCTOR((ctx))->bucket

#define XSD10_WXS_SCHEMA(ctx) (ctx)->schema

#define XSD10_WXS_ADD_LOCAL(ctx, item) \
    do { \
        if ((item != NULL) && \
            (xml_xsd10_add_item_size(&(XSD10_WXS_BUCKET(ctx)->locals), 10, \
                                  item) < 0)) { \
            xml_xsd10_p_err_memory(ctx); \
            free(item); \
            item = NULL; \
        } \
    } while (0)

#define XSD10_WXS_ADD_GLOBAL(ctx, item) \
    do { \
        if ((item != NULL) && \
            (xml_xsd10_add_item_size(&(XSD10_WXS_BUCKET(ctx)->globals), 5, \
                                  item) < 0)) { \
            xml_xsd10_p_err_memory(ctx); \
            free(item); \
            item = NULL; \
        } \
    } while (0)

#define XSD10_WXS_ADD_PENDING(ctx, item) \
    do { \
        if ((item != NULL) && \
            (xml_xsd10_add_item_size(&((ctx)->constructor->pending), 10, \
                                  item) < 0)) { \
            xml_xsd10_p_err_memory(ctx); \
        } \
    } while (0)
/* xmlSchemaItemList macros. */
#define XSD10_WXS_ILIST_IS_EMPTY(l) ((l == NULL) || ((l)->nbItems == 0))
/* Misc macros. The node is an XMQNode (an element). */
#define XSD10_IS_SCHEMA(node, type) \
   ((node != NULL) && (xml_node_kind(node) == XML_DOM_ELEMENT) && (xml_node_ns(node) != NULL) && \
    (xml_xsd10_str_equal(xml_node_name(node), (const char *) type)) && \
    (xml_xsd10_str_equal(xml_ns_href(xml_node_ns(node)), xml_xsd10_ns)))

#define XSD10_FREE_AND_NULL(str) if ((str) != NULL) { free((char *) (str)); str = NULL; }

/* Since we put the default/fixed values into the dict, we can
   use pointer comparison for those values.
   REMOVED: (xmlStrEqual((v1), (v2))) */
#define XSD10_WXS_ARE_DEFAULT_STR_EQUAL(v1, v2) ((v1) == (v2))

#define XSD10_INODE_NILLED(item) (item->flags & XML_XSD10_SCHEMA_ELEM_INFO_NILLED)

#define XSD10_CAN_PARSE_SCHEMA(b) (((b)->doc != NULL) && ((b)->parsed == 0))

#define XSD10_HFAILURE if (res == -1) goto exit_failure;

#define XSD10_HERROR if (res != 0) goto exit_error;

#define XSD10_HSTOP(ctx) if ((ctx)->stop) goto exit;
/* Some flags used for various schema constraints. */
#define XSD10_SUBSET_RESTRICTION  1<<0
#define XSD10_SUBSET_EXTENSION    1<<1
#define XSD10_SUBSET_SUBSTITUTION 1<<2
#define XSD10_SUBSET_LIST         1<<3
#define XSD10_SUBSET_UNION        1<<4

#define xml_xsd10_get_q_name_ref_name(r) (XSD10_WXS_QNAME_CAST (r))->name
#define xml_xsd10_get_q_name_ref_target_ns(r) (XSD10_WXS_QNAME_CAST (r))->targetNamespace

#define XSD10_WXS_ERROR_TYPE_ERROR 1
#define XSD10_WXS_ERROR_TYPE_WARNING 2

#define XSD10_VERROR(err, type, msg) \
    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt, err, NULL, type, msg, NULL, NULL);

#define XSD10_VERROR_INT(func, msg) xml_xsd10_internal_err(XSD10_ACTXT_CAST vctxt, func, msg);

#define XSD10_PERROR_INT(func, msg) xml_xsd10_internal_err(XSD10_ACTXT_CAST pctxt, func, msg);
#define XSD10_PERROR_INT2(func, msg) xml_xsd10_internal_err(XSD10_ACTXT_CAST ctxt, func, msg);

#define XSD10_AERROR_INT(func, msg) xml_xsd10_internal_err(actxt, func, msg);

// Structs of xmlschemas.c ////////////////////////////////////////

typedef struct XmlXsd10NodeInfo XmlXsd10NodeInfo;

/** libxml2 xmlSchemaItemList. */
typedef struct XmlXsd10ItemList XmlXsd10ItemList;
struct XmlXsd10ItemList {
    void **items;  /* used for dynamic addition of schemata */
    int nbItems; /* used for dynamic addition of schemata */
    int sizeItems; /* used for dynamic addition of schemata */
};

#define XML_XSD10_SCHEMA_CTXT_PARSER 1
#define XML_XSD10_SCHEMA_CTXT_VALIDATOR 2

/** libxml2 xmlSchemaAbstractCtxt. */
typedef struct XmlXsd10AbstractCtxt XmlXsd10AbstractCtxt;
struct XmlXsd10AbstractCtxt {
    int type; /* E.g. XML_XSD10_SCHEMA_CTXT_VALIDATOR */
    void *dummy; /* Fix alignment issues */
};

typedef struct XmlXsd10Bucket XmlXsd10Bucket;

#define XML_XSD10_SCHEMA_SCHEMA_MAIN 0
#define XML_XSD10_SCHEMA_SCHEMA_IMPORT 1
#define XML_XSD10_SCHEMA_SCHEMA_INCLUDE 2
#define XML_XSD10_SCHEMA_SCHEMA_REDEFINE 3

/** Used to create a graph of schema relationships (libxml2 xmlSchemaSchemaRelation). */
typedef struct XmlXsd10SchemaRelation XmlXsd10SchemaRelation;
struct XmlXsd10SchemaRelation {
    XmlXsd10SchemaRelation *next;
    int type; /* E.g. XML_XSD10_SCHEMA_SCHEMA_IMPORT */
    const char *importNamespace;
    XmlXsd10Bucket *bucket;
};

#define XML_XSD10_SCHEMA_BUCKET_MARKED 1<<0
#define XML_XSD10_SCHEMA_BUCKET_COMPS_ADDED 1<<1

/** libxml2 xmlSchemaBucket. XmlXsd10Bucket, XmlXsd10Import and XmlXsd10Include
    start of structures must be kept similar. ids is new (replaces libxml2 doc->ids). */
struct XmlXsd10Bucket {
    int type;
    int flags;
    const char *schemaLocation;
    const char *origTargetNamespace;
    const char *targetNamespace;
    XmlDom *doc;
    XmlXsd10SchemaRelation *relations;
    int located;
    int parsed;
    int imported;
    int preserveDoc;
    XmlXsd10ItemList *globals; /* Global components. */
    XmlXsd10ItemList *locals; /* Local components. */
    XmlXsd10Ids *ids; /* The xs:ID attributes of doc (freed with the bucket). */
};

/**
 * (extends XmlXsd10Bucket) libxml2 xmlSchemaImport.
 *
 * Reflects a schema. Holds some information
 * about the schema and its toplevel components. Duplicate
 * toplevel components are not checked at this level.
 */
typedef struct XmlXsd10Import XmlXsd10Import;
struct XmlXsd10Import {
    int type; /* Main OR import OR include. */
    int flags;
    const char *schemaLocation; /* The URI of the schema document. */
    /* For chameleon includes, @origTargetNamespace will be NULL */
    const char *origTargetNamespace;
    /*
    * For chameleon includes, @targetNamespace will be the
    * targetNamespace of the including schema.
    */
    const char *targetNamespace;
    XmlDom *doc; /* The schema node-tree. */
    /* @relations will hold any included/imported/redefined schemas. */
    XmlXsd10SchemaRelation *relations;
    int located;
    int parsed;
    int imported;
    int preserveDoc;
    XmlXsd10ItemList *globals;
    XmlXsd10ItemList *locals;
    XmlXsd10Ids *ids;
    /* The imported schema. */
    XmlXsd10Schema *schema;
};

/** (extends XmlXsd10Bucket) libxml2 xmlSchemaInclude. */
typedef struct XmlXsd10Include XmlXsd10Include;
struct XmlXsd10Include {
    int type;
    int flags;
    const char *schemaLocation;
    const char *origTargetNamespace;
    const char *targetNamespace;
    XmlDom *doc;
    XmlXsd10SchemaRelation *relations;
    int located;
    int parsed;
    int imported;
    int preserveDoc;
    XmlXsd10ItemList *globals; /* Global components. */
    XmlXsd10ItemList *locals; /* Local components. */
    XmlXsd10Ids *ids;

    /* The owning main or import schema bucket. */
    XmlXsd10Import *ownerImport;
};

/** The abstract base type for schema components (libxml2 xmlSchemaBasicItem). */
typedef struct XmlXsd10BasicItem XmlXsd10BasicItem;
struct XmlXsd10BasicItem {
    XmlXsd10TypeType type;
    void *dummy; /* Fix alignment issues */
};

/** The abstract base type for annotated schema components (libxml2 xmlSchemaAnnotItem).
    (Extends XmlXsd10BasicItem) */
typedef struct XmlXsd10AnnotItem XmlXsd10AnnotItem;
struct XmlXsd10AnnotItem {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
};

/* XmlXsd10TreeItem (libxml2 xmlSchemaTreeItem), XmlXsd10Particle and XmlXsd10ModelGroup are in xml_xsd10_types.h. */

#define XML_XSD10_SCHEMA_ATTR_USE_FIXED 1<<0
/** The abstract base type for tree-like structured schema components (libxml2 xmlSchemaAttributeUse).
    (Extends XmlXsd10TreeItem) */
typedef struct XmlXsd10AttributeUse XmlXsd10AttributeUse;
struct XmlXsd10AttributeUse {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
    XmlXsd10AttributeUse *next; /* The next attr. use. */
    /*
    * The attr. decl. OR a QName-ref. to an attr. decl. OR
    * a QName-ref. to an attribute group definition.
    */
    XmlXsd10Attribute *attrDecl;

    int flags;
    XMQNode *node;
    int occurs; /* required, optional */
    const char * defValue;
    XmlXsd10Val *defVal;
};

/** A helper component to reflect attribute prohibitions (libxml2 xmlSchemaAttributeUseProhib).
    (Extends XmlXsd10BasicItem) */
typedef struct XmlXsd10AttributeUseProhib XmlXsd10AttributeUseProhib;
struct XmlXsd10AttributeUseProhib {
    XmlXsd10TypeType type; /* == XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB */
    XMQNode *node;
    const char *name;
    const char *targetNamespace;
    int isRef;
};

/** libxml2 xmlSchemaRedef. */
typedef struct XmlXsd10Redef XmlXsd10Redef;
struct XmlXsd10Redef {
    XmlXsd10Redef *next;
    XmlXsd10BasicItem *item; /* The redefining component. */
    XmlXsd10BasicItem *reference; /* The referencing component. */
    XmlXsd10BasicItem *target; /* The to-be-redefined component. */
    const char *refName; /* The name of the to-be-redefined component. */
    const char *refTargetNs; /* The target namespace of the
                                   to-be-redefined comp. */
    XmlXsd10Bucket *targetBucket; /* The redefined schema. */
};

/** libxml2 xmlSchemaConstructionCtxt. */
typedef struct XmlXsd10ConstructionCtxt XmlXsd10ConstructionCtxt;
struct XmlXsd10ConstructionCtxt {
    XmlXsd10Schema *mainSchema; /* The main schema. */
    XmlXsd10Bucket *mainBucket; /* The main schema bucket */
    XmlXsd10Dict *dict;
    XmlXsd10ItemList *buckets; /* List of schema buckets. */
    /* XmlXsd10ItemList *relations; */ /* List of schema relations. */
    XmlXsd10Bucket *bucket; /* The current schema bucket */
    XmlXsd10ItemList *pending; /* All Components of all schemas that
                                     need to be fixed. */
    XmlXsd10Hash *substGroups;
    XmlXsd10Redef *redefs;
    XmlXsd10Redef *lastRedef;
};

#define XML_XSD10_SCHEMAS_PARSE_ERROR 1
/* SCHEMAS_PARSE_OPTIONS is XML_PARSE_NOENT: the loaders substitute the entities. */

/** libxml2 xmlSchemaParserCtxt. */
struct XmlXsd10ParserCtxt {
    int type;
    void *errCtxt;             /* user specific error context */
    XmlXsd10ValidityErrorFunc error;   /* the callback in case of errors */
    XmlXsd10ValidityWarningFunc warning;       /* the callback in case of warning */
    int err;
    int nberrors;
    XmlXsd10StructuredErrorFunc serror;

    XmlXsd10ConstructionCtxt *constructor;
    int ownsConstructor; /* TODO: Move this to parser *flags*. */

    /* XmlXsd10Schema *topschema;	*/
    /* XmlXsd10Hash *namespaces;  */

    XmlXsd10Schema *schema;        /* The main schema in use */
    int counter;

    const char *URL;
    XmlDom *doc;
    int preserve;		/* Whether the doc should be freed  */

    const char *buffer;
    int size;

    /*
     * Used to build complex element content models
     */
    XmlAutomata *am;
    XmlAutomataState *start;
    XmlAutomataState *end;
    XmlAutomataState *state;

    XmlXsd10Dict *dict;		/* dictionary for interned string names */
    XmlXsd10Type *ctxtType; /* The current context simple/complex type */
    int options;
    XmlXsd10ValidCtxt *vctxt;
    int isS4S;
    int isRedefine;
    int xsiAssemble;
    int stop; /* If the parser should stop; i.e. a critical error. */
    const char *targetNamespace;
    XmlXsd10Bucket *redefined; /* The schema to be redefined. */

    XmlXsd10Redef *redef; /* Used for redefinitions. */
    int redefCounter; /* Used for redefinitions. */
    XmlXsd10ItemList *attrProhibs;

    XmlXsd10Loader resourceLoader; /* NULL: xml_xsd10_default_loader */
    void *resourceCtxt;
};

/** A component reference item (not a schema component) (libxml2 xmlSchemaQNameRef).
    (Extends XmlXsd10BasicItem) */
typedef struct XmlXsd10QNameRef XmlXsd10QNameRef;
struct XmlXsd10QNameRef {
    XmlXsd10TypeType type;
    XmlXsd10BasicItem *item; /* The resolved referenced item. */
    XmlXsd10TypeType itemType;
    const char *name;
    const char *targetNamespace;
    XMQNode *node;
};

#define XML_XSD10_SCHEMA_MODEL_GROUP_DEF_MARKED 1<<0
#define XML_XSD10_SCHEMA_MODEL_GROUP_DEF_REDEFINED 1<<1
/** A model group definition component (libxml2 xmlSchemaModelGroupDef).
    (Extends XmlXsd10TreeItem) */
typedef struct XmlXsd10ModelGroupDef XmlXsd10ModelGroupDef;
struct XmlXsd10ModelGroupDef {
    XmlXsd10TypeType type; /* XML_XSD10_SCHEMA_TYPE_GROUP */
    XmlXsd10Annot *annot;
    XmlXsd10TreeItem *next; /* not used */
    XmlXsd10TreeItem *children; /* the "model group" */
    const char *name;
    const char *targetNamespace;
    XMQNode *node;
    int flags;
};

typedef struct XmlXsd10IDC XmlXsd10IDC;

/** The identity-constraint "field" and "selector" item, holding the
    XPath expression (libxml2 xmlSchemaIDCSelect). */
typedef struct XmlXsd10IDCSelect XmlXsd10IDCSelect;
struct XmlXsd10IDCSelect {
    XmlXsd10IDCSelect *next;
    XmlXsd10IDC *idc;
    int index; /* an index position if significant for IDC key-sequences */
    const char *xpath; /* the XPath expression */
    void *xpathComp; /* the compiled XPath expression (an XmlPattern) */
};

/** The identity-constraint definition component (libxml2 xmlSchemaIDC).
    (Extends XmlXsd10AnnotItem) */
struct XmlXsd10IDC {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
    XmlXsd10IDC *next;
    XMQNode *node;
    const char *name;
    const char *targetNamespace;
    XmlXsd10IDCSelect *selector;
    XmlXsd10IDCSelect *fields;
    int nbFields;
    XmlXsd10QNameRef *ref;
};

/** The augmented IDC information used for validation (libxml2 xmlSchemaIDCAug). */
typedef struct XmlXsd10IDCAug XmlXsd10IDCAug;
struct XmlXsd10IDCAug {
    XmlXsd10IDCAug *next; /* next in a list */
    XmlXsd10IDC *def; /* the IDC definition */
    int keyrefDepth; /* the lowest tree level to which IDC
                        tables need to be bubbled upwards */
};

/** The key sequence of a node table item (libxml2 xmlSchemaPSVIIDCKey). */
typedef struct XmlXsd10PSVIIDCKey XmlXsd10PSVIIDCKey;
struct XmlXsd10PSVIIDCKey {
    XmlXsd10Type *type;
    XmlXsd10Val *val;
};

/** The node table item of a node table (libxml2 xmlSchemaPSVIIDCNode). */
typedef struct XmlXsd10PSVIIDCNode XmlXsd10PSVIIDCNode;
struct XmlXsd10PSVIIDCNode {
    XMQNode *node;
    XmlXsd10PSVIIDCKey **keys;
    int nodeLine;
    int nodeQNameID;

};

/** The identity-constraint binding item of the [identity-constraint table]
    (libxml2 xmlSchemaPSVIIDCBinding). */
typedef struct XmlXsd10PSVIIDCBinding XmlXsd10PSVIIDCBinding;
struct XmlXsd10PSVIIDCBinding {
    XmlXsd10PSVIIDCBinding *next; /* next binding of a specific node */
    XmlXsd10IDC *definition; /* the IDC definition */
    XmlXsd10PSVIIDCNode **nodeTable; /* array of key-sequences */
    int nbNodes; /* number of entries in the node table */
    int sizeNodes; /* size of the node table */
    XmlXsd10ItemList *dupls;
};

#define XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR 1
#define XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD 2

#define XSD10_XPATH_STATE_OBJ_MATCHES -2
#define XSD10_XPATH_STATE_OBJ_BLOCKED -3

typedef struct XmlXsd10IDCMatcher XmlXsd10IDCMatcher;

/** The state object used to evaluate XPath expressions (libxml2 xmlSchemaIDCStateObj). */
typedef struct XmlXsd10IDCStateObj XmlXsd10IDCStateObj;
struct XmlXsd10IDCStateObj {
    int type;
    XmlXsd10IDCStateObj *next; /* next if in a list */
    int depth; /* depth of creation */
    int *history; /* list of (depth, state-id) tuples */
    int nbHistory;
    int sizeHistory;
    XmlXsd10IDCMatcher *matcher; /* the correspondent field/selector
                                       matcher */
    XmlXsd10IDCSelect *sel;
    void *xpathCtxt; /* an XmlStreamCtxt */
};

#define XSD10_IDC_MATCHER 0

/** Used to evaluate IDC selectors (and fields) (libxml2 xmlSchemaIDCMatcher). */
struct XmlXsd10IDCMatcher {
    int type;
    int depth; /* the tree depth at creation time */
    XmlXsd10IDCMatcher *next; /* next in the list */
    XmlXsd10IDCMatcher *nextCached; /* next in the cache list */
    XmlXsd10IDCAug *aidc; /* the augmented IDC item */
    int idcType;
    XmlXsd10PSVIIDCKey ***keySeqs; /* the key-sequences of the target
                                         elements */
    int sizeKeySeqs;
    XmlXsd10ItemList *targets; /* list of target-node
                                     (XmlXsd10PSVIIDCNode *) entries */
    XmlXsd10Hash *htab;
};

/* Element info flags. */
#define XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES  (1<<0)
#define XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES (1<<1)
#define XML_XSD10_SCHEMA_ELEM_INFO_NILLED            (1<<2)
#define XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE        (1<<3)

#define XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED      (1<<4)
#define XML_XSD10_SCHEMA_ELEM_INFO_EMPTY             (1<<5)
#define XML_XSD10_SCHEMA_ELEM_INFO_HAS_CONTENT       (1<<6)

#define XML_XSD10_SCHEMA_ELEM_INFO_HAS_ELEM_CONTENT  (1<<7)
#define XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT   (1<<8)
#define XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED  (1<<9)
#define XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE      (1<<10)

/** Holds information of an element node (libxml2 xmlSchemaNodeInfo). */
struct XmlXsd10NodeInfo {
    int nodeType;
    XMQNode *node;
    int nodeLine;
    const char *localName;
    const char *nsName;
    const char *value;
    XmlXsd10Val *val; /* the pre-computed value if any */
    XmlXsd10Type *typeDef; /* the complex/simple type definition if any */

    int flags; /* combination of node info flags */

    int valNeeded;
    int normVal;

    XmlXsd10Element *decl; /* the element/attribute declaration */
    int depth;
    XmlXsd10PSVIIDCBinding *idcTable; /* the table of PSVI IDC bindings
                                            for the scope element*/
    XmlXsd10IDCMatcher *idcMatchers; /* the IDC matchers for the scope
                                           element */
    XmlRegExecCtxt *regexCtxt;

    const char **nsBindings; /* Namespace bindings on this element */
    int nbNsBindings;
    int sizeNsBindings;

    int hasKeyrefs;
    int appliedXPath; /* Indicates that an XPath has been applied. */
};

#define XML_XSD10_SCHEMAS_ATTR_UNKNOWN 1
#define XML_XSD10_SCHEMAS_ATTR_ASSESSED 2
#define XML_XSD10_SCHEMAS_ATTR_PROHIBITED 3
#define XML_XSD10_SCHEMAS_ATTR_ERR_MISSING 4
#define XML_XSD10_SCHEMAS_ATTR_INVALID_VALUE 5
#define XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE 6
#define XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE 7
#define XML_XSD10_SCHEMAS_ATTR_DEFAULT 8
#define XML_XSD10_SCHEMAS_ATTR_VALIDATE_VALUE 9
#define XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL 10
#define XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_USE 11
#define XML_XSD10_SCHEMAS_ATTR_HAS_ATTR_DECL 12
#define XML_XSD10_SCHEMAS_ATTR_WILD_SKIP 13
#define XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL 14
#define XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID 15
#define XML_XSD10_SCHEMAS_ATTR_ERR_WILD_AND_USE_ID 16
#define XML_XSD10_SCHEMAS_ATTR_META 17
/* @metaType values of XmlXsd10AttrInfo. */
#define XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE 1
#define XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL 2
#define XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC 3
#define XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC 4
#define XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS 5

/** libxml2 xmlSchemaAttrInfo (starts like XmlXsd10NodeInfo). */
typedef struct XmlXsd10AttrInfo XmlXsd10AttrInfo;
struct XmlXsd10AttrInfo {
    int nodeType;
    XMQNode *node;
    int nodeLine;
    const char *localName;
    const char *nsName;
    const char *value;
    XmlXsd10Val *val; /* the pre-computed value if any */
    XmlXsd10Type *typeDef; /* the complex/simple type definition if any */
    int flags; /* combination of node info flags */

    XmlXsd10Attribute *decl; /* the attribute declaration */
    XmlXsd10AttributeUse *use;  /* the attribute use */
    int state;
    int metaType;
    const char *vcValue; /* the value constraint value */
    XmlXsd10NodeInfo *parent;
};

#define XML_XSD10_SCHEMA_VALID_CTXT_FLAG_STREAM 1
/** A Schemas validation context (libxml2 xmlSchemaValidCtxt). The streaming fields
    (input, enc, sax, parserCtxt, reader, locFunc, locCtxt) are not ported, ids is new. */
struct XmlXsd10ValidCtxt {
    int type;
    void *errCtxt;             /* user specific data block */
    XmlXsd10ValidityErrorFunc error;   /* the callback in case of errors */
    XmlXsd10ValidityWarningFunc warning; /* the callback in case of warning */
    XmlXsd10StructuredErrorFunc serror;

    XmlXsd10Schema *schema;        /* The schema in use */
    XmlDom *doc;
    void *user_data; /* TODO: What is this for? */
    char *filename;

    int err;
    int nberrors;

    XMQNode *node;
    XMQNode *cur;
    /* XmlXsd10Type *type; */

    XmlRegExecCtxt *regexp;
    XmlXsd10Val *value;

    int valueWS;
    int options;
    XMQNode *validationRoot;
    XmlXsd10ParserCtxt *pctxt;
    int xsiAssemble;

    int depth;
    XmlXsd10NodeInfo **elemInfos; /* array of element information */
    int sizeElemInfos;
    XmlXsd10NodeInfo *inode; /* the current element information */

    XmlXsd10IDCAug *aidcs; /* a list of augmented IDC information */

    XmlXsd10IDCStateObj *xpathStates; /* first active state object. */
    XmlXsd10IDCStateObj *xpathStatePool; /* first stored state object. */
    XmlXsd10IDCMatcher *idcMatcherCache; /* Cache for IDC matcher objects. */

    XmlXsd10PSVIIDCNode **idcNodes; /* list of all IDC node-table entries*/
    int nbIdcNodes;
    int sizeIdcNodes;

    XmlXsd10PSVIIDCKey **idcKeys; /* list of all IDC node-table entries */
    int nbIdcKeys;
    int sizeIdcKeys;

    int flags;

    XmlXsd10Dict *dict;

    XmlXsd10AttrInfo **attrInfos;
    int nbAttrInfos;
    int sizeAttrInfos;

    int skipDepth;
    XmlXsd10ItemList *nodeQNames;
    int hasKeyrefs;
    int createIDCNodeTables;
    int psviExposeIDCNodeTables;

    XmlXsd10Ids *ids; /* The ID attributes of the validated document (replaces libxml2 doc->ids). */
};

/** libxml2 xmlSchemaSubstGroup. */
typedef struct XmlXsd10SubstGroup XmlXsd10SubstGroup;
struct XmlXsd10SubstGroup {
    XmlXsd10Element *head;
    XmlXsd10ItemList *members;
};

/** An entry in hash tables to quickly look up keys/uniques (libxml2 xmlIDCHashEntry). */
typedef struct XmlXsd10IDCHashEntry XmlXsd10IDCHashEntry;
struct XmlXsd10IDCHashEntry {
    XmlXsd10IDCHashEntry *next; /* next item with same hash */
    int index;               /* index into associated item list */
};

/** A schema (libxml2 struct _xmlSchema of schemasInternals.h). */
struct XmlXsd10Schema {
    const char *name; /* schema name */
    const char *targetNamespace; /* the target namespace */
    const char *version;
    const char *id; /* Obsolete */
    XmlDom *doc;
    XmlXsd10Annot *annot;
    int flags;

    XmlXsd10Hash *typeDecl;
    XmlXsd10Hash *attrDecl;
    XmlXsd10Hash *attrgrpDecl;
    XmlXsd10Hash *elemDecl;
    XmlXsd10Hash *notaDecl;

    XmlXsd10Hash *schemasImports;

    void *_private;        /* unused by the library for users or bindings */
    XmlXsd10Hash *groupDecl;
    XmlXsd10Dict        *dict;
    void *includes;     /* the includes, this is opaque for now */
    int preserve;        /* whether to free the document */
    int counter; /* used to give anonymous components unique names */
    XmlXsd10Hash *idcDef; /* All identity-constraint defs. */
    void *volatiles; /* Obsolete */
};

// Functions of xmlschemas.c //////////////////////////////////////
// Every function of xmlschemas.c that is ported, by the part implementing it
// (in libxml2 source order, the libxml2 name in the comment).

// xml_xsd10_schemas (xmlschemas.c lines 1-4565 without the debug dumps, plus the
// context lifecycle 5447-5462, 9618-9750, 12164-12262, the parser setters and
// utilities 20989-21152 and the validation setters 27302-27427).
const char *xml_xsd10_item_type_to_str(XmlXsd10TypeType type); // xmlSchemaItemTypeToStr
const char *xml_xsd10_get_component_type_str(XmlXsd10BasicItem *item); // xmlSchemaGetComponentTypeStr
XMQNode *xml_xsd10_get_component_node(XmlXsd10BasicItem *item); // xmlSchemaGetComponentNode
const char *xml_xsd10_format_q_name(char **buf, const char *namespaceName, const char *localName); // xmlSchemaFormatQName
const char *xml_xsd10_format_q_name_ns(char **buf, XmlNs *ns, const char *localName); // xmlSchemaFormatQNameNs
const char *xml_xsd10_get_component_name(XmlXsd10BasicItem *item); // xmlSchemaGetComponentName
const char *xml_xsd10_get_component_target_ns(XmlXsd10BasicItem *item); // xmlSchemaGetComponentTargetNs
const char *xml_xsd10_get_component_q_name(char **buf, void *item); // xmlSchemaGetComponentQName
const char *xml_xsd10_get_component_designation(char **buf, void *item); // xmlSchemaGetComponentDesignation
const char *xml_xsd10_get_idc_designation(char **buf, XmlXsd10IDC *idc); // xmlSchemaGetIDCDesignation
const char *xml_xsd10_wildcard_pc_to_string(int pc); // xmlSchemaWildcardPCToString
int xml_xsd10_get_canon_value_whtsp_ext_1(XmlXsd10Val *val, XmlXsd10WhitespaceValueType ws, char **retValue, int for_hash); // xmlSchemaGetCanonValueWhtspExt_1
int xml_xsd10_get_canon_value_whtsp_ext(XmlXsd10Val *val, XmlXsd10WhitespaceValueType ws, char **retValue); // xmlSchemaGetCanonValueWhtspExt
int xml_xsd10_get_canon_value_hash(XmlXsd10Val *val, char **retValue); // xmlSchemaGetCanonValueHash
char *xml_xsd10_format_item_for_report(char **buf, const char *itemDes, XmlXsd10BasicItem *item, XMQNode *itemNode); // xmlSchemaFormatItemForReport
const char *xml_xsd10_format_facet_enum_set(XmlXsd10AbstractCtxt *actxt, char **buf, XmlXsd10Type *type); // xmlSchemaFormatFacetEnumSet
void xml_xsd10_p_err_memory(XmlXsd10ParserCtxt *ctxt); // xmlSchemaPErrMemory
void xml_xsd10_p_err_full(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, XmlXsd10ErrorLevel level, const char *file, int line, const char *str1, const char *str2, const char *str3, int col, const char *msg, ...); // xmlSchemaPErrFull
void xml_xsd10_p_err(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, const char *msg, const char *str1, const char *str2); // xmlSchemaPErr
void xml_xsd10_p_err2(XmlXsd10ParserCtxt *ctxt, XMQNode *node, XMQNode *child, int error, const char *msg, const char *str1, const char *str2); // xmlSchemaPErr2
void xml_xsd10_p_err_ext(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, const char *strData1, const char *strData2, const char *strData3, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4, const char *str5); // xmlSchemaPErrExt
void xml_xsd10_v_err_memory(XmlXsd10ValidCtxt *ctxt); // xmlSchemaVErrMemory
void xml_xsd10_v_err_full(XmlXsd10ValidCtxt *ctxt, XMQNode *node, int code, XmlXsd10ErrorLevel level, const char *file, int line, const char *str1, const char *str2, const char *str3, int col, const char *msg, ...); // xmlSchemaVErrFull
void xml_xsd10_err4_line(XmlXsd10AbstractCtxt *ctxt, XmlXsd10ErrorLevel errorLevel, int code, XMQNode *node, int line, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4); // xmlSchemaErr4Line
void xml_xsd10_err3(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2, const char *str3); // xmlSchemaErr3
void xml_xsd10_err4(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4); // xmlSchemaErr4
void xml_xsd10_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2); // xmlSchemaErr
char *xml_xsd10_format_node_for_error(char **msg, XmlXsd10AbstractCtxt *actxt, XMQNode *node); // xmlSchemaFormatNodeForError
void xml_xsd10_internal_err2(XmlXsd10AbstractCtxt *actxt, const char *funcName, const char *message, const char *str1, const char *str2); // xmlSchemaInternalErr2
void xml_xsd10_internal_err(XmlXsd10AbstractCtxt *actxt, const char *funcName, const char *message); // xmlSchemaInternalErr
void xml_xsd10_custom_err4(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10BasicItem *item, const char *message, const char *str1, const char *str2, const char *str3, const char *str4); // xmlSchemaCustomErr4
void xml_xsd10_custom_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10BasicItem *item, const char *message, const char *str1, const char *str2); // xmlSchemaCustomErr
void xml_xsd10_custom_warning(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10Type *type, const char *message, const char *str1, const char *str2, const char *str3); // xmlSchemaCustomWarning
void xml_xsd10_keyref_err(XmlXsd10ValidCtxt *vctxt, int error, XmlXsd10PSVIIDCNode *idcNode, XmlXsd10Type *type, const char *message, const char *str1, const char *str2); // xmlSchemaKeyrefErr
int xml_xsd10_eval_error_node_type(XmlXsd10AbstractCtxt *actxt, XMQNode *node); // xmlSchemaEvalErrorNodeType
int xml_xsd10_is_global_item(XmlXsd10Type *item); // xmlSchemaIsGlobalItem
void xml_xsd10_simple_type_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *value, XmlXsd10Type *type, int displayValue); // xmlSchemaSimpleTypeErr
const char *xml_xsd10_format_error_node_q_name(char **str, XmlXsd10NodeInfo *ni, XMQNode *node); // xmlSchemaFormatErrorNodeQName
void xml_xsd10_illegal_attr_err(XmlXsd10AbstractCtxt *actxt, int error, XmlXsd10AttrInfo *ni, XMQNode *node); // xmlSchemaIllegalAttrErr
void xml_xsd10_complex_type_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10Type *type, const char *message, int nbval, int nbneg, char **values); // xmlSchemaComplexTypeErr
void xml_xsd10_facet_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *value, unsigned long length, XmlXsd10Type *type, XmlXsd10Facet *facet, const char *message, const char *str1, const char *str2); // xmlSchemaFacetErr
void xml_xsd10_p_missing_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, const char *message); // xmlSchemaPMissingAttrErr
void xml_xsd10_p_res_comp_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, const char *refName, const char *refURI, XmlXsd10TypeType refType, const char *refTypeStr); // xmlSchemaPResCompAttrErr
void xml_xsd10_p_custom_attr_err(XmlXsd10ParserCtxt *ctxt, int error, char **ownerDes, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *msg); // xmlSchemaPCustomAttrErr
void xml_xsd10_p_illegal_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerComp, XMQNode *attr); // xmlSchemaPIllegalAttrErr
void xml_xsd10_p_custom_err_ext(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *item, XMQNode *itemElem, const char *message, const char *str1, const char *str2, const char *str3); // xmlSchemaPCustomErrExt
void xml_xsd10_p_custom_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *item, XMQNode *itemElem, const char *message, const char *str1); // xmlSchemaPCustomErr
void xml_xsd10_p_attr_use_err4(XmlXsd10ParserCtxt *ctxt, int error, XMQNode *node, XmlXsd10BasicItem *ownerItem, const XmlXsd10AttributeUse *attruse, const char *message, const char *str1, const char *str2, const char *str3,const char *str4); // xmlSchemaPAttrUseErr4
void xml_xsd10_p_illegal_facet_atomic_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10Type *type, XmlXsd10Type *baseType, XmlXsd10Facet *facet); // xmlSchemaPIllegalFacetAtomicErr
void xml_xsd10_p_illegal_facet_list_union_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10Type *type, XmlXsd10Facet *facet); // xmlSchemaPIllegalFacetListUnionErr
void xml_xsd10_p_mutual_excl_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *name1, const char *name2); // xmlSchemaPMutualExclAttrErr
void xml_xsd10_p_simple_type_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *node, XmlXsd10Type *type, const char *expected, const char *value, const char *message, const char *str1, const char *str2); // xmlSchemaPSimpleTypeErr
void xml_xsd10_p_content_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, XMQNode *child, const char *message, const char *content); // xmlSchemaPContentErr
XmlXsd10Schema *xml_xsd10_new_schema(XmlXsd10ParserCtxt *ctxt); // xmlSchemaNewSchema
XmlXsd10Annot *xml_xsd10_new_annot(XmlXsd10ParserCtxt *ctxt, XMQNode *node); // xmlSchemaNewAnnot
XmlXsd10ItemList *xml_xsd10_item_list_create(void); // xmlSchemaItemListCreate
void xml_xsd10_item_list_clear(XmlXsd10ItemList *list); // xmlSchemaItemListClear
int xml_xsd10_item_list_grow(XmlXsd10ItemList *list, int initialSize); // xmlSchemaItemListGrow
int xml_xsd10_item_list_add_size(XmlXsd10ItemList *list, int initialSize, void *item); // xmlSchemaItemListAddSize
int xml_xsd10_item_list_add(XmlXsd10ItemList *list, void *item); // xmlSchemaItemListAdd
int xml_xsd10_item_list_insert(XmlXsd10ItemList *list, void *item, int idx); // xmlSchemaItemListInsert
int xml_xsd10_item_list_remove(XmlXsd10ItemList *list, int idx); // xmlSchemaItemListRemove
void xml_xsd10_item_list_free(XmlXsd10ItemList *list); // xmlSchemaItemListFree
void xml_xsd10_bucket_free(XmlXsd10Bucket *bucket); // xmlSchemaBucketFree
void xml_xsd10_bucket_free_entry(void *bucket, const char *name); // xmlSchemaBucketFreeEntry
XmlXsd10Bucket *xml_xsd10_bucket_create(XmlXsd10ParserCtxt *pctxt, int type, const char *targetNamespace); // xmlSchemaBucketCreate
int xml_xsd10_add_item_size(XmlXsd10ItemList **list, int initialSize, void *item); // xmlSchemaAddItemSize
void xml_xsd10_free_annot(XmlXsd10Annot *annot); // xmlSchemaFreeAnnot
void xml_xsd10_free_notation(XmlXsd10Notation *nota); // xmlSchemaFreeNotation
void xml_xsd10_free_attribute(XmlXsd10Attribute *attr); // xmlSchemaFreeAttribute
void xml_xsd10_free_attribute_use(XmlXsd10AttributeUse *use); // xmlSchemaFreeAttributeUse
void xml_xsd10_free_attribute_use_prohib(XmlXsd10AttributeUseProhib *prohib); // xmlSchemaFreeAttributeUseProhib
void xml_xsd10_free_wildcard_ns_set(XmlXsd10WildcardNs *set); // xmlSchemaFreeWildcardNsSet
void xml_xsd10_free_wildcard(XmlXsd10Wildcard *wildcard); // xmlSchemaFreeWildcard
void xml_xsd10_free_attribute_group(XmlXsd10AttributeGroup *attrGr); // xmlSchemaFreeAttributeGroup
void xml_xsd10_free_q_name_ref(XmlXsd10QNameRef *item); // xmlSchemaFreeQNameRef
void xml_xsd10_free_type_link_list(XmlXsd10TypeLink *link); // xmlSchemaFreeTypeLinkList
void xml_xsd10_free_idc_state_obj_list(XmlXsd10IDCStateObj *sto); // xmlSchemaFreeIDCStateObjList
void xml_xsd10_free_idc(XmlXsd10IDC *idcDef); // xmlSchemaFreeIDC
void xml_xsd10_free_element(XmlXsd10Element *elem); // xmlSchemaFreeElement
void xml_xsd10_free_type(XmlXsd10Type *type); // xmlSchemaFreeType
void xml_xsd10_free_model_group_def(XmlXsd10ModelGroupDef *item); // xmlSchemaFreeModelGroupDef
void xml_xsd10_free_model_group(XmlXsd10ModelGroup *item); // xmlSchemaFreeModelGroup
void xml_xsd10_component_list_free(XmlXsd10ItemList *list); // xmlSchemaComponentListFree
// xml_xsd10_free: public, see above (xmlSchemaFree)
XMQNode *xml_xsd10_get_prop_node(XMQNode *node, const char *name); // xmlSchemaGetPropNode
XMQNode *xml_xsd10_get_prop_node_ns(XMQNode *node, const char *uri, const char *name); // xmlSchemaGetPropNodeNs
const char *xml_xsd10_get_node_content(XmlXsd10ParserCtxt *ctxt, XMQNode *node); // xmlSchemaGetNodeContent
const char *xml_xsd10_get_node_content_no_dict(XMQNode *node); // xmlSchemaGetNodeContentNoDict
const char *xml_xsd10_get_prop(XmlXsd10ParserCtxt *ctxt, XMQNode *node, const char *name); // xmlSchemaGetProp
void xml_xsd10_subst_group_free(XmlXsd10SubstGroup *group); // xmlSchemaSubstGroupFree
void xml_xsd10_subst_group_free_entry(void *group, const char *name); // xmlSchemaSubstGroupFreeEntry
void xml_xsd10_redef_list_free(XmlXsd10Redef *redef); // xmlSchemaRedefListFree
void xml_xsd10_construction_ctxt_free(XmlXsd10ConstructionCtxt *con); // xmlSchemaConstructionCtxtFree
XmlXsd10ConstructionCtxt *xml_xsd10_construction_ctxt_create(XmlXsd10Dict *dict); // xmlSchemaConstructionCtxtCreate
XmlXsd10ParserCtxt *xml_xsd10_parser_ctxt_create(void); // xmlSchemaParserCtxtCreate
XmlXsd10ParserCtxt *xml_xsd10_new_parser_ctxt_use_dict(const char *URL, XmlXsd10Dict *dict); // xmlSchemaNewParserCtxtUseDict
int xml_xsd10_create_p_ctxt_on_v_ctxt(XmlXsd10ValidCtxt *vctxt); // xmlSchemaCreatePCtxtOnVCtxt
// xml_xsd10_new_parser_ctxt: public, see above (xmlSchemaNewParserCtxt)
// xml_xsd10_new_mem_parser_ctxt: public, see above (xmlSchemaNewMemParserCtxt)
// xml_xsd10_new_doc_parser_ctxt: public, see above (xmlSchemaNewDocParserCtxt)
// xml_xsd10_free_parser_ctxt: public, see above (xmlSchemaFreeParserCtxt)
// xml_xsd10_set_parser_errors: public, see above (xmlSchemaSetParserErrors)
// xml_xsd10_set_parser_structured_errors: public, see above (xmlSchemaSetParserStructuredErrors)
// xml_xsd10_get_parser_errors: public, see above (xmlSchemaGetParserErrors)
// xml_xsd10_set_resource_loader: public, see above (xmlSchemaSetResourceLoader)
const char *xml_xsd10_facet_type_to_string(XmlXsd10TypeType type); // xmlSchemaFacetTypeToString
XmlXsd10WhitespaceValueType xml_xsd10_get_white_space_facet_value(XmlXsd10Type *type); // xmlSchemaGetWhiteSpaceFacetValue
// xml_xsd10_is_valid: public, see above (xmlSchemaIsValid)
// xml_xsd10_set_valid_errors: public, see above (xmlSchemaSetValidErrors)
// xml_xsd10_set_valid_structured_errors: public, see above (xmlSchemaSetValidStructuredErrors)
// xml_xsd10_get_valid_errors: public, see above (xmlSchemaGetValidErrors)
// xml_xsd10_set_valid_options: public, see above (xmlSchemaSetValidOptions)
// xml_xsd10_valid_ctxt_get_options: public, see above (xmlSchemaValidCtxtGetOptions)

// xml_xsd10_parse (xmlschemas.c lines 4567-12108: parsing functions).
XmlXsd10Element *xml_xsd10_get_elem(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetElem
XmlXsd10Type *xml_xsd10_get_type(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetType
XmlXsd10Attribute *xml_xsd10_get_attribute_decl(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetAttributeDecl
XmlXsd10AttributeGroup *xml_xsd10_get_attribute_group(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetAttributeGroup
XmlXsd10ModelGroupDef *xml_xsd10_get_group(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetGroup
XmlXsd10Notation *xml_xsd10_get_notation(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetNotation
XmlXsd10IDC *xml_xsd10_get_idc(XmlXsd10Schema *schema, const char *name, const char *nsName); // xmlSchemaGetIDC
XmlXsd10BasicItem *xml_xsd10_get_named_component(XmlXsd10Schema *schema, XmlXsd10TypeType itemType, const char *name, const char *targetNs); // xmlSchemaGetNamedComponent
int xml_xsd10_is_blank(char *str, int len); // xmlSchemaIsBlank
XmlXsd10BasicItem *xml_xsd10_find_redef_comp_in_graph(XmlXsd10Bucket *bucket, XmlXsd10TypeType type, const char *name, const char *nsName); // xmlSchemaFindRedefCompInGraph
XmlXsd10Notation *xml_xsd10_add_notation(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, const char *name, const char *nsName, XMQNode *node); // xmlSchemaAddNotation
XmlXsd10Attribute *xml_xsd10_add_attribute(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, const char *name, const char *nsName, XMQNode *node, int topLevel); // xmlSchemaAddAttribute
XmlXsd10AttributeUse *xml_xsd10_add_attribute_use(XmlXsd10ParserCtxt *pctxt, XMQNode *node); // xmlSchemaAddAttributeUse
XmlXsd10Redef *xml_xsd10_add_redef(XmlXsd10ParserCtxt *pctxt, XmlXsd10Bucket *targetBucket, void *item, const char *refName, const char *refTargetNs); // xmlSchemaAddRedef
XmlXsd10AttributeGroup *xml_xsd10_add_attribute_group_definition(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, const char *name, const char *nsName, XMQNode *node); // xmlSchemaAddAttributeGroupDefinition
XmlXsd10Element *xml_xsd10_add_element(XmlXsd10ParserCtxt *ctxt, const char *name, const char *nsName, XMQNode *node, int topLevel); // xmlSchemaAddElement
XmlXsd10Type *xml_xsd10_add_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10TypeType type, const char *name, const char *nsName, XMQNode *node, int topLevel); // xmlSchemaAddType
XmlXsd10QNameRef *xml_xsd10_new_q_name_ref(XmlXsd10ParserCtxt *pctxt, XmlXsd10TypeType refType, const char *refName, const char *refNs); // xmlSchemaNewQNameRef
XmlXsd10AttributeUseProhib *xml_xsd10_add_attribute_use_prohib(XmlXsd10ParserCtxt *pctxt); // xmlSchemaAddAttributeUseProhib
XmlXsd10ModelGroup *xml_xsd10_add_model_group(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10TypeType type, XMQNode *node); // xmlSchemaAddModelGroup
XmlXsd10Particle *xml_xsd10_add_particle(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int min, int max); // xmlSchemaAddParticle
XmlXsd10ModelGroupDef *xml_xsd10_add_model_group_definition(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, const char *name, const char *nsName, XMQNode *node); // xmlSchemaAddModelGroupDefinition
XmlXsd10WildcardNs *xml_xsd10_new_wildcard_ns_constraint(XmlXsd10ParserCtxt *ctxt); // xmlSchemaNewWildcardNsConstraint
XmlXsd10IDC *xml_xsd10_add_idc(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, const char *name, const char *nsName, int category, XMQNode *node); // xmlSchemaAddIDC
XmlXsd10Wildcard *xml_xsd10_add_wildcard(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10TypeType type, XMQNode *node); // xmlSchemaAddWildcard
XmlXsd10SubstGroup *xml_xsd10_subst_group_add(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *head); // xmlSchemaSubstGroupAdd
XmlXsd10SubstGroup *xml_xsd10_subst_group_get(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *head); // xmlSchemaSubstGroupGet
int xml_xsd10_add_element_substitution_member(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *head, XmlXsd10Element *member); // xmlSchemaAddElementSubstitutionMember
int xml_xsd10_p_val_attr_node_q_name_value(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *value, const char **uri, const char **local); // xmlSchemaPValAttrNodeQNameValue
int xml_xsd10_p_val_attr_node_q_name(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char **uri, const char **local); // xmlSchemaPValAttrNodeQName
int xml_xsd10_p_val_attr_q_name(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, const char **uri, const char **local); // xmlSchemaPValAttrQName
int xml_xsd10_p_val_attr_node_id(XmlXsd10ParserCtxt *ctxt, XMQNode *attr); // xmlSchemaPValAttrNodeID
int xml_xsd10_p_val_attr_id(XmlXsd10ParserCtxt *ctxt, XMQNode *ownerElem, const char *name); // xmlSchemaPValAttrID
int xml_xsd10_get_max_occurs(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int min, int max, int def, const char *expected); // xmlGetMaxOccurs
int xml_xsd10_get_min_occurs(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int min, int max, int def, const char *expected); // xmlGetMinOccurs
int xml_xsd10_p_get_bool_node_value(XmlXsd10ParserCtxt *ctxt, XmlXsd10BasicItem *ownerItem, XMQNode *node); // xmlSchemaPGetBoolNodeValue
int xml_xsd10_get_boolean_prop(XmlXsd10ParserCtxt *ctxt, XMQNode *node, const char *name, int def); // xmlGetBooleanProp
int xml_xsd10_p_val_attr_node_value(XmlXsd10ParserCtxt *pctxt, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *value, XmlXsd10Type *type); // xmlSchemaPValAttrNodeValue
int xml_xsd10_p_val_attr_node(XmlXsd10ParserCtxt *ctxt, XmlXsd10BasicItem *ownerItem, XMQNode *attr, XmlXsd10Type *type, const char **value); // xmlSchemaPValAttrNode
int xml_xsd10_p_val_attr(XmlXsd10ParserCtxt *ctxt, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, XmlXsd10Type *type, const char **value); // xmlSchemaPValAttr
int xml_xsd10_check_reference(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node, XMQNode *attr, const char *namespaceName); // xmlSchemaCheckReference
int xml_xsd10_parse_local_attributes(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode **child, XmlXsd10ItemList **list, int parentType, int *hasRefs); // xmlSchemaParseLocalAttributes
XmlXsd10Annot *xml_xsd10_parse_annotation(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int needed); // xmlSchemaParseAnnotation
XmlXsd10Facet *xml_xsd10_parse_facet(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseFacet
int xml_xsd10_parse_wildcard_ns(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XmlXsd10Wildcard *wildc, XMQNode *node); // xmlSchemaParseWildcardNs
int xml_xsd10_p_check_particle_correct_2(XmlXsd10ParserCtxt *ctxt, XmlXsd10Particle *item, XMQNode *node, int minOccurs, int maxOccurs); // xmlSchemaPCheckParticleCorrect_2
XmlXsd10Particle *xml_xsd10_parse_any(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseAny
XmlXsd10Notation *xml_xsd10_parse_notation(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseNotation
XmlXsd10Wildcard *xml_xsd10_parse_any_attribute(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseAnyAttribute
XmlXsd10BasicItem *xml_xsd10_parse_local_attribute(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node, XmlXsd10ItemList *uses, int parentType); // xmlSchemaParseLocalAttribute
XmlXsd10Attribute *xml_xsd10_parse_global_attribute(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseGlobalAttribute
XmlXsd10QNameRef *xml_xsd10_parse_attribute_group_ref(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseAttributeGroupRef
XmlXsd10AttributeGroup *xml_xsd10_parse_attribute_group_definition(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseAttributeGroupDefinition
int xml_xsd10_p_val_attr_form_default(const char *value, int *flags, int flagQualified); // xmlSchemaPValAttrFormDefault
int xml_xsd10_p_val_attr_block_final(const char *value, int *flags, int flagAll, int flagExtension, int flagRestriction, int flagSubstitution, int flagList, int flagUnion); // xmlSchemaPValAttrBlockFinal
int xml_xsd10_check_c_selector_x_path(XmlXsd10ParserCtxt *ctxt, XmlXsd10IDC *idc, XmlXsd10IDCSelect *selector, XMQNode *attr, int isField); // xmlSchemaCheckCSelectorXPath
XmlXsd10Annot *xml_xsd10_add_annotation(XmlXsd10AnnotItem *annItem, XmlXsd10Annot *annot); // xmlSchemaAddAnnotation
XmlXsd10IDCSelect *xml_xsd10_parse_idc_selector_and_field(XmlXsd10ParserCtxt *ctxt, XmlXsd10IDC *idc, XMQNode *node, int isField); // xmlSchemaParseIDCSelectorAndField
XmlXsd10IDC *xml_xsd10_parse_idc(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, XmlXsd10TypeType idcCategory, const char *targetNamespace); // xmlSchemaParseIDC
XmlXsd10BasicItem *xml_xsd10_parse_element(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, int *isElemRef, int topLevel); // xmlSchemaParseElement
int xml_xsd10_parse_union(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseUnion
XmlXsd10Type *xml_xsd10_parse_list(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseList
XmlXsd10Type *xml_xsd10_parse_simple_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, int topLevel); // xmlSchemaParseSimpleType
XmlXsd10TreeItem *xml_xsd10_parse_model_group_def_ref(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseModelGroupDefRef
XmlXsd10ModelGroupDef *xml_xsd10_parse_model_group_definition(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseModelGroupDefinition
void xml_xsd10_cleanup_doc(XmlXsd10ParserCtxt *ctxt, XMQNode *root); // xmlSchemaCleanupDoc
void xml_xsd10_clear_schema_defaults(XmlXsd10Schema *schema); // xmlSchemaClearSchemaDefaults
int xml_xsd10_parse_schema_element(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseSchemaElement
int xml_xsd10_parse_schema_top_level(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *nodes); // xmlSchemaParseSchemaTopLevel
XmlXsd10SchemaRelation *xml_xsd10_schema_relation_create(void); // xmlSchemaSchemaRelationCreate
XmlXsd10Bucket *xml_xsd10_get_schema_bucket(XmlXsd10ParserCtxt *pctxt, const char *schemaLocation); // xmlSchemaGetSchemaBucket
XmlXsd10Bucket *xml_xsd10_get_chameleon_schema_bucket(XmlXsd10ParserCtxt *pctxt, const char *schemaLocation, const char *targetNamespace); // xmlSchemaGetChameleonSchemaBucket
XmlXsd10Bucket *xml_xsd10_get_schema_bucket_by_tns(XmlXsd10ParserCtxt *pctxt, const char *targetNamespace, int imported); // xmlSchemaGetSchemaBucketByTNS
int xml_xsd10_parse_new_doc_with_context(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XmlXsd10Bucket *bucket); // xmlSchemaParseNewDocWithContext
int xml_xsd10_parse_new_doc(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XmlXsd10Bucket *bucket); // xmlSchemaParseNewDoc
void xml_xsd10_schema_relation_add_child(XmlXsd10Bucket *bucket, XmlXsd10SchemaRelation *rel); // xmlSchemaSchemaRelationAddChild
const char *xml_xsd10_build_absolute_uri(XmlXsd10Dict *dict, const char *location, XMQNode *ctxtNode, const char *baseURI); // xmlSchemaBuildAbsoluteURI
int xml_xsd10_add_schema_doc(XmlXsd10ParserCtxt *pctxt, int type, const char *schemaLocation, XmlDom *schemaDoc, const char *schemaBuffer, int schemaBufferLen, XMQNode *invokingNode, const char *sourceTargetNamespace, const char *importNamespace, XmlXsd10Bucket **bucket); // xmlSchemaAddSchemaDoc
int xml_xsd10_parse_import(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseImport
int xml_xsd10_parse_include_or_redefine_attrs(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node, char **schemaLocation, int type); // xmlSchemaParseIncludeOrRedefineAttrs
int xml_xsd10_parse_include_or_redefine(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node, int type); // xmlSchemaParseIncludeOrRedefine
int xml_xsd10_parse_redefine(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseRedefine
int xml_xsd10_parse_include(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema, XMQNode *node); // xmlSchemaParseInclude
XmlXsd10TreeItem *xml_xsd10_parse_model_group(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, XmlXsd10TypeType type, int withParticle); // xmlSchemaParseModelGroup
XmlXsd10Type *xml_xsd10_parse_restriction(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, XmlXsd10TypeType parentType); // xmlSchemaParseRestriction
XmlXsd10Type *xml_xsd10_parse_extension(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, XmlXsd10TypeType parentType); // xmlSchemaParseExtension
int xml_xsd10_parse_simple_content(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, int *hasRestrictionOrExtension); // xmlSchemaParseSimpleContent
int xml_xsd10_parse_complex_content(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, int *hasRestrictionOrExtension); // xmlSchemaParseComplexContent
XmlXsd10Type *xml_xsd10_parse_complex_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema, XMQNode *node, int topLevel); // xmlSchemaParseComplexType

// xml_xsd10_build (xmlschemas.c lines 12264-20988: content models, fixup, constraints, xmlSchemaParse).
int xml_xsd10_build_content_model_for_subst_group(XmlXsd10ParserCtxt *pctxt, XmlXsd10Particle *particle, int counter, XmlAutomataState *end); // xmlSchemaBuildContentModelForSubstGroup
int xml_xsd10_build_content_model_for_element(XmlXsd10ParserCtxt *ctxt, XmlXsd10Particle *particle); // xmlSchemaBuildContentModelForElement
int xml_xsd10_build_a_content_model(XmlXsd10ParserCtxt *pctxt, XmlXsd10Particle *particle); // xmlSchemaBuildAContentModel
void xml_xsd10_build_content_model(XmlXsd10Type *type, XmlXsd10ParserCtxt *ctxt); // xmlSchemaBuildContentModel
void xml_xsd10_resolve_element_references(XmlXsd10Element *elemDecl, XmlXsd10ParserCtxt *ctxt); // xmlSchemaResolveElementReferences
int xml_xsd10_resolve_union_member_types(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaResolveUnionMemberTypes
int xml_xsd10_is_derived_from_built_in_type(XmlXsd10Type *type, int valType); // xmlSchemaIsDerivedFromBuiltInType
XmlXsd10Type *xml_xsd10_get_primitive_type(XmlXsd10Type *type); // xmlSchemaGetPrimitiveType
int xml_xsd10_clone_wildcard_ns_constraints(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *dest, XmlXsd10Wildcard *source); // xmlSchemaCloneWildcardNsConstraints
int xml_xsd10_union_wildcards(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *completeWild, XmlXsd10Wildcard *curWild); // xmlSchemaUnionWildcards
int xml_xsd10_intersect_wildcards(XmlXsd10ParserCtxt *ctxt, XmlXsd10Wildcard *completeWild, XmlXsd10Wildcard *curWild); // xmlSchemaIntersectWildcards
int xml_xsd10_check_cosns_subset(XmlXsd10Wildcard *sub, XmlXsd10Wildcard *super); // xmlSchemaCheckCOSNSSubset
int xml_xsd10_get_effective_value_constraint(XmlXsd10AttributeUse *attruse, int *fixed, const char **value, XmlXsd10Val **val); // xmlSchemaGetEffectiveValueConstraint
int xml_xsd10_check_cvc_wildcard_namespace(XmlXsd10Wildcard *wild, const char *ns); // xmlSchemaCheckCVCWildcardNamespace
int xml_xsd10_check_derivation_ok_restriction2to4(XmlXsd10ParserCtxt *pctxt, int action, XmlXsd10BasicItem *item, XmlXsd10BasicItem *baseItem, XmlXsd10ItemList *uses, XmlXsd10ItemList *baseUses, XmlXsd10Wildcard *wild, XmlXsd10Wildcard *baseWild); // xmlSchemaCheckDerivationOKRestriction2to4
int xml_xsd10_fixup_type_attribute_uses(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaFixupTypeAttributeUses
int xml_xsd10_type_final_contains(XmlXsd10Type *type, int final); // xmlSchemaTypeFinalContains
XmlXsd10TypeLink *xml_xsd10_get_union_simple_type_member_types(XmlXsd10Type *type); // xmlSchemaGetUnionSimpleTypeMemberTypes
int xml_xsd10_get_particle_emptiable(XmlXsd10Particle *particle); // xmlSchemaGetParticleEmptiable
int xml_xsd10_is_particle_emptiable(XmlXsd10Particle *particle); // xmlSchemaIsParticleEmptiable
int xml_xsd10_check_cosst_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int subset); // xmlSchemaCheckCOSSTDerivedOK
int xml_xsd10_check_type_def_circular_internal(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *ctxtType, XmlXsd10Type *ancestor); // xmlSchemaCheckTypeDefCircularInternal
void xml_xsd10_check_type_def_circular(XmlXsd10Type *item, XmlXsd10ParserCtxt *ctxt); // xmlSchemaCheckTypeDefCircular
int xml_xsd10_check_union_type_def_circular_recur(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *ctxType, XmlXsd10TypeLink *members); // xmlSchemaCheckUnionTypeDefCircularRecur
int xml_xsd10_check_union_type_def_circular(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaCheckUnionTypeDefCircular
void xml_xsd10_resolve_type_references(XmlXsd10Type *typeDef, XmlXsd10ParserCtxt *ctxt); // xmlSchemaResolveTypeReferences
int xml_xsd10_check_st_props_correct(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaCheckSTPropsCorrect
int xml_xsd10_check_cosst_restricts(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaCheckCOSSTRestricts
int xml_xsd10_create_v_ctxt_on_p_ctxt(XmlXsd10ParserCtxt *ctxt); // xmlSchemaCreateVCtxtOnPCtxt
int xml_xsd10_parse_check_cos_valid_default(XmlXsd10ParserCtxt *pctxt, XMQNode *node, XmlXsd10Type *type, const char *value, XmlXsd10Val **val); // xmlSchemaParseCheckCOSValidDefault
int xml_xsd10_check_ct_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaCheckCTPropsCorrect
int xml_xsd10_are_equal_types(XmlXsd10Type *typeA, XmlXsd10Type *typeB); // xmlSchemaAreEqualTypes
int xml_xsd10_check_cosct_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int set); // xmlSchemaCheckCOSCTDerivedOK
int xml_xsd10_check_cos_derived_ok(XmlXsd10AbstractCtxt *actxt, XmlXsd10Type *type, XmlXsd10Type *baseType, int set); // xmlSchemaCheckCOSDerivedOK
int xml_xsd10_check_cosct_extends(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaCheckCOSCTExtends
int xml_xsd10_check_derivation_ok_restriction(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaCheckDerivationOKRestriction
int xml_xsd10_check_ct_component(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaCheckCTComponent
int xml_xsd10_check_srcct(XmlXsd10ParserCtxt *ctxt, XmlXsd10Type *type); // xmlSchemaCheckSRCCT
void xml_xsd10_derive_facet_err(XmlXsd10ParserCtxt *pctxt, XmlXsd10Facet *facet1, XmlXsd10Facet *facet2, int lessGreater, int orEqual, int ofBase); // xmlSchemaDeriveFacetErr
int xml_xsd10_derive_and_validate_facets(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaDeriveAndValidateFacets
int xml_xsd10_finish_member_type_definitions_property(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaFinishMemberTypeDefinitionsProperty
void xml_xsd10_type_fixup_optim_facets(XmlXsd10Type *type); // xmlSchemaTypeFixupOptimFacets
int xml_xsd10_type_fixup_whitespace(XmlXsd10Type *type); // xmlSchemaTypeFixupWhitespace
int xml_xsd10_fixup_simple_type_stage_one(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaFixupSimpleTypeStageOne
int xml_xsd10_fixup_simple_type_stage_two(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaFixupSimpleTypeStageTwo
int xml_xsd10_fixup_complex_type(XmlXsd10ParserCtxt *pctxt, XmlXsd10Type *type); // xmlSchemaFixupComplexType
int xml_xsd10_type_fixup(XmlXsd10Type *type, XmlXsd10AbstractCtxt *actxt); // xmlSchemaTypeFixup
int xml_xsd10_check_facet_ctxt(XmlXsd10Facet *facet, XmlXsd10Type *typeDecl, XmlXsd10ParserCtxt *pctxt, const char *name); // xmlSchemaCheckFacet
int xml_xsd10_check_facet_values(XmlXsd10Type *typeDecl, XmlXsd10ParserCtxt *pctxt); // xmlSchemaCheckFacetValues
XmlXsd10TreeItem *xml_xsd10_get_circ_model_gr_def_ref(XmlXsd10ModelGroupDef *groupDef, XmlXsd10TreeItem *particle); // xmlSchemaGetCircModelGrDefRef
void xml_xsd10_check_group_def_circular(XmlXsd10ModelGroupDef *item, XmlXsd10ParserCtxt *ctxt); // xmlSchemaCheckGroupDefCircular
void xml_xsd10_model_group_to_model_group_def_fixup(XmlXsd10ParserCtxt *ctxt, XmlXsd10ModelGroup *mg); // xmlSchemaModelGroupToModelGroupDefFixup
XmlXsd10QNameRef *xml_xsd10_check_attr_group_circular_recur(XmlXsd10AttributeGroup *ctxtGr, XmlXsd10ItemList *list); // xmlSchemaCheckAttrGroupCircularRecur
int xml_xsd10_check_attr_group_circular(XmlXsd10AttributeGroup *attrGr, XmlXsd10ParserCtxt *ctxt); // xmlSchemaCheckAttrGroupCircular
int xml_xsd10_expand_attribute_group_refs(XmlXsd10ParserCtxt *pctxt, XmlXsd10BasicItem *item, XmlXsd10Wildcard **completeWild, XmlXsd10ItemList *list, XmlXsd10ItemList *prohibs); // xmlSchemaExpandAttributeGroupRefs
int xml_xsd10_attribute_group_expand_refs(XmlXsd10ParserCtxt *pctxt, XmlXsd10AttributeGroup *attrGr); // xmlSchemaAttributeGroupExpandRefs
int xml_xsd10_check_ag_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10AttributeGroup *attrGr); // xmlSchemaCheckAGPropsCorrect
int xml_xsd10_resolve_attr_group_references(XmlXsd10QNameRef *ref, XmlXsd10ParserCtxt *ctxt); // xmlSchemaResolveAttrGroupReferences
int xml_xsd10_check_attr_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Attribute *attr); // xmlSchemaCheckAttrPropsCorrect
XmlXsd10Element *xml_xsd10_check_subst_group_circular(XmlXsd10Element *elemDecl, XmlXsd10Element *ancestor); // xmlSchemaCheckSubstGroupCircular
int xml_xsd10_check_elem_props_correct(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *elemDecl); // xmlSchemaCheckElemPropsCorrect
void xml_xsd10_check_elem_subst_group(XmlXsd10ParserCtxt *ctxt, XmlXsd10Element *elemDecl); // xmlSchemaCheckElemSubstGroup
void xml_xsd10_check_element_decl_component(XmlXsd10Element *elemDecl, XmlXsd10ParserCtxt *ctxt); // xmlSchemaCheckElementDeclComponent
void xml_xsd10_resolve_model_group_particle_references(XmlXsd10ParserCtxt *ctxt, XmlXsd10ModelGroup *mg); // xmlSchemaResolveModelGroupParticleReferences
int xml_xsd10_are_values_equal(XmlXsd10Val *x, XmlXsd10Val *y); // xmlSchemaAreValuesEqual
int xml_xsd10_resolve_attr_use_references(XmlXsd10AttributeUse *ause, XmlXsd10ParserCtxt *ctxt); // xmlSchemaResolveAttrUseReferences
int xml_xsd10_check_attr_use_props_correct(XmlXsd10ParserCtxt *ctxt, XmlXsd10AttributeUse *use); // xmlSchemaCheckAttrUsePropsCorrect
int xml_xsd10_resolve_attr_type_references(XmlXsd10Attribute *item, XmlXsd10ParserCtxt *ctxt); // xmlSchemaResolveAttrTypeReferences
int xml_xsd10_resolve_idc_key_references(XmlXsd10IDC *idc, XmlXsd10ParserCtxt *pctxt); // xmlSchemaResolveIDCKeyReferences
int xml_xsd10_resolve_attr_use_prohib_references(XmlXsd10AttributeUseProhib *prohib, XmlXsd10ParserCtxt *pctxt); // xmlSchemaResolveAttrUseProhibReferences
int xml_xsd10_check_src_redefine_first(XmlXsd10ParserCtxt *pctxt); // xmlSchemaCheckSRCRedefineFirst
int xml_xsd10_check_src_redefine_second(XmlXsd10ParserCtxt *pctxt); // xmlSchemaCheckSRCRedefineSecond
int xml_xsd10_add_components(XmlXsd10ParserCtxt *pctxt, XmlXsd10Bucket *bucket); // xmlSchemaAddComponents
int xml_xsd10_fixup_components(XmlXsd10ParserCtxt *pctxt, XmlXsd10Bucket *rootBucket); // xmlSchemaFixupComponents
// xml_xsd10_parse: public, see above (xmlSchemaParse)

// xml_xsd10_valid (xmlschemas.c lines 21152-26564 and 27029-27764: simple type validation,
// identity constraints, the validation of element infos and attributes, the dom walk).
int xml_xsd10_assemble_by_location(XmlXsd10ValidCtxt *vctxt, XmlXsd10Schema *schema, XMQNode *node, const char *nsName, const char *location); // xmlSchemaAssembleByLocation
XmlXsd10AttrInfo *xml_xsd10_get_meta_attr_info(XmlXsd10ValidCtxt *vctxt, int metaType); // xmlSchemaGetMetaAttrInfo
int xml_xsd10_assemble_by_xsi(XmlXsd10ValidCtxt *vctxt); // xmlSchemaAssembleByXSI
const char *xml_xsd10_lookup_namespace(XmlXsd10ValidCtxt *vctxt, const char *prefix); // xmlSchemaLookupNamespace
int xml_xsd10_validate_notation(XmlXsd10ValidCtxt *vctxt, XmlXsd10Schema *schema, XMQNode *node, const char *value, XmlXsd10Val **val, int valNeeded); // xmlSchemaValidateNotation
int xml_xsd10_v_add_node_q_name(XmlXsd10ValidCtxt *vctxt, const char *lname, const char *nsname); // xmlSchemaVAddNodeQName
void xml_xsd10_augment_idc(void *payload, void *data, const char *name); // xmlSchemaAugmentIDC
void xml_xsd10_augment_imported_idc(void *payload, void *data, const char *name); // xmlSchemaAugmentImportedIDC
XmlXsd10PSVIIDCBinding *xml_xsd10_idc_new_binding(XmlXsd10IDC *idcDef); // xmlSchemaIDCNewBinding
int xml_xsd10_idc_store_node_table_item(XmlXsd10ValidCtxt *vctxt, XmlXsd10PSVIIDCNode *item); // xmlSchemaIDCStoreNodeTableItem
int xml_xsd10_idc_store_key(XmlXsd10ValidCtxt *vctxt, XmlXsd10PSVIIDCKey *key); // xmlSchemaIDCStoreKey
int xml_xsd10_idc_append_node_table_item(XmlXsd10PSVIIDCBinding *bind, XmlXsd10PSVIIDCNode *ntItem); // xmlSchemaIDCAppendNodeTableItem
XmlXsd10PSVIIDCBinding *xml_xsd10_idc_acquire_binding(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher); // xmlSchemaIDCAcquireBinding
XmlXsd10ItemList *xml_xsd10_idc_acquire_target_list(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher); // xmlSchemaIDCAcquireTargetList
void xml_xsd10_idc_free_key(XmlXsd10PSVIIDCKey *key); // xmlSchemaIDCFreeKey
void xml_xsd10_idc_free_binding(XmlXsd10PSVIIDCBinding *bind); // xmlSchemaIDCFreeBinding
void xml_xsd10_idc_free_idc_table(XmlXsd10PSVIIDCBinding *bind); // xmlSchemaIDCFreeIDCTable
void xml_xsd10_free_idc_hash_entry(void *payload, const char *name); // xmlFreeIDCHashEntry
void xml_xsd10_idc_free_matcher_list(XmlXsd10IDCMatcher *matcher); // xmlSchemaIDCFreeMatcherList
void xml_xsd10_idc_release_matcher_list(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher); // xmlSchemaIDCReleaseMatcherList
int xml_xsd10_idc_add_state_object(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher, XmlXsd10IDCSelect *sel, int type); // xmlSchemaIDCAddStateObject
int xml_xsd10_x_path_evaluate(XmlXsd10ValidCtxt *vctxt, XmlNodeKind nodeType); // xmlSchemaXPathEvaluate
const char *xml_xsd10_format_idc_key_sequence_1(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count, int for_hash); // xmlSchemaFormatIDCKeySequence_1
const char *xml_xsd10_format_idc_key_sequence(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count); // xmlSchemaFormatIDCKeySequence
const char *xml_xsd10_hash_key_sequence(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count); // xmlSchemaHashKeySequence
int xml_xsd10_x_path_pop(XmlXsd10ValidCtxt *vctxt); // xmlSchemaXPathPop
int xml_xsd10_x_path_process_history(XmlXsd10ValidCtxt *vctxt, int depth); // xmlSchemaXPathProcessHistory
int xml_xsd10_idc_register_matchers(XmlXsd10ValidCtxt *vctxt, XmlXsd10Element *elemDecl); // xmlSchemaIDCRegisterMatchers
int xml_xsd10_idc_fill_node_tables(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *ielem); // xmlSchemaIDCFillNodeTables
int xml_xsd10_bubble_idc_node_tables(XmlXsd10ValidCtxt *vctxt); // xmlSchemaBubbleIDCNodeTables
int xml_xsd10_check_cvcidc_key_ref(XmlXsd10ValidCtxt *vctxt); // xmlSchemaCheckCVCIDCKeyRef
XmlXsd10AttrInfo *xml_xsd10_get_fresh_attr_info(XmlXsd10ValidCtxt *vctxt); // xmlSchemaGetFreshAttrInfo
int xml_xsd10_validator_push_attribute(XmlXsd10ValidCtxt *vctxt, XMQNode *attrNode, int nodeLine, const char *localName, const char *nsName, int ownedNames, char *value, int ownedValue); // xmlSchemaValidatorPushAttribute
void xml_xsd10_clear_elem_info(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *ielem); // xmlSchemaClearElemInfo
XmlXsd10NodeInfo *xml_xsd10_get_fresh_elem_info(XmlXsd10ValidCtxt *vctxt); // xmlSchemaGetFreshElemInfo
int xml_xsd10_validate_facets(XmlXsd10AbstractCtxt *actxt, XMQNode *node, XmlXsd10Type *type, XmlXsd10ValType valType, const char *value, XmlXsd10Val *val, unsigned long length, int fireErrors); // xmlSchemaValidateFacets
char *xml_xsd10_normalize_value(XmlXsd10Type *type, const char *value); // xmlSchemaNormalizeValue
int xml_xsd10_validate_q_name(XmlXsd10ValidCtxt *vctxt, const char *value, XmlXsd10Val **val, int valNeeded); // xmlSchemaValidateQName
int xml_xsd10_v_check_cvc_simple_type(XmlXsd10AbstractCtxt *actxt, XMQNode *node, XmlXsd10Type *type, const char *value, XmlXsd10Val **retVal, int fireErrors, int normalize, int isNormalized); // xmlSchemaVCheckCVCSimpleType
int xml_xsd10_v_expand_q_name(XmlXsd10ValidCtxt *vctxt, const char *value, const char **nsName, const char **localName); // xmlSchemaVExpandQName
int xml_xsd10_process_xsi_type(XmlXsd10ValidCtxt *vctxt, XmlXsd10AttrInfo *iattr, XmlXsd10Type **localType, XmlXsd10Element *elemDecl); // xmlSchemaProcessXSIType
int xml_xsd10_validate_elem_decl(XmlXsd10ValidCtxt *vctxt); // xmlSchemaValidateElemDecl
int xml_xsd10_v_attributes_simple(XmlXsd10ValidCtxt *vctxt); // xmlSchemaVAttributesSimple
void xml_xsd10_clear_attr_infos(XmlXsd10ValidCtxt *vctxt); // xmlSchemaClearAttrInfos
int xml_xsd10_v_attributes_complex(XmlXsd10ValidCtxt *vctxt); // xmlSchemaVAttributesComplex
int xml_xsd10_validate_elem_wildcard(XmlXsd10ValidCtxt *vctxt, int *skip); // xmlSchemaValidateElemWildcard
int xml_xsd10_check_cos_valid_default(XmlXsd10ValidCtxt *vctxt, const char *value, XmlXsd10Val **val); // xmlSchemaCheckCOSValidDefault
void xml_xsd10_v_content_model_callback(void *exec, const char *name, void *transdata, void *inputdata); // xmlSchemaVContentModelCallback (exec: see XmlRegExecCallbacks)
int xml_xsd10_validator_push_elem(XmlXsd10ValidCtxt *vctxt); // xmlSchemaValidatorPushElem
int xml_xsd10_v_check_i_node_data_type(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *inode, XmlXsd10Type *type, const char *value); // xmlSchemaVCheckINodeDataType
int xml_xsd10_validator_pop_elem(XmlXsd10ValidCtxt *vctxt); // xmlSchemaValidatorPopElem
int xml_xsd10_validate_child_elem(XmlXsd10ValidCtxt *vctxt); // xmlSchemaValidateChildElem
int xml_xsd10_v_push_text(XmlXsd10ValidCtxt *vctxt, int nodeType, const char *value, int len, int mode, int *consumed); // xmlSchemaVPushText
int xml_xsd10_validate_elem(XmlXsd10ValidCtxt *vctxt); // xmlSchemaValidateElem
// xml_xsd10_new_valid_ctxt: public, see above (xmlSchemaNewValidCtxt)
// xml_xsd10_validate_set_filename: public, see above (xmlSchemaValidateSetFilename)
void xml_xsd10_clear_valid_ctxt(XmlXsd10ValidCtxt *vctxt); // xmlSchemaClearValidCtxt
// xml_xsd10_free_valid_ctxt: public, see above (xmlSchemaFreeValidCtxt)
int xml_xsd10_v_doc_walk(XmlXsd10ValidCtxt *vctxt); // xmlSchemaVDocWalk
int xml_xsd10_pre_run(XmlXsd10ValidCtxt *vctxt); // xmlSchemaPreRun
void xml_xsd10_post_run(XmlXsd10ValidCtxt *vctxt); // xmlSchemaPostRun
int xml_xsd10_v_start(XmlXsd10ValidCtxt *vctxt); // xmlSchemaVStart
// xml_xsd10_validate_one_element: public, see above (xmlSchemaValidateOneElement)
// xml_xsd10_validate_doc: public, see above (xmlSchemaValidateDoc)

#endif // XML_XSD10_INTERNALS

#ifndef XMQ_NO_XSD
#define XML_XSD10_SCHEMAS_MODULE
#endif

#endif // XML_XSD10_SCHEMAS_H
