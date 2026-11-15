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

/* Portions of this file are ported from libxml2 (xmlschemas.c, and the helpers
   it uses from error.c, xmlstring.c, tree.c, dict.c and hash.c, version 2.16.0
   as found in the libxmq fork in 3rdparty/libxml2).
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

/* Schema parsing (port of the parsing functions of libxml2 xmlschemas.c: parsing,
   utilities for parsing, schema extraction from an infoset, reading schemas). */

#ifndef BUILDING_DIST_XMQ

#define XML_XSD10_INTERNALS
#include"xml_xsd10_schemas.h"
#include"xml_xsd10_parse.h"
#include"xml_dom.h"
#include"xml_parser.h"
#include"xml_regexp.h"
#include"xml_pattern.h"
#include"xml_uri.h"
#include"xml_xsd10_types.h"

#include<errno.h>
#include<limits.h>
#include<stdarg.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_XSD10_PARSE_MODULE

/************************************************************************
 *									*
 *			Parsing functions				*
 *									*
 ************************************************************************/

#define XSD10_ENABLE_REDEFINE

#define XSD10_IS_BLANK_CH(c) (((c) == 0x20) || ((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd))

#define XSD10_IS_BLANK_NODE(n)						\
    ((xml_node_kind(n) == XML_DOM_TEXT) && (xml_xsd10_is_blank((char *) xml_node_content(n), -1)))

/************************************************************************
 *									*
 *		Skipping the nodes removed by xmlSchemaCleanupDoc	*
 *									*
 ************************************************************************/

/*
 * libxml2 xmlSchemaCleanupDoc unlinks and frees the blank text nodes, comments,
 * processing instructions and entity references of a schema document before it
 * is parsed (it does not descend into an element whose first child is an entity
 * reference or declaration). The native port never modifies the schema documents,
 * so the parsing functions walk the children with xsd10_parse_first_child and
 * xsd10_parse_next, which skip exactly the nodes that libxml2 would have removed.
 */

/* 1 if xmlSchemaCleanupDoc visits node: no ancestor element has an entity reference or
   declaration as first child. */
static int xsd10_parse_cleanup_visits(XMQNode *node)
{
    XMQNode *p;

    for (p = xml_node_parent(node); (p != NULL) && (xml_node_kind(p) == XML_DOM_ELEMENT);
         p = xml_node_parent(p)) {
        XMQNode *first = xml_node_first_child(p);
        if ((first != NULL) &&
            ((xml_node_kind(first) == XML_DOM_ENTITY_DECL) ||
             (xml_node_kind(first) == XML_DOM_ENTITY_REF)))
            return(0);
    }
    return(1);
}

/* 1 if xmlSchemaCleanupDoc removes node. */
static int xsd10_parse_removed_by_cleanup(XMQNode *cur)
{
    if (xml_node_kind(cur) == XML_DOM_TEXT) {
        if (XSD10_IS_BLANK_NODE(cur)) {
            if (xml_xsd10_tree_node_get_space_preserve(cur) != 1)
                return(xsd10_parse_cleanup_visits(cur));
        }
        return(0);
    } else if ((xml_node_kind(cur) != XML_DOM_ELEMENT) &&
               (xml_node_kind(cur) != XML_DOM_CDATA)) {
        return(xsd10_parse_cleanup_visits(cur));
    }
    return(0);
}

/* The first child that xmlSchemaCleanupDoc keeps (libxml2 node->children after the cleanup). */
static XMQNode *xsd10_parse_first_child(XMQNode *node)
{
    XMQNode *cur;

    if (node == NULL)
        return(NULL);
    cur = xml_node_first_child(node);
    while ((cur != NULL) && xsd10_parse_removed_by_cleanup(cur))
        cur = xml_node_next(cur);
    return(cur);
}

/* The next sibling that xmlSchemaCleanupDoc keeps (libxml2 node->next after the cleanup). */
static XMQNode *xsd10_parse_next(XMQNode *node)
{
    XMQNode *cur;

    if (node == NULL)
        return(NULL);
    cur = xml_node_next(node);
    while ((cur != NULL) && xsd10_parse_removed_by_cleanup(cur))
        cur = xml_node_next(cur);
    return(cur);
}

/* The first attribute of an element as a node (libxml2 node->properties). */
static XMQNode *xsd10_parse_first_attr(XMQNode *node)
{
    if ((node == NULL) || (xml_node_kind(node) != XML_DOM_ELEMENT))
        return(NULL);
    return(xml_attr_node(xml_node_first_attr(node)));
}

/* The native macro gets the type of ret (for the casts from void *). */
#define XSD10_WXS_FIND_GLOBAL_ITEM(slot, rtype)			\
    if (xml_xsd10_str_equal(nsName, schema->targetNamespace)) { \
	ret = (rtype) xml_xsd10_hash_lookup(schema->slot, name); \
	if (ret != NULL) goto exit; \
    } \
    if (xml_xsd10_hash_size(schema->schemasImports) > 1) { \
	XmlXsd10Import *import; \
	if (nsName == NULL) \
	    import = (XmlXsd10Import *) xml_xsd10_hash_lookup(schema->schemasImports, \
		XML_XSD10_SCHEMAS_NO_NAMESPACE); \
	else \
	    import = (XmlXsd10Import *) xml_xsd10_hash_lookup(schema->schemasImports, nsName); \
	if (import == NULL) \
	    goto exit; \
	ret = (rtype) xml_xsd10_hash_lookup(import->schema->slot, name); \
    }

/**
 * Port of xmlSchemaGetElem.
 *
 * Lookup a global element declaration in the schema.
 *
 * @param schema  the schema context
 * @param name  the element name
 * @param nsName  the element namespace
 * @returns the element declaration or NULL if not found.
 */
XmlXsd10Element *xml_xsd10_get_elem(XmlXsd10Schema *schema, const char *name,
                 const char *nsName)
{
    XmlXsd10Element *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return(NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(elemDecl, XmlXsd10Element *)
    }
exit:
    return (ret);
}

/**
 * Port of xmlSchemaGetType.
 *
 * Lookup a type in the schemas or the predefined types
 *
 * @param schema  the main schema
 * @param name  the type's name
 * @param nsName  the type's namespace
 * @returns the group definition or NULL if not found.
 */
XmlXsd10Type *xml_xsd10_get_type(XmlXsd10Schema *schema, const char *name,
                 const char *nsName)
{
    XmlXsd10Type *ret = NULL;

    if (name == NULL)
        return (NULL);
    /* First try the built-in types. */
    if ((nsName != NULL) && xml_xsd10_str_equal(nsName, xml_xsd10_ns)) {
	ret = xml_xsd10_get_predefined_type(name, nsName);
	if (ret != NULL)
	    goto exit;
	/*
	* Note that we try the parsed schemas as well here
	* since one might have parsed the S4S, which contain more
	* than the built-in types.
	* TODO: Can we optimize this?
	*/
    }
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(typeDecl, XmlXsd10Type *)
    }
exit:

    return (ret);
}

/**
 * Port of xmlSchemaGetAttributeDecl.
 *
 * Lookup a an attribute in the schema or imported schemas
 *
 * @param schema  the context of the schema
 * @param name  the name of the attribute
 * @param nsName  the target namespace of the attribute
 * @returns the attribute declaration or NULL if not found.
 */
XmlXsd10Attribute *xml_xsd10_get_attribute_decl(XmlXsd10Schema *schema, const char *name,
                 const char *nsName)
{
    XmlXsd10Attribute *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return (NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(attrDecl, XmlXsd10Attribute *)
    }
exit:
    return (ret);
}

/**
 * Port of xmlSchemaGetAttributeGroup.
 *
 * Lookup a an attribute group in the schema or imported schemas
 *
 * @param schema  the context of the schema
 * @param name  the name of the attribute group
 * @param nsName  the target namespace of the attribute group
 * @returns the attribute group definition or NULL if not found.
 */
XmlXsd10AttributeGroup *xml_xsd10_get_attribute_group(XmlXsd10Schema *schema, const char *name,
                 const char *nsName)
{
    XmlXsd10AttributeGroup *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return (NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(attrgrpDecl, XmlXsd10AttributeGroup *)
    }
exit:
    /* TODO:
    if ((ret != NULL) && (ret->redef != NULL)) {
	* Return the last redefinition. *
	ret = ret->redef;
    }
    */
    return (ret);
}

/**
 * Port of xmlSchemaGetGroup.
 *
 * Lookup a group in the schema or imported schemas
 *
 * @param schema  the context of the schema
 * @param name  the name of the group
 * @param nsName  the target namespace of the group
 * @returns the group definition or NULL if not found.
 */
XmlXsd10ModelGroupDef *xml_xsd10_get_group(XmlXsd10Schema *schema, const char *name,
                 const char *nsName)
{
    XmlXsd10ModelGroupDef *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return (NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(groupDecl, XmlXsd10ModelGroupDef *)
    }
exit:

    return (ret);
}

/** Port of xmlSchemaGetNotation. */
XmlXsd10Notation *xml_xsd10_get_notation(XmlXsd10Schema *schema,
		     const char *name,
		     const char *nsName)
{
    XmlXsd10Notation *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return (NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(notaDecl, XmlXsd10Notation *)
    }
exit:
    return (ret);
}

/** Port of xmlSchemaGetIDC. */
XmlXsd10IDC *xml_xsd10_get_idc(XmlXsd10Schema *schema,
		const char *name,
		const char *nsName)
{
    XmlXsd10IDC *ret = NULL;

    if ((name == NULL) || (schema == NULL))
        return (NULL);
    if (schema != NULL) {
	XSD10_WXS_FIND_GLOBAL_ITEM(idcDef, XmlXsd10IDC *)
    }
exit:
    return (ret);
}

/**
 * Port of xmlSchemaGetNamedComponent.
 *
 * Lookup a group in the schema or imported schemas
 *
 * @param schema  the schema
 * @param itemType  the item type
 * @param name  the name of the group
 * @param targetNs  the target namespace of the group
 * @returns the group definition or NULL if not found.
 */
XmlXsd10BasicItem *xml_xsd10_get_named_component(XmlXsd10Schema *schema,
			   XmlXsd10TypeType itemType,
			   const char *name,
			   const char *targetNs)
{
    switch (itemType) {
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return ((XmlXsd10BasicItem *) xml_xsd10_get_group(schema,
		name, targetNs));
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    return ((XmlXsd10BasicItem *) xml_xsd10_get_elem(schema,
		name, targetNs));
	default:
	    /* TODO */
	    return (NULL);
    }
}

/************************************************************************
 *									*
 *			Parsing functions				*
 *									*
 ************************************************************************/


/**
 * Port of xmlSchemaIsBlank.
 *
 * Check if a string is ignorable
 *
 * @param str  a string
 * @param len  the length of the string or -1
 * @returns 1 if the string is NULL or made of blanks chars, 0 otherwise
 */
int xml_xsd10_is_blank(char *str, int len)
{
    if (str == NULL)
        return (1);
    if (len < 0) {
	while (*str != 0) {
	    if (!(XSD10_IS_BLANK_CH(*str)))
		return (0);
	    str++;
	}
    } else while ((*str != 0) && (len != 0)) {
	if (!(XSD10_IS_BLANK_CH(*str)))
	    return (0);
	str++;
	len--;
    }

    return (1);
}

#define XSD10_WXS_COMP_NAME(c, t) ((t) (c))->name
#define XSD10_WXS_COMP_TNS(c, t) ((t) (c))->targetNamespace
/*
 * Port of xmlSchemaFindRedefCompInGraph.
 *
* xmlSchemaFindRedefCompInGraph:
* ATTENTION TODO: This uses pointer comp. for strings.
*/
XmlXsd10BasicItem *xml_xsd10_find_redef_comp_in_graph(XmlXsd10Bucket *bucket,
			      XmlXsd10TypeType type,
			      const char *name,
			      const char *nsName)
{
    XmlXsd10BasicItem *ret;
    int i;

    if ((bucket == NULL) || (name == NULL))
	return(NULL);
    if ((bucket->globals == NULL) ||
	(bucket->globals->nbItems == 0))
	goto subschemas;
    /*
    * Search in global components.
    */
    for (i = 0; i < bucket->globals->nbItems; i++) {
	ret = (XmlXsd10BasicItem *) bucket->globals->items[i];
	if (ret->type == type) {
	    switch (type) {
		case XML_XSD10_SCHEMA_TYPE_COMPLEX:
		case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		    if ((XSD10_WXS_COMP_NAME(ret, XmlXsd10Type *) == name) &&
			(XSD10_WXS_COMP_TNS(ret, XmlXsd10Type *) ==
			nsName))
		    {
			return(ret);
		    }
		    break;
		case XML_XSD10_SCHEMA_TYPE_GROUP:
		    if ((XSD10_WXS_COMP_NAME(ret,
			    XmlXsd10ModelGroupDef *) == name) &&
			(XSD10_WXS_COMP_TNS(ret,
			    XmlXsd10ModelGroupDef *) == nsName))
		    {
			return(ret);
		    }
		    break;
		case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		    if ((XSD10_WXS_COMP_NAME(ret,
			    XmlXsd10AttributeGroup *) == name) &&
			(XSD10_WXS_COMP_TNS(ret,
			    XmlXsd10AttributeGroup *) == nsName))
		    {
			return(ret);
		    }
		    break;
		default:
		    /* Should not be hit. */
		    return(NULL);
	    }
	}
    }
subschemas:
    /*
    * Process imported/included schemas.
    */
    if (bucket->relations != NULL) {
	XmlXsd10SchemaRelation *rel = bucket->relations;

	/*
	* TODO: Marking the bucket will not avoid multiple searches
	* in the same schema, but avoids at least circularity.
	*/
	bucket->flags |= XML_XSD10_SCHEMA_BUCKET_MARKED;
	do {
	    if ((rel->bucket != NULL) &&
		((rel->bucket->flags & XML_XSD10_SCHEMA_BUCKET_MARKED) == 0)) {
		ret = xml_xsd10_find_redef_comp_in_graph(rel->bucket,
		    type, name, nsName);
		if (ret != NULL)
		    return(ret);
	    }
	    rel = rel->next;
	} while (rel != NULL);
	 bucket->flags ^= XML_XSD10_SCHEMA_BUCKET_MARKED;
    }
    return(NULL);
}

/**
 * Port of xmlSchemaAddNotation.
 *
 * Add an XML schema annotation declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema being built
 * @param name  the item name
 * @param nsName  the element namespace
 * @param node  unused
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Notation *xml_xsd10_add_notation(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                     const char *name, const char *nsName,
		     XMQNode *node )
{
    (void) node;
    XmlXsd10Notation *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (name == NULL))
        return (NULL);

    ret = (XmlXsd10Notation *) malloc(sizeof(XmlXsd10Notation));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Notation));
    ret->type = XML_XSD10_SCHEMA_TYPE_NOTATION;
    ret->name = name;
    ret->targetNamespace = nsName;
    /* TODO: do we need the node to be set?
    * ret->node = node;*/
    XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaAddAttribute.
 *
 * Add an XML schema Attribute declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema being built
 * @param name  the item name
 * @param nsName  the item namespace
 * @param node  the context node
 * @param topLevel  whether this is a top-level element
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Attribute *xml_xsd10_add_attribute(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                      const char *name, const char *nsName,
		      XMQNode *node, int topLevel)
{
    XmlXsd10Attribute *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL))
        return (NULL);

    ret = (XmlXsd10Attribute *) malloc(sizeof(XmlXsd10Attribute));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Attribute));
    ret->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE;
    ret->node = node;
    ret->name = name;
    ret->targetNamespace = nsName;

    if (topLevel)
	XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    else
	XSD10_WXS_ADD_LOCAL(ctxt, ret);
    XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaAddAttributeUse.
 *
 * Add an XML schema Attribute declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param pctxt  a schema parser context
 * @param node  the context node
 * @returns the new structure or NULL in case of error
 */
XmlXsd10AttributeUse *xml_xsd10_add_attribute_use(XmlXsd10ParserCtxt *pctxt,
			 XMQNode *node)
{
    XmlXsd10AttributeUse *ret = NULL;

    if (pctxt == NULL)
        return (NULL);

    ret = (XmlXsd10AttributeUse *) malloc(sizeof(XmlXsd10AttributeUse));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(pctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10AttributeUse));
    ret->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE;
    ret->node = node;

    XSD10_WXS_ADD_LOCAL(pctxt, ret);
    return (ret);
}

/*
 * Port of xmlSchemaAddRedef.
 *
* xmlSchemaAddRedef:
*
* Adds a redefinition information. This is used at a later stage to:
* resolve references to the redefined components and to check constraints.
*/
XmlXsd10Redef *xml_xsd10_add_redef(XmlXsd10ParserCtxt *pctxt,
		  XmlXsd10Bucket *targetBucket,
		  void *item,
		  const char *refName,
		  const char *refTargetNs)
{
    XmlXsd10Redef *ret;

    ret = (XmlXsd10Redef *)
	malloc(sizeof(XmlXsd10Redef));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(pctxt);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Redef));
    ret->item = (XmlXsd10BasicItem *) item;
    ret->targetBucket = targetBucket;
    ret->refName = refName;
    ret->refTargetNs = refTargetNs;
    if (XSD10_WXS_CONSTRUCTOR(pctxt)->redefs == NULL)
	XSD10_WXS_CONSTRUCTOR(pctxt)->redefs = ret;
    else
	XSD10_WXS_CONSTRUCTOR(pctxt)->lastRedef->next = ret;
    XSD10_WXS_CONSTRUCTOR(pctxt)->lastRedef = ret;

    return (ret);
}

/**
 * Port of xmlSchemaAddAttributeGroupDefinition.
 *
 * Add an XML schema Attribute Group definition.
 *
 * @param pctxt  a schema parser context
 * @param schema  the schema being built
 * @param name  the item name
 * @param nsName  the target namespace
 * @param node  the corresponding node
 * @returns the new structure or NULL in case of error
 */
XmlXsd10AttributeGroup *xml_xsd10_add_attribute_group_definition(XmlXsd10ParserCtxt *pctxt,
                           XmlXsd10Schema *schema ,
			   const char *name,
			   const char *nsName,
			   XMQNode *node)
{
    (void) schema;
    XmlXsd10AttributeGroup *ret = NULL;

    if ((pctxt == NULL) || (name == NULL))
        return (NULL);

    ret = (XmlXsd10AttributeGroup *)
        malloc(sizeof(XmlXsd10AttributeGroup));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(pctxt);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10AttributeGroup));
    ret->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP;
    ret->name = name;
    ret->targetNamespace = nsName;
    ret->node = node;

    /* TODO: Remove the flag. */
    ret->flags |= XML_XSD10_SCHEMAS_ATTRGROUP_GLOBAL;
    if (pctxt->isRedefine) {
	pctxt->redef = xml_xsd10_add_redef(pctxt, pctxt->redefined,
	    ret, name, nsName);
	if (pctxt->redef == NULL) {
	    free(ret);
	    return(NULL);
	}
	pctxt->redefCounter = 0;
    }
    XSD10_WXS_ADD_GLOBAL(pctxt, ret);
    XSD10_WXS_ADD_PENDING(pctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaAddElement.
 *
 * Add an XML schema Element declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param name  the type name
 * @param nsName  the type namespace
 * @param node  the context node
 * @param topLevel  whether this is a top-level element
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Element *xml_xsd10_add_element(XmlXsd10ParserCtxt *ctxt,
                    const char *name, const char *nsName,
		    XMQNode *node, int topLevel)
{
    XmlXsd10Element *ret = NULL;

    if ((ctxt == NULL) || (name == NULL))
        return (NULL);

    ret = (XmlXsd10Element *) malloc(sizeof(XmlXsd10Element));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Element));
    ret->type = XML_XSD10_SCHEMA_TYPE_ELEMENT;
    ret->name = name;
    ret->targetNamespace = nsName;
    ret->node = node;

    if (topLevel)
	XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    else
	XSD10_WXS_ADD_LOCAL(ctxt, ret);
    XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaAddType.
 *
 * Add an XML schema item
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema being built
 * @param type  the type
 * @param name  the item name
 * @param nsName  the namespace
 * @param node  the context node
 * @param topLevel  whether this is a top-level element
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Type *xml_xsd10_add_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
		 XmlXsd10TypeType type,
                 const char *name, const char *nsName,
		 XMQNode *node, int topLevel)
{
    XmlXsd10Type *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL))
        return (NULL);

    ret = (XmlXsd10Type *) malloc(sizeof(XmlXsd10Type));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Type));
    ret->type = type;
    ret->name = name;
    ret->targetNamespace = nsName;
    ret->node = node;
    if (topLevel) {
	if (ctxt->isRedefine) {
	    ctxt->redef = xml_xsd10_add_redef(ctxt, ctxt->redefined,
		ret, name, nsName);
	    if (ctxt->redef == NULL) {
		free(ret);
		return(NULL);
	    }
	    ctxt->redefCounter = 0;
	}
	XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    } else
	XSD10_WXS_ADD_LOCAL(ctxt, ret);
    XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}

/** Port of xmlSchemaNewQNameRef. */
XmlXsd10QNameRef *xml_xsd10_new_q_name_ref(XmlXsd10ParserCtxt *pctxt,
		     XmlXsd10TypeType refType,
		     const char *refName,
		     const char *refNs)
{
    XmlXsd10QNameRef *ret;

    ret = (XmlXsd10QNameRef *)
	malloc(sizeof(XmlXsd10QNameRef));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(pctxt);
	return (NULL);
    }
    ret->node = NULL;
    ret->type = XML_XSD10_SCHEMA_EXTRA_QNAMEREF;
    ret->name = refName;
    ret->targetNamespace = refNs;
    ret->item = NULL;
    ret->itemType = refType;
    /*
    * Store the reference item in the schema.
    */
    XSD10_WXS_ADD_LOCAL(pctxt, ret);
    return (ret);
}

/** Port of xmlSchemaAddAttributeUseProhib. */
XmlXsd10AttributeUseProhib *xml_xsd10_add_attribute_use_prohib(XmlXsd10ParserCtxt *pctxt)
{
    XmlXsd10AttributeUseProhib *ret;

    ret = (XmlXsd10AttributeUseProhib *)
	malloc(sizeof(XmlXsd10AttributeUseProhib));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(pctxt);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10AttributeUseProhib));
    ret->type = XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB;
    XSD10_WXS_ADD_LOCAL(pctxt, ret);
    return (ret);
}


/**
 * Port of xmlSchemaAddModelGroup.
 *
 * Adds a schema model group
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema being built
 * @param type  the "compositor" type of the model group
 * @param node  the node in the schema doc
 * @returns the new structure or NULL in case of error
 */
XmlXsd10ModelGroup *xml_xsd10_add_model_group(XmlXsd10ParserCtxt *ctxt,
		       XmlXsd10Schema *schema,
		       XmlXsd10TypeType type,
		       XMQNode *node)
{
    XmlXsd10ModelGroup *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL))
        return (NULL);

    ret = (XmlXsd10ModelGroup *)
	malloc(sizeof(XmlXsd10ModelGroup));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(ctxt);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ModelGroup));
    ret->type = type;
    ret->node = node;
    XSD10_WXS_ADD_LOCAL(ctxt, ret);
    if ((type == XML_XSD10_SCHEMA_TYPE_SEQUENCE) ||
	(type == XML_XSD10_SCHEMA_TYPE_CHOICE))
	XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}


/**
 * Port of xmlSchemaAddParticle.
 *
 * Adds an XML schema particle component.
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param node  the corresponding node in the schema doc
 * @param min  the minOccurs
 * @param max  the maxOccurs
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Particle *xml_xsd10_add_particle(XmlXsd10ParserCtxt *ctxt,
		     XMQNode *node, int min, int max)
{
    XmlXsd10Particle *ret = NULL;
    if (ctxt == NULL)
        return (NULL);

    ret = (XmlXsd10Particle *)
	malloc(sizeof(XmlXsd10Particle));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(ctxt);
	return (NULL);
    }
    ret->type = XML_XSD10_SCHEMA_TYPE_PARTICLE;
    ret->annot = NULL;
    ret->node = node;
    ret->minOccurs = min;
    ret->maxOccurs = max;
    ret->next = NULL;
    ret->children = NULL;

    XSD10_WXS_ADD_LOCAL(ctxt, ret);
    /*
    * Note that addition to pending components will be done locally
    * to the specific parsing function, since the most particles
    * need not to be fixed up (i.e. the reference to be resolved).
    * REMOVED: WXS_ADD_PENDING(ctxt, ret);
    */
    return (ret);
}

/**
 * Port of xmlSchemaAddModelGroupDefinition.
 *
 * Add an XML schema Group definition
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param name  the group name
 * @param nsName  the group namespace
 * @param node  the context node
 * @returns the new structure or NULL in case of error
 */
XmlXsd10ModelGroupDef *xml_xsd10_add_model_group_definition(XmlXsd10ParserCtxt *ctxt,
				 XmlXsd10Schema *schema,
				 const char *name,
				 const char *nsName,
				 XMQNode *node)
{
    XmlXsd10ModelGroupDef *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (name == NULL))
        return (NULL);

    ret = (XmlXsd10ModelGroupDef *)
	malloc(sizeof(XmlXsd10ModelGroupDef));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ModelGroupDef));
    ret->name = name;
    ret->type = XML_XSD10_SCHEMA_TYPE_GROUP;
    ret->node = node;
    ret->targetNamespace = nsName;

    if (ctxt->isRedefine) {
	ctxt->redef = xml_xsd10_add_redef(ctxt, ctxt->redefined,
	    ret, name, nsName);
	if (ctxt->redef == NULL) {
	    free(ret);
	    return(NULL);
	}
	ctxt->redefCounter = 0;
    }
    XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaNewWildcardNsConstraint.
 *
 * Creates a new wildcard namespace constraint.
 *
 * @param ctxt  a schema validation context
 * @returns the new structure or NULL in case of error
 */
XmlXsd10WildcardNs *xml_xsd10_new_wildcard_ns_constraint(XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10WildcardNs *ret;

    ret = (XmlXsd10WildcardNs *)
	malloc(sizeof(XmlXsd10WildcardNs));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(ctxt);
	return (NULL);
    }
    ret->value = NULL;
    ret->next = NULL;
    return (ret);
}

/** Port of xmlSchemaAddIDC. */
XmlXsd10IDC *xml_xsd10_add_idc(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                  const char *name, const char *nsName,
		  int category, XMQNode *node)
{
    XmlXsd10IDC *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (name == NULL))
        return (NULL);

    ret = (XmlXsd10IDC *) malloc(sizeof(XmlXsd10IDC));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10IDC));
    /* The target namespace of the parent element declaration. */
    ret->targetNamespace = nsName;
    ret->name = name;
    ret->type = (XmlXsd10TypeType) category;
    ret->node = node;

    XSD10_WXS_ADD_GLOBAL(ctxt, ret);
    /*
    * Only keyrefs need to be fixup up.
    */
    if (category == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF)
	XSD10_WXS_ADD_PENDING(ctxt, ret);
    return (ret);
}

/**
 * Port of xmlSchemaAddWildcard.
 *
 * Adds a wildcard.
 * It corresponds to a xsd:anyAttribute and xsd:any.
 *
 * @param ctxt  a schema validation context
 * @param schema  a schema
 * @param type  the type
 * @param node  the context node
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Wildcard *xml_xsd10_add_wildcard(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
		     XmlXsd10TypeType type, XMQNode *node)
{
    XmlXsd10Wildcard *ret = NULL;

    if ((ctxt == NULL) || (schema == NULL))
        return (NULL);

    ret = (XmlXsd10Wildcard *) malloc(sizeof(XmlXsd10Wildcard));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Wildcard));
    ret->type = type;
    ret->node = node;
    XSD10_WXS_ADD_LOCAL(ctxt, ret);
    return (ret);
}


/** Port of xmlSchemaSubstGroupAdd. */
XmlXsd10SubstGroup *xml_xsd10_subst_group_add(XmlXsd10ParserCtxt *pctxt,
		       XmlXsd10Element *head)
{
    XmlXsd10SubstGroup *ret;

    /* Init subst group hash. */
    if (XSD10_WXS_SUBST_GROUPS(pctxt) == NULL) {
	XSD10_WXS_SUBST_GROUPS(pctxt) = xml_xsd10_hash_create_dict(10, pctxt->dict);
	if (XSD10_WXS_SUBST_GROUPS(pctxt) == NULL)
	    return(NULL);
    }
    /* Create a new substitution group. */
    ret = (XmlXsd10SubstGroup *) malloc(sizeof(XmlXsd10SubstGroup));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(NULL);
	return(NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10SubstGroup));
    ret->head = head;
    /* Create list of members. */
    ret->members = xml_xsd10_item_list_create();
    if (ret->members == NULL) {
	xml_xsd10_subst_group_free(ret);
	return(NULL);
    }
    /* Add subst group to hash. */
    if (xml_xsd10_hash_add_entry2(XSD10_WXS_SUBST_GROUPS(pctxt),
	head->name, head->targetNamespace, ret) != 0) {
	XSD10_PERROR_INT("xmlSchemaSubstGroupAdd",
	    "failed to add a new substitution container");
	xml_xsd10_subst_group_free(ret);
	return(NULL);
    }
    return(ret);
}

/** Port of xmlSchemaSubstGroupGet. */
XmlXsd10SubstGroup *xml_xsd10_subst_group_get(XmlXsd10ParserCtxt *pctxt,
		       XmlXsd10Element *head)
{
    if (XSD10_WXS_SUBST_GROUPS(pctxt) == NULL)
	return(NULL);
    return((XmlXsd10SubstGroup *) xml_xsd10_hash_lookup2(XSD10_WXS_SUBST_GROUPS(pctxt),
	head->name, head->targetNamespace));

}

/**
 * Port of xmlSchemaAddElementSubstitutionMember.
 *
 * Allocate a new annotation structure.
 *
 * @param pctxt  a schema parser context
 * @param head  the head of the substitution group
 * @param member  the new member of the substitution group
 * @returns the newly allocated structure or NULL in case or error
 */
int xml_xsd10_add_element_substitution_member(XmlXsd10ParserCtxt *pctxt,
				      XmlXsd10Element *head,
				      XmlXsd10Element *member)
{
    XmlXsd10SubstGroup *substGroup = NULL;

    if ((pctxt == NULL) || (head == NULL) || (member == NULL))
	return (-1);

    substGroup = xml_xsd10_subst_group_get(pctxt, head);
    if (substGroup == NULL)
	substGroup = xml_xsd10_subst_group_add(pctxt, head);
    if (substGroup == NULL)
	return(-1);
    if (xml_xsd10_item_list_add(substGroup->members, member) == -1)
	return(-1);
    return(0);
}

/************************************************************************
 *									*
 *		Utilities for parsing					*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaPValAttrNodeQNameValue.
 *
 * Extracts the local name and the URI of a QName value and validates it.
 * This one is intended to be used on attribute values that
 * should resolve to schema components.
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema context
 * @param attr  the attribute node
 * @param ownerItem  the parent as a schema object
 * @param value  the QName value
 * @param uri  the resulting namespace URI if found
 * @param local  the resulting local part if found, the attribute value otherwise
 * @returns 0, in case the QName is valid, a positive error code
 * if not valid and -1 if an internal error occurs.
 */
int xml_xsd10_p_val_attr_node_q_name_value(XmlXsd10ParserCtxt *ctxt,
				       XmlXsd10Schema *schema,
				       XmlXsd10BasicItem *ownerItem,
				       XMQNode *attr,
				       const char *value,
				       const char **uri,
				       const char **local)
{
    const char *pref;
    XmlNs *ns;
    int len, ret;

    *uri = NULL;
    *local = NULL;
    ret = xml_xsd10_tree_validate_qname(value, 1);
    if (ret > 0) {
	xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    ownerItem, (XMQNode *) attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME),
	    NULL, value, NULL, NULL, NULL);
	*local = value;
	return (ctxt->err);
    } else if (ret < 0)
	return (-1);

    if (!strchr((char *) value, ':')) {
	ns = xml_xsd10_tree_search_ns(xml_node_parent(attr), NULL);
	if (ns && xml_ns_href(ns) && xml_ns_href(ns)[0])
	    *uri = xml_xsd10_dict_lookup(ctxt->dict, xml_ns_href(ns), -1);
	else if (schema->flags & XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS) {
	    /* TODO: move XML_SCHEMAS_INCLUDING_CONVERT_NS to the
	    * parser context. */
	    /*
	    * This one takes care of included schemas with no
	    * target namespace.
	    */
	    *uri = ctxt->targetNamespace;
	}
	*local = xml_xsd10_dict_lookup(ctxt->dict, value, -1);
	return (0);
    }
    /*
    * At this point xmlSplitQName3 has to return a local name.
    */
    *local = xml_xsd10_tree_split_qname3(value, &len);
    *local = xml_xsd10_dict_lookup(ctxt->dict, *local, -1);
    pref = xml_xsd10_dict_lookup(ctxt->dict, value, len);
    ns = xml_xsd10_tree_search_ns(xml_node_parent(attr), pref);
    if (ns == NULL) {
	xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    ownerItem, (XMQNode *) attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME), NULL, value,
	    "The value '%s' of simple type 'xs:QName' has no "
	    "corresponding namespace declaration in scope", value, NULL);
	return (ctxt->err);
    } else {
        *uri = xml_xsd10_dict_lookup(ctxt->dict, xml_ns_href(ns), -1);
    }
    return (0);
}

/**
 * Port of xmlSchemaPValAttrNodeQName.
 *
 * Extracts and validates the QName of an attribute value.
 * This one is intended to be used on attribute values that
 * should resolve to schema components.
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema context
 * @param ownerItem  the owner as a schema object
 * @param attr  the attribute node
 * @param uri  the resulting namespace URI if found
 * @param local  the resulting local part if found, the attribute value otherwise
 * @returns 0, in case the QName is valid, a positive error code
 * if not valid and -1 if an internal error occurs.
 */
int xml_xsd10_p_val_attr_node_q_name(XmlXsd10ParserCtxt *ctxt,
				       XmlXsd10Schema *schema,
				       XmlXsd10BasicItem *ownerItem,
				       XMQNode *attr,
				       const char **uri,
				       const char **local)
{
    const char *value;

    value = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
    return (xml_xsd10_p_val_attr_node_q_name_value(ctxt, schema,
	ownerItem, attr, value, uri, local));
}

/**
 * Port of xmlSchemaPValAttrQName.
 *
 * Extracts and validates the QName of an attribute value.
 *
 * @param ctxt  a schema parser context
 * @param schema  the schema context
 * @param ownerItem  the owner as a schema object
 * @param ownerElem  the parent node of the attribute
 * @param name  the name of the attribute
 * @param uri  the resulting namespace URI if found
 * @param local  the resulting local part if found, the attribute value otherwise
 * @returns 0, in case the QName is valid, a positive error code
 * if not valid and -1 if an internal error occurs.
 */
int xml_xsd10_p_val_attr_q_name(XmlXsd10ParserCtxt *ctxt,
				   XmlXsd10Schema *schema,
				   XmlXsd10BasicItem *ownerItem,
				   XMQNode *ownerElem,
				   const char *name,
				   const char **uri,
				   const char **local)
{
    XMQNode *attr;

    attr = xml_xsd10_get_prop_node(ownerElem, name);
    if (attr == NULL) {
	*local = NULL;
	*uri = NULL;
	return (0);
    }
    return (xml_xsd10_p_val_attr_node_q_name(ctxt, schema,
	ownerItem, attr, uri, local));
}

/**
 * Port of xmlSchemaPValAttrNodeID.
 *
 * Extracts and validates the ID of an attribute value.
 *
 * @param ctxt  a schema parser context
 * @param attr  attribute node
 * @returns 0, in case the ID is valid, a positive error code
 * if not valid and -1 if an internal error occurs.
 */
int xml_xsd10_p_val_attr_node_id(XmlXsd10ParserCtxt *ctxt, XMQNode *attr)
{
    int ret;
    const char *value;

    if (attr == NULL)
	return(0);
    value = xml_xsd10_get_node_content_no_dict((XMQNode *) attr);
    ret = xml_xsd10_tree_validate_ncname(value, 1);
    if (ret == 0) {
	/*
	* NOTE: the IDness might have already be declared in the DTD
	*/
	XmlXsd10Bucket *bucket = XSD10_WXS_BUCKET(ctxt);

	/* libxml2 registers the IDs in the document (attr->atype, xmlAddIDSafe),
	   the native port in the bucket of the schema document. */
	if ((bucket != NULL) && (bucket->ids == NULL))
	    bucket->ids = xml_xsd10_ids_new();
	if ((bucket == NULL) || (bucket->ids == NULL)) {
	    xml_xsd10_p_err_memory(ctxt);
	} else if (xml_xsd10_ids_attr_type(bucket->ids, attr) != XML_XSD10_ATTRIBUTE_ID) {
	    char *strip;
            int res;

	    /*
	    * TODO: Use xmlSchemaStrip here; it's not exported at this
	    * moment.
	    */
	    strip = xml_xsd10_collapse_string(value);
	    if (strip != NULL) {
		free((char *) value);
		value = strip;
	    }
	    res = xml_xsd10_ids_add_id(bucket->ids, attr, value);

            if (res < 0) {
                xml_xsd10_p_err_memory(ctxt);
	    } else if (res == 0) {
		ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
		xml_xsd10_p_simple_type_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		    NULL, (XMQNode *) attr,
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ID),
		    NULL, NULL, "Duplicate value '%s' of simple "
		    "type 'xs:ID'", value, NULL);
	    }
	}
    } else if (ret > 0) {
	ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
	xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    NULL, (XMQNode *) attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ID),
	    NULL, NULL, "The value '%s' of simple type 'xs:ID' is "
	    "not a valid 'xs:NCName'",
	    value, NULL);
    }
    if (value != NULL)
	free((char *)value);

    return (ret);
}

/** Port of xmlSchemaPValAttrID. */
int xml_xsd10_p_val_attr_id(XmlXsd10ParserCtxt *ctxt,
		    XMQNode *ownerElem,
		    const char *name)
{
    XMQNode *attr;

    attr = xml_xsd10_get_prop_node(ownerElem, (const char *) name);
    if (attr == NULL)
	return(0);
    return(xml_xsd10_p_val_attr_node_id(ctxt, attr));

}

/**
 * Port of xmlGetMaxOccurs.
 *
 * Get the maxOccurs property
 *
 * @param ctxt  a schema validation context
 * @param node  a subtree containing XML Schema information
 * @param min  minimum
 * @param max  maximum
 * @param def  default
 * @param expected  expected
 * @returns the default if not found, or the value
 */
int xml_xsd10_get_max_occurs(XmlXsd10ParserCtxt *ctxt, XMQNode *node,
		int min, int max, int def, const char *expected)
{
    const char *val, *cur;
    int ret = 0;
    XMQNode *attr;

    attr = xml_xsd10_get_prop_node(node, "maxOccurs");
    if (attr == NULL)
	return (def);
    val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
    if (val == NULL)
        return (def);

    if (xml_xsd10_str_equal(val, (const char *) "unbounded")) {
	if (max != XML_XSD10_UNBOUNDED) {
	    xml_xsd10_p_simple_type_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		/* XML_SCHEMAP_INVALID_MINOCCURS, */
		NULL, (XMQNode *) attr, NULL, expected,
		val, NULL, NULL, NULL);
	    return (def);
	} else
	    return (XML_XSD10_UNBOUNDED);  /* encoding it with -1 might be another option */
    }

    cur = val;
    while (XSD10_IS_BLANK_CH(*cur))
        cur++;
    if (*cur == 0) {
        xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    /* XML_SCHEMAP_INVALID_MINOCCURS, */
	    NULL, (XMQNode *) attr, NULL, expected,
	    val, NULL, NULL, NULL);
	return (def);
    }
    while ((*cur >= '0') && (*cur <= '9')) {
        if (ret > INT_MAX / 10) {
            ret = INT_MAX;
        } else {
            int digit = *cur - '0';
            ret *= 10;
            if (ret > INT_MAX - digit)
                ret = INT_MAX;
            else
                ret += digit;
        }
        cur++;
    }
    while (XSD10_IS_BLANK_CH(*cur))
        cur++;
    /*
    * TODO: Restrict the maximal value to Integer.
    */
    if ((*cur != 0) || (ret < min) || ((max != -1) && (ret > max))) {
	xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    /* XML_SCHEMAP_INVALID_MINOCCURS, */
	    NULL, (XMQNode *) attr, NULL, expected,
	    val, NULL, NULL, NULL);
        return (def);
    }
    return (ret);
}

/**
 * Port of xmlGetMinOccurs.
 *
 * Get the minOccurs property
 *
 * @param ctxt  a schema validation context
 * @param node  a subtree containing XML Schema information
 * @param min  minimum
 * @param max  maximum
 * @param def  default
 * @param expected  expected
 * @returns the default if not found, or the value
 */
int xml_xsd10_get_min_occurs(XmlXsd10ParserCtxt *ctxt, XMQNode *node,
		int min, int max, int def, const char *expected)
{
    const char *val, *cur;
    int ret = 0;
    XMQNode *attr;

    attr = xml_xsd10_get_prop_node(node, "minOccurs");
    if (attr == NULL)
	return (def);
    val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
    if (val == NULL)
	return (def);
    cur = val;
    while (XSD10_IS_BLANK_CH(*cur))
        cur++;
    if (*cur == 0) {
        xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    /* XML_SCHEMAP_INVALID_MINOCCURS, */
	    NULL, (XMQNode *) attr, NULL, expected,
	    val, NULL, NULL, NULL);
        return (def);
    }
    while ((*cur >= '0') && (*cur <= '9')) {
        if (ret > INT_MAX / 10) {
            ret = INT_MAX;
        } else {
            int digit = *cur - '0';
            ret *= 10;
            if (ret > INT_MAX - digit)
                ret = INT_MAX;
            else
                ret += digit;
        }
        cur++;
    }
    while (XSD10_IS_BLANK_CH(*cur))
        cur++;
    /*
    * TODO: Restrict the maximal value to Integer.
    */
    if ((*cur != 0) || (ret < min) || ((max != -1) && (ret > max))) {
	xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    /* XML_SCHEMAP_INVALID_MINOCCURS, */
	    NULL, (XMQNode *) attr, NULL, expected,
	    val, NULL, NULL, NULL);
        return (def);
    }
    return (ret);
}

/**
 * Port of xmlSchemaPGetBoolNodeValue.
 *
 * Converts a boolean string value into 1 or 0.
 *
 * @param ctxt  a schema validation context
 * @param ownerItem  the owner as a schema item
 * @param node  the node holding the value
 * @returns 0 or 1.
 */
int xml_xsd10_p_get_bool_node_value(XmlXsd10ParserCtxt *ctxt,
			   XmlXsd10BasicItem *ownerItem,
			   XMQNode *node)
{
    char *value = NULL;
    int res = 0;

    value = xml_xsd10_tree_node_get_content(node);
    /*
    * 3.2.2.1 Lexical representation
    * An instance of a datatype that is defined as `boolean`
    * can have the following legal literals {true, false, 1, 0}.
    */
    if (xml_xsd10_str_equal( value,  "true"))
        res = 1;
    else if (xml_xsd10_str_equal( value,  "false"))
        res = 0;
    else if (xml_xsd10_str_equal( value,  "1"))
	res = 1;
    else if (xml_xsd10_str_equal( value,  "0"))
        res = 0;
    else {
        xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_INVALID_BOOLEAN,
	    ownerItem, node,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_BOOLEAN),
	    NULL,  value,
	    NULL, NULL, NULL);
    }
    if (value != NULL)
	free(value);
    return (res);
}

/**
 * Port of xmlGetBooleanProp.
 *
 * Evaluate if a boolean property is set
 *
 * @param ctxt  a schema validation context
 * @param node  a subtree containing XML Schema information
 * @param name  the attribute name
 * @param def  the default value
 * @returns the default if not found, 0 if found to be false,
 * 1 if found to be true
 */
int xml_xsd10_get_boolean_prop(XmlXsd10ParserCtxt *ctxt,
		  XMQNode *node,
                  const char *name, int def)
{
    const char *val;

    val = xml_xsd10_get_prop(ctxt, node, name);
    if (val == NULL)
        return (def);
    /*
    * 3.2.2.1 Lexical representation
    * An instance of a datatype that is defined as `boolean`
    * can have the following legal literals {true, false, 1, 0}.
    */
    if (xml_xsd10_str_equal(val,  "true"))
        def = 1;
    else if (xml_xsd10_str_equal(val,  "false"))
        def = 0;
    else if (xml_xsd10_str_equal(val,  "1"))
	def = 1;
    else if (xml_xsd10_str_equal(val,  "0"))
        def = 0;
    else {
        xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_INVALID_BOOLEAN,
	    NULL,
	    (XMQNode *) xml_xsd10_get_prop_node(node, name),
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_BOOLEAN),
	    NULL, val, NULL, NULL, NULL);
    }
    return (def);
}

/************************************************************************
 *									*
 *		Schema extraction from an Infoset			*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaPValAttrNodeValue.
 *
 * Validates a value against the given built-in type.
 * This one is intended to be used internally for validation
 * of schema attribute values during parsing of the schema.
 *
 * @param pctxt  a schema parser context
 * @param ownerItem  the schema object owner if existent
 * @param attr  the schema attribute node being validated
 * @param value  the value
 * @param type  the built-in type to be validated against
 * @returns 0 if the value is valid, a positive error code
 * number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_p_val_attr_node_value(XmlXsd10ParserCtxt *pctxt,
			   XmlXsd10BasicItem *ownerItem,
			   XMQNode *attr,
			   const char *value,
			   XmlXsd10Type *type)
{

    int ret = 0;

    /*
    * NOTE: Should we move this to xmlschematypes.c? Hmm, but this
    * one is really meant to be used internally, so better not.
    */
    if ((pctxt == NULL) || (type == NULL) || (attr == NULL))
	return (-1);
    if (type->type != XML_XSD10_SCHEMA_TYPE_BASIC) {
	XSD10_PERROR_INT("xmlSchemaPValAttrNodeValue",
	    "the given type is not a built-in type");
	return (-1);
    }
    switch (type->builtInType) {
	case XML_XSD10_SCHEMAS_NCNAME:
	case XML_XSD10_SCHEMAS_QNAME:
	case XML_XSD10_SCHEMAS_ANYURI:
	case XML_XSD10_SCHEMAS_TOKEN:
	case XML_XSD10_SCHEMAS_LANGUAGE:
	    ret = xml_xsd10_val_predef_type_node(type, value, NULL,
		(XMQNode *) attr);
	    break;
	default: {
	    XSD10_PERROR_INT("xmlSchemaPValAttrNodeValue",
		"validation using the given type is not supported while "
		"parsing a schema");
	    return (-1);
	}
    }
    /*
    * TODO: Should we use the S4S error codes instead?
    */
    if (ret < 0) {
	XSD10_PERROR_INT("xmlSchemaPValAttrNodeValue",
	    "failed to validate a schema attribute value");
	return (-1);
    } else if (ret > 0) {
	if (XSD10_WXS_IS_LIST(type))
	    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
	else
	    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
	xml_xsd10_p_simple_type_err(pctxt,
	    ret, ownerItem, (XMQNode *) attr,
	    type, NULL, value, NULL, NULL, NULL);
    }
    return (ret);
}

/**
 * Port of xmlSchemaPValAttrNode.
 *
 * Extracts and validates a value against the given built-in type.
 * This one is intended to be used internally for validation
 * of schema attribute values during parsing of the schema.
 *
 * @param ctxt  a schema parser context
 * @param ownerItem  the schema object owner if existent
 * @param attr  the schema attribute node being validated
 * @param type  the built-in type to be validated against
 * @param value  the resulting value if any
 * @returns 0 if the value is valid, a positive error code
 * number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_p_val_attr_node(XmlXsd10ParserCtxt *ctxt,
			   XmlXsd10BasicItem *ownerItem,
			   XMQNode *attr,
			   XmlXsd10Type *type,
			   const char **value)
{
    const char *val;

    if ((ctxt == NULL) || (type == NULL) || (attr == NULL))
	return (-1);

    val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
    if (value != NULL)
	*value = val;

    return (xml_xsd10_p_val_attr_node_value(ctxt, ownerItem, attr,
	val, type));
}

/**
 * Port of xmlSchemaPValAttr.
 *
 * Extracts and validates a value against the given built-in type.
 * This one is intended to be used internally for validation
 * of schema attribute values during parsing of the schema.
 *
 * @param ctxt  a schema parser context
 * @param ownerItem  the schema object owner if existent
 * @param ownerElem  the owner element node
 * @param name  the name of the schema attribute node
 * @param type  the built-in type to be validated against
 * @param value  the resulting value if any
 * @returns 0 if the value is valid, a positive error code
 * number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_p_val_attr(XmlXsd10ParserCtxt *ctxt,
		       XmlXsd10BasicItem *ownerItem,
		       XMQNode *ownerElem,
		       const char *name,
		       XmlXsd10Type *type,
		       const char **value)
{
    XMQNode *attr;

    if ((ctxt == NULL) || (type == NULL)) {
	if (value != NULL)
	    *value = NULL;
	return (-1);
    }
    if (type->type != XML_XSD10_SCHEMA_TYPE_BASIC) {
	if (value != NULL)
	    *value = NULL;
	xml_xsd10_p_err(ctxt, ownerElem,
	    XML_XSD10_SCHEMAP_INTERNAL,
	    "Internal error: xmlSchemaPValAttr, the given "
	    "type '%s' is not a built-in type.\n",
	    type->name, NULL);
	return (-1);
    }
    attr = xml_xsd10_get_prop_node(ownerElem, name);
    if (attr == NULL) {
	if (value != NULL)
	    *value = NULL;
	return (0);
    }
    return (xml_xsd10_p_val_attr_node(ctxt, ownerItem, attr,
	type, value));
}

/** Port of xmlSchemaCheckReference. */
int xml_xsd10_check_reference(XmlXsd10ParserCtxt *pctxt,
		  XmlXsd10Schema *schema ,
		  XMQNode *node,
		  XMQNode *attr,
		  const char *namespaceName)
{
    (void) schema;
    /* TODO: Pointer comparison instead? */
    if (xml_xsd10_str_equal(pctxt->targetNamespace, namespaceName))
	return (0);
    if (xml_xsd10_str_equal(xml_xsd10_ns, namespaceName))
	return (0);
    /*
    * Check if the referenced namespace was <import>ed.
    */
    if (XSD10_WXS_BUCKET(pctxt)->relations != NULL) {
	XmlXsd10SchemaRelation *rel;

	rel = XSD10_WXS_BUCKET(pctxt)->relations;
	do {
	    if (XSD10_WXS_IS_BUCKET_IMPMAIN(rel->type) &&
		xml_xsd10_str_equal(namespaceName, rel->importNamespace))
		return (0);
	    rel = rel->next;
	} while (rel != NULL);
    }
    /*
    * No matching <import>ed namespace found.
    */
    {
	XMQNode *n = (attr != NULL) ? (XMQNode *) attr : node;

	if (namespaceName == NULL)
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE, n, NULL,
		"References from this schema to components in no "
		"namespace are not allowed, since not indicated by an "
		"import statement", NULL, NULL);
	else
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_SRC_RESOLVE, n, NULL,
		"References from this schema to components in the "
		"namespace '%s' are not allowed, since not indicated by an "
		"import statement", namespaceName, NULL);
    }
    return (XML_XSD10_SCHEMAP_SRC_RESOLVE);
}

/**
 * Port of xmlSchemaParseLocalAttributes.
 *
 * Parses attribute uses and attribute declarations and
 * attribute group references.
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param child  a subtree containing XML Schema information
 * @param list  an item list
 * @param parentType  the hosting type where the attributes will be anchored
 * @param hasRefs  set if references were found
 */
int xml_xsd10_parse_local_attributes(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                        XMQNode **child, XmlXsd10ItemList **list,
			int parentType, int *hasRefs)
{
    void *item;

    while ((XSD10_IS_SCHEMA((*child), "attribute")) ||
           (XSD10_IS_SCHEMA((*child), "attributeGroup"))) {
        if (XSD10_IS_SCHEMA((*child), "attribute")) {
	    item = xml_xsd10_parse_local_attribute(ctxt, schema, *child,
		*list, parentType);
        } else {
            item = xml_xsd10_parse_attribute_group_ref(ctxt, schema, *child);
	    if ((item != NULL) && (hasRefs != NULL))
		*hasRefs = 1;
        }
	if (item != NULL) {
	    if (*list == NULL) {
		/* TODO: Customize grow factor. */
		*list = xml_xsd10_item_list_create();
		if (*list == NULL)
		    return(-1);
	    }
	    if (xml_xsd10_item_list_add_size(*list, 2, item) == -1)
		return(-1);
	}
        *child = xsd10_parse_next((*child));
    }
    return (0);
}

/**
 * Port of xmlSchemaParseAnnotation.
 *
 * parse a XML schema Attribute declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param node  a subtree containing XML Schema information
 * @param needed  needed
 * @returns -1 in case of error, 0 if the declaration is improper and
 *         1 in case of success.
 */
XmlXsd10Annot *xml_xsd10_parse_annotation(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int needed)
{
    XmlXsd10Annot *ret;
    XMQNode *child = NULL;
    XMQNode *attr;
    int barked = 0;

    /*
    * INFO: S4S completed.
    */
    /*
    * id = ID
    * {any attributes with non-schema namespace . . .}>
    * Content: (appinfo | documentation)*
    */
    if ((ctxt == NULL) || (node == NULL))
        return (NULL);
    if (needed)
	ret = xml_xsd10_new_annot(ctxt, node);
    else
	ret = NULL;
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (((xml_node_ns_uri(attr) == NULL) &&
	    (!xml_xsd10_str_equal(xml_node_name(attr),  "id"))) ||
	    ((xml_node_ns_uri(attr) != NULL) &&
	    xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns))) {

	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    while (child != NULL) {
	if (XSD10_IS_SCHEMA(child, "appinfo")) {
	    /* TODO: make available the content of "appinfo". */
	    /*
	    * source = anyURI
	    * {any attributes with non-schema namespace . . .}>
	    * Content: ({any})*
	    */
	    attr = xsd10_parse_first_attr(child);
	    while (attr != NULL) {
		if (((xml_node_ns_uri(attr) == NULL) &&
		     (!xml_xsd10_str_equal(xml_node_name(attr),  "source"))) ||
		     ((xml_node_ns_uri(attr) != NULL) &&
		      xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns))) {

		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
		attr = xml_node_next(attr);
	    }
	    xml_xsd10_p_val_attr(ctxt, NULL, child, "source",
		xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI), NULL);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "documentation")) {
	    /* TODO: make available the content of "documentation". */
	    /*
	    * source = anyURI
	    * {any attributes with non-schema namespace . . .}>
	    * Content: ({any})*
	    */
	    attr = xsd10_parse_first_attr(child);
	    while (attr != NULL) {
		if (xml_node_ns_uri(attr) == NULL) {
		    if (!xml_xsd10_str_equal(xml_node_name(attr),  "source")) {
			xml_xsd10_p_illegal_attr_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		    }
		} else {
		    if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns) ||
			(xml_xsd10_str_equal(xml_node_name(attr),  "lang") &&
			(!xml_xsd10_str_equal(xml_node_ns_uri(attr), XML_XSD10_XML_XML_NAMESPACE)))) {

			xml_xsd10_p_illegal_attr_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		    }
		}
		attr = xml_node_next(attr);
	    }
	    /*
	    * Attribute "xml:lang".
	    */
	    attr = xml_xsd10_get_prop_node_ns(child, (const char *) XML_XSD10_XML_XML_NAMESPACE, "lang");
	    if (attr != NULL)
		xml_xsd10_p_val_attr_node(ctxt, NULL, attr,
		xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_LANGUAGE), NULL);
	    child = xsd10_parse_next(child);
	} else {
	    if (!barked)
		xml_xsd10_p_content_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		    NULL, node, child, NULL, "(appinfo | documentation)*");
	    barked = 1;
	    child = xsd10_parse_next(child);
	}
    }

    return (ret);
}

/**
 * Port of xmlSchemaParseFacet.
 *
 * parse a XML schema Facet declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns the new type structure or NULL in case of error
 */
XmlXsd10Facet *xml_xsd10_parse_facet(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                    XMQNode *node)
{
    XmlXsd10Facet *facet;
    XMQNode *child = NULL;
    const char *value;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    facet = xml_xsd10_new_facet();
    if (facet == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    facet->node = node;
    value = xml_xsd10_get_prop(ctxt, node, "value");
    if (value == NULL) {
        xml_xsd10_p_err2(ctxt, node, child, XML_XSD10_SCHEMAP_FACET_NO_VALUE,
                       "Facet %s has no value\n", xml_node_name(node), NULL);
        xml_xsd10_free_facet(facet);
        return (NULL);
    }
    if (XSD10_IS_SCHEMA(node, "minInclusive")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MININCLUSIVE;
    } else if (XSD10_IS_SCHEMA(node, "minExclusive")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE;
    } else if (XSD10_IS_SCHEMA(node, "maxInclusive")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE;
    } else if (XSD10_IS_SCHEMA(node, "maxExclusive")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE;
    } else if (XSD10_IS_SCHEMA(node, "totalDigits")) {
        facet->type = XML_XSD10_SCHEMA_FACET_TOTALDIGITS;
    } else if (XSD10_IS_SCHEMA(node, "fractionDigits")) {
        facet->type = XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS;
    } else if (XSD10_IS_SCHEMA(node, "pattern")) {
        facet->type = XML_XSD10_SCHEMA_FACET_PATTERN;
    } else if (XSD10_IS_SCHEMA(node, "enumeration")) {
        facet->type = XML_XSD10_SCHEMA_FACET_ENUMERATION;
    } else if (XSD10_IS_SCHEMA(node, "whiteSpace")) {
        facet->type = XML_XSD10_SCHEMA_FACET_WHITESPACE;
    } else if (XSD10_IS_SCHEMA(node, "length")) {
        facet->type = XML_XSD10_SCHEMA_FACET_LENGTH;
    } else if (XSD10_IS_SCHEMA(node, "maxLength")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MAXLENGTH;
    } else if (XSD10_IS_SCHEMA(node, "minLength")) {
        facet->type = XML_XSD10_SCHEMA_FACET_MINLENGTH;
    } else {
        xml_xsd10_p_err2(ctxt, node, child, XML_XSD10_SCHEMAP_UNKNOWN_FACET_TYPE,
                       "Unknown facet type %s\n", xml_node_name(node), NULL);
        xml_xsd10_free_facet(facet);
        return (NULL);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    facet->value = value;
    if ((facet->type != XML_XSD10_SCHEMA_FACET_PATTERN) &&
	(facet->type != XML_XSD10_SCHEMA_FACET_ENUMERATION)) {
	const char *fixed;

	fixed = xml_xsd10_get_prop(ctxt, node, "fixed");
	if (fixed != NULL) {
	    if (xml_xsd10_str_equal(fixed,  "true"))
		facet->fixed = 1;
	}
    }
    child = xsd10_parse_first_child(node);

    if (XSD10_IS_SCHEMA(child, "annotation")) {
        facet->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
        xml_xsd10_p_err2(ctxt, node, child, XML_XSD10_SCHEMAP_UNKNOWN_FACET_CHILD,
                       "Facet %s has unexpected child content\n",
                       xml_node_name(node), NULL);
    }
    return (facet);
}

/**
 * Port of xmlSchemaParseWildcardNs.
 *
 * Parses the attribute "processContents" and "namespace"
 * of a xsd:anyAttribute and xsd:any.
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema parser context
 * @param schema  unused
 * @param wildc  the wildcard, already created
 * @param node  a subtree containing XML Schema information
 * @returns 0 if everything goes fine, a positive error code
 * if something is not valid and -1 if an internal error occurs.
 */
int xml_xsd10_parse_wildcard_ns(XmlXsd10ParserCtxt *ctxt,
			 XmlXsd10Schema *schema ,
			 XmlXsd10Wildcard *wildc,
			 XMQNode *node)
{
    (void) schema;
    const char *pc, *ns, *dictnsItem;
    int ret = 0;
    char *nsItem;
    XmlXsd10WildcardNs *tmp, *lastNs = NULL;
    XMQNode *attr;

    pc = xml_xsd10_get_prop(ctxt, node, "processContents");
    if ((pc == NULL)
        || (xml_xsd10_str_equal(pc, (const char *) "strict"))) {
        wildc->processContents = XML_XSD10_SCHEMAS_ANY_STRICT;
    } else if (xml_xsd10_str_equal(pc, (const char *) "skip")) {
        wildc->processContents = XML_XSD10_SCHEMAS_ANY_SKIP;
    } else if (xml_xsd10_str_equal(pc, (const char *) "lax")) {
        wildc->processContents = XML_XSD10_SCHEMAS_ANY_LAX;
    } else {
        xml_xsd10_p_simple_type_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    NULL, node,
	    NULL, "(strict | skip | lax)", pc,
	    NULL, NULL, NULL);
        wildc->processContents = XML_XSD10_SCHEMAS_ANY_STRICT;
	ret = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
    }
    /*
     * Build the namespace constraints.
     */
    attr = xml_xsd10_get_prop_node(node, "namespace");
    ns = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
    if (ns == NULL)
        return (-1);
    if ((attr == NULL) || (xml_xsd10_str_equal(ns,  "##any")))
	wildc->any = 1;
    else if (xml_xsd10_str_equal(ns,  "##other")) {
	wildc->negNsSet = xml_xsd10_new_wildcard_ns_constraint(ctxt);
	if (wildc->negNsSet == NULL) {
	    return (-1);
	}
	wildc->negNsSet->value = ctxt->targetNamespace;
    } else {
	const char *end, *cur;

	cur = ns;
	do {
	    while (XSD10_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    nsItem = xml_xsd10_strndup(cur, end - cur);
	    if ((xml_xsd10_str_equal(nsItem,  "##other")) ||
		    (xml_xsd10_str_equal(nsItem,  "##any"))) {
		xml_xsd10_p_simple_type_err(ctxt,
		    XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER,
		    NULL, (XMQNode *) attr,
		    NULL,
		    "((##any | ##other) | List of (xs:anyURI | "
		    "(##targetNamespace | ##local)))",
		    nsItem, NULL, NULL, NULL);
		ret = XML_XSD10_SCHEMAP_WILDCARD_INVALID_NS_MEMBER;
	    } else {
		if (xml_xsd10_str_equal(nsItem,  "##targetNamespace")) {
		    dictnsItem = ctxt->targetNamespace;
		} else if (xml_xsd10_str_equal(nsItem,  "##local")) {
		    dictnsItem = NULL;
		} else {
		    /*
		    * Validate the item (anyURI).
		    */
		    xml_xsd10_p_val_attr_node_value(ctxt, NULL, attr,
			nsItem, xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI));
		    dictnsItem = xml_xsd10_dict_lookup(ctxt->dict, nsItem, -1);
		}
		/*
		* Avoid duplicate namespaces.
		*/
		tmp = wildc->nsSet;
		while (tmp != NULL) {
		    if (dictnsItem == tmp->value)
			break;
		    tmp = tmp->next;
		}
		if (tmp == NULL) {
		    tmp = xml_xsd10_new_wildcard_ns_constraint(ctxt);
		    if (tmp == NULL) {
			free(nsItem);
			return (-1);
		    }
		    tmp->value = dictnsItem;
		    tmp->next = NULL;
		    if (wildc->nsSet == NULL)
			wildc->nsSet = tmp;
		    else if (lastNs != NULL)
			lastNs->next = tmp;
		    lastNs = tmp;
		}

	    }
	    free(nsItem);
	    cur = end;
	} while (*cur != 0);
    }
    return (ret);
}

/** Port of xmlSchemaPCheckParticleCorrect_2. */
int xml_xsd10_p_check_particle_correct_2(XmlXsd10ParserCtxt *ctxt,
				 XmlXsd10Particle *item ,
				 XMQNode *node,
				 int minOccurs,
				 int maxOccurs) {
    (void) item;


    if ((maxOccurs == 0) && ( minOccurs == 0))
	return (0);
    if (maxOccurs != XML_XSD10_UNBOUNDED) {
	/*
	* TODO: Maybe we should better not create the particle,
	* if min/max is invalid, since it could confuse the build of the
	* content model.
	*/
	/*
	* 3.9.6 Schema Component Constraint: Particle Correct
	*
	*/
	if (maxOccurs < 1) {
	    /*
	    * 2.2 {max occurs} must be greater than or equal to 1.
	    */
	    xml_xsd10_p_custom_attr_err(ctxt,
		XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2,
		NULL, NULL,
		xml_xsd10_get_prop_node(node, "maxOccurs"),
		"The value must be greater than or equal to 1");
	    return (XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_2);
	} else if (minOccurs > maxOccurs) {
	    /*
	    * 2.1 {min occurs} must not be greater than {max occurs}.
	    */
	    xml_xsd10_p_custom_attr_err(ctxt,
		XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1,
		NULL, NULL,
		xml_xsd10_get_prop_node(node, "minOccurs"),
		"The value must not be greater than the value of 'maxOccurs'");
	    return (XML_XSD10_SCHEMAP_P_PROPS_CORRECT_2_1);
	}
    }
    return (0);
}

/**
 * Port of xmlSchemaParseAny.
 *
 * Parsea a XML schema `<any>` element. A particle and wildcard
 * will be created (except if minOccurs==maxOccurs==0, in this case
 * nothing will be created).
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns the particle or NULL in case of error or if minOccurs==maxOccurs==0
 */
XmlXsd10Particle *xml_xsd10_parse_any(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                  XMQNode *node)
{
    XmlXsd10Particle *particle;
    XMQNode *child = NULL;
    XmlXsd10Wildcard *wild;
    int min, max;
    XMQNode *attr;
    XmlXsd10Annot *annot = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "minOccurs")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "maxOccurs")) &&
	        (!xml_xsd10_str_equal(xml_node_name(attr),  "namespace")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "processContents"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * minOccurs/maxOccurs.
    */
    max = xml_xsd10_get_max_occurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
	"(xs:nonNegativeInteger | unbounded)");
    min = xml_xsd10_get_min_occurs(ctxt, node, 0, -1, 1,
	"xs:nonNegativeInteger");
    xml_xsd10_p_check_particle_correct_2(ctxt, NULL, node, min, max);
    /*
    * Create & parse the wildcard.
    */
    wild = xml_xsd10_add_wildcard(ctxt, schema, XML_XSD10_SCHEMA_TYPE_ANY, node);
    if (wild == NULL)
	return (NULL);
    xml_xsd10_parse_wildcard_ns(ctxt, schema, wild, node);
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?)");
    }
    /*
    * No component if minOccurs==maxOccurs==0.
    */
    if ((min == 0) && (max == 0)) {
	/* Don't free the wildcard, since it's already on the list. */
	return (NULL);
    }
    /*
    * Create the particle.
    */
    particle = xml_xsd10_add_particle(ctxt, node, min, max);
    if (particle == NULL)
        return (NULL);
    particle->annot = annot;
    particle->children = (XmlXsd10TreeItem *) wild;

    return (particle);
}

/**
 * Port of xmlSchemaParseNotation.
 *
 * parse a XML schema Notation declaration
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns the new structure or NULL in case of error
 */
XmlXsd10Notation *xml_xsd10_parse_notation(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                       XMQNode *node)
{
    const char *name;
    XmlXsd10Notation *ret;
    XMQNode *child = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    name = xml_xsd10_get_prop(ctxt, node, "name");
    if (name == NULL) {
        xml_xsd10_p_err2(ctxt, node, child, XML_XSD10_SCHEMAP_NOTATION_NO_NAME,
                       "Notation has no name\n", NULL, NULL);
        return (NULL);
    }
    ret = xml_xsd10_add_notation(ctxt, schema, name,
	ctxt->targetNamespace, node);
    if (ret == NULL)
        return (NULL);
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");

    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        ret->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?)");
    }

    return (ret);
}

/**
 * Port of xmlSchemaParseAnyAttribute.
 *
 * parse a XML schema AnyAttribute declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns a wildcard or NULL.
 */
XmlXsd10Wildcard *xml_xsd10_parse_any_attribute(XmlXsd10ParserCtxt *ctxt,
                           XmlXsd10Schema *schema, XMQNode *node)
{
    XmlXsd10Wildcard *ret;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    ret = xml_xsd10_add_wildcard(ctxt, schema, XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE,
	node);
    if (ret == NULL) {
        return (NULL);
    }
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
	        (!xml_xsd10_str_equal(xml_node_name(attr),  "namespace")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "processContents"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * Parse the namespace list.
    */
    if (xml_xsd10_parse_wildcard_ns(ctxt, schema, ret, node) != 0)
	return (NULL);
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        ret->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?)");
    }

    return (ret);
}


/**
 * Port of xmlSchemaParseLocalAttribute.
 *
 * parse a XML schema Attribute declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param pctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param uses  attribute uses list
 * @param parentType  parent type
 * @returns the attribute declaration.
 */
XmlXsd10BasicItem *xml_xsd10_parse_local_attribute(XmlXsd10ParserCtxt *pctxt,
			     XmlXsd10Schema *schema,
			     XMQNode *node,
			     XmlXsd10ItemList *uses,
			     int parentType)
{
    const char *attrValue, *name = NULL, *ns = NULL;
    XmlXsd10AttributeUse *use = NULL;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *tmpNs = NULL, *tmpName = NULL, *defValue = NULL;
    int isRef = 0, occurs = XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL;
    int	nberrors, hasForm = 0, defValueType = 0;

#define XSD10_WXS_ATTR_DEF_VAL_DEFAULT 1
#define XSD10_WXS_ATTR_DEF_VAL_FIXED 2

    /*
     * 3.2.3 Constraints on XML Representations of Attribute Declarations
     */

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    attr = xml_xsd10_get_prop_node(node, "ref");
    if (attr != NULL) {
	if (xml_xsd10_p_val_attr_node_q_name(pctxt, schema,
	    NULL, attr, &tmpNs, &tmpName) != 0) {
	    return (NULL);
	}
	if (xml_xsd10_check_reference(pctxt, schema, node, attr, tmpNs) != 0)
	    return(NULL);
	isRef = 1;
    }
    nberrors = pctxt->nberrors;
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if (isRef) {
		if (xml_xsd10_str_equal(xml_node_name(attr),  "id")) {
		    xml_xsd10_p_val_attr_node_id(pctxt, attr);
		    goto attr_next;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "ref")) {
		    goto attr_next;
		}
	    } else {
		if (xml_xsd10_str_equal(xml_node_name(attr),  "name")) {
		    goto attr_next;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "id")) {
		    xml_xsd10_p_val_attr_node_id(pctxt, attr);
		    goto attr_next;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "type")) {
		    xml_xsd10_p_val_attr_node_q_name(pctxt, schema, NULL,
			attr, &tmpNs, &tmpName);
		    goto attr_next;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "form")) {
		    /*
		    * Evaluate the target namespace
		    */
		    hasForm = 1;
		    attrValue = xml_xsd10_get_node_content(pctxt,
			(XMQNode *) attr);
		    if (xml_xsd10_str_equal(attrValue,  "qualified")) {
			ns = pctxt->targetNamespace;
		    } else if (!xml_xsd10_str_equal(attrValue,  "unqualified"))
		    {
			xml_xsd10_p_simple_type_err(pctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
			    NULL, (XMQNode *) attr,
			    NULL, "(qualified | unqualified)",
			    attrValue, NULL, NULL, NULL);
		    }
		    goto attr_next;
		}
	    }
	    if (xml_xsd10_str_equal(xml_node_name(attr),  "use")) {

		attrValue = xml_xsd10_get_node_content(pctxt, (XMQNode *) attr);
		/* TODO: Maybe we need to normalize the value beforehand. */
		if (xml_xsd10_str_equal(attrValue,  "optional"))
		    occurs = XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL;
		else if (xml_xsd10_str_equal(attrValue,  "prohibited"))
		    occurs = XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED;
		else if (xml_xsd10_str_equal(attrValue,  "required"))
		    occurs = XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED;
		else {
		    xml_xsd10_p_simple_type_err(pctxt,
			XML_XSD10_SCHEMAP_INVALID_ATTR_USE,
			NULL, (XMQNode *) attr,
			NULL, "(optional | prohibited | required)",
			attrValue, NULL, NULL, NULL);
		}
		goto attr_next;
	    } else if (xml_xsd10_str_equal(xml_node_name(attr),  "default")) {
		/*
		* 3.2.3 : 1
		* default and fixed must not both be present.
		*/
		if (defValue) {
		    xml_xsd10_p_mutual_excl_attr_err(pctxt,
			XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
			NULL, attr, "default", "fixed");
		} else {
		    defValue = xml_xsd10_get_node_content(pctxt, (XMQNode *) attr);
		    defValueType = XSD10_WXS_ATTR_DEF_VAL_DEFAULT;
		}
		goto attr_next;
	    } else if (xml_xsd10_str_equal(xml_node_name(attr),  "fixed")) {
		/*
		* 3.2.3 : 1
		* default and fixed must not both be present.
		*/
		if (defValue) {
		    xml_xsd10_p_mutual_excl_attr_err(pctxt,
			XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
			NULL, attr, "default", "fixed");
		} else {
		    defValue = xml_xsd10_get_node_content(pctxt, (XMQNode *) attr);
		    defValueType = XSD10_WXS_ATTR_DEF_VAL_FIXED;
		}
		goto attr_next;
	    }
	} else if (! xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns))
	    goto attr_next;

	xml_xsd10_p_illegal_attr_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);

attr_next:
	attr = xml_node_next(attr);
    }
    /*
    * 3.2.3 : 2
    * If default and use are both present, use must have
    * the actual value optional.
    */
    if ((defValueType == XSD10_WXS_ATTR_DEF_VAL_DEFAULT) &&
	(occurs != XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL)) {
	xml_xsd10_p_simple_type_err(pctxt,
	    XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_2,
	    NULL, node, NULL,
	    "(optional | prohibited | required)", NULL,
	    "The value of the attribute 'use' must be 'optional' "
	    "if the attribute 'default' is present",
	    NULL, NULL);
    }
    /*
    * We want correct attributes.
    */
    if (nberrors != pctxt->nberrors)
	return(NULL);
    if (! isRef) {
	XmlXsd10Attribute *attrDecl;

	/* TODO: move XML_SCHEMAS_QUALIF_ATTR to the parser. */
	if ((! hasForm) && (schema->flags & XML_XSD10_SCHEMAS_QUALIF_ATTR))
	    ns = pctxt->targetNamespace;
	/*
	* 3.2.6 Schema Component Constraint: xsi: Not Allowed
	* TODO: Move this to the component layer.
	*/
	if (xml_xsd10_str_equal(ns, xml_xsd10_instance_ns)) {
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_NO_XSI,
		node, NULL,
		"The target namespace must not match '%s'",
		xml_xsd10_instance_ns, NULL);
	}
	attr = xml_xsd10_get_prop_node(node, "name");
	if (attr == NULL) {
	    xml_xsd10_p_missing_attr_err(pctxt, XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		NULL, node, "name", NULL);
	    return (NULL);
	}
	if (xml_xsd10_p_val_attr_node(pctxt, NULL, attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0) {
	    return (NULL);
	}
	/*
	* 3.2.6 Schema Component Constraint: xmlns Not Allowed
	* TODO: Move this to the component layer.
	*/
	if (xml_xsd10_str_equal(name,  "xmlns")) {
	    xml_xsd10_p_simple_type_err(pctxt,
		XML_XSD10_SCHEMAP_NO_XMLNS,
		NULL, (XMQNode *) attr,
		xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), NULL, NULL,
		"The value of the attribute must not match 'xmlns'",
		NULL, NULL);
	    return (NULL);
	}
	if (occurs == XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED)
	    goto check_children;
	/*
	* Create the attribute use component.
	*/
	use = xml_xsd10_add_attribute_use(pctxt, node);
	if (use == NULL)
	    return(NULL);
	use->occurs = occurs;
	/*
	* Create the attribute declaration.
	*/
	attrDecl = xml_xsd10_add_attribute(pctxt, schema, name, ns, node, 0);
	if (attrDecl == NULL)
	    return (NULL);
	if (tmpName != NULL) {
	    attrDecl->typeName = tmpName;
	    attrDecl->typeNs = tmpNs;
	}
	use->attrDecl = attrDecl;
	/*
	* Value constraint.
	*/
	if (defValue != NULL) {
	    attrDecl->defValue = defValue;
	    if (defValueType == XSD10_WXS_ATTR_DEF_VAL_FIXED)
		attrDecl->flags |= XML_XSD10_SCHEMAS_ATTR_FIXED;
	}
    } else if (occurs != XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED) {
	XmlXsd10QNameRef *ref;

	/*
	* Create the attribute use component.
	*/
	use = xml_xsd10_add_attribute_use(pctxt, node);
	if (use == NULL)
	    return(NULL);
	/*
	* We need to resolve the reference at later stage.
	*/
	XSD10_WXS_ADD_PENDING(pctxt, use);
	use->occurs = occurs;
	/*
	* Create a QName reference to the attribute declaration.
	*/
	ref = xml_xsd10_new_q_name_ref(pctxt, XML_XSD10_SCHEMA_TYPE_ATTRIBUTE,
	    tmpName, tmpNs);
	if (ref == NULL)
	    return(NULL);
	/*
	* Assign the reference. This will be substituted for the
	* referenced attribute declaration when the QName is resolved.
	*/
	use->attrDecl = XSD10_WXS_ATTR_CAST ref;
	/*
	* Value constraint.
	*/
	if (defValue != NULL)
	    use->defValue = defValue;
	if (defValueType == XSD10_WXS_ATTR_DEF_VAL_FIXED)
	    use->flags |= XML_XSD10_SCHEMA_ATTR_USE_FIXED;
    }

check_children:
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (occurs == XML_XSD10_SCHEMAS_ATTR_USE_PROHIBITED) {
	XmlXsd10AttributeUseProhib *prohib;

	if (XSD10_IS_SCHEMA(child, "annotation")) {
	    xml_xsd10_parse_annotation(pctxt, child, 0);
	    child = xsd10_parse_next(child);
	}
	if (child != NULL) {
	    xml_xsd10_p_content_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?)");
	}
	/*
	* Check for pointlessness of attribute prohibitions.
	*/
	if (parentType == XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP) {
	    xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
		node, NULL,
		"Skipping attribute use prohibition, since it is "
		"pointless inside an <attributeGroup>",
		NULL, NULL, NULL);
	    return(NULL);
	} else if (parentType == XML_XSD10_SCHEMA_TYPE_EXTENSION) {
	    xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
		node, NULL,
		"Skipping attribute use prohibition, since it is "
		"pointless when extending a type",
		NULL, NULL, NULL);
	    return(NULL);
	}
	if (! isRef) {
	    tmpName = name;
	    tmpNs = ns;
	}
	/*
	* Check for duplicate attribute prohibitions.
	*/
	if (uses) {
	    int i;

	    for (i = 0; i < uses->nbItems; i++) {
		use = (XmlXsd10AttributeUse *) uses->items[i];
		if ((use->type == XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB) &&
		    (tmpName == (XSD10_WXS_ATTR_PROHIB_CAST use)->name) &&
		    (tmpNs == (XSD10_WXS_ATTR_PROHIB_CAST use)->targetNamespace))
		{
		    char *str = NULL;

		    xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
			XML_XSD10_SCHEMAP_WARN_ATTR_POINTLESS_PROH,
			node, NULL,
			"Skipping duplicate attribute use prohibition '%s'",
			xml_xsd10_format_q_name(&str, tmpNs, tmpName),
			NULL, NULL);
		    XSD10_FREE_AND_NULL(str)
		    return(NULL);
		}
	    }
	}
	/*
	* Create the attribute prohibition helper component.
	*/
	prohib = xml_xsd10_add_attribute_use_prohib(pctxt);
	if (prohib == NULL)
	    return(NULL);
	prohib->node = node;
	prohib->name = tmpName;
	prohib->targetNamespace = tmpNs;
	if (isRef) {
	    /*
	    * We need at least to resolve to the attribute declaration.
	    */
	    XSD10_WXS_ADD_PENDING(pctxt, prohib);
	}
	return(XSD10_WXS_BASIC_CAST prohib);
    } else {
	if (XSD10_IS_SCHEMA(child, "annotation")) {
	    /*
	    * TODO: Should this go into the attr decl?
	    */
	    use->annot = xml_xsd10_parse_annotation(pctxt, child, 1);
	    child = xsd10_parse_next(child);
	}
	if (isRef) {
	    if (child != NULL) {
		if (XSD10_IS_SCHEMA(child, "simpleType"))
		    /*
		    * 3.2.3 : 3.2
		    * If ref is present, then all of <simpleType>,
		    * form and type must be absent.
		    */
		    xml_xsd10_p_content_err(pctxt,
			XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_3_2,
			NULL, node, child, NULL,
			"(annotation?)");
		else
		    xml_xsd10_p_content_err(pctxt,
			XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
			NULL, node, child, NULL,
			"(annotation?)");
	    }
	} else {
	    if (XSD10_IS_SCHEMA(child, "simpleType")) {
		if (XSD10_WXS_ATTRUSE_DECL(use)->typeName != NULL) {
		    /*
		    * 3.2.3 : 4
		    * type and <simpleType> must not both be present.
		    */
		    xml_xsd10_p_content_err(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4,
			NULL, node, child,
			"The attribute 'type' and the <simpleType> child "
			"are mutually exclusive", NULL);
		} else
		    XSD10_WXS_ATTRUSE_TYPEDEF(use) =
			xml_xsd10_parse_simple_type(pctxt, schema, child, 0);
		child = xsd10_parse_next(child);
	    }
	    if (child != NULL)
		xml_xsd10_p_content_err(pctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, simpleType?)");
	}
    }
    return (XSD10_WXS_BASIC_CAST use);
}


/** Port of xmlSchemaParseGlobalAttribute. */
XmlXsd10Attribute *xml_xsd10_parse_global_attribute(XmlXsd10ParserCtxt *pctxt,
			      XmlXsd10Schema *schema,
			      XMQNode *node)
{
    const char *attrValue;
    XmlXsd10Attribute *ret;
    XMQNode *child = NULL;
    XMQNode *attr;

    /*
     * Note that the w3c spec assumes the schema to be validated with schema
     * for schemas beforehand.
     *
     * 3.2.3 Constraints on XML Representations of Attribute Declarations
     */
    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /*
    * 3.2.3 : 3.1
    * One of ref or name must be present, but not both
    */
    attr = xml_xsd10_get_prop_node(node, "name");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(pctxt, XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "name", NULL);
	return (NULL);
    }
    if (xml_xsd10_p_val_attr_node(pctxt, NULL, attr,
	xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &attrValue) != 0) {
	return (NULL);
    }
    /*
    * 3.2.6 Schema Component Constraint: xmlns Not Allowed
    * TODO: Move this to the component layer.
    */
    if (xml_xsd10_str_equal(attrValue,  "xmlns")) {
	xml_xsd10_p_simple_type_err(pctxt,
	    XML_XSD10_SCHEMAP_NO_XMLNS,
	    NULL, (XMQNode *) attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), NULL, NULL,
	    "The value of the attribute must not match 'xmlns'",
	    NULL, NULL);
	return (NULL);
    }
    /*
    * 3.2.6 Schema Component Constraint: xsi: Not Allowed
    * TODO: Move this to the component layer.
    *       Or better leave it here and add it to the component layer
    *       if we have a schema construction API.
    */
    if (xml_xsd10_str_equal(pctxt->targetNamespace, xml_xsd10_instance_ns)) {
	xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
	    XML_XSD10_SCHEMAP_NO_XSI, node, NULL,
	    "The target namespace must not match '%s'",
	    xml_xsd10_instance_ns, NULL);
    }

    ret = xml_xsd10_add_attribute(pctxt, schema, attrValue,
	pctxt->targetNamespace, node, 1);
    if (ret == NULL)
	return (NULL);
    ret->flags |= XML_XSD10_SCHEMAS_ATTR_GLOBAL;

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "default")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "fixed")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "type")))
	    {
		xml_xsd10_p_illegal_attr_err(pctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_q_name(pctxt, schema, NULL,
	node, "type", &ret->typeNs, &ret->typeName);

    xml_xsd10_p_val_attr_id(pctxt, node,  "id");
    /*
    * Attribute "fixed".
    */
    ret->defValue = xml_xsd10_get_prop(pctxt, node, "fixed");
    if (ret->defValue != NULL)
	ret->flags |= XML_XSD10_SCHEMAS_ATTR_FIXED;
    /*
    * Attribute "default".
    */
    attr = xml_xsd10_get_prop_node(node, "default");
    if (attr != NULL) {
	/*
	* 3.2.3 : 1
	* default and fixed must not both be present.
	*/
	if (ret->flags & XML_XSD10_SCHEMAS_ATTR_FIXED) {
	    xml_xsd10_p_mutual_excl_attr_err(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_1,
		XSD10_WXS_BASIC_CAST ret, attr, "default", "fixed");
	} else
	    ret->defValue = xml_xsd10_get_node_content(pctxt, (XMQNode *) attr);
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        ret->annot = xml_xsd10_parse_annotation(pctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (XSD10_IS_SCHEMA(child, "simpleType")) {
	if (ret->typeName != NULL) {
	    /*
	    * 3.2.3 : 4
	    * type and <simpleType> must not both be present.
	    */
	    xml_xsd10_p_content_err(pctxt, XML_XSD10_SCHEMAP_SRC_ATTRIBUTE_4,
		NULL, node, child,
		"The attribute 'type' and the <simpleType> child "
		"are mutually exclusive", NULL);
	} else
	    ret->subtypes = xml_xsd10_parse_simple_type(pctxt, schema, child, 0);
	child = xsd10_parse_next(child);
    }
    if (child != NULL)
	xml_xsd10_p_content_err(pctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?, simpleType?)");

    return (ret);
}

/**
 * Port of xmlSchemaParseAttributeGroupRef.
 *
 * Parse an attribute group definition reference.
 * Note that a reference to an attribute group does not
 * correspond to any component at all.
 * *WARNING* this interface is highly subject to change
 *
 * @param pctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns the attribute group or NULL in case of error.
 */
XmlXsd10QNameRef *xml_xsd10_parse_attribute_group_ref(XmlXsd10ParserCtxt *pctxt,
				XmlXsd10Schema *schema,
				XMQNode *node)
{
    XmlXsd10QNameRef *ret;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *refNs = NULL, *ref = NULL;

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    attr = xml_xsd10_get_prop_node(node, "ref");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "ref", NULL);
	return (NULL);
    }
    xml_xsd10_p_val_attr_node_q_name(pctxt, schema,
	NULL, attr, &refNs, &ref);
    if (xml_xsd10_check_reference(pctxt, schema, node, attr, refNs) != 0)
	return(NULL);

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "ref")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "id")))
	    {
		xml_xsd10_p_illegal_attr_err(pctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /* Attribute ID */
    xml_xsd10_p_val_attr_id(pctxt, node,  "id");

    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* TODO: We do not have a place to store the annotation, do we?
	*/
        xml_xsd10_parse_annotation(pctxt, child, 0);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?)");
    }

    /*
    * Handle attribute group redefinitions.
    */
    if (pctxt->isRedefine && pctxt->redef &&
	(pctxt->redef->item->type ==
	    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP) &&
	(ref == pctxt->redef->refName) &&
	(refNs == pctxt->redef->refTargetNs))
    {
	/*
	* SPEC src-redefine:
	* (7.1) "If it has an <attributeGroup> among its contents
	* the `actual value` of whose ref [attribute] is the same
	* as the `actual value` of its own name attribute plus
	* target namespace, then it must have exactly one such group."
	*/
	if (pctxt->redefCounter != 0) {
	    char *str = NULL;

	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		XML_XSD10_SCHEMAP_SRC_REDEFINE, node, NULL,
		"The redefining attribute group definition "
		"'%s' must not contain more than one "
		"reference to the redefined definition",
		xml_xsd10_format_q_name(&str, refNs, ref), NULL);
	    XSD10_FREE_AND_NULL(str);
	    return(NULL);
	}
	pctxt->redefCounter++;
	/*
	* URGENT TODO: How to ensure that the reference will not be
	* handled by the normal component resolution mechanism?
	*/
	ret = xml_xsd10_new_q_name_ref(pctxt,
	    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, ref, refNs);
	if (ret == NULL)
	    return(NULL);
	ret->node = node;
	pctxt->redef->reference = XSD10_WXS_BASIC_CAST ret;
    } else {
	/*
	* Create a QName-reference helper component. We will substitute this
	* component for the attribute uses of the referenced attribute group
	* definition.
	*/
	ret = xml_xsd10_new_q_name_ref(pctxt,
	    XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, ref, refNs);
	if (ret == NULL)
	    return(NULL);
	ret->node = node;
	/* Add to pending items, to be able to resolve the reference. */
	XSD10_WXS_ADD_PENDING(pctxt, ret);
    }
    return (ret);
}

/**
 * Port of xmlSchemaParseAttributeGroupDefinition.
 *
 * parse a XML schema Attribute Group declaration
 * *WARNING* this interface is highly subject to change
 *
 * @param pctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns the attribute group definition or NULL in case of error.
 */
XmlXsd10AttributeGroup *xml_xsd10_parse_attribute_group_definition(XmlXsd10ParserCtxt *pctxt,
				       XmlXsd10Schema *schema,
				       XMQNode *node)
{
    const char *name;
    XmlXsd10AttributeGroup *ret;
    XMQNode *child = NULL;
    XMQNode *attr;
    int hasRefs = 0;
    int res;

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    attr = xml_xsd10_get_prop_node(node, "name");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "name", NULL);
	return (NULL);
    }
    /*
    * The name is crucial, exit if invalid.
    */
    if (xml_xsd10_p_val_attr_node(pctxt,
	NULL, attr,
	xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0) {
	return (NULL);
    }
    ret = xml_xsd10_add_attribute_group_definition(pctxt, schema,
	name, pctxt->targetNamespace, node);
    if (ret == NULL)
	return (NULL);
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "id")))
	    {
		xml_xsd10_p_illegal_attr_err(pctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /* Attribute ID */
    xml_xsd10_p_val_attr_id(pctxt, node,  "id");
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        ret->annot = xml_xsd10_parse_annotation(pctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    /*
    * Parse contained attribute decls/refs.
    */
    res = xml_xsd10_parse_local_attributes(pctxt, schema, &child,
	(XmlXsd10ItemList **) &(ret->attrUses),
	XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP, &hasRefs);
    if (hasRefs)
	ret->flags |= XML_XSD10_SCHEMAS_ATTRGROUP_HAS_REFS;
    if (res == -1)
	return(NULL);
    /*
    * Parse the attribute wildcard.
    */
    if (XSD10_IS_SCHEMA(child, "anyAttribute")) {
	ret->attributeWildcard = xml_xsd10_parse_any_attribute(pctxt,
	    schema, child);
	child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?, ((attribute | attributeGroup)*, anyAttribute?))");
    }
    return (ret);
}

/**
 * Port of xmlSchemaPValAttrFormDefault.
 *
 * @param value  the value
 * @param flags  the flags to be modified
 * @param flagQualified  the specific flag for "qualified"
 * @returns 0 if the value is valid, 1 otherwise.
 */
int xml_xsd10_p_val_attr_form_default(const char *value,
			     int *flags,
			     int flagQualified)
{
    if (xml_xsd10_str_equal(value,  "qualified")) {
	if  ((*flags & flagQualified) == 0)
	    *flags |= flagQualified;
    } else if (!xml_xsd10_str_equal(value,  "unqualified"))
	return (1);

    return (0);
}

/**
 * Port of xmlSchemaPValAttrBlockFinal.
 *
 * Validates the value of the attribute "final" and "block". The value
 * is converted into the specified flag values and returned in `flags`.
 *
 * @param value  the value
 * @param flags  the flags to be modified
 * @param flagAll  the specific flag for "\#all"
 * @param flagExtension  the specific flag for "extension"
 * @param flagRestriction  the specific flag for "restriction"
 * @param flagSubstitution  the specific flag for "substitution"
 * @param flagList  the specific flag for "list"
 * @param flagUnion  the specific flag for "union"
 * @returns 0 if the value is valid, 1 otherwise.
 */

int xml_xsd10_p_val_attr_block_final(const char *value,
			    int *flags,
			    int flagAll,
			    int flagExtension,
			    int flagRestriction,
			    int flagSubstitution,
			    int flagList,
			    int flagUnion)
{
    int ret = 0;

    /*
    * TODO: This does not check for duplicate entries.
    */
    if ((flags == NULL) || (value == NULL))
	return (-1);
    if (value[0] == 0)
	return (0);
    if (xml_xsd10_str_equal(value,  "#all")) {
	if (flagAll != -1)
	    *flags |= flagAll;
	else {
	    if (flagExtension != -1)
		*flags |= flagExtension;
	    if (flagRestriction != -1)
		*flags |= flagRestriction;
	    if (flagSubstitution != -1)
		*flags |= flagSubstitution;
	    if (flagList != -1)
		*flags |= flagList;
	    if (flagUnion != -1)
		*flags |= flagUnion;
	}
    } else {
	const char *end, *cur = value;
	char *item;

	do {
	    while (XSD10_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    item = xml_xsd10_strndup(cur, end - cur);
	    if (xml_xsd10_str_equal(item,  "extension")) {
		if (flagExtension != -1) {
		    if ((*flags & flagExtension) == 0)
			*flags |= flagExtension;
		} else
		    ret = 1;
	    } else if (xml_xsd10_str_equal(item,  "restriction")) {
		if (flagRestriction != -1) {
		    if ((*flags & flagRestriction) == 0)
			*flags |= flagRestriction;
		} else
		    ret = 1;
	    } else if (xml_xsd10_str_equal(item,  "substitution")) {
		if (flagSubstitution != -1) {
		    if ((*flags & flagSubstitution) == 0)
			*flags |= flagSubstitution;
		} else
		    ret = 1;
	    } else if (xml_xsd10_str_equal(item,  "list")) {
		if (flagList != -1) {
		    if ((*flags & flagList) == 0)
			*flags |= flagList;
		} else
		    ret = 1;
	    } else if (xml_xsd10_str_equal(item,  "union")) {
		if (flagUnion != -1) {
		    if ((*flags & flagUnion) == 0)
			*flags |= flagUnion;
		} else
		    ret = 1;
	    } else
		ret = 1;
	    if (item != NULL)
		free(item);
	    cur = end;
	} while ((ret == 0) && (*cur != 0));
    }

    return (ret);
}

/** Port of xmlSchemaCheckCSelectorXPath. */
int xml_xsd10_check_c_selector_x_path(XmlXsd10ParserCtxt *ctxt,
			     XmlXsd10IDC *idc,
			     XmlXsd10IDCSelect *selector,
			     XMQNode *attr,
			     int isField)
{
    XMQNode *node;

    /*
    * c-selector-xpath:
    * Schema Component Constraint: Selector Value OK
    *
    * TODO: 1 The {selector} must be a valid XPath expression, as defined
    * in [XPath].
    */
    if (selector == NULL) {
	xml_xsd10_p_err(ctxt, idc->node,
	    XML_XSD10_SCHEMAP_INTERNAL,
	    "Internal error: xmlSchemaCheckCSelectorXPath, "
	    "the selector is not specified.\n", NULL, NULL);
	return (-1);
    }
    if (attr == NULL)
	node = idc->node;
    else
	node = (XMQNode *) attr;
    if (selector->xpath == NULL) {
	xml_xsd10_p_custom_err(ctxt,
	    /* TODO: Adjust error code. */
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    NULL, node,
	    "The XPath expression of the selector is not valid", NULL);
	return (XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE);
    } else {
	const char **nsArray = NULL;
	XmlNs **nsList = NULL;
	/*
	* Compile the XPath expression.
	*/
	/*
	* TODO: We need the array of in-scope namespaces for compilation.
	* TODO: Call xmlPatterncompile with different options for selector/
	* field.
	*/
	if (attr == NULL)
	    nsList = NULL;
	else
	    nsList = xml_xsd10_tree_get_ns_list(xml_node_parent(attr));
	/*
	* Build an array of prefixes and namespaces.
	*/
	if (nsList != NULL) {
	    int i, count = 0;

	    for (i = 0; nsList[i] != NULL; i++)
		count++;

	    nsArray = (const char **) malloc(
		(count * 2 + 1) * sizeof(const char *));
	    if (nsArray == NULL) {
		xml_xsd10_p_err_memory(ctxt);
		free(nsList);
		return (-1);
	    }
	    for (i = 0; i < count; i++) {
		nsArray[2 * i] = xml_ns_href(nsList[i]);
		nsArray[2 * i + 1] = xml_ns_prefix(nsList[i]);
	    }
	    nsArray[count * 2] = NULL;
	    free(nsList);
	}
	/*
	* TODO: Differentiate between "selector" and "field".
	*/
	if (isField)
	    selector->xpathComp = (void *) xml_pattern_compile(selector->xpath,
		XML_PATTERN_FLAG_XSFIELD, nsArray);
	else
	    selector->xpathComp = (void *) xml_pattern_compile(selector->xpath,
		XML_PATTERN_FLAG_XSSEL, nsArray);
	if (nsArray != NULL)
	    free((char **) nsArray);

	if (selector->xpathComp == NULL) {
	    xml_xsd10_p_custom_err(ctxt,
		/* TODO: Adjust error code? */
		XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		NULL, node,
		"The XPath expression '%s' could not be "
		"compiled", selector->xpath);
	    return (XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE);
	}
    }
    return (0);
}

#define XSD10_ADD_ANNOTATION(annot)   \
    XmlXsd10Annot *cur = item->annot; \
    if (item->annot == NULL) {  \
	item->annot = annot;    \
	return (annot);         \
    }                           \
    cur = item->annot;          \
    while (cur->next != NULL) { \
	cur = cur->next;	\
    }                           \
    cur->next = annot;

/**
 * Port of xmlSchemaAddAnnotation.
 *
 * Adds the annotation to the given schema component.
 *
 * @param annItem  the schema component
 * @param annot  the annotation
 * @returns the given annotation.
 */
XmlXsd10Annot *xml_xsd10_add_annotation(XmlXsd10AnnotItem *annItem,
		       XmlXsd10Annot *annot)
{
    if ((annItem == NULL) || (annot == NULL))
	return (NULL);
    switch (annItem->type) {
	case XML_XSD10_SCHEMA_TYPE_ELEMENT: {
		XmlXsd10Element *item = (XmlXsd10Element *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE: {
		XmlXsd10Attribute *item = (XmlXsd10Attribute *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
	case XML_XSD10_SCHEMA_TYPE_ANY: {
		XmlXsd10Wildcard *item = (XmlXsd10Wildcard *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_PARTICLE:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE: {
		XmlXsd10AnnotItem *item = (XmlXsd10AnnotItem *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP: {
		XmlXsd10AttributeGroup *item =
		    (XmlXsd10AttributeGroup *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_NOTATION: {
		XmlXsd10Notation *item = (XmlXsd10Notation *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
	case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
	case XML_XSD10_SCHEMA_FACET_PATTERN:
	case XML_XSD10_SCHEMA_FACET_ENUMERATION:
	case XML_XSD10_SCHEMA_FACET_WHITESPACE:
	case XML_XSD10_SCHEMA_FACET_LENGTH:
	case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
	case XML_XSD10_SCHEMA_FACET_MINLENGTH: {
		XmlXsd10Facet *item = (XmlXsd10Facet *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	case XML_XSD10_SCHEMA_TYPE_COMPLEX: {
		XmlXsd10Type *item = (XmlXsd10Type *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_GROUP: {
		XmlXsd10ModelGroupDef *item = (XmlXsd10ModelGroupDef *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	case XML_XSD10_SCHEMA_TYPE_CHOICE:
	case XML_XSD10_SCHEMA_TYPE_ALL: {
		XmlXsd10ModelGroup *item = (XmlXsd10ModelGroup *) annItem;
		XSD10_ADD_ANNOTATION(annot)
	    }
	    break;
	default:
	     xml_xsd10_p_custom_err(NULL,
		XML_XSD10_SCHEMAP_INTERNAL,
		NULL, NULL,
		"Internal error: xmlSchemaAddAnnotation, "
		"The item is not a annotated schema component", NULL);
	     break;
    }
    return (annot);
}

/**
 * Port of xmlSchemaParseIDCSelectorAndField.
 *
 * Parses a XML Schema identity-constraint definition's
 * `<selector>` and `<field>` elements.
 *
 * @param ctxt  a schema validation context
 * @param idc  the identity-constraint definition
 * @param node  a subtree containing XML Schema information
 * @param isField  field indicator
 * @returns the parsed identity-constraint definition.
 */
XmlXsd10IDCSelect *xml_xsd10_parse_idc_selector_and_field(XmlXsd10ParserCtxt *ctxt,
			  XmlXsd10IDC *idc,
			  XMQNode *node,
			  int isField)
{
    XmlXsd10IDCSelect *item;
    XMQNode *child = NULL;
    XMQNode *attr;

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "xpath"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /*
    * Create the item.
    */
    item = (XmlXsd10IDCSelect *) malloc(sizeof(XmlXsd10IDCSelect));
    if (item == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(item, 0, sizeof(XmlXsd10IDCSelect));
    /*
    * Attribute "xpath" (mandatory).
    */
    attr = xml_xsd10_get_prop_node(node, "xpath");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node,
	    "name", NULL);
    } else {
	item->xpath = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	/*
	* URGENT TODO: "field"s have an other syntax than "selector"s.
	*/

	if (xml_xsd10_check_c_selector_x_path(ctxt, idc, item, attr,
	    isField) == -1) {
	    xml_xsd10_p_err(ctxt,
		(XMQNode *) attr,
		XML_XSD10_SCHEMAP_INTERNAL,
		"Internal error: xmlSchemaParseIDCSelectorAndField, "
		"validating the XPath expression of a IDC selector.\n",
		NULL, NULL);
	}

    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the parent IDC.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) idc,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
	child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?)");
    }

    return (item);
}

/**
 * Port of xmlSchemaParseIDC.
 *
 * Parses a XML Schema identity-constraint definition.
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param idcCategory  the IDC category
 * @param targetNamespace  the target namespace
 * @returns the parsed identity-constraint definition.
 */
XmlXsd10IDC *xml_xsd10_parse_idc(XmlXsd10ParserCtxt *ctxt,
		  XmlXsd10Schema *schema,
		  XMQNode *node,
		  XmlXsd10TypeType idcCategory,
		  const char *targetNamespace)
{
    XmlXsd10IDC *item = NULL;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *name = NULL;
    XmlXsd10IDCSelect *field = NULL, *lastField = NULL;

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		((idcCategory != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) ||
		 (!xml_xsd10_str_equal(xml_node_name(attr),  "refer")))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /*
    * Attribute "name" (mandatory).
    */
    attr = xml_xsd10_get_prop_node(node, "name");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node,
	    "name", NULL);
	return (NULL);
    } else if (xml_xsd10_p_val_attr_node(ctxt,
	NULL, attr,
	xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0) {
	return (NULL);
    }
    /* Create the component. */
    item = xml_xsd10_add_idc(ctxt, schema, name, targetNamespace,
	idcCategory, node);
    if (item == NULL)
	return(NULL);

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    if (idcCategory == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
	/*
	* Attribute "refer" (mandatory).
	*/
	attr = xml_xsd10_get_prop_node(node, "refer");
	if (attr == NULL) {
	    xml_xsd10_p_missing_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		NULL, node,
		"refer", NULL);
	} else {
	    /*
	    * Create a reference item.
	    */
	    item->ref = xml_xsd10_new_q_name_ref(ctxt, XML_XSD10_SCHEMA_TYPE_IDC_KEY,
		NULL, NULL);
	    if (item->ref == NULL)
		return (NULL);
	    xml_xsd10_p_val_attr_node_q_name(ctxt, schema,
		NULL, attr,
		&(item->ref->targetNamespace),
		&(item->ref->name));
	    xml_xsd10_check_reference(ctxt, schema, node, attr,
		item->ref->targetNamespace);
	}
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	item->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	child = xsd10_parse_next(child);
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
		NULL, node, child,
		"A child element is missing",
		"(annotation?, (selector, field+))");
    }
    /*
    * Child element <selector>.
    */
    if (XSD10_IS_SCHEMA(child, "selector")) {
	item->selector = xml_xsd10_parse_idc_selector_and_field(ctxt,
	    item, child, 0);
	child = xsd10_parse_next(child);
	/*
	* Child elements <field>.
	*/
	if (XSD10_IS_SCHEMA(child, "field")) {
	    do {
		field = xml_xsd10_parse_idc_selector_and_field(ctxt,
		    item, child, 1);
		if (field != NULL) {
		    field->index = item->nbFields;
		    item->nbFields++;
		    if (lastField != NULL)
			lastField->next = field;
		    else
			item->fields = field;
		    lastField = field;
		}
		child = xsd10_parse_next(child);
	    } while (XSD10_IS_SCHEMA(child, "field"));
	} else {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child,
		NULL, "(annotation?, (selector, field+))");
	}
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?, (selector, field+))");
    }

    return (item);
}

/**
 * Port of xmlSchemaParseElement.
 *
 * Parses a XML schema element declaration.
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param isElemRef  set if the element is a reference
 * @param topLevel  indicates if this is global declaration
 * @returns the element declaration or a particle; NULL in case
 * of an error or if the particle has minOccurs==maxOccurs==0.
 */
XmlXsd10BasicItem *xml_xsd10_parse_element(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                      XMQNode *node, int *isElemRef, int topLevel)
{
    XmlXsd10Element *decl = NULL;
    XmlXsd10Particle *particle = NULL;
    XmlXsd10Annot *annot = NULL;
    XMQNode *child = NULL;
    XMQNode *attr, *nameAttr;
    int min, max, isRef = 0;
    char *des = NULL;

    /* 3.3.3 Constraints on XML Representations of Element Declarations */
    /* TODO: Complete implementation of 3.3.6 */

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    if (isElemRef != NULL)
	*isElemRef = 0;
    /*
    * If we get a "ref" attribute on a local <element> we will assume it's
    * a reference - even if there's a "name" attribute; this seems to be more
    * robust.
    */
    nameAttr = xml_xsd10_get_prop_node(node, "name");
    attr = xml_xsd10_get_prop_node(node, "ref");
    if ((topLevel) || (attr == NULL)) {
	if (nameAttr == NULL) {
	    xml_xsd10_p_missing_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		NULL, node, "name", NULL);
	    return (NULL);
	}
    } else
	isRef = 1;

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	child = xsd10_parse_next(child);
    }
    /*
    * Skip particle part if a global declaration.
    */
    if (topLevel)
	goto declaration_part;
    /*
    * The particle part ==================================================
    */
    min = xml_xsd10_get_min_occurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
    max = xml_xsd10_get_max_occurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1, "(xs:nonNegativeInteger | unbounded)");
    xml_xsd10_p_check_particle_correct_2(ctxt, NULL, node, min, max);
    particle = xml_xsd10_add_particle(ctxt, node, min, max);
    if (particle == NULL)
	goto return_null;

    /* ret->flags |= XML_SCHEMAS_ELEM_REF; */

    if (isRef) {
	const char *refNs = NULL, *ref = NULL;
	XmlXsd10QNameRef *refer = NULL;
	/*
	* The reference part =============================================
	*/
	if (isElemRef != NULL)
	    *isElemRef = 1;

	xml_xsd10_p_val_attr_node_q_name(ctxt, schema,
	    NULL, attr, &refNs, &ref);
	xml_xsd10_check_reference(ctxt, schema, node, attr, refNs);
	/*
	* SPEC (3.3.3 : 2.1) "One of ref or name must be present, but not both"
	*/
	if (nameAttr != NULL) {
	    xml_xsd10_p_mutual_excl_attr_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_ELEMENT_2_1, NULL, nameAttr, "ref", "name");
	}
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if (xml_xsd10_str_equal(xml_node_name(attr),  "ref") ||
		    xml_xsd10_str_equal(xml_node_name(attr),  "name") ||
		    xml_xsd10_str_equal(xml_node_name(attr),  "id") ||
		    xml_xsd10_str_equal(xml_node_name(attr),  "maxOccurs") ||
		    xml_xsd10_str_equal(xml_node_name(attr),  "minOccurs"))
		{
		    attr = xml_node_next(attr);
		    continue;
		} else {
		    /* SPEC (3.3.3 : 2.2) */
		    xml_xsd10_p_custom_attr_err(ctxt,
			XML_XSD10_SCHEMAP_SRC_ELEMENT_2_2,
			NULL, NULL, attr,
			"Only the attributes 'minOccurs', 'maxOccurs' and "
			"'id' are allowed in addition to 'ref'");
		    break;
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
	/*
	* No children except <annotation> expected.
	*/
	if (child != NULL) {
	    xml_xsd10_p_content_err(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL, "(annotation?)");
	}
	if ((min == 0) && (max == 0))
	    goto return_null;
	/*
	* Create the reference item and attach it to the particle.
	*/
	refer = xml_xsd10_new_q_name_ref(ctxt, XML_XSD10_SCHEMA_TYPE_ELEMENT,
	    ref, refNs);
	if (refer == NULL)
	    goto return_null;
	particle->children = (XmlXsd10TreeItem *) refer;
	particle->annot = annot;
	/*
	* Add the particle to pending components, since the reference
	* need to be resolved.
	*/
	XSD10_WXS_ADD_PENDING(ctxt, particle);
	return ((XmlXsd10BasicItem *) particle);
    }
    /*
    * The declaration part ===============================================
    */
declaration_part:
    {
	const char *ns = NULL, *fixed, *name, *attrValue;
	XmlXsd10IDC *curIDC = NULL, *lastIDC = NULL;

	if (xml_xsd10_p_val_attr_node(ctxt, NULL, nameAttr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0)
	    goto return_null;
	/*
	* Evaluate the target namespace.
	*/
	if (topLevel) {
	    ns = ctxt->targetNamespace;
	} else {
	    attr = xml_xsd10_get_prop_node(node, "form");
	    if (attr != NULL) {
		attrValue = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
		if (xml_xsd10_str_equal(attrValue,  "qualified")) {
		    ns = ctxt->targetNamespace;
		} else if (!xml_xsd10_str_equal(attrValue,  "unqualified")) {
		    xml_xsd10_p_simple_type_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
			NULL, (XMQNode *) attr,
			NULL, "(qualified | unqualified)",
			attrValue, NULL, NULL, NULL);
		}
	    } else if (schema->flags & XML_XSD10_SCHEMAS_QUALIF_ELEM)
		ns = ctxt->targetNamespace;
	}
	decl = xml_xsd10_add_element(ctxt, name, ns, node, topLevel);
	if (decl == NULL) {
	    goto return_null;
	}
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if ((!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "type")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "default")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "fixed")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "block")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "nillable")))
		{
		    if (topLevel == 0) {
			if ((!xml_xsd10_str_equal(xml_node_name(attr),  "maxOccurs")) &&
			    (!xml_xsd10_str_equal(xml_node_name(attr),  "minOccurs")) &&
			    (!xml_xsd10_str_equal(xml_node_name(attr),  "form")))
			{
			    xml_xsd10_p_illegal_attr_err(ctxt,
				XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
			}
		    } else if ((!xml_xsd10_str_equal(xml_node_name(attr),  "final")) &&
			(!xml_xsd10_str_equal(xml_node_name(attr),  "abstract")) &&
			(!xml_xsd10_str_equal(xml_node_name(attr),  "substitutionGroup"))) {

			xml_xsd10_p_illegal_attr_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		    }
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {

		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
	/*
	* Extract/validate attributes.
	*/
	if (topLevel) {
	    /*
	    * Process top attributes of global element declarations here.
	    */
	    decl->flags |= XML_XSD10_SCHEMAS_ELEM_GLOBAL;
	    decl->flags |= XML_XSD10_SCHEMAS_ELEM_TOPLEVEL;
	    xml_xsd10_p_val_attr_q_name(ctxt, schema,
		NULL, node, "substitutionGroup",
		&(decl->substGroupNs), &(decl->substGroup));
	    if (xml_xsd10_get_boolean_prop(ctxt, node, "abstract", 0))
		decl->flags |= XML_XSD10_SCHEMAS_ELEM_ABSTRACT;
	    /*
	    * Attribute "final".
	    */
	    attr = xml_xsd10_get_prop_node(node, "final");
	    if (attr == NULL) {
		if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION)
		    decl->flags |= XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION;
		if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION)
		    decl->flags |= XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION;
	    } else {
		attrValue = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
		if (xml_xsd10_p_val_attr_block_final(attrValue, &(decl->flags),
		    -1,
		    XML_XSD10_SCHEMAS_ELEM_FINAL_EXTENSION,
		    XML_XSD10_SCHEMAS_ELEM_FINAL_RESTRICTION, -1, -1, -1) != 0) {
		    xml_xsd10_p_simple_type_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
			NULL, (XMQNode *) attr,
			NULL, "(#all | List of (extension | restriction))",
			attrValue, NULL, NULL, NULL);
		}
	    }
	}
	/*
	* Attribute "block".
	*/
	attr = xml_xsd10_get_prop_node(node, "block");
	if (attr == NULL) {
	    /*
	    * Apply default "block" values.
	    */
	    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION)
		decl->flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION;
	    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION)
		decl->flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION;
	    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION)
		decl->flags |= XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION;
	} else {
	    attrValue = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	    if (xml_xsd10_p_val_attr_block_final(attrValue, &(decl->flags),
		-1,
		XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION,
		XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION,
		XML_XSD10_SCHEMAS_ELEM_BLOCK_SUBSTITUTION, -1, -1) != 0) {
		xml_xsd10_p_simple_type_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		    NULL, (XMQNode *) attr,
		    NULL, "(#all | List of (extension | "
		    "restriction | substitution))", attrValue,
		    NULL, NULL, NULL);
	    }
	}
	if (xml_xsd10_get_boolean_prop(ctxt, node, "nillable", 0))
	    decl->flags |= XML_XSD10_SCHEMAS_ELEM_NILLABLE;

	attr = xml_xsd10_get_prop_node(node, "type");
	if (attr != NULL) {
	    xml_xsd10_p_val_attr_node_q_name(ctxt, schema,
		NULL, attr,
		&(decl->namedTypeNs), &(decl->namedType));
	    xml_xsd10_check_reference(ctxt, schema, node,
		attr, decl->namedTypeNs);
	}
	decl->value = xml_xsd10_get_prop(ctxt, node, "default");
	attr = xml_xsd10_get_prop_node(node, "fixed");
	if (attr != NULL) {
	    fixed = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	    if (decl->value != NULL) {
		/*
		* 3.3.3 : 1
		* default and fixed must not both be present.
		*/
		xml_xsd10_p_mutual_excl_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_ELEMENT_1,
		    NULL, attr, "default", "fixed");
	    } else {
		decl->flags |= XML_XSD10_SCHEMAS_ELEM_FIXED;
		decl->value = fixed;
	    }
	}
	/*
	* And now for the children...
	*/
	if (XSD10_IS_SCHEMA(child, "complexType")) {
	    /*
	    * 3.3.3 : 3
	    * "type" and either <simpleType> or <complexType> are mutually
	    * exclusive
	    */
	    if (decl->namedType != NULL) {
		xml_xsd10_p_content_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_ELEMENT_3,
		    NULL, node, child,
		    "The attribute 'type' and the <complexType> child are "
		    "mutually exclusive", NULL);
	    } else
		XSD10_WXS_ELEM_TYPEDEF(decl) = xml_xsd10_parse_complex_type(ctxt, schema, child, 0);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "simpleType")) {
	    /*
	    * 3.3.3 : 3
	    * "type" and either <simpleType> or <complexType> are
	    * mutually exclusive
	    */
	    if (decl->namedType != NULL) {
		xml_xsd10_p_content_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_ELEMENT_3,
		    NULL, node, child,
		    "The attribute 'type' and the <simpleType> child are "
		    "mutually exclusive", NULL);
	    } else
		XSD10_WXS_ELEM_TYPEDEF(decl) = xml_xsd10_parse_simple_type(ctxt, schema, child, 0);
	    child = xsd10_parse_next(child);
	}
	while ((XSD10_IS_SCHEMA(child, "unique")) ||
	    (XSD10_IS_SCHEMA(child, "key")) || (XSD10_IS_SCHEMA(child, "keyref"))) {
	    if (XSD10_IS_SCHEMA(child, "unique")) {
		curIDC = xml_xsd10_parse_idc(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE, decl->targetNamespace);
	    } else if (XSD10_IS_SCHEMA(child, "key")) {
		curIDC = xml_xsd10_parse_idc(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_IDC_KEY, decl->targetNamespace);
	    } else if (XSD10_IS_SCHEMA(child, "keyref")) {
		curIDC = xml_xsd10_parse_idc(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_IDC_KEYREF, decl->targetNamespace);
	    }
	    if (lastIDC != NULL)
		lastIDC->next = curIDC;
	    else
		decl->idcs = (void *) curIDC;
	    lastIDC = curIDC;
	    child = xsd10_parse_next(child);
	}
	if (child != NULL) {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child,
		NULL, "(annotation?, ((simpleType | complexType)?, "
		"(unique | key | keyref)*))");
	}
	decl->annot = annot;
    }
    /*
    * NOTE: Element Declaration Representation OK 4. will be checked at a
    * different layer.
    */
    XSD10_FREE_AND_NULL(des)
    if (topLevel)
	return ((XmlXsd10BasicItem *) decl);
    else {
	particle->children = (XmlXsd10TreeItem *) decl;
	return ((XmlXsd10BasicItem *) particle);
    }

return_null:
    XSD10_FREE_AND_NULL(des);
    if (annot != NULL) {
	if (particle != NULL)
	    particle->annot = NULL;
	if (decl != NULL)
	    decl->annot = NULL;
	xml_xsd10_free_annot(annot);
    }
    return (NULL);
}

/**
 * Port of xmlSchemaParseUnion.
 *
 * parse a XML schema Union definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns -1 in case of internal error, 0 in case of success and a positive
 * error code otherwise.
 */
int xml_xsd10_parse_union(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                    XMQNode *node)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *cur = NULL;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (-1);
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    /*
    * Mark the simple type as being of variety "union".
    */
    type->flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_UNION;
    /*
    * SPEC (Base type) (2) "If the <list> or <union> alternative is chosen,
    * then the `simple ur-type definition`."
    */
    type->baseType = xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "memberTypes"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * Attribute "memberTypes". This is a list of QNames.
    * TODO: Check the value to contain anything.
    */
    attr = xml_xsd10_get_prop_node(node, "memberTypes");
    if (attr != NULL) {
	const char *end;
	char *tmp;
	const char *localName, *nsName;
	XmlXsd10TypeLink *link, *lastLink = NULL;
	XmlXsd10QNameRef *ref;

	cur = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
        if (cur == NULL)
            return (-1);
	type->base = cur;
	do {
	    while (XSD10_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    tmp = xml_xsd10_strndup(cur, end - cur);
            if (tmp == NULL) {
                xml_xsd10_p_err_memory(ctxt);
                return (-1);
            }
	    if (xml_xsd10_p_val_attr_node_q_name_value(ctxt, schema,
		NULL, attr,  tmp, &nsName, &localName) == 0) {
		/*
		* Create the member type link.
		*/
		link = (XmlXsd10TypeLink *)
		    malloc(sizeof(XmlXsd10TypeLink));
		if (link == NULL) {
		    xml_xsd10_p_err_memory(ctxt);
	            XSD10_FREE_AND_NULL(tmp)
		    return (-1);
		}
		link->type = NULL;
		link->next = NULL;
		/*
		* Create a reference item.
		*/
		ref = xml_xsd10_new_q_name_ref(ctxt, XML_XSD10_SCHEMA_TYPE_SIMPLE,
		    localName, nsName);
		if (ref == NULL) {
                    xml_xsd10_p_err_memory(ctxt);
                    free(link);
		    XSD10_FREE_AND_NULL(tmp)
		    return (-1);
		}
		/*
		* Assign the reference to the link, it will be resolved
		* later during fixup of the union simple type.
		*/
		link->type = (XmlXsd10Type *) ref;

		if (lastLink == NULL)
		    type->memberTypes = link;
		else
		    lastLink->next = link;
		lastLink = link;
	    }
	    XSD10_FREE_AND_NULL(tmp)
	    cur = end;
	} while (*cur != 0);

    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the simple type ancestor.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (XSD10_IS_SCHEMA(child, "simpleType")) {
	XmlXsd10Type *subtype, *last = NULL;

	/*
	* Anchor the member types in the "subtypes" field of the
	* simple type.
	*/
	while (XSD10_IS_SCHEMA(child, "simpleType")) {
	    subtype = (XmlXsd10Type *)
		xml_xsd10_parse_simple_type(ctxt, schema, child, 0);
	    if (subtype != NULL) {
		if (last == NULL) {
		    type->subtypes = subtype;
		    last = subtype;
		} else {
		    last->next = subtype;
		    last = subtype;
		}
		last->next = NULL;
	    }
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL, "(annotation?, simpleType*)");
    }
    if ((attr == NULL) && (type->subtypes == NULL)) {
	 /*
	* src-union-memberTypes-or-simpleTypes
	* Either the memberTypes [attribute] of the <union> element must
	* be non-empty or there must be at least one simpleType [child].
	*/
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_SRC_UNION_MEMBERTYPES_OR_SIMPLETYPES,
	    NULL, node,
	    "Either the attribute 'memberTypes' or "
	    "at least one <simpleType> child must be present", NULL);
    }
    return (0);
}

/**
 * Port of xmlSchemaParseList.
 *
 * parse a XML schema List definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns -1 in case of error, 0 if the declaration is improper and
 *         1 in case of success.
 */
XmlXsd10Type *xml_xsd10_parse_list(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                   XMQNode *node)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    /*
    * Mark the type as being of variety "list".
    */
    type->flags |= XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST;
    /*
    * SPEC (Base type) (2) "If the <list> or <union> alternative is chosen,
    * then the `simple ur-type definition`."
    */
    type->baseType = xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "itemType"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");

    /*
    * Attribute "itemType". NOTE that we will use the "ref" and "refNs"
    * fields for holding the reference to the itemType.
    *
    * REVAMP TODO: Use the "base" and "baseNs" fields, since we will remove
    * the "ref" fields.
    */
    xml_xsd10_p_val_attr_q_name(ctxt, schema, NULL,
	node, "itemType", &(type->baseNs), &(type->base));
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (XSD10_IS_SCHEMA(child, "simpleType")) {
	/*
	* src-list-itemType-or-simpleType
	* Either the itemType [attribute] or the <simpleType> [child] of
	* the <list> element must be present, but not both.
	*/
	if (type->base != NULL) {
	    xml_xsd10_p_custom_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
		NULL, node,
		"The attribute 'itemType' and the <simpleType> child "
		"are mutually exclusive", NULL);
	} else {
	    type->subtypes = xml_xsd10_parse_simple_type(ctxt, schema, child, 0);
	}
        child = xsd10_parse_next(child);
    } else if (type->base == NULL) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
	    NULL, node,
	    "Either the attribute 'itemType' or the <simpleType> child "
	    "must be present", NULL);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL, "(annotation?, simpleType?)");
    }
    if ((type->base == NULL) &&
	(type->subtypes == NULL) &&
	(xml_xsd10_get_prop_node(node, "itemType") == NULL)) {
	xml_xsd10_p_custom_err(ctxt,
	    XML_XSD10_SCHEMAP_SRC_SIMPLE_TYPE_1,
	    NULL, node,
	    "Either the attribute 'itemType' or the <simpleType> child "
	    "must be present", NULL);
    }
    return (NULL);
}

/**
 * Port of xmlSchemaParseSimpleType.
 *
 * parse a XML schema Simple Type definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param topLevel  whether this is a top-level element
 * @returns -1 in case of error, 0 if the declaration is improper and
 * 1 in case of success.
 */
XmlXsd10Type *xml_xsd10_parse_simple_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                         XMQNode *node, int topLevel)
{
    XmlXsd10Type *type, *oldCtxtType;
    XMQNode *child = NULL;
    const char *attrValue = NULL;
    XMQNode *attr;
    int hasRestriction = 0;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    if (topLevel) {
	attr = xml_xsd10_get_prop_node(node, "name");
	if (attr == NULL) {
	    xml_xsd10_p_missing_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		NULL, node,
		"name", NULL);
	    return (NULL);
	} else {
	    if (xml_xsd10_p_val_attr_node(ctxt,
		NULL, attr,
		xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &attrValue) != 0)
		return (NULL);
	    /*
	    * Skip built-in types.
	    */
	    if (ctxt->isS4S) {
		XmlXsd10Type *biType;

		if (ctxt->isRedefine) {
		    /*
		    * REDEFINE: Disallow redefinition of built-in-types.
		    * TODO: It seems that the spec does not say anything
		    * about this case.
		    */
		    xml_xsd10_p_custom_err(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
			NULL, node,
			"Redefinition of built-in simple types is not "
			"supported", NULL);
		    return(NULL);
		}
		biType = xml_xsd10_get_predefined_type(attrValue, xml_xsd10_ns);
		if (biType != NULL)
		    return (biType);
	    }
	}
    }
    /*
    * TargetNamespace:
    * SPEC "The `actual value` of the targetNamespace [attribute]
    * of the <schema> ancestor element information item if present,
    * otherwise `absent`.
    */
    if (topLevel == 0) {
#ifdef XSD10_ENABLE_NAMED_LOCALS
        char buf[40];
#endif
	/*
	* Parse as local simple type definition.
	*/
#ifdef XSD10_ENABLE_NAMED_LOCALS
        snprintf(buf, 39, "#ST%d", ctxt->counter++ + 1);
	type = xml_xsd10_add_type(ctxt, schema,
	    XML_XSD10_SCHEMA_TYPE_SIMPLE,
	    xml_xsd10_dict_lookup(ctxt->dict, (const char *)buf, -1),
	    ctxt->targetNamespace, node, 0);
#else
	type = xml_xsd10_add_type(ctxt, schema,
	    XML_XSD10_SCHEMA_TYPE_SIMPLE,
	    NULL, ctxt->targetNamespace, node, 0);
#endif
	if (type == NULL)
	    return (NULL);
	type->type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
	type->contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if (!xml_xsd10_str_equal(xml_node_name(attr),  "id")) {
		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
    } else {
	/*
	* Parse as global simple type definition.
	*
	* Note that attrValue is the value of the attribute "name" here.
	*/
	type = xml_xsd10_add_type(ctxt, schema, XML_XSD10_SCHEMA_TYPE_SIMPLE,
	    attrValue, ctxt->targetNamespace, node, 1);
	if (type == NULL)
	    return (NULL);
	type->type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
	type->contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
	type->flags |= XML_XSD10_SCHEMAS_TYPE_GLOBAL;
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "final"))) {
		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
	/*
	* Attribute "final".
	*/
	attr = xml_xsd10_get_prop_node(node, "final");
	if (attr == NULL) {
	    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION)
		type->flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION;
	    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST)
		type->flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_LIST;
	    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION)
		type->flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_UNION;
	} else {
	    attrValue = xml_xsd10_get_prop(ctxt, node, "final");
	    if (xml_xsd10_p_val_attr_block_final(attrValue, &(type->flags),
		-1, -1, XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION, -1,
		XML_XSD10_SCHEMAS_TYPE_FINAL_LIST,
		XML_XSD10_SCHEMAS_TYPE_FINAL_UNION) != 0) {

		xml_xsd10_p_simple_type_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		    XSD10_WXS_BASIC_CAST type, (XMQNode *) attr,
		    NULL, "(#all | List of (list | union | restriction)",
		    attrValue, NULL, NULL, NULL);
	    }
	}
    }
    type->targetNamespace = ctxt->targetNamespace;
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * And now for the children...
    */
    oldCtxtType = ctxt->ctxtType;

    ctxt->ctxtType = type;

    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        type->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
	    NULL, node, child, NULL,
	    "(annotation?, (restriction | list | union))");
    } else if (XSD10_IS_SCHEMA(child, "restriction")) {
        xml_xsd10_parse_restriction(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_SIMPLE);
	hasRestriction = 1;
        child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "list")) {
        xml_xsd10_parse_list(ctxt, schema, child);
        child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "union")) {
        xml_xsd10_parse_union(ctxt, schema, child);
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt, XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?, (restriction | list | union))");
    }
    /*
    * REDEFINE: SPEC src-redefine (5)
    * "Within the [children], each <simpleType> must have a
    * <restriction> among its [children] ... the `actual value` of whose
    * base [attribute] must be the same as the `actual value` of its own
    * name attribute plus target namespace;"
    */
    if (topLevel && ctxt->isRedefine && (! hasRestriction)) {
	xml_xsd10_p_custom_err(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
	    NULL, node, "This is a redefinition, thus the "
	    "<simpleType> must have a <restriction> child", NULL);
    }

    ctxt->ctxtType = oldCtxtType;
    return (type);
}

/**
 * Port of xmlSchemaParseModelGroupDefRef.
 *
 * Parses a reference to a model group definition.
 *
 * We will return a particle component with a qname-component or
 * NULL in case of an error.
 *
 * @param ctxt  the parser context
 * @param schema  the schema being built
 * @param node  the node
 */
XmlXsd10TreeItem *xml_xsd10_parse_model_group_def_ref(XmlXsd10ParserCtxt *ctxt,
			       XmlXsd10Schema *schema,
			       XMQNode *node)
{
    XmlXsd10Particle *item;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *ref = NULL, *refNs = NULL;
    int min, max;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    attr = xml_xsd10_get_prop_node(node, "ref");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "ref", NULL);
	return (NULL);
    } else if (xml_xsd10_p_val_attr_node_q_name(ctxt, schema, NULL,
	attr, &refNs, &ref) != 0) {
	return (NULL);
    }
    xml_xsd10_check_reference(ctxt, schema, node, attr, refNs);
    min = xml_xsd10_get_min_occurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
    max = xml_xsd10_get_max_occurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
	"(xs:nonNegativeInteger | unbounded)");
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "ref")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "minOccurs")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "maxOccurs"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    item = xml_xsd10_add_particle(ctxt, node, min, max);
    if (item == NULL)
	return (NULL);
    /*
    * Create a qname-reference and set as the term; it will be substituted
    * for the model group after the reference has been resolved.
    */
    item->children = (XmlXsd10TreeItem *)
	xml_xsd10_new_q_name_ref(ctxt, XML_XSD10_SCHEMA_TYPE_GROUP, ref, refNs);
    xml_xsd10_p_check_particle_correct_2(ctxt, item, node, min, max);
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    /* TODO: Is annotation even allowed for a model group reference? */
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* TODO: What to do exactly with the annotation?
	*/
	item->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?)");
    }
    /*
    * Corresponds to no component at all if minOccurs==maxOccurs==0.
    */
    if ((min == 0) && (max == 0))
	return (NULL);

    return ((XmlXsd10TreeItem *) item);
}

/**
 * Port of xmlSchemaParseModelGroupDefinition.
 *
 * Parses a XML schema model group definition.
 *
 * Note that the constraint src-redefine (6.2) can't be applied until
 * references have been resolved. So we will do this at the
 * component fixup level.
 *
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns -1 in case of error, 0 if the declaration is improper and
 *         1 in case of success.
 */
XmlXsd10ModelGroupDef *xml_xsd10_parse_model_group_definition(XmlXsd10ParserCtxt *ctxt,
				   XmlXsd10Schema *schema,
				   XMQNode *node)
{
    XmlXsd10ModelGroupDef *item;
    XMQNode *child = NULL;
    XMQNode *attr;
    const char *name;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    attr = xml_xsd10_get_prop_node(node, "name");
    if (attr == NULL) {
	xml_xsd10_p_missing_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node,
	    "name", NULL);
	return (NULL);
    } else if (xml_xsd10_p_val_attr_node(ctxt, NULL, attr,
	xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0) {
	return (NULL);
    }
    item = xml_xsd10_add_model_group_definition(ctxt, schema, name,
	ctxt->targetNamespace, node);
    if (item == NULL)
	return (NULL);
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "name")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "id"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	item->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	child = xsd10_parse_next(child);
    }
    if (XSD10_IS_SCHEMA(child, "all")) {
	item->children = xml_xsd10_parse_model_group(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_ALL, 0);
	child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "choice")) {
	item->children = xml_xsd10_parse_model_group(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_CHOICE, 0);
	child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "sequence")) {
	item->children = xml_xsd10_parse_model_group(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 0);
	child = xsd10_parse_next(child);
    }



    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?, (all | choice | sequence)?)");
    }
    return (item);
}

/**
 * Port of xmlSchemaCleanupDoc.
 *
 * removes unwanted nodes in a schemas document tree
 *
 * @param ctxt  a schema validation context
 * @param root  the root of the document.
 */
void xml_xsd10_cleanup_doc(XmlXsd10ParserCtxt *ctxt, XMQNode *root)
{
    /*
    * libxml2 removes all the blank text nodes, the comments, the processing
    * instructions and the entity references of the tree here (it does not
    * descend into elements whose first child is an entity reference).
    * The native port never modifies a schema document: the parsing functions
    * skip the nodes that would have been removed instead, see
    * xsd10_parse_first_child and xsd10_parse_next.
    */
    (void) ctxt;
    (void) root;
}


/** Port of xmlSchemaClearSchemaDefaults. */
void xml_xsd10_clear_schema_defaults(XmlXsd10Schema *schema)
{
    if (schema->flags & XML_XSD10_SCHEMAS_QUALIF_ELEM)
	schema->flags ^= XML_XSD10_SCHEMAS_QUALIF_ELEM;

    if (schema->flags & XML_XSD10_SCHEMAS_QUALIF_ATTR)
	schema->flags ^= XML_XSD10_SCHEMAS_QUALIF_ATTR;

    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION)
	schema->flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION;
    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION)
	schema->flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION;
    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST)
	schema->flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST;
    if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION)
	schema->flags ^= XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION;

    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION)
	schema->flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION;
    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION)
	schema->flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION;
    if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION)
	schema->flags ^= XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION;
}

/** Port of xmlSchemaParseSchemaElement. */
int xml_xsd10_parse_schema_element(XmlXsd10ParserCtxt *ctxt,
			     XmlXsd10Schema *schema,
			     XMQNode *node)
{
    XMQNode *attr;
    const char *val;
    int res = 0, oldErrs = ctxt->nberrors;

    /*
    * Those flags should be moved to the parser context flags,
    * since they are not visible at the component level. I.e.
    * they are used if processing schema *documents* only.
    */
    res = xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    XSD10_HFAILURE;

    /*
    * Since the version is of type xs:token, we won't bother to
    * check it.
    */
    /* REMOVED:
    attr = xmlSchemaGetPropNode(node, "version");
    if (attr != NULL) {
	res = xmlSchemaPValAttrNode(ctxt, NULL, NULL, attr,
	    xmlSchemaGetBuiltInType(XML_SCHEMAS_TOKEN), &val);
	HFAILURE;
    }
    */
    attr = xml_xsd10_get_prop_node(node, "targetNamespace");
    if (attr != NULL) {
	res = xml_xsd10_p_val_attr_node(ctxt, NULL, attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI), NULL);
	XSD10_HFAILURE;
	if (res != 0) {
	    ctxt->stop = XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE;
	    goto exit;
	}
    }
    attr = xml_xsd10_get_prop_node(node, "elementFormDefault");
    if (attr != NULL) {
	val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	res = xml_xsd10_p_val_attr_form_default(val, &schema->flags,
	    XML_XSD10_SCHEMAS_QUALIF_ELEM);
	XSD10_HFAILURE;
	if (res != 0) {
	    xml_xsd10_p_simple_type_err(ctxt,
		XML_XSD10_SCHEMAP_ELEMFORMDEFAULT_VALUE,
		NULL, (XMQNode *) attr, NULL,
		"(qualified | unqualified)", val, NULL, NULL, NULL);
	}
    }
    attr = xml_xsd10_get_prop_node(node, "attributeFormDefault");
    if (attr != NULL) {
	val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	res = xml_xsd10_p_val_attr_form_default(val, &schema->flags,
	    XML_XSD10_SCHEMAS_QUALIF_ATTR);
	XSD10_HFAILURE;
	if (res != 0) {
	    xml_xsd10_p_simple_type_err(ctxt,
		XML_XSD10_SCHEMAP_ATTRFORMDEFAULT_VALUE,
		NULL, (XMQNode *) attr, NULL,
		"(qualified | unqualified)", val, NULL, NULL, NULL);
	}
    }
    attr = xml_xsd10_get_prop_node(node, "finalDefault");
    if (attr != NULL) {
	val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	res = xml_xsd10_p_val_attr_block_final(val, &(schema->flags), -1,
	    XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION,
	    XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION,
	    -1,
	    XML_XSD10_SCHEMAS_FINAL_DEFAULT_LIST,
	    XML_XSD10_SCHEMAS_FINAL_DEFAULT_UNION);
	XSD10_HFAILURE;
	if (res != 0) {
	    xml_xsd10_p_simple_type_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		NULL, (XMQNode *) attr, NULL,
		"(#all | List of (extension | restriction | list | union))",
		val, NULL, NULL, NULL);
	}
    }
    attr = xml_xsd10_get_prop_node(node, "blockDefault");
    if (attr != NULL) {
	val = xml_xsd10_get_node_content(ctxt, (XMQNode *) attr);
	res = xml_xsd10_p_val_attr_block_final(val, &(schema->flags), -1,
	    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION,
	    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION,
	    XML_XSD10_SCHEMAS_BLOCK_DEFAULT_SUBSTITUTION, -1, -1);
	XSD10_HFAILURE;
	if (res != 0) {
	    xml_xsd10_p_simple_type_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
		NULL, (XMQNode *) attr, NULL,
		"(#all | List of (extension | restriction | substitution))",
		val, NULL, NULL, NULL);
	}
    }

exit:
    if (oldErrs != ctxt->nberrors)
	res = ctxt->err;
    return(res);
exit_failure:
    return(-1);
}

/**
 * Port of xmlSchemaParseSchemaTopLevel.
 *
 * @param ctxt  a schema validation context
 * @param schema  the schemas
 * @param nodes  the list of top level nodes
 * @returns the internal XML Schema structure built from the resource or
 *         NULL in case of error
 */
int xml_xsd10_parse_schema_top_level(XmlXsd10ParserCtxt *ctxt,
                             XmlXsd10Schema *schema, XMQNode *nodes)
{
    XMQNode *child;
    XmlXsd10Annot *annot;
    int res = 0, oldErrs, tmpOldErrs;

    if ((ctxt == NULL) || (schema == NULL) || (nodes == NULL))
        return(-1);

    oldErrs = ctxt->nberrors;
    child = nodes;
    while ((XSD10_IS_SCHEMA(child, "include")) ||
	   (XSD10_IS_SCHEMA(child, "import")) ||
	   (XSD10_IS_SCHEMA(child, "redefine")) ||
	   (XSD10_IS_SCHEMA(child, "annotation"))) {
	if (XSD10_IS_SCHEMA(child, "annotation")) {
	    annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	    if (schema->annot == NULL)
		schema->annot = annot;
	    else
		xml_xsd10_free_annot(annot);
	} else if (XSD10_IS_SCHEMA(child, "import")) {
	    tmpOldErrs = ctxt->nberrors;
	    res = xml_xsd10_parse_import(ctxt, schema, child);
	    XSD10_HFAILURE;
	    XSD10_HSTOP(ctxt);
	    if (tmpOldErrs != ctxt->nberrors)
		goto exit;
	} else if (XSD10_IS_SCHEMA(child, "include")) {
	    tmpOldErrs = ctxt->nberrors;
	    res = xml_xsd10_parse_include(ctxt, schema, child);
	    XSD10_HFAILURE;
	    XSD10_HSTOP(ctxt);
	    if (tmpOldErrs != ctxt->nberrors)
		goto exit;
	} else if (XSD10_IS_SCHEMA(child, "redefine")) {
	    tmpOldErrs = ctxt->nberrors;
	    res = xml_xsd10_parse_redefine(ctxt, schema, child);
	    XSD10_HFAILURE;
	    XSD10_HSTOP(ctxt);
	    if (tmpOldErrs != ctxt->nberrors)
		goto exit;
	}
	child = xsd10_parse_next(child);
    }
    /*
    * URGENT TODO: Change the functions to return int results.
    * We need especially to catch internal errors.
    */
    while (child != NULL) {
	if (XSD10_IS_SCHEMA(child, "complexType")) {
	    xml_xsd10_parse_complex_type(ctxt, schema, child, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "simpleType")) {
	    xml_xsd10_parse_simple_type(ctxt, schema, child, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "element")) {
	    xml_xsd10_parse_element(ctxt, schema, child, NULL, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "attribute")) {
	    xml_xsd10_parse_global_attribute(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "attributeGroup")) {
	    xml_xsd10_parse_attribute_group_definition(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "group")) {
	    xml_xsd10_parse_model_group_definition(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "notation")) {
	    xml_xsd10_parse_notation(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	} else {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, xml_node_parent(child), child,
		NULL, "((include | import | redefine | annotation)*, "
		"(((simpleType | complexType | group | attributeGroup) "
		"| element | attribute | notation), annotation*)*)");
	    child = xsd10_parse_next(child);
	}
	while (XSD10_IS_SCHEMA(child, "annotation")) {
	    /*
	    * TODO: We should add all annotations.
	    */
	    annot = xml_xsd10_parse_annotation(ctxt, child, 1);
	    if (schema->annot == NULL)
		schema->annot = annot;
	    else
		xml_xsd10_free_annot(annot);
	    child = xsd10_parse_next(child);
	}
    }
exit:
    ctxt->ctxtType = NULL;
    if (oldErrs != ctxt->nberrors)
	res = ctxt->err;
    return(res);
exit_failure:
    return(-1);
}

/** Port of xmlSchemaSchemaRelationCreate. */
XmlXsd10SchemaRelation *xml_xsd10_schema_relation_create(void)
{
    XmlXsd10SchemaRelation *ret;

    ret = (XmlXsd10SchemaRelation *)
	malloc(sizeof(XmlXsd10SchemaRelation));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(NULL);
	return(NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10SchemaRelation));
    return(ret);
}

#if 0

#endif


/**
 * Port of xmlSchemaGetSchemaBucket.
 *
 * @param pctxt  the schema parser context
 * @param schemaLocation  the URI of the schema document
 * @returns a schema bucket if it was already parsed from
 *         `schemaLocation`, NULL otherwise.
 */
XmlXsd10Bucket *xml_xsd10_get_schema_bucket(XmlXsd10ParserCtxt *pctxt,
			    const char *schemaLocation)
{
    XmlXsd10Bucket *cur;
    XmlXsd10ItemList *list;

    list = pctxt->constructor->buckets;
    if (list->nbItems == 0)
	return(NULL);
    else {
	int i;
	for (i = 0; i < list->nbItems; i++) {
	    cur = (XmlXsd10Bucket *) list->items[i];
	    /* Pointer comparison! */
	    if (cur->schemaLocation == schemaLocation)
		return(cur);
	}
    }
    return(NULL);
}

/** Port of xmlSchemaGetChameleonSchemaBucket. */
XmlXsd10Bucket *xml_xsd10_get_chameleon_schema_bucket(XmlXsd10ParserCtxt *pctxt,
				     const char *schemaLocation,
				     const char *targetNamespace)
{
    XmlXsd10Bucket *cur;
    XmlXsd10ItemList *list;

    list = pctxt->constructor->buckets;
    if (list->nbItems == 0)
	return(NULL);
    else {
	int i;
	for (i = 0; i < list->nbItems; i++) {
	    cur = (XmlXsd10Bucket *) list->items[i];
	    /* Pointer comparison! */
	    if ((cur->origTargetNamespace == NULL) &&
		(cur->schemaLocation == schemaLocation) &&
		(cur->targetNamespace == targetNamespace))
		return(cur);
	}
    }
    return(NULL);
}


#define XSD10_IS_BAD_SCHEMA_DOC(b) \
    (((b)->doc == NULL) && ((b)->schemaLocation != NULL))

/** Port of xmlSchemaGetSchemaBucketByTNS. */
XmlXsd10Bucket *xml_xsd10_get_schema_bucket_by_tns(XmlXsd10ParserCtxt *pctxt,
				 const char *targetNamespace,
				 int imported)
{
    XmlXsd10Bucket *cur;
    XmlXsd10ItemList *list;

    list = pctxt->constructor->buckets;
    if (list->nbItems == 0)
	return(NULL);
    else {
	int i;
	for (i = 0; i < list->nbItems; i++) {
	    cur = (XmlXsd10Bucket *) list->items[i];
	    if ((! XSD10_IS_BAD_SCHEMA_DOC(cur)) &&
		(cur->origTargetNamespace == targetNamespace) &&
		((imported && cur->imported) ||
		 ((!imported) && (!cur->imported))))
		return(cur);
	}
    }
    return(NULL);
}

/** Port of xmlSchemaParseNewDocWithContext. */
int xml_xsd10_parse_new_doc_with_context(XmlXsd10ParserCtxt *pctxt,
		     XmlXsd10Schema *schema,
		     XmlXsd10Bucket *bucket)
{
    int oldFlags;
    XmlDom *oldDoc;
    XMQNode *node;
    int ret, oldErrs;
    XmlXsd10Bucket *oldbucket = pctxt->constructor->bucket;

    /*
    * Save old values; reset the *main* schema.
    * URGENT TODO: This is not good; move the per-document information
    * to the parser. Get rid of passing the main schema to the
    * parsing functions.
    */
    oldFlags = schema->flags;
    oldDoc = schema->doc;
    if (schema->flags != 0)
	xml_xsd10_clear_schema_defaults(schema);
    schema->doc = bucket->doc;
    pctxt->schema = schema;
    /*
    * Keep the current target namespace on the parser *not* on the
    * main schema.
    */
    pctxt->targetNamespace = bucket->targetNamespace;
    XSD10_WXS_CONSTRUCTOR(pctxt)->bucket = bucket;

    if ((bucket->targetNamespace != NULL) &&
	xml_xsd10_str_equal(bucket->targetNamespace, xml_xsd10_ns)) {
	/*
	* We are parsing the schema for schemas!
	*/
	pctxt->isS4S = 1;
    }
    /* Mark it as parsed, even if parsing fails. */
    bucket->parsed++;
    /* Compile the schema doc. */
    node = xml_dom_root_element(bucket->doc);
    ret = xml_xsd10_parse_schema_element(pctxt, schema, node);
    if (ret != 0)
	goto exit;
    /* An empty schema; just get out. */
    if (xsd10_parse_first_child(node) == NULL)
	goto exit;
    oldErrs = pctxt->nberrors;
    ret = xml_xsd10_parse_schema_top_level(pctxt, schema, xsd10_parse_first_child(node));
    if (ret != 0)
	goto exit;
    /*
    * TODO: Not nice, but I'm not 100% sure we will get always an error
    * as a result of the above functions; so better rely on pctxt->err
    * as well.
    */
    if ((ret == 0) && (oldErrs != pctxt->nberrors)) {
	ret = pctxt->err;
	goto exit;
    }

exit:
    XSD10_WXS_CONSTRUCTOR(pctxt)->bucket = oldbucket;
    /* Restore schema values. */
    schema->doc = oldDoc;
    schema->flags = oldFlags;
    return(ret);
}

/** Port of xmlSchemaParseNewDoc. */
int xml_xsd10_parse_new_doc(XmlXsd10ParserCtxt *pctxt,
		     XmlXsd10Schema *schema,
		     XmlXsd10Bucket *bucket)
{
    XmlXsd10ParserCtxt *newpctxt;
    int res = 0;

    if (bucket == NULL)
	return(0);
    if (bucket->parsed) {
	XSD10_PERROR_INT("xmlSchemaParseNewDoc",
	    "reparsing a schema doc");
	return(-1);
    }
    if (bucket->doc == NULL) {
	XSD10_PERROR_INT("xmlSchemaParseNewDoc",
	    "parsing a schema doc, but there's no doc");
	return(-1);
    }
    if (pctxt->constructor == NULL) {
	XSD10_PERROR_INT("xmlSchemaParseNewDoc",
	    "no constructor");
	return(-1);
    }
    /* Create and init the temporary parser context. */
    newpctxt = xml_xsd10_new_parser_ctxt_use_dict(
	(const char *) bucket->schemaLocation, pctxt->dict);
    if (newpctxt == NULL)
	return(-1);
    newpctxt->constructor = pctxt->constructor;
    /*
    * TODO: Can we avoid that the parser knows about the main schema?
    * It would be better if he knows about the current schema bucket
    * only.
    */
    newpctxt->schema = schema;
    xml_xsd10_set_parser_errors(newpctxt, pctxt->error, pctxt->warning,
	pctxt->errCtxt);
    xml_xsd10_set_parser_structured_errors(newpctxt, pctxt->serror,
	pctxt->errCtxt);
    newpctxt->counter = pctxt->counter;
    /* Not in libxml2 (its temporary context loads with the default resource
       loader): the native loader replaces xmlResourceLoader, keep using it for
       the imports/includes of the nested documents. */
    newpctxt->resourceLoader = pctxt->resourceLoader;
    newpctxt->resourceCtxt = pctxt->resourceCtxt;



    res = xml_xsd10_parse_new_doc_with_context(newpctxt, schema, bucket);

    /* Channel back errors and cleanup the temporary parser context. */
    if (res != 0)
	pctxt->err = res;
    pctxt->nberrors += newpctxt->nberrors;
    pctxt->counter = newpctxt->counter;
    newpctxt->constructor = NULL;
    /* Free the parser context. */
    xml_xsd10_free_parser_ctxt(newpctxt);
    return(res);
}

/** Port of xmlSchemaSchemaRelationAddChild. */
void xml_xsd10_schema_relation_add_child(XmlXsd10Bucket *bucket,
				XmlXsd10SchemaRelation *rel)
{
    XmlXsd10SchemaRelation *cur = bucket->relations;

    if (cur == NULL) {
	bucket->relations = rel;
	return;
    }
    while (cur->next != NULL)
	cur = cur->next;
    cur->next = rel;
}


/** Port of xmlSchemaBuildAbsoluteURI. */
const char *xml_xsd10_build_absolute_uri(XmlXsd10Dict *dict, const char* location,
			  XMQNode *ctxtNode,
			  const char *baseURI)
{
    /*
    * Build an absolute location URI.
    */
    if (location != NULL) {
	char *URI;
	const char *ret;

	if (ctxtNode != NULL) {
	    char *base;

	    base = xml_xsd10_tree_node_get_base(xml_node_dom(ctxtNode), ctxtNode);
	    if (base == NULL) {
		URI = xml_uri_build(location, xml_dom_url(xml_node_dom(ctxtNode)));
	    } else {
		URI = xml_uri_build(location, base);
		free(base);
	    }
	} else if (baseURI != NULL) {
	    URI = xml_uri_build(location, baseURI);
	} else {
	    return(location);
	}
	if (URI != NULL) {
	    ret = xml_xsd10_dict_lookup(dict, URI, -1);
	    free(URI);
	    return(ret);
	}
    }
    return(NULL);
}



/**
 * Port of xmlSchemaAddSchemaDoc.
 *
 * Parse an included (and to-be-redefined) XML schema document.
 *
 * @param pctxt  a schema validation context
 * @param type  import or include or redefine
 * @param schemaLocation  schema location
 * @param schemaDoc  schema document
 * @param schemaBuffer  document buffer
 * @param schemaBufferLen  buffer size
 * @param invokingNode  invoking node
 * @param sourceTargetNamespace  source target namespace
 * @param importNamespace  import namespace
 * @param bucket  bucket
 * @returns 0 on success, a positive error code on errors and
 *         -1 in case of an internal or API error.
 */

int xml_xsd10_add_schema_doc(XmlXsd10ParserCtxt *pctxt,
		int type,
		const char *schemaLocation,
		XmlDom *schemaDoc,
		const char *schemaBuffer,
		int schemaBufferLen,
		XMQNode *invokingNode,
		const char *sourceTargetNamespace,
		const char *importNamespace,
		XmlXsd10Bucket **bucket)
{
    const char *targetNamespace = NULL;
    XmlXsd10SchemaRelation *relation = NULL;
    XmlDom *doc = NULL;
    int res = 0, err = 0, located = 0, preserveDoc = 0;
    XmlXsd10Bucket *bkt = NULL;

    if (bucket != NULL)
	*bucket = NULL;

    switch (type) {
	case XML_XSD10_SCHEMA_SCHEMA_IMPORT:
	case XML_XSD10_SCHEMA_SCHEMA_MAIN:
	    err = XML_XSD10_SCHEMAP_SRC_IMPORT;
	    break;
	case XML_XSD10_SCHEMA_SCHEMA_INCLUDE:
	    err = XML_XSD10_SCHEMAP_SRC_INCLUDE;
	    break;
	case XML_XSD10_SCHEMA_SCHEMA_REDEFINE:
	    err = XML_XSD10_SCHEMAP_SRC_REDEFINE;
	    break;
    }


    /* Special handling for the main schema:
    * skip the location and relation logic and just parse the doc.
    * We need just a bucket to be returned in this case.
    */
    if ((type == XML_XSD10_SCHEMA_SCHEMA_MAIN) || (! XSD10_WXS_HAS_BUCKETS(pctxt)))
	goto doc_load;

    /* Note that we expect the location to be an absolute URI. */
    if (schemaLocation != NULL) {
	bkt = xml_xsd10_get_schema_bucket(pctxt, schemaLocation);
	if ((bkt != NULL) &&
	    (pctxt->constructor->bucket == bkt)) {
	    /* Report self-imports/inclusions/redefinitions. */

	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, err,
		invokingNode, NULL,
		"The schema must not import/include/redefine itself",
		NULL, NULL);
	    goto exit;
	}
    }
    /*
    * Create a relation for the graph of schemas.
    */
    relation = xml_xsd10_schema_relation_create();
    if (relation == NULL)
	return(-1);
    xml_xsd10_schema_relation_add_child(pctxt->constructor->bucket,
	relation);
    relation->type = type;

    /*
    * Save the namespace import information.
    */
    if (XSD10_WXS_IS_BUCKET_IMPMAIN(type)) {
	relation->importNamespace = importNamespace;
	if (schemaLocation == NULL) {
	    /*
	    * No location; this is just an import of the namespace.
	    * Note that we don't assign a bucket to the relation
	    * in this case.
	    */
	    goto exit;
	}
	targetNamespace = importNamespace;
    }

    /* Did we already fetch the doc? */
    if (bkt != NULL) {
	if ((XSD10_WXS_IS_BUCKET_IMPMAIN(type)) && (! bkt->imported)) {
	    /*
	    * We included/redefined and then try to import a schema,
	    * but the new location provided for import was different.
	    */
	    if (schemaLocation == NULL)
		schemaLocation =  "in_memory_buffer";
	    if (!xml_xsd10_str_equal(schemaLocation,
		bkt->schemaLocation)) {
		xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, err,
		    invokingNode, NULL,
		    "The schema document '%s' cannot be imported, since "
		    "it was already included or redefined",
		    schemaLocation, NULL);
		goto exit;
	    }
	} else if ((! XSD10_WXS_IS_BUCKET_IMPMAIN(type)) && (bkt->imported)) {
	    /*
	    * We imported and then try to include/redefine a schema,
	    * but the new location provided for the include/redefine
	    * was different.
	    */
	    if (schemaLocation == NULL)
		schemaLocation =  "in_memory_buffer";
	    if (!xml_xsd10_str_equal(schemaLocation,
		bkt->schemaLocation)) {
		xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, err,
		    invokingNode, NULL,
		    "The schema document '%s' cannot be included or "
		    "redefined, since it was already imported",
		    schemaLocation, NULL);
		goto exit;
	    }
	}
    }

    if (XSD10_WXS_IS_BUCKET_IMPMAIN(type)) {
	/*
	* Given that the schemaLocation [attribute] is only a hint, it is open
	* to applications to ignore all but the first <import> for a given
	* namespace, regardless of the `actual value` of schemaLocation, but
	* such a strategy risks missing useful information when new
	* schemaLocations are offered.
	*
	* We will use the first <import> that comes with a location.
	* Further <import>s *with* a location, will result in an error.
	* TODO: Better would be to just report a warning here, but
	* we'll try it this way until someone complains.
	*
	* Schema Document Location Strategy:
	* 3 Based on the namespace name, identify an existing schema document,
	* either as a resource which is an XML document or a <schema> element
	* information item, in some local schema repository;
	* 5 Attempt to resolve the namespace name to locate such a resource.
	*
	* NOTE: (3) and (5) are not supported.
	*/
	if (bkt != NULL) {
	    relation->bucket = bkt;
	    goto exit;
	}
	bkt = xml_xsd10_get_schema_bucket_by_tns(pctxt,
	    importNamespace, 1);

	if (bkt != NULL) {
	    relation->bucket = bkt;
	    if (bkt->schemaLocation == NULL) {
		/* First given location of the schema; load the doc. */
		bkt->schemaLocation = schemaLocation;
	    } else {
		if (!xml_xsd10_str_equal(schemaLocation,
		    bkt->schemaLocation)) {
		    /*
		    * Additional location given; just skip it.
		    * URGENT TODO: We should report a warning here.
		    * res = XML_SCHEMAP_SRC_IMPORT;
		    */
		    if (schemaLocation == NULL)
			schemaLocation =  "in_memory_buffer";

		    xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
			XML_XSD10_SCHEMAP_WARN_SKIP_SCHEMA,
			invokingNode, NULL,
			"Skipping import of schema located at '%s' for the "
			"namespace '%s', since this namespace was already "
			"imported with the schema located at '%s'",
			schemaLocation, importNamespace, bkt->schemaLocation);
		}
		goto exit;
	    }
	}
	/*
	* No bucket + first location: load the doc and create a
	* bucket.
	*/
    } else {
	/* <include> and <redefine> */
	if (bkt != NULL) {

	    if ((bkt->origTargetNamespace == NULL) &&
		(bkt->targetNamespace != sourceTargetNamespace)) {
		XmlXsd10Bucket *chamel;

		/*
		* Chameleon include/redefine: skip loading only if it was
		* already build for the targetNamespace of the including
		* schema.
		*/
		/*
		* URGENT TODO: If the schema is a chameleon-include then copy
		* the components into the including schema and modify the
		* targetNamespace of those components, do nothing otherwise.
		* NOTE: This is currently worked-around by compiling the
		* chameleon for every distinct including targetNamespace; thus
		* not performant at the moment.
		* TODO: Check when the namespace in wildcards for chameleons
		* needs to be converted: before we built wildcard intersections
		* or after.
		*   Answer: after!
		*/
		chamel = xml_xsd10_get_chameleon_schema_bucket(pctxt,
		    schemaLocation, sourceTargetNamespace);
		if (chamel != NULL) {
		    /* A fitting chameleon was already parsed; NOP. */
		    relation->bucket = chamel;
		    goto exit;
		}
		/*
		* We need to parse the chameleon again for a different
		* targetNamespace.
		* CHAMELEON TODO: Optimize this by only parsing the
		* chameleon once, and then copying the components to
		* the new targetNamespace.
		*/
		bkt = NULL;
	    } else {
		relation->bucket = bkt;
		goto exit;
	    }
	}
    }
    if ((bkt != NULL) && (bkt->doc != NULL)) {
	XSD10_PERROR_INT("xmlSchemaAddSchemaDoc",
	    "trying to load a schema doc, but a doc is already "
	    "assigned to the schema bucket");
	goto exit_failure;
    }

doc_load:
    /*
    * Load the document.
    */
    if (schemaDoc != NULL) {
	doc = schemaDoc;
	/* Don' free this one, since it was provided by the caller. */
	preserveDoc = 1;
	/* TODO: Does the context or the doc hold the location? */
	if (xml_dom_url(schemaDoc) != NULL)
	    schemaLocation = xml_xsd10_dict_lookup(pctxt->dict,
		xml_dom_url(schemaDoc), -1);
        else
	    schemaLocation =  "in_memory_buffer";
    } else if ((schemaLocation != NULL) || (schemaBuffer != NULL)) {
	/* The loading via xmlNewParserCtxt/xmlCtxtReadFile/xmlCtxtReadMemory (with the
	   structured error handler and the resource loader of pctxt) is done by
	   xml_xsd10_p_read_file/xml_xsd10_p_read_memory, notLocated replaces the check
	   of the last error for the XML_FROM_IO domain below. */
	int notLocated = 0;

	if (schemaLocation != NULL) {
	    /* Parse from file. */
	    doc = xml_xsd10_p_read_file(pctxt, schemaLocation, &notLocated);
	} else if (schemaBuffer != NULL) {
	    /* Parse from memory buffer. */
	    doc = xml_xsd10_p_read_memory(pctxt, schemaBuffer, schemaBufferLen,
		&notLocated);
	    schemaLocation =  "in_memory_buffer";
	    if (doc != NULL)
		xml_dom_set_url(doc, schemaLocation);
	}
	/*
	* For <import>:
	* 2.1 The referent is (a fragment of) a resource which is an
	* XML document (see clause 1.1), which in turn corresponds to
	* a <schema> element information item in a well-formed information
	* set, which in turn corresponds to a valid schema.
	* TODO: (2.1) fragments of XML documents are not supported.
	*
	* 2.2 The referent is a <schema> element information item in
	* a well-formed information set, which in turn corresponds
	* to a valid schema.
	* TODO: (2.2) is not supported.
	*/
	if (doc == NULL) {
	    /*
	    * Check if this a parser error, or if the document could
	    * just not be located.
	    * TODO: Try to find specific error codes to react only on
	    * localisation failures.
	    */
	    if (! notLocated) {
		/*
		* We assume a parser error here.
		*/
		located = 1;
		/* TODO: Error code ?? */
		res = XML_XSD10_SCHEMAP_SRC_IMPORT_2_1;
		xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, res,
		    invokingNode, NULL,
		    "Failed to parse the XML resource '%s'",
		    schemaLocation, NULL);
	    }
	}
	if ((doc == NULL) && located)
	    goto exit_error;
    } else {
	xml_xsd10_p_err(pctxt, NULL,
	    XML_XSD10_SCHEMAP_NOTHING_TO_PARSE,
	    "No information for parsing was provided with the "
	    "given schema parser context.\n",
	    NULL, NULL);
	goto exit_failure;
    }
    /*
    * Preprocess the document.
    */
    if (doc != NULL) {
	XMQNode *docElem = NULL;

	located = 1;
	docElem = xml_dom_root_element(doc);
	if (docElem == NULL) {
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, XML_XSD10_SCHEMAP_NOROOT,
		invokingNode, NULL,
		"The document '%s' has no document element",
		schemaLocation, NULL);
	    goto exit_error;
	}
	/*
	* Remove all the blank text nodes.
	*/
	xml_xsd10_cleanup_doc(pctxt, docElem);
	/*
	* Check the schema's top level element.
	*/
	if (!XSD10_IS_SCHEMA(docElem, "schema")) {
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, XML_XSD10_SCHEMAP_NOT_SCHEMA,
		invokingNode, NULL,
		"The XML document '%s' is not a schema document",
		schemaLocation, NULL);
	    goto exit_error;
	}
	/*
	* Note that we don't apply a type check for the
	* targetNamespace value here.
	*/
	targetNamespace = xml_xsd10_get_prop(pctxt, docElem,
	    "targetNamespace");
    }

/* after_doc_loading: */
    if ((bkt == NULL) && located) {
	/* Only create a bucket if the schema was located. */
        bkt = xml_xsd10_bucket_create(pctxt, type,
	    targetNamespace);
	if (bkt == NULL)
	    goto exit_failure;
    }
    if (bkt != NULL) {
	bkt->schemaLocation = schemaLocation;
	bkt->located = located;
	if (doc != NULL) {
	    bkt->doc = doc;
	    bkt->targetNamespace = targetNamespace;
	    bkt->origTargetNamespace = targetNamespace;
	    if (preserveDoc)
		bkt->preserveDoc = 1;
	}
	if (XSD10_WXS_IS_BUCKET_IMPMAIN(type))
	    bkt->imported++;
	    /*
	    * Add it to the graph of schemas.
	    */
	if (relation != NULL)
	    relation->bucket = bkt;
    }

exit:
    /*
    * Return the bucket explicitly; this is needed for the
    * main schema.
    */
    if (bucket != NULL)
	*bucket = bkt;
    return (0);

exit_error:
    if ((doc != NULL) && (! preserveDoc)) {
	xml_dom_free(doc);
	if (bkt != NULL)
	    bkt->doc = NULL;
    }
    return(pctxt->err);

exit_failure:
    if ((doc != NULL) && (! preserveDoc)) {
	xml_dom_free(doc);
	if (bkt != NULL)
	    bkt->doc = NULL;
    }
    return (-1);
}

/**
 * Port of xmlSchemaParseImport.
 *
 * parse a XML schema Import definition
 * *WARNING* this interface is highly subject to change
 *
 * @param pctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @returns 0 in case of success, a positive error code if
 * not valid and -1 in case of an internal error.
 */
int xml_xsd10_parse_import(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema,
                     XMQNode *node)
{
    XMQNode *child;
    const char *namespaceName = NULL, *schemaLocation = NULL;
    const char *thisTargetNamespace;
    XMQNode *attr;
    int ret = 0;
    XmlXsd10Bucket *bucket = NULL;

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (-1);

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "namespace")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "schemaLocation"))) {
		xml_xsd10_p_illegal_attr_err(pctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /*
    * Extract and validate attributes.
    */
    if (xml_xsd10_p_val_attr(pctxt, NULL, node,
	"namespace", xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI),
	&namespaceName) != 0) {
	xml_xsd10_p_simple_type_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    NULL, node,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI),
	    NULL, namespaceName, NULL, NULL, NULL);
	return (pctxt->err);
    }

    if (xml_xsd10_p_val_attr(pctxt, NULL, node,
	"schemaLocation", xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI),
	&schemaLocation) != 0) {
	xml_xsd10_p_simple_type_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
	    NULL, node,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI),
	    NULL, schemaLocation, NULL, NULL, NULL);
	return (pctxt->err);
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        /*
         * the annotation here is simply discarded ...
	 * TODO: really?
         */
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?)");
    }
    /*
    * Apply additional constraints.
    *
    * Note that it is important to use the original @targetNamespace
    * (or none at all), to rule out imports of schemas _with_ a
    * @targetNamespace if the importing schema is a chameleon schema
    * (with no @targetNamespace).
    */
    thisTargetNamespace = XSD10_WXS_BUCKET(pctxt)->origTargetNamespace;
    if (namespaceName != NULL) {
	/*
	* 1.1 If the namespace [attribute] is present, then its `actual value`
	* must not match the `actual value` of the enclosing <schema>'s
	* targetNamespace [attribute].
	*/
	if (xml_xsd10_str_equal(thisTargetNamespace, namespaceName)) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_SRC_IMPORT_1_1,
		NULL, node,
		"The value of the attribute 'namespace' must not match "
		"the target namespace '%s' of the importing schema",
		thisTargetNamespace);
	    return (pctxt->err);
	}
    } else {
	/*
	* 1.2 If the namespace [attribute] is not present, then the enclosing
	* <schema> must have a targetNamespace [attribute].
	*/
	if (thisTargetNamespace == NULL) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_SRC_IMPORT_1_2,
		NULL, node,
		"The attribute 'namespace' must be existent if "
		"the importing schema has no target namespace",
		NULL);
	    return (pctxt->err);
	}
    }
    /*
    * Locate and acquire the schema document.
    */
    if (schemaLocation != NULL)
	schemaLocation = xml_xsd10_build_absolute_uri(pctxt->dict,
	    schemaLocation, node, NULL);
    ret = xml_xsd10_add_schema_doc(pctxt, XML_XSD10_SCHEMA_SCHEMA_IMPORT,
	schemaLocation, NULL, NULL, 0, node, thisTargetNamespace,
	namespaceName, &bucket);

    if (ret != 0)
	return(ret);

    /*
    * For <import>: "It is *not* an error for the application
    * schema reference strategy to fail."
    * So just don't parse if no schema document was found.
    * Note that we will get no bucket if the schema could not be
    * located or if there was no schemaLocation.
    */
    if ((bucket == NULL) && (schemaLocation != NULL)) {
	xml_xsd10_custom_warning(XSD10_ACTXT_CAST pctxt,
	    XML_XSD10_SCHEMAP_WARN_UNLOCATED_SCHEMA,
	    node, NULL,
	    "Failed to locate a schema at location '%s'. "
	    "Skipping the import", schemaLocation, NULL, NULL);
    }

    if ((bucket != NULL) && XSD10_CAN_PARSE_SCHEMA(bucket)) {
	ret = xml_xsd10_parse_new_doc(pctxt, schema, bucket);
    }

    return (ret);
}

/** Port of xmlSchemaParseIncludeOrRedefineAttrs. */
int xml_xsd10_parse_include_or_redefine_attrs(XmlXsd10ParserCtxt *pctxt,
				     XmlXsd10Schema *schema,
				     XMQNode *node,
				     char **schemaLocation,
				     int type)
{
    XMQNode *attr;

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL) ||
	(schemaLocation == NULL))
        return (-1);

    *schemaLocation = NULL;
    /*
    * Check for illegal attributes.
    * Applies for both <include> and <redefine>.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "schemaLocation"))) {
		xml_xsd10_p_illegal_attr_err(pctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(pctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    xml_xsd10_p_val_attr_id(pctxt, node,  "id");
    /*
    * Preliminary step, extract the URI-Reference and make an URI
    * from the base.
    */
    /*
    * Attribute "schemaLocation" is mandatory.
    */
    attr = xml_xsd10_get_prop_node(node, "schemaLocation");
    if (attr != NULL) {
        char *base = NULL;
        char *uri = NULL;

	if (xml_xsd10_p_val_attr_node(pctxt, NULL, attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYURI),
	    (const char **) schemaLocation) != 0)
	    goto exit_error;
	base = xml_xsd10_tree_node_get_base(xml_node_dom(node), node);
	if (base == NULL) {
	    uri = xml_uri_build(*schemaLocation, xml_dom_url(xml_node_dom(node)));
	} else {
	    uri = xml_uri_build(*schemaLocation, base);
	    free(base);
	}
	if (uri == NULL) {
	    XSD10_PERROR_INT("xmlSchemaParseIncludeOrRedefine",
		"could not build an URI from the schemaLocation")
	    goto exit_failure;
	}
	(*schemaLocation) = (char *) xml_xsd10_dict_lookup(pctxt->dict, uri, -1);
	free(uri);
    } else {
	xml_xsd10_p_missing_attr_err(pctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "schemaLocation", NULL);
	goto exit_error;
    }
    /*
    * Report self-inclusion and self-redefinition.
    */
    if (xml_xsd10_str_equal(*schemaLocation, pctxt->URL)) {
	if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE) {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_SRC_REDEFINE,
		NULL, node,
		"The schema document '%s' cannot redefine itself.",
		*schemaLocation);
	} else {
	    xml_xsd10_p_custom_err(pctxt,
		XML_XSD10_SCHEMAP_SRC_INCLUDE,
		NULL, node,
		"The schema document '%s' cannot include itself.",
		*schemaLocation);
	}
	goto exit_error;
    }

    return(0);
exit_error:
    return(pctxt->err);
exit_failure:
    return(-1);
}

/** Port of xmlSchemaParseIncludeOrRedefine. */
int xml_xsd10_parse_include_or_redefine(XmlXsd10ParserCtxt *pctxt,
				XmlXsd10Schema *schema,
				XMQNode *node,
				int type)
{
    XMQNode *child = NULL;
    const char *schemaLocation = NULL;
    int res = 0; /* hasRedefinitions = 0 */
    int isChameleon = 0, wasChameleon = 0;
    XmlXsd10Bucket *bucket = NULL;

    if ((pctxt == NULL) || (schema == NULL) || (node == NULL))
        return (-1);

    /*
    * Parse attributes. Note that the returned schemaLocation will
    * be already converted to an absolute URI.
    */
    res = xml_xsd10_parse_include_or_redefine_attrs(pctxt, schema,
	node, (char **) (&schemaLocation), type);
    if (res != 0)
	return(res);
    /*
    * Load and add the schema document.
    */
    res = xml_xsd10_add_schema_doc(pctxt, type, schemaLocation, NULL,
	NULL, 0, node, pctxt->targetNamespace, NULL, &bucket);
    if (res != 0)
	return(res);
    /*
    * If we get no schema bucket back, then this means that the schema
    * document could not be located or was broken XML or was not
    * a schema document.
    */
    if ((bucket == NULL) || (bucket->doc == NULL)) {
	if (type == XML_XSD10_SCHEMA_SCHEMA_INCLUDE) {
	    /*
	    * WARNING for <include>:
	    * We will raise an error if the schema cannot be located
	    * for inclusions, since the that was the feedback from the
	    * schema people. I.e. the following spec piece will *not* be
	    * satisfied:
	    * SPEC src-include: "It is not an error for the `actual value` of the
	    * schemaLocation [attribute] to fail to resolve it all, in which
	    * case no corresponding inclusion is performed.
	    * So do we need a warning report here?"
	    */
	    res = XML_XSD10_SCHEMAP_SRC_INCLUDE;
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, res,
		node, NULL,
		"Failed to load the document '%s' for inclusion",
		schemaLocation, NULL);
	} else {
	    /*
	    * NOTE: This was changed to raise an error even if no redefinitions
	    * are specified.
	    *
	    * SPEC src-redefine (1)
	    * "If there are any element information items among the [children]
	    * other than <annotation> then the `actual value` of the
	    * schemaLocation [attribute] must successfully resolve."
	    * TODO: Ask the WG if a the location has always to resolve
	    * here as well!
	    */
	    res = XML_XSD10_SCHEMAP_SRC_REDEFINE;
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt, res,
		node, NULL,
		"Failed to load the document '%s' for redefinition",
		schemaLocation, NULL);
	}
    } else {
	/*
	* Check targetNamespace sanity before parsing the new schema.
	* TODO: Note that we won't check further content if the
	* targetNamespace was bad.
	*/
	if (bucket->origTargetNamespace != NULL) {
	    /*
	    * SPEC src-include (2.1)
	    * "SII has a targetNamespace [attribute], and its `actual
	    * value` is identical to the `actual value` of the targetNamespace
	    * [attribute] of SII' (which must have such an [attribute])."
	    */
	    if (pctxt->targetNamespace == NULL) {
		xml_xsd10_custom_err(XSD10_ACTXT_CAST pctxt,
		    XML_XSD10_SCHEMAP_SRC_INCLUDE,
		    node, NULL,
		    "The target namespace of the included/redefined schema "
		    "'%s' has to be absent, since the including/redefining "
		    "schema has no target namespace",
		    schemaLocation, NULL);
		goto exit_error;
	    } else if (!xml_xsd10_str_equal(bucket->origTargetNamespace,
		pctxt->targetNamespace)) {
		/* TODO: Change error function. */
		xml_xsd10_p_custom_err_ext(pctxt,
		    XML_XSD10_SCHEMAP_SRC_INCLUDE,
		    NULL, node,
		    "The target namespace '%s' of the included/redefined "
		    "schema '%s' differs from '%s' of the "
		    "including/redefining schema",
		    bucket->origTargetNamespace, schemaLocation,
		    pctxt->targetNamespace);
		goto exit_error;
	    }
	} else if (pctxt->targetNamespace != NULL) {
	    /*
	    * Chameleons: the original target namespace will
	    * differ from the resulting namespace.
	    */
	    isChameleon = 1;
	    bucket->targetNamespace = pctxt->targetNamespace;
	}
    }
    /*
    * Parse the schema.
    */
    if (bucket && (!bucket->parsed) && (bucket->doc != NULL)) {
	if (isChameleon) {
	    /* TODO: Get rid of this flag on the schema itself. */
	    if ((schema->flags & XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS) == 0) {
		schema->flags |= XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS;
	    } else
		wasChameleon = 1;
	}
	xml_xsd10_parse_new_doc(pctxt, schema, bucket);
	/* Restore chameleon flag. */
	if (isChameleon && (!wasChameleon))
	    schema->flags ^= XML_XSD10_SCHEMAS_INCLUDING_CONVERT_NS;
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE) {
	/*
	* Parse (simpleType | complexType | group | attributeGroup))*
	*/
	pctxt->redefined = bucket;
	/*
	* How to proceed if the redefined schema was not located?
	*/
	pctxt->isRedefine = 1;
	while (XSD10_IS_SCHEMA(child, "annotation") ||
	    XSD10_IS_SCHEMA(child, "simpleType") ||
	    XSD10_IS_SCHEMA(child, "complexType") ||
	    XSD10_IS_SCHEMA(child, "group") ||
	    XSD10_IS_SCHEMA(child, "attributeGroup")) {
	    if (XSD10_IS_SCHEMA(child, "annotation")) {
		/*
		* TODO: discard or not?
		*/
	    } else if (XSD10_IS_SCHEMA(child, "simpleType")) {
		xml_xsd10_parse_simple_type(pctxt, schema, child, 1);
	    } else if (XSD10_IS_SCHEMA(child, "complexType")) {
		xml_xsd10_parse_complex_type(pctxt, schema, child, 1);
		/* hasRedefinitions = 1; */
	    } else if (XSD10_IS_SCHEMA(child, "group")) {
		/* hasRedefinitions = 1; */
		xml_xsd10_parse_model_group_definition(pctxt,
		    schema, child);
	    } else if (XSD10_IS_SCHEMA(child, "attributeGroup")) {
		/* hasRedefinitions = 1; */
		xml_xsd10_parse_attribute_group_definition(pctxt, schema,
		    child);
	    }
	    child = xsd10_parse_next(child);
	}
	pctxt->redefined = NULL;
	pctxt->isRedefine = 0;
    } else {
	if (XSD10_IS_SCHEMA(child, "annotation")) {
	    /*
	    * TODO: discard or not?
	    */
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	res = XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED;
	if (type == XML_XSD10_SCHEMA_SCHEMA_REDEFINE) {
	    xml_xsd10_p_content_err(pctxt, res,
		NULL, node, child, NULL,
		"(annotation | (simpleType | complexType | group | attributeGroup))*");
	} else {
	     xml_xsd10_p_content_err(pctxt, res,
		NULL, node, child, NULL,
		"(annotation?)");
	}
    }
    return(res);

exit_error:
    return(pctxt->err);
}

/** Port of xmlSchemaParseRedefine. */
int xml_xsd10_parse_redefine(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema,
                       XMQNode *node)
{
    int res;
#ifndef XSD10_ENABLE_REDEFINE
    TODO
    return(0);
#endif
    res = xml_xsd10_parse_include_or_redefine(pctxt, schema, node,
	XML_XSD10_SCHEMA_SCHEMA_REDEFINE);
    if (res != 0)
	return(res);
    return(0);
}

/** Port of xmlSchemaParseInclude. */
int xml_xsd10_parse_include(XmlXsd10ParserCtxt *pctxt, XmlXsd10Schema *schema,
                       XMQNode *node)
{
    int res;

    res = xml_xsd10_parse_include_or_redefine(pctxt, schema, node,
	XML_XSD10_SCHEMA_SCHEMA_INCLUDE);
    if (res != 0)
	return(res);
    return(0);
}

/**
 * Port of xmlSchemaParseModelGroup.
 *
 * parse a XML schema Sequence definition.
 * Applies parts of:
 *   Schema Representation Constraint:
 *     Redefinition Constraints and Semantics (src-redefine)
 *     (6.1), (6.1.1), (6.1.2)
 *
 *   Schema Component Constraint:
 *     All Group Limited (cos-all-limited) (2)
 *     TODO: Actually this should go to component-level checks,
 *     but is done here due to performance. Move it to an other layer
 *     is schema construction via an API is implemented.
 *
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param type  the "compositor" type
 * @param withParticle  if a a model group with a particle
 * @returns -1 in case of error, 0 if the declaration is improper and
 *         1 in case of success.
 */
XmlXsd10TreeItem *xml_xsd10_parse_model_group(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
			 XMQNode *node, XmlXsd10TypeType type,
			 int withParticle)
{
    XmlXsd10ModelGroup *item;
    XmlXsd10Particle *particle = NULL;
    XMQNode *child = NULL;
    XMQNode *attr;
    int min = 1, max = 1, isElemRef, hasRefs = 0;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /*
    * Create a model group with the given compositor.
    */
    item = xml_xsd10_add_model_group(ctxt, schema, type, node);
    if (item == NULL)
	return (NULL);

    if (withParticle) {
	if (type == XML_XSD10_SCHEMA_TYPE_ALL) {
	    min = xml_xsd10_get_min_occurs(ctxt, node, 0, 1, 1, "(0 | 1)");
	    max = xml_xsd10_get_max_occurs(ctxt, node, 1, 1, 1, "1");
	} else {
	    /* choice + sequence */
	    min = xml_xsd10_get_min_occurs(ctxt, node, 0, -1, 1, "xs:nonNegativeInteger");
	    max = xml_xsd10_get_max_occurs(ctxt, node, 0, XML_XSD10_UNBOUNDED, 1,
		"(xs:nonNegativeInteger | unbounded)");
	}
	xml_xsd10_p_check_particle_correct_2(ctxt, NULL, node, min, max);
	/*
	* Create a particle
	*/
	particle = xml_xsd10_add_particle(ctxt, node, min, max);
	if (particle == NULL)
	    return (NULL);
	particle->children = (XmlXsd10TreeItem *) item;
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "maxOccurs")) &&
		    (!xml_xsd10_str_equal(xml_node_name(attr),  "minOccurs"))) {
		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
    } else {
	/*
	* Check for illegal attributes.
	*/
	attr = xsd10_parse_first_attr(node);
	while (attr != NULL) {
	    if (xml_node_ns_uri(attr) == NULL) {
		if (!xml_xsd10_str_equal(xml_node_name(attr),  "id")) {
		    xml_xsd10_p_illegal_attr_err(ctxt,
			XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
	    } else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	    attr = xml_node_next(attr);
	}
    }

    /*
    * Extract and validate attributes.
    */
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        item->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    if (type == XML_XSD10_SCHEMA_TYPE_ALL) {
	XmlXsd10Particle *part, *last = NULL;

	while (XSD10_IS_SCHEMA(child, "element")) {
	    part = (XmlXsd10Particle *) xml_xsd10_parse_element(ctxt,
		schema, child, &isElemRef, 0);
	    /*
	    * SPEC cos-all-limited (2)
	    * "The {max occurs} of all the particles in the {particles}
	    * of the ('all') group must be 0 or 1.
	    */
	    if (part != NULL) {
		if (isElemRef)
		    hasRefs++;
		if (part->minOccurs > 1) {
		    xml_xsd10_p_custom_err(ctxt,
			XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
			NULL, child,
			"Invalid value for minOccurs (must be 0 or 1)",
			NULL);
		    /* Reset to 1. */
		    part->minOccurs = 1;
		}
		if (part->maxOccurs > 1) {
		    xml_xsd10_p_custom_err(ctxt,
			XML_XSD10_SCHEMAP_COS_ALL_LIMITED,
			NULL, child,
			"Invalid value for maxOccurs (must be 0 or 1)",
			NULL);
		    /* Reset to 1. */
		    part->maxOccurs = 1;
		}
		if (last == NULL)
		    item->children = (XmlXsd10TreeItem *) part;
		else
		    last->next = (XmlXsd10TreeItem *) part;
		last = part;
	    }
	    child = xsd10_parse_next(child);
	}
	if (child != NULL) {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, (annotation?, element*)");
	}
    } else {
	/* choice + sequence */
	XmlXsd10TreeItem *part = NULL, *last = NULL;

	while ((XSD10_IS_SCHEMA(child, "element")) ||
	    (XSD10_IS_SCHEMA(child, "group")) ||
	    (XSD10_IS_SCHEMA(child, "any")) ||
	    (XSD10_IS_SCHEMA(child, "choice")) ||
	    (XSD10_IS_SCHEMA(child, "sequence"))) {

	    if (XSD10_IS_SCHEMA(child, "element")) {
		part = (XmlXsd10TreeItem *)
		    xml_xsd10_parse_element(ctxt, schema, child, &isElemRef, 0);
		if (part && isElemRef)
		    hasRefs++;
	    } else if (XSD10_IS_SCHEMA(child, "group")) {
		part =
		    xml_xsd10_parse_model_group_def_ref(ctxt, schema, child);
		if (part != NULL)
		    hasRefs++;
		/*
		* Handle redefinitions.
		*/
		if (ctxt->isRedefine && ctxt->redef &&
		    (ctxt->redef->item->type == XML_XSD10_SCHEMA_TYPE_GROUP) &&
		    part && part->children)
		{
		    if ((xml_xsd10_get_q_name_ref_name(part->children) ==
			    ctxt->redef->refName) &&
			(xml_xsd10_get_q_name_ref_target_ns(part->children) ==
			    ctxt->redef->refTargetNs))
		    {
			/*
			* SPEC src-redefine:
			* (6.1) "If it has a <group> among its contents at
			* some level the `actual value` of whose ref
			* [attribute] is the same as the `actual value` of
			* its own name attribute plus target namespace, then
			* all of the following must be true:"
			* (6.1.1) "It must have exactly one such group."
			*/
			if (ctxt->redefCounter != 0) {
			    char *str = NULL;

			    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
				XML_XSD10_SCHEMAP_SRC_REDEFINE, child, NULL,
				"The redefining model group definition "
				"'%s' must not contain more than one "
				"reference to the redefined definition",
				xml_xsd10_format_q_name(&str,
				    ctxt->redef->refTargetNs,
				    ctxt->redef->refName),
				NULL);
			    XSD10_FREE_AND_NULL(str)
			    part = NULL;
			} else if (((XSD10_WXS_PARTICLE(part))->minOccurs != 1) ||
			    ((XSD10_WXS_PARTICLE(part))->maxOccurs != 1))
			{
			    char *str = NULL;
			    /*
			    * SPEC src-redefine:
			    * (6.1.2) "The `actual value` of both that
			    * group's minOccurs and maxOccurs [attribute]
			    * must be 1 (or `absent`).
			    */
			    xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
				XML_XSD10_SCHEMAP_SRC_REDEFINE, child, NULL,
				"The redefining model group definition "
				"'%s' must not contain a reference to the "
				"redefined definition with a "
				"maxOccurs/minOccurs other than 1",
				xml_xsd10_format_q_name(&str,
				    ctxt->redef->refTargetNs,
				    ctxt->redef->refName),
				NULL);
			    XSD10_FREE_AND_NULL(str)
			    part = NULL;
			}
			ctxt->redef->reference = XSD10_WXS_BASIC_CAST part;
			ctxt->redefCounter++;
		    }
		}
	    } else if (XSD10_IS_SCHEMA(child, "any")) {
		part = (XmlXsd10TreeItem *)
		    xml_xsd10_parse_any(ctxt, schema, child);
	    } else if (XSD10_IS_SCHEMA(child, "choice")) {
		part = xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
	    } else if (XSD10_IS_SCHEMA(child, "sequence")) {
		part = xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
	    }
	    if (part != NULL) {
		if (last == NULL)
		    item->children = part;
		else
		    last->next = part;
		last = part;
	    }
	    child = xsd10_parse_next(child);
	}
	if (child != NULL) {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, (element | group | choice | sequence | any)*)");
	}
    }
    if ((max == 0) && (min == 0))
	return (NULL);
    if (hasRefs) {
	/*
	* We need to resolve references.
	*/
	XSD10_WXS_ADD_PENDING(ctxt, item);
    }
    if (withParticle)
	return ((XmlXsd10TreeItem *) particle);
    else
	return ((XmlXsd10TreeItem *) item);
}

/**
 * Port of xmlSchemaParseRestriction.
 *
 * parse a XML schema Restriction definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param parentType  the parent type
 * @returns the type definition or NULL in case of error
 */
XmlXsd10Type *xml_xsd10_parse_restriction(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                          XMQNode *node, XmlXsd10TypeType parentType)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    type->flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION;

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "base"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    /*
    * Extract and validate attributes.
    */
    xml_xsd10_p_val_attr_id(ctxt, node,  "id");
    /*
    * Attribute
    */
    /*
    * Extract the base type. The "base" attribute is mandatory if inside
    * a complex type or if redefining.
    *
    * SPEC (1.2) "...otherwise (<restriction> has no <simpleType> "
    * among its [children]), the simple type definition which is
    * the {content type} of the type definition `resolved` to by
    * the `actual value` of the base [attribute]"
    */
    if (xml_xsd10_p_val_attr_q_name(ctxt, schema, NULL, node, "base",
	&(type->baseNs), &(type->base)) == 0)
    {
	if ((type->base == NULL) && (type->type == XML_XSD10_SCHEMA_TYPE_COMPLEX)) {
	    xml_xsd10_p_missing_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		NULL, node, "base", NULL);
	} else if ((ctxt->isRedefine) &&
	    (type->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL))
	{
	    if (type->base == NULL) {
		xml_xsd10_p_missing_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
		    NULL, node, "base", NULL);
	    } else if ((! xml_xsd10_str_equal(type->base, type->name)) ||
		(! xml_xsd10_str_equal(type->baseNs, type->targetNamespace)))
	    {
		char *str1 = NULL, *str2 = NULL;
		/*
		* REDEFINE: SPEC src-redefine (5)
		* "Within the [children], each <simpleType> must have a
		* <restriction> among its [children] ... the `actual value` of
		* whose base [attribute] must be the same as the `actual value`
		* of its own name attribute plus target namespace;"
		*/
		xml_xsd10_p_custom_err_ext(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
		    NULL, node, "This is a redefinition, but the QName "
		    "value '%s' of the 'base' attribute does not match the "
		    "type's designation '%s'",
		    xml_xsd10_format_q_name(&str1, type->baseNs, type->base),
		    xml_xsd10_format_q_name(&str2, type->targetNamespace,
			type->name), NULL);
		XSD10_FREE_AND_NULL(str1);
		XSD10_FREE_AND_NULL(str2);
		/* Avoid confusion and erase the values. */
		type->base = NULL;
		type->baseNs = NULL;
	    }
	}
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the simple type ancestor.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE) {
	/*
	* Corresponds to <simpleType><restriction><simpleType>.
	*/
	if (XSD10_IS_SCHEMA(child, "simpleType")) {
	    if (type->base != NULL) {
		/*
		* src-restriction-base-or-simpleType
		* Either the base [attribute] or the simpleType [child] of the
		* <restriction> element must be present, but not both.
		*/
		xml_xsd10_p_content_err(ctxt,
		    XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE,
		    NULL, node, child,
		    "The attribute 'base' and the <simpleType> child are "
		    "mutually exclusive", NULL);
	    } else {
		type->baseType = (XmlXsd10Type *)
		    xml_xsd10_parse_simple_type(ctxt, schema, child, 0);
	    }
	    child = xsd10_parse_next(child);
	} else if (type->base == NULL) {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_SRC_RESTRICTION_BASE_OR_SIMPLETYPE,
		NULL, node, child,
		"Either the attribute 'base' or a <simpleType> child "
		"must be present", NULL);
	}
    } else if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT) {
	/*
	* Corresponds to <complexType><complexContent><restriction>...
	* followed by:
	*
	* Model groups <all>, <choice> and <sequence>.
	*/
	if (XSD10_IS_SCHEMA(child, "all")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_ALL, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "choice")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt,
		    schema, child, XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "sequence")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
	    child = xsd10_parse_next(child);
	/*
	* Model group reference <group>.
	*/
	} else if (XSD10_IS_SCHEMA(child, "group")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group_def_ref(ctxt, schema, child);
	    /*
	    * Note that the reference will be resolved in
	    * xmlSchemaResolveTypeReferences();
	    */
	    child = xsd10_parse_next(child);
	}
    } else if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT) {
	/*
	* Corresponds to <complexType><simpleContent><restriction>...
	*
	* "1.1 the simple type definition corresponding to the <simpleType>
	* among the [children] of <restriction> if there is one;"
	*/
	if (XSD10_IS_SCHEMA(child, "simpleType")) {
	    /*
	    * We will store the to-be-restricted simple type in
	    * type->contentTypeDef *temporarily*.
	    */
	    type->contentTypeDef = (XmlXsd10Type *)
		xml_xsd10_parse_simple_type(ctxt, schema, child, 0);
	    if ( type->contentTypeDef == NULL)
		return (NULL);
	    child = xsd10_parse_next(child);
	}
    }

    if ((parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE) ||
	(parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT)) {
	XmlXsd10Facet *facet, *lastfacet = NULL;
	/*
	* Corresponds to <complexType><simpleContent><restriction>...
	* <simpleType><restriction>...
	*/

	/*
	* Add the facets to the simple type ancestor.
	*/
	/*
	* TODO: Datatypes: 4.1.3 Constraints on XML Representation of
	* Simple Type Definition Schema Representation Constraint:
	* *Single Facet Value*
	*/
	while ((XSD10_IS_SCHEMA(child, "minInclusive")) ||
	    (XSD10_IS_SCHEMA(child, "minExclusive")) ||
	    (XSD10_IS_SCHEMA(child, "maxInclusive")) ||
	    (XSD10_IS_SCHEMA(child, "maxExclusive")) ||
	    (XSD10_IS_SCHEMA(child, "totalDigits")) ||
	    (XSD10_IS_SCHEMA(child, "fractionDigits")) ||
	    (XSD10_IS_SCHEMA(child, "pattern")) ||
	    (XSD10_IS_SCHEMA(child, "enumeration")) ||
	    (XSD10_IS_SCHEMA(child, "whiteSpace")) ||
	    (XSD10_IS_SCHEMA(child, "length")) ||
	    (XSD10_IS_SCHEMA(child, "maxLength")) ||
	    (XSD10_IS_SCHEMA(child, "minLength"))) {
	    facet = xml_xsd10_parse_facet(ctxt, schema, child);
	    if (facet != NULL) {
		if (lastfacet == NULL)
		    type->facets = facet;
		else
		    lastfacet->next = facet;
		lastfacet = facet;
		lastfacet->next = NULL;
	    }
	    child = xsd10_parse_next(child);
	}
	/*
	* Create links for derivation and validation.
	*/
	if (type->facets != NULL) {
	    XmlXsd10FacetLink *facetLink, *lastFacetLink = NULL;

	    facet = type->facets;
	    do {
		facetLink = (XmlXsd10FacetLink *)
		    malloc(sizeof(XmlXsd10FacetLink));
		if (facetLink == NULL) {
		    xml_xsd10_p_err_memory(ctxt);
		    free(facetLink);
		    return (NULL);
		}
		facetLink->facet = facet;
		facetLink->next = NULL;
		if (lastFacetLink == NULL)
		    type->facetSet = facetLink;
		else
		    lastFacetLink->next = facetLink;
		lastFacetLink = facetLink;
		facet = facet->next;
	    } while (facet != NULL);
	}
    }
    if (type->type == XML_XSD10_SCHEMA_TYPE_COMPLEX) {
	/*
	* Attribute uses/declarations.
	*/
	if (xml_xsd10_parse_local_attributes(ctxt, schema, &child,
	    (XmlXsd10ItemList **) &(type->attrUses),
	    XML_XSD10_SCHEMA_TYPE_RESTRICTION, NULL) == -1)
	    return(NULL);
	/*
	* Attribute wildcard.
	*/
	if (XSD10_IS_SCHEMA(child, "anyAttribute")) {
	    type->attributeWildcard =
		xml_xsd10_parse_any_attribute(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT) {
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"annotation?, (group | all | choice | sequence)?, "
		"((attribute | attributeGroup)*, anyAttribute?))");
	} else if (parentType == XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT) {
	     xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, (simpleType?, (minExclusive | minInclusive | "
		"maxExclusive | maxInclusive | totalDigits | fractionDigits | "
		"length | minLength | maxLength | enumeration | whiteSpace | "
		"pattern)*)?, ((attribute | attributeGroup)*, anyAttribute?))");
	} else {
	    /* Simple type */
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, (simpleType?, (minExclusive | minInclusive | "
		"maxExclusive | maxInclusive | totalDigits | fractionDigits | "
		"length | minLength | maxLength | enumeration | whiteSpace | "
		"pattern)*))");
	}
    }
    return (NULL);
}

/**
 * Port of xmlSchemaParseExtension.
 *
 * Parses an `<extension>`, which is found inside a
 * `<simpleContent>` or `<complexContent>`.
 * *WARNING* this interface is highly subject to change.
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param parentType  parent type
 * @returns the type definition or NULL in case of error
 */
XmlXsd10Type *xml_xsd10_parse_extension(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                        XMQNode *node, XmlXsd10TypeType parentType)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    type->flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_EXTENSION;

    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "base"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");

    /*
    * Attribute "base" - mandatory.
    */
    if ((xml_xsd10_p_val_attr_q_name(ctxt, schema, NULL, node,
	"base", &(type->baseNs), &(type->base)) == 0) &&
	(type->base == NULL)) {
	xml_xsd10_p_missing_attr_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ATTR_MISSING,
	    NULL, node, "base", NULL);
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the type ancestor.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT) {
	/*
	* Corresponds to <complexType><complexContent><extension>... and:
	*
	* Model groups <all>, <choice>, <sequence> and <group>.
	*/
	if (XSD10_IS_SCHEMA(child, "all")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema,
		    child, XML_XSD10_SCHEMA_TYPE_ALL, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "choice")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema,
		    child, XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "sequence")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema,
		child, XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
	    child = xsd10_parse_next(child);
	} else if (XSD10_IS_SCHEMA(child, "group")) {
	    type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group_def_ref(ctxt, schema, child);
	    /*
	    * Note that the reference will be resolved in
	    * xmlSchemaResolveTypeReferences();
	    */
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	/*
	* Attribute uses/declarations.
	*/
	if (xml_xsd10_parse_local_attributes(ctxt, schema, &child,
	    (XmlXsd10ItemList **) &(type->attrUses),
	    XML_XSD10_SCHEMA_TYPE_EXTENSION, NULL) == -1)
	    return(NULL);
	/*
	* Attribute wildcard.
	*/
	if (XSD10_IS_SCHEMA(child, "anyAttribute")) {
	    ctxt->ctxtType->attributeWildcard =
		xml_xsd10_parse_any_attribute(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	if (parentType == XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT) {
	    /* Complex content extension. */
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, ((group | all | choice | sequence)?, "
		"((attribute | attributeGroup)*, anyAttribute?)))");
	} else {
	    /* Simple content extension. */
	    xml_xsd10_p_content_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
		NULL, node, child, NULL,
		"(annotation?, ((attribute | attributeGroup)*, "
		"anyAttribute?))");
	}
    }
    return (NULL);
}

/**
 * Port of xmlSchemaParseSimpleContent.
 *
 * parse a XML schema SimpleContent definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param hasRestrictionOrExtension  set if restriction or extension was found
 * @returns the type definition or NULL in case of error
 */
int xml_xsd10_parse_simple_content(XmlXsd10ParserCtxt *ctxt,
                            XmlXsd10Schema *schema, XMQNode *node,
			    int *hasRestrictionOrExtension)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL) ||
	(hasRestrictionOrExtension == NULL))
        return (-1);
    *hasRestrictionOrExtension = 0;
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    type->contentType = XML_XSD10_SCHEMA_CONTENT_SIMPLE;
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id"))) {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");

    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the complex type ancestor.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
	    NULL, node, NULL, NULL,
	    "(annotation?, (restriction | extension))");
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
	    NULL, node, NULL, NULL,
	    "(annotation?, (restriction | extension))");
    }
    if (XSD10_IS_SCHEMA(child, "restriction")) {
        xml_xsd10_parse_restriction(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT);
	(*hasRestrictionOrExtension) = 1;
        child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "extension")) {
        xml_xsd10_parse_extension(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_SIMPLE_CONTENT);
	(*hasRestrictionOrExtension) = 1;
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child, NULL,
	    "(annotation?, (restriction | extension))");
    }
    return (0);
}

/**
 * Port of xmlSchemaParseComplexContent.
 *
 * parse a XML schema ComplexContent definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param hasRestrictionOrExtension  set if restriction or extension was found
 * @returns the type definition or NULL in case of error
 */
int xml_xsd10_parse_complex_content(XmlXsd10ParserCtxt *ctxt,
                             XmlXsd10Schema *schema, XMQNode *node,
			     int *hasRestrictionOrExtension)
{
    XmlXsd10Type *type;
    XMQNode *child = NULL;
    XMQNode *attr;

    if ((ctxt == NULL) || (schema == NULL) || (node == NULL) ||
	(hasRestrictionOrExtension == NULL))
        return (-1);
    *hasRestrictionOrExtension = 0;
    /* Not a component, don't create it. */
    type = ctxt->ctxtType;
    /*
    * Check for illegal attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if ((!xml_xsd10_str_equal(xml_node_name(attr),  "id")) &&
		(!xml_xsd10_str_equal(xml_node_name(attr),  "mixed")))
	    {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }

    xml_xsd10_p_val_attr_id(ctxt, node,  "id");

    /*
    * Set the 'mixed' on the complex type ancestor.
    */
    if (xml_xsd10_get_boolean_prop(ctxt, node, "mixed", 0))  {
	if ((type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED) == 0)
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_MIXED;
    }
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
	/*
	* Add the annotation to the complex type ancestor.
	*/
	xml_xsd10_add_annotation((XmlXsd10AnnotItem *) type,
	    xml_xsd10_parse_annotation(ctxt, child, 1));
        child = xsd10_parse_next(child);
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
	    NULL, node, NULL,
	    NULL, "(annotation?, (restriction | extension))");
    }
    if (child == NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_MISSING,
	    NULL, node, NULL,
	    NULL, "(annotation?, (restriction | extension))");
    }
    if (XSD10_IS_SCHEMA(child, "restriction")) {
        xml_xsd10_parse_restriction(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT);
	(*hasRestrictionOrExtension) = 1;
        child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "extension")) {
        xml_xsd10_parse_extension(ctxt, schema, child,
	    XML_XSD10_SCHEMA_TYPE_COMPLEX_CONTENT);
	(*hasRestrictionOrExtension) = 1;
        child = xsd10_parse_next(child);
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?, (restriction | extension))");
    }
    return (0);
}

/**
 * Port of xmlSchemaParseComplexType.
 *
 * parse a XML schema Complex Type definition
 * *WARNING* this interface is highly subject to change
 *
 * @param ctxt  a schema validation context
 * @param schema  the schema being built
 * @param node  a subtree containing XML Schema information
 * @param topLevel  whether this is a top-level element
 * @returns the type definition or NULL in case of error
 */
XmlXsd10Type *xml_xsd10_parse_complex_type(XmlXsd10ParserCtxt *ctxt, XmlXsd10Schema *schema,
                          XMQNode *node, int topLevel)
{
    XmlXsd10Type *type, *ctxtType;
    XMQNode *child = NULL;
    const char *name = NULL;
    XMQNode *attr;
    const char *attrValue;
#ifdef XSD10_ENABLE_NAMED_LOCALS
    char buf[40];
#endif
    int final = 0, block = 0, hasRestrictionOrExtension = 0;


    if ((ctxt == NULL) || (schema == NULL) || (node == NULL))
        return (NULL);

    ctxtType = ctxt->ctxtType;

    if (topLevel) {
	attr = xml_xsd10_get_prop_node(node, "name");
	if (attr == NULL) {
	    xml_xsd10_p_missing_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_MISSING, NULL, node, "name", NULL);
	    return (NULL);
	} else if (xml_xsd10_p_val_attr_node(ctxt, NULL, attr,
	    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NCNAME), &name) != 0) {
	    return (NULL);
	}
    }

    if (topLevel == 0) {
	/*
	* Parse as local complex type definition.
	*/
#ifdef XSD10_ENABLE_NAMED_LOCALS
        snprintf(buf, 39, "#CT%d", ctxt->counter++ + 1);
	type = xml_xsd10_add_type(ctxt, schema,
	    XML_XSD10_SCHEMA_TYPE_COMPLEX,
	    xml_xsd10_dict_lookup(ctxt->dict, (const char *)buf, -1),
	    ctxt->targetNamespace, node, 0);
#else
	type = xml_xsd10_add_type(ctxt, schema,
	    XML_XSD10_SCHEMA_TYPE_COMPLEX,
	    NULL, ctxt->targetNamespace, node, 0);
#endif
	if (type == NULL)
	    return (NULL);
	name = type->name;
	type->node = node;
	type->type = XML_XSD10_SCHEMA_TYPE_COMPLEX;
	/*
	* TODO: We need the target namespace.
	*/
    } else {
	/*
	* Parse as global complex type definition.
	*/
	type = xml_xsd10_add_type(ctxt, schema,
	    XML_XSD10_SCHEMA_TYPE_COMPLEX,
	    name, ctxt->targetNamespace, node, 1);
	if (type == NULL)
	    return (NULL);
	type->node = node;
	type->type = XML_XSD10_SCHEMA_TYPE_COMPLEX;
	type->flags |= XML_XSD10_SCHEMAS_TYPE_GLOBAL;
    }
    type->targetNamespace = ctxt->targetNamespace;
    /*
    * Handle attributes.
    */
    attr = xsd10_parse_first_attr(node);
    while (attr != NULL) {
	if (xml_node_ns_uri(attr) == NULL) {
	    if (xml_xsd10_str_equal(xml_node_name(attr),  "id")) {
		/*
		* Attribute "id".
		*/
		xml_xsd10_p_val_attr_id(ctxt, node,  "id");
	    } else if (xml_xsd10_str_equal(xml_node_name(attr),  "mixed")) {
		/*
		* Attribute "mixed".
		*/
		if (xml_xsd10_p_get_bool_node_value(ctxt,
			NULL, (XMQNode *) attr))
		    type->flags |= XML_XSD10_SCHEMAS_TYPE_MIXED;
	    } else if (topLevel) {
		/*
		* Attributes of global complex type definitions.
		*/
		if (xml_xsd10_str_equal(xml_node_name(attr),  "name")) {
		    /* Pass. */
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "abstract")) {
		    /*
		    * Attribute "abstract".
		    */
		    if (xml_xsd10_p_get_bool_node_value(ctxt,
			    NULL, (XMQNode *) attr))
			type->flags |= XML_XSD10_SCHEMAS_TYPE_ABSTRACT;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "final")) {
		    /*
		    * Attribute "final".
		    */
		    attrValue = xml_xsd10_get_node_content(ctxt,
			(XMQNode *) attr);
		    if (xml_xsd10_p_val_attr_block_final(attrValue,
			&(type->flags),
			-1,
			XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION,
			XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION,
			-1, -1, -1) != 0)
		    {
			xml_xsd10_p_simple_type_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
			    NULL, (XMQNode *) attr, NULL,
			    "(#all | List of (extension | restriction))",
			    attrValue, NULL, NULL, NULL);
		    } else
			final = 1;
		} else if (xml_xsd10_str_equal(xml_node_name(attr),  "block")) {
		    /*
		    * Attribute "block".
		    */
		    attrValue = xml_xsd10_get_node_content(ctxt,
			(XMQNode *) attr);
		    if (xml_xsd10_p_val_attr_block_final(attrValue, &(type->flags),
			-1,
			XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION,
			XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION,
			-1, -1, -1) != 0) {
			xml_xsd10_p_simple_type_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_INVALID_VALUE,
			    NULL, (XMQNode *) attr, NULL,
			    "(#all | List of (extension | restriction)) ",
			    attrValue, NULL, NULL, NULL);
		    } else
			block = 1;
		} else {
			xml_xsd10_p_illegal_attr_err(ctxt,
			    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
		}
	    } else {
		xml_xsd10_p_illegal_attr_err(ctxt,
		    XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	    }
	} else if (xml_xsd10_str_equal(xml_node_ns_uri(attr), xml_xsd10_ns)) {
	    xml_xsd10_p_illegal_attr_err(ctxt,
		XML_XSD10_SCHEMAP_S4S_ATTR_NOT_ALLOWED, NULL, attr);
	}
	attr = xml_node_next(attr);
    }
    if (! block) {
	/*
	* Apply default "block" values.
	*/
	if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_RESTRICTION)
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION;
	if (schema->flags & XML_XSD10_SCHEMAS_BLOCK_DEFAULT_EXTENSION)
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION;
    }
    if (! final) {
	/*
	* Apply default "block" values.
	*/
	if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_RESTRICTION)
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_RESTRICTION;
	if (schema->flags & XML_XSD10_SCHEMAS_FINAL_DEFAULT_EXTENSION)
	    type->flags |= XML_XSD10_SCHEMAS_TYPE_FINAL_EXTENSION;
    }
    /*
    * And now for the children...
    */
    child = xsd10_parse_first_child(node);
    if (XSD10_IS_SCHEMA(child, "annotation")) {
        type->annot = xml_xsd10_parse_annotation(ctxt, child, 1);
        child = xsd10_parse_next(child);
    }
    ctxt->ctxtType = type;
    if (XSD10_IS_SCHEMA(child, "simpleContent")) {
	/*
	* <complexType><simpleContent>...
	* 3.4.3 : 2.2
	* Specifying mixed='true' when the <simpleContent>
	* alternative is chosen has no effect
	*/
	if (type->flags & XML_XSD10_SCHEMAS_TYPE_MIXED)
	    type->flags ^= XML_XSD10_SCHEMAS_TYPE_MIXED;
        xml_xsd10_parse_simple_content(ctxt, schema, child,
	    &hasRestrictionOrExtension);
        child = xsd10_parse_next(child);
    } else if (XSD10_IS_SCHEMA(child, "complexContent")) {
	/*
	* <complexType><complexContent>...
	*/
	type->contentType = XML_XSD10_SCHEMA_CONTENT_EMPTY;
        xml_xsd10_parse_complex_content(ctxt, schema, child,
	    &hasRestrictionOrExtension);
        child = xsd10_parse_next(child);
    } else {
	/*
	* E.g <complexType><sequence>... or <complexType><attribute>... etc.
	*
	* SPEC
	* "...the third alternative (neither <simpleContent> nor
	* <complexContent>) is chosen. This case is understood as shorthand
	* for complex content restricting the `ur-type definition`, and the
	* details of the mappings should be modified as necessary.
	*/
	type->baseType = xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYTYPE);
	type->flags |= XML_XSD10_SCHEMAS_TYPE_DERIVATION_METHOD_RESTRICTION;
	/*
	* Parse model groups.
	*/
        if (XSD10_IS_SCHEMA(child, "all")) {
            type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_ALL, 1);
            child = xsd10_parse_next(child);
        } else if (XSD10_IS_SCHEMA(child, "choice")) {
            type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_CHOICE, 1);
            child = xsd10_parse_next(child);
        } else if (XSD10_IS_SCHEMA(child, "sequence")) {
            type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group(ctxt, schema, child,
		    XML_XSD10_SCHEMA_TYPE_SEQUENCE, 1);
            child = xsd10_parse_next(child);
        } else if (XSD10_IS_SCHEMA(child, "group")) {
            type->subtypes = (XmlXsd10Type *)
		xml_xsd10_parse_model_group_def_ref(ctxt, schema, child);
	    /*
	    * Note that the reference will be resolved in
	    * xmlSchemaResolveTypeReferences();
	    */
            child = xsd10_parse_next(child);
        }
	/*
	* Parse attribute decls/refs.
	*/
        if (xml_xsd10_parse_local_attributes(ctxt, schema, &child,
	    (XmlXsd10ItemList **) &(type->attrUses),
	    XML_XSD10_SCHEMA_TYPE_RESTRICTION, NULL) == -1)
	    return(NULL);
	/*
	* Parse attribute wildcard.
	*/
	if (XSD10_IS_SCHEMA(child, "anyAttribute")) {
	    type->attributeWildcard = xml_xsd10_parse_any_attribute(ctxt, schema, child);
	    child = xsd10_parse_next(child);
	}
    }
    if (child != NULL) {
	xml_xsd10_p_content_err(ctxt,
	    XML_XSD10_SCHEMAP_S4S_ELEM_NOT_ALLOWED,
	    NULL, node, child,
	    NULL, "(annotation?, (simpleContent | complexContent | "
	    "((group | all | choice | sequence)?, ((attribute | "
	    "attributeGroup)*, anyAttribute?))))");
    }
    /*
    * REDEFINE: SPEC src-redefine (5)
    */
    if (topLevel && ctxt->isRedefine && (! hasRestrictionOrExtension)) {
	xml_xsd10_p_custom_err(ctxt, XML_XSD10_SCHEMAP_SRC_REDEFINE,
	    NULL, node, "This is a redefinition, thus the "
	    "<complexType> must have a <restriction> or <extension> "
	    "grand-child", NULL);
    }
    ctxt->ctxtType = ctxtType;
    return (type);
}

#undef XSD10_ENABLE_REDEFINE
#undef XSD10_IS_BLANK_CH
#undef XSD10_IS_BLANK_NODE
#undef XSD10_WXS_FIND_GLOBAL_ITEM
#undef XSD10_WXS_COMP_NAME
#undef XSD10_WXS_COMP_TNS
#undef XSD10_WXS_ATTR_DEF_VAL_DEFAULT
#undef XSD10_WXS_ATTR_DEF_VAL_FIXED
#undef XSD10_ADD_ANNOTATION
#undef XSD10_IS_BAD_SCHEMA_DOC

#endif // XML_XSD10_PARSE_MODULE
