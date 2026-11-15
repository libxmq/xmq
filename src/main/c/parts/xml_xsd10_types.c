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

/* Portions of this file are ported from libxml2 (xmlschemastypes.c, and the parts of
   xmlschemas.c, tree.c, parser.c, xmlstring.c and valid.c it uses, version 2.16.0 as
   found in the libxmq fork in 3rdparty/libxml2).
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

#ifndef BUILDING_DIST_XMQ

#include"xml_xsd10_types.h"
#include"xml_dom.h"
#include"xml_uri.h"
#include"hashmap.h"

#include<float.h>
#include<limits.h>
#include<math.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_XSD10_TYPES_MODULE

/* The libxml2 xmlChar: the values are parsed as unsigned bytes, exactly like libxml2. */
typedef unsigned char XmlXsd10Char;

#define XSD10_TYPES_NS "http://www.w3.org/2001/XMLSchema"
#define XSD10_TYPES_BAD_CAST (XmlXsd10Char *)

#define XSD10_TYPES_IS_BLANK_CH(c) (((c) == 0x20) || ((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd))

#define XSD10_TYPES_IS_WSP_REPLACE_CH(c)	((((c) == 0x9) || ((c) == 0xa)) || \
				 ((c) == 0xd))

#define XSD10_TYPES_IS_WSP_SPACE_CH(c)	((c) == 0x20)

#define XSD10_TYPES_IS_WSP_BLANK_CH(c) XSD10_TYPES_IS_BLANK_CH(c)

/* Date value */
typedef struct XmlXsd10ValDate XmlXsd10ValDate;
struct XmlXsd10ValDate {
    long		year;
    unsigned int	mon	:4;	/* 1 <=  mon    <= 12   */
    unsigned int	day	:5;	/* 1 <=  day    <= 31   */
    unsigned int	hour	:5;	/* 0 <=  hour   <= 24   */
    unsigned int	min	:6;	/* 0 <=  min    <= 59	*/
    double		sec;
    unsigned int	tz_flag	:1;	/* is tzo explicitly set? */
    signed int		tzo	:12;	/* -1440 <= tzo <= 1440;
					   currently only -840 to +840 are needed */
};

/* Duration value */
typedef struct XmlXsd10ValDuration XmlXsd10ValDuration;
struct XmlXsd10ValDuration {
    long	        mon;		/* mon stores years also */
    long	day;
    double		sec;            /* sec stores min and hour also */
};

typedef struct XmlXsd10ValDecimal XmlXsd10ValDecimal;
struct XmlXsd10ValDecimal
{
	XmlXsd10Char *str;
	unsigned integralPlaces;
	unsigned fractionalPlaces;
};

typedef struct XmlXsd10ValQName XmlXsd10ValQName;
struct XmlXsd10ValQName {
    XmlXsd10Char *name;
    XmlXsd10Char *uri;
};

typedef struct XmlXsd10ValHex XmlXsd10ValHex;
struct XmlXsd10ValHex {
    XmlXsd10Char     *str;
    unsigned int total;
};

typedef struct XmlXsd10ValBase64 XmlXsd10ValBase64;
struct XmlXsd10ValBase64 {
    XmlXsd10Char     *str;
    unsigned int total;
};

/* Port of struct _xmlSchemaVal. */
struct XmlXsd10Val {
    XmlXsd10ValType type;
    struct XmlXsd10Val *next;
    union {
	XmlXsd10ValDecimal     decimal;
        XmlXsd10ValDate        date;
        XmlXsd10ValDuration    dur;
	XmlXsd10ValQName	qname;
	XmlXsd10ValHex		hex;
	XmlXsd10ValBase64	base64;
	float			f;
	double			d;
	int			b;
	XmlXsd10Char                *str;
    } value;
};

/* The ID table of a document (libxml2 doc->ids, doc->refs and attr->atype). */
typedef struct XmlXsd10IdRef
{
    char *value;
    XMQNode *attr;
} XmlXsd10IdRef;

struct XmlXsd10Ids
{
    HashMap *ids; // id value -> XMQNode * (the attribute)
    HashMap *atypes; // "%p" of the attribute -> (void*)(intptr_t)atype
    XmlXsd10IdRef *refs;
    size_t num_refs;
    size_t max_refs;
};

/* The regexp implementation for pattern facets. */
static XmlXsd10RegexpHooks xsd10_types_regexp_hooks_ = { NULL, NULL, NULL };

void xml_xsd10_set_regexp_hooks(const XmlXsd10RegexpHooks *hooks)
{
    if (hooks == NULL) {
        xsd10_types_regexp_hooks_.compile = NULL;
        xsd10_types_regexp_hooks_.exec = NULL;
        xsd10_types_regexp_hooks_.free = NULL;
    } else {
        xsd10_types_regexp_hooks_ = *hooks;
    }
}

/* libxml2 xmlRegexpExec: -1 without a regexp. */
static int xsd10_types_regexp_exec(void *regexp, const XmlXsd10Char *value)
{
    if ((regexp == NULL) || (value == NULL) || (xsd10_types_regexp_hooks_.exec == NULL))
        return (-1);
    return xsd10_types_regexp_hooks_.exec(regexp, (const char *) value);
}

static void xsd10_types_regexp_free(void *regexp)
{
    if ((regexp != NULL) && (xsd10_types_regexp_hooks_.free != NULL))
        xsd10_types_regexp_hooks_.free(regexp);
}

/************************************************************************
 *									*
 *			String helpers (libxml2 xmlstring.c)		*
 *									*
 ************************************************************************/

/* Port of xmlStrndup. */
static XmlXsd10Char *xsd10_types_strndup(const XmlXsd10Char *cur, long len)
{
    XmlXsd10Char *ret;

    if ((cur == NULL) || (len < 0)) return(NULL);
    ret = (XmlXsd10Char *) malloc((size_t) len + 1);
    if (ret == NULL) {
        return(NULL);
    }
    memcpy(ret, cur, len);
    ret[len] = 0;
    return(ret);
}

/* Port of xmlStrdup. */
static XmlXsd10Char *xsd10_types_strdup(const XmlXsd10Char *cur)
{
    if (cur == NULL) return(NULL);
    return xsd10_types_strndup(cur, (long) strlen((const char *) cur));
}

/* Port of xmlStrEqual. */
static int xsd10_types_str_equal(const XmlXsd10Char *str1, const XmlXsd10Char *str2)
{
    if (str1 == str2) return(1);
    if (str1 == NULL) return(0);
    if (str2 == NULL) return(0);
    return (strcmp((const char *) str1, (const char *) str2) == 0);
}

/* Port of xmlStrcmp. */
static int xsd10_types_strcmp(const XmlXsd10Char *str1, const XmlXsd10Char *str2)
{
    if (str1 == str2) return(0);
    if (str1 == NULL) return(-1);
    if (str2 == NULL) return(1);
    return strcmp((const char *) str1, (const char *) str2);
}

/* Port of xmlStrcat. */
static XmlXsd10Char *xsd10_types_strcat(XmlXsd10Char *cur, const XmlXsd10Char *add)
{
    size_t size, len;
    XmlXsd10Char *ret;

    if (add == NULL)
        return(cur);
    if (cur == NULL)
        return(xsd10_types_strdup(add));
    size = strlen((const char *) cur);
    len = strlen((const char *) add);
    ret = (XmlXsd10Char *) realloc(cur, size + len + 1);
    if (ret == NULL) {
        free(cur);
        return(NULL);
    }
    memcpy(&ret[size], add, len);
    ret[size + len] = 0;
    return(ret);
}

/* Port of xmlUTF8Strlen. */
static int xsd10_types_utf8_strlen(const XmlXsd10Char *utf)
{
    size_t ret = 0;

    if (utf == NULL)
        return(-1);

    while (*utf != 0) {
        if (utf[0] & 0x80) {
            if ((utf[1] & 0xc0) != 0x80)
                return(-1);
            if ((utf[0] & 0xe0) == 0xe0) {
                if ((utf[2] & 0xc0) != 0x80)
                    return(-1);
                if ((utf[0] & 0xf0) == 0xf0) {
                    if ((utf[0] & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
                        return(-1);
                    utf += 4;
                } else {
                    utf += 3;
                }
            } else {
                utf += 2;
            }
        } else {
            utf++;
        }
        ret++;
    }
    return(ret > INT_MAX ? 0 : (int) ret);
}

/* Port of xmlGetUTF8Char. */
static int xsd10_types_get_utf8_char(const XmlXsd10Char *utf, int *len)
{
    unsigned int c;

    if ((utf == NULL) || (len == NULL)) {
        if (len != NULL)
            *len = 0;
        return(-1);
    }

    c = utf[0];
    if (c < 0x80) {
        if (*len < 1)
            goto error;
        /* 1-byte code */
        *len = 1;
    } else {
        if ((*len < 2) || ((utf[1] & 0xc0) != 0x80))
            goto error;
        if (c < 0xe0) {
            if (c < 0xc2)
                goto error;
            /* 2-byte code */
            *len = 2;
            c = (c & 0x1f) << 6;
            c |= utf[1] & 0x3f;
        } else {
            if ((*len < 3) || ((utf[2] & 0xc0) != 0x80))
                goto error;
            if (c < 0xf0) {
                /* 3-byte code */
                *len = 3;
                c = (c & 0xf) << 12;
                c |= (utf[1] & 0x3f) << 6;
                c |= utf[2] & 0x3f;
                if ((c < 0x800) || ((c >= 0xd800) && (c < 0xe000)))
                    goto error;
            } else {
                if ((*len < 4) || ((utf[3] & 0xc0) != 0x80))
                    goto error;
                *len = 4;
                /* 4-byte code */
                c = (c & 0x7) << 18;
                c |= (utf[1] & 0x3f) << 12;
                c |= (utf[2] & 0x3f) << 6;
                c |= utf[3] & 0x3f;
                if ((c < 0x10000) || (c >= 0x110000))
                    goto error;
            }
        }
    }
    return((int) c);

error:
    *len = 0;
    return(-1);
}

/************************************************************************
 *									*
 *	Name, NCName, QName and NMToken (libxml2 parser.c, tree.c)	*
 *									*
 ************************************************************************/

/* Port of xmlIsNameStartCharNew (XML 1.0 fifth edition). */
static int xsd10_types_is_name_start_char(int c)
{
    if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
        (((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         (c == '_') || (c == ':') ||
         ((c >= 0xC0) && (c <= 0xD6)) ||
         ((c >= 0xD8) && (c <= 0xF6)) ||
         ((c >= 0xF8) && (c <= 0x2FF)) ||
         ((c >= 0x370) && (c <= 0x37D)) ||
         ((c >= 0x37F) && (c <= 0x1FFF)) ||
         ((c >= 0x200C) && (c <= 0x200D)) ||
         ((c >= 0x2070) && (c <= 0x218F)) ||
         ((c >= 0x2C00) && (c <= 0x2FEF)) ||
         ((c >= 0x3001) && (c <= 0xD7FF)) ||
         ((c >= 0xF900) && (c <= 0xFDCF)) ||
         ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
         ((c >= 0x10000) && (c <= 0xEFFFF))))
        return(1);
    return(0);
}

/* Port of xmlIsNameCharNew (XML 1.0 fifth edition). */
static int xsd10_types_is_name_char(int c)
{
    if ((c != ' ') && (c != '>') && (c != '/') && /* accelerators */
        (((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         ((c >= '0') && (c <= '9')) || /* !start */
         (c == '_') || (c == ':') ||
         (c == '-') || (c == '.') || (c == 0xB7) || /* !start */
         ((c >= 0xC0) && (c <= 0xD6)) ||
         ((c >= 0xD8) && (c <= 0xF6)) ||
         ((c >= 0xF8) && (c <= 0x2FF)) ||
         ((c >= 0x300) && (c <= 0x36F)) || /* !start */
         ((c >= 0x370) && (c <= 0x37D)) ||
         ((c >= 0x37F) && (c <= 0x1FFF)) ||
         ((c >= 0x200C) && (c <= 0x200D)) ||
         ((c >= 0x203F) && (c <= 0x2040)) || /* !start */
         ((c >= 0x2070) && (c <= 0x218F)) ||
         ((c >= 0x2C00) && (c <= 0x2FEF)) ||
         ((c >= 0x3001) && (c <= 0xD7FF)) ||
         ((c >= 0xF900) && (c <= 0xFDCF)) ||
         ((c >= 0xFDF0) && (c <= 0xFFFD)) ||
         ((c >= 0x10000) && (c <= 0xEFFFF))))
         return(1);
    return(0);
}

#define XSD10_TYPES_SCAN_NC         1
#define XSD10_TYPES_SCAN_NMTOKEN    2

/* Port of xmlScanName (without a size limit and without XML_SCAN_OLD10):
   returns a pointer to the end of the name, ptr if the name is invalid. */
static const XmlXsd10Char *xsd10_types_scan_name(const XmlXsd10Char *ptr, int flags)
{
    int stop = flags & XSD10_TYPES_SCAN_NC ? ':' : 0;

    while (1) {
        int c, len;

        c = *ptr;
        if (c < 0x80) {
            if (c == stop)
                break;
            len = 1;
        } else {
            len = 4;
            c = xsd10_types_get_utf8_char(ptr, &len);
            if (c < 0)
                break;
        }

        if (flags & XSD10_TYPES_SCAN_NMTOKEN ?
                !xsd10_types_is_name_char(c) :
                !xsd10_types_is_name_start_char(c))
            break;

        ptr += len;
        flags |= XSD10_TYPES_SCAN_NMTOKEN;
    }

    return(ptr);
}

/* Port of xmlValidateNCName (also used for Name and NMToken with other scan flags). */
static int xsd10_types_validate_scanned(const XmlXsd10Char *value, int space, int flags)
{
    const XmlXsd10Char *cur;

    if (value == NULL)
        return(-1);

    cur = value;

    if (space) {
	while (XSD10_TYPES_IS_BLANK_CH(*cur))
            cur++;
    }

    value = cur;
    cur = xsd10_types_scan_name(value, flags);
    if ((cur == NULL) || (cur == value))
        return(1);

    if (space) {
	while (XSD10_TYPES_IS_BLANK_CH(*cur))
            cur++;
    }

    return(*cur != 0);
}

/* Port of xmlValidateNCName. */
static int xsd10_types_validate_ncname(const XmlXsd10Char *value, int space)
{
    return xsd10_types_validate_scanned(value, space, XSD10_TYPES_SCAN_NC);
}

/* Port of xmlValidateName. */
static int xsd10_types_validate_name(const XmlXsd10Char *value, int space)
{
    return xsd10_types_validate_scanned(value, space, 0);
}

/* Port of xmlValidateNMToken. */
static int xsd10_types_validate_nmtoken(const XmlXsd10Char *value, int space)
{
    return xsd10_types_validate_scanned(value, space, XSD10_TYPES_SCAN_NMTOKEN);
}

/* Port of xmlValidateQName. */
static int xsd10_types_validate_qname(const XmlXsd10Char *value, int space)
{
    const XmlXsd10Char *cur;

    if (value == NULL)
        return(-1);

    cur = value;

    if (space) {
	while (XSD10_TYPES_IS_BLANK_CH(*cur))
            cur++;
    }

    value = cur;
    cur = xsd10_types_scan_name(value, XSD10_TYPES_SCAN_NC);
    if ((cur == NULL) || (cur == value))
        return(1);

    if (*cur == ':') {
        cur += 1;
        value = cur;
        cur = xsd10_types_scan_name(value, XSD10_TYPES_SCAN_NC);
        if ((cur == NULL) || (cur == value))
            return(1);
    }

    if (space) {
	while (XSD10_TYPES_IS_BLANK_CH(*cur))
            cur++;
    }

    return(*cur != 0);
}

/* Port of xmlSplitQName2: NULL if the name has no prefix, otherwise the malloced local part
   and *prefix the malloced prefix. */
static XmlXsd10Char *xsd10_types_split_qname2(const XmlXsd10Char *name, XmlXsd10Char **prefix)
{
    int len = 0;
    XmlXsd10Char *ret = NULL;

    if (prefix == NULL) return(NULL);
    *prefix = NULL;
    if (name == NULL) return(NULL);

    /* nasty but valid */
    if (name[0] == ':')
	return(NULL);

    /*
     * we are not trying to validate but just to cut, and yes it will
     * work even if this is as set of UTF-8 encoded chars
     */
    while ((name[len] != 0) && (name[len] != ':'))
	len++;

    if ((name[len] == 0) || (name[len+1] == 0))
	return(NULL);

    *prefix = xsd10_types_strndup(name, len);
    if (*prefix == NULL)
	return(NULL);
    ret = xsd10_types_strdup(&name[len + 1]);
    if (ret == NULL) {
	if (*prefix != NULL) {
	    free(*prefix);
	    *prefix = NULL;
	}
	return(NULL);
    }

    return(ret);
}

/* Replaces libxml2 xmlSearchNs(node->doc, node, prefix): the in scope namespace uri of the
   prefix (NULL for the default namespace), NULL if not declared. */
static const XmlXsd10Char *xsd10_types_search_ns(XMQNode *node, const XmlXsd10Char *prefix, int *found)
{
    XmlNs *ns;

    *found = 0;
    if (node == NULL)
        return (NULL);
    ns = xml_ns_lookup_prefix(node, (const char *) prefix);
    if ((ns == NULL) || (xml_ns_href(ns) == NULL))
        return (NULL);
    *found = 1;
    return ((const XmlXsd10Char *) xml_ns_href(ns));
}

/************************************************************************
 *									*
 *			The ID table					*
 *									*
 ************************************************************************/

XmlXsd10Ids *xml_xsd10_ids_new(void)
{
    XmlXsd10Ids *ids = (XmlXsd10Ids *) malloc(sizeof(XmlXsd10Ids));
    if (ids == NULL)
        return (NULL);
    memset(ids, 0, sizeof(*ids));
    ids->ids = hashmap_create(1024);
    ids->atypes = hashmap_create(1024);
    return (ids);
}

void xml_xsd10_ids_free(XmlXsd10Ids *ids)
{
    size_t i;

    if (ids == NULL)
        return;
    hashmap_free(ids->ids);
    hashmap_free(ids->atypes);
    for (i = 0; i < ids->num_refs; i++)
        free(ids->refs[i].value);
    free(ids->refs);
    free(ids);
}

static void xsd10_types_attr_key(const XMQNode *attr, char *buf, size_t size)
{
    snprintf(buf, size, "%p", (const void *) attr);
}

int xml_xsd10_ids_attr_type(XmlXsd10Ids *ids, const XMQNode *attr)
{
    char key[64];

    if ((ids == NULL) || (attr == NULL))
        return (0);
    xsd10_types_attr_key(attr, key, sizeof(key));
    return ((int) (intptr_t) hashmap_get(ids->atypes, key));
}

void xml_xsd10_ids_set_attr_type(XmlXsd10Ids *ids, const XMQNode *attr, int atype)
{
    char key[64];

    if ((ids == NULL) || (attr == NULL))
        return;
    xsd10_types_attr_key(attr, key, sizeof(key));
    if (atype == 0)
        hashmap_remove(ids->atypes, key);
    else
        hashmap_put(ids->atypes, key, (void *) (intptr_t) atype);
}

/* Port of xmlAddIDSafe (xmlAddIDInternal). */
int xml_xsd10_ids_add_id(XmlXsd10Ids *ids, XMQNode *attr, const char *value)
{
    const char *key;
    void *val;
    HashMapIterator *it;

    if ((value == NULL) || (value[0] == 0))
	return(0);
    if ((attr == NULL) || (ids == NULL))
	return(0);

    if (hashmap_get(ids->ids, value) != NULL)
        return(0);

    /* An attribute holds one id (libxml2 xmlRemoveID of the previous one). */
    it = hashmap_iterate(ids->ids);
    while (hashmap_next_key_value(it, &key, &val)) {
        if (val == (void *) attr) {
            char *old = strdup(key);
            hashmap_free_iterator(it);
            hashmap_remove(ids->ids, old);
            free(old);
            it = NULL;
            break;
        }
    }
    if (it != NULL)
        hashmap_free_iterator(it);

    hashmap_put(ids->ids, value, attr);
    xml_xsd10_ids_set_attr_type(ids, attr, XML_XSD10_ATTRIBUTE_ID);
    return(1);
}

XMQNode *xml_xsd10_ids_lookup(XmlXsd10Ids *ids, const char *value)
{
    if ((ids == NULL) || (value == NULL))
        return (NULL);
    return ((XMQNode *) hashmap_get(ids->ids, value));
}

/* Port of xmlAddRef (the list of references of the document). */
void xml_xsd10_ids_add_ref(XmlXsd10Ids *ids, XMQNode *attr, const char *value)
{
    if ((ids == NULL) || (value == NULL) || (attr == NULL))
        return;
    if (ids->num_refs == ids->max_refs) {
        size_t n = ids->max_refs ? ids->max_refs * 2 : 16;
        XmlXsd10IdRef *refs = (XmlXsd10IdRef *) realloc(ids->refs, n * sizeof(XmlXsd10IdRef));
        if (refs == NULL)
            return;
        ids->refs = refs;
        ids->max_refs = n;
    }
    ids->refs[ids->num_refs].value = strdup(value);
    ids->refs[ids->num_refs].attr = attr;
    ids->num_refs++;
}

size_t xml_xsd10_ids_num_refs(XmlXsd10Ids *ids)
{
    return (ids ? ids->num_refs : 0);
}

const char *xml_xsd10_ids_ref_value(XmlXsd10Ids *ids, size_t i)
{
    if ((ids == NULL) || (i >= ids->num_refs))
        return (NULL);
    return (ids->refs[i].value);
}

XMQNode *xml_xsd10_ids_ref_attr(XmlXsd10Ids *ids, size_t i)
{
    if ((ids == NULL) || (i >= ids->num_refs))
        return (NULL);
    return (ids->refs[i].attr);
}

/************************************************************************
 *									*
 *			The built in types				*
 *									*
 ************************************************************************/

/*
 * libxml2 builds the built in types in a global hash table on first use
 * (xmlSchemaInitTypes, protected by a mutex). Here they are static data,
 * fully initialized at compile time: the array xsd10_types_builtin_ is indexed
 * by the XmlXsd10ValType of the type (index 0 is unused). The types are never
 * modified or freed, so no initialization or locking is needed.
 */

/* The minLength facet of the built in list types, port of xmlSchemaNewMinLengthFacet(1):
   a nonNegativeInteger value "+1.0". */
static XmlXsd10Char xsd10_types_min_length_str_[3][5] = { "+1.0", "+1.0", "+1.0" };

static XmlXsd10Val xsd10_types_min_length_val_[3] = {
    { XML_XSD10_SCHEMAS_NNINTEGER, NULL, { { xsd10_types_min_length_str_[0], 1, 1 } } },
    { XML_XSD10_SCHEMAS_NNINTEGER, NULL, { { xsd10_types_min_length_str_[1], 1, 1 } } },
    { XML_XSD10_SCHEMAS_NNINTEGER, NULL, { { xsd10_types_min_length_str_[2], 1, 1 } } }
};

static XmlXsd10Facet xsd10_types_min_length_facet_[3] = {
    { XML_XSD10_SCHEMA_FACET_MINLENGTH, NULL, NULL, NULL, NULL, NULL, 0, 0, &xsd10_types_min_length_val_[0], NULL },
    { XML_XSD10_SCHEMA_FACET_MINLENGTH, NULL, NULL, NULL, NULL, NULL, 0, 0, &xsd10_types_min_length_val_[1], NULL },
    { XML_XSD10_SCHEMA_FACET_MINLENGTH, NULL, NULL, NULL, NULL, NULL, 0, 0, &xsd10_types_min_length_val_[2], NULL }
};

/* The content type of xs:anyType (3.4.7 Built-in Complex Type Definition): a particle
   holding a sequence holding a particle (minOccurs 0, maxOccurs unbounded) of a lax
   wildcard, plus a lax attribute wildcard. */
static XmlXsd10Wildcard xsd10_types_any_wildcard_ = {
    XML_XSD10_SCHEMA_TYPE_ANY, NULL, NULL, NULL, 0, 0, XML_XSD10_SCHEMAS_ANY_LAX, 1, NULL, NULL, 0
};

static XmlXsd10Wildcard xsd10_types_any_attribute_wildcard_ = {
    (XmlXsd10TypeType) 0, NULL, NULL, NULL, 0, 0, XML_XSD10_SCHEMAS_ANY_LAX, 1, NULL, NULL, 0
};

static XmlXsd10Particle xsd10_types_any_inner_particle_ = {
    XML_XSD10_SCHEMA_TYPE_PARTICLE, NULL, NULL, (XmlXsd10TreeItem *) &xsd10_types_any_wildcard_,
    0, XML_XSD10_UNBOUNDED, NULL
};

static XmlXsd10ModelGroup xsd10_types_any_sequence_ = {
    XML_XSD10_SCHEMA_TYPE_SEQUENCE, NULL, NULL, (XmlXsd10TreeItem *) &xsd10_types_any_inner_particle_, NULL
};

static XmlXsd10Particle xsd10_types_any_particle_ = {
    XML_XSD10_SCHEMA_TYPE_PARTICLE, NULL, NULL, (XmlXsd10TreeItem *) &xsd10_types_any_sequence_,
    1, 1, NULL
};

#define XSD10_TYPES_PRIMITIVE (XML_XSD10_SCHEMAS_TYPE_BUILTIN_PRIMITIVE | XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC)
#define XSD10_TYPES_ATOMIC XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC
#define XSD10_TYPES_LIST (XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST | XML_XSD10_SCHEMAS_TYPE_HAS_FACETS)
#define XSD10_TYPES_REF(t) (&xsd10_types_builtin_[XML_XSD10_SCHEMAS_##t])

/* Port of xmlSchemaInitBasicType: one built in type. */
#define XSD10_TYPES_DEF(name, t, base, flags, facets, subtypes, contentType, wildcard) \
    { XML_XSD10_SCHEMA_TYPE_BASIC, NULL, name, NULL, NULL, NULL, NULL, subtypes, NULL, NULL, 0, 0, \
      flags, contentType, NULL, NULL, base, facets, NULL, 0, NULL, wildcard, XML_XSD10_SCHEMAS_##t, \
      NULL, NULL, NULL, NULL, NULL, XSD10_TYPES_NS, NULL }

#define XSD10_TYPES_SIMPLE(name, t, base, flags) \
    XSD10_TYPES_DEF(name, t, XSD10_TYPES_REF(base), flags, NULL, NULL, XML_XSD10_SCHEMA_CONTENT_BASIC, NULL)

#define XSD10_TYPES_LISTDEF(name, t, item, n) \
    XSD10_TYPES_DEF(name, t, XSD10_TYPES_REF(ANYSIMPLETYPE), XSD10_TYPES_LIST, &xsd10_types_min_length_facet_[n], \
                    XSD10_TYPES_REF(item), XML_XSD10_SCHEMA_CONTENT_BASIC, NULL)

/* Port of xmlSchemaInitTypes: the built in types, indexed by their XmlXsd10ValType. */
static XmlXsd10Type xsd10_types_builtin_[XML_XSD10_SCHEMAS_ANYSIMPLETYPE + 1] = {
    /* 0 XML_XSD10_SCHEMAS_UNKNOWN, unused */
    XSD10_TYPES_DEF(NULL, UNKNOWN, NULL, 0, NULL, NULL, XML_XSD10_SCHEMA_CONTENT_UNKNOWN, NULL),
    XSD10_TYPES_SIMPLE("string", STRING, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("normalizedString", NORMSTRING, STRING, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("decimal", DECIMAL, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("time", TIME, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("gDay", GDAY, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("gMonth", GMONTH, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("gMonthDay", GMONTHDAY, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("gYear", GYEAR, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("gYearMonth", GYEARMONTH, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("date", DATE, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("dateTime", DATETIME, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("duration", DURATION, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("float", FLOAT, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("double", DOUBLE, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("boolean", BOOLEAN, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("token", TOKEN, NORMSTRING, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("language", LANGUAGE, TOKEN, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("NMTOKEN", NMTOKEN, TOKEN, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_LISTDEF("NMTOKENS", NMTOKENS, NMTOKEN, 2),
    XSD10_TYPES_SIMPLE("Name", NAME, TOKEN, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("QName", QNAME, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("NCName", NCNAME, NAME, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("ID", ID, NCNAME, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("IDREF", IDREF, NCNAME, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_LISTDEF("IDREFS", IDREFS, IDREF, 1),
    XSD10_TYPES_SIMPLE("ENTITY", ENTITY, NCNAME, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_LISTDEF("ENTITIES", ENTITIES, ENTITY, 0),
    XSD10_TYPES_SIMPLE("NOTATION", NOTATION, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("anyURI", ANYURI, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("integer", INTEGER, DECIMAL, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("nonPositiveInteger", NPINTEGER, INTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("negativeInteger", NINTEGER, NPINTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("nonNegativeInteger", NNINTEGER, INTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("positiveInteger", PINTEGER, NNINTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("int", INT, LONG, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("unsignedInt", UINT, ULONG, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("long", LONG, INTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("unsignedLong", ULONG, NNINTEGER, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("short", SHORT, INT, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("unsignedShort", USHORT, UINT, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("byte", BYTE, SHORT, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("unsignedByte", UBYTE, USHORT, XSD10_TYPES_ATOMIC),
    XSD10_TYPES_SIMPLE("hexBinary", HEXBINARY, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    XSD10_TYPES_SIMPLE("base64Binary", BASE64BINARY, ANYSIMPLETYPE, XSD10_TYPES_PRIMITIVE),
    /* anyType is its own base type, its content type is mixed. */
    XSD10_TYPES_DEF("anyType", ANYTYPE, XSD10_TYPES_REF(ANYTYPE), 0, NULL,
                    (XmlXsd10Type *) &xsd10_types_any_particle_, XML_XSD10_SCHEMA_CONTENT_MIXED,
                    &xsd10_types_any_attribute_wildcard_),
    XSD10_TYPES_DEF("anySimpleType", ANYSIMPLETYPE, XSD10_TYPES_REF(ANYTYPE), 0, NULL, NULL,
                    XML_XSD10_SCHEMA_CONTENT_BASIC, NULL)
};

#define xsd10_types_float_def_ (&xsd10_types_builtin_[XML_XSD10_SCHEMAS_FLOAT])

/* Port of xmlSchemaInitTypes. */
int xml_xsd10_init_types(void)
{
    return (0);
}

/* Port of xmlSchemaCleanupTypes. */
void xml_xsd10_cleanup_types(void)
{
}

/* Port of xmlSchemaIsBuiltInTypeFacet. */
int
xml_xsd10_is_built_in_type_facet(XmlXsd10Type *type, int facetType)
{
    if (type == NULL)
	return (-1);
    if (type->type != XML_XSD10_SCHEMA_TYPE_BASIC)
	return (-1);
    switch (type->builtInType) {
	case XML_XSD10_SCHEMAS_BOOLEAN:
	    if ((facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
		(facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE))
		return (1);
	    else
		return (0);
	case XML_XSD10_SCHEMAS_STRING:
	case XML_XSD10_SCHEMAS_NOTATION:
	case XML_XSD10_SCHEMAS_QNAME:
	case XML_XSD10_SCHEMAS_ANYURI:
	case XML_XSD10_SCHEMAS_BASE64BINARY:
	case XML_XSD10_SCHEMAS_HEXBINARY:
	    if ((facetType == XML_XSD10_SCHEMA_FACET_LENGTH) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH) ||
		(facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
		(facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
		(facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE))
		return (1);
	    else
		return (0);
	case XML_XSD10_SCHEMAS_DECIMAL:
	    if ((facetType == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) ||
		(facetType == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS) ||
		(facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
		(facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE))
		return (1);
	    else
		return (0);
	case XML_XSD10_SCHEMAS_TIME:
	case XML_XSD10_SCHEMAS_GDAY:
	case XML_XSD10_SCHEMAS_GMONTH:
	case XML_XSD10_SCHEMAS_GMONTHDAY:
	case XML_XSD10_SCHEMAS_GYEAR:
	case XML_XSD10_SCHEMAS_GYEARMONTH:
	case XML_XSD10_SCHEMAS_DATE:
	case XML_XSD10_SCHEMAS_DATETIME:
	case XML_XSD10_SCHEMAS_DURATION:
	case XML_XSD10_SCHEMAS_FLOAT:
	case XML_XSD10_SCHEMAS_DOUBLE:
	    if ((facetType == XML_XSD10_SCHEMA_FACET_PATTERN) ||
		(facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) ||
		(facetType == XML_XSD10_SCHEMA_FACET_WHITESPACE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE) ||
		(facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE))
		return (1);
	    else
		return (0);
	default:
	    break;
    }
    return (0);
}

/* Port of xmlSchemaGetBuiltInType: the type struct for a built-in type by its type id. */
XmlXsd10Type *
xml_xsd10_get_built_in_type(XmlXsd10ValType type)
{
    if ((type >= XML_XSD10_SCHEMAS_STRING) && (type <= XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
        return (&xsd10_types_builtin_[type]);
    return (NULL);
}

/* Port of xmlSchemaGetPredefinedType: lookup a type in the default XML Schemas type library. */
XmlXsd10Type *
xml_xsd10_get_predefined_type(const char *name, const char *ns)
{
    int i;

    if (name == NULL)
	return(NULL);
    /* libxml2 looks up (name, ns) in a hash table where all types have the xsd namespace. */
    if ((ns == NULL) || (strcmp(ns, XSD10_TYPES_NS) != 0))
        return (NULL);
    for (i = XML_XSD10_SCHEMAS_STRING; i <= XML_XSD10_SCHEMAS_ANYSIMPLETYPE; i++) {
        if (strcmp(xsd10_types_builtin_[i].name, name) == 0)
            return (&xsd10_types_builtin_[i]);
    }
    return (NULL);
}

/* Port of xmlSchemaGetBuiltInListSimpleTypeItemType. */
XmlXsd10Type *
xml_xsd10_get_built_in_list_simple_type_item_type(XmlXsd10Type *type)
{
    if ((type == NULL) || (type->type != XML_XSD10_SCHEMA_TYPE_BASIC))
	return (NULL);
    switch (type->builtInType) {
	case XML_XSD10_SCHEMAS_NMTOKENS:
	    return (XSD10_TYPES_REF(NMTOKEN));
	case XML_XSD10_SCHEMAS_IDREFS:
	    return (XSD10_TYPES_REF(IDREF));
	case XML_XSD10_SCHEMAS_ENTITIES:
	    return (XSD10_TYPES_REF(ENTITY));
	default:
	    return (NULL);
    }
}

/************************************************************************
 *									*
 *			Base types support				*
 *									*
 ************************************************************************/

/* Port of xmlSchemaNewValue: allocate a new simple type value. */
static XmlXsd10Val *
xsd10_types_new_value(XmlXsd10ValType type) {
    XmlXsd10Val *value;

    value = (XmlXsd10Val *) malloc(sizeof(XmlXsd10Val));
    if (value == NULL) {
	return(NULL);
    }
    memset(value, 0, sizeof(XmlXsd10Val));
    value->type = type;
    return(value);
}

/* Port of xmlSchemaValDecimalGetFractionalPart. */
static const XmlXsd10Char *
xsd10_types_decimal_get_fractional_part(const XmlXsd10ValDecimal *decimal)
{
    /* 2 = sign+dot */
    return decimal->str+2+decimal->integralPlaces;
}

/* Port of xmlSchemaValDecimalIsInteger. */
static int
xsd10_types_decimal_is_integer(const XmlXsd10ValDecimal *decimal)
{
    return decimal->fractionalPlaces == 1 && xsd10_types_decimal_get_fractional_part(decimal)[0] == '0';
}

/* Port of xmlSchemaValDecimalGetSignificantDigitCount. */
static unsigned long
xsd10_types_decimal_get_significant_digit_count(const XmlXsd10ValDecimal *decimal)
{
	unsigned fractionalPlaces = xsd10_types_decimal_is_integer(decimal) ? 0 : decimal->fractionalPlaces;
	unsigned integralPlaces = decimal->integralPlaces;
	if(integralPlaces == 1 && decimal->str[1] == '0')
	{
		integralPlaces = 0;
	}
	if(integralPlaces+fractionalPlaces == 0)
	{
		/* 0, but that's still 1 significant digit */
		return 1;
	}
	return integralPlaces+fractionalPlaces;
}

/* Port of xmlSchemaValDecimalCompare: compares two decimals, returns a positive value if
   lhs > rhs, negative if lhs < rhs, or 0 if lhs == rhs. */
static int xsd10_types_decimal_compare(const XmlXsd10ValDecimal *lhs, const XmlXsd10ValDecimal *rhs)
{
    int sign = 1;
    /* may be +0 and -0 for some reason, handle */
    if(strcmp((const char*)lhs->str+1, "0.0") == 0 &&
        strcmp((const char*)rhs->str+1, "0.0") == 0)
    {
        return 0;
    }
    /* first take care of sign */
    if(lhs->str[0] != rhs->str[0])
    {
        /* ASCII- > ASCII+ */
        return rhs->str[0]-lhs->str[0];
    }
    /* signs are equal, but if negative the comparison must be reversed */
    if(lhs->str[0] == '-')
    {
        sign = -1;
    }
    /* internal representation never contains leading zeroes, longer decimal representation = larger number */
    if(lhs->integralPlaces != rhs->integralPlaces)
    {
        return ((int)lhs->integralPlaces-(int)rhs->integralPlaces)*sign;
    }
    /* same length, only digits => lexicographical sorting == numerical sorting.
       If integral parts are equal it will compare compare fractional parts. Again, lexicographical is good enough,
       length doesn't matter. We'll be starting from 0.1, always comparing like to like, and NULL < '0'
       If one is shorter and is equal until end, it must be smaller, since there are no trailing zeroes
       and the longer number must therefore have at least one non-zero digit after the other has ended.
       +1 to skip the sign
    */
    return strcmp((const char*)lhs->str+1, (const char*)rhs->str+1)*sign;
}

/* Port of xmlSchemaValDecimalCompareWithInteger. */
static int xsd10_types_decimal_compare_with_integer(const XmlXsd10ValDecimal *lhs, long rhs)
{
    /* can handle integers up to 128 bits, should be good for a while */
    char buf[43];
    XmlXsd10ValDecimal tmpVal;
    /* 3 = sign+dot+0+NULL */
    tmpVal.integralPlaces = snprintf(buf, sizeof(buf), "%+ld.0", rhs)-3;
    tmpVal.str = (XmlXsd10Char*)buf;
    tmpVal.fractionalPlaces = 1;
    return xsd10_types_decimal_compare(lhs, &tmpVal);
}

/* Port of xmlSchemaValueAppend: appends a next sibling to a list of computed values. */
int
xml_xsd10_value_append(XmlXsd10Val *prev, XmlXsd10Val *cur) {

    if ((prev == NULL) || (cur == NULL))
	return (-1);
    prev->next = cur;
    return (0);
}

/* Port of xmlSchemaValueGetNext: accessor for the next sibling of a list of computed values. */
XmlXsd10Val *
xml_xsd10_value_get_next(XmlXsd10Val *cur) {

    if (cur == NULL)
	return (NULL);
    return (cur->next);
}

/* Port of xmlSchemaValueGetAsString: accessor for the string value of a computed value. */
const char *
xml_xsd10_value_get_as_string(XmlXsd10Val *val)
{
    if (val == NULL)
	return (NULL);
    switch (val->type) {
	case XML_XSD10_SCHEMAS_STRING:
	case XML_XSD10_SCHEMAS_NORMSTRING:
	case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
	case XML_XSD10_SCHEMAS_TOKEN:
        case XML_XSD10_SCHEMAS_LANGUAGE:
        case XML_XSD10_SCHEMAS_NMTOKEN:
        case XML_XSD10_SCHEMAS_NAME:
        case XML_XSD10_SCHEMAS_NCNAME:
        case XML_XSD10_SCHEMAS_ID:
        case XML_XSD10_SCHEMAS_IDREF:
        case XML_XSD10_SCHEMAS_ENTITY:
        case XML_XSD10_SCHEMAS_ANYURI:
	    return ((const char *) val->value.str);
	default:
	    break;
    }
    return (NULL);
}

/* Port of xmlSchemaValueGetAsBoolean: 1 if true and 0 if false, or in case of an error. Hmm. */
int
xml_xsd10_value_get_as_boolean(XmlXsd10Val *val)
{
    if ((val == NULL) || (val->type != XML_XSD10_SCHEMAS_BOOLEAN))
	return (0);
    return (val->value.b);
}

/* Port of xmlSchemaNewStringValue: allocate a new simple type value. The type can be
   of XML_XSD10_SCHEMAS_STRING.
   WARNING: This one is intended to be expanded for other
   string based types. We need this for anySimpleType as well.
   The given value is consumed and freed with the struct. */
XmlXsd10Val *
xml_xsd10_new_string_value(XmlXsd10ValType type,
			const char *value)
{
    XmlXsd10Val *val;

    if (type != XML_XSD10_SCHEMAS_STRING)
	return(NULL);
    val = (XmlXsd10Val *) malloc(sizeof(XmlXsd10Val));
    if (val == NULL) {
	return(NULL);
    }
    memset(val, 0, sizeof(XmlXsd10Val));
    val->type = type;
    val->value.str = (XmlXsd10Char *) value;
    return(val);
}

/* Port of xmlSchemaNewNOTATIONValue: allocate a new NOTATION value.
   The given values are consumed and freed with the struct. */
XmlXsd10Val *
xml_xsd10_new_notation_value(const char *name,
			  const char *ns)
{
    XmlXsd10Val *val;

    val = xsd10_types_new_value(XML_XSD10_SCHEMAS_NOTATION);
    if (val == NULL)
	return (NULL);

    val->value.qname.name = (XmlXsd10Char *)name;
    if (ns != NULL)
	val->value.qname.uri = (XmlXsd10Char *)ns;
    return(val);
}

/* Port of xmlSchemaNewQNameValue: allocate a new QName value.
   The given values are consumed and freed with the struct. */
XmlXsd10Val *
xml_xsd10_new_qname_value(const char *namespaceName,
		       const char *localName)
{
    XmlXsd10Val *val;

    val = xsd10_types_new_value(XML_XSD10_SCHEMAS_QNAME);
    if (val == NULL)
	return (NULL);

    val->value.qname.name = (XmlXsd10Char *) localName;
    val->value.qname.uri = (XmlXsd10Char *) namespaceName;
    return(val);
}

/* Port of xmlSchemaFreeValue. */
void
xml_xsd10_free_value(XmlXsd10Val *value) {
    XmlXsd10Val *prev;

    while (value != NULL) {
	switch (value->type) {
	    case XML_XSD10_SCHEMAS_STRING:
	    case XML_XSD10_SCHEMAS_NORMSTRING:
	    case XML_XSD10_SCHEMAS_TOKEN:
	    case XML_XSD10_SCHEMAS_LANGUAGE:
	    case XML_XSD10_SCHEMAS_NMTOKEN:
	    case XML_XSD10_SCHEMAS_NMTOKENS:
	    case XML_XSD10_SCHEMAS_NAME:
	    case XML_XSD10_SCHEMAS_NCNAME:
	    case XML_XSD10_SCHEMAS_ID:
	    case XML_XSD10_SCHEMAS_IDREF:
	    case XML_XSD10_SCHEMAS_IDREFS:
	    case XML_XSD10_SCHEMAS_ENTITY:
	    case XML_XSD10_SCHEMAS_ENTITIES:
	    case XML_XSD10_SCHEMAS_ANYURI:
	    case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
		if (value->value.str != NULL)
		    free(value->value.str);
		break;
	    case XML_XSD10_SCHEMAS_NOTATION:
	    case XML_XSD10_SCHEMAS_QNAME:
		if (value->value.qname.uri != NULL)
		    free(value->value.qname.uri);
		if (value->value.qname.name != NULL)
		    free(value->value.qname.name);
		break;
	    case XML_XSD10_SCHEMAS_HEXBINARY:
		if (value->value.hex.str != NULL)
		    free(value->value.hex.str);
		break;
	    case XML_XSD10_SCHEMAS_BASE64BINARY:
		if (value->value.base64.str != NULL)
		    free(value->value.base64.str);
		break;
	    case XML_XSD10_SCHEMAS_DECIMAL:
	    case XML_XSD10_SCHEMAS_INTEGER:
	    case XML_XSD10_SCHEMAS_NNINTEGER:
	    case XML_XSD10_SCHEMAS_PINTEGER:
	    case XML_XSD10_SCHEMAS_NPINTEGER:
	    case XML_XSD10_SCHEMAS_NINTEGER:
	    case XML_XSD10_SCHEMAS_INT:
	    case XML_XSD10_SCHEMAS_UINT:
	    case XML_XSD10_SCHEMAS_LONG:
	    case XML_XSD10_SCHEMAS_ULONG:
	    case XML_XSD10_SCHEMAS_SHORT:
	    case XML_XSD10_SCHEMAS_USHORT:
	    case XML_XSD10_SCHEMAS_BYTE:
	    case XML_XSD10_SCHEMAS_UBYTE:
		if (value->value.decimal.str != NULL)
		    free(value->value.decimal.str);
		break;
	    default:
		break;
	}
	prev = value;
	value = value->next;
	free(prev);
    }
}

/****************************************************************
 *								*
 *		Convenience macros and functions		*
 *								*
 ****************************************************************/

#define XSD10_TYPES_IS_TZO_CHAR(c)						\
	((c == 0) || (c == 'Z') || (c == '+') || (c == '-'))

#define XSD10_TYPES_VALID_YEAR(yr)          (yr != 0)
#define XSD10_TYPES_VALID_MONTH(mon)        ((mon >= 1) && (mon <= 12))
/* VALID_DAY should only be used when month is unknown */
#define XSD10_TYPES_VALID_DAY(day)          ((day >= 1) && (day <= 31))
#define XSD10_TYPES_VALID_HOUR(hr)          ((hr >= 0) && (hr <= 23))
#define XSD10_TYPES_VALID_MIN(min)          ((min >= 0) && (min <= 59))
#define XSD10_TYPES_VALID_SEC(sec)          ((sec >= 0) && (sec < 60))
#define XSD10_TYPES_VALID_TZO(tzo)          ((tzo >= -840) && (tzo <= 840))
#define XSD10_TYPES_IS_LEAP(y)						\
	(((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0))

static const unsigned int xsd10_types_days_in_month_[12] =
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const unsigned int xsd10_types_days_in_month_leap_[12] =
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define XSD10_TYPES_MAX_DAYINMONTH(yr,mon)                                  \
        (XSD10_TYPES_IS_LEAP(yr) ? xsd10_types_days_in_month_leap_[mon - 1] : xsd10_types_days_in_month_[mon - 1])

#define XSD10_TYPES_VALID_MDAY(dt)						\
	(XSD10_TYPES_IS_LEAP(dt->year) ?				        \
	    (dt->day <= xsd10_types_days_in_month_leap_[dt->mon - 1]) :	        \
	    (dt->day <= xsd10_types_days_in_month_[dt->mon - 1]))

#define XSD10_TYPES_VALID_DATE(dt)						\
	(XSD10_TYPES_VALID_YEAR(dt->year) && XSD10_TYPES_VALID_MONTH(dt->mon) && XSD10_TYPES_VALID_MDAY(dt))

#define XSD10_TYPES_VALID_END_OF_DAY(dt)					\
	((dt)->hour == 24 && (dt)->min == 0 && (dt)->sec == 0)

#define XSD10_TYPES_VALID_TIME(dt)						\
	((((dt->hour < 24) && (dt->min < 60) &&           \
	  XSD10_TYPES_VALID_SEC(dt->sec)) || XSD10_TYPES_VALID_END_OF_DAY(dt)) &&	\
	 XSD10_TYPES_VALID_TZO(dt->tzo))

#define XSD10_TYPES_VALID_DATETIME(dt)					\
	(XSD10_TYPES_VALID_DATE(dt) && XSD10_TYPES_VALID_TIME(dt))

#define XSD10_TYPES_SECS_PER_MIN            60
#define XSD10_TYPES_MINS_PER_HOUR           60
#define XSD10_TYPES_HOURS_PER_DAY           24
#define XSD10_TYPES_SECS_PER_HOUR           (XSD10_TYPES_MINS_PER_HOUR * XSD10_TYPES_SECS_PER_MIN)
#define XSD10_TYPES_SECS_PER_DAY            (XSD10_TYPES_HOURS_PER_DAY * XSD10_TYPES_SECS_PER_HOUR)
#define XSD10_TYPES_MINS_PER_DAY            (XSD10_TYPES_HOURS_PER_DAY * XSD10_TYPES_MINS_PER_HOUR)

static const long xsd10_types_day_in_year_by_month_[12] =
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static const long xsd10_types_day_in_leap_year_by_month_[12] =
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

#define XSD10_TYPES_DAY_IN_YEAR(day, month, year)				\
        ((XSD10_TYPES_IS_LEAP(year) ?					\
                xsd10_types_day_in_leap_year_by_month_[month - 1] :		\
                xsd10_types_day_in_year_by_month_[month - 1]) + day)

/* Port of _xmlSchemaParseGYear: parses a xs:gYear without time zone and fills in the
   appropriate field of the dt structure. str is updated to point just after the
   xs:gYear. It is supposed that dt->year is big enough to contain the year.
   Returns 0 or the error code. */
static int
xsd10_types_parse_gyear (XmlXsd10ValDate *dt, const XmlXsd10Char **str) {
    const XmlXsd10Char *cur = *str, *firstChar;
    int isneg = 0, digcnt = 0;

    if (((*cur < '0') || (*cur > '9')) &&
	(*cur != '-') && (*cur != '+'))
	return -1;

    if (*cur == '-') {
	isneg = 1;
	cur++;
    }

    firstChar = cur;

    while ((*cur >= '0') && (*cur <= '9')) {
        int digit = *cur - '0';

        if (dt->year > LONG_MAX / 10)
            return 2;
	dt->year *= 10;
        if (dt->year > LONG_MAX - digit)
            return 2;
        dt->year += digit;
	cur++;
	digcnt++;
    }

    /* year must be at least 4 digits (CCYY); over 4
     * digits cannot have a leading zero. */
    if ((digcnt < 4) || ((digcnt > 4) && (*firstChar == '0')))
	return 1;

    if (isneg)
	dt->year = - dt->year;

    if (!XSD10_TYPES_VALID_YEAR(dt->year))
	return 2;

    *str = cur;
    return 0;
}

/* Port of PARSE_2_DIGITS: parses a 2-digits integer and updates num with the value. cur is
   updated to point just after the integer. In case of error, invalid is set to TRUE,
   values of num and cur are undefined. */
#define XSD10_TYPES_PARSE_2_DIGITS(num, cur, invalid)			\
	if ((cur[0] < '0') || (cur[0] > '9') ||			\
	    (cur[1] < '0') || (cur[1] > '9'))			\
	    invalid = 1;					\
	else							\
	    num = (cur[0] - '0') * 10 + (cur[1] - '0');		\
	cur += 2;

/* Port of PARSE_FLOAT: parses a float and updates num with the value. cur is
   updated to point just after the float. The float must have a
   2-digits integer part and may or may not have a decimal part.
   In case of error, invalid is set to TRUE, values of num and
   cur are undefined. */
#define XSD10_TYPES_PARSE_FLOAT(num, cur, invalid)				\
	XSD10_TYPES_PARSE_2_DIGITS(num, cur, invalid);			\
	if (!invalid && (*cur == '.')) {			\
	    double mult = 1;				        \
	    cur++;						\
	    if ((*cur < '0') || (*cur > '9'))			\
		invalid = 1;					\
	    while ((*cur >= '0') && (*cur <= '9')) {		\
		mult /= 10;					\
		num += (*cur - '0') * mult;			\
		cur++;						\
	    }							\
	}

/* Port of _xmlSchemaParseGMonth: parses a xs:gMonth without time zone. */
static int
xsd10_types_parse_gmonth (XmlXsd10ValDate *dt, const XmlXsd10Char **str) {
    const XmlXsd10Char *cur = *str;
    int ret = 0;
    unsigned int value = 0;

    XSD10_TYPES_PARSE_2_DIGITS(value, cur, ret);
    if (ret != 0)
	return ret;

    if (!XSD10_TYPES_VALID_MONTH(value))
	return 2;

    dt->mon = value;

    *str = cur;
    return 0;
}

/* Port of _xmlSchemaParseGDay: parses a xs:gDay without time zone. */
static int
xsd10_types_parse_gday (XmlXsd10ValDate *dt, const XmlXsd10Char **str) {
    const XmlXsd10Char *cur = *str;
    int ret = 0;
    unsigned int value = 0;

    XSD10_TYPES_PARSE_2_DIGITS(value, cur, ret);
    if (ret != 0)
	return ret;

    if (!XSD10_TYPES_VALID_DAY(value))
	return 2;

    dt->day = value;
    *str = cur;
    return 0;
}

/* Port of _xmlSchemaParseTime: parses a xs:time without time zone and fills in the
   appropriate fields of the dt structure. str is updated to point just after the
   xs:time. In case of error, values of dt fields are undefined. */
static int
xsd10_types_parse_time (XmlXsd10ValDate *dt, const XmlXsd10Char **str) {
    const XmlXsd10Char *cur = *str;
    int ret = 0;
    int value = 0;

    XSD10_TYPES_PARSE_2_DIGITS(value, cur, ret);
    if (ret != 0)
	return ret;
    if (*cur != ':')
	return 1;
    if (!XSD10_TYPES_VALID_HOUR(value) && value != 24 /* Allow end-of-day hour */)
	return 2;
    cur++;

    /* the ':' insures this string is xs:time */
    dt->hour = value;

    XSD10_TYPES_PARSE_2_DIGITS(value, cur, ret);
    if (ret != 0)
	return ret;
    if (!XSD10_TYPES_VALID_MIN(value))
	return 2;
    dt->min = value;

    if (*cur != ':')
	return 1;
    cur++;

    XSD10_TYPES_PARSE_FLOAT(dt->sec, cur, ret);
    if (ret != 0)
	return ret;

    if (!XSD10_TYPES_VALID_TIME(dt))
	return 2;

    *str = cur;
    return 0;
}

/* Port of _xmlSchemaParseTimeZone: parses a time zone without time zone and fills in the
   appropriate field of the dt structure. str is updated to point just after the time zone. */
static int
xsd10_types_parse_time_zone (XmlXsd10ValDate *dt, const XmlXsd10Char **str) {
    const XmlXsd10Char *cur;
    int ret = 0;

    if (str == NULL)
	return -1;
    cur = *str;

    switch (*cur) {
    case 0:
	dt->tz_flag = 0;
	dt->tzo = 0;
	break;

    case 'Z':
	dt->tz_flag = 1;
	dt->tzo = 0;
	cur++;
	break;

    case '+':
    case '-': {
	int isneg = 0, tmp = 0;
	isneg = (*cur == '-');

	cur++;

	XSD10_TYPES_PARSE_2_DIGITS(tmp, cur, ret);
	if (ret != 0)
	    return ret;
	if (!XSD10_TYPES_VALID_HOUR(tmp))
	    return 2;

	if (*cur != ':')
	    return 1;
	cur++;

	dt->tzo = tmp * 60;

	XSD10_TYPES_PARSE_2_DIGITS(tmp, cur, ret);
	if (ret != 0)
	    return ret;
	if (!XSD10_TYPES_VALID_MIN(tmp))
	    return 2;

	dt->tzo += tmp;
	if (isneg)
	    dt->tzo = - dt->tzo;

	if (!XSD10_TYPES_VALID_TZO(dt->tzo))
	    return 2;

	dt->tz_flag = 1;
	break;
      }
    default:
	return 1;
    }

    *str = cur;
    return 0;
}

/* Port of _xmlSchemaBase64Decode: converts a base64 encoded character to its base 64 value.
   Returns 0-63 (value), 64 (pad), or -1 (not recognized). */
static int
xsd10_types_base64_decode (const XmlXsd10Char ch) {
    if (('A' <= ch) && (ch <= 'Z')) return ch - 'A';
    if (('a' <= ch) && (ch <= 'z')) return ch - 'a' + 26;
    if (('0' <= ch) && (ch <= '9')) return ch - '0' + 52;
    if ('+' == ch) return 62;
    if ('/' == ch) return 63;
    if ('=' == ch) return 64;
    return -1;
}

/****************************************************************
 *								*
 *	XML Schema Dates/Times Datatypes Handling		*
 *								*
 ****************************************************************/

/* Port of xmlSchemaValidateDates: check that dateTime conforms to the lexical space of one
   of the date types. if true a value is computed and returned in val.
   Returns 0 if this validates, a positive error code number otherwise
   and -1 in case of internal or API error. */
static int
xsd10_types_validate_dates (XmlXsd10ValType type,
	                const XmlXsd10Char *dateTime, XmlXsd10Val **val,
			int collapse) {
    XmlXsd10Val *dt;
    int ret;
    const XmlXsd10Char *cur = dateTime;

#define XSD10_TYPES_RETURN_TYPE_IF_VALID(t)					\
    if (XSD10_TYPES_IS_TZO_CHAR(*cur)) {					\
	ret = xsd10_types_parse_time_zone(&(dt->value.date), &cur);	\
	if (ret == 0) {						\
	    if (*cur != 0)					\
		goto error;					\
	    dt->type = t;					\
	    goto done;						\
	}							\
    }

    if (dateTime == NULL)
	return -1;

    if (collapse)
	while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

    if ((*cur != '-') && (*cur < '0') && (*cur > '9'))
	return 1;

    dt = xsd10_types_new_value(XML_XSD10_SCHEMAS_UNKNOWN);
    if (dt == NULL)
	return -1;

    if ((cur[0] == '-') && (cur[1] == '-')) {
	/*
	 * It's an incomplete date (xs:gMonthDay, xs:gMonth or
	 * xs:gDay)
	 */
	cur += 2;

	/* is it an xs:gDay? */
	if (*cur == '-') {
	    if (type == XML_XSD10_SCHEMAS_GMONTH)
		goto error;
	  ++cur;
	    ret = xsd10_types_parse_gday(&(dt->value.date), &cur);
	    if (ret != 0)
		goto error;

	    XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_GDAY);

	    goto error;
	}

	/*
	 * it should be an xs:gMonthDay or xs:gMonth
	 */
	ret = xsd10_types_parse_gmonth(&(dt->value.date), &cur);
	if (ret != 0)
	    goto error;

        /*
         * a '-' char could indicate this type is xs:gMonthDay or
         * a negative time zone offset. Check for xs:gMonthDay first.
         * Also the first three char's of a negative tzo (-MM:SS) can
         * appear to be a valid day; so even if the day portion
         * of the xs:gMonthDay verifies, we must insure it was not
         * a tzo.
         */
        if (*cur == '-') {
            const XmlXsd10Char *rewnd = cur;
            cur++;

	    ret = xsd10_types_parse_gday(&(dt->value.date), &cur);
            if ((ret == 0) && ((*cur == 0) || (*cur != ':'))) {

                /*
                 * we can use the VALID_MDAY macro to validate the month
                 * and day because the leap year test will flag year zero
                 * as a leap year (even though zero is an invalid year).
		 * FUTURE TODO: Zero will become valid in XML Schema 1.1
		 * probably.
                 */
                if (XSD10_TYPES_VALID_MDAY((&(dt->value.date)))) {

	            XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_GMONTHDAY);

                    goto error;
                }
            }

            /*
             * not xs:gMonthDay so rewind and check if just xs:gMonth
             * with an optional time zone.
             */
            cur = rewnd;
        }

	XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_GMONTH);

	goto error;
    }

    /*
     * It's a right-truncated date or an xs:time.
     * Try to parse an xs:time then fallback on right-truncated dates.
     */
    if ((*cur >= '0') && (*cur <= '9')) {
	ret = xsd10_types_parse_time(&(dt->value.date), &cur);
	if (ret == 0) {
	    /* it's an xs:time */
	    XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_TIME);
	}
    }

    /* fallback on date parsing */
    cur = dateTime;

    ret = xsd10_types_parse_gyear(&(dt->value.date), &cur);
    if (ret != 0)
	goto error;

    /* is it an xs:gYear? */
    XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_GYEAR);

    if (*cur != '-')
	goto error;
    cur++;

    ret = xsd10_types_parse_gmonth(&(dt->value.date), &cur);
    if (ret != 0)
	goto error;

    /* is it an xs:gYearMonth? */
    XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_GYEARMONTH);

    if (*cur != '-')
	goto error;
    cur++;

    ret = xsd10_types_parse_gday(&(dt->value.date), &cur);
    if ((ret != 0) || !XSD10_TYPES_VALID_DATE((&(dt->value.date))))
	goto error;

    /* is it an xs:date? */
    XSD10_TYPES_RETURN_TYPE_IF_VALID(XML_XSD10_SCHEMAS_DATE);

    if (*cur != 'T')
	goto error;
    cur++;

    /* it should be an xs:dateTime */
    ret = xsd10_types_parse_time(&(dt->value.date), &cur);
    if (ret != 0)
	goto error;

    ret = xsd10_types_parse_time_zone(&(dt->value.date), &cur);
    if (collapse)
	while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
    if ((ret != 0) || (*cur != 0) || (!(XSD10_TYPES_VALID_DATETIME((&(dt->value.date))))))
	goto error;


    dt->type = XML_XSD10_SCHEMAS_DATETIME;

done:
    if ((type != XML_XSD10_SCHEMAS_UNKNOWN) && (type != dt->type))
        goto error;

    if (val != NULL)
        *val = dt;
    else
	xml_xsd10_free_value(dt);

    return 0;

error:
    if (dt != NULL)
	xml_xsd10_free_value(dt);
    return 1;
#undef XSD10_TYPES_RETURN_TYPE_IF_VALID
}

/* Port of xmlSchemaValidateDuration: check that duration conforms to the lexical space of
   the duration type. if true a value is computed and returned in val.
   Returns 0 if this validates, a positive error code number otherwise
   and -1 in case of internal or API error. */
static int
xsd10_types_validate_duration (XmlXsd10Type *type,
	                   const XmlXsd10Char *duration, XmlXsd10Val **val,
			   int collapse) {
    const XmlXsd10Char  *cur = duration;
    XmlXsd10Val *dur;
    int isneg = 0;
    unsigned int seq = 0;
    long days, secs = 0;
    double sec_frac = 0.0;

    (void) type;
    if (duration == NULL)
	return -1;

    if (collapse)
	while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

    if (*cur == '-') {
        isneg = 1;
        cur++;
    }

    /* duration must start with 'P' (after sign) */
    if (*cur++ != 'P')
	return 1;

    if (*cur == 0)
	return 1;

    dur = xsd10_types_new_value(XML_XSD10_SCHEMAS_DURATION);
    if (dur == NULL)
	return -1;

    while (*cur != 0) {
        long           num = 0;
        size_t         has_digits = 0;
        int            has_frac = 0;
        const XmlXsd10Char  desig[] = {'Y', 'M', 'D', 'H', 'M', 'S'};

        /* input string should be empty or invalid date/time item */
        if (seq >= sizeof(desig))
            goto error;

        /* T designator must be present for time items */
        if (*cur == 'T') {
            if (seq > 3)
                goto error;
            cur++;
            seq = 3;
        } else if (seq == 3)
            goto error;

        /* Parse integral part. */
        while (*cur >= '0' && *cur <= '9') {
            long digit = *cur - '0';

            if (num > LONG_MAX / 10)
                goto error;
            num *= 10;
            if (num > LONG_MAX - digit)
                goto error;
            num += digit;

            has_digits = 1;
            cur++;
        }

        if (*cur == '.') {
            /* Parse fractional part. */
            double mult = 1.0;
            cur++;
            has_frac = 1;
            while (*cur >= '0' && *cur <= '9') {
                mult /= 10.0;
                sec_frac += (*cur - '0') * mult;
                has_digits = 1;
                cur++;
            }
        }

        while (*cur != desig[seq]) {
            seq++;
            /* No T designator or invalid char. */
            if (seq == 3 || seq == sizeof(desig))
                goto error;
        }
	cur++;

        if (!has_digits || (has_frac && (seq != 5)))
            goto error;

        switch (seq) {
            case 0:
                /* Year */
                if (num > LONG_MAX / 12)
                    goto error;
                dur->value.dur.mon = num * 12;
                break;
            case 1:
                /* Month */
                if (dur->value.dur.mon > LONG_MAX - num)
                    goto error;
                dur->value.dur.mon += num;
                break;
            case 2:
                /* Day */
                dur->value.dur.day = num;
                break;
            case 3:
                /* Hour */
                days = num / XSD10_TYPES_HOURS_PER_DAY;
                if (dur->value.dur.day > LONG_MAX - days)
                    goto error;
                dur->value.dur.day += days;
                secs = (num % XSD10_TYPES_HOURS_PER_DAY) * XSD10_TYPES_SECS_PER_HOUR;
                break;
            case 4:
                /* Minute */
                days = num / XSD10_TYPES_MINS_PER_DAY;
                if (dur->value.dur.day > LONG_MAX - days)
                    goto error;
                dur->value.dur.day += days;
                secs += (num % XSD10_TYPES_MINS_PER_DAY) * XSD10_TYPES_SECS_PER_MIN;
                break;
            case 5:
                /* Second */
                days = num / XSD10_TYPES_SECS_PER_DAY;
                if (dur->value.dur.day > LONG_MAX - days)
                    goto error;
                dur->value.dur.day += days;
                secs += num % XSD10_TYPES_SECS_PER_DAY;
                break;
        }

        seq++;
    }

    days = secs / XSD10_TYPES_SECS_PER_DAY;
    if (dur->value.dur.day > LONG_MAX - days)
        goto error;
    dur->value.dur.day += days;
    dur->value.dur.sec = (secs % XSD10_TYPES_SECS_PER_DAY) + sec_frac;

    if (isneg) {
        dur->value.dur.mon = -dur->value.dur.mon;
        dur->value.dur.day = -dur->value.dur.day;
        dur->value.dur.sec = -dur->value.dur.sec;
    }

    if (val != NULL)
        *val = dur;
    else
	xml_xsd10_free_value(dur);

    return 0;

error:
    if (dur != NULL)
	xml_xsd10_free_value(dur);
    return 1;
}

/* Port of xmlSchemaStrip: removes the leading and ending spaces of a string,
   returns the new string or NULL if no change was required. */
static XmlXsd10Char *
xsd10_types_strip(const XmlXsd10Char *value) {
    const XmlXsd10Char *start = value, *end, *f;

    if (value == NULL) return(NULL);
    while ((*start != 0) && (XSD10_TYPES_IS_BLANK_CH(*start))) start++;
    end = start;
    while (*end != 0) end++;
    f = end;
    end--;
    while ((end > start) && (XSD10_TYPES_IS_BLANK_CH(*end))) end--;
    end++;
    if ((start == value) && (f == end)) return(NULL);
    return(xsd10_types_strndup(start, end - start));
}

/* Port of xmlSchemaWhiteSpaceReplace: replaces 0xd, 0x9 and 0xa with a space. */
static XmlXsd10Char *
xsd10_types_white_space_replace(const XmlXsd10Char *value) {
    const XmlXsd10Char *cur = value;
    XmlXsd10Char *ret = NULL, *mcur;

    if (value == NULL)
	return(NULL);

    while ((*cur != 0) &&
	(((*cur) != 0xd) && ((*cur) != 0x9) && ((*cur) != 0xa))) {
	cur++;
    }
    if (*cur == 0)
	return (NULL);
    ret = xsd10_types_strdup(value);
    if (ret == NULL)
        return(NULL);
    /* TODO FIXME: I guess gcc will bark at this. */
    mcur = (XmlXsd10Char *)  (ret + (cur - value));
    do {
	if ( ((*mcur) == 0xd) || ((*mcur) == 0x9) || ((*mcur) == 0xa) )
	    *mcur = ' ';
	mcur++;
    } while (*mcur != 0);
    return(ret);
}

char *
xml_xsd10_white_space_replace(const char *value)
{
    return ((char *) xsd10_types_white_space_replace((const XmlXsd10Char *) value));
}

/* Port of xmlSchemaCollapseString: removes and normalize white spaces in the string,
   returns the new string or NULL if no change was required. */
static XmlXsd10Char *
xsd10_types_collapse_string(const XmlXsd10Char *value) {
    const XmlXsd10Char *start = value, *end, *f;
    XmlXsd10Char *g;
    long col = 0;

    if (value == NULL) return(NULL);
    while ((*start != 0) && (XSD10_TYPES_IS_BLANK_CH(*start))) start++;
    end = start;
    while (*end != 0) {
	if ((*end == ' ') && (XSD10_TYPES_IS_BLANK_CH(end[1]))) {
	    col = end - start;
	    break;
	} else if ((*end == 0xa) || (*end == 0x9) || (*end == 0xd)) {
	    col = end - start;
	    break;
	}
	end++;
    }
    if (col == 0) {
	f = end;
	end--;
	while ((end > start) && (XSD10_TYPES_IS_BLANK_CH(*end))) end--;
	end++;
	if ((start == value) && (f == end)) return(NULL);
	return(xsd10_types_strndup(start, end - start));
    }
    start = xsd10_types_strdup(start);
    if (start == NULL) return(NULL);
    g = (XmlXsd10Char *) (start + col);
    end = g;
    while (*end != 0) {
	if (XSD10_TYPES_IS_BLANK_CH(*end)) {
	    end++;
	    while (XSD10_TYPES_IS_BLANK_CH(*end)) end++;
	    if (*end != 0)
		*g++ = ' ';
	} else
	    *g++ = *end++;
    }
    *g = 0;
    return((XmlXsd10Char *) start);
}

char *
xml_xsd10_collapse_string(const char *value)
{
    return ((char *) xsd10_types_collapse_string((const XmlXsd10Char *) value));
}

static int
xsd10_types_val_atomic_type(XmlXsd10Type *type, const XmlXsd10Char * value,
                       XmlXsd10Val ** val, XMQNode *node, int flags,
		       XmlXsd10WhitespaceValueType ws,
		       int normOnTheFly, int applyNorm, int createStringValue,
		       XmlXsd10Ids *ids);

/* Port of xmlSchemaValAtomicListNode: check that a value conforms to the lexical space of the
   predefined list type. if true a value is computed and returned in ret.
   Returns the number of items if this validates, a negative error code number otherwise. */
static int
xsd10_types_val_atomic_list_node(XmlXsd10Type *type, const XmlXsd10Char *value,
	                   XmlXsd10Val **ret, XMQNode *node, XmlXsd10Ids *ids) {
    XmlXsd10Char *val, *cur, *endval;
    int nb_values = 0;
    int tmp = 0;

    if (value == NULL) {
	return(-1);
    }
    val = xsd10_types_strdup(value);
    if (val == NULL) {
	return(-1);
    }
    if (ret != NULL) {
        *ret = NULL;
    }
    cur = val;
    /*
     * Split the list
     */
    while (XSD10_TYPES_IS_BLANK_CH(*cur)) *cur++ = 0;
    while (*cur != 0) {
	if (XSD10_TYPES_IS_BLANK_CH(*cur)) {
	    *cur = 0;
	    cur++;
	    while (XSD10_TYPES_IS_BLANK_CH(*cur)) *cur++ = 0;
	} else {
	    nb_values++;
	    cur++;
	    while ((*cur != 0) && (!XSD10_TYPES_IS_BLANK_CH(*cur))) cur++;
	}
    }
    if (nb_values == 0) {
	free(val);
	return(nb_values);
    }
    endval = cur;
    cur = val;
    while ((*cur == 0) && (cur != endval)) cur++;
    while (cur != endval) {
	/* xmlSchemaValPredefTypeNode */
	tmp = xsd10_types_val_atomic_type(type, cur, NULL, node, 0,
	    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, ids);
	if (tmp != 0)
	    break;
	while (*cur != 0) cur++;
	while ((*cur == 0) && (cur != endval)) cur++;
    }
    /* TODO what return value ? c.f. bug #158628
    if (ret != NULL) {
	TODO
    } */
    free(val);
    if (tmp == 0)
	return(nb_values);
    return(-1);
}

/* Port of xmlSchemaParseUInt: parse an unsigned long into a decimal.
   Returns the number of significant digits in the number or
   -1 if overflow of the capacity and -2 if it's not a number. */
static int xsd10_types_parse_uint(const XmlXsd10Char **str, XmlXsd10ValDecimal *val) {
	const XmlXsd10Char *tmp, *cur = *str;
	int ret = 0, i = 0;

    if (!((*cur >= '0') && (*cur <= '9')))
        return(-2);

    while (*cur == '0') {        /* ignore leading zeroes */
        cur++;
    }
    /* back up in case there is nothing after the leading zeroes */
    if(!(*cur >= '0' && *cur <= '9'))
    {
        --cur;
    }
    tmp = cur;
    while ((*tmp != 0) && (*tmp >= '0') && (*tmp <= '9')) {
        i++;tmp++;ret++;
    }
    if (val->integralPlaces + val->fractionalPlaces < (unsigned)i + 1)
    {
        if (val->str != NULL)
        {
            free(val->str);
        }
        /*  sign, dot, fractional 0 and NULL terminator */
        val->str = (XmlXsd10Char *) malloc(i + 4);
        if (val->str == NULL)
            return(-1);
    }
    val->fractionalPlaces = 1;
    val->integralPlaces = i;
    snprintf((char *)val->str, i + 4, "+%.*s.0", i, cur);

    *str = tmp;
    return(ret);
}

/* Port of xmlSchemaCheckLanguageType: check that a value conforms to the lexical space of the
   language datatype. Must conform to [a-zA-Z]{1,8}(-[a-zA-Z0-9]{1,8})*
   Returns 1 if this validates, 0 otherwise. */
static int
xsd10_types_check_language_type(const XmlXsd10Char* value) {
    int first = 1, len = 0;
    const XmlXsd10Char* cur = value;

    if (value == NULL)
        return (0);

    while (cur[0] != 0) {
        if (!( ((cur[0] >= 'a') && (cur[0] <= 'z')) || ((cur[0] >= 'A') && (cur[0] <= 'Z'))
            || (cur[0] == '-')
            || ((first == 0) && ((cur[0] >= '0') && (cur[0] <= '9'))) ))
            return (0);
        if (cur[0] == '-') {
            if ((len < 1) || (len > 8))
                return (0);
            len = 0;
            first = 0;
        }
        else
            len++;
        cur++;
    }
    if ((len < 1) || (len > 8))
        return (0);

    return (1);
}

/* Replaces libxml2 xmlGetDocEntity + etype check: is name an unparsed entity declared in the
   dtd of the document of node? */
static int
xsd10_types_is_unparsed_entity(XMQNode *node, const XmlXsd10Char *name)
{
    XMQNode *ent = xml_dom_get_entity(xml_node_dom(node), (const char *) name);
    if (ent == NULL)
        return (0);
    return (xml_entity_decl_type(ent) == XML_ENTITY_EXTERNAL_GENERAL_UNPARSED);
}

/* Replaces libxml2 xmlGetDtdNotationDesc(doc->intSubset, name). */
static int
xsd10_types_has_notation(XMQNode *node, const XmlXsd10Char *name)
{
    XMQNode *dtd = xml_dom_dtd(xml_node_dom(node));
    XMQNode *n;

    if (dtd == NULL)
        return (0);
    for (n = xml_dtd_first_notation(dtd); n != NULL; n = xml_node_next(n)) {
        const char *nn = xml_node_name(n);
        if ((nn != NULL) && (strcmp(nn, (const char *) name) == 0))
            return (1);
    }
    return (0);
}

/* Port of xmlSchemaValAtomicType: check that a value conforms to the lexical space of the
   atomic type. if true a value is computed and returned in val.
   This checks the value space for list types as well (IDREFS, NMTOKENS).
   ids replaces the ID table of the libxml2 document (may be NULL).
   Returns 0 if this validates, a positive error code number otherwise
   and -1 in case of internal or API error. */
static int
xsd10_types_val_atomic_type(XmlXsd10Type *type, const XmlXsd10Char * value,
                       XmlXsd10Val ** val, XMQNode *node, int flags,
		       XmlXsd10WhitespaceValueType ws,
		       int normOnTheFly, int applyNorm, int createStringValue,
		       XmlXsd10Ids *ids)
{
    XmlXsd10Val *v;
    XmlXsd10Char *norm = NULL;
    int ret = 0;

    if (type == NULL)
        return (-1);

    /*
     * validating a non existent text node is similar to validating
     * an empty one.
     */
    if (value == NULL)
        value = XSD10_TYPES_BAD_CAST "";

    if (val != NULL)
        *val = NULL;
    if ((flags == 0) && (value != NULL)) {

        if ((type->builtInType != XML_XSD10_SCHEMAS_STRING) &&
	  (type->builtInType != XML_XSD10_SCHEMAS_ANYTYPE) &&
	  (type->builtInType != XML_XSD10_SCHEMAS_ANYSIMPLETYPE)) {
	    if (type->builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
		norm = xsd10_types_white_space_replace(value);
            else
		norm = xsd10_types_collapse_string(value);
            if (norm != NULL)
                value = norm;
        }
    }

    switch (type->builtInType) {
        case XML_XSD10_SCHEMAS_UNKNOWN:
            goto error;
	case XML_XSD10_SCHEMAS_ANYTYPE:
	case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
	    if ((createStringValue) && (val != NULL)) {
		v = xsd10_types_new_value(XML_XSD10_SCHEMAS_ANYSIMPLETYPE);
		if (v != NULL) {
		    v->value.str = xsd10_types_strdup(value);
		    *val = v;
		} else {
		    goto error;
		}
	    }
	    goto return0;
        case XML_XSD10_SCHEMAS_STRING:
	    if (! normOnTheFly) {
		const XmlXsd10Char *cur = value;

		if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE) {
		    while (*cur != 0) {
			if ((*cur == 0xd) || (*cur == 0xa) || (*cur == 0x9)) {
			    goto return1;
			} else {
			    cur++;
			}
		    }
		} else if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE) {
		    while (*cur != 0) {
			if ((*cur == 0xd) || (*cur == 0xa) || (*cur == 0x9)) {
			    goto return1;
			} else if XSD10_TYPES_IS_WSP_SPACE_CH(*cur) {
			    cur++;
			    if XSD10_TYPES_IS_WSP_SPACE_CH(*cur)
				goto return1;
			} else {
			    cur++;
			}
		    }
		}
	    }
	    if (createStringValue && (val != NULL)) {
		if (applyNorm) {
		    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			norm = xsd10_types_collapse_string(value);
		    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
			norm = xsd10_types_white_space_replace(value);
		    if (norm != NULL)
			value = norm;
		}
		v = xsd10_types_new_value(XML_XSD10_SCHEMAS_STRING);
		if (v != NULL) {
		    v->value.str = xsd10_types_strdup(value);
		    *val = v;
		} else {
		    goto error;
		}
	    }
            goto return0;
        case XML_XSD10_SCHEMAS_NORMSTRING:{
		if (normOnTheFly) {
		    if (applyNorm) {
			/* libxml2 leaks the value normalized above here. */
			XmlXsd10Char *prev = norm;
			if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			    norm = xsd10_types_collapse_string(value);
			else
			    norm = xsd10_types_white_space_replace(value);
			if (norm != NULL)
			    value = norm;
			else
			    norm = prev;
			if ((prev != NULL) && (prev != norm))
			    free(prev);
		    }
		} else {
		    const XmlXsd10Char *cur = value;
		    while (*cur != 0) {
			if ((*cur == 0xd) || (*cur == 0xa) || (*cur == 0x9)) {
			    goto return1;
			} else {
			    cur++;
			}
		    }
		}
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_NORMSTRING);
                    if (v != NULL) {
                        v->value.str = xsd10_types_strdup(value);
                        *val = v;
                    } else {
                        goto error;
                    }
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_DECIMAL:{
                const XmlXsd10Char *cur = value;
                const XmlXsd10Char *numStart, *numEnd;
                XmlXsd10ValDecimal decimal;
                XmlXsd10Char sign;

                memset(&decimal, 0, sizeof(decimal));

                if ((cur == NULL) || (*cur == 0))
                    goto return1;

		/*
		* xs:decimal has a whitespace-facet value of 'collapse'.
		*/
		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

		/*
		* First we handle an optional sign.
		*/
                sign = '+';
                if (*cur == '-') {
                    sign = '-';
                    cur++;
		} else if (*cur == '+')
                    cur++;
		/*
		* Disallow: "", "-", "- "
		*/
		if (*cur == 0)
		    goto return1;

		/*
		* Skip leading zeroes.
		*/
		while (*cur == '0') {
		    cur++;
		}

                numStart = cur;

                while ((*cur >= '0') && (*cur <= '9')) {
                    ++cur;
                    ++decimal.integralPlaces;
                }
                if (*cur == '.') {
                    ++cur;
                }
                while ((*cur >= '0') && (*cur <= '9')) {
                    ++cur;
                    ++decimal.fractionalPlaces;
                }

                /*  disallow "." */
                if (
                    decimal.fractionalPlaces == 0 && decimal.integralPlaces == 0
                    && (numStart == value || numStart[-1] != '0')
                ) {
                    goto return1;
                }

                numEnd = cur;

                /*  find if there are trailing FRACTIONAL zeroes, and deal with them if necessary */
                while (numEnd > numStart && decimal.fractionalPlaces && numEnd[-1] == '0') {
                    --numEnd;
                    --decimal.fractionalPlaces;
                }

		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
		if (*cur != 0)
		    goto return1; /* error if any extraneous chars */
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_DECIMAL);
                    if (v != NULL) {
                        /*  create a standardized representation */
                        size_t bufsize;
                        const char *integralStart = (const char *)numStart;
                        const char *fractionalStart = (const char *)numEnd - decimal.fractionalPlaces;
                        if (decimal.integralPlaces == 0)
                        {
                            integralStart = "0";
                            decimal.integralPlaces = 1;
                        }
                        if (decimal.fractionalPlaces == 0)
                        {
                            fractionalStart = "0";
                            decimal.fractionalPlaces = 1;
                        }
                        /*  3 = sign, dot, NULL terminator */
                        bufsize = decimal.integralPlaces + decimal.fractionalPlaces + 3;
                        decimal.str = (XmlXsd10Char *) malloc(bufsize);
                        if (!decimal.str)
                        {
                            xml_xsd10_free_value(v);
                            goto error;
                        }
                        snprintf((char *)decimal.str, bufsize, "%c%.*s.%.*s", sign, (int) decimal.integralPlaces, integralStart,
                                (int) decimal.fractionalPlaces, fractionalStart);
                        v->value.decimal = decimal;
                        *val = v;
                    }
                    else
                    {
                        goto error;
                    }
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_TIME:
        case XML_XSD10_SCHEMAS_GDAY:
        case XML_XSD10_SCHEMAS_GMONTH:
        case XML_XSD10_SCHEMAS_GMONTHDAY:
        case XML_XSD10_SCHEMAS_GYEAR:
        case XML_XSD10_SCHEMAS_GYEARMONTH:
        case XML_XSD10_SCHEMAS_DATE:
        case XML_XSD10_SCHEMAS_DATETIME:
            ret = xsd10_types_validate_dates((XmlXsd10ValType) type->builtInType, value, val,
		normOnTheFly);
            break;
        case XML_XSD10_SCHEMAS_DURATION:
            ret = xsd10_types_validate_duration(type, value, val,
		normOnTheFly);
            break;
        case XML_XSD10_SCHEMAS_FLOAT:
        case XML_XSD10_SCHEMAS_DOUBLE: {
                const XmlXsd10Char *cur = value;
                int neg = 0;
                int digits_before = 0;
                int digits_after = 0;

		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

                if ((cur[0] == 'N') && (cur[1] == 'a') && (cur[2] == 'N')) {
                    cur += 3;
                    if (*cur != 0)
                        goto return1;
                    if (val != NULL) {
                        if (type == xsd10_types_float_def_) {
                            v = xsd10_types_new_value(XML_XSD10_SCHEMAS_FLOAT);
                            if (v != NULL) {
                                v->value.f = (float) NAN;
                            } else {
                                xml_xsd10_free_value(v);
                                goto error;
                            }
                        } else {
                            v = xsd10_types_new_value(XML_XSD10_SCHEMAS_DOUBLE);
                            if (v != NULL) {
                                v->value.d = NAN;
                            } else {
                                xml_xsd10_free_value(v);
                                goto error;
                            }
                        }
                        *val = v;
                    }
                    goto return0;
                }
                if (*cur == '-') {
                    neg = 1;
                    cur++;
                }
                if ((cur[0] == 'I') && (cur[1] == 'N') && (cur[2] == 'F')) {
                    cur += 3;
                    if (*cur != 0)
                        goto return1;
                    if (val != NULL) {
                        if (type == xsd10_types_float_def_) {
                            v = xsd10_types_new_value(XML_XSD10_SCHEMAS_FLOAT);
                            if (v != NULL) {
                                if (neg)
                                    v->value.f = (float) -INFINITY;
                                else
                                    v->value.f = (float) INFINITY;
                            } else {
                                xml_xsd10_free_value(v);
                                goto error;
                            }
                        } else {
                            v = xsd10_types_new_value(XML_XSD10_SCHEMAS_DOUBLE);
                            if (v != NULL) {
                                if (neg)
                                    v->value.d = -INFINITY;
                                else
                                    v->value.d = INFINITY;
                            } else {
                                xml_xsd10_free_value(v);
                                goto error;
                            }
                        }
                        *val = v;
                    }
                    goto return0;
                }
                if ((neg == 0) && (*cur == '+'))
                    cur++;
                if ((cur[0] == 0) || (cur[0] == '+') || (cur[0] == '-'))
                    goto return1;
                while ((*cur >= '0') && (*cur <= '9')) {
                    cur++;
                    digits_before++;
                }
                if (*cur == '.') {
                    cur++;
                    while ((*cur >= '0') && (*cur <= '9')) {
                        cur++;
                        digits_after++;
                    }
                }
                if ((digits_before == 0) && (digits_after == 0))
                    goto return1;
                if ((*cur == 'e') || (*cur == 'E')) {
                    cur++;
                    if ((*cur == '-') || (*cur == '+'))
                        cur++;
                    while ((*cur >= '0') && (*cur <= '9'))
                        cur++;
                }
		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

                if (*cur != 0)
                    goto return1;
                if (val != NULL) {
                    if (type == xsd10_types_float_def_) {
                        v = xsd10_types_new_value(XML_XSD10_SCHEMAS_FLOAT);
                        if (v != NULL) {
			    /*
			    * TODO: sscanf seems not to give the correct
			    * value for extremely high/low values.
			    * E.g. "1E-149" results in zero.
			    */
                            if (sscanf((const char *) value, "%f",
                                 &(v->value.f)) == 1) {
                                *val = v;
                            } else {
                                xml_xsd10_free_value(v);
                                goto return1;
                            }
                        } else {
                            goto error;
                        }
                    } else {
                        v = xsd10_types_new_value(XML_XSD10_SCHEMAS_DOUBLE);
                        if (v != NULL) {
			    /*
			    * TODO: sscanf seems not to give the correct
			    * value for extremely high/low values.
			    */
                            if (sscanf((const char *) value, "%lf",
                                 &(v->value.d)) == 1) {
                                *val = v;
                            } else {
                                xml_xsd10_free_value(v);
                                goto return1;
                            }
                        } else {
                            goto error;
                        }
                    }
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_BOOLEAN:{
                const XmlXsd10Char *cur = value;

		if (normOnTheFly) {
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
		    if (*cur == '0') {
			ret = 0;
			cur++;
		    } else if (*cur == '1') {
			ret = 1;
			cur++;
		    } else if (*cur == 't') {
			cur++;
			if ((*cur++ == 'r') && (*cur++ == 'u') &&
			    (*cur++ == 'e')) {
			    ret = 1;
			} else
			    goto return1;
		    } else if (*cur == 'f') {
			cur++;
			if ((*cur++ == 'a') && (*cur++ == 'l') &&
			    (*cur++ == 's') && (*cur++ == 'e')) {
			    ret = 0;
			} else
			    goto return1;
		    } else
			goto return1;
		    if (*cur != 0) {
			while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
			if (*cur != 0)
			    goto return1;
		    }
		} else {
		    if ((cur[0] == '0') && (cur[1] == 0))
			ret = 0;
		    else if ((cur[0] == '1') && (cur[1] == 0))
			ret = 1;
		    else if ((cur[0] == 't') && (cur[1] == 'r')
			&& (cur[2] == 'u') && (cur[3] == 'e')
			&& (cur[4] == 0))
			ret = 1;
		    else if ((cur[0] == 'f') && (cur[1] == 'a')
			&& (cur[2] == 'l') && (cur[3] == 's')
			&& (cur[4] == 'e') && (cur[5] == 0))
			ret = 0;
		    else
			goto return1;
		}
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_BOOLEAN);
                    if (v != NULL) {
                        v->value.b = ret;
                        *val = v;
                    } else {
                        goto error;
                    }
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_TOKEN:{
                const XmlXsd10Char *cur = value;

		if (! normOnTheFly) {
		    while (*cur != 0) {
			if ((*cur == 0xd) || (*cur == 0xa) || (*cur == 0x9)) {
			    goto return1;
			} else if (*cur == ' ') {
			    cur++;
			    if (*cur == 0)
				goto return1;
			    if (*cur == ' ')
				goto return1;
			} else {
			    cur++;
			}
		    }
		}
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_TOKEN);
                    if (v != NULL) {
                        v->value.str = xsd10_types_strdup(value);
                        *val = v;
                    } else {
                        goto error;
                    }
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_LANGUAGE:
	    if ((norm == NULL) && (normOnTheFly)) {
		norm = xsd10_types_collapse_string(value);
		if (norm != NULL)
		    value = norm;
	    }

            if (xsd10_types_check_language_type(value) == 1) {
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_LANGUAGE);
                    if (v != NULL) {
                        v->value.str = xsd10_types_strdup(value);
                        *val = v;
                    } else {
                        goto error;
                    }
                }
                goto return0;
            }
            goto return1;
        case XML_XSD10_SCHEMAS_NMTOKEN:
            if (xsd10_types_validate_nmtoken(value, 1) == 0) {
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_NMTOKEN);
                    if (v != NULL) {
                        v->value.str = xsd10_types_strdup(value);
                        *val = v;
                    } else {
                        goto error;
                    }
                }
                goto return0;
            }
            goto return1;
        case XML_XSD10_SCHEMAS_NMTOKENS:
            ret = xsd10_types_val_atomic_list_node(XSD10_TYPES_REF(NMTOKEN),
                                             value, val, node, ids);
            if (ret > 0)
                ret = 0;
            else
                ret = 1;
            goto done;
        case XML_XSD10_SCHEMAS_NAME:
            ret = xsd10_types_validate_name(value, 1);
            if ((ret == 0) && (val != NULL) && (value != NULL)) {
		v = xsd10_types_new_value(XML_XSD10_SCHEMAS_NAME);
		if (v != NULL) {
		     const XmlXsd10Char *start = value, *end;
		     while (XSD10_TYPES_IS_BLANK_CH(*start)) start++;
		     end = start;
		     while ((*end != 0) && (!XSD10_TYPES_IS_BLANK_CH(*end))) end++;
		     v->value.str = xsd10_types_strndup(start, end - start);
		    *val = v;
		} else {
		    goto error;
		}
            }
            goto done;
        case XML_XSD10_SCHEMAS_QNAME:{
                const XmlXsd10Char *uri = NULL;
                XmlXsd10Char *local = NULL;

                ret = xsd10_types_validate_qname(value, 1);
		if (ret != 0)
		    goto done;
                if (node != NULL) {
                    XmlXsd10Char *prefix;
		    int found;

                    local = xsd10_types_split_qname2(value, &prefix);
		    uri = xsd10_types_search_ns(node, prefix, &found);
		    if ((!found) && (prefix != NULL)) {
			free(prefix);
			if (local != NULL)
			    free(local);
			goto return1;
		    }
                    if (prefix != NULL)
                        free(prefix);
                }
                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_QNAME);
                    if (v == NULL) {
			if (local != NULL)
			    free(local);
			goto error;
		    }
		    if (local != NULL)
			v->value.qname.name = local;
		    else
			v->value.qname.name = xsd10_types_strdup(value);
		    if (uri != NULL)
			v->value.qname.uri = xsd10_types_strdup(uri);
		    *val = v;
                } else
		    if (local != NULL)
			free(local);
                goto done;
            }
        case XML_XSD10_SCHEMAS_NCNAME:
            ret = xsd10_types_validate_ncname(value, 1);
            if ((ret == 0) && (val != NULL)) {
                v = xsd10_types_new_value(XML_XSD10_SCHEMAS_NCNAME);
                if (v != NULL) {
                    v->value.str = xsd10_types_strdup(value);
                    *val = v;
                } else {
                    goto error;
                }
            }
            goto done;
        case XML_XSD10_SCHEMAS_ID:
            ret = xsd10_types_validate_ncname(value, 1);
            if ((ret == 0) && (val != NULL)) {
                v = xsd10_types_new_value(XML_XSD10_SCHEMAS_ID);
                if (v != NULL) {
                    v->value.str = xsd10_types_strdup(value);
                    *val = v;
                } else {
                    goto error;
                }
            }
            if ((ret == 0) && (node != NULL) && (ids != NULL) &&
                (xml_node_kind(node) == XML_DOM_ATTRIBUTE)) {
                /*
                 * NOTE: the IDness might have already be declared in the DTD
                 */
                if (xml_xsd10_ids_attr_type(ids, node) != XML_XSD10_ATTRIBUTE_ID) {
                    XmlXsd10Char *strip;
                    int res;

                    strip = xsd10_types_strip(value);
                    if (strip != NULL) {
                        res = xml_xsd10_ids_add_id(ids, node, (const char *) strip);
                        free(strip);
                    } else
                        res = xml_xsd10_ids_add_id(ids, node, (const char *) value);
                    if (res < 0) {
                        goto error;
                    } else if (res == 0) {
                        ret = 2;
                    }
                }
            }
            goto done;
        case XML_XSD10_SCHEMAS_IDREF:
            ret = xsd10_types_validate_ncname(value, 1);
            if ((ret == 0) && (val != NULL)) {
		v = xsd10_types_new_value(XML_XSD10_SCHEMAS_IDREF);
		if (v == NULL)
		    goto error;
		v->value.str = xsd10_types_strdup(value);
		*val = v;
            }
            if ((ret == 0) && (node != NULL) && (ids != NULL) &&
                (xml_node_kind(node) == XML_DOM_ATTRIBUTE)) {
                XmlXsd10Char *strip;

                strip = xsd10_types_strip(value);
                if (strip != NULL) {
                    xml_xsd10_ids_add_ref(ids, node, (const char *) strip);
                    free(strip);
                } else
                    xml_xsd10_ids_add_ref(ids, node, (const char *) value);
                xml_xsd10_ids_set_attr_type(ids, node, XML_XSD10_ATTRIBUTE_IDREF);
            }
            goto done;
        case XML_XSD10_SCHEMAS_IDREFS:
            ret = xsd10_types_val_atomic_list_node(XSD10_TYPES_REF(IDREF),
                                             value, val, node, ids);
            if (ret < 0)
                ret = 2;
            else
                ret = 0;
            if ((ret == 0) && (node != NULL) &&
                (xml_node_kind(node) == XML_DOM_ATTRIBUTE)) {
                xml_xsd10_ids_set_attr_type(ids, node, XML_XSD10_ATTRIBUTE_IDREFS);
            }
            goto done;
        case XML_XSD10_SCHEMAS_ENTITY:{
                XmlXsd10Char *strip;

                ret = xsd10_types_validate_ncname(value, 1);
                if (node == NULL)
                    ret = 3;
                if (ret == 0) {
                    int ent;

                    strip = xsd10_types_strip(value);
                    if (strip != NULL) {
                        ent = xsd10_types_is_unparsed_entity(node, strip);
                        free(strip);
                    } else {
                        ent = xsd10_types_is_unparsed_entity(node, value);
                    }
                    if (!ent)
                        ret = 4;
                }
                if ((ret == 0) && (val != NULL)) {
                    /* TODO */
                }
                if ((ret == 0) && (node != NULL) &&
                    (xml_node_kind(node) == XML_DOM_ATTRIBUTE)) {
                    xml_xsd10_ids_set_attr_type(ids, node, XML_XSD10_ATTRIBUTE_ENTITY);
                }
                goto done;
            }
        case XML_XSD10_SCHEMAS_ENTITIES:
            if (node == NULL)
                goto return3;
            ret = xsd10_types_val_atomic_list_node(XSD10_TYPES_REF(ENTITY),
                                             value, val, node, ids);
            if (ret <= 0)
                ret = 1;
            else
                ret = 0;
            if ((ret == 0) && (node != NULL) &&
                (xml_node_kind(node) == XML_DOM_ATTRIBUTE)) {
                xml_xsd10_ids_set_attr_type(ids, node, XML_XSD10_ATTRIBUTE_ENTITIES);
            }
            goto done;
        case XML_XSD10_SCHEMAS_NOTATION:{
                XmlXsd10Char *uri = NULL;
                XmlXsd10Char *local = NULL;

                ret = xsd10_types_validate_qname(value, 1);
                if ((ret == 0) && (node != NULL)) {
                    XmlXsd10Char *prefix;

                    local = xsd10_types_split_qname2(value, &prefix);
                    if (prefix != NULL) {
                        const XmlXsd10Char *href;
                        int found;

                        href = xsd10_types_search_ns(node, prefix, &found);
                        if (!found)
                            ret = 1;
                        else if (val != NULL)
                            uri = xsd10_types_strdup(href);
                    }
                    if ((local != NULL) && ((val == NULL) || (ret != 0))) {
                        free(local);
                        local = NULL;
                    }
                    if (prefix != NULL)
                        free(prefix);
                }
                if (node == NULL)
                    ret = 3;
                if (ret == 0) {
                    if (xsd10_types_has_notation(node, value))
                        ret = 0;
                    else
                        ret = 1;
                }
                if ((ret == 0) && (val != NULL)) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_NOTATION);
                    if (v != NULL) {
                        if (local != NULL)
                            v->value.qname.name = local;
                        else
                            v->value.qname.name = xsd10_types_strdup(value);
                        if (uri != NULL)
                            v->value.qname.uri = uri;

                        *val = v;
                    } else {
                        if (local != NULL)
                            free(local);
                        if (uri != NULL)
                            free(uri);
                        goto error;
                    }
                } else {
                    /* libxml2 leaks these here. */
                    if (local != NULL)
                        free(local);
                    if (uri != NULL)
                        free(uri);
                }
                goto done;
            }
        case XML_XSD10_SCHEMAS_ANYURI:{
                if (*value != 0) {
		    XmlURI *uri;
		    XmlXsd10Char *tmpval, *cur;
		    if ((norm == NULL) && (normOnTheFly)) {
			norm = xsd10_types_collapse_string(value);
			if (norm != NULL)
			    value = norm;
		    }
		    tmpval = xsd10_types_strdup(value);
                    if (tmpval == NULL)
                        goto error;
		    for (cur = tmpval; *cur; ++cur) {
			if (*cur < 32 || *cur >= 127 || *cur == ' ' ||
			    *cur == '<' || *cur == '>' || *cur == '"' ||
			    *cur == '{' || *cur == '}' || *cur == '|' ||
			    *cur == '\\' || *cur == '^' || *cur == '`' ||
			    *cur == '\'')
			    *cur = '_';
		    }
                    uri = xml_uri_parse((const char *) tmpval);
		    free(tmpval);
                    if (uri == NULL)
                        goto return1;
                    xml_uri_free(uri);
                }

                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_ANYURI);
                    if (v == NULL)
                        goto error;
                    v->value.str = xsd10_types_strdup(value);
                    *val = v;
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_HEXBINARY:{
                const XmlXsd10Char *cur = value, *start;
                XmlXsd10Char *base;
                int total, i = 0;

                if (cur == NULL)
                    goto return1;

		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

		start = cur;
                while (((*cur >= '0') && (*cur <= '9')) ||
                       ((*cur >= 'A') && (*cur <= 'F')) ||
                       ((*cur >= 'a') && (*cur <= 'f'))) {
                    i++;
                    cur++;
                }
		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;

                if (*cur != 0)
                    goto return1;
                if ((i % 2) != 0)
                    goto return1;

                if (val != NULL) {

                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_HEXBINARY);
                    if (v == NULL)
                        goto error;
		    /*
		    * Copy only the normalized piece.
		    * CRITICAL TODO: Check this.
		    */
                    cur = xsd10_types_strndup(start, i);
                    if (cur == NULL) {
                        free(v);
                        goto return1;
                    }

                    total = i / 2;      /* number of octets */

                    base = (XmlXsd10Char *) cur;
                    while (i-- > 0) {
                        if (*base >= 'a')
                            *base = *base - ('a' - 'A');
                        base++;
                    }

                    v->value.hex.str = (XmlXsd10Char *) cur;
                    v->value.hex.total = total;
                    *val = v;
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_BASE64BINARY:{
                /* ISSUE:
                 *
                 * Ignore all stray characters? (yes, currently)
                 * Worry about long lines? (no, currently)
                 *
                 * rfc2045.txt:
                 *
                 * "The encoded output stream must be represented in lines of
                 * no more than 76 characters each.  All line breaks or other
                 * characters not found in Table 1 must be ignored by decoding
                 * software.  In base64 data, characters other than those in
                 * Table 1, line breaks, and other white space probably
                 * indicate a transmission error, about which a warning
                 * message or even a message rejection might be appropriate
                 * under some circumstances." */
                const XmlXsd10Char *cur = value;
                XmlXsd10Char *base;
                int total, i = 0, pad = 0;

                if (cur == NULL)
                    goto return1;

                for (; *cur; ++cur) {
                    int decc;

                    decc = xsd10_types_base64_decode(*cur);
                    if (decc < 0) ;
                    else if (decc < 64)
                        i++;
                    else
                        break;
                }
                for (; *cur; ++cur) {
                    int decc;

                    decc = xsd10_types_base64_decode(*cur);
                    if (decc < 0) ;
                    else if (decc < 64)
                        goto return1;
                    if (decc == 64)
                        pad++;
                }

                /* rfc2045.txt: "Special processing is performed if fewer than
                 * 24 bits are available at the end of the data being encoded.
                 * A full encoding quantum is always completed at the end of a
                 * body.  When fewer than 24 input bits are available in an
                 * input group, zero bits are added (on the right) to form an
                 * integral number of 6-bit groups.  Padding at the end of the
                 * data is performed using the "=" character.  Since all
                 * base64 input is an integral number of octets, only the
                 * following cases can arise: (1) the final quantum of
                 * encoding input is an integral multiple of 24 bits; here,
                 * the final unit of encoded output will be an integral
                 * multiple of 4 characters with no "=" padding, (2) the final
                 * quantum of encoding input is exactly 8 bits; here, the
                 * final unit of encoded output will be two characters
                 * followed by two "=" padding characters, or (3) the final
                 * quantum of encoding input is exactly 16 bits; here, the
                 * final unit of encoded output will be three characters
                 * followed by one "=" padding character." */

                total = 3 * (i / 4);
                if (pad == 0) {
                    if (i % 4 != 0)
                        goto return1;
                } else if (pad == 1) {
                    int decc;

                    if (i % 4 != 3)
                        goto return1;
                    for (decc = xsd10_types_base64_decode(*cur);
                         (decc < 0) || (decc > 63);
                         decc = xsd10_types_base64_decode(*cur))
                        --cur;
                    /* 16bits in 24bits means 2 pad bits: nnnnnn nnmmmm mmmm00*/
                    /* 00111100 -> 0x3c */
                    if (decc & ~0x3c)
                        goto return1;
                    total += 2;
                } else if (pad == 2) {
                    int decc;

                    if (i % 4 != 2)
                        goto return1;
                    for (decc = xsd10_types_base64_decode(*cur);
                         (decc < 0) || (decc > 63);
                         decc = xsd10_types_base64_decode(*cur))
                        --cur;
                    /* 8bits in 12bits means 4 pad bits: nnnnnn nn0000 */
                    /* 00110000 -> 0x30 */
                    if (decc & ~0x30)
                        goto return1;
                    total += 1;
                } else
                    goto return1;

                if (val != NULL) {
                    v = xsd10_types_new_value(XML_XSD10_SCHEMAS_BASE64BINARY);
                    if (v == NULL)
                        goto error;
                    base =
                        (XmlXsd10Char *) malloc(i + pad + 1);
                    if (base == NULL) {
                        free(v);
                        goto return1;
                    }
                    v->value.base64.str = base;
                    for (cur = value; *cur; ++cur)
                        if (xsd10_types_base64_decode(*cur) >= 0) {
                            *base = *cur;
                            ++base;
                        }
                    *base = 0;
                    v->value.base64.total = total;
                    *val = v;
                }
                goto return0;
            }
        case XML_XSD10_SCHEMAS_INTEGER:
        case XML_XSD10_SCHEMAS_PINTEGER:
        case XML_XSD10_SCHEMAS_NPINTEGER:
        case XML_XSD10_SCHEMAS_NINTEGER:
        case XML_XSD10_SCHEMAS_NNINTEGER:
        case XML_XSD10_SCHEMAS_LONG:
        case XML_XSD10_SCHEMAS_BYTE:
        case XML_XSD10_SCHEMAS_SHORT:
        case XML_XSD10_SCHEMAS_INT:
        case XML_XSD10_SCHEMAS_UINT:
        case XML_XSD10_SCHEMAS_ULONG:
        case XML_XSD10_SCHEMAS_USHORT:
        case XML_XSD10_SCHEMAS_UBYTE: {
                const XmlXsd10Char *cur = value;
                XmlXsd10ValDecimal decimal;
                XmlXsd10Char sign = '+';

                memset(&decimal, 0, sizeof(decimal));

                if (cur == NULL)
                    goto return1;
 		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
                if (*cur == '-') {
                    sign = '-';
                    cur++;
                } else if (*cur == '+')
                    cur++;
                ret = xsd10_types_parse_uint(&cur, &decimal);
                /* add sign */
                if (ret < 0)
                    goto valIntegerReturn1;
                decimal.str[0] = sign;
		if (normOnTheFly)
		    while XSD10_TYPES_IS_WSP_BLANK_CH(*cur) cur++;
                if (*cur != 0)
                    goto valIntegerReturn1;
                if (type->builtInType == XML_XSD10_SCHEMAS_NPINTEGER)
                {
                    if(xsd10_types_decimal_compare_with_integer(&decimal, 0) > 0)
                        goto valIntegerReturn1;
                }
                else if (type->builtInType == XML_XSD10_SCHEMAS_PINTEGER)
                {
                    if (sign == '-')
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) <= 0)
                        goto valIntegerReturn1;
                }
                else if (type->builtInType == XML_XSD10_SCHEMAS_NINTEGER)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) >= 0)
                        goto valIntegerReturn1;
                }
                else if (type->builtInType == XML_XSD10_SCHEMAS_NNINTEGER)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) < 0)
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_LONG)
                {
                    /* (u)int64_t may not be available on 32 bit platform, just use decimal */
                    XmlXsd10ValDecimal tmpDecimal;
                    static const char maxLong[] = "+9223372036854775807.0";
                    static const char minLong[] = "-9223372036854775808.0";
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 19;
                    tmpDecimal.str = XSD10_TYPES_BAD_CAST maxLong;
                    if (xsd10_types_decimal_compare(&decimal, &tmpDecimal) > 0)
                        goto valIntegerReturn1;
                    tmpDecimal.str = XSD10_TYPES_BAD_CAST minLong;
                    if (xsd10_types_decimal_compare(&decimal, &tmpDecimal) < 0)
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_ULONG)
                {
                    XmlXsd10ValDecimal tmpDecimal;
                    static const char maxULong[] = "+18446744073709551615.0";
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 20;
                    tmpDecimal.str = (XmlXsd10Char*)maxULong;
                    if (xsd10_types_decimal_compare(&decimal, &tmpDecimal) > 0)
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) < 0)
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_INT)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0x7fffffff) > 0) /* INT32_MAX */
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, -0x7fffffff-1) < 0) /* INT32_MIN */
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_SHORT)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0x7fff) > 0) /* INT16_MAX */
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, -0x8000) < 0) /* INT16_MIN */
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_BYTE)
                {if (xsd10_types_decimal_compare_with_integer(&decimal, 0x7f) > 0) /* INT8_MAX */
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, -0x80) < 0) /* INT8_MIN */
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_UINT)
                {
                    XmlXsd10ValDecimal tmpDecimal;
                    static const char maxUInt[] = "+4294967295.0";
                    tmpDecimal.fractionalPlaces = 1;
                    tmpDecimal.integralPlaces = 10;
                    tmpDecimal.str = (XmlXsd10Char*)maxUInt;
                    if (xsd10_types_decimal_compare(&decimal, &tmpDecimal) > 0)
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) < 0)
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_USHORT)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0xffff) > 0) /* UINT16_MAX */
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) < 0)
                        goto valIntegerReturn1;
                }
                else if(type->builtInType == XML_XSD10_SCHEMAS_UBYTE)
                {
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0xff) > 0) /* UINT8_MAX */
                        goto valIntegerReturn1;
                    if (xsd10_types_decimal_compare_with_integer(&decimal, 0) < 0)
                        goto valIntegerReturn1;
                }
                if (val != NULL) {
                    v = xsd10_types_new_value((XmlXsd10ValType) type->builtInType);
                    if (v == NULL) {
                        free(decimal.str);
                        goto error;
                    }
                    v->value.decimal = decimal;
                    *val = v;
                }
                else if(decimal.str != NULL)
                {
                    free(decimal.str);
                }
                goto return0;
            valIntegerReturn1:
                if(decimal.str != NULL)
                {
                    free(decimal.str);
                }
                goto return1;
            }
    }

  done:
    if (norm != NULL)
        free(norm);
    return (ret);
  return3:
    if (norm != NULL)
        free(norm);
    return (3);
  return1:
    if (norm != NULL)
        free(norm);
    return (1);
  return0:
    if (norm != NULL)
        free(norm);
    return (0);
  error:
    if (norm != NULL)
        free(norm);
    return (-1);
}

/* Port of xmlSchemaValPredefTypeNode. */
int
xml_xsd10_val_predef_type_node(XmlXsd10Type *type, const char *value,
	                   XmlXsd10Val **val, XMQNode *node) {
    return(xsd10_types_val_atomic_type(type, (const XmlXsd10Char *) value, val, node, 0,
	XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, NULL));
}

int
xml_xsd10_val_predef_type_node_ids(XmlXsd10Type *type, const char *value,
	                   XmlXsd10Val **val, XMQNode *node, XmlXsd10Ids *ids) {
    return(xsd10_types_val_atomic_type(type, (const XmlXsd10Char *) value, val, node, 0,
	XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 1, 0, ids));
}

/* Port of xmlSchemaValPredefTypeNodeNoNorm: this one does not apply any normalization to the value. */
int
xml_xsd10_val_predef_type_node_no_norm(XmlXsd10Type *type, const char *value,
				 XmlXsd10Val **val, XMQNode *node) {
    return(xsd10_types_val_atomic_type(type, (const XmlXsd10Char *) value, val, node, 1,
	XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, NULL));
}

int
xml_xsd10_val_predef_type_node_no_norm_ids(XmlXsd10Type *type, const char *value,
				 XmlXsd10Val **val, XMQNode *node, XmlXsd10Ids *ids) {
    return(xsd10_types_val_atomic_type(type, (const XmlXsd10Char *) value, val, node, 1,
	XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, ids));
}

/* Port of xmlSchemaValidatePredefinedType. */
int
xml_xsd10_validate_predefined_type(XmlXsd10Type *type, const char *value,
	                        XmlXsd10Val **val) {
    return(xml_xsd10_val_predef_type_node(type, value, val, NULL));
}

/* Port of xmlSchemaCompareDecimals: -1 if x < y, 0 if x == y, 1 if x > y and -2 in case of error. */
static int
xsd10_types_compare_decimals(XmlXsd10Val *x, XmlXsd10Val *y)
{
    int res = xsd10_types_decimal_compare(&x->value.decimal, &y->value.decimal);
    if(res > 0)
    {
        return 1;
    }
    if(res < 0)
    {
        return -1;
    }
    return 0;
}

/* Port of xmlSchemaCompareDurations: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
   case of error. */
static int
xsd10_types_compare_durations(XmlXsd10Val *x, XmlXsd10Val *y)
{
    long carry, mon, day;
    double sec;
    int invert = 1;
    long xmon, xday, myear, minday, maxday;
    static const long dayRange [2][12] = {
        { 0, 28, 59, 89, 120, 150, 181, 212, 242, 273, 303, 334, },
        { 0, 31, 62, 92, 123, 153, 184, 215, 245, 276, 306, 337} };

    if ((x == NULL) || (y == NULL))
        return -2;

    /* months */
    mon = x->value.dur.mon - y->value.dur.mon;

    /* seconds */
    sec = x->value.dur.sec - y->value.dur.sec;
    carry = (long)(sec / XSD10_TYPES_SECS_PER_DAY);
    sec -= ((double)carry) * XSD10_TYPES_SECS_PER_DAY;

    /* days */
    day = x->value.dur.day - y->value.dur.day + carry;

    /* easy test */
    if (mon == 0) {
        if (day == 0)
            if (sec == 0.0)
                return 0;
            else if (sec < 0.0)
                return -1;
            else
                return 1;
        else if (day < 0)
            return -1;
        else
            return 1;
    }

    if (mon > 0) {
        if ((day >= 0) && (sec >= 0.0))
            return 1;
        else {
            xmon = mon;
            xday = -day;
        }
    } else if ((day <= 0) && (sec <= 0.0)) {
        return -1;
    } else {
	invert = -1;
        xmon = -mon;
        xday = day;
    }

    myear = xmon / 12;
    if (myear == 0) {
	minday = 0;
	maxday = 0;
    } else {
        if (myear > LONG_MAX / 366)
            return -2;
        /* FIXME: This doesn't take leap year exceptions every 100/400 years
           into account. */
	maxday = 365 * myear + (myear + 3) / 4;
        /* FIXME: Needs to be calculated separately */
	minday = maxday - 1;
    }

    xmon = xmon % 12;
    minday += dayRange[0][xmon];
    maxday += dayRange[1][xmon];

    if ((maxday == minday) && (maxday == xday))
	return(0); /* can this really happen ? */
    if (maxday < xday)
        return(-invert);
    if (minday > xday)
        return(invert);

    /* indeterminate */
    return 2;
}

/*
 * macros for adding date/times and durations
 */
#define XSD10_TYPES_FQUOTIENT(a,b)                  (floor(((double)a/(double)b)))
#define XSD10_TYPES_MODULO(a,b)                     (a - XSD10_TYPES_FQUOTIENT(a,b) * b)
#define XSD10_TYPES_FQUOTIENT_RANGE(a,low,high)     (XSD10_TYPES_FQUOTIENT((a-low),(high-low)))
#define XSD10_TYPES_MODULO_RANGE(a,low,high)        ((XSD10_TYPES_MODULO((a-low),(high-low)))+low)

/* Port of xmlSchemaDupVal: makes a copy of v. The calling program is responsible for freeing
   the returned value. */
static XmlXsd10Val *
xsd10_types_dup_val (XmlXsd10Val *v)
{
    XmlXsd10Val *ret = xsd10_types_new_value(v->type);
    if (ret == NULL)
        return NULL;

    memcpy(ret, v, sizeof(XmlXsd10Val));
    ret->next = NULL;
    return ret;
}

/* Port of xmlSchemaCopyValue: copies the precomputed value. This duplicates any string within.
   Returns the copy or NULL if a copy for a data-type is not implemented. */
XmlXsd10Val *
xml_xsd10_copy_value(XmlXsd10Val *val)
{
    XmlXsd10Val *ret = NULL, *prev = NULL, *cur;

    /*
    * Copy the string values.
    */
    while (val != NULL) {
	switch (val->type) {
	    case XML_XSD10_SCHEMAS_ANYTYPE:
	    case XML_XSD10_SCHEMAS_IDREFS:
	    case XML_XSD10_SCHEMAS_ENTITIES:
	    case XML_XSD10_SCHEMAS_NMTOKENS:
                goto error;
	    case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
	    case XML_XSD10_SCHEMAS_STRING:
	    case XML_XSD10_SCHEMAS_NORMSTRING:
	    case XML_XSD10_SCHEMAS_TOKEN:
	    case XML_XSD10_SCHEMAS_LANGUAGE:
	    case XML_XSD10_SCHEMAS_NAME:
	    case XML_XSD10_SCHEMAS_NCNAME:
	    case XML_XSD10_SCHEMAS_ID:
	    case XML_XSD10_SCHEMAS_IDREF:
	    case XML_XSD10_SCHEMAS_ENTITY:
	    case XML_XSD10_SCHEMAS_NMTOKEN:
	    case XML_XSD10_SCHEMAS_ANYURI:
		cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
		if (val->value.str != NULL)
		    cur->value.str = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.str);
		break;
	    case XML_XSD10_SCHEMAS_QNAME:
	    case XML_XSD10_SCHEMAS_NOTATION:
		cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
		if (val->value.qname.name != NULL)
		    cur->value.qname.name =
                    xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.qname.name);
		if (val->value.qname.uri != NULL)
		    cur->value.qname.uri =
                    xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.qname.uri);
		break;
	    case XML_XSD10_SCHEMAS_HEXBINARY:
		cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
		if (val->value.hex.str != NULL)
		    cur->value.hex.str = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.hex.str);
		break;
	    case XML_XSD10_SCHEMAS_BASE64BINARY:
		cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
		if (val->value.base64.str != NULL)
		    cur->value.base64.str =
                    xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.base64.str);
		break;
            case XML_XSD10_SCHEMAS_DECIMAL:
            case XML_XSD10_SCHEMAS_INTEGER:
            case XML_XSD10_SCHEMAS_PINTEGER:
            case XML_XSD10_SCHEMAS_NPINTEGER:
            case XML_XSD10_SCHEMAS_NINTEGER:
            case XML_XSD10_SCHEMAS_NNINTEGER:
            case XML_XSD10_SCHEMAS_LONG:
            case XML_XSD10_SCHEMAS_BYTE:
            case XML_XSD10_SCHEMAS_SHORT:
            case XML_XSD10_SCHEMAS_INT:
            case XML_XSD10_SCHEMAS_UINT:
            case XML_XSD10_SCHEMAS_ULONG:
            case XML_XSD10_SCHEMAS_USHORT:
            case XML_XSD10_SCHEMAS_UBYTE:
                cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
                if (val->value.decimal.str != NULL)
                    cur->value.decimal.str = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.decimal.str);
		break;
	    default:
		cur = xsd10_types_dup_val(val);
                if (cur == NULL)
                    goto error;
		break;
	}
	if (ret == NULL)
	    ret = cur;
	else
	    prev->next = cur;
	prev = cur;
	val = val->next;
    }
    return (ret);

error:
    xml_xsd10_free_value(ret);
    return (NULL);
}

/* Port of _xmlSchemaDateAdd: compute a new date/time from dt and dur. This function assumes dt
   is either XML_SCHEMAS_DATETIME, XML_SCHEMAS_DATE, XML_SCHEMAS_GYEARMONTH,
   or XML_SCHEMAS_GYEAR. The returned value is the same type as dt.
   The calling program is responsible for freeing the returned value. */
static XmlXsd10Val *
xsd10_types_date_add (XmlXsd10Val *dt, XmlXsd10Val *dur)
{
    XmlXsd10Val *ret, *tmp;
    long carry, tempdays, temp;
    XmlXsd10ValDate *r, *d;
    XmlXsd10ValDuration *u;

    if ((dt == NULL) || (dur == NULL))
        return NULL;

    ret = xsd10_types_new_value(dt->type);
    if (ret == NULL)
        return NULL;

    /* make a copy so we don't alter the original value */
    tmp = xsd10_types_dup_val(dt);
    if (tmp == NULL) {
        xml_xsd10_free_value(ret);
        return NULL;
    }

    r = &(ret->value.date);
    d = &(tmp->value.date);
    u = &(dur->value.dur);

    /* normalization */
    if (d->mon == 0)
        d->mon = 1;

    /* normalize for time zone offset */
    u->sec -= (d->tzo * 60);
    d->tzo = 0;

    /* normalization */
    if (d->day == 0)
        d->day = 1;

    /* month */
    carry  = d->mon + u->mon;
    r->mon = (unsigned int) XSD10_TYPES_MODULO_RANGE(carry, 1, 13);
    carry  = (long) XSD10_TYPES_FQUOTIENT_RANGE(carry, 1, 13);

    /* year (may be modified later) */
    r->year = d->year + carry;
    if (r->year == 0) {
        if (d->year > 0)
            r->year--;
        else
            r->year++;
    }

    /* time zone */
    r->tzo     = d->tzo;
    r->tz_flag = d->tz_flag;

    /* seconds */
    r->sec = d->sec + u->sec;
    carry  = (long) XSD10_TYPES_FQUOTIENT((long)r->sec, 60);
    if (r->sec != 0.0) {
        r->sec = XSD10_TYPES_MODULO(r->sec, 60.0);
    }

    /* minute */
    carry += d->min;
    r->min = (unsigned int) XSD10_TYPES_MODULO(carry, 60);
    carry  = (long) XSD10_TYPES_FQUOTIENT(carry, 60);

    /* hours */
    carry  += d->hour;
    r->hour = (unsigned int) XSD10_TYPES_MODULO(carry, 24);
    carry   = (long)XSD10_TYPES_FQUOTIENT(carry, 24);

    /*
     * days
     * Note we use tempdays because the temporary values may need more
     * than 5 bits
     */
    if ((XSD10_TYPES_VALID_YEAR(r->year)) && (XSD10_TYPES_VALID_MONTH(r->mon)) &&
                  (d->day > XSD10_TYPES_MAX_DAYINMONTH(r->year, r->mon)))
        tempdays = XSD10_TYPES_MAX_DAYINMONTH(r->year, r->mon);
    else if (d->day < 1)
        tempdays = 1;
    else
        tempdays = d->day;

    tempdays += u->day + carry;

    while (1) {
        if (tempdays < 1) {
            long tmon = (long) XSD10_TYPES_MODULO_RANGE((int)r->mon-1, 1, 13);
            long tyr  = r->year + (long)XSD10_TYPES_FQUOTIENT_RANGE((int)r->mon-1, 1, 13);
            if (tyr == 0)
                tyr--;
	    /*
	     * Coverity detected an overrun in daysInMonth
	     * of size 12 at position 12 with index variable "((r)->mon - 1)"
	     */
	    if (tmon < 1)
	        tmon = 1;
	    if (tmon > 12)
	        tmon = 12;
            tempdays += XSD10_TYPES_MAX_DAYINMONTH(tyr, tmon);
            carry = -1;
        } else if (XSD10_TYPES_VALID_YEAR(r->year) && XSD10_TYPES_VALID_MONTH(r->mon) &&
                   tempdays > (long) XSD10_TYPES_MAX_DAYINMONTH(r->year, r->mon)) {
            tempdays = tempdays - XSD10_TYPES_MAX_DAYINMONTH(r->year, r->mon);
            carry = 1;
        } else
            break;

        temp = r->mon + carry;
        r->mon = (unsigned int) XSD10_TYPES_MODULO_RANGE(temp, 1, 13);
        r->year = r->year + (long) XSD10_TYPES_FQUOTIENT_RANGE(temp, 1, 13);
        if (r->year == 0) {
            if (temp < 1)
                r->year--;
            else
                r->year++;
	}
    }

    r->day = tempdays;

    /*
     * adjust the date/time type to the date values
     */
    if (ret->type != XML_XSD10_SCHEMAS_DATETIME) {
        if ((r->hour) || (r->min) || (r->sec))
            ret->type = XML_XSD10_SCHEMAS_DATETIME;
        else if (ret->type != XML_XSD10_SCHEMAS_DATE) {
            if ((r->mon != 1) && (r->day != 1))
                ret->type = XML_XSD10_SCHEMAS_DATE;
            else if ((ret->type != XML_XSD10_SCHEMAS_GYEARMONTH) && (r->mon != 1))
                ret->type = XML_XSD10_SCHEMAS_GYEARMONTH;
        }
    }

    xml_xsd10_free_value(tmp);

    return ret;
}

/* Port of xmlSchemaDateNormalize: normalize dt to GMT time. The offset parameter is subtracted
   from the return value is a time-zone offset is present on dt.
   Returns a normalized copy of dt or NULL if error. */
static XmlXsd10Val *
xsd10_types_date_normalize (XmlXsd10Val *dt, double offset)
{
    XmlXsd10Val *dur, *ret;

    if (dt == NULL)
        return NULL;

    if (((dt->type != XML_XSD10_SCHEMAS_TIME) &&
         (dt->type != XML_XSD10_SCHEMAS_DATETIME) &&
	 (dt->type != XML_XSD10_SCHEMAS_DATE)) || (dt->value.date.tzo == 0))
        return xsd10_types_dup_val(dt);

    dur = xsd10_types_new_value(XML_XSD10_SCHEMAS_DURATION);
    if (dur == NULL)
        return NULL;

    dur->value.date.sec -= offset;

    ret = xsd10_types_date_add(dt, dur);

    xml_xsd10_free_value(dur);

    if (ret == NULL)
        return NULL;

    /* ret->value.date.tzo = 0; */
    return ret;
}

/* Port of _xmlSchemaDateCastYMToDays: convert mon and year of dt to total number of days. Take the
   number of years since (or before) 1 AD and add the number of leap
   years. This is a function  because negative
   years must be handled a little differently and there is no zero year. */
static long
xsd10_types_date_cast_ym_to_days (const XmlXsd10Val *dt)
{
    long ret;
    int mon;

    mon = dt->value.date.mon;
    if (mon <= 0) mon = 1; /* normalization */

    if (dt->value.date.year <= 0)
        ret = (dt->value.date.year * 365) +
              (((dt->value.date.year+1)/4)-((dt->value.date.year+1)/100)+
               ((dt->value.date.year+1)/400)) +
              XSD10_TYPES_DAY_IN_YEAR(0, mon, dt->value.date.year);
    else
        ret = ((dt->value.date.year-1) * 365) +
              (((dt->value.date.year-1)/4)-((dt->value.date.year-1)/100)+
               ((dt->value.date.year-1)/400)) +
              XSD10_TYPES_DAY_IN_YEAR(0, mon, dt->value.date.year);

    return ret;
}

/* Port of TIME_TO_NUMBER: calculates the number of seconds in the time portion of dt. */
#define XSD10_TYPES_TIME_TO_NUMBER(dt)                              \
    ((double)((dt->value.date.hour * XSD10_TYPES_SECS_PER_HOUR) +   \
              (dt->value.date.min * XSD10_TYPES_SECS_PER_MIN) +	\
              (dt->value.date.tzo * XSD10_TYPES_SECS_PER_MIN)) +	\
               dt->value.date.sec)

/* Port of xmlSchemaCompareDates: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
   case of error. */
static int
xsd10_types_compare_dates (XmlXsd10Val *x, XmlXsd10Val *y)
{
    unsigned char xmask, ymask, xor_mask, and_mask;
    XmlXsd10Val *p1, *p2, *q1, *q2;
    long p1d, p2d, q1d, q2d;

    if ((x == NULL) || (y == NULL))
        return -2;

    if ((x->value.date.year > LONG_MAX / 366) ||
        (x->value.date.year < LONG_MIN / 366) ||
        (y->value.date.year > LONG_MAX / 366) ||
        (y->value.date.year < LONG_MIN / 366)) {
        /* Possible overflow when converting to days. */
        return -2;
    }

    if (x->value.date.tz_flag) {

        if (!y->value.date.tz_flag) {
            p1 = xsd10_types_date_normalize(x, 0);
            if (p1 == NULL)
                return -2;
            p1d = xsd10_types_date_cast_ym_to_days(p1) + p1->value.date.day;
            /* normalize y + 14:00 */
            q1 = xsd10_types_date_normalize(y, (14 * XSD10_TYPES_SECS_PER_HOUR));
            if (q1 == NULL) {
		xml_xsd10_free_value(p1);
                return -2;
            }

            q1d = xsd10_types_date_cast_ym_to_days(q1) + q1->value.date.day;
            if (p1d < q1d) {
		xml_xsd10_free_value(p1);
		xml_xsd10_free_value(q1);
                return -1;
	    } else if (p1d == q1d) {
                double sec;

                sec = XSD10_TYPES_TIME_TO_NUMBER(p1) - XSD10_TYPES_TIME_TO_NUMBER(q1);
                if (sec < 0.0) {
		    xml_xsd10_free_value(p1);
		    xml_xsd10_free_value(q1);
                    return -1;
		} else {
		    int ret = 0;
                    /* normalize y - 14:00 */
                    q2 = xsd10_types_date_normalize(y, -(14 * XSD10_TYPES_SECS_PER_HOUR));
                    if (q2 == NULL) {
                        xml_xsd10_free_value(p1);
                        xml_xsd10_free_value(q1);
                        return -2;
                    }
                    q2d = xsd10_types_date_cast_ym_to_days(q2) + q2->value.date.day;
                    if (p1d > q2d)
                        ret = 1;
                    else if (p1d == q2d) {
                        sec = XSD10_TYPES_TIME_TO_NUMBER(p1) - XSD10_TYPES_TIME_TO_NUMBER(q2);
                        if (sec > 0.0)
                            ret = 1;
                        else
                            ret = 2; /* indeterminate */
                    }
		    xml_xsd10_free_value(p1);
		    xml_xsd10_free_value(q1);
		    xml_xsd10_free_value(q2);
		    if (ret != 0)
		        return(ret);
                }
            } else {
		xml_xsd10_free_value(p1);
		xml_xsd10_free_value(q1);
	    }
        }
    } else if (y->value.date.tz_flag) {
        q1 = xsd10_types_date_normalize(y, 0);
        if (q1 == NULL)
            return -2;
        q1d = xsd10_types_date_cast_ym_to_days(q1) + q1->value.date.day;

        /* normalize x - 14:00 */
        p1 = xsd10_types_date_normalize(x, -(14 * XSD10_TYPES_SECS_PER_HOUR));
        if (p1 == NULL) {
	    xml_xsd10_free_value(q1);
            return -2;
        }
        p1d = xsd10_types_date_cast_ym_to_days(p1) + p1->value.date.day;

        if (p1d < q1d) {
	    xml_xsd10_free_value(p1);
	    xml_xsd10_free_value(q1);
            return -1;
	} else if (p1d == q1d) {
            double sec;

            sec = XSD10_TYPES_TIME_TO_NUMBER(p1) - XSD10_TYPES_TIME_TO_NUMBER(q1);
            if (sec < 0.0) {
		xml_xsd10_free_value(p1);
		xml_xsd10_free_value(q1);
                return -1;
	    } else {
	        int ret = 0;
                /* normalize x + 14:00 */
                p2 = xsd10_types_date_normalize(x, (14 * XSD10_TYPES_SECS_PER_HOUR));
                if (p2 == NULL) {
                    xml_xsd10_free_value(p1);
                    xml_xsd10_free_value(q1);
                    return -2;
                }
                p2d = xsd10_types_date_cast_ym_to_days(p2) + p2->value.date.day;

                if (p2d > q1d) {
                    ret = 1;
		} else if (p2d == q1d) {
                    sec = XSD10_TYPES_TIME_TO_NUMBER(p2) - XSD10_TYPES_TIME_TO_NUMBER(q1);
                    if (sec > 0.0)
                        ret = 1;
                    else
                        ret = 2; /* indeterminate */
                }
		xml_xsd10_free_value(p1);
		xml_xsd10_free_value(q1);
		xml_xsd10_free_value(p2);
		if (ret != 0)
		    return(ret);
            }
	} else {
	    xml_xsd10_free_value(p1);
	    xml_xsd10_free_value(q1);
        }
    }

    /*
     * if the same type then calculate the difference
     */
    if (x->type == y->type) {
        int ret = 0;
        q1 = xsd10_types_date_normalize(y, 0);
        if (q1 == NULL)
            return -2;
        q1d = xsd10_types_date_cast_ym_to_days(q1) + q1->value.date.day;

        p1 = xsd10_types_date_normalize(x, 0);
        if (p1 == NULL) {
	    xml_xsd10_free_value(q1);
            return -2;
        }
        p1d = xsd10_types_date_cast_ym_to_days(p1) + p1->value.date.day;

        if (p1d < q1d) {
            ret = -1;
	} else if (p1d > q1d) {
            ret = 1;
	} else {
            double sec;

            sec = XSD10_TYPES_TIME_TO_NUMBER(p1) - XSD10_TYPES_TIME_TO_NUMBER(q1);
            if (sec < 0.0)
                ret = -1;
            else if (sec > 0.0)
                ret = 1;

        }
	xml_xsd10_free_value(p1);
	xml_xsd10_free_value(q1);
        return(ret);
    }

    switch (x->type) {
        case XML_XSD10_SCHEMAS_DATETIME:
            xmask = 0xf;
            break;
        case XML_XSD10_SCHEMAS_DATE:
            xmask = 0x7;
            break;
        case XML_XSD10_SCHEMAS_GYEAR:
            xmask = 0x1;
            break;
        case XML_XSD10_SCHEMAS_GMONTH:
            xmask = 0x2;
            break;
        case XML_XSD10_SCHEMAS_GDAY:
            xmask = 0x3;
            break;
        case XML_XSD10_SCHEMAS_GYEARMONTH:
            xmask = 0x3;
            break;
        case XML_XSD10_SCHEMAS_GMONTHDAY:
            xmask = 0x6;
            break;
        case XML_XSD10_SCHEMAS_TIME:
            xmask = 0x8;
            break;
        default:
            xmask = 0;
            break;
    }

    switch (y->type) {
        case XML_XSD10_SCHEMAS_DATETIME:
            ymask = 0xf;
            break;
        case XML_XSD10_SCHEMAS_DATE:
            ymask = 0x7;
            break;
        case XML_XSD10_SCHEMAS_GYEAR:
            ymask = 0x1;
            break;
        case XML_XSD10_SCHEMAS_GMONTH:
            ymask = 0x2;
            break;
        case XML_XSD10_SCHEMAS_GDAY:
            ymask = 0x3;
            break;
        case XML_XSD10_SCHEMAS_GYEARMONTH:
            ymask = 0x3;
            break;
        case XML_XSD10_SCHEMAS_GMONTHDAY:
            ymask = 0x6;
            break;
        case XML_XSD10_SCHEMAS_TIME:
            ymask = 0x8;
            break;
        default:
            ymask = 0;
            break;
    }

    xor_mask = xmask ^ ymask;           /* mark type differences */
    and_mask = xmask & ymask;           /* mark field specification */

    /* year */
    if (xor_mask & 1)
        return 2; /* indeterminate */
    else if (and_mask & 1) {
        if (x->value.date.year < y->value.date.year)
            return -1;
        else if (x->value.date.year > y->value.date.year)
            return 1;
    }

    /* month */
    if (xor_mask & 2)
        return 2; /* indeterminate */
    else if (and_mask & 2) {
        if (x->value.date.mon < y->value.date.mon)
            return -1;
        else if (x->value.date.mon > y->value.date.mon)
            return 1;
    }

    /* day */
    if (xor_mask & 4)
        return 2; /* indeterminate */
    else if (and_mask & 4) {
        if (x->value.date.day < y->value.date.day)
            return -1;
        else if (x->value.date.day > y->value.date.day)
            return 1;
    }

    /* time */
    if (xor_mask & 8)
        return 2; /* indeterminate */
    else if (and_mask & 8) {
        if (x->value.date.hour < y->value.date.hour)
            return -1;
        else if (x->value.date.hour > y->value.date.hour)
            return 1;
        else if (x->value.date.min < y->value.date.min)
            return -1;
        else if (x->value.date.min > y->value.date.min)
            return 1;
        else if (x->value.date.sec < y->value.date.sec)
            return -1;
        else if (x->value.date.sec > y->value.date.sec)
            return 1;
    }

    return 0;
}

/* Port of xmlSchemaComparePreserveReplaceStrings: compare 2 string for their normalized values.
   x is a string with whitespace of "preserve", y is a string with a whitespace of "replace".
   I.e. x could be an "xsd:string" and y an "xsd:normalizedString".
   invert inverts the result if x < y or x > y.
   Returns -1 if x < y, 0 if x == y, 1 if x > y, and -2 in case of error. */
static int
xsd10_types_compare_preserve_replace_strings(const XmlXsd10Char *x,
				       const XmlXsd10Char *y,
				       int invert)
{
    int tmp;

    while ((*x != 0) && (*y != 0)) {
	if (XSD10_TYPES_IS_WSP_REPLACE_CH(*y)) {
	    if (! XSD10_TYPES_IS_WSP_SPACE_CH(*x)) {
		if ((*x - 0x20) < 0) {
		    if (invert)
			return(1);
		    else
			return(-1);
		} else {
		    if (invert)
			return(-1);
		    else
			return(1);
		}
	    }
	} else {
	    tmp = *x - *y;
	    if (tmp < 0) {
		if (invert)
		    return(1);
		else
		    return(-1);
	    }
	    if (tmp > 0) {
		if (invert)
		    return(-1);
		else
		    return(1);
	    }
	}
	x++;
	y++;
    }
    if (*x != 0) {
	if (invert)
	    return(-1);
	else
	    return(1);
    }
    if (*y != 0) {
	if (invert)
	    return(1);
	else
	    return(-1);
    }
    return(0);
}

/* Port of xmlSchemaComparePreserveCollapseStrings: x is a string with whitespace of "preserve",
   y is a string with a whitespace of "collapse". */
static int
xsd10_types_compare_preserve_collapse_strings(const XmlXsd10Char *x,
				        const XmlXsd10Char *y,
					int invert)
{
    int tmp;

    /*
    * Skip leading blank chars of the collapsed string.
    */
    while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
	y++;

    while ((*x != 0) && (*y != 0)) {
	if XSD10_TYPES_IS_WSP_BLANK_CH(*y) {
	    if (! XSD10_TYPES_IS_WSP_SPACE_CH(*x)) {
		/*
		* The yv character would have been replaced to 0x20.
		*/
		if ((*x - 0x20) < 0) {
		    if (invert)
			return(1);
		    else
			return(-1);
		} else {
		    if (invert)
			return(-1);
		    else
			return(1);
		}
	    }
	    x++;
	    y++;
	    /*
	    * Skip contiguous blank chars of the collapsed string.
	    */
	    while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
		y++;
	} else {
	    tmp = *x++ - *y++;
	    if (tmp < 0) {
		if (invert)
		    return(1);
		else
		    return(-1);
	    }
	    if (tmp > 0) {
		if (invert)
		    return(-1);
		else
		    return(1);
	    }
	}
    }
    if (*x != 0) {
	 if (invert)
	     return(-1);
	 else
	     return(1);
    }
    if (*y != 0) {
	/*
	* Skip trailing blank chars of the collapsed string.
	*/
	while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
	    y++;
	if (*y != 0) {
	    if (invert)
		return(1);
	    else
		return(-1);
	}
    }
    return(0);
}

/* Port of xmlSchemaCompareReplaceCollapseStrings: x is a string with whitespace of "replace",
   y is a string with a whitespace of "collapse". */
static int
xsd10_types_compare_replace_collapse_strings(const XmlXsd10Char *x,
				       const XmlXsd10Char *y,
				       int invert)
{
    int tmp;

    /*
    * Skip leading blank chars of the collapsed string.
    */
    while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
	y++;

    while ((*x != 0) && (*y != 0)) {
	if XSD10_TYPES_IS_WSP_BLANK_CH(*y) {
	    if (! XSD10_TYPES_IS_WSP_BLANK_CH(*x)) {
		/*
		* The yv character would have been replaced to 0x20.
		*/
		if ((*x - 0x20) < 0) {
		    if (invert)
			return(1);
		    else
			return(-1);
		} else {
		    if (invert)
			return(-1);
		    else
			return(1);
		}
	    }
	    x++;
	    y++;
	    /*
	    * Skip contiguous blank chars of the collapsed string.
	    */
	    while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
		y++;
	} else {
	    if XSD10_TYPES_IS_WSP_BLANK_CH(*x) {
		/*
		* The xv character would have been replaced to 0x20.
		*/
		if ((0x20 - *y) < 0) {
		    if (invert)
			return(1);
		    else
			return(-1);
		} else {
		    if (invert)
			return(-1);
		    else
			return(1);
		}
	    }
	    tmp = *x++ - *y++;
	    if (tmp < 0)
		return(-1);
	    if (tmp > 0)
		return(1);
	}
    }
    if (*x != 0) {
	 if (invert)
	     return(-1);
	 else
	     return(1);
    }
    if (*y != 0) {
	/*
	* Skip trailing blank chars of the collapsed string.
	*/
	while XSD10_TYPES_IS_WSP_BLANK_CH(*y)
	    y++;
	if (*y != 0) {
	    if (invert)
		return(1);
	    else
		return(-1);
	}
    }
    return(0);
}

/* Port of xmlSchemaCompareReplacedStrings: compare 2 string for their normalized values. */
static int
xsd10_types_compare_replaced_strings(const XmlXsd10Char *x,
				const XmlXsd10Char *y)
{
    int tmp;

    while ((*x != 0) && (*y != 0)) {
	if XSD10_TYPES_IS_WSP_BLANK_CH(*y) {
	    if (! XSD10_TYPES_IS_WSP_BLANK_CH(*x)) {
		if ((*x - 0x20) < 0)
		    return(-1);
		else
		    return(1);
	    }
	} else {
	    if XSD10_TYPES_IS_WSP_BLANK_CH(*x) {
		if ((0x20 - *y) < 0)
		    return(-1);
		else
		    return(1);
	    }
	    tmp = *x - *y;
	    if (tmp < 0)
		return(-1);
	    if (tmp > 0)
		return(1);
	}
	x++;
	y++;
    }
    if (*x != 0)
        return(1);
    if (*y != 0)
        return(-1);
    return(0);
}

/* Port of xmlSchemaCompareNormStrings: compare 2 string for their normalized values. */
static int
xsd10_types_compare_norm_strings(const XmlXsd10Char *x,
			    const XmlXsd10Char *y) {
    int tmp;

    while (XSD10_TYPES_IS_BLANK_CH(*x)) x++;
    while (XSD10_TYPES_IS_BLANK_CH(*y)) y++;
    while ((*x != 0) && (*y != 0)) {
	if (XSD10_TYPES_IS_BLANK_CH(*x)) {
	    if (!XSD10_TYPES_IS_BLANK_CH(*y)) {
		tmp = *x - *y;
		return(tmp);
	    }
	    while (XSD10_TYPES_IS_BLANK_CH(*x)) x++;
	    while (XSD10_TYPES_IS_BLANK_CH(*y)) y++;
	} else {
	    tmp = *x++ - *y++;
	    if (tmp < 0)
		return(-1);
	    if (tmp > 0)
		return(1);
	}
    }
    if (*x != 0) {
	while (XSD10_TYPES_IS_BLANK_CH(*x)) x++;
	if (*x != 0)
	    return(1);
    }
    if (*y != 0) {
	while (XSD10_TYPES_IS_BLANK_CH(*y)) y++;
	if (*y != 0)
	    return(-1);
    }
    return(0);
}

/* Port of xmlSchemaCompareFloats: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
   case of error. */
static int
xsd10_types_compare_floats(XmlXsd10Val *x, XmlXsd10Val *y) {
    double d1, d2;

    if ((x == NULL) || (y == NULL))
	return(-2);

    /*
     * Cast everything to doubles.
     */
    if (x->type == XML_XSD10_SCHEMAS_DOUBLE)
	d1 = x->value.d;
    else if (x->type == XML_XSD10_SCHEMAS_FLOAT)
	d1 = x->value.f;
    else
	return(-2);

    if (y->type == XML_XSD10_SCHEMAS_DOUBLE)
	d2 = y->value.d;
    else if (y->type == XML_XSD10_SCHEMAS_FLOAT)
	d2 = y->value.f;
    else
	return(-2);

    /*
     * Check for special cases.
     */
    if (isnan(d1)) {
	if (isnan(d2))
	    return(0);
	return(1);
    }
    if (isnan(d2))
	return(-1);

    /*
     * basic tests, the last one we should have equality, but
     * portability is more important than speed and handling
     * NaN or Inf in a portable way is always a challenge, so ...
     */
    if (d1 < d2)
	return(-1);
    if (d1 > d2)
	return(1);
    if (d1 == d2)
	return(0);
    return(2);
}

/* Port of xmlSchemaCompareValuesInternal: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y,
   3 if not comparable and -2 in case of error. */
static int
xsd10_types_compare_values_internal(XmlXsd10ValType xtype,
			       XmlXsd10Val *x,
			       const XmlXsd10Char *xvalue,
			       XmlXsd10WhitespaceValueType xws,
			       XmlXsd10ValType ytype,
			       XmlXsd10Val *y,
			       const XmlXsd10Char *yvalue,
			       XmlXsd10WhitespaceValueType yws)
{
    switch (xtype) {
	case XML_XSD10_SCHEMAS_UNKNOWN:
	case XML_XSD10_SCHEMAS_ANYTYPE:
	    return(-2);
        case XML_XSD10_SCHEMAS_INTEGER:
        case XML_XSD10_SCHEMAS_NPINTEGER:
        case XML_XSD10_SCHEMAS_NINTEGER:
        case XML_XSD10_SCHEMAS_NNINTEGER:
        case XML_XSD10_SCHEMAS_PINTEGER:
        case XML_XSD10_SCHEMAS_INT:
        case XML_XSD10_SCHEMAS_UINT:
        case XML_XSD10_SCHEMAS_LONG:
        case XML_XSD10_SCHEMAS_ULONG:
        case XML_XSD10_SCHEMAS_SHORT:
        case XML_XSD10_SCHEMAS_USHORT:
        case XML_XSD10_SCHEMAS_BYTE:
        case XML_XSD10_SCHEMAS_UBYTE:
	case XML_XSD10_SCHEMAS_DECIMAL:
	    if ((x == NULL) || (y == NULL))
		return(-2);
	    if (ytype == xtype)
		return(xsd10_types_compare_decimals(x, y));
	    if ((ytype == XML_XSD10_SCHEMAS_DECIMAL) ||
		(ytype == XML_XSD10_SCHEMAS_INTEGER) ||
		(ytype == XML_XSD10_SCHEMAS_NPINTEGER) ||
		(ytype == XML_XSD10_SCHEMAS_NINTEGER) ||
		(ytype == XML_XSD10_SCHEMAS_NNINTEGER) ||
		(ytype == XML_XSD10_SCHEMAS_PINTEGER) ||
		(ytype == XML_XSD10_SCHEMAS_INT) ||
		(ytype == XML_XSD10_SCHEMAS_UINT) ||
		(ytype == XML_XSD10_SCHEMAS_LONG) ||
		(ytype == XML_XSD10_SCHEMAS_ULONG) ||
		(ytype == XML_XSD10_SCHEMAS_SHORT) ||
		(ytype == XML_XSD10_SCHEMAS_USHORT) ||
		(ytype == XML_XSD10_SCHEMAS_BYTE) ||
		(ytype == XML_XSD10_SCHEMAS_UBYTE))
		return(xsd10_types_compare_decimals(x, y));
	    return(-2);
        case XML_XSD10_SCHEMAS_DURATION:
	    if ((x == NULL) || (y == NULL))
		return(-2);
	    if (ytype == XML_XSD10_SCHEMAS_DURATION)
                return(xsd10_types_compare_durations(x, y));
            return(-2);
        case XML_XSD10_SCHEMAS_TIME:
        case XML_XSD10_SCHEMAS_GDAY:
        case XML_XSD10_SCHEMAS_GMONTH:
        case XML_XSD10_SCHEMAS_GMONTHDAY:
        case XML_XSD10_SCHEMAS_GYEAR:
        case XML_XSD10_SCHEMAS_GYEARMONTH:
        case XML_XSD10_SCHEMAS_DATE:
        case XML_XSD10_SCHEMAS_DATETIME:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if ((ytype == XML_XSD10_SCHEMAS_DATETIME)  ||
                (ytype == XML_XSD10_SCHEMAS_TIME)      ||
                (ytype == XML_XSD10_SCHEMAS_GDAY)      ||
                (ytype == XML_XSD10_SCHEMAS_GMONTH)    ||
                (ytype == XML_XSD10_SCHEMAS_GMONTHDAY) ||
                (ytype == XML_XSD10_SCHEMAS_GYEAR)     ||
                (ytype == XML_XSD10_SCHEMAS_DATE)      ||
                (ytype == XML_XSD10_SCHEMAS_GYEARMONTH))
                return (xsd10_types_compare_dates(x, y));
            return (-2);
	/*
	* Note that we will support comparison of string types against
	* anySimpleType as well.
	*/
	case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
	case XML_XSD10_SCHEMAS_STRING:
        case XML_XSD10_SCHEMAS_NORMSTRING:
        case XML_XSD10_SCHEMAS_TOKEN:
        case XML_XSD10_SCHEMAS_LANGUAGE:
        case XML_XSD10_SCHEMAS_NMTOKEN:
        case XML_XSD10_SCHEMAS_NAME:
        case XML_XSD10_SCHEMAS_NCNAME:
        case XML_XSD10_SCHEMAS_ID:
        case XML_XSD10_SCHEMAS_IDREF:
        case XML_XSD10_SCHEMAS_ENTITY:
        case XML_XSD10_SCHEMAS_ANYURI:
	{
	    const XmlXsd10Char *xv, *yv;

	    if (x == NULL)
		xv = xvalue;
	    else
		xv = x->value.str;
	    if (y == NULL)
		yv = yvalue;
	    else
		yv = y->value.str;
	    /*
	    * TODO: Compare those against QName.
	    */
	    if (ytype == XML_XSD10_SCHEMAS_QNAME) {
		/* TODO */
		if (y == NULL)
		    return(-2);
		return (-2);
	    }
            if ((ytype == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
		(ytype == XML_XSD10_SCHEMAS_STRING) ||
		(ytype == XML_XSD10_SCHEMAS_NORMSTRING) ||
                (ytype == XML_XSD10_SCHEMAS_TOKEN) ||
                (ytype == XML_XSD10_SCHEMAS_LANGUAGE) ||
                (ytype == XML_XSD10_SCHEMAS_NMTOKEN) ||
                (ytype == XML_XSD10_SCHEMAS_NAME) ||
                (ytype == XML_XSD10_SCHEMAS_NCNAME) ||
                (ytype == XML_XSD10_SCHEMAS_ID) ||
                (ytype == XML_XSD10_SCHEMAS_IDREF) ||
                (ytype == XML_XSD10_SCHEMAS_ENTITY) ||
                (ytype == XML_XSD10_SCHEMAS_ANYURI)) {

		if (xws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE) {

		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE) {
			/* TODO: What about x < y or x > y. */
			if (xsd10_types_str_equal(xv, yv))
			    return (0);
			else
			    return (2);
		    } else if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
			return (xsd10_types_compare_preserve_replace_strings(xv, yv, 0));
		    else if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			return (xsd10_types_compare_preserve_collapse_strings(xv, yv, 0));

		} else if (xws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE) {

		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
			return (xsd10_types_compare_preserve_replace_strings(yv, xv, 1));
		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
			return (xsd10_types_compare_replaced_strings(xv, yv));
		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			return (xsd10_types_compare_replace_collapse_strings(xv, yv, 0));

		} else if (xws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE) {

		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_PRESERVE)
			return (xsd10_types_compare_preserve_collapse_strings(yv, xv, 1));
		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
			return (xsd10_types_compare_replace_collapse_strings(yv, xv, 1));
		    if (yws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			return (xsd10_types_compare_norm_strings(xv, yv));
		} else
		    return (-2);

	    }
            return (-2);
	}
        case XML_XSD10_SCHEMAS_QNAME:
	case XML_XSD10_SCHEMAS_NOTATION:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if ((ytype == XML_XSD10_SCHEMAS_QNAME) ||
		(ytype == XML_XSD10_SCHEMAS_NOTATION)) {
		if ((xsd10_types_str_equal(x->value.qname.name, y->value.qname.name)) &&
		    (xsd10_types_str_equal(x->value.qname.uri, y->value.qname.uri)))
		    return(0);
		return(2);
	    }
	    return (-2);
        case XML_XSD10_SCHEMAS_FLOAT:
        case XML_XSD10_SCHEMAS_DOUBLE:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if ((ytype == XML_XSD10_SCHEMAS_FLOAT) ||
                (ytype == XML_XSD10_SCHEMAS_DOUBLE))
                return (xsd10_types_compare_floats(x, y));
            return (-2);
        case XML_XSD10_SCHEMAS_BOOLEAN:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if (ytype == XML_XSD10_SCHEMAS_BOOLEAN) {
		if (x->value.b == y->value.b)
		    return(0);
		if (x->value.b == 0)
		    return(-1);
		return(1);
	    }
	    return (-2);
        case XML_XSD10_SCHEMAS_HEXBINARY:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if (ytype == XML_XSD10_SCHEMAS_HEXBINARY) {
	        if (x->value.hex.total == y->value.hex.total) {
		    int ret = xsd10_types_strcmp(x->value.hex.str, y->value.hex.str);
		    if (ret > 0)
			return(1);
		    else if (ret == 0)
			return(0);
		}
		else if (x->value.hex.total > y->value.hex.total)
		    return(1);

		return(-1);
            }
            return (-2);
        case XML_XSD10_SCHEMAS_BASE64BINARY:
	    if ((x == NULL) || (y == NULL))
		return(-2);
            if (ytype == XML_XSD10_SCHEMAS_BASE64BINARY) {
                if (x->value.base64.total == y->value.base64.total) {
                    int ret = xsd10_types_strcmp(x->value.base64.str,
		                        y->value.base64.str);
                    if (ret > 0)
                        return(1);
                    else if (ret == 0)
                        return(0);
		    else
		        return(-1);
                }
                else if (x->value.base64.total > y->value.base64.total)
                    return(1);
                else
                    return(-1);
            }
            return (-2);
        case XML_XSD10_SCHEMAS_IDREFS:
        case XML_XSD10_SCHEMAS_ENTITIES:
        case XML_XSD10_SCHEMAS_NMTOKENS:
	    /* TODO */
	    break;
    }
    return -2;
}

/* Port of xmlSchemaCompareValues: -1 if x < y, 0 if x == y, 1 if x > y, 2 if x <> y, and -2 in
   case of error. */
int
xml_xsd10_compare_values(XmlXsd10Val *x, XmlXsd10Val *y) {
    XmlXsd10WhitespaceValueType xws, yws;

    if ((x == NULL) || (y == NULL))
        return(-2);
    if (x->type == XML_XSD10_SCHEMAS_STRING)
	xws = XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
    else if (x->type == XML_XSD10_SCHEMAS_NORMSTRING)
        xws = XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
    else
        xws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;

    if (y->type == XML_XSD10_SCHEMAS_STRING)
	yws = XML_XSD10_SCHEMA_WHITESPACE_PRESERVE;
    else if (y->type == XML_XSD10_SCHEMAS_NORMSTRING)
        yws = XML_XSD10_SCHEMA_WHITESPACE_REPLACE;
    else
        yws = XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE;

    return(xsd10_types_compare_values_internal(x->type, x, NULL, xws, y->type,
	y, NULL, yws));
}

/* Port of xmlSchemaCompareValuesWhtsp. */
int
xml_xsd10_compare_values_whtsp(XmlXsd10Val *x,
			    XmlXsd10WhitespaceValueType xws,
			    XmlXsd10Val *y,
			    XmlXsd10WhitespaceValueType yws)
{
    if ((x == NULL) || (y == NULL))
	return(-2);
    return(xsd10_types_compare_values_internal(x->type, x, NULL, xws, y->type,
	y, NULL, yws));
}

/* Port of xmlSchemaCompareValuesWhtspExt. */
static int
xsd10_types_compare_values_whtsp_ext(XmlXsd10ValType xtype,
			       XmlXsd10Val *x,
			       const XmlXsd10Char *xvalue,
			       XmlXsd10WhitespaceValueType xws,
			       XmlXsd10ValType ytype,
			       XmlXsd10Val *y,
			       const XmlXsd10Char *yvalue,
			       XmlXsd10WhitespaceValueType yws)
{
    return(xsd10_types_compare_values_internal(xtype, x, xvalue, xws, ytype, y,
	yvalue, yws));
}

/* Port of xmlSchemaNormLen: computes the UTF8 length of the normalized value of the string.
   Returns the length or -1 in case of error. */
static int
xsd10_types_norm_len(const XmlXsd10Char *value) {
    const XmlXsd10Char *utf;
    int ret = 0;

    if (value == NULL)
	return(-1);
    utf = value;
    while (XSD10_TYPES_IS_BLANK_CH(*utf)) utf++;
    while (*utf != 0) {
	if (utf[0] & 0x80) {
	    if ((utf[1] & 0xc0) != 0x80)
		return(-1);
	    if ((utf[0] & 0xe0) == 0xe0) {
		if ((utf[2] & 0xc0) != 0x80)
		    return(-1);
		if ((utf[0] & 0xf0) == 0xf0) {
		    if ((utf[0] & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
			return(-1);
		    utf += 4;
		} else {
		    utf += 3;
		}
	    } else {
		utf += 2;
	    }
	} else if (XSD10_TYPES_IS_BLANK_CH(*utf)) {
	    while (XSD10_TYPES_IS_BLANK_CH(*utf)) utf++;
	    if (*utf == 0)
		break;
	} else {
	    utf++;
	}
	ret++;
    }
    return(ret);
}

/* Port of xmlSchemaGetFacetValueAsULong: extract the value of a facet. */
unsigned long
xml_xsd10_get_facet_value_as_ulong(XmlXsd10Facet *facet)
{
    /*
    * TODO: Check if this is a decimal.
    */
    char *discard;
    if (facet == NULL || facet->val == NULL)
        return 0;
    return strtoul((const char*)facet->val->value.decimal.str+1, &discard, 10);
}

/* Port of xmlSchemaValidateListSimpleTypeFacet: checks the value of a list simple type against
   a facet. Returns 0 if the value is valid, a positive error code number otherwise and -1 in
   case of an internal error. */
int
xml_xsd10_validate_list_simple_type_facet(XmlXsd10Facet *facet,
				     const char *value,
				     unsigned long actualLen,
				     unsigned long *expectedLen)
{
    if (facet == NULL)
        return(-1);
    /*
    * TODO: Check if this will work with large numbers.
    * (compare value.decimal.mi and value.decimal.hi as well?).
    */
    if (facet->type == XML_XSD10_SCHEMA_FACET_LENGTH) {
        if (actualLen != xml_xsd10_get_facet_value_as_ulong(facet)) {
	    if (expectedLen != NULL)
                *expectedLen = xml_xsd10_get_facet_value_as_ulong(facet);
	    return (XML_XSD10_SCHEMAV_CVC_LENGTH_VALID);
	}
    } else if (facet->type == XML_XSD10_SCHEMA_FACET_MINLENGTH) {
        if (actualLen < xml_xsd10_get_facet_value_as_ulong(facet)) {
	    if (expectedLen != NULL)
                *expectedLen = xml_xsd10_get_facet_value_as_ulong(facet);
	    return (XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID);
	}
    } else if (facet->type == XML_XSD10_SCHEMA_FACET_MAXLENGTH) {
        if (actualLen > xml_xsd10_get_facet_value_as_ulong(facet)) {
	    if (expectedLen != NULL)
                *expectedLen = xml_xsd10_get_facet_value_as_ulong(facet);
	    return (XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID);
	}
    } else
	/*
	* NOTE: That we can pass NULL as xmlSchemaVal to
	* xmlSchemaValidateFacet, since the remaining facet types
	* are: XML_SCHEMA_FACET_PATTERN, XML_SCHEMA_FACET_ENUMERATION.
	*/
	return(xml_xsd10_validate_facet(NULL, facet, value, NULL));
    return (0);
}

/* Port of xmlSchemaValidateLengthFacetInternal: checks a value against a "length", "minLength"
   and "maxLength" facet; sets length to the computed length of value.
   Returns 0 if the value is valid, a positive error code otherwise and -1 in case of an internal
   or API error. */
static int
xsd10_types_validate_length_facet_internal(XmlXsd10Facet *facet,
				     XmlXsd10ValType valType,
				     const XmlXsd10Char *value,
				     XmlXsd10Val *val,
				     unsigned long *length,
				     XmlXsd10WhitespaceValueType ws)
{
    unsigned int len = 0;

    if ((length == NULL) || (facet == NULL))
        return (-1);
    *length = 0;
    if ((facet->type != XML_XSD10_SCHEMA_FACET_LENGTH) &&
	(facet->type != XML_XSD10_SCHEMA_FACET_MAXLENGTH) &&
	(facet->type != XML_XSD10_SCHEMA_FACET_MINLENGTH))
	return (-1);

    /*
    * TODO: length, maxLength and minLength must be of type
    * nonNegativeInteger only. Check if decimal is used somehow.
    */
    if ((facet->val == NULL) ||
	((facet->val->type != XML_XSD10_SCHEMAS_DECIMAL) &&
	 (facet->val->type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
	!(xsd10_types_decimal_is_integer(&facet->val->value.decimal)))
 {
	return(-1);
    }
    if ((val != NULL) && (val->type == XML_XSD10_SCHEMAS_HEXBINARY))
	len = val->value.hex.total;
    else if ((val != NULL) && (val->type == XML_XSD10_SCHEMAS_BASE64BINARY))
	len = val->value.base64.total;
    else {
	switch (valType) {
	    case XML_XSD10_SCHEMAS_STRING:
	    case XML_XSD10_SCHEMAS_NORMSTRING:
		if (ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN) {
		    /*
		    * This is to ensure API compatibility with the old
		    * xmlSchemaValidateLengthFacet(). Anyway, this was and
		    * is not the correct handling.
		    * TODO: Get rid of this case somehow.
		    */
		    if (valType == XML_XSD10_SCHEMAS_STRING)
			len = xsd10_types_utf8_strlen(value);
		    else
			len = xsd10_types_norm_len(value);
		} else if (value != NULL) {
		    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			len = xsd10_types_norm_len(value);
		    else
		    /*
		    * Should be OK for "preserve" as well.
		    */
		    len = xsd10_types_utf8_strlen(value);
		}
		break;
	    case XML_XSD10_SCHEMAS_IDREF:
	    case XML_XSD10_SCHEMAS_TOKEN:
	    case XML_XSD10_SCHEMAS_LANGUAGE:
	    case XML_XSD10_SCHEMAS_NMTOKEN:
	    case XML_XSD10_SCHEMAS_NAME:
	    case XML_XSD10_SCHEMAS_NCNAME:
	    case XML_XSD10_SCHEMAS_ID:
		/*
		* FIXME: What exactly to do with anyURI?
		*/
	    case XML_XSD10_SCHEMAS_ANYURI:
		if (value != NULL)
		    len = xsd10_types_norm_len(value);
		break;
	    case XML_XSD10_SCHEMAS_QNAME:
	    case XML_XSD10_SCHEMAS_NOTATION:
		/*
		* For QName and NOTATION, those facets are
		* deprecated and should be ignored.
		*/
		return (0);
	    default:
		/* TODO */
                break;
	}
    }
    *length = (unsigned long) len;
    /*
    * TODO: Return the whole expected value. (This may be possible now with xmlSchemaValDecimalCompareWithInteger)
    */
    if (facet->type == XML_XSD10_SCHEMA_FACET_LENGTH) {
        if (len != xml_xsd10_get_facet_value_as_ulong(facet))
	    return(XML_XSD10_SCHEMAV_CVC_LENGTH_VALID);
    } else if (facet->type == XML_XSD10_SCHEMA_FACET_MINLENGTH) {
        if (len < xml_xsd10_get_facet_value_as_ulong(facet))
	    return(XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID);
    } else {
        if (len > xml_xsd10_get_facet_value_as_ulong(facet))
	    return(XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID);
    }

    return (0);
}

/* Port of xmlSchemaValidateLengthFacet. */
int
xml_xsd10_validate_length_facet(XmlXsd10Type *type,
			     XmlXsd10Facet *facet,
			     const char *value,
			     XmlXsd10Val *val,
			     unsigned long *length)
{
    if (type == NULL)
        return(-1);
    return (xsd10_types_validate_length_facet_internal(facet,
	(XmlXsd10ValType) type->builtInType, (const XmlXsd10Char *) value, val, length,
	XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN));
}

/* Port of xmlSchemaValidateLengthFacetWhtsp. */
int
xml_xsd10_validate_length_facet_whtsp(XmlXsd10Facet *facet,
				  XmlXsd10ValType valType,
				  const char *value,
				  XmlXsd10Val *val,
				  unsigned long *length,
				  XmlXsd10WhitespaceValueType ws)
{
    return (xsd10_types_validate_length_facet_internal(facet, valType, (const XmlXsd10Char *) value, val,
	length, ws));
}

/* Port of xmlSchemaValidateFacetInternal: check a value against a facet condition.
   Returns 0 if the element is schemas valid, a positive error code
   number otherwise and -1 in case of internal or API error. */
static int
xsd10_types_validate_facet_internal(XmlXsd10Facet *facet,
			       XmlXsd10WhitespaceValueType fws,
			       XmlXsd10ValType valType,
			       const XmlXsd10Char *value,
			       XmlXsd10Val *val,
			       XmlXsd10WhitespaceValueType ws)
{
    int ret;

    if (facet == NULL)
	return(-1);

    switch (facet->type) {
	case XML_XSD10_SCHEMA_FACET_PATTERN:
	    /*
	    * NOTE that for patterns, the @value needs to be the normalized
	    * value, *not* the lexical initial value or the canonical value.
	    */
	    if (value == NULL)
		return(-1);
	    /*
	    * If string-derived type, regexp must be tested on the value space of
	    * the datatype.
	    * See https://www.w3.org/TR/xmlschema-2/#rf-pattern
	    */
	    if (val &&
                val->value.str &&
                ((val->type >= XML_XSD10_SCHEMAS_STRING &&
                  val->type <= XML_XSD10_SCHEMAS_NORMSTRING) ||
                 (val->type >= XML_XSD10_SCHEMAS_TOKEN &&
                  val->type <= XML_XSD10_SCHEMAS_ENTITIES &&
                  val->type != XML_XSD10_SCHEMAS_QNAME))) {
                value = val->value.str;
            }
	    ret = xsd10_types_regexp_exec(facet->regexp, value);
	    if (ret == 1)
		return(0);
	    if (ret == 0)
		return(XML_XSD10_SCHEMAV_CVC_PATTERN_VALID);
	    return(ret);
	case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
	    ret = xml_xsd10_compare_values(val, facet->val);
	    if (ret == -2)
		return(-1);
	    if (ret == -1)
		return(0);
	    return(XML_XSD10_SCHEMAV_CVC_MAXEXCLUSIVE_VALID);
	case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
	    ret = xml_xsd10_compare_values(val, facet->val);
	    if (ret == -2)
		return(-1);
	    if ((ret == -1) || (ret == 0))
		return(0);
	    return(XML_XSD10_SCHEMAV_CVC_MAXINCLUSIVE_VALID);
	case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
	    ret = xml_xsd10_compare_values(val, facet->val);
	    if (ret == -2)
		return(-1);
	    if (ret == 1)
		return(0);
	    return(XML_XSD10_SCHEMAV_CVC_MINEXCLUSIVE_VALID);
	case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
	    ret = xml_xsd10_compare_values(val, facet->val);
	    if (ret == -2)
		return(-1);
	    if ((ret == 1) || (ret == 0))
		return(0);
	    return(XML_XSD10_SCHEMAV_CVC_MININCLUSIVE_VALID);
	case XML_XSD10_SCHEMA_FACET_WHITESPACE:
	    /* TODO whitespaces */
	    /*
	    * NOTE: Whitespace should be handled to normalize
	    * the value to be validated against a the facets;
	    * not to normalize the value in-between.
	    */
	    return(0);
	case  XML_XSD10_SCHEMA_FACET_ENUMERATION:
	    if (ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN) {
		/*
		* This is to ensure API compatibility with the old
		* xmlSchemaValidateFacet().
		* TODO: Get rid of this case.
		*/
		if ((facet->value != NULL) &&
		    (xsd10_types_str_equal((const XmlXsd10Char *) facet->value, value)))
		    return(0);
	    } else {
		ret = xsd10_types_compare_values_whtsp_ext(facet->val->type,
		    facet->val, (const XmlXsd10Char *) facet->value, fws, valType, val,
		    value, ws);
		if (ret == -2)
		    return(-1);
		if (ret == 0)
		    return(0);
	    }
	    return(XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID);
	case XML_XSD10_SCHEMA_FACET_LENGTH:
	    /*
	    * SPEC (1.3) "if {primitive type definition} is QName or NOTATION,
	    * then any {value} is facet-valid."
	    */
	    if ((valType == XML_XSD10_SCHEMAS_QNAME) ||
		(valType == XML_XSD10_SCHEMAS_NOTATION))
		return (0);
            /* Falls through. */
	case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
	case XML_XSD10_SCHEMA_FACET_MINLENGTH: {
	    unsigned int len = 0;

	    if ((valType == XML_XSD10_SCHEMAS_QNAME) ||
		(valType == XML_XSD10_SCHEMAS_NOTATION))
		return (0);
	    /*
	    * TODO: length, maxLength and minLength must be of type
	    * nonNegativeInteger only. Check if decimal is used somehow.
	    */
	    if ((facet->val == NULL) ||
		((facet->val->type != XML_XSD10_SCHEMAS_DECIMAL) &&
		 (facet->val->type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
		!xsd10_types_decimal_is_integer(&facet->val->value.decimal)) {
		return(-1);
	    }
	    if ((val != NULL) && (val->type == XML_XSD10_SCHEMAS_HEXBINARY))
		len = val->value.hex.total;
	    else if ((val != NULL) && (val->type == XML_XSD10_SCHEMAS_BASE64BINARY))
		len = val->value.base64.total;
	    else {
		switch (valType) {
		    case XML_XSD10_SCHEMAS_STRING:
		    case XML_XSD10_SCHEMAS_NORMSTRING:
			if (ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN) {
			    /*
			    * This is to ensure API compatibility with the old
			    * xmlSchemaValidateFacet(). Anyway, this was and
			    * is not the correct handling.
			    * TODO: Get rid of this case somehow.
			    */
			    if (valType == XML_XSD10_SCHEMAS_STRING)
				len = xsd10_types_utf8_strlen(value);
			    else
				len = xsd10_types_norm_len(value);
			} else if (value != NULL) {
			    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
				len = xsd10_types_norm_len(value);
			    else
				/*
				* Should be OK for "preserve" as well.
				*/
				len = xsd10_types_utf8_strlen(value);
			}
			break;
		    case XML_XSD10_SCHEMAS_IDREF:
		    case XML_XSD10_SCHEMAS_TOKEN:
		    case XML_XSD10_SCHEMAS_LANGUAGE:
		    case XML_XSD10_SCHEMAS_NMTOKEN:
		    case XML_XSD10_SCHEMAS_NAME:
		    case XML_XSD10_SCHEMAS_NCNAME:
		    case XML_XSD10_SCHEMAS_ID:
		    case XML_XSD10_SCHEMAS_ANYURI:
			if (value != NULL)
			    len = xsd10_types_norm_len(value);
			break;
		    default:
		        /* TODO */
                        break;
		}
	    }
	    if (facet->type == XML_XSD10_SCHEMA_FACET_LENGTH) {
                if (len != xml_xsd10_get_facet_value_as_ulong(facet))
		    return(XML_XSD10_SCHEMAV_CVC_LENGTH_VALID);
	    } else if (facet->type == XML_XSD10_SCHEMA_FACET_MINLENGTH) {
                if (len < xml_xsd10_get_facet_value_as_ulong(facet))
		    return(XML_XSD10_SCHEMAV_CVC_MINLENGTH_VALID);
	    } else if (len > xml_xsd10_get_facet_value_as_ulong(facet)) {
                return (XML_XSD10_SCHEMAV_CVC_MAXLENGTH_VALID);
	    }
	    break;
	}
	case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
	case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:

	    if ((facet->val == NULL) ||
		((facet->val->type != XML_XSD10_SCHEMAS_PINTEGER) &&
		 (facet->val->type != XML_XSD10_SCHEMAS_NNINTEGER)) ||
		!xsd10_types_decimal_is_integer(&facet->val->value.decimal)) {
		return(-1);
	    }
	    if ((val == NULL) ||
		((val->type != XML_XSD10_SCHEMAS_DECIMAL) &&
		 (val->type != XML_XSD10_SCHEMAS_INTEGER) &&
		 (val->type != XML_XSD10_SCHEMAS_NPINTEGER) &&
		 (val->type != XML_XSD10_SCHEMAS_NINTEGER) &&
		 (val->type != XML_XSD10_SCHEMAS_NNINTEGER) &&
		 (val->type != XML_XSD10_SCHEMAS_PINTEGER) &&
		 (val->type != XML_XSD10_SCHEMAS_INT) &&
		 (val->type != XML_XSD10_SCHEMAS_UINT) &&
		 (val->type != XML_XSD10_SCHEMAS_LONG) &&
		 (val->type != XML_XSD10_SCHEMAS_ULONG) &&
		 (val->type != XML_XSD10_SCHEMAS_SHORT) &&
		 (val->type != XML_XSD10_SCHEMAS_USHORT) &&
		 (val->type != XML_XSD10_SCHEMAS_BYTE) &&
		 (val->type != XML_XSD10_SCHEMAS_UBYTE))) {
		return(-1);
	    }
	    if (facet->type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) {
                if (xsd10_types_decimal_get_significant_digit_count(&val->value.decimal) > xml_xsd10_get_facet_value_as_ulong(facet))
	            return(XML_XSD10_SCHEMAV_CVC_TOTALDIGITS_VALID);

	    } else if (facet->type == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS) {
			if ((xsd10_types_decimal_is_integer(&val->value.decimal) ? 0 : val->value.decimal.fractionalPlaces) > xml_xsd10_get_facet_value_as_ulong(facet))
		    return(XML_XSD10_SCHEMAV_CVC_FRACTIONDIGITS_VALID);
	    }
	    break;
	default:
	    /* TODO */
            break;
    }
    return(0);

}

/* Port of xmlSchemaValidateFacet: check a value against a facet condition.
   Returns 0 if the element is schemas valid, a positive error code
   number otherwise and -1 in case of internal or API error. */
int
xml_xsd10_validate_facet(XmlXsd10Type *base,
	               XmlXsd10Facet *facet,
	               const char *value,
		       XmlXsd10Val *val)
{
    /*
    * This tries to ensure API compatibility regarding the old
    * xmlSchemaValidateFacet() and the new xmlSchemaValidateFacetInternal() and
    * xmlSchemaValidateFacetWhtsp().
    */
    if (val != NULL)
	return(xsd10_types_validate_facet_internal(facet,
	    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, val->type, (const XmlXsd10Char *) value, val,
	    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN));
    else if (base != NULL)
	return(xsd10_types_validate_facet_internal(facet,
	    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, (XmlXsd10ValType) base->builtInType, (const XmlXsd10Char *) value, val,
	    XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN));
    return(-1);
}

/* Port of xmlSchemaValidateFacetWhtsp: check a value against a facet condition. This takes value
   normalization according to the specified whitespace types into account.
   Note that value needs to be the *normalized* value if the facet is of type "pattern". */
int
xml_xsd10_validate_facet_whtsp(XmlXsd10Facet *facet,
			    XmlXsd10WhitespaceValueType fws,
			    XmlXsd10ValType valType,
			    const char *value,
			    XmlXsd10Val *val,
			    XmlXsd10WhitespaceValueType ws)
{
     return(xsd10_types_validate_facet_internal(facet, fws, valType,
	 (const XmlXsd10Char *) value, val, ws));
}

/* Port of xmlSchemaGetCanonValue: get the canonical lexical representation of the value.
   The caller has to FREE the returned retValue.

   WARNING: Some value types are not supported yet, resulting
   in a retValue of "???".

   TODO: XML Schema 1.0 does not define canonical representations
   for: duration, gYearMonth, gYear, gMonthDay, gMonth, gDay,
   anyURI, QName, NOTATION. This will be fixed in XML Schema 1.1.

   Returns 0 if the value could be built, 1 if the value type is
   not supported yet and -1 in case of API errors. */
static int
xsd10_types_get_canon_value(XmlXsd10Val *val, XmlXsd10Char **retValue)
{
    if ((retValue == NULL) || (val == NULL))
	return (-1);
    *retValue = NULL;
    switch (val->type) {
	case XML_XSD10_SCHEMAS_STRING:
	    if (val->value.str == NULL)
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "");
	    else
		*retValue =
		    xsd10_types_strdup((const XmlXsd10Char *) val->value.str);
	    break;
	case XML_XSD10_SCHEMAS_NORMSTRING:
	    if (val->value.str == NULL)
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "");
	    else {
		*retValue = xsd10_types_white_space_replace(
		    (const XmlXsd10Char *) val->value.str);
		if ((*retValue) == NULL)
		    *retValue = xsd10_types_strdup(
			(const XmlXsd10Char *) val->value.str);
	    }
	    break;
	case XML_XSD10_SCHEMAS_TOKEN:
	case XML_XSD10_SCHEMAS_LANGUAGE:
	case XML_XSD10_SCHEMAS_NMTOKEN:
	case XML_XSD10_SCHEMAS_NAME:
	case XML_XSD10_SCHEMAS_NCNAME:
	case XML_XSD10_SCHEMAS_ID:
	case XML_XSD10_SCHEMAS_IDREF:
	case XML_XSD10_SCHEMAS_ENTITY:
	case XML_XSD10_SCHEMAS_NOTATION: /* Unclear */
	case XML_XSD10_SCHEMAS_ANYURI:   /* Unclear */
	    if (val->value.str == NULL)
		return (-1);
	    *retValue =
		xsd10_types_collapse_string(XSD10_TYPES_BAD_CAST val->value.str);
	    if (*retValue == NULL)
		*retValue =
		    xsd10_types_strdup((const XmlXsd10Char *) val->value.str);
	    break;
	case XML_XSD10_SCHEMAS_QNAME:
	    /* TODO: Unclear in XML Schema 1.0. */
	    if (val->value.qname.uri == NULL) {
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.qname.name);
		return (0);
	    } else {
		/* Like libxml2, the uri is appended twice ("{uri}uri"). */
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "{");
		*retValue = xsd10_types_strcat((XmlXsd10Char *) (*retValue),
		    XSD10_TYPES_BAD_CAST val->value.qname.uri);
		*retValue = xsd10_types_strcat((XmlXsd10Char *) (*retValue),
		    XSD10_TYPES_BAD_CAST "}");
		*retValue = xsd10_types_strcat((XmlXsd10Char *) (*retValue),
		    XSD10_TYPES_BAD_CAST val->value.qname.uri);
	    }
	    break;
	case XML_XSD10_SCHEMAS_DECIMAL: {
                XmlXsd10Char *start = val->value.decimal.str;
                if(start[0] == '+')
                {
                    start += 1;
                }
                *retValue = xsd10_types_strdup(start);
	    }
	    break;
	case XML_XSD10_SCHEMAS_INTEGER:
        case XML_XSD10_SCHEMAS_PINTEGER:
        case XML_XSD10_SCHEMAS_NPINTEGER:
        case XML_XSD10_SCHEMAS_NINTEGER:
        case XML_XSD10_SCHEMAS_NNINTEGER:
	case XML_XSD10_SCHEMAS_LONG:
        case XML_XSD10_SCHEMAS_BYTE:
        case XML_XSD10_SCHEMAS_SHORT:
        case XML_XSD10_SCHEMAS_INT:
	case XML_XSD10_SCHEMAS_UINT:
        case XML_XSD10_SCHEMAS_ULONG:
        case XML_XSD10_SCHEMAS_USHORT:
        case XML_XSD10_SCHEMAS_UBYTE: {
            XmlXsd10Char *start = val->value.decimal.str;
            /* 2 = sign+NULL */
            size_t bufSize = val->value.decimal.integralPlaces+2;
            if(start[0] == '+')
            {
                start += 1;
                bufSize -= 1;
            }
            *retValue = (XmlXsd10Char *) malloc(bufSize);
            if(*retValue) {
                /* no need to limit string length in format, it will only print bufSize-1 chars anyways */
                snprintf((char*)*retValue, bufSize, "%.*s", (int) (bufSize - 1), (const char *) start);
            }
	    }
	    break;
	case XML_XSD10_SCHEMAS_BOOLEAN:
	    if (val->value.b)
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "true");
	    else
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "false");
	    break;
	case XML_XSD10_SCHEMAS_DURATION: {
		char buf[100];
		unsigned long year;
		unsigned long mon, day, hour = 0, min = 0;
		double sec = 0, left;

		/* TODO: Unclear in XML Schema 1.0 */
		/*
		* TODO: This results in a normalized output of the value
		* - which is NOT conformant to the spec -
		* since the exact values of each property are not
		* recoverable. Think about extending the structure to
		* provide a field for every property.
		*/
		year = (unsigned long) XSD10_TYPES_FQUOTIENT(labs(val->value.dur.mon), 12);
		mon = labs(val->value.dur.mon) - 12 * year;

		day = (unsigned long) XSD10_TYPES_FQUOTIENT(fabs(val->value.dur.sec), 86400);
		left = fabs(val->value.dur.sec) - day * 86400;
		if (left > 0) {
		    hour = (unsigned long) XSD10_TYPES_FQUOTIENT(left, 3600);
		    left = left - (hour * 3600);
		    if (left > 0) {
			min = (unsigned long) XSD10_TYPES_FQUOTIENT(left, 60);
			sec = left - (min * 60);
		    }
		}
		/* Like libxml2: the sign is inverted and the days of the duration are not used. */
		if ((val->value.dur.mon < 0) || (val->value.dur.sec < 0))
		    snprintf(buf, 100, "P%luY%luM%luDT%luH%luM%.14gS",
			year, mon, day, hour, min, sec);
		else
		    snprintf(buf, 100, "-P%luY%luM%luDT%luH%luM%.14gS",
			year, mon, day, hour, min, sec);
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	case XML_XSD10_SCHEMAS_GYEAR: {
		char buf[30];
		/* TODO: Unclear in XML Schema 1.0 */
		/* TODO: What to do with the timezone? */
		snprintf(buf, 30, "%04ld", val->value.date.year);
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	case XML_XSD10_SCHEMAS_GMONTH: {
		/* TODO: Unclear in XML Schema 1.0 */
		/* TODO: What to do with the timezone? */
		*retValue = (XmlXsd10Char *) malloc(6);
		if (*retValue == NULL)
		    return(-1);
		snprintf((char *) *retValue, 6, "--%02u",
		    (unsigned) val->value.date.mon);
	    }
	    break;
        case XML_XSD10_SCHEMAS_GDAY: {
		/* TODO: Unclear in XML Schema 1.0 */
		/* TODO: What to do with the timezone? */
		*retValue = (XmlXsd10Char *) malloc(6);
		if (*retValue == NULL)
		    return(-1);
		snprintf((char *) *retValue, 6, "---%02u",
		    (unsigned) val->value.date.day);
	    }
	    break;
        case XML_XSD10_SCHEMAS_GMONTHDAY: {
		/* TODO: Unclear in XML Schema 1.0 */
		/* TODO: What to do with the timezone? */
		*retValue = (XmlXsd10Char *) malloc(8);
		if (*retValue == NULL)
		    return(-1);
		snprintf((char *) *retValue, 8, "--%02u-%02u",
		    (unsigned) val->value.date.mon, (unsigned) val->value.date.day);
	    }
	    break;
        case XML_XSD10_SCHEMAS_GYEARMONTH: {
		char buf[35];
		/* TODO: Unclear in XML Schema 1.0 */
		/* TODO: What to do with the timezone? */
		if (val->value.date.year < 0)
		    snprintf(buf, 35, "-%04ld-%02u",
			labs(val->value.date.year),
			(unsigned) val->value.date.mon);
		else
		    snprintf(buf, 35, "%04ld-%02u",
			val->value.date.year, (unsigned) val->value.date.mon);
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	case XML_XSD10_SCHEMAS_TIME:
	    {
		char buf[30];

		if (val->value.date.tz_flag) {
		    XmlXsd10Val *norm;

		    norm = xsd10_types_date_normalize(val, 0);
		    if (norm == NULL)
			return (-1);
		    /*
		    * TODO: Check if "%.14g" is portable.
		    */
		    snprintf(buf, 30,
			"%02u:%02u:%02.14gZ",
			(unsigned) norm->value.date.hour,
			(unsigned) norm->value.date.min,
			norm->value.date.sec);
		    xml_xsd10_free_value(norm);
		} else {
		    snprintf(buf, 30,
			"%02u:%02u:%02.14g",
			(unsigned) val->value.date.hour,
			(unsigned) val->value.date.min,
			val->value.date.sec);
		}
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
        case XML_XSD10_SCHEMAS_DATE:
	    {
		char buf[30];

		if (val->value.date.tz_flag) {
		    XmlXsd10Val *norm;

		    norm = xsd10_types_date_normalize(val, 0);
		    if (norm == NULL)
			return (-1);
		    /*
		    * TODO: Append the canonical value of the
		    * recoverable timezone and not "Z".
		    */
		    snprintf(buf, 30,
			"%04ld-%02u-%02uZ",
			norm->value.date.year, (unsigned) norm->value.date.mon,
			(unsigned) norm->value.date.day);
		    xml_xsd10_free_value(norm);
		} else {
		    snprintf(buf, 30,
			"%04ld-%02u-%02u",
			val->value.date.year, (unsigned) val->value.date.mon,
			(unsigned) val->value.date.day);
		}
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
        case XML_XSD10_SCHEMAS_DATETIME:
	    {
		char buf[50];

		if (val->value.date.tz_flag) {
		    XmlXsd10Val *norm;

		    norm = xsd10_types_date_normalize(val, 0);
		    if (norm == NULL)
			return (-1);
		    /*
		    * TODO: Check if "%.14g" is portable.
		    */
		    snprintf(buf, 50,
			"%04ld-%02u-%02uT%02u:%02u:%02.14gZ",
			norm->value.date.year, (unsigned) norm->value.date.mon,
			(unsigned) norm->value.date.day, (unsigned) norm->value.date.hour,
			(unsigned) norm->value.date.min, norm->value.date.sec);
		    xml_xsd10_free_value(norm);
		} else {
		    snprintf(buf, 50,
			"%04ld-%02u-%02uT%02u:%02u:%02.14g",
			val->value.date.year, (unsigned) val->value.date.mon,
			(unsigned) val->value.date.day, (unsigned) val->value.date.hour,
			(unsigned) val->value.date.min, val->value.date.sec);
		}
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	case XML_XSD10_SCHEMAS_HEXBINARY:
	    *retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.hex.str);
	    break;
	case XML_XSD10_SCHEMAS_BASE64BINARY:
	    /*
	    * TODO: Is the following spec piece implemented?:
	    * SPEC: "Note: For some values the canonical form defined
	    * above does not conform to [RFC 2045], which requires breaking
	    * with linefeeds at appropriate intervals."
	    */
	    *retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST val->value.base64.str);
	    break;
	case XML_XSD10_SCHEMAS_FLOAT: {
		char buf[30];
		/*
		* |m| < 16777216, -149 <= e <= 104.
		* TODO: Handle, NaN, INF, -INF. The format is not
		* yet conformant. The c type float does not cover
		* the whole range.
		*/
		snprintf(buf, 30, "%01.14e", val->value.f);
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	case XML_XSD10_SCHEMAS_DOUBLE: {
		char buf[40];
		/* |m| < 9007199254740992, -1075 <= e <= 970 */
		/*
		* TODO: Handle, NaN, INF, -INF. The format is not
		* yet conformant. The c type float does not cover
		* the whole range.
		*/
		snprintf(buf, 40, "%01.14e", val->value.d);
		*retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST buf);
	    }
	    break;
	default:
	    *retValue = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "???");
	    return (1);
    }
    if (*retValue == NULL)
	return(-1);
    return (0);
}

int
xml_xsd10_get_canon_value(XmlXsd10Val *val, const char **retValue)
{
    XmlXsd10Char *r = NULL;
    int ret;

    if (retValue == NULL)
        return (-1);
    ret = xsd10_types_get_canon_value(val, &r);
    if (val != NULL)
        *retValue = (const char *) r;
    return (ret);
}

/* Port of xmlSchemaGetCanonValueWhtsp: get the canonical representation of the value.
   The caller has to free the returned retValue.
   Returns 0 if the value could be built, 1 if the value type is
   not supported yet and -1 in case of API errors. */
int
xml_xsd10_get_canon_value_whtsp(XmlXsd10Val *val,
			    const char **retValue,
			    XmlXsd10WhitespaceValueType ws)
{
    XmlXsd10Char *r = NULL;

    if ((retValue == NULL) || (val == NULL))
	return (-1);
    if ((ws == XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN) ||
	(ws > XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE))
	return (-1);

    *retValue = NULL;
    switch (val->type) {
	case XML_XSD10_SCHEMAS_STRING:
	    if (val->value.str == NULL)
		r = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "");
	    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
		r = xsd10_types_collapse_string(val->value.str);
	    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
		r = xsd10_types_white_space_replace(val->value.str);
	    if (r == NULL)
		r = xsd10_types_strdup(val->value.str);
	    break;
	case XML_XSD10_SCHEMAS_NORMSTRING:
	    if (val->value.str == NULL)
		r = xsd10_types_strdup(XSD10_TYPES_BAD_CAST "");
	    else {
		if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
		    r = xsd10_types_collapse_string(val->value.str);
		else
		    r = xsd10_types_white_space_replace(val->value.str);
		if (r == NULL)
		    r = xsd10_types_strdup(val->value.str);
	    }
	    break;
	default:
	    return (xml_xsd10_get_canon_value(val, retValue));
    }
    *retValue = (const char *) r;
    return (0);
}

/* Port of xmlSchemaGetValType: accessor for the type of a value. */
XmlXsd10ValType
xml_xsd10_get_val_type(XmlXsd10Val *val)
{
    if (val == NULL)
        return(XML_XSD10_SCHEMAS_UNKNOWN);
    return (val->type);
}

/************************************************************************
 *									*
 *		Facets (libxml2 xmlschemas.c)				*
 *									*
 ************************************************************************/

/* Port of xmlSchemaNewFacet: allocate a new Facet structure. */
XmlXsd10Facet *
xml_xsd10_new_facet(void)
{
    XmlXsd10Facet *ret;

    ret = (XmlXsd10Facet *) malloc(sizeof(XmlXsd10Facet));
    if (ret == NULL) {
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Facet));

    return (ret);
}

/* Port of xmlSchemaFreeAnnot. */
static void
xsd10_types_free_annot(XmlXsd10Annot *annot)
{
    if (annot == NULL)
        return;
    if (annot->next == NULL) {
	free(annot);
    } else {
	XmlXsd10Annot *prev;

	do {
	    prev = annot;
	    annot = annot->next;
	    free(prev);
	} while (annot != NULL);
    }
}

/* Port of xmlSchemaFreeFacet: deallocate a Schema Facet structure. */
void
xml_xsd10_free_facet(XmlXsd10Facet *facet)
{
    if (facet == NULL)
        return;
    if (facet->val != NULL)
        xml_xsd10_free_value(facet->val);
    if (facet->regexp != NULL)
        xsd10_types_regexp_free(facet->regexp);
    if (facet->annot != NULL)
        xsd10_types_free_annot(facet->annot);
    free(facet);
}

/* Port of xmlSchemaGetWhiteSpaceFacetValue for built in types. */
static XmlXsd10WhitespaceValueType
xsd10_types_builtin_white_space(XmlXsd10Type *type)
{
    /*
    * Note that we assume a whitespace of preserve for anySimpleType.
    */
    if ((type->builtInType == XML_XSD10_SCHEMAS_STRING) ||
	(type->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
	return(XML_XSD10_SCHEMA_WHITESPACE_PRESERVE);
    else if (type->builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
	return(XML_XSD10_SCHEMA_WHITESPACE_REPLACE);
    /*
    * For all `atomic` datatypes other than string (and types `derived`
    * by `restriction` from it) the value of whiteSpace is fixed to
    * collapse
    * Note that this includes built-in list datatypes.
    */
    return(XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE);
}

/* Port of xmlSchemaValidateNotation (called with a parser context): the notations of the schema
   are not available here (libxml2 uses the empty schema of a temporary parser context), so
   a valid QName is never a declared notation. */
static int
xsd10_types_validate_notation(const XmlXsd10Char *value)
{
    int ret;

    ret = xsd10_types_validate_qname(value, 1);
    if (ret != 0)
	return (ret);
    return (1);
}

/* Port of xmlSchemaVCheckCVCSimpleType (cvc-simple-type) for built in types, as called by
   xmlSchemaCheckFacet with a parser context and without error reporting. */
static int
xsd10_types_check_cvc_simple_type(XMQNode *node,
			     XmlXsd10Type *type,
			     const XmlXsd10Char *value,
			     XmlXsd10Val **retVal,
			     int normalize,
			     int isNormalized)
{
    int ret = 0, valNeeded = (retVal) ? 1 : 0;
    XmlXsd10Val *val = NULL;
    XmlXsd10Char *normValue = NULL;

#define XSD10_TYPES_NORMALIZE(atype) \
    if ((! isNormalized) && \
    (normalize || (type->flags & XML_XSD10_SCHEMAS_TYPE_NORMVALUENEEDED))) { \
	switch (xsd10_types_builtin_white_space(atype)) { \
	    case XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE: \
		normValue = xsd10_types_collapse_string(value); break; \
	    case XML_XSD10_SCHEMA_WHITESPACE_REPLACE: \
		normValue = xsd10_types_white_space_replace(value); break; \
	    default: break; \
	} \
	if (normValue != NULL) \
	    value = normValue; \
	isNormalized = 1; \
    }

    if ((retVal != NULL) && (*retVal != NULL)) {
	xml_xsd10_free_value(*retVal);
	*retVal = NULL;
    }
    /* User derived types are checked by the schema part. */
    if (type->type != XML_XSD10_SCHEMA_TYPE_BASIC)
        return (-1);
    if ((! valNeeded) && (type->flags & XML_XSD10_SCHEMAS_TYPE_FACETSNEEDVALUE))
	valNeeded = 1;
    if (value == NULL)
	value = XSD10_TYPES_BAD_CAST "";
    if ((type->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE) ||
        (type->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_ATOMIC)) {
	XmlXsd10Type *biType; /* The built-in type. */
	/*
	* SPEC (1.2.1) "if {variety} is `atomic` then the string must `match`
	* a literal in the `lexical space` of {base type definition}"
	*/
	/*
	* Whitespace-normalize.
	*/
	XSD10_TYPES_NORMALIZE(type);
	biType = type;
	switch (biType->builtInType) {
	    case XML_XSD10_SCHEMAS_NOTATION:
		ret = xsd10_types_validate_notation(value);
		break;
	    default:
		if (valNeeded)
		    ret = xsd10_types_val_atomic_type(biType, value, &val, node, 1,
			XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, NULL);
		else
		    ret = xsd10_types_val_atomic_type(biType, value, NULL, node, 1,
			XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN, 1, 0, 1, NULL);
		break;
	}
	if (ret != 0) {
	    if (ret < 0)
		goto internal_error;
	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST)
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
	    else
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_1;
	}
	/* The facets of built in types are not checked (xmlSchemaValidateFacets). */
    } else if (type->flags & XML_XSD10_SCHEMAS_TYPE_VARIETY_LIST) {

	XmlXsd10Type *itemType;
	const XmlXsd10Char *cur, *end;
	XmlXsd10Char *tmpValue = NULL;
	unsigned long len = 0;
	XmlXsd10Val *prevVal = NULL, *curVal = NULL;
	/* 1.2.2 if {variety} is `list` then the string must be a sequence
	* of white space separated tokens, each of which `match`es a literal
	* in the `lexical space` of {item type definition}
	*/
	XSD10_TYPES_NORMALIZE(type);
	itemType = type->subtypes;
	cur = value;
	do {
	    while (XSD10_TYPES_IS_BLANK_CH(*cur))
		cur++;
	    end = cur;
	    while ((*end != 0) && (!(XSD10_TYPES_IS_BLANK_CH(*end))))
		end++;
	    if (end == cur)
		break;
	    tmpValue = xsd10_types_strndup(cur, end - cur);
	    len++;

	    if (valNeeded)
		ret = xsd10_types_check_cvc_simple_type(node, itemType,
		    tmpValue, &curVal, 0, 1);
	    else
		ret = xsd10_types_check_cvc_simple_type(node, itemType,
		    tmpValue, NULL, 0, 1);
	    free(tmpValue);
	    tmpValue = NULL;
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
		if (ret < 0)
		    goto internal_error;
		ret = XML_XSD10_SCHEMAV_CVC_DATATYPE_VALID_1_2_2;
		break;
	    }
	    cur = end;
	} while (*cur != 0);
	(void) len;
	/* The facets of built in types are not checked (xmlSchemaValidateFacets). */
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
#undef XSD10_TYPES_NORMALIZE
}

/* Port of xmlSchemaCheckFacet (without a parser context, as used by RelaxNG): checks and
   computes the values of facets.
   Returns 0 if valid, a positive error code if not valid and
   -1 in case of an internal or API error. */
int
xml_xsd10_check_facet(XmlXsd10Facet *facet,
                    XmlXsd10Type *typeDecl,
		    const char *name)
{
    int ret = 0;

    (void) name;
    if ((facet == NULL) || (typeDecl == NULL))
        return(-1);

    switch (facet->type) {
        case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
        case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
	case XML_XSD10_SCHEMA_FACET_ENUMERATION: {
                /*
                 * Okay we need to validate the value
                 * at that point.
                 */
		XmlXsd10Type *base;

		/* 4.3.5.5 Constraints on enumeration Schema Components
		* Schema Component Constraint: enumeration valid restriction
		* It is an `error` if any member of {value} is not in the
		* `value space` of {base type definition}.
		*
		* minInclusive, maxInclusive, minExclusive, maxExclusive:
		* The value `must` be in the
		* `value space` of the `base type`.
		*/
		/*
		* This function is intended to deliver a compiled value
		* on the facet. In this implementation of XML Schemata the
		* type holding a facet, won't be a built-in type.
		* Thus to ensure that other API
		* calls (relaxng) do work, if the given type is a built-in
		* type, we will assume that the given built-in type *is
		* already* the base type.
		*/
		if (typeDecl->type != XML_XSD10_SCHEMA_TYPE_BASIC) {
		    base = typeDecl->baseType;
		    if (base == NULL) {
			return (-1);
		    }
		} else
		    base = typeDecl;

		/*
		* NOTE: This call does not check the content nodes,
		* since they are not available:
		* facet->node is just the node holding the facet
		* definition, *not* the attribute holding the *value*
		* of the facet.
		*/
		ret = xsd10_types_check_cvc_simple_type(
		    facet->node, base,
		    (const XmlXsd10Char *) facet->value, &(facet->val), 1, 0);
                if (ret != 0) {
		    if (ret < 0) {
			goto internal_error;
		    }
		    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
		    goto exit;
                } else if (facet->val == NULL) {
		    /* TODO */
		}
                break;
            }
        case XML_XSD10_SCHEMA_FACET_PATTERN:
            facet->regexp = (xsd10_types_regexp_hooks_.compile != NULL && facet->value != NULL) ?
                xsd10_types_regexp_hooks_.compile(facet->value) : NULL;
            if (facet->regexp == NULL) {
		ret = XML_XSD10_SCHEMAP_REGEXP_INVALID;
            }
            break;
        case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
        case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
        case XML_XSD10_SCHEMA_FACET_LENGTH:
        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
        case XML_XSD10_SCHEMA_FACET_MINLENGTH:

	    if (facet->type == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) {
		ret = xml_xsd10_validate_predefined_type(
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_PINTEGER),
		    facet->value, &(facet->val));
	    } else {
		ret = xml_xsd10_validate_predefined_type(
		    xml_xsd10_get_built_in_type(XML_XSD10_SCHEMAS_NNINTEGER),
		    facet->value, &(facet->val));
	    }
	    if (ret != 0) {
		if (ret < 0) {
		    goto internal_error;
		}
		ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
	    }
	    break;

        case XML_XSD10_SCHEMA_FACET_WHITESPACE:{
                if (xsd10_types_str_equal((const XmlXsd10Char *) facet->value, XSD10_TYPES_BAD_CAST "preserve")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_PRESERVE;
                } else if (xsd10_types_str_equal((const XmlXsd10Char *) facet->value, XSD10_TYPES_BAD_CAST "replace")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_REPLACE;
                } else if (xsd10_types_str_equal((const XmlXsd10Char *) facet->value, XSD10_TYPES_BAD_CAST "collapse")) {
                    facet->whitespace = XML_XSD10_SCHEMAS_FACET_COLLAPSE;
                } else {
		    ret = XML_XSD10_SCHEMAP_INVALID_FACET_VALUE;
                }
            }
            /* Falls through. */
        default:
            break;
    }
exit:
    return (ret);
internal_error:
    return (-1);
}

#undef XSD10_TYPES_NS
#undef XSD10_TYPES_BAD_CAST
#undef XSD10_TYPES_IS_BLANK_CH
#undef XSD10_TYPES_IS_WSP_REPLACE_CH
#undef XSD10_TYPES_IS_WSP_SPACE_CH
#undef XSD10_TYPES_IS_WSP_BLANK_CH
#undef XSD10_TYPES_SCAN_NC
#undef XSD10_TYPES_SCAN_NMTOKEN
#undef XSD10_TYPES_PRIMITIVE
#undef XSD10_TYPES_ATOMIC
#undef XSD10_TYPES_LIST
#undef XSD10_TYPES_REF
#undef XSD10_TYPES_DEF
#undef XSD10_TYPES_SIMPLE
#undef XSD10_TYPES_LISTDEF
#undef xsd10_types_float_def_
#undef XSD10_TYPES_IS_TZO_CHAR
#undef XSD10_TYPES_VALID_YEAR
#undef XSD10_TYPES_VALID_MONTH
#undef XSD10_TYPES_VALID_DAY
#undef XSD10_TYPES_VALID_HOUR
#undef XSD10_TYPES_VALID_MIN
#undef XSD10_TYPES_VALID_SEC
#undef XSD10_TYPES_VALID_TZO
#undef XSD10_TYPES_IS_LEAP
#undef XSD10_TYPES_MAX_DAYINMONTH
#undef XSD10_TYPES_VALID_MDAY
#undef XSD10_TYPES_VALID_DATE
#undef XSD10_TYPES_VALID_END_OF_DAY
#undef XSD10_TYPES_VALID_TIME
#undef XSD10_TYPES_VALID_DATETIME
#undef XSD10_TYPES_SECS_PER_MIN
#undef XSD10_TYPES_MINS_PER_HOUR
#undef XSD10_TYPES_HOURS_PER_DAY
#undef XSD10_TYPES_SECS_PER_HOUR
#undef XSD10_TYPES_SECS_PER_DAY
#undef XSD10_TYPES_MINS_PER_DAY
#undef XSD10_TYPES_DAY_IN_YEAR
#undef XSD10_TYPES_PARSE_2_DIGITS
#undef XSD10_TYPES_PARSE_FLOAT
#undef XSD10_TYPES_FQUOTIENT
#undef XSD10_TYPES_MODULO
#undef XSD10_TYPES_FQUOTIENT_RANGE
#undef XSD10_TYPES_MODULO_RANGE
#undef XSD10_TYPES_TIME_TO_NUMBER

#endif // XML_XSD10_TYPES_MODULE
