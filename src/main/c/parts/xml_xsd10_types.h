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

/* Portions of this file are ported from libxml2 (xmlschemastypes.h and
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

#ifndef XML_XSD10_TYPES_H
#define XML_XSD10_TYPES_H

/* The built in simple types of XML Schema 1.0 (Datatypes), a port of libxml2
   xmlschemastypes.c on the native dom: the type definitions, lexical value
   parsing (to the libxml2 value representation), canonical values, value
   comparison and facet validation.

   The component structs of the schema (types, facets, wildcards, ...) of
   libxml2 schemasInternals.h are also here since they are shared with the
   schema parser/validator. Names follow libxml2 with an Xsd10 prefix:
   xmlSchemaType -> XmlXsd10Type, XML_SCHEMAS_STRING -> XML_XSD10_SCHEMAS_STRING,
   XML_SCHEMA_FACET_LENGTH -> XML_XSD10_SCHEMA_FACET_LENGTH,
   xmlSchemaCompareValues -> xml_xsd10_compare_values.

   Strings are char* (utf8). The libxml2 dict strings of the components are
   plain malloced strings owned by whoever creates the component.

   The built in type definitions are static data (initialized at compile time,
   never modified or freed), there is no global state to set up and the lookup
   functions are thread safe. xml_xsd10_init_types and xml_xsd10_cleanup_types
   exist for symmetry with libxml2 and do nothing.

   Differences to libxml2 caused by the native dom:
   - The ID table (libxml2 doc->ids, doc->refs and attr->atype) is an explicit
     XmlXsd10Ids passed to the *_ids variants. Without a table (the libxml2 named
     functions) ID/IDREF values on attribute nodes are not registered.
   - Pattern facets need regular expressions, which are provided through
     xml_xsd10_set_regexp_hooks (libxml2 xmlRegexpCompile/Exec/Free).
   - xml_xsd10_check_facet only computes values against built in base types (the
     case of libxml2 xmlSchemaCheckFacet without a parser context), the
     schema part checks facets of user derived types. */

#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#endif

#include<stddef.h>

#define XML_XSD10_SCHEMAS_NAMESPACE_NAME "http://www.w3.org/2001/XMLSchema"

/** Schema value type (libxml2 xmlSchemaValType). */
typedef enum {
    XML_XSD10_SCHEMAS_UNKNOWN = 0,
    XML_XSD10_SCHEMAS_STRING = 1,
    XML_XSD10_SCHEMAS_NORMSTRING = 2,
    XML_XSD10_SCHEMAS_DECIMAL = 3,
    XML_XSD10_SCHEMAS_TIME = 4,
    XML_XSD10_SCHEMAS_GDAY = 5,
    XML_XSD10_SCHEMAS_GMONTH = 6,
    XML_XSD10_SCHEMAS_GMONTHDAY = 7,
    XML_XSD10_SCHEMAS_GYEAR = 8,
    XML_XSD10_SCHEMAS_GYEARMONTH = 9,
    XML_XSD10_SCHEMAS_DATE = 10,
    XML_XSD10_SCHEMAS_DATETIME = 11,
    XML_XSD10_SCHEMAS_DURATION = 12,
    XML_XSD10_SCHEMAS_FLOAT = 13,
    XML_XSD10_SCHEMAS_DOUBLE = 14,
    XML_XSD10_SCHEMAS_BOOLEAN = 15,
    XML_XSD10_SCHEMAS_TOKEN = 16,
    XML_XSD10_SCHEMAS_LANGUAGE = 17,
    XML_XSD10_SCHEMAS_NMTOKEN = 18,
    XML_XSD10_SCHEMAS_NMTOKENS = 19,
    XML_XSD10_SCHEMAS_NAME = 20,
    XML_XSD10_SCHEMAS_QNAME = 21,
    XML_XSD10_SCHEMAS_NCNAME = 22,
    XML_XSD10_SCHEMAS_ID = 23,
    XML_XSD10_SCHEMAS_IDREF = 24,
    XML_XSD10_SCHEMAS_IDREFS = 25,
    XML_XSD10_SCHEMAS_ENTITY = 26,
    XML_XSD10_SCHEMAS_ENTITIES = 27,
    XML_XSD10_SCHEMAS_NOTATION = 28,
    XML_XSD10_SCHEMAS_ANYURI = 29,
    XML_XSD10_SCHEMAS_INTEGER = 30,
    XML_XSD10_SCHEMAS_NPINTEGER = 31,
    XML_XSD10_SCHEMAS_NINTEGER = 32,
    XML_XSD10_SCHEMAS_NNINTEGER = 33,
    XML_XSD10_SCHEMAS_PINTEGER = 34,
    XML_XSD10_SCHEMAS_INT = 35,
    XML_XSD10_SCHEMAS_UINT = 36,
    XML_XSD10_SCHEMAS_LONG = 37,
    XML_XSD10_SCHEMAS_ULONG = 38,
    XML_XSD10_SCHEMAS_SHORT = 39,
    XML_XSD10_SCHEMAS_USHORT = 40,
    XML_XSD10_SCHEMAS_BYTE = 41,
    XML_XSD10_SCHEMAS_UBYTE = 42,
    XML_XSD10_SCHEMAS_HEXBINARY = 43,
    XML_XSD10_SCHEMAS_BASE64BINARY = 44,
    XML_XSD10_SCHEMAS_ANYTYPE = 45,
    XML_XSD10_SCHEMAS_ANYSIMPLETYPE = 46
} XmlXsd10ValType;

/** The kinds of schema components (libxml2 xmlSchemaTypeType). */
typedef enum {
    XML_XSD10_SCHEMA_TYPE_BASIC = 1, /* A built-in datatype */
    XML_XSD10_SCHEMA_TYPE_ANY,
    XML_XSD10_SCHEMA_TYPE_FACET,
    XML_XSD10_SCHEMA_TYPE_SIMPLE,
    XML_XSD10_SCHEMA_TYPE_COMPLEX,
    XML_XSD10_SCHEMA_TYPE_SEQUENCE = 6,
    XML_XSD10_SCHEMA_TYPE_CHOICE,
    XML_XSD10_SCHEMA_TYPE_ALL,
    XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT,
    XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT,
    XML_XSD10_SCHEMA_TYPE_UR,
    XML_XSD10_SCHEMA_TYPE_RESTRICTION,
    XML_XSD10_SCHEMA_TYPE_EXTENSION,
    XML_XSD10_SCHEMA_TYPE_ELEMENT,
    XML_XSD10_SCHEMA_TYPE_ATTRIBUTE,
    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP,
    XML_XSD10_SCHEMA_TYPE_GROUP,
    XML_XSD10_SCHEMA_TYPE_NOTATION,
    XML_XSD10_SCHEMA_TYPE_LIST,
    XML_XSD10_SCHEMA_TYPE_UNION,
    XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE,
    XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE,
    XML_XSD10_SCHEMA_TYPE_IDC_KEY,
    XML_XSD10_SCHEMA_TYPE_IDC_KEYREF,
    XML_XSD10_SCHEMA_TYPE_PARTICLE = 25,
    XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE,
    XML_XSD10_SCHEMA_FACET_MININCLUSIVE = 1000,
    XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE,
    XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE,
    XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE,
    XML_XSD10_SCHEMA_FACET_TOTALDIGITS,
    XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS,
    XML_XSD10_SCHEMA_FACET_PATTERN,
    XML_XSD10_SCHEMA_FACET_ENUMERATION,
    XML_XSD10_SCHEMA_FACET_WHITESPACE,
    XML_XSD10_SCHEMA_FACET_LENGTH,
    XML_XSD10_SCHEMA_FACET_MAXLENGTH,
    XML_XSD10_SCHEMA_FACET_MINLENGTH,
    XML_XSD10_SCHEMA_EXTRA_QNAMEREF = 2000,
    XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB
} XmlXsd10TypeType;

/** Schema content type (libxml2 xmlSchemaContentType). */
typedef enum {
    XML_XSD10_SCHEMA_CONTENT_UNKNOWN = 0,
    XML_XSD10_SCHEMA_CONTENT_EMPTY = 1,
    XML_XSD10_SCHEMA_CONTENT_ELEMENTS,
    XML_XSD10_SCHEMA_CONTENT_MIXED,
    XML_XSD10_SCHEMA_CONTENT_SIMPLE,
    XML_XSD10_SCHEMA_CONTENT_MIXED_OR_ELEMENTS, /* Obsolete */
    XML_XSD10_SCHEMA_CONTENT_BASIC,
    XML_XSD10_SCHEMA_CONTENT_ANY
} XmlXsd10ContentType;

/** The whitespace facet values (libxml2 xmlSchemaWhitespaceValueType). */
typedef enum {
    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN = 0,
    XML_XSD10_SCHEMA_WHITESPACE_PRESERVE = 1,
    XML_XSD10_SCHEMA_WHITESPACE_REPLACE = 2,
    XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE = 3
} XmlXsd10WhitespaceValueType;

/** The libxml2 error codes (xmlParserErrors) returned by the functions here. */
typedef enum {
    XML_XSD10_SCHEMAP_INVALID_FACET_VALUE = 1717,
    XML_XSD10_SCHEMAP_REGEXP_INVALID = 1756,
    XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1 = 1824,
    XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2 = 1825,
    XML_XSD10_SCHEMAV_CVC_LENGTH_VALID = 1830,
    XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID = 1831,
    XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID = 1832,
    XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID = 1833,
    XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID = 1834,
    XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID = 1835,
    XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID = 1836,
    XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID = 1837,
    XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID = 1838,
    XML_XSD10_SCHEMAV_CVC_PATTERN_VALID = 1839,
    XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID = 1840
} XmlXsd10TypesError;

/** The attribute types of libxml2 (xmlAttributeType), as recorded in an XmlXsd10Ids. */
typedef enum {
    XML_XSD10_ATTRIBUTE_CDATA = 1,
    XML_XSD10_ATTRIBUTE_ID,
    XML_XSD10_ATTRIBUTE_IDREF,
    XML_XSD10_ATTRIBUTE_IDREFS,
    XML_XSD10_ATTRIBUTE_ENTITY,
    XML_XSD10_ATTRIBUTE_ENTITIES,
    XML_XSD10_ATTRIBUTE_NMTOKEN,
    XML_XSD10_ATTRIBUTE_NMTOKENS,
    XML_XSD10_ATTRIBUTE_ENUMERATION,
    XML_XSD10_ATTRIBUTE_NOTATION
} XmlXsd10AttributeType;

/** A computed value (libxml2 xmlSchemaVal), opaque. */
struct XmlXsd10Val;
typedef struct XmlXsd10Val XmlXsd10Val;

typedef struct XmlXsd10Type XmlXsd10Type;
typedef struct XmlXsd10Facet XmlXsd10Facet;
typedef struct XmlXsd10Annot XmlXsd10Annot;
typedef struct XmlXsd10Attribute XmlXsd10Attribute;
typedef struct XmlXsd10AttributeLink XmlXsd10AttributeLink;
typedef struct XmlXsd10WildcardNs XmlXsd10WildcardNs;
typedef struct XmlXsd10Wildcard XmlXsd10Wildcard;
typedef struct XmlXsd10AttributeGroup XmlXsd10AttributeGroup;
typedef struct XmlXsd10TypeLink XmlXsd10TypeLink;
typedef struct XmlXsd10FacetLink XmlXsd10FacetLink;
typedef struct XmlXsd10Element XmlXsd10Element;
typedef struct XmlXsd10Notation XmlXsd10Notation;
typedef struct XmlXsd10TreeItem XmlXsd10TreeItem;
typedef struct XmlXsd10Particle XmlXsd10Particle;
typedef struct XmlXsd10ModelGroup XmlXsd10ModelGroup;

/** Annotation (libxml2 xmlSchemaAnnot). */
struct XmlXsd10Annot {
    struct XmlXsd10Annot *next;
    XMQNode *content;         /* the annotation */
};

/* Skip unknown attribute from validation. Obsolete, not used anymore. */
#define XML_XSD10_SCHEMAS_ANYATTR_SKIP 1
/* Ignore validation non definition on attributes. Obsolete, not used anymore. */
#define XML_XSD10_SCHEMAS_ANYATTR_LAX 2
/* Apply strict validation rules on attributes. Obsolete, not used anymore. */
#define XML_XSD10_SCHEMAS_ANYATTR_STRICT 3
/* Skip unknown attribute from validation */
#define XML_XSD10_SCHEMAS_ANY_SKIP 1
/* Used by wildcards. Validate if type found, don't worry if not found */
#define XML_XSD10_SCHEMAS_ANY_LAX 2
/* Used by wildcards. Apply strict validation rules */
#define XML_XSD10_SCHEMAS_ANY_STRICT 3
/* Used by wildcards. The attribute is prohibited. */
#define XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED 0
/* The attribute is required. */
#define XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED 1
/* The attribute is optional. */
#define XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL 2
/* allow elements in no namespace */
#define XML_XSD10_SCHEMAS_ATTR_GLOBAL (1 << 0)
/* allow elements in no namespace */
#define XML_XSD10_SCHEMAS_ATTR_NSDEFAULT (1 << 7)
/* this is set when the "type" and "ref" references have been resolved. */
#define XML_XSD10_SCHEMAS_ATTR_INTERNAL_RESOLVED (1 << 8)
/* the attribute has a fixed value */
#define XML_XSD10_SCHEMAS_ATTR_FIXED (1 << 9)

/** An attribute definition (libxml2 xmlSchemaAttribute). */
struct XmlXsd10Attribute {
    XmlXsd10TypeType type;
    struct XmlXsd10Attribute *next; /* the next attribute (not used?) */
    const char *name; /* the name of the declaration */
    const char *id; /* Deprecated; not used */
    const char *ref; /* Deprecated; not used */
    const char *refNs; /* Deprecated; not used */
    const char *typeName; /* the local name of the type definition */
    const char *typeNs; /* the ns URI of the type definition */
    XmlXsd10Annot *annot;

    XmlXsd10Type *base; /* Deprecated; not used */
    int occurs; /* Deprecated; not used */
    const char *defValue; /* The initial value of the value constraint */
    XmlXsd10Type *subtypes; /* the type definition */
    XMQNode *node;
    const char *targetNamespace;
    int flags;
    const char *refPrefix; /* Deprecated; not used */
    XmlXsd10Val *defVal; /* The compiled value constraint */
    XmlXsd10Attribute *refDecl; /* Deprecated; not used */
};

/** Used to build a list of attribute uses on complexType definitions (libxml2 xmlSchemaAttributeLink).
    WARNING: Deprecated; not used. */
struct XmlXsd10AttributeLink {
    struct XmlXsd10AttributeLink *next;/* the next attribute link ... */
    struct XmlXsd10Attribute *attr;/* the linked attribute */
};

/* If the wildcard is complete. */
#define XML_XSD10_SCHEMAS_WILDCARD_COMPLETE (1 << 0)

/** Used to build a list of namespaces on wildcards (libxml2 xmlSchemaWildcardNs). */
struct XmlXsd10WildcardNs {
    struct XmlXsd10WildcardNs *next;/* the next constraint link ... */
    const char *value;/* the value */
};

/** A wildcard (libxml2 xmlSchemaWildcard). */
struct XmlXsd10Wildcard {
    XmlXsd10TypeType type;        /* The kind of type */
    const char *id; /* Deprecated; not used */
    XmlXsd10Annot *annot;
    XMQNode *node;
    int minOccurs; /* Deprecated; not used */
    int maxOccurs; /* Deprecated; not used */
    int processContents;
    int any; /* Indicates if the ns constraint is of ##any */
    XmlXsd10WildcardNs *nsSet; /* The list of allowed namespaces */
    XmlXsd10WildcardNs *negNsSet; /* The negated namespace */
    int flags;
};

/* The attribute wildcard has been built. */
#define XML_XSD10_SCHEMAS_ATTRGROUP_WILDCARD_BUILDED (1 << 0)
/* The attribute group has been defined. */
#define XML_XSD10_SCHEMAS_ATTRGROUP_GLOBAL (1 << 1)
/* Marks the attr group as marked; used for circular checks. */
#define XML_XSD10_SCHEMAS_ATTRGROUP_MARKED (1 << 2)
/* The attr group was redefined. */
#define XML_XSD10_SCHEMAS_ATTRGROUP_REDEFINED (1 << 3)
/* Whether this attr. group contains attr. group references. */
#define XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS (1 << 4)

/** An attribute group definition (libxml2 xmlSchemaAttributeGroup).
    XmlXsd10Attribute and XmlXsd10AttributeGroup start of structures must be kept similar */
struct XmlXsd10AttributeGroup {
    XmlXsd10TypeType type;        /* The kind of type */
    struct XmlXsd10Attribute *next;/* the next attribute if in a group ... */
    const char *name;
    const char *id;
    const char *ref; /* Deprecated; not used */
    const char *refNs; /* Deprecated; not used */
    XmlXsd10Annot *annot;

    XmlXsd10Attribute *attributes; /* Deprecated; not used */
    XMQNode *node;
    int flags;
    XmlXsd10Wildcard *attributeWildcard;
    const char *refPrefix; /* Deprecated; not used */
    XmlXsd10AttributeGroup *refItem; /* Deprecated; not used */
    const char *targetNamespace;
    void *attrUses;
};

/** Used to build a list of types, e.g. member types of simpleType with variety "union"
    (libxml2 xmlSchemaTypeLink). */
struct XmlXsd10TypeLink {
    struct XmlXsd10TypeLink *next;/* the next type link ... */
    XmlXsd10Type *type;/* the linked type */
};

/** Used to build a list of facets (libxml2 xmlSchemaFacetLink). */
struct XmlXsd10FacetLink {
    struct XmlXsd10FacetLink *next;/* the next facet link ... */
    XmlXsd10Facet *facet;/* the linked facet */
};

/* the element content type is mixed */
#define XML_XSD10_SCHEMAS_TYPE_MIXED (1 << 0)
/* the simple or complex type has a derivation method of "extension". */
#define XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION (1 << 1)
/* the simple or complex type has a derivation method of "restriction". */
#define XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION (1 << 2)
/* the type is global */
#define XML_XSD10_SCHEMAS_TYPE_GLOBAL (1 << 3)
/* the complexType owns an attribute wildcard, i.e. it can be freed by the complexType */
#define XML_XSD10_SCHEMAS_TYPE_OWNED_ATTR_WILDCARD (1 << 4) /* Obsolete. */
/* the simpleType has a variety of "absent". TODO: Actually not necessary :-/, since if
   none of the variety flags occur then it's automatically absent. */
#define XML_XSD10_SCHEMAS_TYPE_VARIETY_ABSENT (1 << 5)
/* the simpleType has a variety of "list". */
#define XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST (1 << 6)
/* the simpleType has a variety of "union". */
#define XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION (1 << 7)
/* the simpleType has a variety of "union". */
#define XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC (1 << 8)
/* the complexType has a final of "extension". */
#define XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION (1 << 9)
/* the simpleType/complexType has a final of "restriction". */
#define XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION (1 << 10)
/* the simpleType has a final of "list". */
#define XML_XSD10_SCHEMAS_TYPE_FINAL_LIST (1 << 11)
/* the simpleType has a final of "union". */
#define XML_XSD10_SCHEMAS_TYPE_FINAL_UNION (1 << 12)
/* the simpleType has a final of "default". */
#define XML_XSD10_SCHEMAS_TYPE_FINAL_DEFAULT (1 << 13)
/* Marks the item as a builtin primitive. */
#define XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE (1 << 14)
/* Marks the item as marked; used for circular checks. */
#define XML_XSD10_SCHEMAS_TYPE_MARKED (1 << 16)
/* the complexType did not specify 'block' so use the default of the <schema> item. */
#define XML_XSD10_SCHEMAS_TYPE_BLOCK_DEFAULT (1 << 17)
/* the complexType has a 'block' of "extension". */
#define XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION (1 << 18)
/* the complexType has a 'block' of "restriction". */
#define XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION (1 << 19)
/* the simple/complexType is abstract. */
#define XML_XSD10_SCHEMAS_TYPE_ABSTRACT (1 << 20)
/* indicates if the facets need a computed value */
#define XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE (1 << 21)
/* indicates that the type was typefixed */
#define XML_XSD10_SCHEMAS_TYPE_INTERNAL_RESOLVED (1 << 22)
/* indicates that the type is invalid */
#define XML_XSD10_SCHEMAS_TYPE_INTERNAL_INVALID (1 << 23)
/* a whitespace-facet value of "preserve" */
#define XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE (1 << 24)
/* a whitespace-facet value of "replace" */
#define XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE (1 << 25)
/* a whitespace-facet value of "collapse" */
#define XML_XSD10_SCHEMAS_TYPE_WHITESPACE_COLLAPSE (1 << 26)
/* has facets */
#define XML_XSD10_SCHEMAS_TYPE_HAS_FACETS (1 << 27)
/* indicates if the facets (pattern) need a normalized value */
#define XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED (1 << 28)
/* First stage of fixup was done. */
#define XML_XSD10_SCHEMAS_TYPE_FIXUP_1 (1 << 29)
/* The type was redefined. */
#define XML_XSD10_SCHEMAS_TYPE_REDEFINED (1 << 30)

/** Schemas type definition (libxml2 xmlSchemaType). */
struct XmlXsd10Type {
    XmlXsd10TypeType type; /* The kind of type */
    struct XmlXsd10Type *next; /* the next type if in a sequence ... */
    const char *name;
    const char *id ; /* Deprecated; not used */
    const char *ref; /* Deprecated; not used */
    const char *refNs; /* Deprecated; not used */
    XmlXsd10Annot *annot;
    XmlXsd10Type *subtypes;
    XmlXsd10Attribute *attributes; /* Deprecated; not used */
    XMQNode *node;
    int minOccurs; /* Deprecated; not used */
    int maxOccurs; /* Deprecated; not used */

    int flags;
    XmlXsd10ContentType contentType;
    const char *base; /* Base type's local name */
    const char *baseNs; /* Base type's target namespace */
    XmlXsd10Type *baseType; /* The base type component */
    XmlXsd10Facet *facets; /* Local facets */
    struct XmlXsd10Type *redef; /* Deprecated; not used */
    int recurse; /* Obsolete */
    XmlXsd10AttributeLink **attributeUses; /* Deprecated; not used */
    XmlXsd10Wildcard *attributeWildcard;
    int builtInType; /* Type of built-in types. */
    XmlXsd10TypeLink *memberTypes; /* member-types if a union type. */
    XmlXsd10FacetLink *facetSet; /* All facets (incl. inherited) */
    const char *refPrefix; /* Deprecated; not used */
    XmlXsd10Type *contentTypeDef; /* Used for the simple content of complex types.
                                     Could we use @subtypes for this? */
    void *contModel; /* Holds the automaton of the content model (a compiled regexp) */
    const char *targetNamespace;
    void *attrUses;
};

/* the element is nillable */
#define XML_XSD10_SCHEMAS_ELEM_NILLABLE (1 << 0)
/* the element is global */
#define XML_XSD10_SCHEMAS_ELEM_GLOBAL (1 << 1)
/* the element has a default value */
#define XML_XSD10_SCHEMAS_ELEM_DEFAULT (1 << 2)
/* the element has a fixed value */
#define XML_XSD10_SCHEMAS_ELEM_FIXED (1 << 3)
/* the element is abstract */
#define XML_XSD10_SCHEMAS_ELEM_ABSTRACT (1 << 4)
/* the element is top level. obsolete: use XML_XSD10_SCHEMAS_ELEM_GLOBAL instead */
#define XML_XSD10_SCHEMAS_ELEM_TOPLEVEL (1 << 5)
/* the element is a reference to a type */
#define XML_XSD10_SCHEMAS_ELEM_REF (1 << 6)
/* allow elements in no namespace. Obsolete, not used anymore. */
#define XML_XSD10_SCHEMAS_ELEM_NSDEFAULT (1 << 7)
/* this is set when "type", "ref", "substitutionGroup" references have been resolved. */
#define XML_XSD10_SCHEMAS_ELEM_INTERNAL_RESOLVED (1 << 8)
/* a helper flag for the search of circular references. */
#define XML_XSD10_SCHEMAS_ELEM_CIRCULAR (1 << 9)
/* the "block" attribute is absent */
#define XML_XSD10_SCHEMAS_ELEM_BLOCK_ABSENT (1 << 10)
/* disallowed substitutions are absent */
#define XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION (1 << 11)
/* disallowed substitutions: "restriction" */
#define XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION (1 << 12)
/* disallowed substitutions: "substitution" */
#define XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION (1 << 13)
/* substitution group exclusions are absent */
#define XML_XSD10_SCHEMAS_ELEM_FINAL_ABSENT (1 << 14)
/* substitution group exclusions: "extension" */
#define XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION (1 << 15)
/* substitution group exclusions: "restriction" */
#define XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION (1 << 16)
/* the declaration is a substitution group head */
#define XML_XSD10_SCHEMAS_ELEM_SUBST_GROUP_HEAD (1 << 17)
/* this is set when the elem decl has been checked against all constraints */
#define XML_XSD10_SCHEMAS_ELEM_INTERNAL_CHECKED (1 << 18)

/** An element definition (libxml2 xmlSchemaElement).
    XmlXsd10Type, XmlXsd10Facet and XmlXsd10Element start of structures must be kept similar */
struct XmlXsd10Element {
    XmlXsd10TypeType type; /* The kind of type */
    struct XmlXsd10Type *next; /* Not used? */
    const char *name;
    const char *id; /* Deprecated; not used */
    const char *ref; /* Deprecated; not used */
    const char *refNs; /* Deprecated; not used */
    XmlXsd10Annot *annot;
    XmlXsd10Type *subtypes; /* the type definition */
    XmlXsd10Attribute *attributes;
    XMQNode *node;
    int minOccurs; /* Deprecated; not used */
    int maxOccurs; /* Deprecated; not used */

    int flags;
    const char *targetNamespace;
    const char *namedType;
    const char *namedTypeNs;
    const char *substGroup;
    const char *substGroupNs;
    const char *scope;
    const char *value; /* The original value of the value constraint. */
    struct XmlXsd10Element *refDecl; /* This will now be used for the
                                        substitution group affiliation */
    void *contModel; /* Obsolete for WXS, maybe used for RelaxNG */
    XmlXsd10ContentType contentType;
    const char *refPrefix; /* Deprecated; not used */
    XmlXsd10Val *defVal; /* The compiled value constraint. */
    void *idcs; /* The identity-constraint defs */
};

/* unknown facet handling */
#define XML_XSD10_SCHEMAS_FACET_UNKNOWN 0
/* preserve the type of the facet */
#define XML_XSD10_SCHEMAS_FACET_PRESERVE 1
/* replace the type of the facet */
#define XML_XSD10_SCHEMAS_FACET_REPLACE 2
/* collapse the types of the facet */
#define XML_XSD10_SCHEMAS_FACET_COLLAPSE 3

/** A facet definition (libxml2 xmlSchemaFacet). */
struct XmlXsd10Facet {
    XmlXsd10TypeType type;        /* The kind of type */
    struct XmlXsd10Facet *next;/* the next type if in a sequence ... */
    const char *value; /* The original value (owned by the creator of the facet) */
    const char *id; /* Obsolete */
    XmlXsd10Annot *annot;
    XMQNode *node;
    int fixed; /* XML_XSD10_SCHEMAS_FACET_PRESERVE, etc. */
    int whitespace;
    XmlXsd10Val *val; /* The compiled value */
    void *regexp; /* The regex for patterns (from the regexp hooks) */
};

/** A notation definition (libxml2 xmlSchemaNotation). */
struct XmlXsd10Notation {
    XmlXsd10TypeType type; /* The kind of type */
    const char *name;
    XmlXsd10Annot *annot;
    const char *identifier;
    const char *targetNamespace;
};

/* The tree items of the content models (in libxml2 duplicated in xmlschemastypes.c and xmlschemas.c). */
#define XML_XSD10_UNBOUNDED (1 << 30)

/** libxml2 xmlSchemaTreeItem. */
struct XmlXsd10TreeItem {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
    XmlXsd10TreeItem *next;
    XmlXsd10TreeItem *children;
};

/** libxml2 xmlSchemaParticle. */
struct XmlXsd10Particle {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
    XmlXsd10TreeItem *next;
    XmlXsd10TreeItem *children;
    int minOccurs;
    int maxOccurs;
    XMQNode *node;
};

/** libxml2 xmlSchemaModelGroup. */
struct XmlXsd10ModelGroup {
    XmlXsd10TypeType type;
    XmlXsd10Annot *annot;
    XmlXsd10TreeItem *next;
    XmlXsd10TreeItem *children;
    XMQNode *node;
};

/** The ID table of a document: replaces libxml2 doc->ids, doc->refs and attr->atype. */
struct XmlXsd10Ids;
typedef struct XmlXsd10Ids XmlXsd10Ids;

/** Regular expressions for pattern facets (libxml2 xmlRegexpCompile, xmlRegexpExec, xmlRegFreeRegexp). */
typedef struct XmlXsd10RegexpHooks
{
    /** Compile an XML Schema regular expression, NULL if invalid. */
    void *(*compile)(const char *regexp);
    /** 1 if value matches, 0 if not, negative on errors. */
    int (*exec)(void *regexp, const char *value);
    void (*free)(void *regexp);
} XmlXsd10RegexpHooks;

/** Set the regexp implementation, before any pattern facet is used (NULL removes it,
    pattern facets are then invalid). Not thread safe, set it once at startup. */
void xml_xsd10_set_regexp_hooks(const XmlXsd10RegexpHooks *hooks);

// Built in types //////////////////////////////////////////////////

/** Port of xmlSchemaInitTypes: the types are static, always returns 0. */
int xml_xsd10_init_types(void);
/** Port of xmlSchemaCleanupTypes: nothing to free. */
void xml_xsd10_cleanup_types(void);
/** Port of xmlSchemaGetPredefinedType: lookup by name and namespace (the xsd namespace). */
XmlXsd10Type *xml_xsd10_get_predefined_type(const char *name, const char *ns);
/** Port of xmlSchemaGetBuiltInType. */
XmlXsd10Type *xml_xsd10_get_built_in_type(XmlXsd10ValType type);
/** Port of xmlSchemaGetBuiltInListSimpleTypeItemType. */
XmlXsd10Type *xml_xsd10_get_built_in_list_simple_type_item_type(XmlXsd10Type *type);
/** Port of xmlSchemaIsBuiltInTypeFacet: 1 if the facet can be used with the built in type,
    0 otherwise and -1 if the type is not a built in type. */
int xml_xsd10_is_built_in_type_facet(XmlXsd10Type *type, int facetType);

// Values /////////////////////////////////////////////////////////

/** Port of xmlSchemaValidatePredefinedType: 0 if the value validates (*val gets the value if val is
    not NULL), a positive error code otherwise and -1 on internal or API errors. */
int xml_xsd10_validate_predefined_type(XmlXsd10Type *type, const char *value, XmlXsd10Val **val);
/** Port of xmlSchemaValPredefTypeNode: node is used for QName/NOTATION namespace resolution and
    ENTITY/NOTATION lookup in the dtd. */
int xml_xsd10_val_predef_type_node(XmlXsd10Type *type, const char *value, XmlXsd10Val **val, XMQNode *node);
/** Port of xmlSchemaValPredefTypeNodeNoNorm: the value is not whitespace normalized. */
int xml_xsd10_val_predef_type_node_no_norm(XmlXsd10Type *type, const char *value, XmlXsd10Val **val, XMQNode *node);
/** Like xml_xsd10_val_predef_type_node, ID/IDREF values of attribute nodes are registered in ids
    (like libxml2 does in the document) when ids is not NULL. */
int xml_xsd10_val_predef_type_node_ids(XmlXsd10Type *type, const char *value, XmlXsd10Val **val,
                                       XMQNode *node, XmlXsd10Ids *ids);
int xml_xsd10_val_predef_type_node_no_norm_ids(XmlXsd10Type *type, const char *value, XmlXsd10Val **val,
                                               XMQNode *node, XmlXsd10Ids *ids);

/** Port of xmlSchemaFreeValue (frees the whole list of values). */
void xml_xsd10_free_value(XmlXsd10Val *val);
/** Port of xmlSchemaCopyValue: a deep copy, NULL for list types. */
XmlXsd10Val *xml_xsd10_copy_value(XmlXsd10Val *val);
/** Port of xmlSchemaGetValType. */
XmlXsd10ValType xml_xsd10_get_val_type(XmlXsd10Val *val);
/** Port of xmlSchemaValueAppend: 0 or -1 on API errors. */
int xml_xsd10_value_append(XmlXsd10Val *prev, XmlXsd10Val *cur);
/** Port of xmlSchemaValueGetNext. */
XmlXsd10Val *xml_xsd10_value_get_next(XmlXsd10Val *cur);
/** Port of xmlSchemaValueGetAsString: the string of string based values, NULL for others. */
const char *xml_xsd10_value_get_as_string(XmlXsd10Val *val);
/** Port of xmlSchemaValueGetAsBoolean. */
int xml_xsd10_value_get_as_boolean(XmlXsd10Val *val);
/** Port of xmlSchemaNewStringValue: only XML_XSD10_SCHEMAS_STRING, the malloced value is consumed. */
XmlXsd10Val *xml_xsd10_new_string_value(XmlXsd10ValType type, const char *value);
/** Port of xmlSchemaNewNOTATIONValue: the malloced strings are consumed. */
XmlXsd10Val *xml_xsd10_new_notation_value(const char *name, const char *ns);
/** Port of xmlSchemaNewQNameValue: the malloced strings are consumed. */
XmlXsd10Val *xml_xsd10_new_qname_value(const char *namespaceName, const char *localName);

/** Port of xmlSchemaCompareValues: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y (indeterminate
    or incomparable) and -2 on errors (different value spaces). */
int xml_xsd10_compare_values(XmlXsd10Val *x, XmlXsd10Val *y);
/** Port of xmlSchemaCompareValuesWhtsp. */
int xml_xsd10_compare_values_whtsp(XmlXsd10Val *x, XmlXsd10WhitespaceValueType xws,
                                   XmlXsd10Val *y, XmlXsd10WhitespaceValueType yws);

/** Port of xmlSchemaGetCanonValue: *retValue is malloced (free it), returns 0, 1 if the type is not
    supported (*retValue is then "???") and -1 on API errors. */
int xml_xsd10_get_canon_value(XmlXsd10Val *val, const char **retValue);
/** Port of xmlSchemaGetCanonValueWhtsp. */
int xml_xsd10_get_canon_value_whtsp(XmlXsd10Val *val, const char **retValue, XmlXsd10WhitespaceValueType ws);

/** Port of xmlSchemaWhiteSpaceReplace: a malloced copy with tab/cr/nl replaced by spaces, NULL if no change was needed. */
char *xml_xsd10_white_space_replace(const char *value);
/** Port of xmlSchemaCollapseString: a malloced collapsed copy, NULL if no change was needed. */
char *xml_xsd10_collapse_string(const char *value);

// Facets /////////////////////////////////////////////////////////

/** Port of xmlSchemaNewFacet. */
XmlXsd10Facet *xml_xsd10_new_facet(void);
/** Port of xmlSchemaFreeFacet: frees val, regexp and annotations (not the value string). */
void xml_xsd10_free_facet(XmlXsd10Facet *facet);
/** Port of xmlSchemaCheckFacet without a parser context: computes facet->val (and the regexp,
    the whitespace) from facet->value. 0 if valid, a positive error code if not and -1 on internal
    errors or when the base type of typeDecl is not a built in type. */
int xml_xsd10_check_facet(XmlXsd10Facet *facet, XmlXsd10Type *typeDecl, const char *name);
/** Port of xmlSchemaValidateFacet: 0 if valid, a positive error code otherwise, -1 on errors. */
int xml_xsd10_validate_facet(XmlXsd10Type *base, XmlXsd10Facet *facet, const char *value, XmlXsd10Val *val);
/** Port of xmlSchemaValidateFacetWhtsp. */
int xml_xsd10_validate_facet_whtsp(XmlXsd10Facet *facet, XmlXsd10WhitespaceValueType fws,
                                   XmlXsd10ValType valType, const char *value, XmlXsd10Val *val,
                                   XmlXsd10WhitespaceValueType ws);
/** Port of xmlSchemaValidateLengthFacet. */
int xml_xsd10_validate_length_facet(XmlXsd10Type *type, XmlXsd10Facet *facet, const char *value,
                                    XmlXsd10Val *val, unsigned long *length);
/** Port of xmlSchemaValidateLengthFacetWhtsp. */
int xml_xsd10_validate_length_facet_whtsp(XmlXsd10Facet *facet, XmlXsd10ValType valType, const char *value,
                                          XmlXsd10Val *val, unsigned long *length, XmlXsd10WhitespaceValueType ws);
/** Port of xmlSchemaValidateListSimpleTypeFacet. */
int xml_xsd10_validate_list_simple_type_facet(XmlXsd10Facet *facet, const char *value,
                                              unsigned long actualLen, unsigned long *expectedLen);
/** Port of xmlSchemaGetFacetValueAsULong. */
unsigned long xml_xsd10_get_facet_value_as_ulong(XmlXsd10Facet *facet);

// ID table ///////////////////////////////////////////////////////

XmlXsd10Ids *xml_xsd10_ids_new(void);
void xml_xsd10_ids_free(XmlXsd10Ids *ids);
/** Port of xmlAddIDSafe: 1 if added, 0 if the id is empty or already defined, -1 on errors.
    The attribute gets the type XML_XSD10_ATTRIBUTE_ID. */
int xml_xsd10_ids_add_id(XmlXsd10Ids *ids, XMQNode *attr, const char *value);
/** The attribute holding the id, or NULL (libxml2 xmlGetID). */
XMQNode *xml_xsd10_ids_lookup(XmlXsd10Ids *ids, const char *value);
/** The recorded type (XmlXsd10AttributeType) of the attribute, 0 if none (libxml2 attr->atype). */
int xml_xsd10_ids_attr_type(XmlXsd10Ids *ids, const XMQNode *attr);
/** Record the type of an attribute (for example declared as ID in a dtd). */
void xml_xsd10_ids_set_attr_type(XmlXsd10Ids *ids, const XMQNode *attr, int atype);
/** Port of xmlAddRef: remember that attr refers to the id value. */
void xml_xsd10_ids_add_ref(XmlXsd10Ids *ids, XMQNode *attr, const char *value);
/** The references in the order added. */
size_t xml_xsd10_ids_num_refs(XmlXsd10Ids *ids);
const char *xml_xsd10_ids_ref_value(XmlXsd10Ids *ids, size_t i);
XMQNode *xml_xsd10_ids_ref_attr(XmlXsd10Ids *ids, size_t i);

#ifndef XMQ_NO_XSD
#define XML_XSD10_TYPES_MODULE
#endif

#endif // XML_XSD10_TYPES_H
