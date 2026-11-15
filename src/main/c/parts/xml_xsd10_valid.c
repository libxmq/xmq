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

/* Validation (port of libxml2 xmlschemas.c: simple type validation, DOM
   validation, identity constraints and the validation interfaces). */

#ifndef BUILDING_DIST_XMQ

#define XML_XSD10_INTERNALS
#include"xml_xsd10_schemas.h"
#include"xml_xsd10_valid.h"
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

#ifdef XML_XSD10_VALID_MODULE

#define XSD10_VALID_IS_BLANK_CH(c) (((c) == 0x20) || ((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd))

/************************************************************************
 *									*
 *			Simple type validation				*
 *									*
 ************************************************************************/


/************************************************************************
 *									*
 *			DOM Validation code				*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaAssembleByLocation.
 *
 * Expands an existing schema by an additional schema.
 *
 * @param vctxt  a schema validation context
 * @param schema  the existing schema
 * @param node  the node that fired the assembling
 * @param nsName  the namespace name of the new schema
 * @param location  the location of the schema
 * @returns 0 if the new schema is correct, a positive error code
 * number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_assemble_by_location(XmlXsd10ValidCtxt *vctxt, XmlXsd10Schema *schema, XMQNode *node, const char *nsName, const char *location)
{
    int ret = 0;
    XmlXsd10ParserCtxt *pctxt;
    XmlXsd10Bucket *bucket = NULL;
    const char *baseURI = NULL;

    if ((vctxt == NULL) || (schema == NULL))
	return (-1);

    if (vctxt->pctxt == NULL) {
	XSD10_VERROR_INT("xmlSchemaAssembleByLocation",
	    "no parser context available");
	return(-1);
    }
    pctxt = vctxt->pctxt;
    if (pctxt->constructor == NULL) {
	XSD10_PERROR_INT("xmlSchemaAssembleByLocation",
	    "no constructor");
	return(-1);
    }
    /*
    * Acquire the schema document.
    * In streaming mode, node is NULL, so pass the input document's
    * filename as the base URI for resolving relative locations.
    * (The streaming modes are not ported: node is never NULL here.)
    */
    location = xml_xsd10_build_absolute_uri(pctxt->dict,
        location, node, baseURI);
    /*
    * Note that we pass XML_SCHEMA_SCHEMA_IMPORT here;
    * the process will automatically change this to
    * XML_SCHEMA_SCHEMA_MAIN if it is the first schema document.
    */
    ret = xml_xsd10_add_schema_doc(pctxt, XML_XSD10_SCHEMA_SCHEMA_IMPORT,
	location, NULL, NULL, 0, node, NULL, nsName,
	&bucket);
    if (ret != 0)
	return(ret);
    if (bucket == NULL) {
	/*
	* Generate a warning that the document could not be located.
	*/
	xml_xsd10_custom_warning(XSD10_ACTXT_CAST vctxt, XML_XSD10_SCHEMAV_MISC,
	    node, NULL,
	    "The document at location '%s' could not be acquired",
	    location, NULL, NULL);
	return(ret);
    }
    /*
    * The first located schema will be handled as if all other
    * schemas imported by XSI were imported by this first schema.
    */
    if ((bucket != NULL) &&
	(XSD10_WXS_CONSTRUCTOR(pctxt)->bucket == NULL))
	XSD10_WXS_CONSTRUCTOR(pctxt)->bucket = bucket;
    /*
    * TODO: Is this handled like an import? I.e. is it not an error
    * if the schema cannot be located?
    */
    if ((bucket == NULL) || (! XSD10_CAN_PARSE_SCHEMA(bucket)))
	return(0);
    /*
    * We will reuse the parser context for every schema imported
    * directly via XSI. So reset the context.
    */
    pctxt->nberrors = 0;
    pctxt->err = 0;
    pctxt->doc = bucket->doc;

    ret = xml_xsd10_parse_new_doc_with_context(pctxt, schema, bucket);
    if (ret == -1) {
	pctxt->doc = NULL;
	goto exit_failure;
    }
    /* Paranoid error channelling. */
    if ((ret == 0) && (pctxt->nberrors != 0))
	ret = pctxt->err;
    if (pctxt->nberrors == 0) {
	/*
	* Only bother to fixup pending components, if there was
	* no error yet.
	* For every XSI acquired schema (and its sub-schemata) we will
	* fixup the components.
	*/
	xml_xsd10_fixup_components(pctxt, bucket);
	ret = pctxt->err;
	/*
	* Not nice, but we need somehow to channel the schema parser
	* error to the validation context.
	*/
	if ((ret != 0) && (vctxt->err == 0))
	    vctxt->err = ret;
	vctxt->nberrors += pctxt->nberrors;
    } else {
	/* Add to validation error sum. */
	vctxt->nberrors += pctxt->nberrors;
    }
    pctxt->doc = NULL;
    return(ret);
exit_failure:
    pctxt->doc = NULL;
    return (-1);
}

/** Port of xmlSchemaGetMetaAttrInfo. */
XmlXsd10AttrInfo *xml_xsd10_get_meta_attr_info(XmlXsd10ValidCtxt *vctxt, int metaType)
{
    if (vctxt->nbAttrInfos == 0)
	return (NULL);
    {
	int i;
	XmlXsd10AttrInfo *iattr;

	for (i = 0; i < vctxt->nbAttrInfos; i++) {
	    iattr = vctxt->attrInfos[i];
	    if (iattr->metaType == metaType)
		return (iattr);
	}

    }
    return (NULL);
}

/**
 * Port of xmlSchemaAssembleByXSI.
 *
 * Expands an existing schema by an additional schema using
 * the xsi:schemaLocation or xsi:noNamespaceSchemaLocation attribute
 * of an instance. If xsi:noNamespaceSchemaLocation is used, `noNamespace`
 * must be set to 1.
 *
 * @param vctxt  a schema validation context
 * @returns 0 if the new schema is correct, a positive error code
 * number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_assemble_by_xsi(XmlXsd10ValidCtxt *vctxt)
{
    const char *cur, *end;
    const char *nsname = NULL, *location;
    int ret = 0;
    XmlXsd10AttrInfo *iattr;

    /*
    * Parse the value; we will assume an even number of values
    * to be given (this is how Xerces and XSV work).
    *
    * URGENT TODO: !! This needs to work for both
    * @noNamespaceSchemaLocation AND @schemaLocation on the same
    * element !!
    */
    iattr = xml_xsd10_get_meta_attr_info(vctxt,
	XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC);
    if (iattr == NULL)
	iattr = xml_xsd10_get_meta_attr_info(vctxt,
	XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC);
    if (iattr == NULL)
	return (0);
    cur = iattr->value;
    do {
	/*
	* TODO: Move the string parsing mechanism away from here.
	*/
	if (iattr->metaType == XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC) {
	    /*
	    * Get the namespace name.
	    */
	    while (XSD10_VALID_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_VALID_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    /* TODO: Don't use the schema's dict. */
	    nsname = xml_xsd10_dict_lookup(vctxt->schema->dict, cur, end - cur);
	    cur = end;
	}
	/*
	* Get the URI.
	*/
	while (XSD10_VALID_IS_BLANK_CH(*cur))
	    cur++;
	end = cur;
	while ((*end != 0) && (!(XSD10_VALID_IS_BLANK_CH(*end))))
	    end++;
	if (end == cur) {
	    if (iattr->metaType ==
		XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC)
	    {
		/*
		* If using @schemaLocation then tuples are expected.
		* I.e. the namespace name *and* the document's URI.
		*/
		xml_xsd10_custom_warning(XSD10_ACTXT_CAST vctxt, XML_XSD10_SCHEMAV_MISC,
		    iattr->node, NULL,
		    "The value must consist of tuples: the target namespace "
		    "name and the document's URI", NULL, NULL, NULL);
	    }
	    break;
	}
	/* TODO: Don't use the schema's dict. */
	location = xml_xsd10_dict_lookup(vctxt->schema->dict, cur, end - cur);
	cur = end;
	ret = xml_xsd10_assemble_by_location(vctxt, vctxt->schema,
	    iattr->node, nsname, location);
	if (ret == -1) {
	    XSD10_VERROR_INT("xmlSchemaAssembleByXSI",
		"assembling schemata");
	    return (-1);
	}
    } while (*cur != 0);
    return (ret);
}

/** Port of xmlSchemaLookupNamespace. */
const char *xml_xsd10_lookup_namespace(XmlXsd10ValidCtxt *vctxt, const char *prefix)
{
    /* The SAX (nsBindings) and reader branches are not ported. */
    {
	XmlNs *ns;

	if ((vctxt->inode->node == NULL) ||
	    (xml_node_dom(vctxt->inode->node) == NULL)) {
	    XSD10_VERROR_INT("xmlSchemaLookupNamespace",
		"no node or node's doc available");
	    return (NULL);
	}
	ns = xml_xsd10_tree_search_ns(vctxt->inode->node, prefix);
	if (ns != NULL)
	    return (xml_ns_href(ns));
	return (NULL);
    }
}

/*
* This one works on the schema of the validation context.
*/
/** Port of xmlSchemaValidateNotation. */
int xml_xsd10_validate_notation(XmlXsd10ValidCtxt *vctxt, XmlXsd10Schema *schema, XMQNode *node, const char *value, XmlXsd10Val **val, int valNeeded)
{
    int ret;

    if (vctxt && (vctxt->schema == NULL)) {
	XSD10_VERROR_INT("xmlSchemaValidateNotation",
	    "a schema is needed on the validation context");
	return (-1);
    }
    ret = xml_xsd10_tree_validate_qname(value, 1);
    if (ret != 0)
	return (ret);
    {
	char *localName = NULL;
	char *prefix = NULL;

	localName = xml_xsd10_tree_split_qname2(value, &prefix);
	if (prefix != NULL) {
	    const char *nsName = NULL;

	    if (vctxt != NULL)
		nsName = xml_xsd10_lookup_namespace(vctxt, prefix);
	    else if (node != NULL) {
		XmlNs *ns = xml_xsd10_tree_search_ns(node, prefix);
		if (ns != NULL)
		    nsName = xml_ns_href(ns);
	    } else {
		free(prefix);
		free(localName);
		return (1);
	    }
	    if (nsName == NULL) {
		free(prefix);
		free(localName);
		return (1);
	    }
	    if (xml_xsd10_get_notation(schema, localName, nsName) != NULL) {
		if ((valNeeded) && (val != NULL)) {
		    (*val) = xml_xsd10_new_notation_value(xml_xsd10_strdup(localName),
						       xml_xsd10_strdup(nsName));
		    if (*val == NULL)
			ret = -1;
		}
	    } else
		ret = 1;
	    free(prefix);
	    free(localName);
	} else {
	    if (xml_xsd10_get_notation(schema, value, NULL) != NULL) {
		if (valNeeded && (val != NULL)) {
		    (*val) = xml_xsd10_new_notation_value(
			xml_xsd10_strdup(value), NULL);
		    if (*val == NULL)
			ret = -1;
		}
	    } else
		return (1);
	}
    }
    return (ret);
}

/** Port of xmlSchemaVAddNodeQName. */
int xml_xsd10_v_add_node_q_name(XmlXsd10ValidCtxt *vctxt, const char* lname, const char* nsname)
{
    int i;

    lname = xml_xsd10_dict_lookup(vctxt->dict, lname, -1);
    if (lname == NULL)
	return(-1);
    if (nsname != NULL) {
	nsname = xml_xsd10_dict_lookup(vctxt->dict, nsname, -1);
	if (nsname == NULL)
	    return(-1);
    }
    for (i = 0; i < vctxt->nodeQNames->nbItems; i += 2) {
	if ((vctxt->nodeQNames->items [i] == lname) &&
	    (vctxt->nodeQNames->items[i +1] == nsname))
	    /* Already there */
	    return(i);
    }
    /* Add new entry. */
    i = vctxt->nodeQNames->nbItems;
    if (xml_xsd10_item_list_add(vctxt->nodeQNames, (void *) lname) < 0) {
        xml_xsd10_v_err_memory(vctxt);
        return(-1);
    }
    if (xml_xsd10_item_list_add(vctxt->nodeQNames, (void *) nsname) < 0) {
        vctxt->nodeQNames->nbItems--;
        xml_xsd10_v_err_memory(vctxt);
        return(-1);
    }
    return(i);
}

/************************************************************************
 *									*
 *  Validation of identity-constraints (IDC)                            *
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaAugmentIDC.
 *
 * Creates an augmented IDC definition item.
 *
 * @param payload  the IDC definition
 * @param data  validation context
 * @param name  unused
 * @returns the item, or NULL on internal errors.
 */
void xml_xsd10_augment_idc(void *payload, void *data, const char *name)
{
    (void) name;
    XmlXsd10IDC *idcDef = (XmlXsd10IDC *) payload;
    XmlXsd10ValidCtxt *vctxt = (XmlXsd10ValidCtxt *) data;
    XmlXsd10IDCAug *aidc;

    aidc = (XmlXsd10IDCAug *) malloc(sizeof(XmlXsd10IDCAug));
    if (aidc == NULL) {
	xml_xsd10_v_err_memory(vctxt);
	return;
    }
    aidc->keyrefDepth = -1;
    aidc->def = idcDef;
    aidc->next = NULL;
    if (vctxt->aidcs == NULL)
	vctxt->aidcs = aidc;
    else {
	aidc->next = vctxt->aidcs;
	vctxt->aidcs = aidc;
    }
    /*
    * Save if we have keyrefs at all.
    */
    if ((vctxt->hasKeyrefs == 0) &&
	(idcDef->type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF))
	vctxt->hasKeyrefs = 1;
}

/**
 * Port of xmlSchemaAugmentImportedIDC.
 *
 * Creates an augmented IDC definition for the imported schema.
 *
 * @param payload  the imported schema
 * @param data  validation context
 * @param name  unused
 */
void xml_xsd10_augment_imported_idc(void *payload, void *data, const char *name)
{
    (void) name;
    XmlXsd10Import *imported = (XmlXsd10Import *) payload;
    XmlXsd10ValidCtxt *vctxt = (XmlXsd10ValidCtxt *) data;
    if (imported->schema->idcDef != NULL) {
	    xml_xsd10_hash_scan(imported->schema->idcDef, xml_xsd10_augment_idc, vctxt);
    }
}

/**
 * Port of xmlSchemaIDCNewBinding.
 *
 * Creates a new IDC binding.
 *
 * @param idcDef  the IDC definition of this binding
 * @returns the new IDC binding, NULL on internal errors.
 */
XmlXsd10PSVIIDCBinding *xml_xsd10_idc_new_binding(XmlXsd10IDC *idcDef)
{
    XmlXsd10PSVIIDCBinding *ret;

    ret = (XmlXsd10PSVIIDCBinding *) malloc(
	    sizeof(XmlXsd10PSVIIDCBinding));
    if (ret == NULL) {
	xml_xsd10_v_err_memory(NULL);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10PSVIIDCBinding));
    ret->definition = idcDef;
    return (ret);
}

/**
 * Port of xmlSchemaIDCStoreNodeTableItem.
 *
 * The validation context is used to store IDC node table items.
 * They are stored to avoid copying them if IDC node-tables are merged
 * with corresponding parent IDC node-tables (bubbling).
 *
 * @param vctxt  the WXS validation context
 * @param item  the IDC node table item
 * @returns 0 if succeeded, -1 on internal errors.
 */
int xml_xsd10_idc_store_node_table_item(XmlXsd10ValidCtxt *vctxt, XmlXsd10PSVIIDCNode *item)
{
    /*
    * Add to global list.
    */
    if (vctxt->sizeIdcNodes <= vctxt->nbIdcNodes) {
        XmlXsd10PSVIIDCNode **tmp;
        int newSize;

        newSize = xml_xsd10_grow_capacity(vctxt->sizeIdcNodes, sizeof(tmp[0]),
                                  20, XML_XSD10_MAX_ITEMS);
	if (newSize < 0) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (-1);
	}
	tmp = (XmlXsd10PSVIIDCNode **) realloc(vctxt->idcNodes, newSize * sizeof(tmp[0]));
	if (tmp == NULL) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (-1);
	}

        vctxt->idcNodes = tmp;
        vctxt->sizeIdcNodes = newSize;
    }

    vctxt->idcNodes[vctxt->nbIdcNodes++] = item;
    return (0);
}

/**
 * Port of xmlSchemaIDCStoreKey.
 *
 * The validation context is used to store an IDC key.
 *
 * @param vctxt  the WXS validation context
 * @param key  the IDC key
 * @returns 0 if succeeded, -1 on internal errors.
 */
int xml_xsd10_idc_store_key(XmlXsd10ValidCtxt *vctxt, XmlXsd10PSVIIDCKey *key)
{
    /*
    * Add to global list.
    */
    if (vctxt->sizeIdcKeys <= vctxt->nbIdcKeys) {
        XmlXsd10PSVIIDCKey **tmp;
        int newSize;

        newSize = xml_xsd10_grow_capacity(vctxt->sizeIdcKeys, sizeof(tmp[0]),
                                  20, XML_XSD10_MAX_ITEMS);
	if (newSize < 0) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (-1);
	}
	tmp = (XmlXsd10PSVIIDCKey **) realloc(vctxt->idcKeys, newSize * sizeof(tmp[0]));
	if (tmp == NULL) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (-1);
	}

        vctxt->idcKeys = tmp;
        vctxt->sizeIdcKeys = newSize;
    }

    vctxt->idcKeys[vctxt->nbIdcKeys++] = key;
    return (0);
}

/**
 * Port of xmlSchemaIDCAppendNodeTableItem.
 *
 * Appends the IDC node-table item to the binding.
 *
 * @param bind  the IDC binding
 * @param ntItem  the node-table item
 * @returns 0 on success and -1 on internal errors.
 */
int xml_xsd10_idc_append_node_table_item(XmlXsd10PSVIIDCBinding *bind, XmlXsd10PSVIIDCNode *ntItem)
{
    if (bind->sizeNodes <= bind->nbNodes) {
        XmlXsd10PSVIIDCNode **tmp;
        int newSize;

        newSize = xml_xsd10_grow_capacity(bind->sizeNodes, sizeof(tmp[0]),
                                  10, XML_XSD10_MAX_ITEMS);
	if (newSize < 0) {
	    xml_xsd10_v_err_memory(NULL);
	    return(-1);
	}
	tmp = (XmlXsd10PSVIIDCNode **) realloc(bind->nodeTable, newSize * sizeof(tmp[0]));
	if (tmp == NULL) {
	    xml_xsd10_v_err_memory(NULL);
	    return(-1);
	}

        bind->nodeTable = tmp;
        bind->sizeNodes = newSize;
    }

    bind->nodeTable[bind->nbNodes++] = ntItem;
    return(0);
}

/**
 * Port of xmlSchemaIDCAcquireBinding.
 *
 * Looks up an PSVI IDC binding, for the IDC definition and
 * of the given matcher. If none found, a new one is created
 * and added to the IDC table.
 *
 * @param vctxt  the WXS validation context
 * @param matcher  the IDC matcher
 * @returns an IDC binding or NULL on internal errors.
 */
XmlXsd10PSVIIDCBinding *xml_xsd10_idc_acquire_binding(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher)
{
    XmlXsd10NodeInfo *ielem;

    ielem = vctxt->elemInfos[matcher->depth];

    if (ielem->idcTable == NULL) {
	ielem->idcTable = xml_xsd10_idc_new_binding(matcher->aidc->def);
	if (ielem->idcTable == NULL)
	    return (NULL);
	return(ielem->idcTable);
    } else {
	XmlXsd10PSVIIDCBinding *bind = NULL;

	bind = ielem->idcTable;
	do {
	    if (bind->definition == matcher->aidc->def)
		return(bind);
	    if (bind->next == NULL) {
		bind->next = xml_xsd10_idc_new_binding(matcher->aidc->def);
		if (bind->next == NULL)
		    return (NULL);
		return(bind->next);
	    }
	    bind = bind->next;
	} while (bind != NULL);
    }
    return (NULL);
}

/** Port of xmlSchemaIDCAcquireTargetList. */
XmlXsd10ItemList *xml_xsd10_idc_acquire_target_list(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher)
{
    if (matcher->targets == NULL) {
	matcher->targets = xml_xsd10_item_list_create();
        if (matcher->targets == NULL)
            xml_xsd10_v_err_memory(vctxt);
    }
    return(matcher->targets);
}

/**
 * Port of xmlSchemaIDCFreeKey.
 *
 * Frees an IDC key together with its compiled value.
 *
 * @param key  the IDC key
 */
void xml_xsd10_idc_free_key(XmlXsd10PSVIIDCKey *key)
{
    if (key->val != NULL)
	xml_xsd10_free_value(key->val);
    free(key);
}

/**
 * Port of xmlSchemaIDCFreeBinding.
 *
 * Frees an IDC binding. Note that the node table-items
 * are not freed.
 */
void xml_xsd10_idc_free_binding(XmlXsd10PSVIIDCBinding *bind)
{
    if (bind->nodeTable != NULL)
	free(bind->nodeTable);
    if (bind->dupls != NULL)
	xml_xsd10_item_list_free(bind->dupls);
    free(bind);
}

/**
 * Port of xmlSchemaIDCFreeIDCTable.
 *
 * Frees an IDC table, i.e. all the IDC bindings in the list.
 *
 * @param bind  the first IDC binding in the list
 */
void xml_xsd10_idc_free_idc_table(XmlXsd10PSVIIDCBinding *bind)
{
    XmlXsd10PSVIIDCBinding *prev;

    while (bind != NULL) {
	prev = bind;
	bind = bind->next;
	xml_xsd10_idc_free_binding(prev);
    }
}

/** Port of xmlFreeIDCHashEntry. */
void xml_xsd10_free_idc_hash_entry(void *payload, const char *name)
{
    (void) name;
    XmlXsd10IDCHashEntry *e = (XmlXsd10IDCHashEntry *) payload, *n;
    while (e) {
	n = e->next;
	free(e);
	e = n;
    }
}

/**
 * Port of xmlSchemaIDCFreeMatcherList.
 *
 * Frees a list of IDC matchers.
 *
 * @param matcher  the first IDC matcher in the list
 */
void xml_xsd10_idc_free_matcher_list(XmlXsd10IDCMatcher *matcher)
{
    XmlXsd10IDCMatcher *next;

    while (matcher != NULL) {
	next = matcher->next;
	if (matcher->keySeqs != NULL) {
	    int i;
	    for (i = 0; i < matcher->sizeKeySeqs; i++)
		if (matcher->keySeqs[i] != NULL)
		    free(matcher->keySeqs[i]);
	    free(matcher->keySeqs);
	}
	if (matcher->targets != NULL) {
	    if (matcher->idcType == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
		int i;
		XmlXsd10PSVIIDCNode *idcNode;
		/*
		* Node-table items for keyrefs are not stored globally
		* to the validation context, since they are not bubbled.
		* We need to free them here.
		*/
		for (i = 0; i < matcher->targets->nbItems; i++) {
		    idcNode =
			(XmlXsd10PSVIIDCNode *) matcher->targets->items[i];
		    free(idcNode->keys);
		    free(idcNode);
		}
	    }
	    xml_xsd10_item_list_free(matcher->targets);
	}
	if (matcher->htab != NULL)
	  xml_xsd10_hash_free(matcher->htab, xml_xsd10_free_idc_hash_entry);
	free(matcher);
	matcher = next;
    }
}

/**
 * Port of xmlSchemaIDCReleaseMatcherList.
 *
 * Caches a list of IDC matchers for reuse.
 *
 * @param vctxt  the WXS validation context
 * @param matcher  the first IDC matcher in the list
 */
void xml_xsd10_idc_release_matcher_list(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher)
{
    XmlXsd10IDCMatcher *next;

    while (matcher != NULL) {
	next = matcher->next;
	if (matcher->keySeqs != NULL) {
	    int i;
	    /*
	    * Don't free the array, but only the content.
	    */
	    for (i = 0; i < matcher->sizeKeySeqs; i++)
		if (matcher->keySeqs[i] != NULL) {
		    free(matcher->keySeqs[i]);
		    matcher->keySeqs[i] = NULL;
		}
	}
	if (matcher->targets) {
	    if (matcher->idcType == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
		int i;
		XmlXsd10PSVIIDCNode *idcNode;
		/*
		* Node-table items for keyrefs are not stored globally
		* to the validation context, since they are not bubbled.
		* We need to free them here.
		*/
		for (i = 0; i < matcher->targets->nbItems; i++) {
		    idcNode =
			(XmlXsd10PSVIIDCNode *) matcher->targets->items[i];
		    free(idcNode->keys);
		    free(idcNode);
		}
	    }
	    xml_xsd10_item_list_free(matcher->targets);
	    matcher->targets = NULL;
	}
	if (matcher->htab != NULL) {
	    xml_xsd10_hash_free(matcher->htab, xml_xsd10_free_idc_hash_entry);
	    matcher->htab = NULL;
	}
	matcher->next = NULL;
	/*
	* Cache the matcher.
	*/
	if (vctxt->idcMatcherCache != NULL)
	    matcher->nextCached = vctxt->idcMatcherCache;
	vctxt->idcMatcherCache = matcher;

	matcher = next;
    }
}

/**
 * Port of xmlSchemaIDCAddStateObject.
 *
 * Creates/reuses and activates state objects for the given
 * XPath information; if the XPath expression consists of unions,
 * multiple state objects are created for every unioned expression.
 *
 * @param vctxt  the WXS validation context
 * @param matcher  the IDC matcher
 * @param sel  the XPath information
 * @param type  "selector" or "field"
 * @returns 0 on success and -1 on internal errors.
 */
int xml_xsd10_idc_add_state_object(XmlXsd10ValidCtxt *vctxt, XmlXsd10IDCMatcher *matcher, XmlXsd10IDCSelect *sel, int type)
{
    XmlXsd10IDCStateObj *sto;

    /*
    * Reuse the state objects from the pool.
    */
    if (vctxt->xpathStatePool != NULL) {
	sto = vctxt->xpathStatePool;
	vctxt->xpathStatePool = sto->next;
	sto->next = NULL;
    } else {
	/*
	* Create a new state object.
	*/
	sto = (XmlXsd10IDCStateObj *) malloc(sizeof(XmlXsd10IDCStateObj));
	if (sto == NULL) {
	    xml_xsd10_v_err_memory(NULL);
	    return (-1);
	}
	memset(sto, 0, sizeof(XmlXsd10IDCStateObj));
    }
    /*
    * Add to global list.
    */
    if (vctxt->xpathStates != NULL)
	sto->next = vctxt->xpathStates;
    vctxt->xpathStates = sto;

    /*
    * Free the old xpath validation context.
    */
    if (sto->xpathCtxt != NULL)
	xml_stream_ctxt_free((XmlStreamCtxt *) sto->xpathCtxt);

    /*
    * Create a new XPath (pattern) validation context.
    */
    sto->xpathCtxt = (void *) xml_pattern_get_stream_ctxt(
	(XmlPattern *) sel->xpathComp);
    if (sto->xpathCtxt == NULL) {
	XSD10_VERROR_INT("xmlSchemaIDCAddStateObject",
	    "failed to create an XPath validation context");
	return (-1);
    }
    sto->type = type;
    sto->depth = vctxt->depth;
    sto->matcher = matcher;
    sto->sel = sel;
    sto->nbHistory = 0;

    return (0);
}

/**
 * Port of xmlSchemaXPathEvaluate.
 *
 * Evaluates all active XPath state objects.
 *
 * @param vctxt  the WXS validation context
 * @param nodeType  the nodeType of the current node
 * @returns the number of IC "field" state objects which resolved to
 * this node, 0 if none resolved and -1 on internal errors.
 */
int xml_xsd10_x_path_evaluate(XmlXsd10ValidCtxt *vctxt, XmlNodeKind nodeType)
{
    XmlXsd10IDCStateObj *sto, *head = NULL, *first;
    int res, resolved = 0, depth = vctxt->depth;

    if (vctxt->xpathStates == NULL)
	return (0);

    if (nodeType == XML_DOM_ATTRIBUTE)
	depth++;
    /*
    * Process all active XPath state objects.
    */
    first = vctxt->xpathStates;
    sto = first;
    while (sto != head) {
	if (nodeType == XML_DOM_ELEMENT)
	    res = xml_stream_push((XmlStreamCtxt *) sto->xpathCtxt,
		vctxt->inode->localName, vctxt->inode->nsName);
	else
	    res = xml_stream_push_attr((XmlStreamCtxt *) sto->xpathCtxt,
		vctxt->inode->localName, vctxt->inode->nsName);

	if (res == -1) {
	    XSD10_VERROR_INT("xmlSchemaXPathEvaluate",
		"calling xmlStreamPush()");
	    return (-1);
	}
	if (res == 0)
	    goto next_sto;
	/*
	* Full match.
	*/
	/*
	* Register a match in the state object history.
	*/
        if (sto->sizeHistory <= sto->nbHistory) {
            int *tmp;
            int newSize;

            newSize = xml_xsd10_grow_capacity(sto->sizeHistory, sizeof(tmp[0]),
                                      5, XML_XSD10_MAX_ITEMS);
            if (newSize < 0) {
                xml_xsd10_v_err_memory(vctxt);
                return (-1);
            }
            tmp = (int *) realloc(sto->history, newSize * sizeof(tmp[0]));
            if (tmp == NULL) {
                xml_xsd10_v_err_memory(vctxt);
                return (-1);
            }

            sto->history = tmp;
            sto->sizeHistory = newSize;
	}
	sto->history[sto->nbHistory++] = depth;

	if (sto->type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR) {
	    XmlXsd10IDCSelect *sel;
	    /*
	    * Activate state objects for the IDC fields of
	    * the IDC selector.
	    */
	    sel = sto->matcher->aidc->def->fields;
	    while (sel != NULL) {
		if (xml_xsd10_idc_add_state_object(vctxt, sto->matcher,
		    sel, XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD) == -1)
		    return (-1);
		sel = sel->next;
	    }
	} else if (sto->type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD) {
	    /*
	    * An IDC key node was found by the IDC field.
	    */
	    /*
	    * Notify that the character value of this node is
	    * needed.
	    */
	    if (resolved == 0) {
		if ((vctxt->inode->flags &
		    XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED) == 0)
		vctxt->inode->flags |= XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED;
	    }
	    resolved++;
	}
next_sto:
	if (sto->next == NULL) {
	    /*
	    * Evaluate field state objects created on this node as well.
	    */
	    head = first;
	    sto = vctxt->xpathStates;
	} else
	    sto = sto->next;
    }
    return (resolved);
}

/** Port of xmlSchemaFormatIDCKeySequence_1. */
const char *xml_xsd10_format_idc_key_sequence_1(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count, int for_hash)
{
    int i, res;
    char *value = NULL;

    *buf = xml_xsd10_strdup("[");
    for (i = 0; i < count; i++) {
	*buf = xml_xsd10_strcat(*buf, "'");
	if (!for_hash)
	    res = xml_xsd10_get_canon_value_whtsp_ext(seq[i]->val,
		    xml_xsd10_get_white_space_facet_value(seq[i]->type),
		    &value);
	else {
	    res = xml_xsd10_get_canon_value_hash(seq[i]->val, &value);
	}
	if (res == 0)
	    *buf = xml_xsd10_strcat(*buf, value);
	else {
	    XSD10_VERROR_INT("xmlSchemaFormatIDCKeySequence",
		"failed to compute a canonical value");
	    *buf = xml_xsd10_strcat(*buf, "???");
	}
	if (i < count -1)
	    *buf = xml_xsd10_strcat(*buf, "', ");
	else
	    *buf = xml_xsd10_strcat(*buf, "'");
	if (value != NULL) {
	    free(value);
	    value = NULL;
	}
    }
    *buf = xml_xsd10_strcat(*buf, "]");

    return (*buf);
}

/** Port of xmlSchemaFormatIDCKeySequence. */
const char *xml_xsd10_format_idc_key_sequence(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count)
{
    return xml_xsd10_format_idc_key_sequence_1(vctxt, buf, seq, count, 0);
}

/** Port of xmlSchemaHashKeySequence. */
const char *xml_xsd10_hash_key_sequence(XmlXsd10ValidCtxt *vctxt, char **buf, XmlXsd10PSVIIDCKey **seq, int count)
{
    return xml_xsd10_format_idc_key_sequence_1(vctxt, buf, seq, count, 1);
}

/**
 * Port of xmlSchemaXPathPop.
 *
 * Pops all XPath states.
 *
 * @param vctxt  the WXS validation context
 * @returns 0 on success and -1 on internal errors.
 */
int xml_xsd10_x_path_pop(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10IDCStateObj *sto;
    int res;

    if (vctxt->xpathStates == NULL)
	return(0);
    sto = vctxt->xpathStates;
    do {
	res = xml_stream_pop((XmlStreamCtxt *) sto->xpathCtxt);
	if (res == -1)
	    return (-1);
	sto = sto->next;
    } while (sto != NULL);
    return(0);
}

/**
 * Port of xmlSchemaXPathProcessHistory.
 *
 * Processes and pops the history items of the IDC state objects.
 * IDC key-sequences are validated/created on IDC bindings.
 *
 * @param vctxt  the WXS validation context
 * @param depth  depth
 * @returns 0 on success and -1 on internal errors.
 */
int xml_xsd10_x_path_process_history(XmlXsd10ValidCtxt *vctxt, int depth)
{
    XmlXsd10IDCStateObj *sto, *nextsto;
    int res, matchDepth;
    XmlXsd10PSVIIDCKey *key = NULL;
    XmlXsd10Type *type = vctxt->inode->typeDef, *simpleType = NULL;

    if (vctxt->xpathStates == NULL)
	return (0);
    sto = vctxt->xpathStates;

    /*
    * Evaluate the state objects.
    */
    while (sto != NULL) {
	res = xml_stream_pop((XmlStreamCtxt *) sto->xpathCtxt);
	if (res == -1) {
	    XSD10_VERROR_INT("xmlSchemaXPathProcessHistory",
		"calling xmlStreamPop()");
	    return (-1);
	}
	if (sto->nbHistory == 0)
	    goto deregister_check;

	matchDepth = sto->history[sto->nbHistory -1];

	/*
	* Only matches at the current depth are of interest.
	*/
	if (matchDepth != depth) {
	    sto = sto->next;
	    continue;
	}
	if (sto->type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_FIELD) {
	    /*
	    * NOTE: According to
	    *   http://www.w3.org/Bugs/Public/show_bug.cgi?id=2198
	    *   ... the simple-content of complex types is also allowed.
	    */

	    if (XSD10_WXS_IS_COMPLEX(type)) {
		if (XSD10_WXS_HAS_SIMPLE_CONTENT(type)) {
		    /*
		    * Sanity check for complex types with simple content.
		    */
		    simpleType = type->contentTypeDef;
		    if (simpleType == NULL) {
			XSD10_VERROR_INT("xmlSchemaXPathProcessHistory",
			    "field resolves to a CT with simple content "
			    "but the CT is missing the ST definition");
			return (-1);
		    }
		} else
		    simpleType = NULL;
	    } else
		simpleType = type;
	    if (simpleType == NULL) {
		char *str = NULL;

		/*
		* Not qualified if the field resolves to a node of non
		* simple type.
		*/
		xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_CVC_IDC, NULL,
		    XSD10_WXS_BASIC_CAST sto->matcher->aidc->def,
		    "The XPath '%s' of a field of %s does evaluate to a node of "
		    "non-simple type",
		    sto->sel->xpath,
		    xml_xsd10_get_idc_designation(&str, sto->matcher->aidc->def));
		XSD10_FREE_AND_NULL(str);
		sto->nbHistory--;
		goto deregister_check;
	    }

	    if ((key == NULL) && (vctxt->inode->val == NULL)) {
		/*
		* Failed to provide the normalized value; maybe
		* the value was invalid.
		*/
		XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_IDC,
		    XSD10_WXS_BASIC_CAST sto->matcher->aidc->def,
		    "Warning: No precomputed value available, the value "
		    "was either invalid or something strange happened");
		sto->nbHistory--;
		goto deregister_check;
	    } else {
		XmlXsd10IDCMatcher *matcher = sto->matcher;
		XmlXsd10PSVIIDCKey **keySeq;
		int pos, idx;

		/*
		* The key will be anchored on the matcher's list of
		* key-sequences. The position in this list is determined
		* by the target node's depth relative to the matcher's
		* depth of creation (i.e. the depth of the scope element).
		*
		* Element        Depth    Pos   List-entries
		* <scope>          0              NULL
		*   <bar>          1              NULL
		*     <target/>    2       2      target
		*   <bar>
                * </scope>
		*
		* The size of the list is only dependent on the depth of
		* the tree.
		* An entry will be NULLed in selector_leave, i.e. when
		* we hit the target's
		*/
		pos = sto->depth - matcher->depth;
		idx = sto->sel->index;

		/*
		* Create/grow the array of key-sequences.
		*/
                if (pos >= matcher->sizeKeySeqs) {
                    XmlXsd10PSVIIDCKey ***tmp;
                    int oldSize = matcher->sizeKeySeqs;
                    int newSize, i;

                    newSize = xml_xsd10_grow_capacity(pos, sizeof(tmp[0]),
                                              10, XML_XSD10_MAX_ITEMS);
                    if (newSize < 0) {
                        xml_xsd10_v_err_memory(vctxt);
                        return (-1);
                    }
                    tmp = (XmlXsd10PSVIIDCKey ***) realloc(matcher->keySeqs,
                                     newSize * sizeof(tmp[0]));
                    if (tmp == NULL) {
                        xml_xsd10_v_err_memory(vctxt);
                        return (-1);
                    }

                    matcher->keySeqs = tmp;
                    matcher->sizeKeySeqs = newSize;

		    /*
		    * The array needs to be NULLed.
		    * TODO: Use memset?
		    */
		    for (i = oldSize; i < newSize; i++)
			matcher->keySeqs[i] = NULL;
		}

		/*
		* Get/create the key-sequence.
		*/
		keySeq = matcher->keySeqs[pos];
		if (keySeq == NULL) {
		    goto create_sequence;
		} else if (keySeq[idx] != NULL) {
		    char *str = NULL;
		    /*
		    * cvc-identity-constraint:
		    * 3 For each node in the `target node set` all
		    * of the {fields}, with that node as the context
		    * node, evaluate to either an empty node-set or
		    * a node-set with exactly one member, which must
		    * have a simple type.
		    *
		    * The key was already set; report an error.
		    */
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
			XML_XSD10_SCHEMAV_CVC_IDC, NULL,
			XSD10_WXS_BASIC_CAST matcher->aidc->def,
			"The XPath '%s' of a field of %s evaluates to a "
			"node-set with more than one member",
			sto->sel->xpath,
			xml_xsd10_get_idc_designation(&str, matcher->aidc->def));
		    XSD10_FREE_AND_NULL(str);
		    sto->nbHistory--;
		    goto deregister_check;
		} else
		    goto create_key;

create_sequence:
		/*
		* Create a key-sequence.
		*/
		keySeq = (XmlXsd10PSVIIDCKey **) malloc(
		    matcher->aidc->def->nbFields *
		    sizeof(XmlXsd10PSVIIDCKey *));
		if (keySeq == NULL) {
		    xml_xsd10_v_err_memory(NULL);
		    return(-1);
		}
		memset(keySeq, 0, matcher->aidc->def->nbFields *
		    sizeof(XmlXsd10PSVIIDCKey *));
		matcher->keySeqs[pos] = keySeq;
create_key:
		/*
		* Create a key once per node only.
		*/
		if (key == NULL) {
		    key = (XmlXsd10PSVIIDCKey *) malloc(
			sizeof(XmlXsd10PSVIIDCKey));
		    if (key == NULL) {
			xml_xsd10_v_err_memory(NULL);
			free(keySeq);
			matcher->keySeqs[pos] = NULL;
			return(-1);
		    }
		    /*
		    * Consume the compiled value.
		    */
		    key->type = simpleType;
		    key->val = vctxt->inode->val;
		    vctxt->inode->val = NULL;
		    /*
		    * Store the key in a global list.
		    */
		    if (xml_xsd10_idc_store_key(vctxt, key) == -1) {
			xml_xsd10_idc_free_key(key);
			return (-1);
		    }
		}
		keySeq[idx] = key;
	    }
	} else if (sto->type == XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR) {

	    XmlXsd10PSVIIDCKey ***keySeq = NULL;
	    /* xmlSchemaPSVIIDCBindingPtr bind; */
	    XmlXsd10PSVIIDCNode *ntItem;
	    XmlXsd10IDCMatcher *matcher;
	    XmlXsd10IDC *idc;
	    XmlXsd10ItemList *targets;
	    int pos, i, j, nbKeys;
	    /*
	    * Here we have the following scenario:
	    * An IDC 'selector' state object resolved to a target node,
	    * during the time this target node was in the
	    * ancestor-or-self axis, the 'field' state object(s) looked
	    * out for matching nodes to create a key-sequence for this
	    * target node. Now we are back to this target node and need
	    * to put the key-sequence, together with the target node
	    * itself, into the node-table of the corresponding IDC
	    * binding.
	    */
	    matcher = sto->matcher;
	    idc = matcher->aidc->def;
	    nbKeys = idc->nbFields;
	    pos = depth - matcher->depth;
	    /*
	    * Check if the matcher has any key-sequences at all, plus
	    * if it has a key-sequence for the current target node.
	    */
	    if ((matcher->keySeqs == NULL) ||
		(matcher->sizeKeySeqs <= pos)) {
		if (idc->type == XML_XSD10_SCHEMA_TYPE_IDC_KEY)
		    goto selector_key_error;
		else
		    goto selector_leave;
	    }

	    keySeq = &(matcher->keySeqs[pos]);
	    if (*keySeq == NULL) {
		if (idc->type == XML_XSD10_SCHEMA_TYPE_IDC_KEY)
		    goto selector_key_error;
		else
		    goto selector_leave;
	    }

	    for (i = 0; i < nbKeys; i++) {
		if ((*keySeq)[i] == NULL) {
		    /*
		    * Not qualified, if not all fields did resolve.
		    */
		    if (idc->type == XML_XSD10_SCHEMA_TYPE_IDC_KEY) {
			/*
			* All fields of a "key" IDC must resolve.
			*/
			goto selector_key_error;
		    }
		    goto selector_leave;
		}
	    }
	    /*
	    * All fields did resolve.
	    */

	    /*
	    * 4.1 If the {identity-constraint category} is unique(/key),
	    * then no two members of the `qualified node set` have
	    * `key-sequences` whose members are pairwise equal, as
	    * defined by Equal in [XML Schemas: Datatypes].
	    *
	    * Get the IDC binding from the matcher and check for
	    * duplicate key-sequences.
	    */
#if 0
	    bind = xml_xsd10_idc_acquire_binding(vctxt, matcher);
#endif
	    targets = xml_xsd10_idc_acquire_target_list(vctxt, matcher);
            if (targets == NULL)
                return(-1);

	    if ((idc->type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) &&
		(targets->nbItems != 0)) {
		XmlXsd10PSVIIDCKey *ckey, *bkey, **bkeySeq;
		XmlXsd10IDCHashEntry *e;

		res = 0;

		if (!matcher->htab)
		    e = NULL;
		else {
		    char *value = NULL;
		    xml_xsd10_hash_key_sequence(vctxt, &value, *keySeq, nbKeys);
		    e = (XmlXsd10IDCHashEntry *) xml_xsd10_hash_lookup(matcher->htab, value);
		    XSD10_FREE_AND_NULL(value);
		}

		/*
		* Compare the key-sequences, key by key.
		*/
		for (;e; e = e->next) {
		    bkeySeq =
			((XmlXsd10PSVIIDCNode *) targets->items[e->index])->keys;
		    for (j = 0; j < nbKeys; j++) {
			ckey = (*keySeq)[j];
			bkey = bkeySeq[j];
			res = xml_xsd10_are_values_equal(ckey->val, bkey->val);
			if (res == -1) {
			    return (-1);
			} else if (res == 0) {
			    /*
			    * One of the keys differs, so the key-sequence
			    * won't be equal; get out.
			    */
			    break;
			}
		    }
		    if (res == 1) {
			/*
			* Duplicate key-sequence found.
			*/
			break;
		    }
		}
		if (e) {
		    char *str = NULL, *strB = NULL;
		    /*
		    * TODO: Try to report the key-sequence.
		    */
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
			XML_XSD10_SCHEMAV_CVC_IDC, NULL,
			XSD10_WXS_BASIC_CAST idc,
			"Duplicate key-sequence %s in %s",
			xml_xsd10_format_idc_key_sequence(vctxt, &str,
			    (*keySeq), nbKeys),
			xml_xsd10_get_idc_designation(&strB, idc));
		    XSD10_FREE_AND_NULL(str);
		    XSD10_FREE_AND_NULL(strB);
		    goto selector_leave;
		}
	    }
	    /*
	    * Add a node-table item to the IDC binding.
	    */
	    ntItem = (XmlXsd10PSVIIDCNode *) malloc(
		sizeof(XmlXsd10PSVIIDCNode));
	    if (ntItem == NULL) {
		xml_xsd10_v_err_memory(NULL);
		free(*keySeq);
		*keySeq = NULL;
		return(-1);
	    }
	    memset(ntItem, 0, sizeof(XmlXsd10PSVIIDCNode));

	    /*
	    * Store the node-table item in a global list.
	    */
	    if (idc->type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
		if (xml_xsd10_idc_store_node_table_item(vctxt, ntItem) == -1) {
		    free(ntItem);
		    free(*keySeq);
		    *keySeq = NULL;
		    return (-1);
		}
		ntItem->nodeQNameID = -1;
	    } else {
		/*
		* Save a cached QName for this node on the IDC node, to be
		* able to report it, even if the node is not saved.
		*/
		ntItem->nodeQNameID = xml_xsd10_v_add_node_q_name(vctxt,
		    vctxt->inode->localName, vctxt->inode->nsName);
		if (ntItem->nodeQNameID == -1) {
		    free(ntItem);
		    free(*keySeq);
		    *keySeq = NULL;
		    return (-1);
		}
	    }
	    /*
	    * Init the node-table item: Save the node, position and
	    * consume the key-sequence.
	    */
	    ntItem->node = vctxt->node;
	    ntItem->nodeLine = vctxt->inode->nodeLine;
	    ntItem->keys = *keySeq;
	    *keySeq = NULL;
#if 0
	    if (xml_xsd10_idc_append_node_table_item(bind, ntItem) == -1)
#endif
	    if (xml_xsd10_item_list_add(targets, ntItem) == -1) {
		if (idc->type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
		    /*
		    * Free the item, since keyref items won't be
		    * put on a global list.
		    */
		    free(ntItem->keys);
		    free(ntItem);
		}
		return (-1);
	    }
	    if (idc->type != XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
		char *value = NULL;
		XmlXsd10IDCHashEntry *r, *e;
		if (!matcher->htab)
		  matcher->htab = xml_xsd10_hash_create(4);
		xml_xsd10_hash_key_sequence(vctxt, &value, ntItem->keys, nbKeys);
		e = (XmlXsd10IDCHashEntry *) malloc(sizeof *e);
                if (e == NULL) {
                    xml_xsd10_v_err_memory(vctxt);
                    goto mem_error;
                }
		e->index = targets->nbItems - 1;
		r = (XmlXsd10IDCHashEntry *) xml_xsd10_hash_lookup(matcher->htab, value);
		if (r) {
		    e->next = r->next;
		    r->next = e;
		} else {
		    e->next = NULL;
		    if (xml_xsd10_hash_add_entry(matcher->htab, value, e) < 0) {
                        xml_xsd10_v_err_memory(vctxt);
                        free(e);
                    }
		}
mem_error:
		XSD10_FREE_AND_NULL(value);
	    }

	    goto selector_leave;
selector_key_error:
	    {
		char *str = NULL;
		/*
		* 4.2.1 (KEY) The `target node set` and the
		* `qualified node set` are equal, that is, every
		* member of the `target node set` is also a member
		* of the `qualified node set` and vice versa.
		*/
		xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_CVC_IDC, NULL,
		    XSD10_WXS_BASIC_CAST idc,
		    "Not all fields of %s evaluate to a node",
		    xml_xsd10_get_idc_designation(&str, idc), NULL);
		XSD10_FREE_AND_NULL(str);
	    }
selector_leave:
	    /*
	    * Free the key-sequence if not added to the IDC table.
	    */
	    if ((keySeq != NULL) && (*keySeq != NULL)) {
		free(*keySeq);
		*keySeq = NULL;
	    }
	} /* if selector */

	sto->nbHistory--;

deregister_check:
	/*
	* Deregister state objects if they reach the depth of creation.
	*/
	if ((sto->nbHistory == 0) && (sto->depth == depth)) {
	    if (vctxt->xpathStates != sto) {
		XSD10_VERROR_INT("xmlSchemaXPathProcessHistory",
		    "The state object to be removed is not the first "
		    "in the list");
	    }
	    nextsto = sto->next;
	    /*
	    * Unlink from the list of active XPath state objects.
	    */
	    vctxt->xpathStates = sto->next;
	    sto->next = vctxt->xpathStatePool;
	    /*
	    * Link it to the pool of reusable state objects.
	    */
	    vctxt->xpathStatePool = sto;
	    sto = nextsto;
	} else
	    sto = sto->next;
    } /* while (sto != NULL) */
    return (0);
}

/**
 * Port of xmlSchemaIDCRegisterMatchers.
 *
 * Creates helper objects to evaluate IDC selectors/fields
 * successively.
 *
 * @param vctxt  the WXS validation context
 * @param elemDecl  the element declaration
 * @returns 0 if OK and -1 on internal errors.
 */
int xml_xsd10_idc_register_matchers(XmlXsd10ValidCtxt *vctxt, XmlXsd10Element *elemDecl)
{
    XmlXsd10IDCMatcher *matcher, *last = NULL;
    XmlXsd10IDC *idc, *refIdc;
    XmlXsd10IDCAug *aidc;

    idc = (XmlXsd10IDC *) elemDecl->idcs;
    if (idc == NULL)
	return (0);

    if (vctxt->inode->idcMatchers != NULL) {
	XSD10_VERROR_INT("xmlSchemaIDCRegisterMatchers",
	    "The chain of IDC matchers is expected to be empty");
	return (-1);
    }
    do {
	if (idc->type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) {
	    /*
	    * Since IDCs bubbles are expensive we need to know the
	    * depth at which the bubbles should stop; this will be
	    * the depth of the top-most keyref IDC. If no keyref
	    * references a key/unique IDC, the keyrefDepth will
	    * be -1, indicating that no bubbles are needed.
	    */
	    refIdc = (XmlXsd10IDC *) idc->ref->item;
	    if (refIdc != NULL) {
		/*
		* Remember that we have keyrefs on this node.
		*/
		vctxt->inode->hasKeyrefs = 1;
		/*
		* Lookup the referenced augmented IDC info.
		*/
		aidc = vctxt->aidcs;
		while (aidc != NULL) {
		    if (aidc->def == refIdc)
			break;
		    aidc = aidc->next;
		}
		if (aidc == NULL) {
		    XSD10_VERROR_INT("xmlSchemaIDCRegisterMatchers",
			"Could not find an augmented IDC item for an IDC "
			"definition");
		    return (-1);
		}
		if ((aidc->keyrefDepth == -1) ||
		    (vctxt->depth < aidc->keyrefDepth))
		    aidc->keyrefDepth = vctxt->depth;
	    }
	}
	/*
	* Lookup the augmented IDC item for the IDC definition.
	*/
	aidc = vctxt->aidcs;
	while (aidc != NULL) {
	    if (aidc->def == idc)
		break;
	    aidc = aidc->next;
	}
	if (aidc == NULL) {
	    XSD10_VERROR_INT("xmlSchemaIDCRegisterMatchers",
		"Could not find an augmented IDC item for an IDC definition");
	    return (-1);
	}
	/*
	* Create an IDC matcher for every IDC definition.
	*/
	if (vctxt->idcMatcherCache != NULL) {
	    /*
	    * Reuse a cached matcher.
	    */
	    matcher = vctxt->idcMatcherCache;
	    vctxt->idcMatcherCache = matcher->nextCached;
	    matcher->nextCached = NULL;
	} else {
	    matcher = (XmlXsd10IDCMatcher *)
		malloc(sizeof(XmlXsd10IDCMatcher));
	    if (matcher == NULL) {
		xml_xsd10_v_err_memory(vctxt);
		return (-1);
	    }
	    memset(matcher, 0, sizeof(XmlXsd10IDCMatcher));
	}
	if (last == NULL)
	    vctxt->inode->idcMatchers = matcher;
	else
	    last->next = matcher;
	last = matcher;

	matcher->type = XSD10_IDC_MATCHER;
	matcher->depth = vctxt->depth;
	matcher->aidc = aidc;
	matcher->idcType = aidc->def->type;
	/*
	* Init the automaton state object.
	*/
	if (xml_xsd10_idc_add_state_object(vctxt, matcher,
	    idc->selector, XSD10_XPATH_STATE_OBJ_TYPE_IDC_SELECTOR) == -1)
	    return (-1);

	idc = idc->next;
    } while (idc != NULL);
    return (0);
}

/** Port of xmlSchemaIDCFillNodeTables. */
int xml_xsd10_idc_fill_node_tables(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *ielem)
{
    XmlXsd10PSVIIDCBinding *bind;
    int res, i, j, k, nbTargets, nbFields, nbDupls, nbNodeTable;
    XmlXsd10PSVIIDCKey **keys, **ntkeys;
    XmlXsd10PSVIIDCNode **targets, **dupls;

    XmlXsd10IDCMatcher *matcher = ielem->idcMatchers;
    /* vctxt->createIDCNodeTables */
    while (matcher != NULL) {
	/*
	* Skip keyref IDCs and empty IDC target-lists.
	*/
	if ((matcher->aidc->def->type == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) ||
	    XSD10_WXS_ILIST_IS_EMPTY(matcher->targets))
	{
	    matcher = matcher->next;
	    continue;
	}
	/*
	* If we _want_ the IDC node-table to be created in any case
	* then do so. Otherwise create them only if keyrefs need them.
	*/
	if ((! vctxt->createIDCNodeTables) &&
	    ((matcher->aidc->keyrefDepth == -1) ||
	     (matcher->aidc->keyrefDepth > vctxt->depth)))
	{
	    matcher = matcher->next;
	    continue;
	}
	/*
	* Get/create the IDC binding on this element for the IDC definition.
	*/
	bind = xml_xsd10_idc_acquire_binding(vctxt, matcher);
	if (bind == NULL)
	   goto internal_error;

	if (! XSD10_WXS_ILIST_IS_EMPTY(bind->dupls)) {
	    dupls = (XmlXsd10PSVIIDCNode **) bind->dupls->items;
	    nbDupls = bind->dupls->nbItems;
	} else {
	    dupls = NULL;
	    nbDupls = 0;
	}
	if (bind->nodeTable != NULL) {
	    nbNodeTable = bind->nbNodes;
	} else {
	    nbNodeTable = 0;
	}

	if ((nbNodeTable == 0) && (nbDupls == 0)) {
	    /*
	    * Transfer all IDC target-nodes to the IDC node-table.
	    */
            if (bind->nodeTable != NULL)
                free(bind->nodeTable);
	    bind->nodeTable =
		(XmlXsd10PSVIIDCNode **) matcher->targets->items;
	    bind->sizeNodes = matcher->targets->sizeItems;
	    bind->nbNodes = matcher->targets->nbItems;

	    matcher->targets->items = NULL;
	    matcher->targets->sizeItems = 0;
	    matcher->targets->nbItems = 0;
	    if (matcher->htab) {
		xml_xsd10_hash_free(matcher->htab, xml_xsd10_free_idc_hash_entry);
		matcher->htab = NULL;
	    }
	} else {
	    /*
	    * Compare the key-sequences and add to the IDC node-table.
	    */
	    nbTargets = matcher->targets->nbItems;
	    targets = (XmlXsd10PSVIIDCNode **) matcher->targets->items;
	    nbFields = matcher->aidc->def->nbFields;
	    i = 0;
	    do {
		keys = targets[i]->keys;
		if (nbDupls) {
		    /*
		    * Search in already found duplicates first.
		    */
		    j = 0;
		    do {
			if (nbFields == 1) {
			    res = xml_xsd10_are_values_equal(keys[0]->val,
				dupls[j]->keys[0]->val);
			    if (res == -1)
				goto internal_error;
			    if (res == 1) {
				/*
				* Equal key-sequence.
				*/
				goto next_target;
			    }
			} else {
			    res = 0;
			    ntkeys = dupls[j]->keys;
			    for (k = 0; k < nbFields; k++) {
				res = xml_xsd10_are_values_equal(keys[k]->val,
				    ntkeys[k]->val);
				if (res == -1)
				    goto internal_error;
				if (res == 0) {
				    /*
				    * One of the keys differs.
				    */
				    break;
				}
			    }
			    if (res == 1) {
				/*
				* Equal key-sequence found.
				*/
				goto next_target;
			    }
			}
			j++;
		    } while (j < nbDupls);
		}
		if (bind->nbNodes) {
		    j = 0;
		    do {
			if (nbFields == 1) {
			    res = xml_xsd10_are_values_equal(keys[0]->val,
				bind->nodeTable[j]->keys[0]->val);
			    if (res == -1)
				goto internal_error;
			    if (res == 0) {
				/*
				* The key-sequence differs.
				*/
				goto next_node_table_entry;
			    }
			} else {
			    res = 0;
			    ntkeys = bind->nodeTable[j]->keys;
			    for (k = 0; k < nbFields; k++) {
				res = xml_xsd10_are_values_equal(keys[k]->val,
				    ntkeys[k]->val);
				if (res == -1)
				    goto internal_error;
				if (res == 0) {
				    /*
				    * One of the keys differs.
				    */
				    goto next_node_table_entry;
				}
			    }
			}
			/*
			* Add the duplicate to the list of duplicates.
			*/
			if (bind->dupls == NULL) {
			    bind->dupls = xml_xsd10_item_list_create();
			    if (bind->dupls == NULL)
				goto internal_error;
			}
			if (xml_xsd10_item_list_add(bind->dupls, bind->nodeTable[j]) == -1)
			    goto internal_error;
                        dupls = (XmlXsd10PSVIIDCNode **) bind->dupls->items;
			/*
			* Remove the duplicate entry from the IDC node-table.
			*/
			bind->nodeTable[j] = bind->nodeTable[bind->nbNodes -1];
			bind->nbNodes--;

			goto next_target;

next_node_table_entry:
			j++;
		    } while (j < bind->nbNodes);
		}
		/*
		* If everything is fine, then add the IDC target-node to
		* the IDC node-table.
		*/
		if (xml_xsd10_idc_append_node_table_item(bind, targets[i]) == -1)
		    goto internal_error;

next_target:
		i++;
	    } while (i < nbTargets);
	}
	matcher = matcher->next;
    }
    return(0);

internal_error:
    return(-1);
}

/**
 * Port of xmlSchemaBubbleIDCNodeTables.
 *
 * Merges IDC bindings of an element at `depth` into the corresponding IDC
 * bindings of its parent element. If a duplicate note-table entry is found,
 * both, the parent node-table entry and child entry are discarded from the
 * node-table of the parent.
 *
 * @param vctxt  the WXS validation context
 * @returns 0 if OK and -1 on internal errors.
 */
int xml_xsd10_bubble_idc_node_tables(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10PSVIIDCBinding *bind; /* IDC bindings of the current node. */
    XmlXsd10PSVIIDCBinding **parTable, *parBind = NULL; /* parent IDC bindings. */
    XmlXsd10PSVIIDCNode *node, *parNode = NULL, **dupls, **parNodes; /* node-table entries. */
    XmlXsd10IDCAug *aidc;
    int i, j, k, ret = 0, nbFields, oldNum, oldDupls;

    bind = vctxt->inode->idcTable;
    if (bind == NULL) {
	/* Fine, no table, no bubbles. */
	return (0);
    }

    parTable = &(vctxt->elemInfos[vctxt->depth -1]->idcTable);
    /*
    * Walk all bindings; create new or add to existing bindings.
    * Remove duplicate key-sequences.
    */
    while (bind != NULL) {

	if ((bind->nbNodes == 0) && XSD10_WXS_ILIST_IS_EMPTY(bind->dupls))
	    goto next_binding;
	/*
	* Check if the key/unique IDC table needs to be bubbled.
	*/
	if (! vctxt->createIDCNodeTables) {
	    aidc = vctxt->aidcs;
	    do {
		if (aidc->def == bind->definition) {
		    if ((aidc->keyrefDepth == -1) ||
			(aidc->keyrefDepth >= vctxt->depth)) {
			goto next_binding;
		    }
		    break;
		}
		aidc = aidc->next;
	    } while (aidc != NULL);
	}

	if (parTable != NULL)
	    parBind = *parTable;
	/*
	* Search a matching parent binding for the
	* IDC definition.
	*/
	while (parBind != NULL) {
	    if (parBind->definition == bind->definition)
		break;
	    parBind = parBind->next;
	}

	if (parBind != NULL) {
	    /*
	    * Compare every node-table entry of the child node,
	    * i.e. the key-sequence within, ...
	    */
	    oldNum = parBind->nbNodes; /* Skip newly added items. */

	    if (! XSD10_WXS_ILIST_IS_EMPTY(parBind->dupls)) {
		oldDupls = parBind->dupls->nbItems;
		dupls = (XmlXsd10PSVIIDCNode **) parBind->dupls->items;
	    } else {
		dupls = NULL;
		oldDupls = 0;
	    }

	    parNodes = parBind->nodeTable;
	    nbFields = bind->definition->nbFields;

	    for (i = 0; i < bind->nbNodes; i++) {
		node = bind->nodeTable[i];
		if (node == NULL)
		    continue;
		/*
		* ...with every key-sequence of the parent node, already
		* evaluated to be a duplicate key-sequence.
		*/
		if (oldDupls) {
		    j = 0;
		    while (j < oldDupls) {
			if (nbFields == 1) {
			    ret = xml_xsd10_are_values_equal(
				node->keys[0]->val,
				dupls[j]->keys[0]->val);
			    if (ret == -1)
				goto internal_error;
			    if (ret == 0) {
				j++;
				continue;
			    }
			} else {
			    parNode = dupls[j];
			    for (k = 0; k < nbFields; k++) {
				ret = xml_xsd10_are_values_equal(
				    node->keys[k]->val,
				    parNode->keys[k]->val);
				if (ret == -1)
				    goto internal_error;
				if (ret == 0)
				    break;
			    }
			}
			if (ret == 1)
			    /* Duplicate found. */
			    break;
			j++;
		    }
		    if (j != oldDupls) {
			/* Duplicate found. Skip this entry. */
			continue;
		    }
		}
		/*
		* ... and with every key-sequence of the parent node.
		*/
		if (oldNum) {
		    j = 0;
		    while (j < oldNum) {
			parNode = parNodes[j];
			if (nbFields == 1) {
			    ret = xml_xsd10_are_values_equal(
				node->keys[0]->val,
				parNode->keys[0]->val);
			    if (ret == -1)
				goto internal_error;
			    if (ret == 0) {
				j++;
				continue;
			    }
			} else {
			    for (k = 0; k < nbFields; k++) {
				ret = xml_xsd10_are_values_equal(
				    node->keys[k]->val,
				    parNode->keys[k]->val);
				if (ret == -1)
				    goto internal_error;
				if (ret == 0)
				    break;
			    }
			}
			if (ret == 1)
			    /* Duplicate found. */
			    break;
			j++;
		    }
		    if (j != oldNum) {
			/*
			* Handle duplicates. Move the duplicate in
			* the parent's node-table to the list of
			* duplicates.
			*/
			oldNum--;
			parBind->nbNodes--;
			/*
			* Move last old item to pos of duplicate.
			*/
			parNodes[j] = parNodes[oldNum];

			if (parBind->nbNodes != oldNum) {
			    /*
			    * If new items exist, move last new item to
			    * last of old items.
			    */
			    parNodes[oldNum] =
				parNodes[parBind->nbNodes];
			}
			if (parBind->dupls == NULL) {
			    parBind->dupls = xml_xsd10_item_list_create();
			    if (parBind->dupls == NULL)
				goto internal_error;
			}
			xml_xsd10_item_list_add(parBind->dupls, parNode);
		        dupls = (XmlXsd10PSVIIDCNode **)
                            parBind->dupls->items;
		    } else {
			/*
			* Add the node-table entry (node and key-sequence) of
			* the child node to the node table of the parent node.
			*/
                        if (parBind->nbNodes >= parBind->sizeNodes) {
                            XmlXsd10PSVIIDCNode **tmp;
                            int newSize;

                            newSize = xml_xsd10_grow_capacity(parBind->sizeNodes,
                                                      sizeof(tmp[0]),
                                                      10, XML_XSD10_MAX_ITEMS);
                            if (newSize < 0) {
                                xml_xsd10_v_err_memory(vctxt);
                                goto internal_error;
                            }
                            tmp = (XmlXsd10PSVIIDCNode **) realloc(parBind->nodeTable,
                                             newSize * sizeof(tmp[0]));
                            if (tmp == NULL) {
                                xml_xsd10_v_err_memory(vctxt);
                                goto internal_error;
                            }

                            parBind->nodeTable = tmp;
                            parBind->sizeNodes = newSize;
			}
			parNodes = parBind->nodeTable;
			/*
			* Append the new node-table entry to the 'new node-table
			* entries' section.
			*/
			parNodes[parBind->nbNodes++] = node;
		    }

		}

	    }
	} else {
	    /*
	    * No binding for the IDC was found: create a new one and
	    * copy all node-tables.
	    */
	    parBind = xml_xsd10_idc_new_binding(bind->definition);
	    if (parBind == NULL)
		goto internal_error;

	    /*
	    * TODO: Hmm, how to optimize the initial number of
	    * allocated entries?
	    */
	    if (bind->nbNodes != 0) {
		/*
		* Add all IDC node-table entries.
		*/
		if (! vctxt->psviExposeIDCNodeTables) {
		    /*
		    * Just move the entries.
		    * NOTE: this is quite save here, since
		    * all the keyref lookups have already been
		    * performed.
		    */
		    parBind->nodeTable = bind->nodeTable;
		    bind->nodeTable = NULL;
		    parBind->sizeNodes = bind->sizeNodes;
		    bind->sizeNodes = 0;
		    parBind->nbNodes = bind->nbNodes;
		    bind->nbNodes = 0;
		} else {
		    /*
		    * Copy the entries.
		    */
		    parBind->nodeTable = (XmlXsd10PSVIIDCNode **)
			malloc(bind->nbNodes *
			sizeof(XmlXsd10PSVIIDCNode *));
		    if (parBind->nodeTable == NULL) {
			xml_xsd10_v_err_memory(NULL);
			xml_xsd10_idc_free_binding(parBind);
			goto internal_error;
		    }
		    parBind->sizeNodes = bind->nbNodes;
		    parBind->nbNodes = bind->nbNodes;
		    memcpy(parBind->nodeTable, bind->nodeTable,
			bind->nbNodes * sizeof(XmlXsd10PSVIIDCNode *));
		}
	    }
	    if (bind->dupls) {
		/*
		* Move the duplicates.
		*/
		if (parBind->dupls != NULL)
		    xml_xsd10_item_list_free(parBind->dupls);
		parBind->dupls = bind->dupls;
		bind->dupls = NULL;
	    }
            if (parTable != NULL) {
                if (*parTable == NULL)
                    *parTable = parBind;
                else {
                    parBind->next = *parTable;
                    *parTable = parBind;
                }
            }
	}

next_binding:
	bind = bind->next;
    }
    return (0);

internal_error:
    return(-1);
}

/**
 * Port of xmlSchemaCheckCVCIDCKeyRef.
 *
 * Check the cvc-idc-keyref constraints.
 *
 * @param vctxt  the WXS validation context
 */
int xml_xsd10_check_cvcidc_key_ref(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10IDCMatcher *matcher;
    XmlXsd10PSVIIDCBinding *bind;

    matcher = vctxt->inode->idcMatchers;
    /*
    * Find a keyref.
    */
    while (matcher != NULL) {
	if ((matcher->idcType == XML_XSD10_SCHEMA_TYPE_IDC_KEYREF) &&
	    matcher->targets &&
	    matcher->targets->nbItems)
	{
	    int i, j, k, res, nbFields, hasDupls;
	    XmlXsd10PSVIIDCKey **refKeys, **keys;
	    XmlXsd10PSVIIDCNode *refNode = NULL;
	    XmlXsd10Hash *table = NULL;

	    nbFields = matcher->aidc->def->nbFields;

	    /*
	    * Find the IDC node-table for the referenced IDC key/unique.
	    */
	    bind = vctxt->inode->idcTable;
	    while (bind != NULL) {
		if ((XmlXsd10IDC *) matcher->aidc->def->ref->item ==
		    bind->definition)
		    break;
		bind = bind->next;
	    }
	    hasDupls = (bind && bind->dupls && bind->dupls->nbItems) ? 1 : 0;
	    /*
	    * Search for a matching key-sequences.
	    */
	    if (bind) {
		table = xml_xsd10_hash_create(bind->nbNodes * 2);
		for (j = 0; j < bind->nbNodes; j++) {
		    char *value;
		    XmlXsd10IDCHashEntry *r, *e;
		    keys = bind->nodeTable[j]->keys;
		    xml_xsd10_hash_key_sequence(vctxt, &value, keys, nbFields);
		    e = (XmlXsd10IDCHashEntry *) malloc(sizeof *e);
                    if (e == NULL) {
                        xml_xsd10_v_err_memory(vctxt);
                        goto mem_error;
                    }
		    e->index = j;
		    r = (XmlXsd10IDCHashEntry *) xml_xsd10_hash_lookup(table, value);
		    if (r) {
			e->next = r->next;
			r->next = e;
		    } else {
			e->next = NULL;
                        if (xml_xsd10_hash_add_entry(table, value, e) < 0) {
                            xml_xsd10_v_err_memory(vctxt);
                            free(e);
                        }
		    }
mem_error:
		    XSD10_FREE_AND_NULL(value);
		}
	    }
	    for (i = 0; i < matcher->targets->nbItems; i++) {
		res = 0;
		refNode = (XmlXsd10PSVIIDCNode *) matcher->targets->items[i];
		if (bind != NULL) {
		    char *value;
		    XmlXsd10IDCHashEntry *e;
		    refKeys = refNode->keys;
		    xml_xsd10_hash_key_sequence(vctxt, &value, refKeys, nbFields);
		    e = (XmlXsd10IDCHashEntry *) xml_xsd10_hash_lookup(table, value);
		    XSD10_FREE_AND_NULL(value);
		    res = 0;
		    for (;e; e = e->next) {
			keys = bind->nodeTable[e->index]->keys;
			for (k = 0; k < nbFields; k++) {
			    res = xml_xsd10_are_values_equal(keys[k]->val,
							  refKeys[k]->val);
			    if (res == 0)
			        break;
			    else if (res == -1) {
				return (-1);
			    }
			}
			if (res == 1) {
			    /*
			     * Match found.
			     */
			    break;
			}
		    }
		    if ((res == 0) && hasDupls) {
			/*
			* Search in duplicates
			*/
			for (j = 0; j < bind->dupls->nbItems; j++) {
			    keys = ((XmlXsd10PSVIIDCNode *)
				bind->dupls->items[j])->keys;
			    for (k = 0; k < nbFields; k++) {
				res = xml_xsd10_are_values_equal(keys[k]->val,
				    refKeys[k]->val);
				if (res == 0)
				    break;
				else if (res == -1) {
				    return (-1);
				}
			    }
			    if (res == 1) {
				/*
				* Match in duplicates found.
				*/
				char *str = NULL, *strB = NULL;
				xml_xsd10_keyref_err(vctxt,
				    XML_XSD10_SCHEMAV_CVC_IDC, refNode,
				    (XmlXsd10Type *) matcher->aidc->def,
				    "More than one match found for "
				    "key-sequence %s of keyref '%s'",
				    xml_xsd10_format_idc_key_sequence(vctxt, &str,
					refNode->keys, nbFields),
				    xml_xsd10_get_component_q_name(&strB,
					matcher->aidc->def));
				XSD10_FREE_AND_NULL(str);
				XSD10_FREE_AND_NULL(strB);
				break;
			    }
			}
		    }
		}

		if (res == 0) {
		    char *str = NULL, *strB = NULL;
		    xml_xsd10_keyref_err(vctxt,
			XML_XSD10_SCHEMAV_CVC_IDC, refNode,
			(XmlXsd10Type *) matcher->aidc->def,
			"No match found for key-sequence %s of keyref '%s'",
			xml_xsd10_format_idc_key_sequence(vctxt, &str,
			    refNode->keys, nbFields),
			xml_xsd10_get_component_q_name(&strB, matcher->aidc->def));
		    XSD10_FREE_AND_NULL(str);
		    XSD10_FREE_AND_NULL(strB);
		}
	    }
	    if (table) {
		xml_xsd10_hash_free(table, xml_xsd10_free_idc_hash_entry);
	    }
	}
	matcher = matcher->next;
    }
    /* TODO: Return an error if any error encountered. */
    return (0);
}

/************************************************************************
 *									*
 *			XML Reader validation code                      *
 *									*
 ************************************************************************/

/** Port of xmlSchemaGetFreshAttrInfo. */
XmlXsd10AttrInfo *xml_xsd10_get_fresh_attr_info(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10AttrInfo *iattr;
    /*
    * Grow/create list of attribute infos.
    */
    if (vctxt->sizeAttrInfos <= vctxt->nbAttrInfos) {
        XmlXsd10AttrInfo **tmp;
        int oldSize = vctxt->sizeAttrInfos;
        int newSize, i;

        newSize = xml_xsd10_grow_capacity(oldSize, sizeof(tmp[0]), 5, XML_XSD10_MAX_ITEMS);
	if (newSize < 0) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (NULL);
	}
	tmp = (XmlXsd10AttrInfo **) realloc(vctxt->attrInfos, newSize * sizeof(tmp[0]));
	if (tmp == NULL) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (NULL);
	}

        vctxt->attrInfos = tmp;
        vctxt->sizeAttrInfos = newSize;

        for (i = oldSize; i < newSize; i++)
            vctxt->attrInfos[i] = NULL;
    }

    iattr = vctxt->attrInfos[vctxt->nbAttrInfos];
    if (iattr != NULL) {
	if (iattr->localName != NULL) {
	    XSD10_VERROR_INT("xmlSchemaGetFreshAttrInfo",
		"attr info not cleared");
	    return (NULL);
	}
	iattr->nodeType = XML_DOM_ATTRIBUTE;
        vctxt->nbAttrInfos++;
	return (iattr);
    }

    /*
    * Create an attribute info.
    */
    iattr = (XmlXsd10AttrInfo *)
	malloc(sizeof(XmlXsd10AttrInfo));
    if (iattr == NULL) {
	xml_xsd10_v_err_memory(vctxt);
	return (NULL);
    }
    memset(iattr, 0, sizeof(XmlXsd10AttrInfo));
    iattr->nodeType = XML_DOM_ATTRIBUTE;
    vctxt->attrInfos[vctxt->nbAttrInfos++] = iattr;

    return (iattr);
}

/** Port of xmlSchemaValidatorPushAttribute. */
int xml_xsd10_validator_push_attribute(XmlXsd10ValidCtxt *vctxt, XMQNode *attrNode, int nodeLine, const char *localName, const char *nsName, int ownedNames, char *value, int ownedValue)
{
    XmlXsd10AttrInfo *attr;

    attr = xml_xsd10_get_fresh_attr_info(vctxt);
    if (attr == NULL) {
	XSD10_VERROR_INT("xmlSchemaPushAttribute",
	    "calling xmlSchemaGetFreshAttrInfo()");
	return (-1);
    }
    attr->node = attrNode;
    attr->nodeLine = nodeLine;
    attr->state = XML_XSD10_SCHEMAS_ATTR_UNKNOWN;
    attr->localName = localName;
    attr->nsName = nsName;
    if (ownedNames)
	attr->flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES;
    /*
    * Evaluate if it's an XSI attribute.
    */
    if (nsName != NULL) {
	if (xml_xsd10_str_equal(localName, "nil")) {
	    if (xml_xsd10_str_equal(attr->nsName, xml_xsd10_instance_ns)) {
		attr->metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL;
	    }
	} else if (xml_xsd10_str_equal(localName, "type")) {
	    if (xml_xsd10_str_equal(attr->nsName, xml_xsd10_instance_ns)) {
		attr->metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE;
	    }
	} else if (xml_xsd10_str_equal(localName, "schemaLocation")) {
	    if (xml_xsd10_str_equal(attr->nsName, xml_xsd10_instance_ns)) {
		attr->metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_SCHEMA_LOC;
	    }
	} else if (xml_xsd10_str_equal(localName, "noNamespaceSchemaLocation")) {
	    if (xml_xsd10_str_equal(attr->nsName, xml_xsd10_instance_ns)) {
		attr->metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NO_NS_SCHEMA_LOC;
	    }
	} else if (xml_xsd10_str_equal(attr->nsName, xml_xsd10_namespace_ns)) {
	    attr->metaType = XML_XSD10_SCHEMA_ATTR_INFO_META_XMLNS;
	}
    }
    attr->value = value;
    if (ownedValue)
	attr->flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
    if (attr->metaType != 0)
	attr->state = XML_XSD10_SCHEMAS_ATTR_META;
    return (0);
}

/**
 * Port of xmlSchemaClearElemInfo.
 *
 * @param vctxt  the WXS validation context
 * @param ielem  the element information item
 */
void xml_xsd10_clear_elem_info(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *ielem)
{
    ielem->hasKeyrefs = 0;
    ielem->appliedXPath = 0;
    if (ielem->flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES) {
	XSD10_FREE_AND_NULL(ielem->localName);
	XSD10_FREE_AND_NULL(ielem->nsName);
    } else {
	ielem->localName = NULL;
	ielem->nsName = NULL;
    }
    if (ielem->flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES) {
	XSD10_FREE_AND_NULL(ielem->value);
    } else {
	ielem->value = NULL;
    }
    if (ielem->val != NULL) {
	/*
	* PSVI TODO: Be careful not to free it when the value is
	* exposed via PSVI.
	*/
	xml_xsd10_free_value(ielem->val);
	ielem->val = NULL;
    }
    if (ielem->idcMatchers != NULL) {
	/*
	* REVISIT OPTIMIZE TODO: Use a pool of IDC matchers.
	*   Does it work?
	*/
	xml_xsd10_idc_release_matcher_list(vctxt, ielem->idcMatchers);
#if 0
	xml_xsd10_idc_free_matcher_list(ielem->idcMatchers);
#endif
	ielem->idcMatchers = NULL;
    }
    if (ielem->idcTable != NULL) {
	/*
	* OPTIMIZE TODO: Use a pool of IDC tables??.
	*/
	xml_xsd10_idc_free_idc_table(ielem->idcTable);
	ielem->idcTable = NULL;
    }
    if (ielem->regexCtxt != NULL) {
	xml_reg_exec_free(ielem->regexCtxt);
	ielem->regexCtxt = NULL;
    }
    if (ielem->nsBindings != NULL) {
	free((char **)ielem->nsBindings);
	ielem->nsBindings = NULL;
	ielem->nbNsBindings = 0;
	ielem->sizeNsBindings = 0;
    }
}

/**
 * Port of xmlSchemaGetFreshElemInfo.
 *
 * Creates/reuses and initializes the element info item for
 * the current tree depth.
 *
 * @param vctxt  the schema validation context
 * @returns the element info item or NULL on API or internal errors.
 */
XmlXsd10NodeInfo *xml_xsd10_get_fresh_elem_info(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10NodeInfo *info = NULL;

    if (vctxt->depth > vctxt->sizeElemInfos) {
	XSD10_VERROR_INT("xmlSchemaGetFreshElemInfo",
	    "inconsistent depth encountered");
	return (NULL);
    }
    if (vctxt->sizeElemInfos <= vctxt->depth) {
        XmlXsd10NodeInfo **tmp;
        int oldSize = vctxt->sizeElemInfos;
        int newSize, i;

        newSize = xml_xsd10_grow_capacity(oldSize, sizeof(tmp[0]), 10, XML_XSD10_MAX_ITEMS);
        if (newSize < 0) {
            xml_xsd10_v_err_memory(vctxt);
            return (NULL);
        }
        tmp = (XmlXsd10NodeInfo **) realloc(vctxt->elemInfos, newSize * sizeof(tmp[0]));
        if (tmp == NULL) {
            xml_xsd10_v_err_memory(vctxt);
            return (NULL);
        }

        vctxt->elemInfos = tmp;
        vctxt->sizeElemInfos = newSize;

	/*
	* We need the new memory to be NULLed.
	* TODO: Use memset instead?
	*/
	for (i = oldSize; i < newSize; i++)
	    vctxt->elemInfos[i] = NULL;
    } else
	info = vctxt->elemInfos[vctxt->depth];

    if (info == NULL) {
	info = (XmlXsd10NodeInfo *)
	    malloc(sizeof(XmlXsd10NodeInfo));
	if (info == NULL) {
	    xml_xsd10_v_err_memory(vctxt);
	    return (NULL);
	}
	vctxt->elemInfos[vctxt->depth] = info;
    } else {
	if (info->localName != NULL) {
	    XSD10_VERROR_INT("xmlSchemaGetFreshElemInfo",
		"elem info has not been cleared");
	    return (NULL);
	}
    }
    memset(info, 0, sizeof(XmlXsd10NodeInfo));
    info->nodeType = XML_DOM_ELEMENT;
    info->depth = vctxt->depth;

    return (info);
}

#define XSD10_ACTIVATE_ATTRIBUTE(item) vctxt->inode = (XmlXsd10NodeInfo *) item;
#define XSD10_ACTIVATE_ELEM vctxt->inode = vctxt->elemInfos[vctxt->depth];
#define XSD10_ACTIVATE_PARENT_ELEM vctxt->inode = vctxt->elemInfos[vctxt->depth -1];

/** Port of xmlSchemaValidateFacets. */
int xml_xsd10_validate_facets(XmlXsd10AbstractCtxt *actxt, XMQNode *node, XmlXsd10Type *type, XmlXsd10ValType valType, const char *value, XmlXsd10Val *val, unsigned long length, int fireErrors)
{
    int ret, error = 0, found;

    XmlXsd10Type *tmpType;
    XmlXsd10FacetLink *facetLink;
    XmlXsd10Facet *facet;
    unsigned long len = 0;
    XmlXsd10WhitespaceValueType ws;

    /*
    * In Libxml2, derived built-in types have currently no explicit facets.
    */
    if (type->type == XML_XSD10_SCHEMA_TYPE_BASIC)
	return (0);

    /*
    * NOTE: Do not jump away, if the facetSet of the given type is
    * empty: until now, "pattern" and "enumeration" facets of the
    **base types* need to be checked as well.
    */
    if (type->facetSet == NULL)
	goto pattern_and_enum;

    if (! XSD10_WXS_IS_ATOMIC(type)) {
	if (XSD10_WXS_IS_LIST(type))
	    goto XSD10_WXS_IS_LIST;
	else
	    goto pattern_and_enum;
    }

    /*
    * Whitespace handling is only of importance for string-based
    * types.
    */
    tmpType = xml_xsd10_get_primitive_type(type);
    if ((tmpType->builtInType == XML_XSD10_SCHEMAS_STRING) ||
	XSD10_WXS_IS_ANY_SIMPLE_TYPE(tmpType)) {
	ws = xml_xsd10_get_white_space_facet_value(type);
    } else
	ws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;

    /*
    * If the value was not computed (for string or
    * anySimpleType based types), then use the provided
    * type.
    */
    if (val != NULL)
	valType = xml_xsd10_get_val_type(val);

    ret = 0;
    for (facetLink = type->facetSet; facetLink != NULL;
	facetLink = facetLink->next) {
	/*
	* Skip the pattern "whiteSpace": it is used to
	* format the character content beforehand.
	*/
	switch (facetLink->facet->type) {
	    case XML_XSD10_SCHEMA_FACET_WHITESPACE:
	    case XML_XSD10_SCHEMA_FACET_PATTERN:
	    case XML_XSD10_SCHEMA_FACET_ENUMERATION:
		continue;
	    case XML_XSD10_SCHEMA_FACET_LENGTH:
	    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
	    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
		ret = xml_xsd10_validate_length_facet_whtsp(facetLink->facet,
		    valType, value, val, &len, ws);
		break;
	    default:
		ret = xml_xsd10_validate_facet_whtsp(facetLink->facet, ws,
		    valType, value, val, ws);
		break;
	}
	if (ret < 0) {
	    XSD10_AERROR_INT("xmlSchemaValidateFacets",
		"validating against a atomic type facet");
	    return (-1);
	} else if (ret > 0) {
	    if (fireErrors)
		xml_xsd10_facet_err(actxt, ret, node,
		value, len, type, facetLink->facet, NULL, NULL, NULL);
	    else
		return (ret);
	    if (error == 0)
		error = ret;
	}
	ret = 0;
    }

XSD10_WXS_IS_LIST:
    if (! XSD10_WXS_IS_LIST(type))
	goto pattern_and_enum;
    /*
    * "length", "minLength" and "maxLength" of list types.
    */
    ret = 0;
    for (facetLink = type->facetSet; facetLink != NULL;
	facetLink = facetLink->next) {

	switch (facetLink->facet->type) {
	    case XML_XSD10_SCHEMA_FACET_LENGTH:
	    case XML_XSD10_SCHEMA_FACET_MINLENGTH:
	    case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
		ret = xml_xsd10_validate_list_simple_type_facet(facetLink->facet,
		    value, length, NULL);
		break;
	    default:
		continue;
	}
	if (ret < 0) {
	    XSD10_AERROR_INT("xmlSchemaValidateFacets",
		"validating against a list type facet");
	    return (-1);
	} else if (ret > 0) {
	    if (fireErrors)
		xml_xsd10_facet_err(actxt, ret, node,
		value, length, type, facetLink->facet, NULL, NULL, NULL);
	    else
		return (ret);
	    if (error == 0)
		error = ret;
	}
	ret = 0;
    }

pattern_and_enum:
    found = 0;
    /*
    * Process enumerations. Facet values are in the value space
    * of the defining type's base type. This seems to be a bug in the
    * XML Schema 1.0 spec. Use the whitespace type of the base type.
    * Only the first set of enumerations in the ancestor-or-self axis
    * is used for validation.
    */
    ret = 0;
    tmpType = type;
    do {
        for (facet = tmpType->facets; facet != NULL; facet = facet->next) {
            if (facet->type != XML_XSD10_SCHEMA_FACET_ENUMERATION)
                continue;
            found = 1;
            ret = xml_xsd10_are_values_equal(facet->val, val);
            if (ret == 1)
                break;
            else if (ret < 0) {
                XSD10_AERROR_INT("xmlSchemaValidateFacets",
                    "validating against an enumeration facet");
                return (-1);
            }
        }
        if (ret != 0)
            break;
        /*
        * Break on the first set of enumerations. Any additional
        *  enumerations which might be existent on the ancestors
        *  of the current type are restricted by this set; thus
        *  *must**not* be taken into account.
        */
        if (found)
            break;
        tmpType = tmpType->baseType;
    } while ((tmpType != NULL) &&
        (tmpType->type != XML_XSD10_SCHEMA_TYPE_BASIC));
    if (found && (ret == 0)) {
        ret = XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID;
        if (fireErrors) {
            xml_xsd10_facet_err(actxt, ret, node,
                value, 0, type, NULL, NULL, NULL, NULL);
        } else
            return (ret);
        if (error == 0)
            error = ret;
    }

    /*
    * Process patters. Pattern facets are ORed at type level
    * and ANDed if derived. Walk the base type axis.
    */
    tmpType = type;
    facet = NULL;
    do {
        found = 0;
        for (facetLink = tmpType->facetSet; facetLink != NULL;
            facetLink = facetLink->next) {
            if (facetLink->facet->type != XML_XSD10_SCHEMA_FACET_PATTERN)
                continue;
            found = 1;
            /*
            * NOTE that for patterns, @value needs to be the
            * normalized value.
            */
            ret = xml_regexp_exec((XmlRegexp *) facetLink->facet->regexp, value);
            if (ret == 1)
                break;
            else if (ret < 0) {
                XSD10_AERROR_INT("xmlSchemaValidateFacets",
                    "validating against a pattern facet");
                return (-1);
            } else {
                /*
                * Save the last non-validating facet.
                */
                facet = facetLink->facet;
            }
        }
        if (found && (ret != 1)) {
            ret = XML_XSD10_SCHEMAV_CVC_PATTERN_VALID;
            if (fireErrors) {
                xml_xsd10_facet_err(actxt, ret, node,
                    value, 0, type, facet, NULL, NULL, NULL);
            } else
                return (ret);
            if (error == 0)
                error = ret;
            break;
        }
        tmpType = tmpType->baseType;
    } while ((tmpType != NULL) && (tmpType->type != XML_XSD10_SCHEMA_TYPE_BASIC));

    return (error);
}

/** Port of xmlSchemaNormalizeValue. */
char *xml_xsd10_normalize_value(XmlXsd10Type *type, const char *value)
{
    switch (xml_xsd10_get_white_space_facet_value(type)) {
	case XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE:
	    return (xml_xsd10_collapse_string(value));
	case XML_XSD10_SCHEMA_WHITESPACE_REPLACE:
	    return (xml_xsd10_white_space_replace(value));
	default:
	    return (NULL);
    }
}

/** Port of xmlSchemaValidateQName. */
int xml_xsd10_validate_q_name(XmlXsd10ValidCtxt *vctxt, const char *value, XmlXsd10Val **val, int valNeeded)
{
    int ret;
    char *stripped;
    const char *nsName;
    char *local, *prefix = NULL;

    ret = xml_xsd10_tree_validate_qname(value, 1);
    if (ret != 0) {
	if (ret == -1) {
	    XSD10_VERROR_INT("xmlSchemaValidateQName",
		"calling xmlValidateQName()");
	    return (-1);
	}
	return( XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1);
    }
    /*
    * NOTE: xmlSplitQName2 will always return a duplicated
    * strings.
    */
    /* TODO: Export and use xmlSchemaStrip instead */
    stripped = xml_xsd10_collapse_string(value);
    local = xml_xsd10_tree_split_qname2(stripped ? stripped : value, &prefix);
    free(stripped);
    if (local == NULL)
	local = xml_xsd10_strdup(value);
    /*
    * OPTIMIZE TODO: Use flags for:
    *  - is there any namespace binding?
    *  - is there a default namespace?
    */
    nsName = xml_xsd10_lookup_namespace(vctxt, prefix);

    if (prefix != NULL) {
	free(prefix);
	/*
	* A namespace must be found if the prefix is
	* NOT NULL.
	*/
	if (nsName == NULL) {
	    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
	    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt, ret, NULL,
		XSD10_WXS_BASIC_CAST xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME),
		"The QName value '%s' has no "
		"corresponding namespace declaration in "
		"scope", value, NULL);
	    if (local != NULL)
		free(local);
	    return (ret);
	}
    }
    if (valNeeded && val) {
	if (nsName != NULL)
	    *val = xml_xsd10_new_qname_value(
		xml_xsd10_strdup(nsName), local);
	else
	    *val = xml_xsd10_new_qname_value(NULL,
		local);
    } else
	free(local);
    return (0);
}

/*
* The ID table of the schema document holding node (libxml2 registers xs:ID values of
* attribute nodes in node->doc->ids): the one of its bucket, created when needed.
*/
static XmlXsd10Ids *xsd10_valid_schema_doc_ids(XmlXsd10ParserCtxt *pctxt, XMQNode *node)
{
    XmlDom *doc;
    int i;

    if ((node == NULL) || (xml_node_kind(node) != XML_DOM_ATTRIBUTE) ||
        (pctxt->constructor == NULL) || (pctxt->constructor->buckets == NULL))
        return (NULL);
    doc = xml_node_dom(node);
    for (i = 0; i < pctxt->constructor->buckets->nbItems; i++) {
        XmlXsd10Bucket *bucket =
            (XmlXsd10Bucket *) pctxt->constructor->buckets->items[i];
        if ((bucket != NULL) && (bucket->doc == doc)) {
            if (bucket->ids == NULL)
                bucket->ids = xml_xsd10_ids_new();
            return (bucket->ids);
        }
    }
    return (NULL);
}

/*
* cvc-simple-type
*/
/** Port of xmlSchemaVCheckCVCSimpleType. */
int xml_xsd10_v_check_cvc_simple_type(XmlXsd10AbstractCtxt *actxt, XMQNode *node, XmlXsd10Type *type, const char *value, XmlXsd10Val **retVal, int fireErrors, int normalize, int isNormalized)
{
    int ret = 0, valNeeded = (retVal) ? 1 : 0;
    XmlXsd10Val *val = NULL;
    /* xmlSchemaWhitespaceValueType ws; */
    char *normValue = NULL;

#define XSD10_NORMALIZE(atype) \
    if ((! isNormalized) && \
    (normalize || (type->flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED))) { \
	normValue = xml_xsd10_normalize_value(atype, value); \
	if (normValue != NULL) \
	    value = normValue; \
	isNormalized = 1; \
    }

    if ((retVal != NULL) && (*retVal != NULL)) {
	xml_xsd10_free_value(*retVal);
	*retVal = NULL;
    }
    /*
    * 3.14.4 Simple Type Definition Validation Rules
    * Validation Rule: String Valid
    */
    /*
    * 1 It is schema-valid with respect to that definition as defined
    * by Datatype Valid in [XML Schemas: Datatypes].
    */
    /*
    * 2.1 If The definition is ENTITY or is validly derived from ENTITY given
    * the empty set, as defined in Type Derivation OK (Simple) ($3.14.6), then
    * the string must be a `declared entity name`.
    */
    /*
    * 2.2 If The definition is ENTITIES or is validly derived from ENTITIES
    * given the empty set, as defined in Type Derivation OK (Simple) ($3.14.6),
    * then every whitespace-delimited substring of the string must be a `declared
    * entity name`.
    */
    /*
    * 2.3 otherwise no further condition applies.
    */
    if ((! valNeeded) && (type->flags & XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE))
	valNeeded = 1;
    if (value == NULL)
	value = "";
    if (XSD10_WXS_IS_ANY_SIMPLE_TYPE(type) || XSD10_WXS_IS_ATOMIC(type)) {
	XmlXsd10Type *biType; /* The built-in type. */
	/*
	* SPEC (1.2.1) "if {variety} is `atomic` then the string must `match`
	* a literal in the `lexical space` of {base type definition}"
	*/
	/*
	* Whitespace-normalize.
	*/
	XSD10_NORMALIZE(type);
	if (type->type != XML_XSD10_SCHEMA_TYPE_BASIC) {
	    /*
	    * Get the built-in type.
	    */
	    biType = type->baseType;
	    while ((biType != NULL) &&
		(biType->type != XML_XSD10_SCHEMA_TYPE_BASIC))
		biType = biType->baseType;

	    if (biType == NULL) {
		XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
		    "could not get the built-in type");
		goto internal_error;
	    }
	} else
	    biType = type;
	/*
	* NOTATIONs need to be processed here, since they need
	* to lookup in the hashtable of NOTATION declarations of the schema.
	*/
	if (actxt->type == XML_XSD10_SCHEMA_CTXT_VALIDATOR) {
	    switch (biType->builtInType) {
		case XML_XSD10_SCHEMAS_NOTATION:
		    ret = xml_xsd10_validate_notation(
			(XmlXsd10ValidCtxt *) actxt,
			((XmlXsd10ValidCtxt *) actxt)->schema,
			NULL, value, &val, valNeeded);
		    break;
		case XML_XSD10_SCHEMAS_QNAME:
		    ret = xml_xsd10_validate_q_name((XmlXsd10ValidCtxt *) actxt,
			value, &val, valNeeded);
		    break;
		default:
		    /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
		    if (valNeeded)
			ret = xml_xsd10_val_predef_type_node_no_norm_ids(biType,
			    value, &val, node, ((XmlXsd10ValidCtxt *) actxt)->ids);
		    else
			ret = xml_xsd10_val_predef_type_node_no_norm_ids(biType,
			    value, NULL, node, ((XmlXsd10ValidCtxt *) actxt)->ids);
		    break;
	    }
	} else if (actxt->type == XML_XSD10_SCHEMA_CTXT_PARSER) {
	    switch (biType->builtInType) {
		case XML_XSD10_SCHEMAS_NOTATION:
		    ret = xml_xsd10_validate_notation(NULL,
			((XmlXsd10ParserCtxt *) actxt)->schema, node,
			value, &val, valNeeded);
		    break;
		default:
		    /* ws = xmlSchemaGetWhiteSpaceFacetValue(type); */
		    if (valNeeded)
			ret = xml_xsd10_val_predef_type_node_no_norm_ids(biType,
			    value, &val, node,
			    xsd10_valid_schema_doc_ids((XmlXsd10ParserCtxt *) actxt, node));
		    else
			ret = xml_xsd10_val_predef_type_node_no_norm_ids(biType,
			    value, NULL, node,
			    xsd10_valid_schema_doc_ids((XmlXsd10ParserCtxt *) actxt, node));
		    break;
	    }
	} else {
	    /*
	    * Validation via a public API is not implemented yet.
	    */
	    goto internal_error;
	}
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
		    "validating against a built-in type");
		goto internal_error;
	    }
	    if (XSD10_WXS_IS_LIST(type))
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
	    else
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
	}
	if ((ret == 0) && (type->flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS)) {
	    /*
	    * Check facets.
	    */
	    ret = xml_xsd10_validate_facets(actxt, node, type,
		(XmlXsd10ValType) biType->builtInType, value, val,
		0, fireErrors);
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
			"validating facets of atomic simple type");
		    goto internal_error;
		}
		if (XSD10_WXS_IS_LIST(type))
		    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
		else
		    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
	    }
	}
	else if (fireErrors && (ret > 0))
	    xml_xsd10_simple_type_err(actxt, ret, node, value, type, 1);
    } else if (XSD10_WXS_IS_LIST(type)) {

	XmlXsd10Type *itemType;
	const char *cur, *end;
	char *tmpValue = NULL;
	unsigned long len = 0;
	XmlXsd10Val *prevVal = NULL, *curVal = NULL;
	/* 1.2.2 if {variety} is `list` then the string must be a sequence
	* of white space separated tokens, each of which `match`es a literal
	* in the `lexical space` of {item type definition}
	*/
	/*
	* Note that XML_SCHEMAS_TYPE_NORMVALUENEEDED will be set if
	* the list type has an enum or pattern facet.
	*/
	XSD10_NORMALIZE(type);
	/*
	* VAL TODO: Optimize validation of empty values.
	* VAL TODO: We do not have computed values for lists.
	*/
	itemType = XSD10_WXS_LIST_ITEMTYPE(type);
	cur = value;
	do {
	    while (XSD10_VALID_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_VALID_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    tmpValue = xml_xsd10_strndup(cur, end - cur);
	    len++;

	    if (valNeeded)
		ret = xml_xsd10_v_check_cvc_simple_type(actxt, node, itemType,
		    tmpValue, &curVal, fireErrors, 0, 1);
	    else
		ret = xml_xsd10_v_check_cvc_simple_type(actxt, node, itemType,
		    tmpValue, NULL, fireErrors, 0, 1);
	    XSD10_FREE_AND_NULL(tmpValue);
	    if (curVal != NULL) {
		/*
		* Add to list of computed values.
		*/
		if (val == NULL)
		    val = curVal;
		else
		    xml_xsd10_value_append(prevVal, curVal);
		prevVal = curVal;
		curVal = NULL;
	    }
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
			"validating an item of list simple type");
		    goto internal_error;
		}
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
		break;
	    }
	    cur = end;
	} while (*cur != 0);
	XSD10_FREE_AND_NULL(tmpValue);
	if ((ret == 0) && (type->flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS)) {
	    /*
	    * Apply facets (pattern, enumeration).
	    */
	    ret = xml_xsd10_validate_facets(actxt, node, type,
		XML_XSD10_SCHEMAS_UNKNOWN, value, val,
		len, fireErrors);
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
			"validating facets of list simple type");
		    goto internal_error;
		}
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
	    }
	}
	if (fireErrors && (ret > 0)) {
	    /*
	    * Report the normalized value.
	    */
	    normalize = 1;
	    XSD10_NORMALIZE(type);
	    xml_xsd10_simple_type_err(actxt, ret, node, value, type, 1);
	}
    } else if (XSD10_WXS_IS_UNION(type)) {
	XmlXsd10TypeLink *memberLink;
	/*
	* TODO: For all datatypes `derived` by `union`  whiteSpace does
	* not apply directly; however, the normalization behavior of `union`
	* types is controlled by the value of whiteSpace on that one of the
	* `memberTypes` against which the `union` is successfully validated.
	*
	* This means that the value is normalized by the first validating
	* member type, then the facets of the union type are applied. This
	* needs changing of the value!
	*/

	/*
	* 1.2.3 if {variety} is `union` then the string must `match` a
	* literal in the `lexical space` of at least one member of
	* {member type definitions}
	*/
	memberLink = xml_xsd10_get_union_simple_type_member_types(type);
	if (memberLink == NULL) {
	    XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
		"union simple type has no member types");
	    goto internal_error;
	}
	/*
	* Always normalize union type values, since we currently
	* cannot store the whitespace information with the value
	* itself; otherwise a later value-comparison would be
	* not possible.
	*/
	while (memberLink != NULL) {
	    if (valNeeded)
		ret = xml_xsd10_v_check_cvc_simple_type(actxt, node,
		    memberLink->type, value, &val, 0, 1, 0);
	    else
		ret = xml_xsd10_v_check_cvc_simple_type(actxt, node,
		    memberLink->type, value, NULL, 0, 1, 0);
	    if (ret <= 0)
		break;
	    memberLink = memberLink->next;
	}
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
		    "validating members of union simple type");
		goto internal_error;
	    }
	    ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3;
	}
	/*
	* Apply facets (pattern, enumeration).
	*/
	if ((ret == 0) && (type->flags & XML_XSD10_SCHEMAS_TYPE_HAS_FACETS)) {
	    /*
	    * The normalization behavior of `union` types is controlled by
	    * the value of whiteSpace on that one of the `memberTypes`
	    * against which the `union` is successfully validated.
	    */
	    XSD10_NORMALIZE(memberLink->type);
	    ret = xml_xsd10_validate_facets(actxt, node, type,
		XML_XSD10_SCHEMAS_UNKNOWN, value, val,
		0, fireErrors);
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_AERROR_INT("xmlSchemaVCheckCVCSimpleType",
			"validating facets of union simple type");
		    goto internal_error;
		}
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_3;
	    }
	}
	if (fireErrors && (ret > 0))
	    xml_xsd10_simple_type_err(actxt, ret, node, value, type, 1);
    }

    if (normValue != NULL)
	free(normValue);
    if (ret == 0) {
	if (retVal != NULL)
	    *retVal = val;
	else if (val != NULL)
	    xml_xsd10_free_value(val);
    } else if (val != NULL)
	xml_xsd10_free_value(val);
    return (ret);
internal_error:
    if (normValue != NULL)
	free(normValue);
    if (val != NULL)
	xml_xsd10_free_value(val);
    return (-1);
}

/** Port of xmlSchemaVExpandQName. */
int xml_xsd10_v_expand_q_name(XmlXsd10ValidCtxt *vctxt, const char *value, const char **nsName, const char **localName)
{
    int ret = 0;

    if ((nsName == NULL) || (localName == NULL))
	return (-1);
    *nsName = NULL;
    *localName = NULL;

    ret = xml_xsd10_tree_validate_qname(value, 1);
    if (ret == -1)
	return (-1);
    if (ret > 0) {
	xml_xsd10_simple_type_err(XSD10_ACTXT_CAST vctxt,
	    XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1, NULL,
	    value, xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME), 1);
	return (1);
    }
    {
	char *local = NULL;
	char *prefix;

	/*
	* NOTE: xmlSplitQName2 will return a duplicated
	* string.
	*/
	local = xml_xsd10_tree_split_qname2(value, &prefix);
	if (local == NULL)
	    *localName = xml_xsd10_dict_lookup(vctxt->dict, value, -1);
	else {
	    *localName = xml_xsd10_dict_lookup(vctxt->dict, local, -1);
	    free(local);
	}

	*nsName = xml_xsd10_lookup_namespace(vctxt, prefix);

	if (prefix != NULL) {
	    free(prefix);
	    /*
	    * A namespace must be found if the prefix is NOT NULL.
	    */
	    if (*nsName == NULL) {
		xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1, NULL,
		    XSD10_WXS_BASIC_CAST xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME),
		    "The QName value '%s' has no "
		    "corresponding namespace declaration in scope",
		    value, NULL);
		return (2);
	    }
	}
    }
    return (0);
}

/** Port of xmlSchemaProcessXSIType. */
int xml_xsd10_process_xsi_type(XmlXsd10ValidCtxt *vctxt, XmlXsd10AttrInfo *iattr, XmlXsd10Type **localType, XmlXsd10Element *elemDecl)
{
    int ret = 0;
    /*
    * cvc-elt (3.3.4) : (4)
    * AND
    * Schema-Validity Assessment (Element) (cvc-assess-elt)
    *   (1.2.1.2.1) - (1.2.1.2.4)
    * Handle 'xsi:type'.
    */
    if (localType == NULL)
	return (-1);
    *localType = NULL;
    if (iattr == NULL)
	return (0);
    else {
	const char *nsName = NULL, *local = NULL;
	/*
	* TODO: We should report a *warning* that the type was overridden
	* by the instance.
	*/
	XSD10_ACTIVATE_ATTRIBUTE(iattr);
	/*
	* (cvc-elt) (3.3.4) : (4.1)
	* (cvc-assess-elt) (1.2.1.2.2)
	*/
	ret = xml_xsd10_v_expand_q_name(vctxt, iattr->value,
	    &nsName, &local);
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidateElementByDeclaration",
		    "calling xmlSchemaQNameExpand() to validate the "
		    "attribute 'xsi:type'");
		goto internal_error;
	    }
	    goto exit;
	}
	/*
	* (cvc-elt) (3.3.4) : (4.2)
	* (cvc-assess-elt) (1.2.1.2.3)
	*/
	*localType = xml_xsd10_get_type(vctxt->schema, local, nsName);
	if (*localType == NULL) {
	    char *str = NULL;

	    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		XML_XSD10_SCHEMAV_CVC_ELT_4_2, NULL,
		XSD10_WXS_BASIC_CAST xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_QNAME),
		"The QName value '%s' of the xsi:type attribute does not "
		"resolve to a type definition",
		xml_xsd10_format_q_name(&str, nsName, local), NULL);
	    XSD10_FREE_AND_NULL(str);
	    ret = vctxt->err;
	    goto exit;
	}
	if (elemDecl != NULL) {
	    int set = 0;

	    /*
	    * SPEC cvc-elt (3.3.4) : (4.3) (Type Derivation OK)
	    * "The `local type definition` must be validly
	    * derived from the {type definition} given the union of
	    * the {disallowed substitutions} and the {type definition}'s
	    * {prohibited substitutions}, as defined in
	    * Type Derivation OK (Complex) ($3.4.6)
	    * (if it is a complex type definition),
	    * or given {disallowed substitutions} as defined in Type
	    * Derivation OK (Simple) ($3.14.6) (if it is a simple type
	    * definition)."
	    *
	    * {disallowed substitutions}: the "block" on the element decl.
	    * {prohibited substitutions}: the "block" on the type def.
	    */
	    /*
	    * OPTIMIZE TODO: We could map types already evaluated
	    * to be validly derived from other types to avoid checking
	    * this over and over for the same types.
	    */
	    if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_EXTENSION) ||
		(elemDecl->subtypes->flags &
		    XML_XSD10_SCHEMAS_TYPE_BLOCK_EXTENSION))
		set |= XSD10_SUBSET_EXTENSION;

	    if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_BLOCK_RESTRICTION) ||
		(elemDecl->subtypes->flags &
		    XML_XSD10_SCHEMAS_TYPE_BLOCK_RESTRICTION))
		set |= XSD10_SUBSET_RESTRICTION;

	    /*
	    * REMOVED and CHANGED since this produced a parser context
	    * which adds to the string dict of the schema. So this would
	    * change the schema and we don't want this. We don't need
	    * the parser context anymore.
	    *
	    * if ((vctxt->pctxt == NULL) &&
	    *	(xmlSchemaCreatePCtxtOnVCtxt(vctxt) == -1))
	    *	    return (-1);
	    */

	    if (xml_xsd10_check_cos_derived_ok(XSD10_ACTXT_CAST vctxt, *localType,
		elemDecl->subtypes, set) != 0) {
		char *str = NULL;

		xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_CVC_ELT_4_3, NULL, NULL,
		    "The type definition '%s', specified by xsi:type, is "
		    "blocked or not validly derived from the type definition "
		    "of the element declaration",
		    xml_xsd10_format_q_name(&str,
			(*localType)->targetNamespace,
			(*localType)->name),
		    NULL);
		XSD10_FREE_AND_NULL(str);
		ret = vctxt->err;
		*localType = NULL;
	    }
	}
    }
exit:
    XSD10_ACTIVATE_ELEM;
    return (ret);
internal_error:
    XSD10_ACTIVATE_ELEM;
    return (-1);
}

/** Port of xmlSchemaValidateElemDecl. */
int xml_xsd10_validate_elem_decl(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10Element *elemDecl = vctxt->inode->decl;
    XmlXsd10Type *actualType;

    /*
    * cvc-elt (3.3.4) : 1
    */
    if (elemDecl == NULL) {
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_1, NULL,
	    "No matching declaration available");
        return (vctxt->err);
    }
    actualType = XSD10_WXS_ELEM_TYPEDEF(elemDecl);
    /*
    * cvc-elt (3.3.4) : 2
    */
    if (elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_ABSTRACT) {
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_2, NULL,
	    "The element declaration is abstract");
        return (vctxt->err);
    }
    if (actualType == NULL) {
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_TYPE_1, NULL,
	    "The type definition is absent");
	return (XML_XSD10_SCHEMAV_CVC_TYPE_1);
    }
    if (vctxt->nbAttrInfos != 0) {
	int ret;
	XmlXsd10AttrInfo *iattr;
	/*
	* cvc-elt (3.3.4) : 3
	* Handle 'xsi:nil'.
	*/
	iattr = xml_xsd10_get_meta_attr_info(vctxt,
	    XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_NIL);
	if (iattr) {
	    XSD10_ACTIVATE_ATTRIBUTE(iattr);
	    /*
	    * Validate the value.
	    */
	    ret = xml_xsd10_v_check_cvc_simple_type(
		XSD10_ACTXT_CAST vctxt, NULL,
		xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_BOOLEAN),
		iattr->value, &(iattr->val), 1, 0, 0);
	    XSD10_ACTIVATE_ELEM;
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidateElemDecl",
		    "calling xmlSchemaVCheckCVCSimpleType() to "
		    "validate the attribute 'xsi:nil'");
		return (-1);
	    }
	    if (ret == 0) {
		if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_NILLABLE) == 0) {
		    /*
		    * cvc-elt (3.3.4) : 3.1
		    */
		    XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_3_1, NULL,
			"The element is not 'nillable'");
		    /* Does not return an error on purpose. */
		} else {
		    if (xml_xsd10_value_get_as_boolean(iattr->val)) {
			/*
			* cvc-elt (3.3.4) : 3.2.2
			*/
			if ((elemDecl->flags & XML_XSD10_SCHEMAS_ELEM_FIXED) &&
			    (elemDecl->value != NULL)) {
			    XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_3_2_2, NULL,
				"The element cannot be 'nilled' because "
				"there is a fixed value constraint defined "
				"for it");
			     /* Does not return an error on purpose. */
			} else
			    vctxt->inode->flags |=
				XML_XSD10_SCHEMA_ELEM_INFO_NILLED;
		    }
		}
	    }
	}
	/*
	* cvc-elt (3.3.4) : 4
	* Handle 'xsi:type'.
	*/
	iattr = xml_xsd10_get_meta_attr_info(vctxt,
	    XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
	if (iattr) {
	    XmlXsd10Type *localType = NULL;

	    ret = xml_xsd10_process_xsi_type(vctxt, iattr, &localType,
		elemDecl);
	    if (ret != 0) {
		if (ret == -1) {
		    XSD10_VERROR_INT("xmlSchemaValidateElemDecl",
			"calling xmlSchemaProcessXSIType() to "
			"process the attribute 'xsi:type'");
		    return (-1);
		}
		/* Does not return an error on purpose. */
	    }
	    if (localType != NULL) {
		vctxt->inode->flags |= XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE;
		actualType = localType;
	    }
	}
    }
    /*
    * IDC: Register identity-constraint XPath matchers.
    */
    if ((elemDecl->idcs != NULL) &&
	(xml_xsd10_idc_register_matchers(vctxt, elemDecl) == -1))
	    return (-1);
    /*
    * No actual type definition.
    */
    if (actualType == NULL) {
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_TYPE_1, NULL,
	    "The type definition is absent");
	return (XML_XSD10_SCHEMAV_CVC_TYPE_1);
    }
    /*
    * Remember the actual type definition.
    */
    vctxt->inode->typeDef = actualType;

    return (0);
}

/** Port of xmlSchemaVAttributesSimple. */
int xml_xsd10_v_attributes_simple(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10AttrInfo *iattr;
    int ret = 0, i;

    /*
    * SPEC cvc-type (3.1.1)
    * "The attributes of must be empty, excepting those whose namespace
    * name is identical to http://www.w3.org/2001/XMLSchema-instance and
    * whose local name is one of type, nil, schemaLocation or
    * noNamespaceSchemaLocation."
    */
    if (vctxt->nbAttrInfos == 0)
	return (0);
    for (i = 0; i < vctxt->nbAttrInfos; i++) {
	iattr = vctxt->attrInfos[i];
	if (! iattr->metaType) {
	    XSD10_ACTIVATE_ATTRIBUTE(iattr)
	    xml_xsd10_illegal_attr_err(XSD10_ACTXT_CAST vctxt,
		XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1, iattr, NULL);
	    ret = XML_XSD10_SCHEMAV_CVC_TYPE_3_1_1;
        }
    }
    XSD10_ACTIVATE_ELEM
    return (ret);
}

/*
* Cleanup currently used attribute infos.
*/
/** Port of xmlSchemaClearAttrInfos. */
void xml_xsd10_clear_attr_infos(XmlXsd10ValidCtxt *vctxt)
{
    int i;
    XmlXsd10AttrInfo *attr;

    if (vctxt->nbAttrInfos == 0)
	return;
    for (i = 0; i < vctxt->nbAttrInfos; i++) {
	attr = vctxt->attrInfos[i];
	if (attr->flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_NAMES) {
	    if (attr->localName != NULL)
		free((char *) attr->localName);
	    if (attr->nsName != NULL)
		free((char *) attr->nsName);
	}
	if (attr->flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES) {
	    if (attr->value != NULL)
		free((char *) attr->value);
	}
	if (attr->val != NULL) {
	    xml_xsd10_free_value(attr->val);
	    attr->val = NULL;
	}
	memset(attr, 0, sizeof(XmlXsd10AttrInfo));
    }
    vctxt->nbAttrInfos = 0;
}

/*
* 3.4.4 Complex Type Definition Validation Rules
*   Element Locally Valid (Complex Type) (cvc-complex-type)
* 3.2.4 Attribute Declaration Validation Rules
*   Validation Rule: Attribute Locally Valid (cvc-attribute)
*   Attribute Locally Valid (Use) (cvc-au)
*
* Only "assessed" attribute information items will be visible to
* IDCs. I.e. not "lax" (without declaration) and "skip" wild attributes.
*/
/** Port of xmlSchemaVAttributesComplex. */
int xml_xsd10_v_attributes_complex(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10Type *type = vctxt->inode->typeDef;
    XmlXsd10ItemList *attrUseList;
    XmlXsd10AttributeUse *attrUse = NULL;
    XmlXsd10Attribute *attrDecl = NULL;
    XmlXsd10AttrInfo *iattr, *tmpiattr;
    int i, j, found, nbAttrs, nbUses;
    int xpathRes = 0, res, wildIDs = 0, fixed;
    XMQNode *defAttrOwnerElem = NULL;

    /*
    * SPEC (cvc-attribute)
    * (1) "The declaration must not be `absent` (see Missing
    * Sub-components ($5.3) for how this can fail to be
    * the case)."
    * (2) "Its {type definition} must not be absent."
    *
    * NOTE (1) + (2): This is not handled here, since we currently do not
    * allow validation against schemas which have missing sub-components.
    *
    * SPEC (cvc-complex-type)
    * (3) "For each attribute information item in the element information
    * item's [attributes] excepting those whose [namespace name] is
    * identical to http://www.w3.org/2001/XMLSchema-instance and whose
    * [local name] is one of type, nil, schemaLocation or
    * noNamespaceSchemaLocation, the appropriate case among the following
    * must be true:
    *
    */
    attrUseList = (XmlXsd10ItemList *) type->attrUses;
    /*
    * @nbAttrs is the number of attributes present in the instance.
    */
    nbAttrs = vctxt->nbAttrInfos;
    if (attrUseList != NULL)
	nbUses = attrUseList->nbItems;
    else
	nbUses = 0;
    for (i = 0; i < nbUses; i++) {
        found = 0;
	attrUse = (XmlXsd10AttributeUse *) attrUseList->items[i];
	attrDecl = XSD10_WXS_ATTRUSE_DECL(attrUse);
        for (j = 0; j < nbAttrs; j++) {
	    iattr = vctxt->attrInfos[j];
	    /*
	    * SPEC (cvc-complex-type) (3)
	    * Skip meta attributes.
	    */
	    if (iattr->metaType)
		continue;
	    if (iattr->localName[0] != attrDecl->name[0])
		continue;
	    if (!xml_xsd10_str_equal(iattr->localName, attrDecl->name))
		continue;
	    if (!xml_xsd10_str_equal(iattr->nsName, attrDecl->targetNamespace))
		continue;
	    found = 1;
	    /*
	    * SPEC (cvc-complex-type)
	    * (3.1) "If there is among the {attribute uses} an attribute
	    * use with an {attribute declaration} whose {name} matches
	    * the attribute information item's [local name] and whose
	    * {target namespace} is identical to the attribute information
	    * item's [namespace name] (where an `absent` {target namespace}
	    * is taken to be identical to a [namespace name] with no value),
	    * then the attribute information must be `valid` with respect
	    * to that attribute use as per Attribute Locally Valid (Use)
	    * ($3.5.4). In this case the {attribute declaration} of that
	    * attribute use is the `context-determined declaration` for the
	    * attribute information item with respect to Schema-Validity
	    * Assessment (Attribute) ($3.2.4) and
	    * Assessment Outcome (Attribute) ($3.2.5).
	    */
	    iattr->state = XML_XSD10_SCHEMAS_ATTR_ASSESSED;
	    iattr->use = attrUse;
	    /*
	    * Context-determined declaration.
	    */
	    iattr->decl = attrDecl;
	    iattr->typeDef = attrDecl->subtypes;
	    break;
	}

	if (found)
	    continue;

	if (attrUse->occurs == XML_XSD10_SCHEMAS_ATTR_USE_REQUIRED) {
	    /*
	    * Handle non-existent, required attributes.
	    *
	    * SPEC (cvc-complex-type)
	    * (4) "The {attribute declaration} of each attribute use in
	    * the {attribute uses} whose {required} is true matches one
	    * of the attribute information items in the element information
	    * item's [attributes] as per clause 3.1 above."
	    */
	    tmpiattr = xml_xsd10_get_fresh_attr_info(vctxt);
	    if (tmpiattr == NULL) {
		XSD10_VERROR_INT(
		    "xmlSchemaVAttributesComplex",
		    "calling xmlSchemaGetFreshAttrInfo()");
		return (-1);
	    }
	    tmpiattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_MISSING;
	    tmpiattr->use = attrUse;
	    tmpiattr->decl = attrDecl;
	} else if ((attrUse->occurs == XML_XSD10_SCHEMAS_ATTR_USE_OPTIONAL) &&
	    ((attrUse->defValue != NULL) ||
	     (attrDecl->defValue != NULL))) {
	    /*
	    * Handle non-existent, optional, default/fixed attributes.
	    */
	    tmpiattr = xml_xsd10_get_fresh_attr_info(vctxt);
	    if (tmpiattr == NULL) {
		XSD10_VERROR_INT(
		    "xmlSchemaVAttributesComplex",
		    "calling xmlSchemaGetFreshAttrInfo()");
		return (-1);
	    }
	    tmpiattr->state = XML_XSD10_SCHEMAS_ATTR_DEFAULT;
	    tmpiattr->use = attrUse;
	    tmpiattr->decl = attrDecl;
	    tmpiattr->typeDef = attrDecl->subtypes;
	    tmpiattr->localName = attrDecl->name;
	    tmpiattr->nsName = attrDecl->targetNamespace;
	}
    }

    if (vctxt->nbAttrInfos == 0)
	return (0);
    /*
    * Validate against the wildcard.
    */
    if (type->attributeWildcard != NULL) {
	/*
	* SPEC (cvc-complex-type)
	* (3.2.1) "There must be an {attribute wildcard}."
	*/
	for (i = 0; i < nbAttrs; i++) {
	    iattr = vctxt->attrInfos[i];
	    /*
	    * SPEC (cvc-complex-type) (3)
	    * Skip meta attributes.
	    */
	    if (iattr->state != XML_XSD10_SCHEMAS_ATTR_UNKNOWN)
		continue;
	    /*
	    * SPEC (cvc-complex-type)
	    * (3.2.2) "The attribute information item must be `valid` with
	    * respect to it as defined in Item Valid (Wildcard) ($3.10.4)."
	    *
	    * SPEC Item Valid (Wildcard) (cvc-wildcard)
	    * "... its [namespace name] must be `valid` with respect to
	    * the wildcard constraint, as defined in Wildcard allows
	    * Namespace Name ($3.10.4)."
	    */
	    if (xml_xsd10_check_cvc_wildcard_namespace(type->attributeWildcard,
		    iattr->nsName) == 0) {
		/*
		* Handle processContents.
		*
		* SPEC (cvc-wildcard):
		* processContents | context-determined declaration:
		* "strict"          "mustFind"
		* "lax"             "none"
		* "skip"            "skip"
		*/
		if (type->attributeWildcard->processContents ==
		    XML_XSD10_SCHEMAS_ANY_SKIP) {
		     /*
		    * context-determined declaration = "skip"
		    *
		    * SPEC PSVI Assessment Outcome (Attribute)
		    * [validity] = "notKnown"
		    * [validation attempted] = "none"
		    */
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_WILD_SKIP;
		    continue;
		}
		/*
		* Find an attribute declaration.
		*/
		iattr->decl = xml_xsd10_get_attribute_decl(vctxt->schema,
		    iattr->localName, iattr->nsName);
		if (iattr->decl != NULL) {
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_ASSESSED;
		    /*
		    * SPEC (cvc-complex-type)
		    * (5) "Let [Definition:]  the wild IDs be the set of
		    * all attribute information item to which clause 3.2
		    * applied and whose `validation` resulted in a
		    * `context-determined declaration` of mustFind or no
		    * `context-determined declaration` at all, and whose
		    * [local name] and [namespace name] resolve (as
		    * defined by QName resolution (Instance) ($3.15.4)) to
		    * an attribute declaration whose {type definition} is
		    * or is derived from ID. Then all of the following
		    * must be true:"
		    */
		    iattr->typeDef = XSD10_WXS_ATTR_TYPEDEF(iattr->decl);
		    if (xml_xsd10_is_derived_from_built_in_type(
			iattr->typeDef, XML_XSD10_SCHEMAS_ID)) {
			/*
			* SPEC (5.1) "There must be no more than one
			* item in `wild IDs`."
			*/
			if (wildIDs != 0) {
			    /* VAL TODO */
			    iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_DUPLICATE_ID;
			    continue;
			}
			wildIDs++;
			/*
			* SPEC (cvc-complex-type)
			* (5.2) "If `wild IDs` is non-empty, there must not
			* be any attribute uses among the {attribute uses}
			* whose {attribute declaration}'s {type definition}
			* is or is derived from ID."
			*/
                        if (attrUseList != NULL) {
                            for (j = 0; j < attrUseList->nbItems; j++) {
                                if (xml_xsd10_is_derived_from_built_in_type(
                                    XSD10_WXS_ATTRUSE_TYPEDEF(attrUseList->items[j]),
                                    XML_XSD10_SCHEMAS_ID)) {
                                    /* URGENT VAL TODO: implement */
                            iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_AND_USE_ID;
                                    break;
                                }
                            }
                        }
		    }
		} else if (type->attributeWildcard->processContents ==
		    XML_XSD10_SCHEMAS_ANY_LAX) {
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL;
		    /*
		    * SPEC PSVI Assessment Outcome (Attribute)
		    * [validity] = "notKnown"
		    * [validation attempted] = "none"
		    */
		} else {
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL;
		}
	    }
	}
    }

    if (vctxt->nbAttrInfos == 0)
	return (0);

    /*
    * Get the owner element; needed for creation of default attributes.
    * This fixes bug #341337, reported by David Grohmann.
    */
    if (vctxt->options & XML_XSD10_SCHEMA_VAL_VC_I_CREATE) {
	XmlXsd10NodeInfo *ielem = vctxt->elemInfos[vctxt->depth];
	if (ielem && ielem->node && xml_node_dom(ielem->node))
	    defAttrOwnerElem = ielem->node;
    }
    /*
    * Validate values, create default attributes, evaluate IDCs.
    */
    for (i = 0; i < vctxt->nbAttrInfos; i++) {
	iattr = vctxt->attrInfos[i];
	/*
	* VAL TODO: Note that we won't try to resolve IDCs to
	* "lax" and "skip" validated attributes. Check what to
	* do in this case.
	*/
	if ((iattr->state != XML_XSD10_SCHEMAS_ATTR_ASSESSED) &&
	    (iattr->state != XML_XSD10_SCHEMAS_ATTR_DEFAULT))
	    continue;
	/*
	* VAL TODO: What to do if the type definition is missing?
	*/
	if (iattr->typeDef == NULL) {
	    iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE;
	    continue;
	}

	XSD10_ACTIVATE_ATTRIBUTE(iattr);
	fixed = 0;
	xpathRes = 0;

	if (vctxt->xpathStates != NULL) {
	    /*
	    * Evaluate IDCs.
	    */
	    xpathRes = xml_xsd10_x_path_evaluate(vctxt,
		XML_DOM_ATTRIBUTE);
	    if (xpathRes == -1) {
		XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
		    "calling xmlSchemaXPathEvaluate()");
		goto internal_error;
	    }
	}

	if (iattr->state == XML_XSD10_SCHEMAS_ATTR_DEFAULT) {
	    /*
	    * Default/fixed attributes.
	    * We need the value only if we need to resolve IDCs or
	    * will create default attributes.
	    */
	    if ((xpathRes) || (defAttrOwnerElem)) {
		if (iattr->use->defValue != NULL) {
		    iattr->value = (char *) iattr->use->defValue;
		    iattr->val = iattr->use->defVal;
		} else {
		    iattr->value = (char *) iattr->decl->defValue;
		    iattr->val = iattr->decl->defVal;
		}
		/*
		* IDCs will consume the precomputed default value,
		* so we need to clone it.
		*/
		if (iattr->val == NULL) {
		    XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
			"default/fixed value on an attribute use was "
			"not precomputed");
		    goto internal_error;
		}
		iattr->val = xml_xsd10_copy_value(iattr->val);
		if (iattr->val == NULL) {
		    XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
			"calling xmlSchemaCopyValue()");
		    goto internal_error;
		}
	    }
	    /*
	    * PSVI: Add the default attribute to the current element.
	    * VAL TODO: Should we use the *normalized* value? This currently
	    *   uses the *initial* value.
	    */

	    if (defAttrOwnerElem) {
		char *normValue;
		const char *value;

		value = iattr->value;
		/*
		* Normalize the value.
		*/
		normValue = xml_xsd10_normalize_value(iattr->typeDef,
		    iattr->value);
		if (normValue != NULL)
		    value = normValue;

		if (iattr->nsName == NULL) {
		    if (xml_node_append_attr(defAttrOwnerElem,
			iattr->localName, NULL, value) == NULL) {
			XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
			    "calling xmlNewProp()");
			if (normValue != NULL)
			    free(normValue);
			goto internal_error;
		    }
		} else {
		    XmlNs *ns;

		    ns = xml_xsd10_tree_search_ns_by_href(
			defAttrOwnerElem, iattr->nsName);
		    if (ns == NULL) {
			char prefix[13];
			int counter = 0;

			/*
			* Create a namespace declaration on the validation
			* root node if no namespace declaration is in scope.
			*/
			do {
			    snprintf((char *) prefix, 13, "p%d", counter++);
			    ns = xml_xsd10_tree_search_ns(
				defAttrOwnerElem, prefix);
			    if (counter > 1000) {
				XSD10_VERROR_INT(
				    "xmlSchemaVAttributesComplex",
				    "could not compute a ns prefix for a "
				    "default/fixed attribute");
				if (normValue != NULL)
				    free(normValue);
				goto internal_error;
			    }
			} while (ns != NULL);
			ns = xml_ns_declare(vctxt->validationRoot,
			    prefix, iattr->nsName);
		    }
		    /*
		    * TODO:
		    * http://lists.w3.org/Archives/Public/www-xml-schema-comments/2005JulSep/0406.html
		    * If we have QNames: do we need to ensure there's a
		    * prefix defined for the QName?
		    */
		    xml_node_append_attr(defAttrOwnerElem, iattr->localName, ns, value);
		}
		if (normValue != NULL)
		    free(normValue);
	    }
	    /*
	    * Go directly to IDC evaluation.
	    */
	    goto eval_idcs;
	}
	/*
	* Validate the value.
	*/
	if (vctxt->value != NULL) {
	    /*
	    * Free last computed value; just for safety reasons.
	    */
	    xml_xsd10_free_value(vctxt->value);
	    vctxt->value = NULL;
	}
	/*
	* Note that the attribute *use* can be unavailable, if
	* the attribute was a wild attribute.
	*/
	if ((iattr->decl->flags & XML_XSD10_SCHEMAS_ATTR_FIXED) ||
	    ((iattr->use != NULL) &&
	     (iattr->use->flags & XML_XSD10_SCHEMAS_ATTR_FIXED)))
	    fixed = 1;
	else
	    fixed = 0;
	/*
	* SPEC (cvc-attribute)
	* (3) "The item's `normalized value` must be locally `valid`
	* with respect to that {type definition} as per
	* String Valid ($3.14.4)."
	*
	* VAL TODO: Do we already have the
	* "normalized attribute value" here?
	*/
	if (xpathRes || fixed) {
	    iattr->flags |= XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED;
	    /*
	    * Request a computed value.
	    */
	    res = xml_xsd10_v_check_cvc_simple_type(
		XSD10_ACTXT_CAST vctxt,
		iattr->node, iattr->typeDef, iattr->value, &(iattr->val),
		1, 1, 0);
	} else {
	    res = xml_xsd10_v_check_cvc_simple_type(
		XSD10_ACTXT_CAST vctxt,
		iattr->node, iattr->typeDef, iattr->value, NULL,
		1, 0, 0);
	}

	if (res != 0) {
	    if (res == -1) {
		XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
		    "calling xmlSchemaStreamValidateSimpleTypeValue()");
		goto internal_error;
	    }
	    iattr->state = XML_XSD10_SCHEMAS_ATTR_INVALID_VALUE;
	    /*
	    * SPEC PSVI Assessment Outcome (Attribute)
	    * [validity] = "invalid"
	    */
	    goto eval_idcs;
	}

	if (fixed) {
	    /*
	    * SPEC Attribute Locally Valid (Use) (cvc-au)
	    * "For an attribute information item to be `valid`
	    * with respect to an attribute use its *normalized*
	    * value must match the *canonical* lexical
	    * representation of the attribute use's {value
	    * constraint}value, if it is present and fixed."
	    *
	    * VAL TODO: The requirement for the *canonical* value
	    * will be removed in XML Schema 1.1.
	    */
	    /*
	    * SPEC Attribute Locally Valid (cvc-attribute)
	    * (4) "The item's *actual* value must match the *value* of
	    * the {value constraint}, if it is present and fixed."
	    */
	    if (iattr->val == NULL) {
		/* VAL TODO: A value was not precomputed. */
		goto eval_idcs;
	    }
	    if ((iattr->use != NULL) &&
		(iattr->use->defValue != NULL)) {
		if (iattr->use->defVal == NULL) {
		    /* VAL TODO: A default value was not precomputed. */
		    goto eval_idcs;
		}
		iattr->vcValue = iattr->use->defValue;
		/*
		if (xmlSchemaCompareValuesWhtsp(attr->val,
		    (xmlSchemaWhitespaceValueType) ws,
		    attr->use->defVal,
		    (xmlSchemaWhitespaceValueType) ws) != 0) {
		*/
		if (! xml_xsd10_are_values_equal(iattr->val, iattr->use->defVal))
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE;
	    } else {
		if (iattr->decl->defVal == NULL) {
		    /* VAL TODO: A default value was not precomputed. */
		    goto eval_idcs;
		}
		iattr->vcValue = iattr->decl->defValue;
		/*
		if (xmlSchemaCompareValuesWhtsp(attr->val,
		    (xmlSchemaWhitespaceValueType) ws,
		    attrDecl->defVal,
		    (xmlSchemaWhitespaceValueType) ws) != 0) {
		*/
		if (! xml_xsd10_are_values_equal(iattr->val, iattr->decl->defVal))
		    iattr->state = XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE;
	    }
	    /*
	    * [validity] = "valid"
	    */
	}
eval_idcs:
	/*
	* Evaluate IDCs.
	*/
	if (xpathRes) {
	    if (xml_xsd10_x_path_process_history(vctxt,
		vctxt->depth +1) == -1) {
		XSD10_VERROR_INT("xmlSchemaVAttributesComplex",
		    "calling xmlSchemaXPathEvaluate()");
		goto internal_error;
	    }
	} else if (vctxt->xpathStates != NULL)
	    xml_xsd10_x_path_pop(vctxt);
    }

    /*
    * Report errors.
    */
    for (i = 0; i < vctxt->nbAttrInfos; i++) {
	iattr = vctxt->attrInfos[i];
	if ((iattr->state == XML_XSD10_SCHEMAS_ATTR_META) ||
	    (iattr->state == XML_XSD10_SCHEMAS_ATTR_ASSESSED) ||
	    (iattr->state == XML_XSD10_SCHEMAS_ATTR_WILD_SKIP) ||
	    (iattr->state == XML_XSD10_SCHEMAS_ATTR_WILD_LAX_NO_DECL))
	    continue;
	XSD10_ACTIVATE_ATTRIBUTE(iattr);
	switch (iattr->state) {
	    case XML_XSD10_SCHEMAS_ATTR_ERR_MISSING: {
		    char *str = NULL;
		    XSD10_ACTIVATE_ELEM;
		    xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
			XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_4, NULL, NULL,
			"The attribute '%s' is required but missing",
			xml_xsd10_format_q_name(&str,
			    iattr->decl->targetNamespace,
			    iattr->decl->name),
			NULL);
		    XSD10_FREE_AND_NULL(str)
		    break;
		}
	    case XML_XSD10_SCHEMAS_ATTR_ERR_NO_TYPE:
		XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ATTRIBUTE_2, NULL,
		    "The type definition is absent");
		break;
	    case XML_XSD10_SCHEMAS_ATTR_ERR_FIXED_VALUE:
		xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_CVC_AU, NULL, NULL,
		    "The value '%s' does not match the fixed "
		    "value constraint '%s'",
		    iattr->value, iattr->vcValue);
		break;
	    case XML_XSD10_SCHEMAS_ATTR_ERR_WILD_STRICT_NO_DECL:
		XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_WILDCARD, NULL,
		    "No matching global attribute declaration available, but "
		    "demanded by the strict wildcard");
		break;
	    case XML_XSD10_SCHEMAS_ATTR_UNKNOWN:
		if (iattr->metaType)
		    break;
		/*
		* MAYBE VAL TODO: One might report different error messages
		* for the following errors.
		*/
		if (type->attributeWildcard == NULL) {
		    xml_xsd10_illegal_attr_err(XSD10_ACTXT_CAST vctxt,
			XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_1, iattr, NULL);
		} else {
		    xml_xsd10_illegal_attr_err(XSD10_ACTXT_CAST vctxt,
			XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_3_2_2, iattr, NULL);
		}
		break;
	    default:
		break;
	}
    }

    XSD10_ACTIVATE_ELEM;
    return (0);
internal_error:
    XSD10_ACTIVATE_ELEM;
    return (-1);
}

/** Port of xmlSchemaValidateElemWildcard. */
int xml_xsd10_validate_elem_wildcard(XmlXsd10ValidCtxt *vctxt, int *skip)
{
    XmlXsd10Wildcard *wild = (XmlXsd10Wildcard *) vctxt->inode->decl;
    /*
    * The namespace of the element was already identified to be
    * matching the wildcard.
    */
    if ((skip == NULL) || (wild == NULL) ||
	(wild->type != XML_XSD10_SCHEMA_TYPE_ANY)) {
	XSD10_VERROR_INT("xmlSchemaValidateElemWildcard",
	    "bad arguments");
	return (-1);
    }
    *skip = 0;
    if (wild->processContents == XML_XSD10_SCHEMAS_ANY_SKIP) {
	/*
	* URGENT VAL TODO: Either we need to position the stream to the
	* next sibling, or walk the whole subtree.
	*/
	*skip = 1;
	return (0);
    }
    {
	XmlXsd10Element *decl = NULL;

	decl = xml_xsd10_get_elem(vctxt->schema,
	    vctxt->inode->localName, vctxt->inode->nsName);
	if (decl != NULL) {
	    vctxt->inode->decl = decl;
	    return (0);
	}
    }
    if (wild->processContents == XML_XSD10_SCHEMAS_ANY_STRICT) {
	/* VAL TODO: Change to proper error code. */
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_1, NULL, /* WXS_BASIC_CAST wild */
	    "No matching global element declaration available, but "
	    "demanded by the strict wildcard");
	return (vctxt->err);
    }
    if (vctxt->nbAttrInfos != 0) {
	XmlXsd10AttrInfo *iattr;
	/*
	* SPEC Validation Rule: Schema-Validity Assessment (Element)
	* (1.2.1.2.1) - (1.2.1.2.3 )
	*
	* Use the xsi:type attribute for the type definition.
	*/
	iattr = xml_xsd10_get_meta_attr_info(vctxt,
	    XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
	if (iattr != NULL) {
	    if (xml_xsd10_process_xsi_type(vctxt, iattr,
		&(vctxt->inode->typeDef), NULL) == -1) {
		XSD10_VERROR_INT("xmlSchemaValidateElemWildcard",
		    "calling xmlSchemaProcessXSIType() to "
		    "process the attribute 'xsi:nil'");
		return (-1);
	    }
	    /*
	    * Don't return an error on purpose.
	    */
	    return (0);
	}
    }
    /*
    * SPEC Validation Rule: Schema-Validity Assessment (Element)
    *
    * Fallback to "anyType".
    */
    vctxt->inode->typeDef =
	xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYTYPE);
    return (0);
}

/*
* xmlSchemaCheckCOSValidDefault:
*
* This will be called if: not nilled, no content and a default/fixed
* value is provided.
*/

/** Port of xmlSchemaCheckCOSValidDefault. */
int xml_xsd10_check_cos_valid_default(XmlXsd10ValidCtxt *vctxt, const char *value, XmlXsd10Val **val)
{
    int ret = 0;
    XmlXsd10NodeInfo *inode = vctxt->inode;

    /*
    * cos-valid-default:
    * Schema Component Constraint: Element Default Valid (Immediate)
    * For a string to be a valid default with respect to a type
    * definition the appropriate case among the following must be true:
    */
    if XSD10_WXS_IS_COMPLEX(inode->typeDef) {
	/*
	* Complex type.
	*
	* SPEC (2.1) "its {content type} must be a simple type definition
	* or mixed."
	* SPEC (2.2.2) "If the {content type} is mixed, then the {content
	* type}'s particle must be `emptiable` as defined by
	* Particle Emptiable ($3.9.6)."
	*/
	if ((! XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) &&
	    ((! XSD10_WXS_HAS_MIXED_CONTENT(inode->typeDef)) ||
	     (! XSD10_WXS_EMPTIABLE(inode->typeDef)))) {
	    ret = XML_XSD10_SCHEMAP_COS_VALID_DEFAULT_2_1;
	    /* NOTE that this covers (2.2.2) as well. */
	    XSD10_VERROR(ret, NULL,
		"For a string to be a valid default, the type definition "
		"must be a simple type or a complex type with simple content "
		"or mixed content and a particle emptiable");
	    return(ret);
	}
    }
    /*
    * 1 If the type definition is a simple type definition, then the string
    * must be `valid` with respect to that definition as defined by String
    * Valid ($3.14.4).
    *
    * AND
    *
    * 2.2.1 If the {content type} is a simple type definition, then the
    * string must be `valid` with respect to that simple type definition
    * as defined by String Valid ($3.14.4).
    */
    if (XSD10_WXS_IS_SIMPLE(inode->typeDef)) {

	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST vctxt,
	    NULL, inode->typeDef, value, val, 1, 1, 0);

    } else if (XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) {

	ret = xml_xsd10_v_check_cvc_simple_type(XSD10_ACTXT_CAST vctxt,
	    NULL, inode->typeDef->contentTypeDef, value, val, 1, 1, 0);
    }
    if (ret < 0) {
	XSD10_VERROR_INT("xmlSchemaCheckCOSValidDefault",
	    "calling xmlSchemaVCheckCVCSimpleType()");
    }
    return (ret);
}

/** Port of xmlSchemaVContentModelCallback. */
void xml_xsd10_v_content_model_callback(void *exec, const char *name, void *transdata, void *inputdata)
{
    (void) exec;
    (void) name;
    XmlXsd10Element *item = (XmlXsd10Element *) transdata;
    XmlXsd10NodeInfo *inode = (XmlXsd10NodeInfo *) inputdata;
    inode->decl = item;
}

/** Port of xmlSchemaValidatorPushElem. */
int xml_xsd10_validator_push_elem(XmlXsd10ValidCtxt *vctxt)
{
    vctxt->inode = xml_xsd10_get_fresh_elem_info(vctxt);
    if (vctxt->inode == NULL) {
	XSD10_VERROR_INT("xmlSchemaValidatorPushElem",
	    "calling xmlSchemaGetFreshElemInfo()");
	return (-1);
    }
    vctxt->nbAttrInfos = 0;
    return (0);
}

/** Port of xmlSchemaVCheckINodeDataType. */
int xml_xsd10_v_check_i_node_data_type(XmlXsd10ValidCtxt *vctxt, XmlXsd10NodeInfo *inode, XmlXsd10Type *type, const char *value)
{
    if (inode->flags & XML_XSD10_SCHEMA_NODE_INFO_VALUE_NEEDED)
	return (xml_xsd10_v_check_cvc_simple_type(
	    XSD10_ACTXT_CAST vctxt, NULL,
	    type, value, &(inode->val), 1, 1, 0));
    else
	return (xml_xsd10_v_check_cvc_simple_type(
	    XSD10_ACTXT_CAST vctxt, NULL,
	    type, value, NULL, 1, 0, 0));
}



/*
* Process END of element.
*/
/** Port of xmlSchemaValidatorPopElem. */
int xml_xsd10_validator_pop_elem(XmlXsd10ValidCtxt *vctxt)
{
    int ret = 0;
    XmlXsd10NodeInfo *inode = vctxt->inode;

    if (vctxt->nbAttrInfos != 0)
	xml_xsd10_clear_attr_infos(vctxt);
    if (inode->flags & XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED) {
	/*
	* This element was not expected;
	* we will not validate child elements of broken parents.
	* Skip validation of all content of the parent.
	*/
	vctxt->skipDepth = vctxt->depth -1;
	goto end_elem;
    }
    if ((inode->typeDef == NULL) ||
	(inode->flags & XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE)) {
	/*
	* 1. the type definition might be missing if the element was
	*    error prone
	* 2. it might be abstract.
	*/
	goto end_elem;
    }
    /*
    * Check the content model.
    */
    if ((inode->typeDef->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) ||
	(inode->typeDef->contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)) {

	/*
	* Workaround for "anyType".
	*/
	if (inode->typeDef->builtInType == XML_XSD10_SCHEMAS_ANYTYPE)
	    goto character_content;

	if ((inode->flags & XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT) == 0) {
	    char *values[10];
	    int terminal, nbval = 10, nbneg;

	    if (inode->regexCtxt == NULL) {
		/*
		* Create the regex context.
		*/
		inode->regexCtxt =
		    xml_reg_exec_new((XmlRegexp *) inode->typeDef->contModel,
		    xml_xsd10_v_content_model_callback, vctxt);
		if (inode->regexCtxt == NULL) {
		    XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
			"failed to create a regex context");
		    goto internal_error;
		}
	    }

	    /*
	     * Do not check further content if the node has been nilled
	     */
	    if (XSD10_INODE_NILLED(inode)) {
		ret = 0;
                goto skip_nilled;
	    }

	    /*
	    * Get hold of the still expected content, since a further
	    * call to xmlRegExecPushString() will lose this information.
	    */
	    xml_reg_exec_next_values(inode->regexCtxt,
		&nbval, &nbneg, &values[0], &terminal);
	    ret = xml_reg_exec_push_string(inode->regexCtxt, NULL, NULL);
	    if ((ret<0) || ((ret==0) && (!XSD10_INODE_NILLED(inode)))) {
		/*
		* Still missing something.
		*/
		ret = 1;
		inode->flags |=
		    XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT;
		xml_xsd10_complex_type_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_ELEMENT_CONTENT, NULL, NULL,
		    "Missing child element(s)",
		    nbval, nbneg, values);
	    } else {
		/*
		* Content model is satisfied.
		*/
		ret = 0;
	    }

	}
    }

skip_nilled:

    if (inode->typeDef->contentType == XML_XSD10_SCHEMA_CONTENT_ELEMENTS)
	goto end_elem;

character_content:

    if (vctxt->value != NULL) {
	xml_xsd10_free_value(vctxt->value);
	vctxt->value = NULL;
    }
    /*
    * Check character content.
    */
    if (inode->decl == NULL) {
	/*
	* Speedup if no declaration exists.
	*/
	if (XSD10_WXS_IS_SIMPLE(inode->typeDef)) {
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		inode, inode->typeDef, inode->value);
	} else if (XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) {
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		inode, inode->typeDef->contentTypeDef,
		inode->value);
	}
	if (ret < 0) {
	    XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
		"calling xmlSchemaVCheckCVCSimpleType()");
	    goto internal_error;
	}
	goto end_elem;
    }
    /*
    * cvc-elt (3.3.4) : 5
    * The appropriate case among the following must be true:
    */
    /*
    * cvc-elt (3.3.4) : 5.1
    * If the declaration has a {value constraint},
    * the item has neither element nor character [children] and
    * clause 3.2 has not applied, then all of the following must be true:
    */
    if ((inode->decl->value != NULL) &&
	(inode->flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY) &&
	(! XSD10_INODE_NILLED(inode))) {
	/*
	* cvc-elt (3.3.4) : 5.1.1
	* If the `actual type definition` is a `local type definition`
	* then the canonical lexical representation of the {value constraint}
	* value must be a valid default for the `actual type definition` as
	* defined in Element Default Valid (Immediate) ($3.3.6).
	*/
	/*
	* NOTE: 'local' above means types acquired by xsi:type.
	* NOTE: Although the *canonical* value is stated, it is not
	* relevant if canonical or not. Additionally XML Schema 1.1
	* will removed this requirement as well.
	*/
	if (inode->flags & XML_XSD10_SCHEMA_ELEM_INFO_LOCAL_TYPE) {

	    ret = xml_xsd10_check_cos_valid_default(vctxt,
		inode->decl->value, &(inode->val));
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
			"calling xmlSchemaCheckCOSValidDefault()");
		    goto internal_error;
		}
		goto end_elem;
	    }
	    /*
	    * Stop here, to avoid redundant validation of the value
	    * (see following).
	    */
	    goto default_psvi;
	}
	/*
	* cvc-elt (3.3.4) : 5.1.2
	* The element information item with the canonical lexical
	* representation of the {value constraint} value used as its
	* `normalized value` must be `valid` with respect to the
	* `actual type definition` as defined by Element Locally Valid (Type)
	* ($3.3.4).
	*/
	if (XSD10_WXS_IS_SIMPLE(inode->typeDef)) {
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		inode, inode->typeDef, inode->decl->value);
	} else if (XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) {
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		inode, inode->typeDef->contentTypeDef,
		inode->decl->value);
	}
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
		    "calling xmlSchemaVCheckCVCSimpleType()");
		goto internal_error;
	    }
	    goto end_elem;
	}

default_psvi:
	/*
	* PSVI: Create a text node on the instance element.
	*/
	if ((vctxt->options & XML_XSD10_SCHEMA_VAL_VC_I_CREATE) &&
	    (inode->node != NULL)) {
	    XMQNode *textChild;
	    char *normValue;
	    /*
	    * VAL TODO: Normalize the value.
	    */
	    normValue = xml_xsd10_normalize_value(inode->typeDef,
		inode->decl->value);
	    if (normValue != NULL) {
		textChild = xml_node_new_text(xml_node_dom(inode->node),
                        normValue);
		free(normValue);
	    } else
		textChild = xml_node_new_text(xml_node_dom(inode->node),
                        inode->decl->value);
	    if (textChild == NULL) {
		XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
		    "calling xmlNewDocText()");
		goto internal_error;
	    } else
		xml_node_add_child(inode->node, textChild);
	}

    } else if (! XSD10_INODE_NILLED(inode)) {
	/*
	* 5.2.1 The element information item must be `valid` with respect
	* to the `actual type definition` as defined by Element Locally
	* Valid (Type) ($3.3.4).
	*/
	if (XSD10_WXS_IS_SIMPLE(inode->typeDef)) {
	     /*
	    * SPEC (cvc-type) (3.1)
	    * "If the type definition is a simple type definition, ..."
	    * (3.1.3) "If clause 3.2 of Element Locally Valid
	    * (Element) ($3.3.4) did not apply, then the `normalized value`
	    * must be `valid` with respect to the type definition as defined
	    * by String Valid ($3.14.4).
	    */
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		    inode, inode->typeDef, inode->value);
	} else if (XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) {
	    /*
	    * SPEC (cvc-type) (3.2) "If the type definition is a complex type
	    * definition, then the element information item must be
	    * `valid` with respect to the type definition as per
	    * Element Locally Valid (Complex Type) ($3.4.4);"
	    *
	    * SPEC (cvc-complex-type) (2.2)
	    * "If the {content type} is a simple type definition, ...
	    * the `normalized value` of the element information item is
	    * `valid` with respect to that simple type definition as
	    * defined by String Valid ($3.14.4)."
	    */
	    ret = xml_xsd10_v_check_i_node_data_type(vctxt,
		inode, inode->typeDef->contentTypeDef, inode->value);
	}
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidatorPopElem",
		    "calling xmlSchemaVCheckCVCSimpleType()");
		goto internal_error;
	    }
	    goto end_elem;
	}
	/*
	* 5.2.2 If there is a fixed {value constraint} and clause 3.2 has
	* not applied, all of the following must be true:
	*/
	if ((inode->decl->value != NULL) &&
	    (inode->decl->flags & XML_XSD10_SCHEMAS_ELEM_FIXED)) {

	    /*
	    * TODO: We will need a computed value, when comparison is
	    * done on computed values.
	    */
	    /*
	    * 5.2.2.1 The element information item must have no element
	    * information item [children].
	    */
	    if (inode->flags &
		    XML_XSD10_SCHEMA_ELEM_INFO_HAS_ELEM_CONTENT) {
		ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_1;
		XSD10_VERROR(ret, NULL,
		    "The content must not contain element nodes since "
		    "there is a fixed value constraint");
		goto end_elem;
	    } else {
		/*
		* 5.2.2.2 The appropriate case among the following must
		* be true:
		*/
		if (XSD10_WXS_HAS_MIXED_CONTENT(inode->typeDef)) {
		    /*
		    * 5.2.2.2.1 If the {content type} of the `actual type
		    * definition` is mixed, then the *initial value* of the
		    * item must match the canonical lexical representation
		    * of the {value constraint} value.
		    *
		    * ... the *initial value* of an element information
		    * item is the string composed of, in order, the
		    * [character code] of each character information item in
		    * the [children] of that element information item.
		    */
		    if (! xml_xsd10_str_equal(inode->value, inode->decl->value)){
			/*
			* VAL TODO: Report invalid & expected values as well.
			* VAL TODO: Implement the canonical stuff.
			*/
			ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_1;
			xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
			    ret, NULL, NULL,
			    "The initial value '%s' does not match the fixed "
			    "value constraint '%s'",
			    inode->value, inode->decl->value);
			goto end_elem;
		    }
		} else if (XSD10_WXS_HAS_SIMPLE_CONTENT(inode->typeDef)) {
		    /*
		    * 5.2.2.2.2 If the {content type} of the `actual type
		    * definition` is a simple type definition, then the
		    **actual value* of the item must match the canonical
		    * lexical representation of the {value constraint} value.
		    */
		    /*
		    * VAL TODO: *actual value* is the normalized value, impl.
		    *           this.
		    * VAL TODO: Report invalid & expected values as well.
		    * VAL TODO: Implement a comparison with the computed values.
		    */
		    if (! xml_xsd10_str_equal(inode->value,
			    inode->decl->value)) {
			ret = XML_XSD10_SCHEMAV_CVC_ELT_5_2_2_2_2;
			xml_xsd10_custom_err(XSD10_ACTXT_CAST vctxt,
			    ret, NULL, NULL,
			    "The actual value '%s' does not match the fixed "
			    "value constraint '%s'",
			    inode->value,
			    inode->decl->value);
			goto end_elem;
		    }
		}
	    }
	}
    }

end_elem:
    if (vctxt->depth < 0) {
	/* TODO: raise error? */
	return (0);
    }
    if (vctxt->depth == vctxt->skipDepth)
	vctxt->skipDepth = -1;
    /*
    * Evaluate the history of XPath state objects.
    */
    if (inode->appliedXPath &&
	(xml_xsd10_x_path_process_history(vctxt, vctxt->depth) == -1))
	goto internal_error;
    /*
    * MAYBE TODO:
    * SPEC (6) "The element information item must be `valid` with
    * respect to each of the {identity-constraint definitions} as per
    * Identity-constraint Satisfied ($3.11.4)."
    */
    /*
    * PSVI TODO: If we expose IDC node-tables via PSVI then the tables
    *   need to be built in any case.
    *   We will currently build IDC node-tables and bubble them only if
    *   keyrefs do exist.
    */

    /*
    * Add the current IDC target-nodes to the IDC node-tables.
    */
    if ((inode->idcMatchers != NULL) &&
	(vctxt->hasKeyrefs || vctxt->createIDCNodeTables))
    {
	if (xml_xsd10_idc_fill_node_tables(vctxt, inode) == -1)
	    goto internal_error;
    }
    /*
    * Validate IDC keyrefs.
    */
    if (vctxt->inode->hasKeyrefs)
	if (xml_xsd10_check_cvcidc_key_ref(vctxt) == -1)
	    goto internal_error;
    /*
    * Merge/free the IDC table.
    */
    if (inode->idcTable != NULL) {
	if ((vctxt->depth > 0) &&
	    (vctxt->hasKeyrefs || vctxt->createIDCNodeTables))
	{
	    /*
	    * Merge the IDC node table with the table of the parent node.
	    */
	    if (xml_xsd10_bubble_idc_node_tables(vctxt) == -1)
		goto internal_error;
	}
    }
    /*
    * Clear the current ielem.
    * VAL TODO: Don't free the PSVI IDC tables if they are
    * requested for the PSVI.
    */
    xml_xsd10_clear_elem_info(vctxt, inode);
    /*
    * Skip further processing if we are on the validation root.
    */
    if (vctxt->depth == 0) {
	vctxt->depth--;
	vctxt->inode = NULL;
	return (0);
    }
    /*
    * Reset the keyrefDepth if needed.
    */
    if (vctxt->aidcs != NULL) {
	XmlXsd10IDCAug *aidc = vctxt->aidcs;
	do {
	    if (aidc->keyrefDepth == vctxt->depth) {
		/*
		* A 'keyrefDepth' of a key/unique IDC matches the current
		* depth, this means that we are leaving the scope of the
		* top-most keyref IDC which refers to this IDC.
		*/
		aidc->keyrefDepth = -1;
	    }
	    aidc = aidc->next;
	} while (aidc != NULL);
    }
    vctxt->depth--;
    vctxt->inode = vctxt->elemInfos[vctxt->depth];
    /*
    * VAL TODO: 7 If the element information item is the `validation root`, it must be
    * `valid` per Validation Root Valid (ID/IDREF) ($3.3.4).
    */
    return (ret);

internal_error:
    vctxt->err = -1;
    return (-1);
}

/*
* 3.4.4 Complex Type Definition Validation Rules
* Validation Rule: Element Locally Valid (Complex Type) (cvc-complex-type)
*/
/** Port of xmlSchemaValidateChildElem. */
int xml_xsd10_validate_child_elem(XmlXsd10ValidCtxt *vctxt)
{
    XmlXsd10NodeInfo *pielem;
    XmlXsd10Type *ptype;
    int ret = 0;

    if (vctxt->depth <= 0) {
	XSD10_VERROR_INT("xmlSchemaValidateChildElem",
	    "not intended for the validation root");
	return (-1);
    }
    pielem = vctxt->elemInfos[vctxt->depth -1];
    if (pielem->flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY)
	pielem->flags ^= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
    /*
    * Handle 'nilled' elements.
    */
    if (XSD10_INODE_NILLED(pielem)) {
	/*
	* SPEC (cvc-elt) (3.3.4) : (3.2.1)
	*/
	XSD10_ACTIVATE_PARENT_ELEM;
	ret = XML_XSD10_SCHEMAV_CVC_ELT_3_2_1;
	XSD10_VERROR(ret, NULL,
	    "Neither character nor element content is allowed, "
	    "because the element was 'nilled'");
	XSD10_ACTIVATE_ELEM;
	goto unexpected_elem;
    }

    ptype = pielem->typeDef;

    if (ptype->builtInType == XML_XSD10_SCHEMAS_ANYTYPE) {
	/*
	* Workaround for "anyType": we have currently no content model
	* assigned for "anyType", so handle it explicitly.
	* "anyType" has an unbounded, lax "any" wildcard.
	*/
	vctxt->inode->decl = xml_xsd10_get_elem(vctxt->schema,
	    vctxt->inode->localName,
	    vctxt->inode->nsName);

	if (vctxt->inode->decl == NULL) {
	    XmlXsd10AttrInfo *iattr;
	    /*
	    * Process "xsi:type".
	    * SPEC (cvc-assess-elt) (1.2.1.2.1) - (1.2.1.2.3)
	    */
	    iattr = xml_xsd10_get_meta_attr_info(vctxt,
		XML_XSD10_SCHEMA_ATTR_INFO_META_XSI_TYPE);
	    if (iattr != NULL) {
		ret = xml_xsd10_process_xsi_type(vctxt, iattr,
		    &(vctxt->inode->typeDef), NULL);
		if (ret != 0) {
		    if (ret == -1) {
			XSD10_VERROR_INT("xmlSchemaValidateChildElem",
			    "calling xmlSchemaProcessXSIType() to "
			    "process the attribute 'xsi:nil'");
			return (-1);
		    }
		    return (ret);
		}
	    } else {
		 /*
		 * Fallback to "anyType".
		 *
		 * SPEC (cvc-assess-elt)
		 * "If the item cannot be `strictly assessed`, [...]
		 * an element information item's schema validity may be laxly
		 * assessed if its `context-determined declaration` is not
		 * skip by `validating` with respect to the `ur-type
		 * definition` as per Element Locally Valid (Type) ($3.3.4)."
		*/
		vctxt->inode->typeDef =
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_ANYTYPE);
	    }
	}
	return (0);
    }

    switch (ptype->contentType) {
	case XML_XSD10_SCHEMA_CONTENT_EMPTY:
	    /*
	    * SPEC (2.1) "If the {content type} is empty, then the
	    * element information item has no character or element
	    * information item [children]."
	    */
	    XSD10_ACTIVATE_PARENT_ELEM
	    ret = XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1;
	    XSD10_VERROR(ret, NULL,
		"Element content is not allowed, "
		"because the content type is empty");
	    XSD10_ACTIVATE_ELEM
	    goto unexpected_elem;
	    break;

	case XML_XSD10_SCHEMA_CONTENT_MIXED:
        case XML_XSD10_SCHEMA_CONTENT_ELEMENTS: {
	    XmlRegExecCtxt *regexCtxt;
	    char *values[10];
	    int terminal, nbval = 10, nbneg;

	    /* VAL TODO: Optimized "anyType" validation.*/

	    if (ptype->contModel == NULL) {
		XSD10_VERROR_INT("xmlSchemaValidateChildElem",
		    "type has elem content but no content model");
		return (-1);
	    }
	    /*
	    * Safety belt for evaluation if the cont. model was already
	    * examined to be invalid.
	    */
	    if (pielem->flags & XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT) {
		XSD10_VERROR_INT("xmlSchemaValidateChildElem",
		    "validating elem, but elem content is already invalid");
		return (-1);
	    }

	    regexCtxt = pielem->regexCtxt;
	    if (regexCtxt == NULL) {
		/*
		* Create the regex context.
		*/
		regexCtxt = xml_reg_exec_new((XmlRegexp *) ptype->contModel,
		    xml_xsd10_v_content_model_callback, vctxt);
		if (regexCtxt == NULL) {
		    XSD10_VERROR_INT("xmlSchemaValidateChildElem",
			"failed to create a regex context");
		    return (-1);
		}
		pielem->regexCtxt = regexCtxt;
	    }

	    /*
	    * SPEC (2.4) "If the {content type} is element-only or mixed,
	    * then the sequence of the element information item's
	    * element information item [children], if any, taken in
	    * order, is `valid` with respect to the {content type}'s
	    * particle, as defined in Element Sequence Locally Valid
	    * (Particle) ($3.9.4)."
	    */
	    ret = xml_reg_exec_push_string2(regexCtxt,
		vctxt->inode->localName,
		vctxt->inode->nsName,
		vctxt->inode);
	    if (vctxt->err == XML_XSD10_SCHEMAV_INTERNAL) {
		XSD10_VERROR_INT("xmlSchemaValidateChildElem",
		    "calling xmlRegExecPushString2()");
		return (-1);
	    }
	    if (ret < 0) {
		xml_reg_exec_err_info(regexCtxt, NULL, &nbval, &nbneg,
		    &values[0], &terminal);
		xml_xsd10_complex_type_err(XSD10_ACTXT_CAST vctxt,
		    XML_XSD10_SCHEMAV_ELEMENT_CONTENT, NULL,NULL,
		    "This element is not expected",
		    nbval, nbneg, values);
		ret = vctxt->err;
		goto unexpected_elem;
	    } else
		ret = 0;
	}
	    break;
	case XML_XSD10_SCHEMA_CONTENT_SIMPLE:
	case XML_XSD10_SCHEMA_CONTENT_BASIC:
	    XSD10_ACTIVATE_PARENT_ELEM
	    if (XSD10_WXS_IS_COMPLEX(ptype)) {
		/*
		* SPEC (cvc-complex-type) (2.2)
		* "If the {content type} is a simple type definition, then
		* the element information item has no element information
		* item [children], ..."
		*/
		ret = XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_2;
		XSD10_VERROR(ret, NULL, "Element content is not allowed, "
		    "because the content type is a simple type definition");
	    } else {
		/*
		* SPEC (cvc-type) (3.1.2) "The element information item must
		* have no element information item [children]."
		*/
		ret = XML_XSD10_SCHEMAV_CVC_TYPE_3_1_2;
		XSD10_VERROR(ret, NULL, "Element content is not allowed, "
		    "because the type definition is simple");
	    }
	    XSD10_ACTIVATE_ELEM
	    ret = vctxt->err;
	    goto unexpected_elem;
	    break;

	default:
	    break;
    }
    return (ret);
unexpected_elem:
    /*
    * Pop this element and set the skipDepth to skip
    * all further content of the parent element.
    */
    vctxt->skipDepth = vctxt->depth;
    vctxt->inode->flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_NOT_EXPECTED;
    pielem->flags |= XML_XSD10_SCHEMA_ELEM_INFO_ERR_BAD_CONTENT;
    return (ret);
}

#define XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST 1
#define XML_XSD10_SCHEMA_PUSH_TEXT_CREATED 2
#define XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE 3

/** Port of xmlSchemaVPushText. */
int xml_xsd10_v_push_text(XmlXsd10ValidCtxt *vctxt, int nodeType, const char *value, int len, int mode, int *consumed)
{
    /*
    * Unfortunately we have to duplicate the text sometimes.
    * OPTIMIZE: Maybe we could skip it, if:
    *   1. content type is simple
    *   2. whitespace is "collapse"
    *   3. it consists of whitespace only
    *
    * Process character content.
    */
    if (consumed != NULL)
	*consumed = 0;
    if (XSD10_INODE_NILLED(vctxt->inode)) {
	/*
	* SPEC cvc-elt (3.3.4 - 3.2.1)
	* "The element information item must have no character or
	* element information item [children]."
	*/
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_ELT_3_2_1, NULL,
	    "Neither character nor element content is allowed "
	    "because the element is 'nilled'");
	return (vctxt->err);
    }
    /*
    * SPEC (2.1) "If the {content type} is empty, then the
    * element information item has no character or element
    * information item [children]."
    */
    if (vctxt->inode->typeDef->contentType ==
	    XML_XSD10_SCHEMA_CONTENT_EMPTY) {
	XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_1, NULL,
	    "Character content is not allowed, "
	    "because the content type is empty");
	return (vctxt->err);
    }

    if (vctxt->inode->typeDef->contentType ==
	    XML_XSD10_SCHEMA_CONTENT_ELEMENTS) {
	if ((nodeType != XML_DOM_TEXT) ||
	    (! xml_xsd10_is_blank((char *) value, len))) {
	    /*
	    * SPEC cvc-complex-type (2.3)
	    * "If the {content type} is element-only, then the
	    * element information item has no character information
	    * item [children] other than those whose [character
	    * code] is defined as a white space in [XML 1.0 (Second
	    * Edition)]."
	    */
	    XSD10_VERROR(XML_XSD10_SCHEMAV_CVC_COMPLEX_TYPE_2_3, NULL,
		"Character content other than whitespace is not allowed "
		"because the content type is 'element-only'");
	    return (vctxt->err);
	}
	return (0);
    }

    if ((value == NULL) || (value[0] == 0))
	return (0);
    /*
    * Save the value.
    * NOTE that even if the content type is *mixed*, we need the
    **initial value* for default/fixed value constraints.
    */
    if ((vctxt->inode->typeDef->contentType == XML_XSD10_SCHEMA_CONTENT_MIXED) &&
	((vctxt->inode->decl == NULL) ||
	(vctxt->inode->decl->value == NULL)))
	return (0);

    if (vctxt->inode->value == NULL) {
	/*
	* Set the value.
	*/
	switch (mode) {
	    case XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST:
		/*
		* When working on a tree.
		*/
		vctxt->inode->value = value;
		vctxt->inode->flags &=
		    ~XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
		break;
	    case XML_XSD10_SCHEMA_PUSH_TEXT_CREATED:
		/*
		* When working with the reader.
		* The value will be freed by the element info.
		*/
		vctxt->inode->value = value;
		if (consumed != NULL)
		    *consumed = 1;
		vctxt->inode->flags |=
		    XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
		break;
	    case XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE:
		/*
		* When working with SAX.
		* The value will be freed by the element info.
		*/
		if (len != -1)
		    vctxt->inode->value = xml_xsd10_strndup(value, len);
		else
		    vctxt->inode->value = xml_xsd10_strdup(value);
		vctxt->inode->flags |=
		    XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
		break;
	    default:
		break;
	}
    } else {
	if (len < 0)
	    len = xml_xsd10_strlen(value);
	/*
	* Concat the value.
	*/
	if (vctxt->inode->flags & XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES) {
	    vctxt->inode->value = xml_xsd10_strncat(
		(char *) vctxt->inode->value, value, len);
	} else {
	    vctxt->inode->value =
		xml_xsd10_strncat_new(vctxt->inode->value, value, len);
	    vctxt->inode->flags |= XML_XSD10_SCHEMA_NODE_INFO_FLAG_OWNED_VALUES;
	}
    }

    return (0);
}

/** Port of xmlSchemaValidateElem. */
int xml_xsd10_validate_elem(XmlXsd10ValidCtxt *vctxt)
{
    int ret = 0;

    if ((vctxt->skipDepth != -1) &&
	(vctxt->depth >= vctxt->skipDepth)) {
	XSD10_VERROR_INT("xmlSchemaValidateElem",
	    "in skip-state");
	goto internal_error;
    }
    if (vctxt->xsiAssemble) {
	/*
	* We will stop validation if there was an error during
	* dynamic schema construction.
	* Note that we simply set @skipDepth to 0, this could
	* mean that a streaming document via SAX would be
	* still read to the end but it won't be validated any more.
	* TODO: If we are sure how to stop the validation at once
	*   for all input scenarios, then this should be changed to
	*   instantly stop the validation.
	*/
	ret = xml_xsd10_assemble_by_xsi(vctxt);
	if (ret != 0) {
	    if (ret == -1)
		goto internal_error;
	    vctxt->skipDepth = 0;
	    return(ret);
	}
        /*
         * Augment the IDC definitions for the main schema and all imported ones
         * NOTE: main schema is the first in the imported list
         */
        xml_xsd10_hash_scan(vctxt->schema->schemasImports, xml_xsd10_augment_imported_idc,
                    vctxt);
    }
    if (vctxt->depth > 0) {
	/*
	* Validate this element against the content model
	* of the parent.
	*/
	ret = xml_xsd10_validate_child_elem(vctxt);
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidateElem",
		    "calling xmlSchemaStreamValidateChildElement()");
		goto internal_error;
	    }
	    goto exit;
	}
	if (vctxt->depth == vctxt->skipDepth)
	    goto exit;
	if ((vctxt->inode->decl == NULL) &&
	    (vctxt->inode->typeDef == NULL)) {
	    XSD10_VERROR_INT("xmlSchemaValidateElem",
		"the child element was valid but neither the "
		"declaration nor the type was set");
	    goto internal_error;
	}
    } else {
	/*
	* Get the declaration of the validation root.
	*/
	vctxt->inode->decl = xml_xsd10_get_elem(vctxt->schema,
	    vctxt->inode->localName,
	    vctxt->inode->nsName);
	if (vctxt->inode->decl == NULL) {
	    ret = XML_XSD10_SCHEMAV_CVC_ELT_1;
	    XSD10_VERROR(ret, NULL,
		"No matching global declaration available "
		"for the validation root");
	    goto exit;
	}
    }

    if (vctxt->inode->decl == NULL)
	goto type_validation;

    if (vctxt->inode->decl->type == XML_XSD10_SCHEMA_TYPE_ANY) {
	int skip;
	/*
	* Wildcards.
	*/
	ret = xml_xsd10_validate_elem_wildcard(vctxt, &skip);
	if (ret != 0) {
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaValidateElem",
		    "calling xmlSchemaValidateElemWildcard()");
		goto internal_error;
	    }
	    goto exit;
	}
	if (skip) {
	    vctxt->skipDepth = vctxt->depth;
	    goto exit;
	}
	/*
	* The declaration might be set by the wildcard validation,
	* when the processContents is "lax" or "strict".
	*/
	if (vctxt->inode->decl->type != XML_XSD10_SCHEMA_TYPE_ELEMENT) {
	    /*
	    * Clear the "decl" field to not confuse further processing.
	    */
	    vctxt->inode->decl = NULL;
	    goto type_validation;
	}
    }
    /*
    * Validate against the declaration.
    */
    ret = xml_xsd10_validate_elem_decl(vctxt);
    if (ret != 0) {
	if (ret < 0) {
	    XSD10_VERROR_INT("xmlSchemaValidateElem",
		"calling xmlSchemaValidateElemDecl()");
	    goto internal_error;
	}
	goto exit;
    }
    /*
    * Validate against the type definition.
    */
type_validation:

    if (vctxt->inode->typeDef == NULL) {
	vctxt->inode->flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE;
	ret = XML_XSD10_SCHEMAV_CVC_TYPE_1;
	XSD10_VERROR(ret, NULL,
	    "The type definition is absent");
	goto exit;
    }
    if (vctxt->inode->typeDef->flags & XML_XSD10_SCHEMAS_TYPE_ABSTRACT) {
	vctxt->inode->flags |= XML_XSD10_SCHEMA_NODE_INFO_ERR_BAD_TYPE;
	ret = XML_XSD10_SCHEMAV_CVC_TYPE_2;
	    XSD10_VERROR(ret, NULL,
	    "The type definition is abstract");
	goto exit;
    }
    /*
    * Evaluate IDCs. Do it here, since new IDC matchers are registered
    * during validation against the declaration. This must be done
    * _before_ attribute validation.
    */
    if (vctxt->xpathStates != NULL) {
	ret = xml_xsd10_x_path_evaluate(vctxt, XML_DOM_ELEMENT);
	vctxt->inode->appliedXPath = 1;
	if (ret == -1) {
	    XSD10_VERROR_INT("xmlSchemaValidateElem",
		"calling xmlSchemaXPathEvaluate()");
	    goto internal_error;
	}
    }
    /*
    * Validate attributes.
    */
    if (XSD10_WXS_IS_COMPLEX(vctxt->inode->typeDef)) {
	if ((vctxt->nbAttrInfos != 0) ||
	    (vctxt->inode->typeDef->attrUses != NULL)) {

	    ret = xml_xsd10_v_attributes_complex(vctxt);
	}
    } else if (vctxt->nbAttrInfos != 0) {

	ret = xml_xsd10_v_attributes_simple(vctxt);
    }
    /*
    * Clear registered attributes.
    */
    if (vctxt->nbAttrInfos != 0)
	xml_xsd10_clear_attr_infos(vctxt);
    if (ret == -1) {
	XSD10_VERROR_INT("xmlSchemaValidateElem",
	    "calling attributes validation");
	goto internal_error;
    }
    /*
    * Don't return an error if attributes are invalid on purpose.
    */
    ret = 0;

exit:
    if (ret != 0)
	vctxt->skipDepth = vctxt->depth;
    return (ret);
internal_error:
    return (-1);
}

/************************************************************************
 *									*
 *			Validation interfaces				*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaNewValidCtxt.
 *
 * Create an XML Schemas validation context based on the given schema.
 *
 * @param schema  a precompiled XML Schemas
 * @returns the validation context or NULL in case of error
 */
XmlXsd10ValidCtxt *xml_xsd10_new_valid_ctxt(XmlXsd10Schema *schema)
{
    XmlXsd10ValidCtxt *ret;

    ret = (XmlXsd10ValidCtxt *) malloc(sizeof(XmlXsd10ValidCtxt));
    if (ret == NULL) {
        xml_xsd10_v_err_memory(NULL);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ValidCtxt));
    ret->type = XML_XSD10_SCHEMA_CTXT_VALIDATOR;
    ret->dict = xml_xsd10_dict_create();
    if (ret->dict == NULL) {
        xml_xsd10_free_valid_ctxt(ret);
        return(NULL);
    }
    ret->nodeQNames = xml_xsd10_item_list_create();
    if (ret->nodeQNames == NULL) {
        xml_xsd10_free_valid_ctxt(ret);
        return(NULL);
    }
    ret->schema = schema;
    return (ret);
}

/**
 * Port of xmlSchemaValidateSetFilename.
 *
 * Workaround to provide file error reporting information when this is
 * not provided by current APIs
 *
 * @param vctxt  the schema validation context
 * @param filename  the file name
 */
void xml_xsd10_validate_set_filename(XmlXsd10ValidCtxt *vctxt, const char *filename)
{
    if (vctxt == NULL)
        return;
    if (vctxt->filename != NULL)
        free(vctxt->filename);
    if (filename != NULL)
        vctxt->filename = (char *) xml_xsd10_strdup((const char *) filename);
    else
        vctxt->filename = NULL;
}

/**
 * Port of xmlSchemaClearValidCtxt.
 *
 * Free the resources associated to the schema validation context;
 * leaves some fields alive intended for reuse of the context.
 *
 * @param vctxt  the schema validation context
 */
void xml_xsd10_clear_valid_ctxt(XmlXsd10ValidCtxt *vctxt)
{
    if (vctxt == NULL)
        return;

    /*
    * TODO: Should we clear the flags?
    *   Might be problematic if one reuses the context
    *   and assumes that the options remain the same.
    */
    vctxt->flags = 0;
    vctxt->validationRoot = NULL;
    vctxt->doc = NULL;
    /* (The reader field is not ported.) */
    vctxt->hasKeyrefs = 0;
    /* The ID table of the validated document (libxml2 keeps them in doc->ids). */
    if (vctxt->ids != NULL) {
        xml_xsd10_ids_free(vctxt->ids);
        vctxt->ids = NULL;
    }

    if (vctxt->value != NULL) {
        xml_xsd10_free_value(vctxt->value);
	vctxt->value = NULL;
    }
    /*
    * Augmented IDC information.
    */
    if (vctxt->aidcs != NULL) {
	XmlXsd10IDCAug *cur = vctxt->aidcs, *next;
	do {
	    next = cur->next;
	    free(cur);
	    cur = next;
	} while (cur != NULL);
	vctxt->aidcs = NULL;
    }

    if (vctxt->idcNodes != NULL) {
	int i;
	XmlXsd10PSVIIDCNode *item;

	for (i = 0; i < vctxt->nbIdcNodes; i++) {
	    item = vctxt->idcNodes[i];
	    free(item->keys);
	    free(item);
	}
	free(vctxt->idcNodes);
	vctxt->idcNodes = NULL;
	vctxt->nbIdcNodes = 0;
	vctxt->sizeIdcNodes = 0;
    }

    if (vctxt->idcKeys != NULL) {
	int i;
	for (i = 0; i < vctxt->nbIdcKeys; i++)
	    xml_xsd10_idc_free_key(vctxt->idcKeys[i]);
	free(vctxt->idcKeys);
	vctxt->idcKeys = NULL;
	vctxt->nbIdcKeys = 0;
	vctxt->sizeIdcKeys = 0;
    }

    /*
    * Note that we won't delete the XPath state pool here.
    */
    if (vctxt->xpathStates != NULL) {
	xml_xsd10_free_idc_state_obj_list(vctxt->xpathStates);
	vctxt->xpathStates = NULL;
    }
    /*
    * Attribute info.
    */
    if (vctxt->nbAttrInfos != 0) {
	xml_xsd10_clear_attr_infos(vctxt);
    }
    /*
    * Element info.
    */
    if (vctxt->elemInfos != NULL) {
	int i;
	XmlXsd10NodeInfo *ei;

	for (i = 0; i < vctxt->sizeElemInfos; i++) {
	    ei = vctxt->elemInfos[i];
	    if (ei == NULL)
		break;
	    xml_xsd10_clear_elem_info(vctxt, ei);
	}
    }
    xml_xsd10_item_list_clear(vctxt->nodeQNames);
    /* Recreate the dict. */
    xml_xsd10_dict_free(vctxt->dict);
    /*
    * TODO: Is is save to recreate it? Do we have a scenario
    * where the user provides the dict?
    */
    vctxt->dict = xml_xsd10_dict_create();

    if (vctxt->filename != NULL) {
        free(vctxt->filename);
	vctxt->filename = NULL;
    }

    /*
     * Note that some cleanup functions can move items to the cache,
     * so the cache shouldn't be freed too early.
     */
    if (vctxt->idcMatcherCache != NULL) {
	XmlXsd10IDCMatcher *matcher = vctxt->idcMatcherCache, *tmp;

	while (matcher) {
	    tmp = matcher;
	    matcher = matcher->nextCached;
	    xml_xsd10_idc_free_matcher_list(tmp);
	}
	vctxt->idcMatcherCache = NULL;
    }
}

/**
 * Port of xmlSchemaFreeValidCtxt.
 *
 * Free the resources associated to the schema validation context
 *
 * @param ctxt  the schema validation context
 */
void xml_xsd10_free_valid_ctxt(XmlXsd10ValidCtxt *ctxt)
{
    if (ctxt == NULL)
        return;
    if (ctxt->value != NULL)
        xml_xsd10_free_value(ctxt->value);
    if (ctxt->pctxt != NULL)
	xml_xsd10_free_parser_ctxt(ctxt->pctxt);
    if (ctxt->idcNodes != NULL) {
	int i;
	XmlXsd10PSVIIDCNode *item;

	for (i = 0; i < ctxt->nbIdcNodes; i++) {
	    item = ctxt->idcNodes[i];
	    free(item->keys);
	    free(item);
	}
	free(ctxt->idcNodes);
    }
    if (ctxt->idcKeys != NULL) {
	int i;
	for (i = 0; i < ctxt->nbIdcKeys; i++)
	    xml_xsd10_idc_free_key(ctxt->idcKeys[i]);
	free(ctxt->idcKeys);
    }

    if (ctxt->xpathStates != NULL) {
	xml_xsd10_free_idc_state_obj_list(ctxt->xpathStates);
	ctxt->xpathStates = NULL;
    }
    if (ctxt->xpathStatePool != NULL) {
	xml_xsd10_free_idc_state_obj_list(ctxt->xpathStatePool);
	ctxt->xpathStatePool = NULL;
    }

    /*
    * Augmented IDC information.
    */
    if (ctxt->aidcs != NULL) {
	XmlXsd10IDCAug *cur = ctxt->aidcs, *next;
	do {
	    next = cur->next;
	    free(cur);
	    cur = next;
	} while (cur != NULL);
    }
    if (ctxt->attrInfos != NULL) {
	int i;
	XmlXsd10AttrInfo *attr;

	/* Just a paranoid call to the cleanup. */
	if (ctxt->nbAttrInfos != 0)
	    xml_xsd10_clear_attr_infos(ctxt);
	for (i = 0; i < ctxt->sizeAttrInfos; i++) {
	    attr = ctxt->attrInfos[i];
	    free(attr);
	}
	free(ctxt->attrInfos);
    }
    if (ctxt->elemInfos != NULL) {
	int i;
	XmlXsd10NodeInfo *ei;

	for (i = 0; i < ctxt->sizeElemInfos; i++) {
	    ei = ctxt->elemInfos[i];
	    if (ei == NULL)
		break;
	    xml_xsd10_clear_elem_info(ctxt, ei);
	    free(ei);
	}
	free(ctxt->elemInfos);
    }
    if (ctxt->nodeQNames != NULL)
	xml_xsd10_item_list_free(ctxt->nodeQNames);
    if (ctxt->dict != NULL)
	xml_xsd10_dict_free(ctxt->dict);
    if (ctxt->filename != NULL)
	free(ctxt->filename);
    if (ctxt->ids != NULL)
        xml_xsd10_ids_free(ctxt->ids);
    free(ctxt);
}

/**
 * Port of xmlSchemaVDocWalk.
 *
 * Check if any error was detected during validation.
 *
 * @param ctxt  the schema validation context
 * @returns 1 if valid so far, 0 if errors were detected, and -1 in case
 *         of internal error.
 */
int xml_xsd10_v_doc_walk(XmlXsd10ValidCtxt *vctxt)
{
    XMQAttr *attr;
    int ret = 0;
    XmlXsd10NodeInfo *ielem = NULL;
    XMQNode *node, *valRoot;
    const char *nsName;

    /* DOC VAL TODO: Move this to the start function. */
    if (vctxt->validationRoot != NULL)
        valRoot = vctxt->validationRoot;
    else
	valRoot = xml_dom_root_element(vctxt->doc);
    if (valRoot == NULL) {
	/* VAL TODO: Error code? */
	XSD10_VERROR(1, NULL, "The document has no document element");
	return (1);
    }
    vctxt->depth = -1;
    vctxt->validationRoot = valRoot;
    node = valRoot;
    while (node != NULL) {
	if ((vctxt->skipDepth != -1) && (vctxt->depth >= vctxt->skipDepth))
	    goto next_sibling;
	if (xml_node_kind(node) == XML_DOM_ELEMENT) {

	    /*
	    * Init the node-info.
	    */
	    vctxt->depth++;
	    if (xml_xsd10_validator_push_elem(vctxt) == -1)
		goto internal_error;
	    ielem = vctxt->inode;
	    ielem->node = node;
	    ielem->nodeLine = xml_node_line(node);
	    ielem->localName = xml_node_name(node);
	    if (xml_node_ns(node) != NULL)
		ielem->nsName = xml_node_ns_uri(node);
	    ielem->flags |= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
	    /*
	    * Register attributes.
	    * DOC VAL TODO: We do not register namespace declaration
	    * attributes yet.
	    */
	    vctxt->nbAttrInfos = 0;
	    if (xml_node_first_attr(node) != NULL) {
		attr = xml_node_first_attr(node);
		do {
                    char *content;

		    if (xml_node_ns(xml_attr_node(attr)) != NULL)
			nsName = xml_node_ns_uri(xml_attr_node(attr));
		    else
			nsName = NULL;
                    content = xml_xsd10_tree_node_get_content(xml_attr_node(attr));
		    ret = xml_xsd10_validator_push_attribute(vctxt,
			xml_attr_node(attr),
			/*
			* Note that we give it the line number of the
			* parent element.
			*/
			ielem->nodeLine,
			xml_attr_name(attr), nsName, 0,
			content, 1);
		    if (ret == -1) {
			XSD10_VERROR_INT("xmlSchemaDocWalk",
			    "calling xmlSchemaValidatorPushAttribute()");
                        free(content);
			goto internal_error;
		    }
		    attr = xml_attr_next(attr);
		} while (attr);
	    }
	    /*
	    * Validate the element.
	    */
	    ret = xml_xsd10_validate_elem(vctxt);
	    if (ret != 0) {
		if (ret == -1) {
		    XSD10_VERROR_INT("xmlSchemaDocWalk",
			"calling xmlSchemaValidateElem()");
		    goto internal_error;
		}
		/*
		* Don't stop validation; just skip the content
		* of this element.
		*/
		goto leave_node;
	    }
	    if ((vctxt->skipDepth != -1) &&
		(vctxt->depth >= vctxt->skipDepth))
		goto leave_node;
	} else if ((xml_node_kind(node) == XML_DOM_TEXT) ||
	    (xml_node_kind(node) == XML_DOM_CDATA)) {
	    /*
	    * Process character content.
	    */
	    if ((ielem != NULL) && (ielem->flags & XML_XSD10_SCHEMA_ELEM_INFO_EMPTY))
		ielem->flags ^= XML_XSD10_SCHEMA_ELEM_INFO_EMPTY;
	    ret = xml_xsd10_v_push_text(vctxt, xml_node_kind(node), xml_node_content(node),
		-1, XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST, NULL);
	    if (ret < 0) {
		XSD10_VERROR_INT("xmlSchemaVDocWalk",
		    "calling xmlSchemaVPushText()");
		goto internal_error;
	    }
	    /*
	    * DOC VAL TODO: Should we skip further validation of the
	    * element content here?
	    */
	} else if (/* (node->type == XML_ENTITY_NODE) does not exist in the native dom */
	    (xml_node_kind(node) == XML_DOM_ENTITY_REF)) {
	    /*
	    * DOC VAL TODO: What to do with entities?
	    */
	    XSD10_VERROR_INT("xmlSchemaVDocWalk",
		"there is at least one entity reference in the node-tree "
		"currently being validated. Processing of entities with "
		"this XML Schema processor is not supported (yet). Please "
		"substitute entities before validation.");
	    goto internal_error;
	} else {
	    goto leave_node;
	    /*
	    * DOC VAL TODO: XInclude nodes, etc.
	    */
	}
	/*
	* Walk the doc.
	*/
	if (xml_node_first_child(node) != NULL) {
	    node = xml_node_first_child(node);
	    continue;
	}
leave_node:
	if (xml_node_kind(node) == XML_DOM_ELEMENT) {
	    /*
	    * Leaving the scope of an element.
	    */
	    if (node != vctxt->inode->node) {
		XSD10_VERROR_INT("xmlSchemaVDocWalk",
		    "element position mismatch");
		goto internal_error;
	    }
	    ret = xml_xsd10_validator_pop_elem(vctxt);
	    if (ret != 0) {
		if (ret < 0) {
		    XSD10_VERROR_INT("xmlSchemaVDocWalk",
			"calling xmlSchemaValidatorPopElem()");
		    goto internal_error;
		}
	    }
	    if (node == valRoot)
		goto exit;
	}
next_sibling:
	if (xml_node_next(node) != NULL)
	    node = xml_node_next(node);
	else {
	    node = xml_node_parent(node);
	    goto leave_node;
	}
    }

exit:
    return (ret);
internal_error:
    return (-1);
}

/** Port of xmlSchemaPreRun. */
int xml_xsd10_pre_run(XmlXsd10ValidCtxt *vctxt)
{
    /*
    * Some initialization.
    */
    vctxt->err = 0;
    vctxt->nberrors = 0;
    vctxt->depth = -1;
    vctxt->skipDepth = -1;
    vctxt->hasKeyrefs = 0;
#ifdef ENABLE_IDC_NODE_TABLES_TEST
    vctxt->createIDCNodeTables = 1;
#else
    vctxt->createIDCNodeTables = 0;
#endif
    /*
    * The ID table of the instance (replaces libxml2 doc->ids).
    */
    if (vctxt->ids == NULL) {
        vctxt->ids = xml_xsd10_ids_new();
        if (vctxt->ids == NULL) {
            xml_xsd10_v_err_memory(vctxt);
            return (-1);
        }
    }
    /*
    * Create a schema + parser if necessary.
    */
    if (vctxt->schema == NULL) {
	XmlXsd10ParserCtxt *pctxt;

	vctxt->xsiAssemble = 1;
	/*
	* If not schema was given then we will create a schema
	* dynamically using XSI schema locations.
	*
	* Create the schema parser context.
	*/
	if ((vctxt->pctxt == NULL) &&
	   (xml_xsd10_create_p_ctxt_on_v_ctxt(vctxt) == -1))
	   return (-1);
	pctxt = vctxt->pctxt;
	pctxt->xsiAssemble = 1;
	/*
	* Create the schema.
	*/
	vctxt->schema = xml_xsd10_new_schema(pctxt);
	if (vctxt->schema == NULL)
	    return (-1);
	/*
	* Create the schema construction context.
	*/
	pctxt->constructor = xml_xsd10_construction_ctxt_create(pctxt->dict);
	if (pctxt->constructor == NULL)
	    return(-1);
	pctxt->constructor->mainSchema = vctxt->schema;
	/*
	* Take ownership of the constructor to be able to free it.
	*/
	pctxt->ownsConstructor = 1;
    }
    /*
    * Augment the IDC definitions for the main schema and all imported ones
    * NOTE: main schema if the first in the imported list
    */
    xml_xsd10_hash_scan(vctxt->schema->schemasImports, xml_xsd10_augment_imported_idc,
                vctxt);

    return(0);
}

/** Port of xmlSchemaPostRun. */
void xml_xsd10_post_run(XmlXsd10ValidCtxt *vctxt)
{
    if (vctxt->xsiAssemble) {
	if (vctxt->schema != NULL) {
	    xml_xsd10_free(vctxt->schema);
	    vctxt->schema = NULL;
	}
    }
    xml_xsd10_clear_valid_ctxt(vctxt);
}

/** Port of xmlSchemaVStart. */
int xml_xsd10_v_start(XmlXsd10ValidCtxt *vctxt)
{
    int ret = 0;

    if (xml_xsd10_pre_run(vctxt) < 0)
        return(-1);

    if (vctxt->doc != NULL) {
	/*
	 * Tree validation.
	 */
	ret = xml_xsd10_v_doc_walk(vctxt);
    /* The XML Reader and SAX validation are not ported. */
    } else {
	XSD10_VERROR_INT("xmlSchemaVStart",
	    "no instance to validate");
	ret = -1;
    }

    xml_xsd10_post_run(vctxt);
    if (ret == 0)
	ret = vctxt->err;
    return (ret);
}

/**
 * Port of xmlSchemaValidateOneElement.
 *
 * Validate a branch of a tree, starting with the given `elem`.
 *
 * @param ctxt  a schema validation context
 * @param elem  an element node
 * @returns 0 if the element and its subtree is valid, a positive error
 * code number otherwise and -1 in case of an internal or API error.
 */
int xml_xsd10_validate_one_element(XmlXsd10ValidCtxt *ctxt, XMQNode *elem)
{
    if ((ctxt == NULL) || (elem == NULL) || (xml_node_kind(elem) != XML_DOM_ELEMENT))
	return (-1);

    if (ctxt->schema == NULL)
	return (-1);

    ctxt->doc = xml_node_dom(elem);
    ctxt->node = elem;
    ctxt->validationRoot = elem;
    return(xml_xsd10_v_start(ctxt));
}

/**
 * Port of xmlSchemaValidateDoc.
 *
 * Validate a document tree in memory.
 *
 * @param ctxt  a schema validation context
 * @param doc  a parsed document tree
 * @returns 0 if the document is schemas valid, a positive error code
 *     number otherwise and -1 in case of internal or API error.
 */
int xml_xsd10_validate_doc(XmlXsd10ValidCtxt *ctxt, XmlDom *doc)
{
    if ((ctxt == NULL) || (doc == NULL))
        return (-1);

    ctxt->doc = doc;
    ctxt->node = xml_dom_root_element(doc);
    if (ctxt->node == NULL) {
        xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt,
	    XML_XSD10_SCHEMAV_DOCUMENT_ELEMENT_MISSING,
	    xml_dom_document(doc), NULL,
	    "The document has no document element", NULL, NULL);
        return (ctxt->err);
    }
    ctxt->validationRoot = ctxt->node;
    return (xml_xsd10_v_start(ctxt));
}


#undef XSD10_VALID_IS_BLANK_CH
#undef XSD10_ACTIVATE_ATTRIBUTE
#undef XSD10_ACTIVATE_ELEM
#undef XSD10_ACTIVATE_PARENT_ELEM
#undef XSD10_NORMALIZE
#undef XML_XSD10_SCHEMA_PUSH_TEXT_PERSIST
#undef XML_XSD10_SCHEMA_PUSH_TEXT_CREATED
#undef XML_XSD10_SCHEMA_PUSH_TEXT_VOLATILE

#endif // XML_XSD10_VALID_MODULE
