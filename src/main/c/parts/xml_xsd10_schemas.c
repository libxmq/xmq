/* This file is ported from libxml2 (xmlschemas.c, and the helpers
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

#ifndef BUILDING_DIST_XMQ

#define XML_XSD10_INTERNALS
#include"xml_xsd10_schemas.h"
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

#ifdef XML_XSD10_SCHEMAS_MODULE

/*
 * The XML Schemas namespaces
 */
const char xml_xsd10_ns[] = "http://www.w3.org/2001/XMLSchema";

const char xml_xsd10_instance_ns[] = "http://www.w3.org/2001/XMLSchema-instance";

const char xml_xsd10_namespace_ns[] = "http://www.w3.org/2000/xmlns/";

#define XSD10_SCHEMAS_IS_BLANK_CH(c) (((c) == 0x20) || ((0x9 <= (c)) && ((c) <= 0xa)) || ((c) == 0xd))

/************************************************************************
 *									*
 *			String helpers (libxml2 xmlstring.c)		*
 *									*
 ************************************************************************/

int xml_xsd10_str_equal(const char *a, const char *b)
{
    if (a == b) return 1;
    if (a == NULL || b == NULL) return 0;
    return strcmp(a, b) == 0;
}

char *xml_xsd10_strndup(const char *s, int len)
{
    char *ret;

    if ((s == NULL) || (len < 0)) return NULL;
    ret = (char *) malloc((size_t) len + 1);
    if (ret == NULL) return NULL;
    memcpy(ret, s, (size_t) len);
    ret[len] = 0;
    return ret;
}

char *xml_xsd10_strdup(const char *s)
{
    if (s == NULL) return NULL;
    return xml_xsd10_strndup(s, (int) strlen(s));
}

char *xml_xsd10_char_strdup(const char *s)
{
    return xml_xsd10_strdup(s);
}

int xml_xsd10_strlen(const char *s)
{
    if (s == NULL) return 0;
    size_t len = strlen(s);
    return len > INT_MAX ? INT_MAX : (int) len;
}

char *xml_xsd10_strncat(char *cur, const char *add, int len)
{
    int size;
    char *ret;

    if ((add == NULL) || (len == 0))
        return(cur);
    if (len < 0) {
        free(cur);
        return(NULL);
    }
    if (cur == NULL)
        return(xml_xsd10_strndup(add, len));

    size = xml_xsd10_strlen(cur);
    if ((size < 0) || (size > INT_MAX - len)) {
        free(cur);
        return(NULL);
    }
    ret = (char *) realloc(cur, (size_t) size + len + 1);
    if (ret == NULL) {
        free(cur);
        return(NULL);
    }
    memcpy(&ret[size], add, (size_t) len);
    ret[size + len] = 0;
    return(ret);
}

char *xml_xsd10_strcat(char *cur, const char *add)
{
    if (add == NULL)
        return(cur);
    if (cur == NULL)
        return(xml_xsd10_strdup(add));
    return(xml_xsd10_strncat(cur, add, xml_xsd10_strlen(add)));
}

char *xml_xsd10_strncat_new(const char *str1, const char *str2, int len)
{
    int size;
    char *ret;

    if (len < 0) {
        len = xml_xsd10_strlen(str2);
        if (len < 0)
            return(NULL);
    }
    if (str1 == NULL)
        return(xml_xsd10_strndup(str2, len));
    if ((str2 == NULL) || (len == 0))
        return(xml_xsd10_strdup(str1));

    size = xml_xsd10_strlen(str1);
    if ((size < 0) || (size > INT_MAX - len))
        return(NULL);
    ret = (char *) malloc((size_t) size + len + 1);
    if (ret == NULL)
        return(NULL);
    memcpy(ret, str1, (size_t) size);
    memcpy(&ret[size], str2, (size_t) len);
    ret[size + len] = 0;
    return(ret);
}

/* Port of xmlEscapeFormatString: replaces a string with an escaped string,
   all '%' characters are doubled. */
char *xml_xsd10_escape_format_string(char **msg)
{
    char *msgPtr = NULL;
    char *result = NULL;
    char *resultPtr = NULL;
    size_t count = 0;
    size_t msgLen = 0;
    size_t resultLen = 0;

    if (!msg || !*msg)
        return(NULL);

    for (msgPtr = *msg; *msgPtr != '\0'; ++msgPtr) {
        ++msgLen;
        if (*msgPtr == '%')
            ++count;
    }

    if (count == 0)
        return(*msg);

    if ((count > INT_MAX) || (msgLen > INT_MAX - count))
        return(NULL);
    resultLen = msgLen + count + 1;
    result = (char *) malloc(resultLen);
    if (result == NULL) {
        /* Clear *msg to prevent format string vulnerabilities in
           out-of-memory situations. */
        free(*msg);
        *msg = NULL;
        return(NULL);
    }

    for (msgPtr = *msg, resultPtr = result; *msgPtr != '\0'; ++msgPtr, ++resultPtr) {
        *resultPtr = *msgPtr;
        if (*msgPtr == '%')
            *(++resultPtr) = '%';
    }
    result[resultLen - 1] = '\0';

    free(*msg);
    *msg = result;

    return *msg;
}

/* Port of xmlGrowCapacity. */
int xml_xsd10_grow_capacity(int capacity, size_t elemSize, int min, int max)
{
    int extra;

    if (capacity <= 0)
        return(min);

    if ((capacity >= max) ||
        ((size_t) capacity > SIZE_MAX / 2 / elemSize))
        return(-1);

    /* Grow by 50% */
    extra = (capacity + 1) / 2;

    if (capacity > max - extra)
        return(max);

    return(capacity + extra);
}

/* Port of xmlStrVASPrintf with maxSize MAX_ERR_MSG_SIZE: *out is the malloced
   formatted message. Returns 0 on success, 1 if truncated or on other errors
   and -1 on out of memory. */
static int xsd10_schemas_vasprintf(char **out, int maxSize, const char *msg, va_list ap)
{
    char empty[1];
    va_list copy;
    char *buf;
    int res, size;
    int truncated = 0;

    if (out == NULL)
        return(1);
    *out = NULL;
    if (msg == NULL)
        return(1);
    if (maxSize < 32)
        maxSize = 32;

    va_copy(copy, ap);
    res = vsnprintf(empty, 1, msg, copy);
    va_end(copy);

    if (res > 0) {
        /* snprintf seems to work according to C99. */

        if (res < maxSize) {
            size = res + 1;
        } else {
            size = maxSize;
            truncated = 1;
        }
        buf = (char *) malloc((size_t) size);
        if (buf == NULL)
            return(-1);
        va_copy(copy, ap);
        res = vsnprintf(buf, (size_t) size, msg, copy);
        va_end(copy);
        if (res < 0) {
            free(buf);
            return(1);
        }
    } else {
        /* The output is the empty string (or vsnprintf failed). */
        buf = NULL;
        size = 32;
        while (1) {
            buf = (char *) malloc((size_t) size);
            if (buf == NULL)
                return(-1);

            va_copy(copy, ap);
            res = vsnprintf(buf, (size_t) size, msg, copy);
            va_end(copy);
            if ((res >= 0) && (res < size - 1))
                break;

            if (size >= maxSize) {
                truncated = 1;
                break;
            }

            free(buf);

            if (size > maxSize / 2)
                size = maxSize;
            else
                size *= 2;
        }
    }

    /*
     * If the output was truncated, make sure that the buffer doesn't
     * end with a truncated UTF-8 sequence.
     */
    if (truncated != 0) {
        int i = size - 1;

        while (i > 0) {
            /* Break after ASCII */
            if ((unsigned char) buf[i-1] < 0x80)
                break;
            i -= 1;
            /* Break before non-ASCII */
            if ((unsigned char) buf[i] >= 0xc0)
                break;
        }

        buf[i] = 0;
    }

    *out = buf;
    return(truncated);
}

/************************************************************************
 *									*
 *			The dict (libxml2 dict.c)			*
 *									*
 ************************************************************************/

/* An interning dict: open addressing over the malloced strings. Every string
   lives until the dict is freed, so the pointers can be compared. */
struct XmlXsd10Dict
{
    int ref_counter;
    size_t size;      /* The number of slots, a power of two (or 0). */
    size_t nb_elems;
    char **table;
    unsigned *hashes;
};

static unsigned xsd10_schemas_hash_bytes(unsigned h, const char *s, size_t len)
{
    /* FNV-1a */
    for (size_t i = 0; i < len; i++)
    {
        h ^= (unsigned char) s[i];
        h *= 16777619u;
    }
    return h;
}

XmlXsd10Dict *xml_xsd10_dict_create(void)
{
    XmlXsd10Dict *dict = (XmlXsd10Dict *) calloc(1, sizeof(XmlXsd10Dict));
    if (dict == NULL) return NULL;
    dict->ref_counter = 1;
    return dict;
}

int xml_xsd10_dict_reference(XmlXsd10Dict *dict)
{
    if (dict == NULL) return -1;
    dict->ref_counter++;
    return 0;
}

void xml_xsd10_dict_free(XmlXsd10Dict *dict)
{
    if (dict == NULL) return;
    dict->ref_counter--;
    if (dict->ref_counter > 0) return;
    for (size_t i = 0; i < dict->size; i++) free(dict->table[i]);
    free(dict->table);
    free(dict->hashes);
    free(dict);
}

static int xsd10_schemas_dict_grow(XmlXsd10Dict *dict)
{
    size_t new_size = dict->size ? dict->size * 2 : 64;
    char **table = (char **) calloc(new_size, sizeof(char *));
    unsigned *hashes = (unsigned *) calloc(new_size, sizeof(unsigned));
    if (table == NULL || hashes == NULL)
    {
        free(table);
        free(hashes);
        return -1;
    }
    for (size_t i = 0; i < dict->size; i++)
    {
        if (dict->table[i] == NULL) continue;
        size_t j = dict->hashes[i] & (new_size - 1);
        while (table[j] != NULL) j = (j + 1) & (new_size - 1);
        table[j] = dict->table[i];
        hashes[j] = dict->hashes[i];
    }
    free(dict->table);
    free(dict->hashes);
    dict->table = table;
    dict->hashes = hashes;
    dict->size = new_size;
    return 0;
}

const char *xml_xsd10_dict_lookup(XmlXsd10Dict *dict, const char *name, int len)
{
    size_t l;
    unsigned h;

    if ((dict == NULL) || (name == NULL))
        return NULL;
    if (len < 0)
        l = strlen(name);
    else
        l = (size_t) len;
    h = xsd10_schemas_hash_bytes(2166136261u, name, l);

    if (dict->size != 0)
    {
        size_t j = h & (dict->size - 1);
        while (dict->table[j] != NULL)
        {
            if (dict->hashes[j] == h && strncmp(dict->table[j], name, l) == 0 && dict->table[j][l] == 0)
                return dict->table[j];
            j = (j + 1) & (dict->size - 1);
        }
    }
    if ((dict->nb_elems + 1) * 2 > dict->size)
    {
        if (xsd10_schemas_dict_grow(dict) < 0) return NULL;
    }
    char *copy = (char *) malloc(l + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, name, l);
    copy[l] = 0;
    size_t j = h & (dict->size - 1);
    while (dict->table[j] != NULL) j = (j + 1) & (dict->size - 1);
    dict->table[j] = copy;
    dict->hashes[j] = h;
    dict->nb_elems++;
    return copy;
}

const char *xml_xsd10_dict_qlookup(XmlXsd10Dict *dict, const char *prefix, const char *name)
{
    if (name == NULL) return NULL;
    if (prefix == NULL) return xml_xsd10_dict_lookup(dict, name, -1);

    size_t pl = strlen(prefix);
    size_t nl = strlen(name);
    char *buf = (char *) malloc(pl + nl + 2);
    if (buf == NULL) return NULL;
    memcpy(buf, prefix, pl);
    buf[pl] = ':';
    memcpy(buf + pl + 1, name, nl + 1);
    const char *ret = xml_xsd10_dict_lookup(dict, buf, (int) (pl + nl + 1));
    free(buf);
    return ret;
}

int xml_xsd10_dict_owns(XmlXsd10Dict *dict, const char *str)
{
    if ((dict == NULL) || (str == NULL)) return -1;
    for (size_t i = 0; i < dict->size; i++)
    {
        if (dict->table[i] == str) return 1;
    }
    return 0;
}

int xml_xsd10_dict_size(XmlXsd10Dict *dict)
{
    if (dict == NULL) return -1;
    return (int) dict->nb_elems;
}

/************************************************************************
 *									*
 *			Hash tables (libxml2 hash.c api)		*
 *									*
 ************************************************************************/

/* The entries are kept in insertion order (the scan order), a bucket array
   chains them for the lookups. Removed entries are only marked. */
typedef struct Xsd10SchemasHashEntry
{
    char *key;
    char *key2;
    char *key3;
    void *payload;
    unsigned hash_value;
    int next;   /* The next entry in the same bucket, -1 for none. */
    int used;   /* 0 when removed. */
} Xsd10SchemasHashEntry;

struct XmlXsd10Hash
{
    Xsd10SchemasHashEntry *entries;
    int nb_entries;  /* Including the removed ones. */
    int size_entries;
    int *buckets;
    int nb_buckets;  /* A power of two. */
    int nb_elems;    /* The used entries. */
};

static unsigned xsd10_schemas_hash_keys(const char *key, const char *key2, const char *key3)
{
    unsigned h = 2166136261u;
    const char *keys[3] = { key, key2, key3 };
    for (int i = 0; i < 3; i++)
    {
        /* Distinguish NULL from "" and the key boundaries. */
        char marker = keys[i] ? (char) 1 : (char) 2;
        h = xsd10_schemas_hash_bytes(h, &marker, 1);
        if (keys[i]) h = xsd10_schemas_hash_bytes(h, keys[i], strlen(keys[i]));
    }
    return h;
}

XmlXsd10Hash *xml_xsd10_hash_create(int size)
{
    XmlXsd10Hash *hash = (XmlXsd10Hash *) calloc(1, sizeof(XmlXsd10Hash));
    if (hash == NULL) return NULL;
    int nb = 8;
    while (nb < size && nb < (1 << 24)) nb *= 2;
    hash->buckets = (int *) malloc(sizeof(int) * (size_t) nb);
    if (hash->buckets == NULL)
    {
        free(hash);
        return NULL;
    }
    for (int i = 0; i < nb; i++) hash->buckets[i] = -1;
    hash->nb_buckets = nb;
    return hash;
}

XmlXsd10Hash *xml_xsd10_hash_create_dict(int size, XmlXsd10Dict *dict)
{
    (void) dict;
    return xml_xsd10_hash_create(size);
}

void xml_xsd10_hash_free(XmlXsd10Hash *hash, XmlXsd10HashDeallocator dealloc)
{
    if (hash == NULL) return;
    for (int i = 0; i < hash->nb_entries; i++)
    {
        Xsd10SchemasHashEntry *e = &hash->entries[i];
        if (!e->used) continue;
        if ((dealloc != NULL) && (e->payload != NULL))
            dealloc(e->payload, e->key);
        free(e->key);
        free(e->key2);
        free(e->key3);
    }
    free(hash->entries);
    free(hash->buckets);
    free(hash);
}

static int xsd10_schemas_hash_find(XmlXsd10Hash *hash, const char *key, const char *key2, const char *key3, unsigned hv)
{
    int i = hash->buckets[hv & (unsigned) (hash->nb_buckets - 1)];
    while (i >= 0)
    {
        Xsd10SchemasHashEntry *e = &hash->entries[i];
        if (e->hash_value == hv &&
            xml_xsd10_str_equal(e->key, key) &&
            xml_xsd10_str_equal(e->key2, key2) &&
            xml_xsd10_str_equal(e->key3, key3))
            return i;
        i = e->next;
    }
    return -1;
}

static int xsd10_schemas_hash_rehash(XmlXsd10Hash *hash, int nb)
{
    int *buckets = (int *) malloc(sizeof(int) * (size_t) nb);
    if (buckets == NULL) return -1;
    for (int i = 0; i < nb; i++) buckets[i] = -1;
    /* Insert backwards so that the chains keep the insertion order (not required). */
    for (int i = hash->nb_entries - 1; i >= 0; i--)
    {
        Xsd10SchemasHashEntry *e = &hash->entries[i];
        if (!e->used) continue;
        int b = (int) (e->hash_value & (unsigned) (nb - 1));
        e->next = buckets[b];
        buckets[b] = i;
    }
    free(hash->buckets);
    hash->buckets = buckets;
    hash->nb_buckets = nb;
    return 0;
}

/* Add (update == 0) or add/replace (update != 0). Returns 1 if added, 0 if the key exists
   (and was replaced if update), -1 on errors. */
static int xsd10_schemas_hash_update_internal(XmlXsd10Hash *hash, const char *key, const char *key2,
                                              const char *key3, void *payload,
                                              XmlXsd10HashDeallocator dealloc, int update)
{
    if ((hash == NULL) || (key == NULL)) return -1;
    unsigned hv = xsd10_schemas_hash_keys(key, key2, key3);
    int i = xsd10_schemas_hash_find(hash, key, key2, key3, hv);
    if (i >= 0)
    {
        if (update)
        {
            Xsd10SchemasHashEntry *e = &hash->entries[i];
            if ((dealloc != NULL) && (e->payload != NULL)) dealloc(e->payload, e->key);
            e->payload = payload;
        }
        return 0;
    }
    if (hash->nb_entries >= hash->size_entries)
    {
        int ns = xml_xsd10_grow_capacity(hash->size_entries, sizeof(Xsd10SchemasHashEntry), 8, XML_XSD10_MAX_ITEMS);
        if (ns < 0) return -1;
        Xsd10SchemasHashEntry *tmp = (Xsd10SchemasHashEntry *) realloc(hash->entries, sizeof(Xsd10SchemasHashEntry) * (size_t) ns);
        if (tmp == NULL) return -1;
        hash->entries = tmp;
        hash->size_entries = ns;
    }
    Xsd10SchemasHashEntry *e = &hash->entries[hash->nb_entries];
    memset(e, 0, sizeof(*e));
    e->key = xml_xsd10_strdup(key);
    e->key2 = xml_xsd10_strdup(key2);
    e->key3 = xml_xsd10_strdup(key3);
    if ((e->key == NULL) || ((key2 != NULL) && (e->key2 == NULL)) || ((key3 != NULL) && (e->key3 == NULL)))
    {
        free(e->key);
        free(e->key2);
        free(e->key3);
        return -1;
    }
    e->payload = payload;
    e->hash_value = hv;
    e->used = 1;
    int b = (int) (hv & (unsigned) (hash->nb_buckets - 1));
    e->next = hash->buckets[b];
    hash->buckets[b] = hash->nb_entries;
    hash->nb_entries++;
    hash->nb_elems++;
    if ((hash->nb_elems > hash->nb_buckets) && (hash->nb_buckets < (1 << 28)))
        xsd10_schemas_hash_rehash(hash, hash->nb_buckets * 4);
    return 1;
}

int xml_xsd10_hash_add_entry3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3, void *payload)
{
    int res = xsd10_schemas_hash_update_internal(hash, name, name2, name3, payload, NULL, 0);

    if (res == 0)
        res = -1;
    else if (res == 1)
        res = 0;

    return(res);
}

int xml_xsd10_hash_add_entry2(XmlXsd10Hash *hash, const char *name, const char *name2, void *payload)
{
    return xml_xsd10_hash_add_entry3(hash, name, name2, NULL, payload);
}

int xml_xsd10_hash_add_entry(XmlXsd10Hash *hash, const char *name, void *payload)
{
    return xml_xsd10_hash_add_entry3(hash, name, NULL, NULL, payload);
}

int xml_xsd10_hash_update_entry(XmlXsd10Hash *hash, const char *name, void *payload, XmlXsd10HashDeallocator dealloc)
{
    int res = xsd10_schemas_hash_update_internal(hash, name, NULL, NULL, payload, dealloc, 1);

    if (res == 1)
        res = 0;

    return(res);
}

void *xml_xsd10_hash_lookup3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3)
{
    if ((hash == NULL) || (name == NULL)) return NULL;
    int i = xsd10_schemas_hash_find(hash, name, name2, name3, xsd10_schemas_hash_keys(name, name2, name3));
    if (i < 0) return NULL;
    return hash->entries[i].payload;
}

void *xml_xsd10_hash_lookup2(XmlXsd10Hash *hash, const char *name, const char *name2)
{
    return xml_xsd10_hash_lookup3(hash, name, name2, NULL);
}

void *xml_xsd10_hash_lookup(XmlXsd10Hash *hash, const char *name)
{
    return xml_xsd10_hash_lookup3(hash, name, NULL, NULL);
}

int xml_xsd10_hash_remove_entry3(XmlXsd10Hash *hash, const char *name, const char *name2, const char *name3,
                                 XmlXsd10HashDeallocator dealloc)
{
    if ((hash == NULL) || (name == NULL)) return -1;
    unsigned hv = xsd10_schemas_hash_keys(name, name2, name3);
    int i = xsd10_schemas_hash_find(hash, name, name2, name3, hv);
    if (i < 0) return -1;
    Xsd10SchemasHashEntry *e = &hash->entries[i];
    if ((dealloc != NULL) && (e->payload != NULL))
        dealloc(e->payload, e->key);
    /* Unlink from the bucket chain. */
    int b = (int) (hv & (unsigned) (hash->nb_buckets - 1));
    int *link = &hash->buckets[b];
    while (*link != i) link = &hash->entries[*link].next;
    *link = e->next;
    free(e->key);
    free(e->key2);
    free(e->key3);
    e->key = e->key2 = e->key3 = NULL;
    e->payload = NULL;
    e->used = 0;
    e->next = -1;
    hash->nb_elems--;
    return 0;
}

int xml_xsd10_hash_remove_entry2(XmlXsd10Hash *hash, const char *name, const char *name2, XmlXsd10HashDeallocator dealloc)
{
    return xml_xsd10_hash_remove_entry3(hash, name, name2, NULL, dealloc);
}

int xml_xsd10_hash_remove_entry(XmlXsd10Hash *hash, const char *name, XmlXsd10HashDeallocator dealloc)
{
    return xml_xsd10_hash_remove_entry3(hash, name, NULL, NULL, dealloc);
}

int xml_xsd10_hash_size(XmlXsd10Hash *hash)
{
    if (hash == NULL) return -1;
    return hash->nb_elems;
}

void xml_xsd10_hash_scan_full(XmlXsd10Hash *hash, XmlXsd10HashScannerFull scan, void *data)
{
    if ((hash == NULL) || (scan == NULL)) return;
    /* Entries added by the callback are scanned too, removed ones are skipped. */
    for (int i = 0; i < hash->nb_entries; i++)
    {
        Xsd10SchemasHashEntry *e = &hash->entries[i];
        if (e->used && (e->payload != NULL))
            scan(e->payload, data, e->key, e->key2, e->key3);
    }
}

typedef struct
{
    XmlXsd10HashScanner scan;
    void *data;
} Xsd10SchemasStubData;

static void xsd10_schemas_stub_hash_scanner_full(void *payload, void *data, const char *key,
                                                 const char *key2, const char *key3)
{
    Xsd10SchemasStubData *sdata = (Xsd10SchemasStubData *) data;
    (void) key2;
    (void) key3;
    sdata->scan(payload, sdata->data, key);
}

void xml_xsd10_hash_scan(XmlXsd10Hash *hash, XmlXsd10HashScanner scan, void *data)
{
    Xsd10SchemasStubData sdata;
    sdata.data = data;
    sdata.scan = scan;
    xml_xsd10_hash_scan_full(hash, xsd10_schemas_stub_hash_scanner_full, &sdata);
}

/************************************************************************
 *									*
 *		Tree helpers (libxml2 tree.c, parser.c)			*
 *									*
 ************************************************************************/

/* Port of xmlGetUTF8Char. */
static int xsd10_schemas_get_utf8_char(const unsigned char *utf, int *len)
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

/* Port of xmlIsNameStartCharNew (XML 1.0 fifth edition). */
static int xsd10_schemas_is_name_start_char(int c)
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
static int xsd10_schemas_is_name_char(int c)
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

/* Port of xmlScanName with XML_SCAN_NC (no size limit): a pointer to the end of
   the name, ptr if the name is invalid. */
static const unsigned char *xsd10_schemas_scan_ncname(const unsigned char *ptr)
{
    int first = 1;

    while (1) {
        int c, len;

        c = *ptr;
        if (c < 0x80) {
            if (c == ':')
                break;
            len = 1;
        } else {
            len = 4;
            c = xsd10_schemas_get_utf8_char(ptr, &len);
            if (c < 0)
                break;
        }

        if (first ? !xsd10_schemas_is_name_start_char(c) : !xsd10_schemas_is_name_char(c))
            break;

        ptr += len;
        first = 0;
    }

    return(ptr);
}

/* Port of xmlValidateNCName. */
int xml_xsd10_tree_validate_ncname(const char *v, int space)
{
    const unsigned char *value = (const unsigned char *) v;
    const unsigned char *cur;

    if (value == NULL)
        return(-1);

    cur = value;

    if (space) {
	while (XSD10_SCHEMAS_IS_BLANK_CH(*cur))
            cur++;
    }

    value = cur;
    cur = xsd10_schemas_scan_ncname(value);
    if ((cur == NULL) || (cur == value))
        return(1);

    if (space) {
	while (XSD10_SCHEMAS_IS_BLANK_CH(*cur))
            cur++;
    }

    return(*cur != 0);
}

/* Port of xmlValidateQName. */
int xml_xsd10_tree_validate_qname(const char *v, int space)
{
    const unsigned char *value = (const unsigned char *) v;
    const unsigned char *cur;

    if (value == NULL)
        return(-1);

    cur = value;

    if (space) {
	while (XSD10_SCHEMAS_IS_BLANK_CH(*cur))
            cur++;
    }

    value = cur;
    cur = xsd10_schemas_scan_ncname(value);
    if ((cur == NULL) || (cur == value))
        return(1);

    if (*cur == ':') {
        cur += 1;
        value = cur;
        cur = xsd10_schemas_scan_ncname(value);
        if ((cur == NULL) || (cur == value))
            return(1);
    }

    if (space) {
	while (XSD10_SCHEMAS_IS_BLANK_CH(*cur))
            cur++;
    }

    return(*cur != 0);
}

/* Port of xmlSplitQName2. */
char *xml_xsd10_tree_split_qname2(const char *name, char **prefix)
{
    int len = 0;
    char *ret = NULL;

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

    *prefix = xml_xsd10_strndup(name, len);
    if (*prefix == NULL)
	return(NULL);
    ret = xml_xsd10_strdup(&name[len + 1]);
    if (ret == NULL) {
	if (*prefix != NULL) {
	    free(*prefix);
	    *prefix = NULL;
	}
	return(NULL);
    }

    return(ret);
}

/* Port of xmlSplitQName3. */
const char *xml_xsd10_tree_split_qname3(const char *name, int *len)
{
    int l = 0;

    if (name == NULL) return(NULL);
    if (len == NULL) return(NULL);

    /* nasty but valid */
    if (name[0] == ':')
	return(NULL);

    /*
     * we are not trying to validate but just to cut, and yes it will
     * work even if this is as set of UTF-8 encoded chars
     */
    while ((name[l] != 0) && (name[l] != ':'))
	l++;

    if ((name[l] == 0) || (name[l+1] == 0))
	return(NULL);

    *len = l;

    return(&name[l+1]);
}

/* The closest element of node (node itself, the owner of an attribute, the parent of a text). */
static XMQNode *xsd10_schemas_element_of(XMQNode *node)
{
    while ((node != NULL) && (xml_node_kind(node) != XML_DOM_ELEMENT))
        node = xml_node_parent(node);
    return node;
}

/* Port of xmlSearchNs. */
XmlNs *xml_xsd10_tree_search_ns(XMQNode *node, const char *prefix)
{
    XmlNs *ns;

    if (node == NULL)
        return NULL;
    node = xsd10_schemas_element_of(node);
    if (node == NULL)
        return NULL;
    ns = xml_ns_lookup_prefix(node, prefix);
    if ((ns == NULL) || (xml_ns_href(ns) == NULL))
        return NULL;
    return ns;
}

/* Port of xmlSearchNsByHref: a declaration of href in scope of node whose prefix is not
   shadowed, with a prefix if node is an attribute. */
XmlNs *xml_xsd10_tree_search_ns_by_href(XMQNode *node, const char *href)
{
    XMQNode *elem, *cur;
    int is_attr;

    if ((node == NULL) || (href == NULL))
        return NULL;
    is_attr = (xml_node_kind(node) == XML_DOM_ATTRIBUTE);
    elem = xsd10_schemas_element_of(node);
    if (elem == NULL)
        return NULL;
    if (xml_xsd10_str_equal(href, XML_XSD10_XML_XML_NAMESPACE))
        return xml_ns_lookup_prefix(elem, "xml");
    for (cur = elem; (cur != NULL) && (xml_node_kind(cur) == XML_DOM_ELEMENT); cur = xml_node_parent(cur))
    {
        for (XmlNs *ns = xml_node_first_ns_decl(cur); ns != NULL; ns = xml_ns_next_decl(ns))
        {
            if (!xml_xsd10_str_equal(xml_ns_href(ns), href))
                continue;
            if (is_attr && (xml_ns_prefix(ns) == NULL))
                continue;
            if (xml_ns_lookup_prefix(elem, xml_ns_prefix(ns)) == ns)
                return ns;
        }
    }
    return NULL;
}

/* Port of xmlGetNsList. */
XmlNs **xml_xsd10_tree_get_ns_list(XMQNode *node)
{
    XmlNs **namespaces = NULL;
    int nbns = 0;
    int maxns = 0;
    int i;

    if (node == NULL)
        return(NULL);

    while (node != NULL) {
        if (xml_node_kind(node) == XML_DOM_ELEMENT) {
            for (XmlNs *cur = xml_node_first_ns_decl(node); cur != NULL; cur = xml_ns_next_decl(cur)) {
                for (i = 0; i < nbns; i++) {
                    if (xml_xsd10_str_equal(xml_ns_prefix(cur), xml_ns_prefix(namespaces[i])))
                        break;
                }
                if (i >= nbns) {
                    if (nbns >= maxns) {
                        XmlNs **tmp;
                        int newSize;

                        newSize = xml_xsd10_grow_capacity(maxns, sizeof(tmp[0]),
                                                          10, XML_XSD10_MAX_ITEMS);
                        if (newSize < 0) {
                            free(namespaces);
                            return(NULL);
                        }
                        tmp = (XmlNs **) realloc(namespaces,
                                                 ((size_t) newSize + 1) * sizeof(tmp[0]));
                        if (tmp == NULL) {
                            free(namespaces);
                            return(NULL);
                        }
                        namespaces = tmp;
                        maxns = newSize;
                    }
                    namespaces[nbns++] = cur;
                    namespaces[nbns] = NULL;
                }
            }
        }
        node = xml_node_parent(node);
    }

    return(namespaces);
}

/* Port of xmlGetNoNsProp. */
char *xml_xsd10_tree_get_no_ns_prop(XMQNode *node, const char *name)
{
    if ((node == NULL) || (name == NULL) || (xml_node_kind(node) != XML_DOM_ELEMENT))
        return NULL;
    for (XMQAttr *a = xml_node_first_attr(node); a != NULL; a = xml_attr_next(a))
    {
        XMQNode *an = xml_attr_node(a);
        if ((xml_node_ns_uri(an) == NULL) && xml_xsd10_str_equal(xml_node_name(an), name))
            return xml_node_string_value(an);
    }
    return NULL;
}

/* Port of xmlHasProp (without the dtd defaults). */
XMQNode *xml_xsd10_tree_has_prop(XMQNode *node, const char *name)
{
    if ((node == NULL) || (name == NULL) || (xml_node_kind(node) != XML_DOM_ELEMENT))
        return NULL;
    for (XMQAttr *a = xml_node_first_attr(node); a != NULL; a = xml_attr_next(a))
    {
        XMQNode *an = xml_attr_node(a);
        if (xml_xsd10_str_equal(xml_node_name(an), name))
            return an;
    }
    return NULL;
}

/* Port of xmlNodeGetContent. */
char *xml_xsd10_tree_node_get_content(XMQNode *node)
{
    if (node == NULL)
        return NULL;
    switch (xml_node_kind(node))
    {
    case XML_DOM_ELEMENT:
    case XML_DOM_ATTRIBUTE:
    case XML_DOM_DOCUMENT:
        return xml_node_string_value(node);
    case XML_DOM_ENTITY_REF:
        return xml_xsd10_strdup("");
    default:
        break;
    }
    const char *content = xml_node_content(node);
    return xml_xsd10_strdup(content ? content : "");
}

/* The value of the attribute name in namespace ns_uri, malloced, NULL if absent. */
static char *xsd10_schemas_get_ns_prop(XMQNode *node, const char *name, const char *ns_uri)
{
    XMQAttr *a = xml_node_get_attr(node, name, ns_uri);
    if (a == NULL)
        return NULL;
    return xml_node_string_value(xml_attr_node(a));
}

/* Port of xmlNodeGetBase (xmlNodeGetBaseSafe). */
char *xml_xsd10_tree_node_get_base(XmlDom *doc, XMQNode *cur)
{
    char *ret = NULL;
    char *base, *newbase;
    int res;

    if ((cur == NULL) && (doc == NULL))
        return(NULL);
    if (doc == NULL)
        doc = xml_node_dom(cur);

    while (cur != NULL) {
	if (xml_node_kind(cur) == XML_DOM_ELEMENT) {
	    base = xsd10_schemas_get_ns_prop(cur, "base", XML_XSD10_XML_XML_NAMESPACE);
	    if (base != NULL) {
		if (ret != NULL) {
		    res = xml_uri_build_safe(ret, base, &newbase);
                    free(ret);
                    free(base);
                    if (res != 0)
                        return(NULL);
		    ret = newbase;
		} else {
		    ret = base;
		}
		if ((!strncmp(ret, "http://", 7)) ||
		    (!strncmp(ret, "ftp://", 6)) ||
		    (!strncmp(ret, "urn:", 4)))
                    return(ret);
	    }
	}
	cur = xml_node_parent(cur);
    }

    if ((doc != NULL) && (xml_dom_url(doc) != NULL)) {
	if (ret == NULL) {
	    ret = xml_xsd10_strdup(xml_dom_url(doc));
        } else {
            res = xml_uri_build_safe(ret, xml_dom_url(doc), &newbase);
            free(ret);
            if (res != 0)
                return(NULL);
            ret = newbase;
        }
    }

    return(ret);
}

/* Port of xmlNodeGetSpacePreserve. */
int xml_xsd10_tree_node_get_space_preserve(XMQNode *cur)
{
    char *space;

    if ((cur == NULL) || (xml_node_kind(cur) != XML_DOM_ELEMENT))
        return(-1);

    while ((cur != NULL) && (xml_node_kind(cur) == XML_DOM_ELEMENT)) {
	space = xsd10_schemas_get_ns_prop(cur, "space", XML_XSD10_XML_XML_NAMESPACE);
	if (space != NULL) {
	    if (xml_xsd10_str_equal(space, "preserve")) {
		free(space);
		return(1);
	    }
	    if (xml_xsd10_str_equal(space, "default")) {
		free(space);
		return(0);
	    }
	    free(space);
	}

	cur = xml_node_parent(cur);
    }

    return(-1);
}

static long xsd10_schemas_get_line_no_internal(XMQNode *node, int depth)
{
    long result = -1;

    if (depth >= 5)
        return(-1);

    if (!node)
        return result;
    XmlNodeKind kind = xml_node_kind(node);
    if ((kind == XML_DOM_ELEMENT) ||
        (kind == XML_DOM_TEXT) ||
	(kind == XML_DOM_COMMENT) ||
	(kind == XML_DOM_PI)) {
	result = xml_node_line(node);
    } else {
        XMQNode *prev = xml_node_prev(node);
        XMQNode *parent = xml_node_parent(node);
        XmlNodeKind pk = prev ? xml_node_kind(prev) : (XmlNodeKind) 0;
        if ((prev != NULL) &&
            ((pk == XML_DOM_ELEMENT) ||
             (pk == XML_DOM_TEXT) ||
             (pk == XML_DOM_COMMENT) ||
             (pk == XML_DOM_PI)))
            result = xsd10_schemas_get_line_no_internal(prev, depth + 1);
        else if ((parent != NULL) &&
                 (xml_node_kind(parent) == XML_DOM_ELEMENT))
            result = xsd10_schemas_get_line_no_internal(parent, depth + 1);
    }

    return result;
}

/* Port of xmlGetLineNo. */
long xml_xsd10_tree_get_line_no(XMQNode *node)
{
    return xsd10_schemas_get_line_no_internal(node, 0);
}

/************************************************************************
 *									*
 *		Error raising (libxml2 error.c)				*
 *									*
 ************************************************************************/

void xml_xsd10_generic_error(void *ctx, const char *msg, ...)
{
    va_list args;

    (void) ctx;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
}

void xml_xsd10_format_error(const XmlXsd10Error *err, XmlXsd10ValidityErrorFunc channel, void *data)
{
    const char *message;
    const char *file;
    int line;
    int code;
    int domain;
    const char *name = NULL;
    XMQNode *node;
    XmlXsd10ErrorLevel level;

    if ((err == NULL) || (channel == NULL))
        return;

    message = err->message;
    file = err->file;
    line = err->line;
    code = err->code;
    domain = err->domain;
    level = err->level;
    node = err->node;

    if (code == XML_XSD10_ERR_OK)
        return;

    if ((node != NULL) && (xml_node_kind(node) == XML_DOM_ELEMENT) &&
        (domain != XML_XSD10_FROM_SCHEMASV))
        name = xml_node_name(node);

    /* There is never a parser context (libxml2 prints its input position here). */
    if (file != NULL)
        channel(data, "%s:%d: ", file, line);
    else if ((line != 0) &&
             ((domain == XML_XSD10_FROM_PARSER) || (domain == XML_XSD10_FROM_SCHEMASV)||
              (domain == XML_XSD10_FROM_SCHEMASP)))
        channel(data, "Entity: line %d: ", line);
    if (name != NULL) {
        channel(data, "element %s: ", name);
    }
    switch (domain) {
        case XML_XSD10_FROM_PARSER:
            channel(data, "parser ");
            break;
        case XML_XSD10_FROM_MEMORY:
            channel(data, "memory ");
            break;
        case XML_XSD10_FROM_IO:
            channel(data, "I/O ");
            break;
        case XML_XSD10_FROM_SCHEMASV:
            channel(data, "Schemas validity ");
            break;
        case XML_XSD10_FROM_SCHEMASP:
            channel(data, "Schemas parser ");
            break;
        default:
            break;
    }
    switch (level) {
        case XML_XSD10_ERR_NONE:
            channel(data, ": ");
            break;
        case XML_XSD10_ERR_WARNING:
            channel(data, "warning : ");
            break;
        case XML_XSD10_ERR_ERROR:
            channel(data, "error : ");
            break;
        case XML_XSD10_ERR_FATAL:
            channel(data, "error : ");
            break;
    }
    if (message != NULL) {
        int len;
	len = xml_xsd10_strlen(message);
	if ((len > 0) && (message[len - 1] != '\n'))
	    channel(data, "%s\n", message);
	else
	    channel(data, "%s", message);
    } else {
        channel(data, "%s\n", "No error message provided");
    }
    /* libxml2 prints the context of its parser input here (xmlParserPrintFileContextInternal),
       the native errors carry it as text in ctxt when they come from a parser input. */
    if (err->ctxt != NULL) channel(data, "%s", (const char *) err->ctxt);
}

typedef struct
{
    char *buf;
} Xsd10SchemasStringChannel;

static void xsd10_schemas_string_channel(void *ctx, const char *msg, ...)
{
    Xsd10SchemasStringChannel *sc = (Xsd10SchemasStringChannel *) ctx;
    char *part = NULL;
    va_list args;

    va_start(args, msg);
    xsd10_schemas_vasprintf(&part, INT_MAX, msg, args);
    va_end(args);
    sc->buf = xml_xsd10_strcat(sc->buf, part);
    free(part);
}

char *xml_xsd10_format_error_string(const XmlXsd10Error *err)
{
    Xsd10SchemasStringChannel sc;
    sc.buf = NULL;
    xml_xsd10_format_error(err, xsd10_schemas_string_channel, &sc);
    if (sc.buf == NULL)
        sc.buf = xml_xsd10_strdup("");
    return sc.buf;
}

void xml_xsd10_raise_memory_error(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data, int domain)
{
    XmlXsd10Error err;

    memset(&err, 0, sizeof(err));
    err.domain = domain;
    err.code = XML_XSD10_ERR_NO_MEMORY;
    err.level = XML_XSD10_ERR_FATAL;

    if (schannel != NULL) {
        schannel(data, &err);
    } else if (channel != NULL) {
        channel(data, "libxml2: out of memory\n");
    }
}

int xml_xsd10_v_raise_error(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data,
                            void *ctx, XMQNode *node, int domain, int code, XmlXsd10ErrorLevel level,
                            const char *file, int line, const char *str1, const char *str2, const char *str3,
                            int int1, int col, const char *msg, va_list ap)
{
    XmlXsd10Error err;
    char *message = NULL;
    int res;

    (void) ctx;
    if (code == XML_XSD10_ERR_OK)
        return(0);

    /*
     * Find first element parent (xmlVUpdateError).
     */
    if (node != NULL) {
        int i;

        for (i = 0; i < 10; i++) {
            if ((xml_node_kind(node) == XML_DOM_ELEMENT) ||
                (xml_node_parent(node) == NULL))
                break;
            node = xml_node_parent(node);
        }
    }

    /*
     * Get file and line from node.
     */
    if (node != NULL) {
        if ((file == NULL) && (xml_node_dom(node) != NULL))
            file = xml_dom_url(xml_node_dom(node));

        if (line == 0) {
            if (xml_node_kind(node) == XML_DOM_ELEMENT)
                line = xml_node_line(node);
            if (line == 0)
                line = (int) xml_xsd10_tree_get_line_no(node);
        }
    }

    /*
     * Formatting the message (xmlVSetError)
     */
    if (msg == NULL) {
        message = xml_xsd10_strdup("No error message provided");
    } else {
        res = xsd10_schemas_vasprintf(&message, XML_XSD10_MAX_ERR_MSG_SIZE, msg, ap);
        if (res < 0)
            return(-1);
    }
    if (message == NULL)
        return(-1);

    memset(&err, 0, sizeof(err));
    err.domain = domain;
    err.code = code;
    err.message = message;
    err.level = level;
    err.file = xml_xsd10_strdup(file);
    err.line = line;
    err.str1 = xml_xsd10_strdup(str1);
    err.str2 = xml_xsd10_strdup(str2);
    err.str3 = xml_xsd10_strdup(str3);
    err.int1 = int1;
    err.int2 = col;
    err.node = node;
    /* Only the loader passes ctx for I/O errors: the context of the parser input as text
       (the schema error functions pass their schema context, which is not exposed). */
    err.ctxt = (domain == XML_XSD10_FROM_IO) ? ctx : NULL;

    if (schannel != NULL) {
	schannel(data, &err);
    } else if (channel != NULL) {
        /* Don't invoke legacy error handlers */
        if (channel == xml_xsd10_generic_error)
            xml_xsd10_format_error(&err, xml_xsd10_generic_error, NULL);
        else
	    channel(data, "%s", err.message);
    }

    free(err.message);
    free(err.file);
    free(err.str1);
    free(err.str2);
    free(err.str3);
    return(0);
}

/* Raise an error with a ... argument list (libxml2 xmlRaiseError). */
static int xsd10_schemas_raise_error_ctxt(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data,
                                          const char *input_context,
                                          XMQNode *node, int domain, int code, XmlXsd10ErrorLevel level,
                                          const char *file, int line, const char *str1, int int1, int col,
                                          const char *msg, ...)
{
    va_list ap;
    int res;

    va_start(ap, msg);
    res = xml_xsd10_v_raise_error(schannel, channel, data, (void *) input_context, node, domain, code,
                                  level, file, line, str1, NULL, NULL, int1, col, msg,
                                  ap);
    va_end(ap);

    return(res);
}

static int xsd10_schemas_raise_error(XmlXsd10StructuredErrorFunc schannel, XmlXsd10ValidityErrorFunc channel, void *data,
                                     XMQNode *node, int domain, int code, XmlXsd10ErrorLevel level,
                                     const char *file, int line, const char *str1, int int1, int col,
                                     const char *msg, ...)
{
    va_list ap;
    int res;

    va_start(ap, msg);
    res = xml_xsd10_v_raise_error(schannel, channel, data, NULL, node, domain, code,
                                  level, file, line, str1, NULL, NULL, int1, col, msg,
                                  ap);
    va_end(ap);

    return(res);
}

/************************************************************************
 *									*
 *		Regexps and loading schema documents			*
 *									*
 ************************************************************************/

static void *xsd10_schemas_regexp_compile(const char *regexp)
{
    return xml_regexp_compile(regexp);
}

static int xsd10_schemas_regexp_exec(void *regexp, const char *value)
{
    return xml_regexp_exec((XmlRegexp *) regexp, value);
}

static void xsd10_schemas_regexp_free(void *regexp)
{
    xml_regexp_free((XmlRegexp *) regexp);
}

void xml_xsd10_install_regexp_hooks(void)
{
    static const XmlXsd10RegexpHooks hooks = {
        xsd10_schemas_regexp_compile,
        xsd10_schemas_regexp_exec,
        xsd10_schemas_regexp_free
    };
    xml_xsd10_set_regexp_hooks(&hooks);
}

/* Read a whole file, NULL on errors (then errno is set). */
/* *read_failed is set when the file could be opened but not read (eg a directory):
   libxml2 reports that from the parser input instead of from the loader. */
static char *xsd10_schemas_read_file(const char *path, size_t *len, int *read_failed)
{
    *read_failed = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    size_t cap = 4096, n = 0;
    char *buf = (char *) malloc(cap);
    if (buf == NULL)
    {
        fclose(f);
        errno = ENOMEM;
        return NULL;
    }
    while (1)
    {
        if (n == cap)
        {
            char *tmp = (char *) realloc(buf, cap * 2);
            if (tmp == NULL)
            {
                free(buf);
                fclose(f);
                errno = ENOMEM;
                return NULL;
            }
            buf = tmp;
            cap *= 2;
        }
        size_t r = fread(buf + n, 1, cap - n, f);
        if (r == 0) break;
        n += r;
    }
    int failed = ferror(f);
    int e = errno;
    fclose(f);
    if (failed)
    {
        free(buf);
        *read_failed = 1;
        errno = e ? e : EIO;
        return NULL;
    }
    *len = n;
    return buf;
}

/* Where the problems of loading a schema document go: the structured callback of the
   parser context, else stderr (libxml2 reports the xml parser errors to the parser
   context of the schema document, which only gets the structured callback). */
typedef struct
{
    XmlXsd10StructuredErrorFunc serror;
    void *data;
} Xsd10SchemasLoadReport;

static void xsd10_schemas_report_parse_error(Xsd10SchemasLoadReport *rep, const char *uri, XmlParseError *perr)
{
    if (perr->message == NULL) return;
    if ((rep != NULL) && (rep->serror != NULL))
    {
        xsd10_schemas_raise_error(rep->serror, NULL, rep->data, NULL, XML_XSD10_FROM_PARSER,
                                  XML_XSD10_ERR_INTERNAL_ERROR, XML_XSD10_ERR_FATAL, uri, (int) perr->line,
                                  NULL, 0, (int) perr->col, "%s",
                                  perr->short_message ? perr->short_message : perr->message);
    }
    else if ((uri == NULL) && !strncmp(perr->message, "noname.xml:", 11))
    {
        /* libxml2 has no file name for a memory buffer: "Entity: line 1: parser error : ..." */
        fputs("Entity: line ", stderr);
        fputs(perr->message + 11, stderr);
    }
    else
    {
        fputs(perr->message, stderr);
    }
}

static void xsd10_schemas_parse_message(void *user_data, const char *message)
{
    Xsd10SchemasLoadReport *rep = (Xsd10SchemasLoadReport *) user_data;
    if ((rep != NULL) && (rep->serror != NULL))
        xsd10_schemas_raise_error(rep->serror, NULL, rep->data, NULL, XML_XSD10_FROM_PARSER,
                                  XML_XSD10_ERR_INTERNAL_ERROR, XML_XSD10_ERR_WARNING, NULL, 0,
                                  NULL, 0, 0, "%s", message);
    else
        fputs(message, stderr);
}

/* Parse the buffer like xmlCtxtReadMemory with XML_PARSE_NOENT. */
static XmlDom *xsd10_schemas_parse(const char *buf, size_t len, const char *uri, Xsd10SchemasLoadReport *rep)
{
    XmlParseSettings ps;
    XmlParseError perr;
    XmlDom *dom;

    xml_parse_settings_default(&ps);
    ps.source_name = uri;
    ps.message_callback = xsd10_schemas_parse_message;
    ps.message_user_data = rep;
    memset(&perr, 0, sizeof(perr));
    dom = xml_parse_buffer(buf, buf + len, &ps, &perr);
    if (dom == NULL)
        xsd10_schemas_report_parse_error(rep, uri, &perr);
    xml_parse_error_free(&perr);
    if (dom != NULL)
    {
        /* XML_PARSE_NOENT: entities are substituted. */
        memset(&perr, 0, sizeof(perr));
        if (!xml_expand_entities(dom, NULL, NULL, len, &ps, &perr))
        {
            xsd10_schemas_report_parse_error(rep, uri, &perr);
            xml_dom_free(dom);
            dom = NULL;
        }
        xml_parse_error_free(&perr);
    }
    return dom;
}

/* Port of xmlIOErr (errno to the libxml2 XML_IO_* codes, which start at 1501 for EACCES)
   and the messages of xmlErrString. */
typedef struct { int err; int code; const char *msg; } Xsd10SchemasIoErr;
static const Xsd10SchemasIoErr xsd10_schemas_io_errs_[] = {
#ifdef EACCES
    { EACCES, 1501, "Permission denied" },
#endif
#ifdef EAGAIN
    { EAGAIN, 1502, "Resource temporarily unavailable" },
#endif
#ifdef EBADF
    { EBADF, 1503, "Bad file descriptor" },
#endif
#ifdef EBADMSG
    { EBADMSG, 1504, "Bad message" },
#endif
#ifdef EBUSY
    { EBUSY, 1505, "Resource busy" },
#endif
#ifdef ECANCELED
    { ECANCELED, 1506, "Operation canceled" },
#endif
#ifdef ECHILD
    { ECHILD, 1507, "No child processes" },
#endif
#ifdef EDEADLK
    { EDEADLK, 1508, "Resource deadlock avoided" },
#endif
#ifdef EDOM
    { EDOM, 1509, "Domain error" },
#endif
#ifdef EEXIST
    { EEXIST, 1510, "File exists" },
#endif
#ifdef EFAULT
    { EFAULT, 1511, "Bad address" },
#endif
#ifdef EFBIG
    { EFBIG, 1512, "File too large" },
#endif
#ifdef EINPROGRESS
    { EINPROGRESS, 1513, "Operation in progress" },
#endif
#ifdef EINTR
    { EINTR, 1514, "Interrupted function call" },
#endif
#ifdef EINVAL
    { EINVAL, 1515, "Invalid argument" },
#endif
#ifdef EIO
    { EIO, 1516, "Input/output error" },
#endif
#ifdef EISDIR
    { EISDIR, 1517, "Is a directory" },
#endif
#ifdef EMFILE
    { EMFILE, 1518, "Too many open files" },
#endif
#ifdef EMLINK
    { EMLINK, 1519, "Too many links" },
#endif
#ifdef EMSGSIZE
    { EMSGSIZE, 1520, "Inappropriate message buffer length" },
#endif
#ifdef ENAMETOOLONG
    { ENAMETOOLONG, 1521, "Filename too long" },
#endif
#ifdef ENFILE
    { ENFILE, 1522, "Too many open files in system" },
#endif
#ifdef ENODEV
    { ENODEV, 1523, "No such device" },
#endif
#ifdef ENOENT
    { ENOENT, 1524, "No such file or directory" },
#endif
#ifdef ENOEXEC
    { ENOEXEC, 1525, "Exec format error" },
#endif
#ifdef ENOLCK
    { ENOLCK, 1526, "No locks available" },
#endif
#ifdef ENOMEM
    { ENOMEM, 1527, "Not enough space" },
#endif
#ifdef ENOSPC
    { ENOSPC, 1528, "No space left on device" },
#endif
#ifdef ENOSYS
    { ENOSYS, 1529, "Function not implemented" },
#endif
#ifdef ENOTDIR
    { ENOTDIR, 1530, "Not a directory" },
#endif
#ifdef ENOTEMPTY
    { ENOTEMPTY, 1531, "Directory not empty" },
#endif
#ifdef ENOTSUP
    { ENOTSUP, 1532, "Not supported" },
#endif
#ifdef ENOTTY
    { ENOTTY, 1533, "Inappropriate I/O control operation" },
#endif
#ifdef ENXIO
    { ENXIO, 1534, "No such device or address" },
#endif
#ifdef EPERM
    { EPERM, 1535, "Operation not permitted" },
#endif
#ifdef EPIPE
    { EPIPE, 1536, "Broken pipe" },
#endif
#ifdef ERANGE
    { ERANGE, 1537, "Result too large" },
#endif
#ifdef EROFS
    { EROFS, 1538, "Read-only file system" },
#endif
#ifdef ESPIPE
    { ESPIPE, 1539, "Invalid seek" },
#endif
#ifdef ESRCH
    { ESRCH, 1540, "No such process" },
#endif
#ifdef ETIMEDOUT
    { ETIMEDOUT, 1541, "Operation timed out" },
#endif
#ifdef EXDEV
    { EXDEV, 1542, "Improper link" },
#endif
};

/* Load uri like xmlCtxtReadFile with XML_PARSE_NOENT. *notLocated is 1 if the file could not be read. */
static XmlDom *xsd10_schemas_load_file(const char *uri, Xsd10SchemasLoadReport *rep, int *notLocated)
{
    const char *path = uri;
    size_t len = 0;
    char *buf;
    XmlDom *dom;

    if (notLocated != NULL) *notLocated = 0;
    if (uri == NULL) return NULL;
    if (!strncmp(path, "file://", 7))
    {
        path += 7;
        if (!strncmp(path, "localhost/", 10)) path += 9;
    }
    int read_failed = 0;
    buf = xsd10_schemas_read_file(path, &len, &read_failed);
    if (buf == NULL)
    {
        /* Port of xmlIOErr and xmlCtxtErrIO: only a warning if the file could not be found
           (or the error is unknown). */
        int e = errno;
        int code = XML_XSD10_IO_UNKNOWN;
        XmlXsd10ErrorLevel level;
        const char *errstr = "Unknown IO error";
        for (size_t k = 0; k < sizeof(xsd10_schemas_io_errs_) / sizeof(xsd10_schemas_io_errs_[0]); k++)
        {
            if (xsd10_schemas_io_errs_[k].err == e)
            {
                code = xsd10_schemas_io_errs_[k].code;
                errstr = xsd10_schemas_io_errs_[k].msg;
                break;
            }
        }
        if (code == XML_XSD10_IO_ENOENT || code == XML_XSD10_IO_UNKNOWN) level = XML_XSD10_ERR_WARNING;
        else level = XML_XSD10_ERR_FATAL;
        if (read_failed)
        {
            /* The file was opened, reading it failed in the parser input: libxml2 reports the
               error without the uri (msg "%s\n"), at line 1 column 1 of the input. */
            /* The context of the (empty) input: an empty line and the pointer. */
            if ((rep != NULL) && (rep->serror != NULL))
                xsd10_schemas_raise_error_ctxt(rep->serror, NULL, rep->data, "\n^\n", NULL, XML_XSD10_FROM_IO, code, level,
                                               uri, 1, NULL, 0, 1, "%s\n", errstr);
            else
                xsd10_schemas_raise_error_ctxt(NULL, xml_xsd10_generic_error, NULL, "\n^\n", NULL, XML_XSD10_FROM_IO, code, level,
                                               uri, 1, NULL, 0, 1, "%s\n", errstr);
            if (notLocated != NULL) *notLocated = 1;
            return NULL;
        }
        if ((rep != NULL) && (rep->serror != NULL))
            xsd10_schemas_raise_error(rep->serror, NULL, rep->data, NULL, XML_XSD10_FROM_IO, code, level,
                                      NULL, 0, uri, 0, 0, "failed to load \"%s\": %s\n", uri, errstr);
        else
            xsd10_schemas_raise_error(NULL, xml_xsd10_generic_error, NULL, NULL, XML_XSD10_FROM_IO, code, level,
                                      NULL, 0, uri, 0, 0, "failed to load \"%s\": %s\n", uri, errstr);
        if (notLocated != NULL) *notLocated = 1;
        return NULL;
    }
    dom = xsd10_schemas_parse(buf, len, uri, rep);
    free(buf);
    if (dom != NULL)
        xml_dom_set_url(dom, uri);
    return dom;
}

XmlDom *xml_xsd10_default_loader(void *user_data, const char *uri)
{
    (void) user_data;
    return xsd10_schemas_load_file(uri, NULL, NULL);
}

XmlDom *xml_xsd10_p_read_file(XmlXsd10ParserCtxt *pctxt, const char *url, int *notLocated)
{
    Xsd10SchemasLoadReport rep;
    XmlDom *dom;

    if (notLocated != NULL) *notLocated = 0;
    if (url == NULL) return NULL;
    if ((pctxt != NULL) && (pctxt->resourceLoader != NULL))
    {
        dom = pctxt->resourceLoader(pctxt->resourceCtxt, url);
        /* A custom loader cannot tell why it failed, assume the resource was not found. */
        if ((dom == NULL) && (notLocated != NULL)) *notLocated = 1;
        if ((dom != NULL) && (xml_dom_url(dom) == NULL)) xml_dom_set_url(dom, url);
        return dom;
    }
    rep.serror = pctxt ? pctxt->serror : NULL;
    rep.data = pctxt ? pctxt->errCtxt : NULL;
    return xsd10_schemas_load_file(url, &rep, notLocated);
}

XmlDom *xml_xsd10_p_read_memory(XmlXsd10ParserCtxt *pctxt, const char *buffer, int size, int *notLocated)
{
    Xsd10SchemasLoadReport rep;
    XmlDom *dom;

    if (notLocated != NULL) *notLocated = 0;
    if ((buffer == NULL) || (size < 0)) return NULL;
    rep.serror = pctxt ? pctxt->serror : NULL;
    rep.data = pctxt ? pctxt->errCtxt : NULL;
    dom = xsd10_schemas_parse(buffer, (size_t) size, NULL, &rep);
    if (dom != NULL)
        xml_dom_set_url(dom, "in_memory_buffer");
    return dom;
}

/* The namespace uri of an element or attribute, NULL if none (libxml2 node->ns->href when node->ns != NULL). */
static const char *xsd10_schemas_ns_href(XMQNode *node)
{
    return xml_node_ns_uri(node);
}

/************************************************************************
 *									*
 *			Helper functions			        *
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaItemTypeToStr.
 *
 * @param type  the type of the schema item
 * @returns the component name of a schema item.
 */
const char *xml_xsd10_item_type_to_str(XmlXsd10TypeType type)
{
    switch (type) {
	case XML_XSD10_SCHEMA_TYPE_BASIC:
	    return("simple type definition");
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	    return("simple type definition");
	case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    return("complex type definition");
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    return("element declaration");
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
	    return("attribute use");
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
	    return("attribute declaration");
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return("model group definition");
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    return("attribute group definition");
	case XML_XSD10_SCHEMA_TYPE_NOTATION:
	    return("notation declaration");
	case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	    return("model group (sequence)");
	case XML_XSD10_SCHEMA_TYPE_CHOICE:
	    return("model group (choice)");
	case XML_XSD10_SCHEMA_TYPE_ALL:
	    return("model group (all)");
	case XML_XSD10_SCHEMA_TYPE_PARTICLE:
	    return("particle");
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	    return("unique identity-constraint");
	    /* return("IDC (unique)"); */
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	    return("key identity-constraint");
	    /* return("IDC (key)"); */
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	    return("keyref identity-constraint");
	    /* return("IDC (keyref)"); */
	case XML_XSD10_SCHEMA_TYPE_ANY:
	    return("wildcard (any)");
	case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
	    return("[helper component] QName reference");
	case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
	    return("[helper component] attribute use prohibition");
	default:
	    return("Not a schema component");
    }
}

/**
 * Port of xmlSchemaGetComponentTypeStr.
 *
 * @param item  the type of the schema item
 * @returns the component name of a schema item.
 */
const char *xml_xsd10_get_component_type_str(XmlXsd10BasicItem *item)
{
    switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_BASIC:
	    if (XSD10_WXS_IS_COMPLEX(XSD10_WXS_TYPE_CAST item))
		return("complex type definition");
	    else
		return("simple type definition");
	default:
	    return(xml_xsd10_item_type_to_str(item->type));
    }
}

/**
 * Port of xmlSchemaGetComponentNode.
 *
 * @param item  a schema component
 * @returns node associated with the schema component.
 * NOTE that such a node need not be available; plus, a component's
 * node need not to reflect the component directly, since there is no
 * one-to-one relationship between the XML Schema representation and
 * the component representation.
 */
XMQNode *xml_xsd10_get_component_node(XmlXsd10BasicItem *item)
{
    switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    return (((XmlXsd10Element *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
	    return (((XmlXsd10Attribute *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	    return (((XmlXsd10Type *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_ANY:
	case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
	    return (((XmlXsd10Wildcard *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_PARTICLE:
	    return (((XmlXsd10Particle *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	case XML_XSD10_SCHEMA_TYPE_CHOICE:
	case XML_XSD10_SCHEMA_TYPE_ALL:
	    return (((XmlXsd10ModelGroup *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return (((XmlXsd10ModelGroupDef *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    return (((XmlXsd10AttributeGroup *) item)->node);
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	    return (((XmlXsd10IDC *) item)->node);
	case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
	    return(((XmlXsd10QNameRef *) item)->node);
	/* TODO: What to do with NOTATIONs?
	case XML_XSD10_SCHEMA_TYPE_NOTATION:
	    return (((XmlXsd10Notation *) item)->node);
	*/
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
	    return (((XmlXsd10AttributeUse *) item)->node);
	default:
	    return (NULL);
    }
}

/**
 * Port of xmlSchemaFormatQName.
 *
 * Returns the given QName in the format "{namespaceName}localName" or
 * just "localName" if `namespaceName` is NULL.
 *
 * @param buf  the string buffer
 * @param namespaceName  the namespace name
 * @param localName  the local name
 * @returns the localName if `namespaceName` is NULL, a formatted
 * string otherwise.
 */
const char *xml_xsd10_format_q_name(char **buf, const char *namespaceName, const char *localName)
{
    XSD10_FREE_AND_NULL(*buf)
    if (namespaceName != NULL) {
	*buf = xml_xsd10_strdup("{");
	*buf = xml_xsd10_strcat(*buf, namespaceName);
	*buf = xml_xsd10_strcat(*buf, "}");
    }
    if (localName != NULL) {
	if (namespaceName == NULL)
	    return(localName);
	*buf = xml_xsd10_strcat(*buf, localName);
    } else {
	*buf = xml_xsd10_strcat(*buf, "(NULL)");
    }
    return ((const char *) *buf);
}

/** Port of xmlSchemaFormatQNameNs. */
const char *xml_xsd10_format_q_name_ns(char **buf, XmlNs *ns, const char *localName)
{
    if (ns != NULL)
	return (xml_xsd10_format_q_name(buf, xml_ns_href(ns), localName));
    else
	return (xml_xsd10_format_q_name(buf, NULL, localName));
}

/** Port of xmlSchemaGetComponentName. */
const char *xml_xsd10_get_component_name(XmlXsd10BasicItem *item)
{
    if (item == NULL) {
        return (NULL);
    }
    switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    return (((XmlXsd10Element *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
	    return (((XmlXsd10Attribute *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    return (((XmlXsd10AttributeGroup *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_BASIC:
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    return (((XmlXsd10Type *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return (((XmlXsd10ModelGroupDef *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	    return (((XmlXsd10IDC *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
	    if (XSD10_WXS_ATTRUSE_DECL(item) != NULL) {
		return(xml_xsd10_get_component_name(
		    XSD10_WXS_BASIC_CAST XSD10_WXS_ATTRUSE_DECL(item)));
	    } else
		return(NULL);
	case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
	    return (((XmlXsd10QNameRef *) item)->name);
	case XML_XSD10_SCHEMA_TYPE_NOTATION:
	    return (((XmlXsd10Notation *) item)->name);
	default:
	    /*
	    * Other components cannot have names.
	    */
	    break;
    }
    return (NULL);
}

/** Port of xmlSchemaGetComponentTargetNs. */
const char *xml_xsd10_get_component_target_ns(XmlXsd10BasicItem *item)
{
    if (item == NULL) {
        return (NULL);
    }
    switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    return (((XmlXsd10Element *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
	    return (((XmlXsd10Attribute *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    return (((XmlXsd10AttributeGroup *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_BASIC:
	    return ("http://www.w3.org/2001/XMLSchema");
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	    return (((XmlXsd10Type *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return (((XmlXsd10ModelGroupDef *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	    return (((XmlXsd10IDC *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
	    if (XSD10_WXS_ATTRUSE_DECL(item) != NULL) {
		return(xml_xsd10_get_component_target_ns(
		    XSD10_WXS_BASIC_CAST XSD10_WXS_ATTRUSE_DECL(item)));
	    }
	    /* TODO: Will returning NULL break something? */
	    break;
	case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
	    return (((XmlXsd10QNameRef *) item)->targetNamespace);
	case XML_XSD10_SCHEMA_TYPE_NOTATION:
	    return (((XmlXsd10Notation *) item)->targetNamespace);
	default:
	    /*
	    * Other components cannot have names.
	    */
	    break;
    }
    return (NULL);
}

/** Port of xmlSchemaGetComponentQName. */
const char *xml_xsd10_get_component_q_name(char **buf, void *item)
{
    return (xml_xsd10_format_q_name(buf,
	xml_xsd10_get_component_target_ns((XmlXsd10BasicItem *) item),
	xml_xsd10_get_component_name((XmlXsd10BasicItem *) item)));
}

/** Port of xmlSchemaGetComponentDesignation. */
const char *xml_xsd10_get_component_designation(char **buf, void *item)
{
    char *str = NULL;

    *buf = xml_xsd10_strcat(*buf, XSD10_WXS_ITEM_TYPE_NAME(item));
    *buf = xml_xsd10_strcat(*buf, " '");
    *buf = xml_xsd10_strcat(*buf, xml_xsd10_get_component_q_name(&str,
	(XmlXsd10BasicItem *) item));
    *buf = xml_xsd10_strcat(*buf, "'");
    XSD10_FREE_AND_NULL(str);
    return(*buf);
}

/** Port of xmlSchemaGetIDCDesignation. */
const char *xml_xsd10_get_idc_designation(char **buf, XmlXsd10IDC *idc)
{
    return(xml_xsd10_get_component_designation(buf, idc));
}

/**
 * Port of xmlSchemaWildcardPCToString.
 *
 * @param pc  the type of processContents
 * @returns a string representation of the type of
 * processContents.
 */
const char *xml_xsd10_wildcard_pc_to_string(int pc)
{
    switch (pc) {
	case XML_XSD10_SCHEMAS_ANY_SKIP:
	    return ("skip");
	case XML_XSD10_SCHEMAS_ANY_LAX:
	    return ("lax");
	case XML_XSD10_SCHEMAS_ANY_STRICT:
	    return ("strict");
	default:
	    return ("invalid process contents");
    }
}

/**
 * Port of xmlSchemaGetCanonValueWhtspExt_1.
 *
 * Get a the canonical representation of the value.
 * The caller has to free the returned retValue.
 *
 * @param val  the precomputed value
 * @param retValue  the returned value
 * @param ws  the whitespace type of the value
 * @param for_hash  non-zero if this is supposed to generate a string for hashing
 * @returns 0 if the value could be built and -1 in case of
 *         API errors or if the value type is not supported yet.
 */
int xml_xsd10_get_canon_value_whtsp_ext_1(XmlXsd10Val *val, XmlXsd10WhitespaceValueType ws, char **retValue, int for_hash)
{
    int list;
    XmlXsd10ValType valType;
    const char *value, *value2 = NULL;


    if ((retValue == NULL) || (val == NULL))
	return (-1);
    list = xml_xsd10_value_get_next(val) ? 1 : 0;
    *retValue = NULL;
    do {
	value = NULL;
	valType = xml_xsd10_get_val_type(val);
	switch (valType) {
	    case XML_XSD10_SCHEMAS_STRING:
	    case XML_XSD10_SCHEMAS_NORMSTRING:
	    case XML_XSD10_SCHEMAS_ANYSIMPLETYPE:
		value = xml_xsd10_value_get_as_string(val);
		if (value != NULL) {
		    if (ws == XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE)
			value2 = xml_xsd10_collapse_string(value);
		    else if (ws == XML_XSD10_SCHEMA_WHITESPACE_REPLACE)
			value2 = xml_xsd10_white_space_replace(value);
		    if (value2 != NULL)
			value = value2;
		}
		break;
	    default:
		if (xml_xsd10_get_canon_value(val, &value2) == -1) {
		    if (value2 != NULL)
			free((char *) value2);
		    goto internal_error;
		}
		if (for_hash && valType == XML_XSD10_SCHEMAS_DECIMAL) {
		    /* We can mostly use the canonical value for hashing,
		       except in the case of decimal.  There the canonical
		       representation requires a trailing '.0' even for
		       non-fractional numbers, but for the derived integer
		       types it forbids any decimal point.  Nevertheless they
		       compare equal if the value is equal.  We need to generate
		       the same hash value for this to work, and it's easiest
		       to just cut off the useless '.0' suffix for the
		       decimal type.  */
		    int len = xml_xsd10_strlen(value2);
		    if (len > 2 && value2[len-1] == '0' && value2[len-2] == '.')
		      ((char*)value2)[len-2] = 0;
		}
		value = value2;
	}
	if (*retValue == NULL)
	    if (value == NULL) {
		if (! list)
		    *retValue = xml_xsd10_strdup("");
	    } else
		*retValue = xml_xsd10_strdup(value);
	else if (value != NULL) {
	    /* List. */
	    *retValue = xml_xsd10_strcat((char *) *retValue, " ");
	    *retValue = xml_xsd10_strcat((char *) *retValue, value);
	}
	XSD10_FREE_AND_NULL(value2)
	val = xml_xsd10_value_get_next(val);
    } while (val != NULL);

    return (0);
internal_error:
    if (*retValue != NULL)
	free((char *) (*retValue));
    if (value2 != NULL)
	free((char *) value2);
    return (-1);
}

/** Port of xmlSchemaGetCanonValueWhtspExt. */
int xml_xsd10_get_canon_value_whtsp_ext(XmlXsd10Val *val, XmlXsd10WhitespaceValueType ws, char **retValue)
{
    return xml_xsd10_get_canon_value_whtsp_ext_1(val, ws, retValue, 0);
}

/** Port of xmlSchemaGetCanonValueHash. */
int xml_xsd10_get_canon_value_hash(XmlXsd10Val *val, char **retValue)
{
    return xml_xsd10_get_canon_value_whtsp_ext_1(val, XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE,
					    retValue, 1);
}

/**
 * Port of xmlSchemaFormatItemForReport.
 *
 * The following order is used to build the resulting
 * designation if the arguments are not NULL:
 * 1a. If itemDes not NULL -> itemDes
 * 1b. If (itemDes not NULL) and (itemName not NULL)
 *     -> itemDes + itemName
 * 2. If the preceding was NULL and (item not NULL) -> item
 * 3. If the preceding was NULL and (itemNode not NULL) -> itemNode
 *
 * If the itemNode is an attribute node, the name of the attribute
 * will be appended to the result.
 *
 * Returns a representation of the given item used
 * for error reports.
 *
 * @param buf  the string buffer
 * @param itemDes  the designation of the item
 * @param item  the item as an object
 * @param itemNode  the node of the item
 * @returns the formatted string and sets `buf` to the resulting value.
 */
char *xml_xsd10_format_item_for_report(char **buf, const char *itemDes, XmlXsd10BasicItem *item, XMQNode *itemNode)
{
    char *str = NULL;
    int named = 1;

    if (*buf != NULL) {
	free(*buf);
	*buf = NULL;
    }

    if (itemDes != NULL) {
	*buf = xml_xsd10_strdup(itemDes);
    } else if (item != NULL) {
	switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_BASIC: {
	    XmlXsd10Type *type = XSD10_WXS_TYPE_CAST item;

	    if (XSD10_WXS_IS_ATOMIC(type))
		*buf = xml_xsd10_strdup("atomic type 'xs:");
	    else if (XSD10_WXS_IS_LIST(type))
		*buf = xml_xsd10_strdup("list type 'xs:");
	    else if (XSD10_WXS_IS_UNION(type))
		*buf = xml_xsd10_strdup("union type 'xs:");
	    else
		*buf = xml_xsd10_strdup("simple type 'xs:");
	    *buf = xml_xsd10_strcat(*buf, type->name);
	    *buf = xml_xsd10_strcat(*buf, "'");
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_SIMPLE: {
	    XmlXsd10Type *type = XSD10_WXS_TYPE_CAST item;

	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) {
		*buf = xml_xsd10_strdup("");
	    } else {
		*buf = xml_xsd10_strdup("local ");
	    }
	    if (XSD10_WXS_IS_ATOMIC(type))
		*buf = xml_xsd10_strcat(*buf, "atomic type");
	    else if (XSD10_WXS_IS_LIST(type))
		*buf = xml_xsd10_strcat(*buf, "list type");
	    else if (XSD10_WXS_IS_UNION(type))
		*buf = xml_xsd10_strcat(*buf, "union type");
	    else
		*buf = xml_xsd10_strcat(*buf, "simple type");
	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) {
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, type->name);
		*buf = xml_xsd10_strcat(*buf, "'");
	    }
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_COMPLEX: {
	    XmlXsd10Type *type = XSD10_WXS_TYPE_CAST item;

	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL)
		*buf = xml_xsd10_strdup("");
	    else
		*buf = xml_xsd10_strdup("local ");
	    *buf = xml_xsd10_strcat(*buf, "complex type");
	    if (type->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL) {
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, type->name);
		*buf = xml_xsd10_strcat(*buf, "'");
	    }
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE: {
		XmlXsd10AttributeUse *ause;

		ause = XSD10_WXS_ATTR_USE_CAST item;
		*buf = xml_xsd10_strdup("attribute use ");
		if (XSD10_WXS_ATTRUSE_DECL(ause) != NULL) {
		    *buf = xml_xsd10_strcat(*buf, "'");
		    *buf = xml_xsd10_strcat(*buf,
			xml_xsd10_get_component_q_name(&str, XSD10_WXS_ATTRUSE_DECL(ause)));
		    XSD10_FREE_AND_NULL(str)
			*buf = xml_xsd10_strcat(*buf, "'");
		} else {
		    *buf = xml_xsd10_strcat(*buf, "(unknown)");
		}
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE: {
		XmlXsd10Attribute *attr;

		attr = (XmlXsd10Attribute *) item;
		*buf = xml_xsd10_strdup("attribute decl.");
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, xml_xsd10_format_q_name(&str,
		    attr->targetNamespace, attr->name));
		XSD10_FREE_AND_NULL(str)
		    *buf = xml_xsd10_strcat(*buf, "'");
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
	    xml_xsd10_get_component_designation(buf, item);
	    break;
	case XML_XSD10_SCHEMA_TYPE_ELEMENT: {
		XmlXsd10Element *elem;

		elem = (XmlXsd10Element *) item;
		*buf = xml_xsd10_strdup("element decl.");
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, xml_xsd10_format_q_name(&str,
		    elem->targetNamespace, elem->name));
		*buf = xml_xsd10_strcat(*buf, "'");
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
	case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
	    if (item->type == XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE)
		*buf = xml_xsd10_strdup("unique '");
	    else if (item->type == XML_XSD10_SCHEMA_TYPE_IDC_KEY)
		*buf = xml_xsd10_strdup("key '");
	    else
		*buf = xml_xsd10_strdup("keyRef '");
	    *buf = xml_xsd10_strcat(*buf, ((XmlXsd10IDC *) item)->name);
	    *buf = xml_xsd10_strcat(*buf, "'");
	    break;
	case XML_XSD10_SCHEMA_TYPE_ANY:
	case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
	    *buf = xml_xsd10_strdup(xml_xsd10_wildcard_pc_to_string(
		    ((XmlXsd10Wildcard *) item)->processContents));
	    *buf = xml_xsd10_strcat(*buf, " wildcard");
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
	case XML_XSD10_SCHEMA_FACET_MINLENGTH:
	    *buf = xml_xsd10_strdup("facet '");
	    *buf = xml_xsd10_strcat(*buf, xml_xsd10_facet_type_to_string(item->type));
	    *buf = xml_xsd10_strcat(*buf, "'");
	    break;
	case XML_XSD10_SCHEMA_TYPE_GROUP: {
		*buf = xml_xsd10_strdup("model group def.");
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, xml_xsd10_get_component_q_name(&str, item));
		*buf = xml_xsd10_strcat(*buf, "'");
		XSD10_FREE_AND_NULL(str)
	    }
	    break;
	case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
	case XML_XSD10_SCHEMA_TYPE_CHOICE:
	case XML_XSD10_SCHEMA_TYPE_ALL:
	case XML_XSD10_SCHEMA_TYPE_PARTICLE:
	    *buf = xml_xsd10_strdup(XSD10_WXS_ITEM_TYPE_NAME(item));
	    break;
	case XML_XSD10_SCHEMA_TYPE_NOTATION: {
		*buf = xml_xsd10_strdup(XSD10_WXS_ITEM_TYPE_NAME(item));
		*buf = xml_xsd10_strcat(*buf, " '");
		*buf = xml_xsd10_strcat(*buf, xml_xsd10_get_component_q_name(&str, item));
		*buf = xml_xsd10_strcat(*buf, "'");
		XSD10_FREE_AND_NULL(str);
	    }
            /* Falls through. */
	default:
	    named = 0;
	}
    } else
	named = 0;

    if ((named == 0) && (itemNode != NULL)) {
	XMQNode *elem;

	if (xml_node_kind(itemNode) == XML_DOM_ATTRIBUTE)
	    elem = xml_node_parent(itemNode);
	else
	    elem = itemNode;
	/* libxml2 overwrites (leaks) a notation designation here, the output is the same. */
	XSD10_FREE_AND_NULL(*buf)
	*buf = xml_xsd10_strdup("Element '");
	if (xsd10_schemas_ns_href(elem) != NULL) {
	    *buf = xml_xsd10_strcat(*buf,
		xml_xsd10_format_q_name(&str, xsd10_schemas_ns_href(elem), xml_node_name(elem)));
	    XSD10_FREE_AND_NULL(str)
	} else
	    *buf = xml_xsd10_strcat(*buf, xml_node_name(elem));
	*buf = xml_xsd10_strcat(*buf, "'");

    }
    if ((itemNode != NULL) && (xml_node_kind(itemNode) == XML_DOM_ATTRIBUTE)) {
	*buf = xml_xsd10_strcat(*buf, ", attribute '");
	if (xsd10_schemas_ns_href(itemNode) != NULL) {
	    *buf = xml_xsd10_strcat(*buf, xml_xsd10_format_q_name(&str,
		xsd10_schemas_ns_href(itemNode), xml_node_name(itemNode)));
	    XSD10_FREE_AND_NULL(str)
	} else
	    *buf = xml_xsd10_strcat(*buf, xml_node_name(itemNode));
	*buf = xml_xsd10_strcat(*buf, "'");
    }
    XSD10_FREE_AND_NULL(str)

    return (xml_xsd10_escape_format_string(buf));
}

/**
 * Port of xmlSchemaFormatFacetEnumSet.
 *
 * Builds a string consisting of all enumeration elements.
 *
 * @param actxt  schema context
 * @param buf  the string buffer
 * @param type  the type holding the enumeration facets
 * @returns a string of all enumeration elements.
 */
const char *xml_xsd10_format_facet_enum_set(XmlXsd10AbstractCtxt *actxt, char **buf, XmlXsd10Type *type)
{
    XmlXsd10Facet *facet;
    XmlXsd10WhitespaceValueType ws;
    char *value = NULL;
    int res, found = 0;

    if (*buf != NULL)
	free(*buf);
    *buf = NULL;

    do {
	/*
	* Use the whitespace type of the base type.
	*/
	ws = xml_xsd10_get_white_space_facet_value(type->baseType);
	for (facet = type->facets; facet != NULL; facet = facet->next) {
	    if (facet->type != XML_XSD10_SCHEMA_FACET_ENUMERATION)
		continue;
	    found = 1;
	    res = xml_xsd10_get_canon_value_whtsp_ext(facet->val,
		ws, &value);
	    if (res == -1) {
		xml_xsd10_internal_err(actxt,
		    "xmlSchemaFormatFacetEnumSet",
		    "compute the canonical lexical representation");
		if (*buf != NULL)
		    free(*buf);
		*buf = NULL;
		return (NULL);
	    }
	    if (*buf == NULL)
		*buf = xml_xsd10_strdup("'");
	    else
		*buf = xml_xsd10_strcat(*buf, ", '");
	    *buf = xml_xsd10_strcat(*buf, value);
	    *buf = xml_xsd10_strcat(*buf, "'");
	    if (value != NULL) {
		free((char *)value);
		value = NULL;
	    }
	}
	/*
	* The enumeration facet of a type restricts the enumeration
	* facet of the ancestor type; i.e., such restricted enumerations
	* do not belong to the set of the given type. Thus we break
	* on the first found enumeration.
	*/
	if (found)
	    break;
	type = type->baseType;
    } while ((type != NULL) && (type->type != XML_XSD10_SCHEMA_TYPE_BASIC));

    return ((const char *) *buf);
}

/************************************************************************
 *									*
 *			Error functions				        *
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaPErrMemory: handle an out of memory condition
 *
 * @param ctxt  schema parser context
 */
void xml_xsd10_p_err_memory(XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10ValidityErrorFunc channel = NULL;
    XmlXsd10StructuredErrorFunc schannel = NULL;
    void *data = NULL;

    if (ctxt != NULL) {
        ctxt->nberrors++;
        ctxt->err = XML_XSD10_ERR_NO_MEMORY;
        channel = ctxt->error;
        schannel = ctxt->serror;
        data = ctxt->errCtxt;
    }

    xml_xsd10_raise_memory_error(schannel, channel, data, XML_XSD10_FROM_SCHEMASP);
}

/** Port of xmlSchemaPErrFull. */
void xml_xsd10_p_err_full(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, XmlXsd10ErrorLevel level, const char *file, int line, const char *str1, const char *str2, const char *str3, int col, const char *msg, ...)
{
    XmlXsd10ValidityErrorFunc channel = NULL;
    XmlXsd10StructuredErrorFunc schannel = NULL;
    void *data = NULL;
    int res;
    va_list ap;

    if (ctxt != NULL) {
        /* Don't overwrite memory errors */
        if (ctxt->err == XML_XSD10_ERR_NO_MEMORY)
            return;

        if (level == XML_XSD10_ERR_WARNING) {
            channel = ctxt->warning;
        } else {
            ctxt->nberrors++;
            ctxt->err = code;
            channel = ctxt->error;
        }
        data = ctxt->errCtxt;
        schannel = ctxt->serror;
    }

    if ((channel == NULL) && (schannel == NULL)) {
        channel = xml_xsd10_generic_error;
        data = NULL;
    }

    va_start(ap, msg);
    res = xml_xsd10_v_raise_error(schannel, channel, data, ctxt, node,
                         XML_XSD10_FROM_SCHEMASP, code, level, file, line,
                         (const char *) str1,
                         (const char *) str2,
                         (const char *) str3,
                         0, col, msg, ap);
    va_end(ap);

    if (res < 0)
        xml_xsd10_p_err_memory(ctxt);
}

/**
 * Port of xmlSchemaPErr: handle a parser error
 *
 * @param ctxt  the parsing context
 * @param node  the context node
 * @param code  the error code
 * @param msg  the error message
 * @param str1  extra data
 * @param str2  extra data
 */
void xml_xsd10_p_err(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, const char *msg, const char *str1, const char *str2)
{
    xml_xsd10_p_err_full(ctxt, node, code, XML_XSD10_ERR_ERROR, NULL, 0,
                      str1, str2, NULL, 0, msg, str1, str2);
}

/**
 * Port of xmlSchemaPErr2: handle a parser error
 *
 * @param ctxt  the parsing context
 * @param node  the context node
 * @param child  the current child
 * @param error  the error code
 * @param msg  the error message
 * @param str1  extra data
 * @param str2  extra data
 */
void xml_xsd10_p_err2(XmlXsd10ParserCtxt *ctxt, XMQNode *node, XMQNode *child, int error, const char *msg, const char *str1, const char *str2)
{
    if (child != NULL)
        xml_xsd10_p_err(ctxt, child, error, msg, str1, str2);
    else
        xml_xsd10_p_err(ctxt, node, error, msg, str1, str2);
}


/**
 * Port of xmlSchemaPErrExt: handle a parser error
 *
 * @param ctxt  the parsing context
 * @param node  the context node
 * @param code  the error code
 * @param strData1  extra data
 * @param strData2  extra data
 * @param strData3  extra data
 * @param msg  the message
 * @param str1  extra parameter for the message display
 * @param str2  extra parameter for the message display
 * @param str3  extra parameter for the message display
 * @param str4  extra parameter for the message display
 * @param str5  extra parameter for the message display
 */
void xml_xsd10_p_err_ext(XmlXsd10ParserCtxt *ctxt, XMQNode *node, int code, const char *strData1, const char *strData2, const char *strData3, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4, const char *str5)
{
    xml_xsd10_p_err_full(ctxt, node, code, XML_XSD10_ERR_ERROR, NULL, 0,
                      strData1, strData2, strData3, 0,
                      msg, str1, str2, str3, str4, str5);
}

/************************************************************************
 *									*
 *			Allround error functions			*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaVErrMemory: handle an out of memory condition
 *
 * @param ctxt  schema validation context
 */
void xml_xsd10_v_err_memory(XmlXsd10ValidCtxt *ctxt)
{
    XmlXsd10ValidityErrorFunc channel = NULL;
    XmlXsd10StructuredErrorFunc schannel = NULL;
    void *data = NULL;

    if (ctxt != NULL) {
        ctxt->nberrors++;
        ctxt->err = XML_XSD10_ERR_NO_MEMORY;
        channel = ctxt->error;
        schannel = ctxt->serror;
        data = ctxt->errCtxt;
    }

    xml_xsd10_raise_memory_error(schannel, channel, data, XML_XSD10_FROM_SCHEMASV);
}

/** Port of xmlSchemaVErrFull. */
void xml_xsd10_v_err_full(XmlXsd10ValidCtxt *ctxt, XMQNode *node, int code, XmlXsd10ErrorLevel level, const char *file, int line, const char *str1, const char *str2, const char *str3, int col, const char *msg, ...)
{
    XmlXsd10ValidityErrorFunc channel = NULL;
    XmlXsd10StructuredErrorFunc schannel = NULL;
    void *data = NULL;
    int res;
    va_list ap;

    if (ctxt != NULL) {
        /* Don't overwrite memory errors */
        if (ctxt->err == XML_XSD10_ERR_NO_MEMORY)
            return;

        if (level == XML_XSD10_ERR_WARNING) {
            channel = ctxt->warning;
        } else {
            ctxt->nberrors++;
            ctxt->err = code;
            channel = ctxt->error;
        }
        data = ctxt->errCtxt;
        schannel = ctxt->serror;
    }

    if ((channel == NULL) && (schannel == NULL)) {
        channel = xml_xsd10_generic_error;
        data = NULL;
    }

    va_start(ap, msg);
    res = xml_xsd10_v_raise_error(schannel, channel, data, ctxt, node,
                         XML_XSD10_FROM_SCHEMASV, code, level, file, line,
                         (const char *) str1,
                         (const char *) str2,
                         (const char *) str3,
                         0, col, msg, ap);
    va_end(ap);

    if (res < 0)
        xml_xsd10_v_err_memory(ctxt);
}

/**
 * Port of xmlSchemaErr4Line: handle a validation error
 *
 * The streaming branches (no node tree: the position of the parser input or
 * of the locator) are not ported.
 *
 * @param ctxt  the validation context
 * @param errorLevel  the error level
 * @param code  the error code
 * @param node  the context node
 * @param line  the line number
 * @param msg  the error message
 * @param str1  extra data
 * @param str2  extra data
 * @param str3  extra data
 * @param str4  extra data
 */
void xml_xsd10_err4_line(XmlXsd10AbstractCtxt *ctxt, XmlXsd10ErrorLevel errorLevel, int code, XMQNode *node, int line, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4)
{
    if (ctxt != NULL) {
	if (ctxt->type == XML_XSD10_SCHEMA_CTXT_VALIDATOR) {
	    XmlXsd10ValidCtxt *vctxt = (XmlXsd10ValidCtxt *) ctxt;
	    const char *file = NULL;
	    int col = 0;

	    /*
	    * Error node. If we specify a line number, then
	    * do not channel any node to the error function.
	    */
	    if (line == 0) {
		if ((node == NULL) &&
		    (vctxt->depth >= 0) &&
		    (vctxt->inode != NULL)) {
		    node = vctxt->inode->node;
		}
	    } else {
		/*
		* Override the given node's (if any) position
		* and channel only the given line number.
		*/
		node = NULL;
		/*
		* Get filename.
		*/
		if (vctxt->doc != NULL)
		    file = xml_dom_url(vctxt->doc);
	    }
	    if ((file == NULL) && (vctxt->filename != NULL))
	        file = vctxt->filename;

            xml_xsd10_v_err_full(vctxt, node, code, errorLevel,
                              file, line, str1, str2, str3, col,
                              msg, str1, str2, str3, str4);
	} else if (ctxt->type == XML_XSD10_SCHEMA_CTXT_PARSER) {
	    XmlXsd10ParserCtxt *pctxt = (XmlXsd10ParserCtxt *) ctxt;

            xml_xsd10_p_err_full(pctxt, node, code, errorLevel,
                              NULL, 0, str1, str2, str3, 0,
                              msg, str1, str2, str3, str4);
	}
    }
}

/**
 * Port of xmlSchemaErr3: handle a validation error
 *
 * @param actxt  the validation context
 * @param error  the error code
 * @param node  the context node
 * @param msg  the error message
 * @param str1  extra data
 * @param str2  extra data
 * @param str3  extra data
 */
void xml_xsd10_err3(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2, const char *str3)
{
    xml_xsd10_err4_line(actxt, XML_XSD10_ERR_ERROR, error, node, 0,
	msg, str1, str2, str3, NULL);
}

/** Port of xmlSchemaErr4. */
void xml_xsd10_err4(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2, const char *str3, const char *str4)
{
    xml_xsd10_err4_line(actxt, XML_XSD10_ERR_ERROR, error, node, 0,
	msg, str1, str2, str3, str4);
}

/** Port of xmlSchemaErr. */
void xml_xsd10_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *msg, const char *str1, const char *str2)
{
    xml_xsd10_err4(actxt, error, node, msg, str1, str2, NULL, NULL);
}

/** Port of xmlSchemaFormatNodeForError. */
char *xml_xsd10_format_node_for_error(char **msg, XmlXsd10AbstractCtxt *actxt, XMQNode *node)
{
    char *str = NULL;

    *msg = NULL;
    if ((node != NULL) &&
	(xml_node_kind(node) != XML_DOM_ELEMENT) &&
	(xml_node_kind(node) != XML_DOM_ATTRIBUTE))
    {
	/*
	* Don't try to format other nodes than element and
	* attribute nodes.
	* Play safe and return an empty string.
	*/
	*msg = xml_xsd10_strdup("");
	return(*msg);
    }
    if (node != NULL) {
	/*
	* Work on tree nodes.
	*/
	if (xml_node_kind(node) == XML_DOM_ATTRIBUTE) {
	    XMQNode *elem = xml_node_parent(node);

	    *msg = xml_xsd10_strdup("Element '");
	    if (xsd10_schemas_ns_href(elem) != NULL)
		*msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
		    xsd10_schemas_ns_href(elem), xml_node_name(elem)));
	    else
		*msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
		    NULL, xml_node_name(elem)));
	    XSD10_FREE_AND_NULL(str);
	    *msg = xml_xsd10_strcat(*msg, "', ");
	    *msg = xml_xsd10_strcat(*msg, "attribute '");
	} else {
	    *msg = xml_xsd10_strdup("Element '");
	}
	if (xsd10_schemas_ns_href(node) != NULL)
	    *msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
	    xsd10_schemas_ns_href(node), xml_node_name(node)));
	else
	    *msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
	    NULL, xml_node_name(node)));
	XSD10_FREE_AND_NULL(str);
	*msg = xml_xsd10_strcat(*msg, "': ");
    } else if (actxt->type == XML_XSD10_SCHEMA_CTXT_VALIDATOR) {
	XmlXsd10ValidCtxt *vctxt = (XmlXsd10ValidCtxt *) actxt;
	/*
	* Work on node infos.
	*/
	if (vctxt->inode->nodeType == XML_DOM_ATTRIBUTE) {
	    XmlXsd10NodeInfo *ielem =
		vctxt->elemInfos[vctxt->depth];

	    *msg = xml_xsd10_strdup("Element '");
	    *msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
		ielem->nsName, ielem->localName));
	    XSD10_FREE_AND_NULL(str);
	    *msg = xml_xsd10_strcat(*msg, "', ");
	    *msg = xml_xsd10_strcat(*msg, "attribute '");
	} else {
	    *msg = xml_xsd10_strdup("Element '");
	}
	*msg = xml_xsd10_strcat(*msg, xml_xsd10_format_q_name(&str,
	    vctxt->inode->nsName, vctxt->inode->localName));
	XSD10_FREE_AND_NULL(str);
	*msg = xml_xsd10_strcat(*msg, "': ");
    } else if (actxt->type == XML_XSD10_SCHEMA_CTXT_PARSER) {
	/*
	* Hmm, no node while parsing?
	* Return an empty string, in case NULL will break something.
	*/
	*msg = xml_xsd10_strdup("");
    } else {
	/* TODO */
	return (NULL);
    }

    /*
     * xmlSchemaFormatItemForReport() also returns an escaped format
     * string, so do this before calling it below (in the future).
     */
    xml_xsd10_escape_format_string(msg);

    /*
    * VAL TODO: The output of the given schema component is currently
    * disabled.
    */
    return (*msg);
}

/** Port of xmlSchemaInternalErr2. */
void xml_xsd10_internal_err2(XmlXsd10AbstractCtxt *actxt, const char *funcName, const char *message, const char *str1, const char *str2)
{
    char *msg = NULL;

    if (actxt == NULL)
        return;
    msg = xml_xsd10_strdup("Internal error: %s, ");
    msg = xml_xsd10_strcat(msg, message);
    msg = xml_xsd10_strcat(msg, ".\n");

    if (actxt->type == XML_XSD10_SCHEMA_CTXT_VALIDATOR)
	xml_xsd10_err3(actxt, XML_XSD10_SCHEMAV_INTERNAL, NULL,
	    (const char *) msg, (const char *) funcName, str1, str2);
    else if (actxt->type == XML_XSD10_SCHEMA_CTXT_PARSER)
	xml_xsd10_err3(actxt, XML_XSD10_SCHEMAP_INTERNAL, NULL,
	    (const char *) msg, (const char *) funcName, str1, str2);

    XSD10_FREE_AND_NULL(msg)
}

/** Port of xmlSchemaInternalErr. */
void xml_xsd10_internal_err(XmlXsd10AbstractCtxt *actxt, const char *funcName, const char *message)
{
    xml_xsd10_internal_err2(actxt, funcName, message, NULL, NULL);
}

/** Port of xmlSchemaCustomErr4. */
void xml_xsd10_custom_err4(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10BasicItem *item, const char *message, const char *str1, const char *str2, const char *str3, const char *str4)
{
    char *msg = NULL;

    if ((node == NULL) && (item != NULL) &&
	(actxt->type == XML_XSD10_SCHEMA_CTXT_PARSER)) {
	node = XSD10_WXS_ITEM_NODE(item);
	xml_xsd10_format_item_for_report(&msg, NULL, item, NULL);
	msg = xml_xsd10_strcat(msg, ": ");
    } else
	xml_xsd10_format_node_for_error(&msg, actxt, node);
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".\n");
    xml_xsd10_err4(actxt, error, node,
	(const char *) msg, str1, str2, str3, str4);
    XSD10_FREE_AND_NULL(msg)
}

/** Port of xmlSchemaCustomErr. */
void xml_xsd10_custom_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10BasicItem *item, const char *message, const char *str1, const char *str2)
{
    xml_xsd10_custom_err4(actxt, error, node, item,
	message, str1, str2, NULL, NULL);
}



/** Port of xmlSchemaCustomWarning. */
void xml_xsd10_custom_warning(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10Type *type, const char *message, const char *str1, const char *str2, const char *str3)
{
    char *msg = NULL;

    (void) type;
    xml_xsd10_format_node_for_error(&msg, actxt, node);
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".\n");

    /* URGENT TODO: Set the error code to something sane. */
    xml_xsd10_err4_line(actxt, XML_XSD10_ERR_WARNING, error, node, 0,
	(const char *) msg, str1, str2, str3, NULL);

    XSD10_FREE_AND_NULL(msg)
}



/** Port of xmlSchemaKeyrefErr. */
void xml_xsd10_keyref_err(XmlXsd10ValidCtxt *vctxt, int error, XmlXsd10PSVIIDCNode *idcNode, XmlXsd10Type *type, const char *message, const char *str1, const char *str2)
{
    char *msg = NULL, *qname = NULL;

    (void) type;
    msg = xml_xsd10_strdup("Element '%s': ");
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".\n");
    xml_xsd10_err4_line(XSD10_ACTXT_CAST vctxt, XML_XSD10_ERR_ERROR,
	error, NULL, idcNode->nodeLine, (const char *) msg,
	xml_xsd10_format_q_name(&qname,
	    (const char *) vctxt->nodeQNames->items[idcNode->nodeQNameID +1],
	    (const char *) vctxt->nodeQNames->items[idcNode->nodeQNameID]),
	str1, str2, NULL);
    XSD10_FREE_AND_NULL(qname);
    XSD10_FREE_AND_NULL(msg);
}

/** Port of xmlSchemaEvalErrorNodeType. */
int xml_xsd10_eval_error_node_type(XmlXsd10AbstractCtxt *actxt, XMQNode *node)
{
    if (node != NULL)
	return (xml_node_kind(node));
    if ((actxt->type == XML_XSD10_SCHEMA_CTXT_VALIDATOR) &&
	(((XmlXsd10ValidCtxt *) actxt)->inode != NULL))
	return ( ((XmlXsd10ValidCtxt *) actxt)->inode->nodeType);
    return (-1);
}

/** Port of xmlSchemaIsGlobalItem. */
int xml_xsd10_is_global_item(XmlXsd10Type *item)
{
    switch (item->type) {
	case XML_XSD10_SCHEMA_TYPE_COMPLEX:
	case XML_XSD10_SCHEMA_TYPE_SIMPLE:
	    if (item->flags & XML_XSD10_SCHEMAS_TYPE_GLOBAL)
		return(1);
	    break;
	case XML_XSD10_SCHEMA_TYPE_GROUP:
	    return (1);
	case XML_XSD10_SCHEMA_TYPE_ELEMENT:
	    if ( ((XmlXsd10Element *) item)->flags &
		XML_XSD10_SCHEMAS_ELEM_GLOBAL)
		return(1);
	    break;
	case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
	    if ( ((XmlXsd10Attribute *) item)->flags &
		XML_XSD10_SCHEMAS_ATTR_GLOBAL)
		return(1);
	    break;
	/* Note that attribute groups are always global. */
	default:
	    return(1);
    }
    return (0);
}

/** Port of xmlSchemaSimpleTypeErr. */
void xml_xsd10_simple_type_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *value, XmlXsd10Type *type, int displayValue)
{
    char *msg = NULL;

    xml_xsd10_format_node_for_error(&msg, actxt, node);

    if (displayValue || (xml_xsd10_eval_error_node_type(actxt, node) ==
	    XML_DOM_ATTRIBUTE))
	msg = xml_xsd10_strcat(msg, "'%s' is not a valid value of ");
    else
	msg = xml_xsd10_strcat(msg, "The character content is not a valid "
	    "value of ");

    if (! xml_xsd10_is_global_item(type))
	msg = xml_xsd10_strcat(msg, "the local ");
    else
	msg = xml_xsd10_strcat(msg, "the ");

    if (XSD10_WXS_IS_ATOMIC(type))
	msg = xml_xsd10_strcat(msg, "atomic type");
    else if (XSD10_WXS_IS_LIST(type))
	msg = xml_xsd10_strcat(msg, "list type");
    else if (XSD10_WXS_IS_UNION(type))
	msg = xml_xsd10_strcat(msg, "union type");

    if (xml_xsd10_is_global_item(type)) {
	char *str = NULL;
	msg = xml_xsd10_strcat(msg, " '");
	if (type->builtInType != 0) {
	    msg = xml_xsd10_strcat(msg, "xs:");
	    str = xml_xsd10_strdup(type->name);
	} else {
	    const char *qName = xml_xsd10_format_q_name(&str, type->targetNamespace, type->name);
	    if (!str)
		str = xml_xsd10_strdup(qName);
	}
	msg = xml_xsd10_strcat(msg, xml_xsd10_escape_format_string(&str));
	msg = xml_xsd10_strcat(msg, "'");
	XSD10_FREE_AND_NULL(str);
    }
    msg = xml_xsd10_strcat(msg, ".\n");
    if (displayValue || (xml_xsd10_eval_error_node_type(actxt, node) ==
	    XML_DOM_ATTRIBUTE))
	xml_xsd10_err(actxt, error, node, (const char *) msg, value, NULL);
    else
	xml_xsd10_err(actxt, error, node, (const char *) msg, NULL, NULL);
    XSD10_FREE_AND_NULL(msg)
}

/** Port of xmlSchemaFormatErrorNodeQName. */
const char *xml_xsd10_format_error_node_q_name(char **str, XmlXsd10NodeInfo *ni, XMQNode *node)
{
    if (node != NULL) {
	if (xsd10_schemas_ns_href(node) != NULL)
	    return (xml_xsd10_format_q_name(str, xsd10_schemas_ns_href(node), xml_node_name(node)));
	else
	    return (xml_xsd10_format_q_name(str, NULL, xml_node_name(node)));
    } else if (ni != NULL)
	return (xml_xsd10_format_q_name(str, ni->nsName, ni->localName));
    return (NULL);
}

/** Port of xmlSchemaIllegalAttrErr. */
void xml_xsd10_illegal_attr_err(XmlXsd10AbstractCtxt *actxt, int error, XmlXsd10AttrInfo *ni, XMQNode *node)
{
    char *msg = NULL, *str = NULL;

    xml_xsd10_format_node_for_error(&msg, actxt, node);
    msg = xml_xsd10_strcat(msg, "The attribute '%s' is not allowed.\n");
    xml_xsd10_err(actxt, error, node, (const char *) msg,
	xml_xsd10_format_error_node_q_name(&str, (XmlXsd10NodeInfo *) ni, node),
	NULL);
    XSD10_FREE_AND_NULL(str)
    XSD10_FREE_AND_NULL(msg)
}

/** Port of xmlSchemaComplexTypeErr. */
void xml_xsd10_complex_type_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, XmlXsd10Type *type, const char *message, int nbval, int nbneg, char **values)
{
    char *str = NULL, *msg = NULL;
    char *localName, *nsName;
    const char *cur, *end;
    int i;

    (void) type;
    xml_xsd10_format_node_for_error(&msg, actxt, node);
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".");
    /*
    * Note that is does not make sense to report that we have a
    * wildcard here, since the wildcard might be unfolded into
    * multiple transitions.
    */
    if (nbval + nbneg > 0) {
	if (nbval + nbneg > 1) {
	    str = xml_xsd10_strdup(" Expected is one of ( ");
	} else
	    str = xml_xsd10_strdup(" Expected is ( ");
	nsName = NULL;

	for (i = 0; i < nbval + nbneg; i++) {
	    cur = values[i];
	    if (cur == NULL)
	        continue;
	    if ((cur[0] == 'n') && (cur[1] == 'o') && (cur[2] == 't') &&
	        (cur[3] == ' ')) {
		cur += 4;
		str = xml_xsd10_strcat(str, "##other");
	    }
	    /*
	    * Get the local name.
	    */
	    localName = NULL;

	    end = cur;
	    if (*end == '*') {
		localName = xml_xsd10_strdup("*");
		end++;
	    } else {
		while ((*end != 0) && (*end != '|'))
		    end++;
		localName = xml_xsd10_strncat(localName, cur, (int) (end - cur));
	    }
	    if (*end != 0) {
		end++;
		/*
		* Skip "*|*" if they come with negated expressions, since
		* they represent the same negated wildcard.
		*/
		if ((nbneg == 0) || (*end != '*') || (*localName != '*')) {
		    /*
		    * Get the namespace name.
		    */
		    cur = end;
		    if (*end == '*') {
			nsName = xml_xsd10_strdup("{*}");
		    } else {
			while (*end != 0)
			    end++;

			if (i >= nbval)
			    nsName = xml_xsd10_strdup("{##other:");
			else
			    nsName = xml_xsd10_strdup("{");

			nsName = xml_xsd10_strncat(nsName, cur, (int) (end - cur));
			nsName = xml_xsd10_strcat(nsName, "}");
		    }
		    str = xml_xsd10_strcat(str, nsName);
		    XSD10_FREE_AND_NULL(nsName)
		} else {
		    XSD10_FREE_AND_NULL(localName);
		    continue;
		}
	    }
	    str = xml_xsd10_strcat(str, localName);
	    XSD10_FREE_AND_NULL(localName);

	    if (i < nbval + nbneg -1)
		str = xml_xsd10_strcat(str, ", ");
	}
	str = xml_xsd10_strcat(str, " ).\n");
	msg = xml_xsd10_strcat(msg, xml_xsd10_escape_format_string(&str));
	XSD10_FREE_AND_NULL(str)
    } else
      msg = xml_xsd10_strcat(msg, "\n");
    xml_xsd10_err(actxt, error, node, (const char *) msg, NULL, NULL);
    free(msg);
}

/** Port of xmlSchemaFacetErr. */
void xml_xsd10_facet_err(XmlXsd10AbstractCtxt *actxt, int error, XMQNode *node, const char *value, unsigned long length, XmlXsd10Type *type, XmlXsd10Facet *facet, const char *message, const char *str1, const char *str2)
{
    char *str = NULL, *msg = NULL;
    XmlXsd10TypeType facetType;
    int nodeType = xml_xsd10_eval_error_node_type(actxt, node);

    xml_xsd10_format_node_for_error(&msg, actxt, node);
    if (error == XML_XSD10_SCHEMAV_CVC_ENUMERATION_VALID) {
	facetType = XML_XSD10_SCHEMA_FACET_ENUMERATION;
	/*
	* If enumerations are validated, one must not expect the
	* facet to be given.
	*/
    } else
	facetType = facet->type;
    msg = xml_xsd10_strcat(msg, "[");
    msg = xml_xsd10_strcat(msg, "facet '");
    msg = xml_xsd10_strcat(msg, xml_xsd10_facet_type_to_string(facetType));
    msg = xml_xsd10_strcat(msg, "'] ");
    if (message == NULL) {
	/*
	* Use a default message.
	*/
	if ((facetType == XML_XSD10_SCHEMA_FACET_LENGTH) ||
	    (facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH) ||
	    (facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH)) {

	    char len[25], actLen[25];

	    /* FIXME, TODO: What is the max expected string length of the
	    * this value?
	    */
	    if (nodeType == XML_DOM_ATTRIBUTE)
		msg = xml_xsd10_strcat(msg, "The value '%s' has a length of '%s'; ");
	    else
		msg = xml_xsd10_strcat(msg, "The value has a length of '%s'; ");

	    snprintf(len, 24, "%lu", xml_xsd10_get_facet_value_as_ulong(facet));
	    snprintf(actLen, 24, "%lu", length);

	    if (facetType == XML_XSD10_SCHEMA_FACET_LENGTH)
		msg = xml_xsd10_strcat(msg,
		"this differs from the allowed length of '%s'.\n");
	    else if (facetType == XML_XSD10_SCHEMA_FACET_MAXLENGTH)
		msg = xml_xsd10_strcat(msg,
		"this exceeds the allowed maximum length of '%s'.\n");
	    else if (facetType == XML_XSD10_SCHEMA_FACET_MINLENGTH)
		msg = xml_xsd10_strcat(msg,
		"this underruns the allowed minimum length of '%s'.\n");

	    if (nodeType == XML_DOM_ATTRIBUTE)
		xml_xsd10_err3(actxt, error, node, (const char *) msg,
		    value, (const char *) actLen, (const char *) len);
	    else
		xml_xsd10_err(actxt, error, node, (const char *) msg,
		    (const char *) actLen, (const char *) len);

	} else if (facetType == XML_XSD10_SCHEMA_FACET_ENUMERATION) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' is not an element "
		"of the set {%s}.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		xml_xsd10_format_facet_enum_set(actxt, &str, type));
	} else if (facetType == XML_XSD10_SCHEMA_FACET_PATTERN) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' is not accepted "
		"by the pattern '%s'.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_MININCLUSIVE) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' is less than the "
		"minimum value allowed ('%s').\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' is greater than the "
		"maximum value allowed ('%s').\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' must be greater than "
		"'%s'.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' must be less than "
		"'%s'.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_TOTALDIGITS) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' has more "
		"digits than are allowed ('%s').\n");
	    xml_xsd10_err(actxt, error, node, (const char*) msg, value,
		facet->value);
	} else if (facetType == XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' has more fractional "
		"digits than are allowed ('%s').\n");
	    xml_xsd10_err(actxt, error, node, (const char*) msg, value,
		facet->value);
	} else if (nodeType == XML_DOM_ATTRIBUTE) {
	    msg = xml_xsd10_strcat(msg, "The value '%s' is not facet-valid.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, value, NULL);
	} else {
	    msg = xml_xsd10_strcat(msg, "The value is not facet-valid.\n");
	    xml_xsd10_err(actxt, error, node, (const char *) msg, NULL, NULL);
	}
    } else {
	msg = xml_xsd10_strcat(msg, (const char *) message);
	msg = xml_xsd10_strcat(msg, ".\n");
	xml_xsd10_err(actxt, error, node, (const char *) msg, str1, str2);
    }
    XSD10_FREE_AND_NULL(str)
    free(msg);
}

/**
 * Port of xmlSchemaPMissingAttrErr: reports an illegal attribute.
 *
 * @param ctxt  the schema validation context
 * @param error  the error code
 * @param ownerItem  the owner as a schema object
 * @param ownerElem  the owner as an element node
 * @param name  name of the attribute
 * @param message  the error message
 */
void xml_xsd10_p_missing_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, const char *message)
{
    char *des = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, ownerItem, ownerElem);

    if (message != NULL)
	xml_xsd10_p_err(ctxt, ownerElem, error, "%s: %s.\n", des, message);
    else
	xml_xsd10_p_err(ctxt, ownerElem, error,
	    "%s: The attribute '%s' is required but missing.\n",
	    des, name);
    XSD10_FREE_AND_NULL(des);
}


/**
 * Port of xmlSchemaPResCompAttrErr.
 *
 * Used to report QName attribute values that failed to resolve
 * to schema components.
 *
 * @param ctxt  the schema validation context
 * @param error  the error code
 * @param ownerItem  the owner as a schema object
 * @param ownerElem  the owner as an element node
 * @param name  the name of the attribute holding the QName
 * @param refName  the referenced local name
 * @param refURI  the referenced namespace URI
 * @param refType  the referenced type
 * @param refTypeStr  the referenced type string
 */
void xml_xsd10_p_res_comp_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, const char *name, const char *refName, const char *refURI, XmlXsd10TypeType refType, const char *refTypeStr)
{
    char *des = NULL, *strA = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, ownerItem, ownerElem);
    if (refTypeStr == NULL)
	refTypeStr = (const char *) xml_xsd10_item_type_to_str(refType);
    xml_xsd10_p_err_ext(ctxt, ownerElem, error,
	    NULL, NULL, NULL,
	    "%s, attribute '%s': The QName value '%s' does not resolve to a(n) "
	    "%s.\n", des, name,
	    xml_xsd10_format_q_name(&strA, refURI, refName),
	    refTypeStr, NULL);
    XSD10_FREE_AND_NULL(des)
    XSD10_FREE_AND_NULL(strA)
}

/**
 * Port of xmlSchemaPCustomAttrErr: reports an illegal attribute during the parse.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param ownerDes  the designation of the owner
 * @param ownerItem  the owner as a schema object
 * @param attr  the illegal attribute node
 * @param msg  the error message
 */
void xml_xsd10_p_custom_attr_err(XmlXsd10ParserCtxt *ctxt, int error, char **ownerDes, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *msg)
{
    char *des = NULL;

    if (ownerDes == NULL)
	xml_xsd10_format_item_for_report(&des, NULL, ownerItem, attr ? xml_node_parent(attr) : NULL);
    else if (*ownerDes == NULL) {
	xml_xsd10_format_item_for_report(ownerDes, NULL, ownerItem, attr ? xml_node_parent(attr) : NULL);
	des = *ownerDes;
    } else
	des = *ownerDes;
    if (attr == NULL) {
	xml_xsd10_p_err_ext(ctxt, NULL, error, NULL, NULL, NULL,
	    "%s, attribute '%s': %s.\n",
	    des, (const char *) "Unknown",
	    (const char *) msg, NULL, NULL);
    } else {
	xml_xsd10_p_err_ext(ctxt, attr, error, NULL, NULL, NULL,
	    "%s, attribute '%s': %s.\n",
	    des, xml_node_name(attr), (const char *) msg, NULL, NULL);
    }
    if (ownerDes == NULL)
	XSD10_FREE_AND_NULL(des);
}

/**
 * Port of xmlSchemaPIllegalAttrErr: reports an illegal attribute during the parse.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param ownerComp  the attribute's owner item (unused)
 * @param attr  the illegal attribute node
 */
void xml_xsd10_p_illegal_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerComp, XMQNode *attr)
{
    char *strA = NULL, *strB = NULL;

    (void) ownerComp;
    xml_xsd10_format_node_for_error(&strA, XSD10_ACTXT_CAST ctxt, xml_node_parent(attr));
    xml_xsd10_err4(XSD10_ACTXT_CAST ctxt, error, attr,
	"%sThe attribute '%s' is not allowed.\n", strA,
	xml_xsd10_format_q_name(&strB, xsd10_schemas_ns_href(attr), xml_node_name(attr)),
	NULL, NULL);
    XSD10_FREE_AND_NULL(strA);
    XSD10_FREE_AND_NULL(strB);
}

/**
 * Port of xmlSchemaPCustomErrExt: reports an error during parsing.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param item  the schema item
 * @param itemElem  the node of the schema item
 * @param message  the error message
 * @param str1  an optional param for the error message
 * @param str2  an optional param for the error message
 * @param str3  an optional param for the error message
 */
void xml_xsd10_p_custom_err_ext(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *item, XMQNode *itemElem, const char *message, const char *str1, const char *str2, const char *str3)
{
    char *des = NULL, *msg = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, item, itemElem);
    msg = xml_xsd10_strdup("%s: ");
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".\n");
    if ((itemElem == NULL) && (item != NULL))
	itemElem = XSD10_WXS_ITEM_NODE(item);
    xml_xsd10_p_err_ext(ctxt, itemElem, error, NULL, NULL, NULL,
	(const char *) msg, des, str1, str2, str3, NULL);
    XSD10_FREE_AND_NULL(des);
    XSD10_FREE_AND_NULL(msg);
}

/**
 * Port of xmlSchemaPCustomErr: reports an error during parsing.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param item  the schema item
 * @param itemElem  the node of the schema item
 * @param message  the error message
 * @param str1  the optional param for the error message
 */
void xml_xsd10_p_custom_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *item, XMQNode *itemElem, const char *message, const char *str1)
{
    xml_xsd10_p_custom_err_ext(ctxt, error, item, itemElem, message,
	str1, NULL, NULL);
}

/**
 * Port of xmlSchemaPAttrUseErr4: reports an attribute use error during parsing.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param node  the context node
 * @param ownerItem  the owner as a schema object
 * @param attruse  the invalid schema attribute
 * @param message  the error message
 * @param str1  the optional param for the error message
 * @param str2  the optional param for the error message
 * @param str3  the optional param for the error message
 * @param str4  the optional param for the error message
 */
void xml_xsd10_p_attr_use_err4(XmlXsd10ParserCtxt *ctxt, int error, XMQNode *node, XmlXsd10BasicItem *ownerItem, const XmlXsd10AttributeUse *attruse, const char *message, const char *str1, const char *str2, const char *str3, const char *str4)
{
    char *str = NULL, *msg = NULL;

    xml_xsd10_format_item_for_report(&msg, NULL, ownerItem, NULL);
    msg = xml_xsd10_strcat(msg, ", ");
    msg = xml_xsd10_strcat(msg,
	xml_xsd10_format_item_for_report(&str, NULL,
	XSD10_WXS_BASIC_CAST attruse, NULL));
    XSD10_FREE_AND_NULL(str);
    msg = xml_xsd10_strcat(msg, ": ");
    msg = xml_xsd10_strcat(msg, (const char *) message);
    msg = xml_xsd10_strcat(msg, ".\n");
    xml_xsd10_err4(XSD10_ACTXT_CAST ctxt, error, node,
	(const char *) msg, str1, str2, str3, str4);
    free(msg);
}

/**
 * Port of xmlSchemaPIllegalFacetAtomicErr: reports an illegal facet for atomic simple types.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param type  the schema type
 * @param baseType  the base type of type
 * @param facet  the illegal facet
 */
void xml_xsd10_p_illegal_facet_atomic_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10Type *type, XmlXsd10Type *baseType, XmlXsd10Facet *facet)
{
    char *des = NULL, *strT = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, XSD10_WXS_BASIC_CAST type, type->node);
    xml_xsd10_p_err_ext(ctxt, type->node, error, NULL, NULL, NULL,
	"%s: The facet '%s' is not allowed on types derived from the "
	"type %s.\n",
	des, xml_xsd10_facet_type_to_string(facet->type),
	xml_xsd10_format_item_for_report(&strT, NULL, XSD10_WXS_BASIC_CAST baseType, NULL),
	NULL, NULL);
    XSD10_FREE_AND_NULL(des);
    XSD10_FREE_AND_NULL(strT);
}

/**
 * Port of xmlSchemaPIllegalFacetListUnionErr: reports an illegal facet for `<list>` and `<union>`.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param type  the schema type
 * @param facet  the illegal facet
 */
void xml_xsd10_p_illegal_facet_list_union_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10Type *type, XmlXsd10Facet *facet)
{
    char *des = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, XSD10_WXS_BASIC_CAST type,
	type->node);
    xml_xsd10_p_err(ctxt, type->node, error,
	"%s: The facet '%s' is not allowed.\n",
	des, xml_xsd10_facet_type_to_string(facet->type));
    XSD10_FREE_AND_NULL(des);
}

/**
 * Port of xmlSchemaPMutualExclAttrErr: reports an illegal attribute.
 *
 * @param ctxt  the schema validation context
 * @param error  the error code
 * @param ownerItem  the owner as a schema object
 * @param attr  the bad attribute node
 * @param name1  first mutually exclusive name
 * @param name2  second mutually exclusive name
 */
void xml_xsd10_p_mutual_excl_attr_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *attr, const char *name1, const char *name2)
{
    char *des = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, XSD10_WXS_BASIC_CAST ownerItem, xml_node_parent(attr));
    xml_xsd10_p_err_ext(ctxt, attr, error, NULL, NULL, NULL,
	"%s: The attributes '%s' and '%s' are mutually exclusive.\n",
	des, name1, name2, NULL, NULL);
    XSD10_FREE_AND_NULL(des);
}

/**
 * Port of xmlSchemaPSimpleTypeErr: reports a simple type validation error.
 * TODO: Should this report the value of an element as well?
 *
 * @param ctxt  the schema validation context
 * @param error  the error code
 * @param ownerItem  the schema object if existent
 * @param node  the validated node
 * @param type  the type specifier
 * @param expected  the expected value
 * @param value  the validated value
 * @param message  error message
 * @param str1  extra data
 * @param str2  extra data
 */
void xml_xsd10_p_simple_type_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *node, XmlXsd10Type *type, const char *expected, const char *value, const char *message, const char *str1, const char *str2)
{
    char *msg = NULL;

    (void) ownerItem;
    xml_xsd10_format_node_for_error(&msg, XSD10_ACTXT_CAST ctxt, node);
    if (message == NULL) {
	/*
	* Use default messages.
	*/
	if (type != NULL) {
	    if (xml_node_kind(node) == XML_DOM_ATTRIBUTE)
		msg = xml_xsd10_strcat(msg, "'%s' is not a valid value of ");
	    else
		msg = xml_xsd10_strcat(msg, "The character content is not a "
		"valid value of ");
	    if (! xml_xsd10_is_global_item(type))
		msg = xml_xsd10_strcat(msg, "the local ");
	    else
		msg = xml_xsd10_strcat(msg, "the ");

	    if (XSD10_WXS_IS_ATOMIC(type))
		msg = xml_xsd10_strcat(msg, "atomic type");
	    else if (XSD10_WXS_IS_LIST(type))
		msg = xml_xsd10_strcat(msg, "list type");
	    else if (XSD10_WXS_IS_UNION(type))
		msg = xml_xsd10_strcat(msg, "union type");

	    if (xml_xsd10_is_global_item(type)) {
		char *str = NULL;
		msg = xml_xsd10_strcat(msg, " '");
		if (type->builtInType != 0) {
		    msg = xml_xsd10_strcat(msg, "xs:");
		    str = xml_xsd10_strdup(type->name);
		} else {
		    const char *qName = xml_xsd10_format_q_name(&str, type->targetNamespace, type->name);
		    if (!str)
			str = xml_xsd10_strdup(qName);
		}
		msg = xml_xsd10_strcat(msg, xml_xsd10_escape_format_string(&str));
		msg = xml_xsd10_strcat(msg, "'.");
		XSD10_FREE_AND_NULL(str);
	    }
	} else {
	    if (xml_node_kind(node) == XML_DOM_ATTRIBUTE)
		msg = xml_xsd10_strcat(msg, "The value '%s' is not valid.");
	    else
		msg = xml_xsd10_strcat(msg, "The character content is not "
		"valid.");
	}
	if (expected) {
	    char *expectedEscaped = xml_xsd10_char_strdup(expected);
	    msg = xml_xsd10_strcat(msg, " Expected is '");
	    msg = xml_xsd10_strcat(msg, xml_xsd10_escape_format_string(&expectedEscaped));
	    XSD10_FREE_AND_NULL(expectedEscaped);
	    msg = xml_xsd10_strcat(msg, "'.\n");
	} else
	    msg = xml_xsd10_strcat(msg, "\n");
	if (xml_node_kind(node) == XML_DOM_ATTRIBUTE)
	    xml_xsd10_p_err(ctxt, node, error, (const char *) msg, value, NULL);
	else
	    xml_xsd10_p_err(ctxt, node, error, (const char *) msg, NULL, NULL);
    } else {
	msg = xml_xsd10_strcat(msg, message);
	msg = xml_xsd10_strcat(msg, ".\n");
	xml_xsd10_p_err_ext(ctxt, node, error, NULL, NULL, NULL,
	     (const char*) msg, str1, str2, NULL, NULL, NULL);
    }
    /* Cleanup. */
    XSD10_FREE_AND_NULL(msg)
}

/**
 * Port of xmlSchemaPContentErr: reports an error concerning the content of a schema element.
 *
 * @param ctxt  the schema parser context
 * @param error  the error code
 * @param ownerItem  the owner item of the holder of the content
 * @param ownerElem  the node of the holder of the content
 * @param child  the invalid child node
 * @param message  the optional error message
 * @param content  the optional string describing the correct content
 */
void xml_xsd10_p_content_err(XmlXsd10ParserCtxt *ctxt, int error, XmlXsd10BasicItem *ownerItem, XMQNode *ownerElem, XMQNode *child, const char *message, const char *content)
{
    char *des = NULL;

    xml_xsd10_format_item_for_report(&des, NULL, ownerItem, ownerElem);
    if (message != NULL)
	xml_xsd10_p_err2(ctxt, ownerElem, child, error,
	    "%s: %s.\n",
	    des, message);
    else {
	if (content != NULL) {
	    xml_xsd10_p_err2(ctxt, ownerElem, child, error,
		"%s: The content is not valid. Expected is %s.\n",
		des, content);
	} else {
	    xml_xsd10_p_err2(ctxt, ownerElem, child, error,
		"%s: The content is not valid.\n",
		des, NULL);
	}
    }
    XSD10_FREE_AND_NULL(des)
}

/************************************************************************
 *									*
 *			Allocation functions				*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaNewSchema: allocate a new Schema structure.
 *
 * @param ctxt  a schema validation context
 * @returns the newly allocated structure or NULL in case or error
 */
XmlXsd10Schema *xml_xsd10_new_schema(XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10Schema *ret;

    ret = (XmlXsd10Schema *) malloc(sizeof(XmlXsd10Schema));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Schema));
    ret->dict = ctxt->dict;
    xml_xsd10_dict_reference(ret->dict);

    return (ret);
}

/* xmlSchemaNewFacet is xml_xsd10_new_facet of xml_xsd10_types. */

/**
 * Port of xmlSchemaNewAnnot: allocate a new annotation structure.
 *
 * @param ctxt  a schema validation context
 * @param node  a node
 * @returns the newly allocated structure or NULL in case or error
 */
XmlXsd10Annot *xml_xsd10_new_annot(XmlXsd10ParserCtxt *ctxt, XMQNode *node)
{
    XmlXsd10Annot *ret;

    ret = (XmlXsd10Annot *) malloc(sizeof(XmlXsd10Annot));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(ctxt);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10Annot));
    ret->content = node;
    return (ret);
}

/** Port of xmlSchemaItemListCreate. */
XmlXsd10ItemList *xml_xsd10_item_list_create(void)
{
    XmlXsd10ItemList *ret;

    ret = (XmlXsd10ItemList *) malloc(sizeof(XmlXsd10ItemList));
    if (ret == NULL) {
	xml_xsd10_p_err_memory(NULL);
	return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ItemList));
    return (ret);
}

/** Port of xmlSchemaItemListClear. */
void xml_xsd10_item_list_clear(XmlXsd10ItemList *list)
{
    if (list->items != NULL) {
	free(list->items);
	list->items = NULL;
    }
    list->nbItems = 0;
    list->sizeItems = 0;
}

/** Port of xmlSchemaItemListGrow. */
int xml_xsd10_item_list_grow(XmlXsd10ItemList *list, int initialSize)
{
    void **tmp;
    int newSize;

    if (initialSize <= 0)
        initialSize = 1;
    newSize = xml_xsd10_grow_capacity(list->sizeItems, sizeof(tmp[0]),
                              initialSize, XML_XSD10_MAX_ITEMS);
    if (newSize < 0)
        return(-1);
    tmp = (void **) realloc(list->items, (size_t) newSize * sizeof(tmp[0]));
    if (tmp == NULL)
        return(-1);

    list->items = tmp;
    list->sizeItems = newSize;
    return(0);
}

/** Port of xmlSchemaItemListAddSize. */
int xml_xsd10_item_list_add_size(XmlXsd10ItemList *list, int initialSize, void *item)
{
    if (list->sizeItems <= list->nbItems) {
        if (xml_xsd10_item_list_grow(list, initialSize) < 0) {
	    xml_xsd10_p_err_memory(NULL);
	    return(-1);
        }
    }

    list->items[list->nbItems++] = item;
    return(0);
}

/** Port of xmlSchemaItemListAdd. */
int xml_xsd10_item_list_add(XmlXsd10ItemList *list, void *item)
{
    return(xml_xsd10_item_list_add_size(list, 20, item));
}

/** Port of xmlSchemaItemListInsert. */
int xml_xsd10_item_list_insert(XmlXsd10ItemList *list, void *item, int idx)
{
    if (list->sizeItems <= list->nbItems) {
        if (xml_xsd10_item_list_grow(list, 20) < 0) {
	    xml_xsd10_p_err_memory(NULL);
	    return(-1);
        }
    }

    /*
    * Just append if the index is greater/equal than the item count.
    */
    if (idx >= list->nbItems) {
	list->items[list->nbItems++] = item;
    } else {
	int i;
	for (i = list->nbItems; i > idx; i--)
	    list->items[i] = list->items[i-1];
	list->items[idx] = item;
	list->nbItems++;
    }
    return(0);
}

/** Port of xmlSchemaItemListRemove. */
int xml_xsd10_item_list_remove(XmlXsd10ItemList *list, int idx)
{
    int i;
    if ((list->items == NULL) || (idx >= list->nbItems))
	return(-1);

    if (list->nbItems == 1) {
	/* TODO: Really free the list? */
	free(list->items);
	list->items = NULL;
	list->nbItems = 0;
	list->sizeItems = 0;
    } else if (list->nbItems -1 == idx) {
	list->nbItems--;
    } else {
	for (i = idx; i < list->nbItems -1; i++)
	    list->items[i] = list->items[i+1];
	list->nbItems--;
    }
    return(0);
}

/**
 * Port of xmlSchemaItemListFree: deallocate a annotation structure
 *
 * @param list  a schema type structure
 */
void xml_xsd10_item_list_free(XmlXsd10ItemList *list)
{
    if (list == NULL)
	return;
    if (list->items != NULL)
	free(list->items);
    free(list);
}

/** Port of xmlSchemaBucketFree (also frees the ids of the bucket). */
void xml_xsd10_bucket_free(XmlXsd10Bucket *bucket)
{
    if (bucket == NULL)
	return;
    if (bucket->globals != NULL) {
	xml_xsd10_component_list_free(bucket->globals);
	xml_xsd10_item_list_free(bucket->globals);
    }
    if (bucket->locals != NULL) {
	xml_xsd10_component_list_free(bucket->locals);
	xml_xsd10_item_list_free(bucket->locals);
    }
    if (bucket->relations != NULL) {
	XmlXsd10SchemaRelation *prev, *cur = bucket->relations;
	do {
	    prev = cur;
	    cur = cur->next;
	    free(prev);
	} while (cur != NULL);
    }
    if (bucket->ids != NULL)
        xml_xsd10_ids_free(bucket->ids);
    if ((! bucket->preserveDoc) && (bucket->doc != NULL)) {
	xml_dom_free(bucket->doc);
    }
    if (bucket->type == XML_XSD10_SCHEMA_SCHEMA_IMPORT) {
	if (XSD10_WXS_IMPBUCKET(bucket)->schema != NULL)
	    xml_xsd10_free(XSD10_WXS_IMPBUCKET(bucket)->schema);
    }
    free(bucket);
}

/** Port of xmlSchemaBucketFreeEntry. */
void xml_xsd10_bucket_free_entry(void *bucket, const char *name)
{
    (void) name;
    xml_xsd10_bucket_free((XmlXsd10Bucket *) bucket);
}

/** Port of xmlSchemaBucketCreate. */
XmlXsd10Bucket *xml_xsd10_bucket_create(XmlXsd10ParserCtxt *pctxt, int type, const char *targetNamespace)
{
    XmlXsd10Bucket *ret;
    int size;
    XmlXsd10Schema *mainSchema;

    if (XSD10_WXS_CONSTRUCTOR(pctxt)->mainSchema == NULL) {
	XSD10_PERROR_INT("xmlSchemaBucketCreate",
	    "no main schema on constructor");
	return(NULL);
    }
    mainSchema = XSD10_WXS_CONSTRUCTOR(pctxt)->mainSchema;
    /* Create the schema bucket. */
    if (XSD10_WXS_IS_BUCKET_INCREDEF(type))
	size = sizeof(XmlXsd10Include);
    else
	size = sizeof(XmlXsd10Import);
    ret = (XmlXsd10Bucket *) malloc((size_t) size);
    if (ret == NULL) {
	xml_xsd10_p_err_memory(NULL);
	return(NULL);
    }
    memset(ret, 0, (size_t) size);
    ret->targetNamespace = targetNamespace;
    ret->type = type;
    ret->globals = xml_xsd10_item_list_create();
    if (ret->globals == NULL) {
	xml_xsd10_bucket_free(ret);
	return(NULL);
    }
    ret->locals = xml_xsd10_item_list_create();
    if (ret->locals == NULL) {
	xml_xsd10_bucket_free(ret);
	return(NULL);
    }
    /*
    * The following will assure that only the first bucket is marked as
    * XML_SCHEMA_SCHEMA_MAIN and it points to the *main* schema.
    * For each following import buckets an xmlSchema will be created.
    * An xmlSchema will be created for every distinct targetNamespace.
    * We assign the targetNamespace to the schemata here.
    */
    if (! XSD10_WXS_HAS_BUCKETS(pctxt)) {
	if (XSD10_WXS_IS_BUCKET_INCREDEF(type)) {
	    XSD10_PERROR_INT("xmlSchemaBucketCreate",
		"first bucket but it's an include or redefine");
	    xml_xsd10_bucket_free(ret);
	    return(NULL);
	}
	/* Force the type to be XML_SCHEMA_SCHEMA_MAIN. */
	ret->type = XML_XSD10_SCHEMA_SCHEMA_MAIN;
	/* Point to the *main* schema. */
	XSD10_WXS_CONSTRUCTOR(pctxt)->mainBucket = ret;
	XSD10_WXS_IMPBUCKET(ret)->schema = mainSchema;
	/*
	* Ensure that the main schema gets a targetNamespace.
	*/
	mainSchema->targetNamespace = targetNamespace;
    } else {
	if (type == XML_XSD10_SCHEMA_SCHEMA_MAIN) {
	    XSD10_PERROR_INT("xmlSchemaBucketCreate",
		"main bucket but it's not the first one");
	    xml_xsd10_bucket_free(ret);
	    return(NULL);
	} else if (type == XML_XSD10_SCHEMA_SCHEMA_IMPORT) {
	    /*
	    * Create a schema for imports and assign the
	    * targetNamespace.
	    */
	    XSD10_WXS_IMPBUCKET(ret)->schema = xml_xsd10_new_schema(pctxt);
	    if (XSD10_WXS_IMPBUCKET(ret)->schema == NULL) {
		xml_xsd10_bucket_free(ret);
		return(NULL);
	    }
	    XSD10_WXS_IMPBUCKET(ret)->schema->targetNamespace = targetNamespace;
	}
    }
    if (XSD10_WXS_IS_BUCKET_IMPMAIN(type)) {
	int res;
	/*
	* Imports go into the "schemasImports" slot of the main *schema*.
	* Note that we create an import entry for the main schema as well; i.e.,
	* even if there's only one schema, we'll get an import.
	*/
	if (mainSchema->schemasImports == NULL) {
	    mainSchema->schemasImports = xml_xsd10_hash_create_dict(5,
		XSD10_WXS_CONSTRUCTOR(pctxt)->dict);
	    if (mainSchema->schemasImports == NULL) {
		xml_xsd10_bucket_free(ret);
		return(NULL);
	    }
	}
	if (targetNamespace == NULL)
	    res = xml_xsd10_hash_add_entry(mainSchema->schemasImports,
		XML_XSD10_SCHEMAS_NO_NAMESPACE, ret);
	else
	    res = xml_xsd10_hash_add_entry(mainSchema->schemasImports,
		targetNamespace, ret);
	if (res != 0) {
	    XSD10_PERROR_INT("xmlSchemaBucketCreate",
		"failed to add the schema bucket to the hash");
	    xml_xsd10_bucket_free(ret);
	    return(NULL);
	}
    } else {
	/* Set the @ownerImport of an include bucket. */
	if (XSD10_WXS_IS_BUCKET_IMPMAIN(XSD10_WXS_CONSTRUCTOR(pctxt)->bucket->type))
	    XSD10_WXS_INCBUCKET(ret)->ownerImport =
		XSD10_WXS_IMPBUCKET(XSD10_WXS_CONSTRUCTOR(pctxt)->bucket);
	else
	    XSD10_WXS_INCBUCKET(ret)->ownerImport =
		XSD10_WXS_INCBUCKET(XSD10_WXS_CONSTRUCTOR(pctxt)->bucket)->ownerImport;

	/* Includes got into the "includes" slot of the *main* schema. */
	if (mainSchema->includes == NULL) {
	    mainSchema->includes = xml_xsd10_item_list_create();
	    if (mainSchema->includes == NULL) {
		xml_xsd10_bucket_free(ret);
		return(NULL);
	    }
	}
	if (xml_xsd10_item_list_add((XmlXsd10ItemList *) mainSchema->includes, ret) < 0) {
	    xml_xsd10_bucket_free(ret);
	    return(NULL);
        }
    }
    /*
    * Add to list of all buckets; this is used for lookup
    * during schema construction time only.
    */
    if (xml_xsd10_item_list_add(XSD10_WXS_CONSTRUCTOR(pctxt)->buckets, ret) == -1)
	return(NULL);
    return(ret);
}

/** Port of xmlSchemaAddItemSize. */
int xml_xsd10_add_item_size(XmlXsd10ItemList **list, int initialSize, void *item)
{
    if (*list == NULL) {
	*list = xml_xsd10_item_list_create();
	if (*list == NULL)
	    return(-1);
    }
    return(xml_xsd10_item_list_add_size(*list, initialSize, item));
}

/**
 * Port of xmlSchemaFreeAnnot: deallocate a annotation structure
 *
 * @param annot  a schema type structure
 */
void xml_xsd10_free_annot(XmlXsd10Annot *annot)
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

/**
 * Port of xmlSchemaFreeNotation: deallocate a Schema Notation structure.
 *
 * @param nota  a schema notation structure
 */
void xml_xsd10_free_notation(XmlXsd10Notation *nota)
{
    if (nota == NULL)
        return;
    if (nota->annot != NULL)
	xml_xsd10_free_annot(nota->annot);
    free(nota);
}

/**
 * Port of xmlSchemaFreeAttribute: deallocates an attribute declaration structure.
 *
 * @param attr  an attribute declaration
 */
void xml_xsd10_free_attribute(XmlXsd10Attribute *attr)
{
    if (attr == NULL)
        return;
    if (attr->annot != NULL)
	xml_xsd10_free_annot(attr->annot);
    if (attr->defVal != NULL)
	xml_xsd10_free_value(attr->defVal);
    free(attr);
}

/**
 * Port of xmlSchemaFreeAttributeUse: deallocates an attribute use structure.
 *
 * @param use  an attribute use
 */
void xml_xsd10_free_attribute_use(XmlXsd10AttributeUse *use)
{
    if (use == NULL)
        return;
    if (use->annot != NULL)
	xml_xsd10_free_annot(use->annot);
    if (use->defVal != NULL)
	xml_xsd10_free_value(use->defVal);
    free(use);
}

/**
 * Port of xmlSchemaFreeAttributeUseProhib: deallocates an attribute use structure.
 *
 * @param prohib  an attribute use prohibition
 */
void xml_xsd10_free_attribute_use_prohib(XmlXsd10AttributeUseProhib *prohib)
{
    if (prohib == NULL)
        return;
    free(prohib);
}

/**
 * Port of xmlSchemaFreeWildcardNsSet.
 * set:  a schema wildcard namespace
 *
 * Deallocates a list of wildcard constraint structures.
 */
void xml_xsd10_free_wildcard_ns_set(XmlXsd10WildcardNs *set)
{
    XmlXsd10WildcardNs *next;

    while (set != NULL) {
	next = set->next;
	free(set);
	set = next;
    }
}

/**
 * Port of xmlSchemaFreeWildcard: deallocates a wildcard structure.
 *
 * @param wildcard  a wildcard structure
 */
void xml_xsd10_free_wildcard(XmlXsd10Wildcard *wildcard)
{
    if (wildcard == NULL)
        return;
    if (wildcard->annot != NULL)
        xml_xsd10_free_annot(wildcard->annot);
    if (wildcard->nsSet != NULL)
	xml_xsd10_free_wildcard_ns_set(wildcard->nsSet);
    if (wildcard->negNsSet != NULL)
	free(wildcard->negNsSet);
    free(wildcard);
}

/**
 * Port of xmlSchemaFreeAttributeGroup: deallocate a Schema Attribute Group structure.
 *
 * @param attrGr  a schema attribute group structure
 */
void xml_xsd10_free_attribute_group(XmlXsd10AttributeGroup *attrGr)
{
    if (attrGr == NULL)
        return;
    if (attrGr->annot != NULL)
        xml_xsd10_free_annot(attrGr->annot);
    if (attrGr->attrUses != NULL)
	xml_xsd10_item_list_free(XSD10_WXS_LIST_CAST attrGr->attrUses);
    free(attrGr);
}

/**
 * Port of xmlSchemaFreeQNameRef: deallocatea a QName reference structure.
 *
 * @param item  a QName reference structure
 */
void xml_xsd10_free_q_name_ref(XmlXsd10QNameRef *item)
{
    free(item);
}

/**
 * Port of xmlSchemaFreeTypeLinkList: deallocate a list of types.
 *
 * @param link  a type link
 */
void xml_xsd10_free_type_link_list(XmlXsd10TypeLink *link)
{
    XmlXsd10TypeLink *next;

    while (link != NULL) {
	next = link->next;
	free(link);
	link = next;
    }
}

/** Port of xmlSchemaFreeIDCStateObjList. */
void xml_xsd10_free_idc_state_obj_list(XmlXsd10IDCStateObj *sto)
{
    XmlXsd10IDCStateObj *next;
    while (sto != NULL) {
	next = sto->next;
	if (sto->history != NULL)
	    free(sto->history);
	if (sto->xpathCtxt != NULL)
	    xml_stream_ctxt_free((XmlStreamCtxt *) sto->xpathCtxt);
	free(sto);
	sto = next;
    }
}

/**
 * Port of xmlSchemaFreeIDC: deallocates an identity-constraint definition.
 *
 * @param idcDef  a identity-constraint definition
 */
void xml_xsd10_free_idc(XmlXsd10IDC *idcDef)
{
    XmlXsd10IDCSelect *cur, *prev;

    if (idcDef == NULL)
	return;
    if (idcDef->annot != NULL)
        xml_xsd10_free_annot(idcDef->annot);
    /* Selector */
    if (idcDef->selector != NULL) {
	if (idcDef->selector->xpathComp != NULL)
	    xml_pattern_free((XmlPattern *) idcDef->selector->xpathComp);
	free(idcDef->selector);
    }
    /* Fields */
    if (idcDef->fields != NULL) {
	cur = idcDef->fields;
	do {
	    prev = cur;
	    cur = cur->next;
	    if (prev->xpathComp != NULL)
		xml_pattern_free((XmlPattern *) prev->xpathComp);
	    free(prev);
	} while (cur != NULL);
    }
    free(idcDef);
}

/**
 * Port of xmlSchemaFreeElement: deallocate a Schema Element structure.
 *
 * @param elem  a schema element structure
 */
void xml_xsd10_free_element(XmlXsd10Element *elem)
{
    if (elem == NULL)
        return;
    if (elem->annot != NULL)
        xml_xsd10_free_annot(elem->annot);
    if (elem->contModel != NULL)
        xml_regexp_free((XmlRegexp *) elem->contModel);
    if (elem->defVal != NULL)
	xml_xsd10_free_value(elem->defVal);
    free(elem);
}

/* xmlSchemaFreeFacet is xml_xsd10_free_facet of xml_xsd10_types (the regexp is
   freed through the regexp hooks, that is xml_regexp_free). */

/**
 * Port of xmlSchemaFreeType: deallocate a Schema Type structure.
 *
 * @param type  a schema type structure
 */
void xml_xsd10_free_type(XmlXsd10Type *type)
{
    if (type == NULL)
        return;
    if (type->annot != NULL)
        xml_xsd10_free_annot(type->annot);
    if (type->facets != NULL) {
        XmlXsd10Facet *facet, *next;

        facet = type->facets;
        while (facet != NULL) {
            next = facet->next;
            xml_xsd10_free_facet(facet);
            facet = next;
        }
    }
    if (type->attrUses != NULL)
	xml_xsd10_item_list_free((XmlXsd10ItemList *) type->attrUses);
    if (type->memberTypes != NULL)
	xml_xsd10_free_type_link_list(type->memberTypes);
    if (type->facetSet != NULL) {
	XmlXsd10FacetLink *next, *link;

	link = type->facetSet;
	do {
	    next = link->next;
	    free(link);
	    link = next;
	} while (link != NULL);
    }
    if (type->contModel != NULL)
        xml_regexp_free((XmlRegexp *) type->contModel);
    free(type);
}

/**
 * Port of xmlSchemaFreeModelGroupDef: deallocates a schema model group definition.
 *
 * @param item  a schema model group definition
 */
void xml_xsd10_free_model_group_def(XmlXsd10ModelGroupDef *item)
{
    if (item->annot != NULL)
	xml_xsd10_free_annot(item->annot);
    free(item);
}

/**
 * Port of xmlSchemaFreeModelGroup: deallocates a schema model group structure.
 *
 * @param item  a schema model group
 */
void xml_xsd10_free_model_group(XmlXsd10ModelGroup *item)
{
    if (item->annot != NULL)
	xml_xsd10_free_annot(item->annot);
    free(item);
}

/** Port of xmlSchemaComponentListFree. */
void xml_xsd10_component_list_free(XmlXsd10ItemList *list)
{
    if ((list == NULL) || (list->nbItems == 0))
	return;
    {
	XmlXsd10TreeItem *item;
	XmlXsd10TreeItem **items = (XmlXsd10TreeItem **) list->items;
	int i;

	for (i = 0; i < list->nbItems; i++) {
	    item = items[i];
	    if (item == NULL)
		continue;
	    switch (item->type) {
		case XML_XSD10_SCHEMA_TYPE_SIMPLE:
		case XML_XSD10_SCHEMA_TYPE_COMPLEX:
		    xml_xsd10_free_type((XmlXsd10Type *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE:
		    xml_xsd10_free_attribute((XmlXsd10Attribute *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE:
		    xml_xsd10_free_attribute_use((XmlXsd10AttributeUse *) item);
		    break;
		case XML_XSD10_SCHEMA_EXTRA_ATTR_USE_PROHIB:
		    xml_xsd10_free_attribute_use_prohib(
			(XmlXsd10AttributeUseProhib *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_ELEMENT:
		    xml_xsd10_free_element((XmlXsd10Element *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_PARTICLE:
		    if (item->annot != NULL)
			xml_xsd10_free_annot(item->annot);
		    free(item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_SEQUENCE:
		case XML_XSD10_SCHEMA_TYPE_CHOICE:
		case XML_XSD10_SCHEMA_TYPE_ALL:
		    xml_xsd10_free_model_group((XmlXsd10ModelGroup *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP:
		    xml_xsd10_free_attribute_group(
			(XmlXsd10AttributeGroup *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_GROUP:
		    xml_xsd10_free_model_group_def(
			(XmlXsd10ModelGroupDef *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_ANY:
		case XML_XSD10_SCHEMA_TYPE_ANY_ATTRIBUTE:
		    xml_xsd10_free_wildcard((XmlXsd10Wildcard *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_IDC_KEY:
		case XML_XSD10_SCHEMA_TYPE_IDC_UNIQUE:
		case XML_XSD10_SCHEMA_TYPE_IDC_KEYREF:
		    xml_xsd10_free_idc((XmlXsd10IDC *) item);
		    break;
		case XML_XSD10_SCHEMA_TYPE_NOTATION:
		    xml_xsd10_free_notation((XmlXsd10Notation *) item);
		    break;
		case XML_XSD10_SCHEMA_EXTRA_QNAMEREF:
		    xml_xsd10_free_q_name_ref((XmlXsd10QNameRef *) item);
		    break;
		default:
		    /* TODO: This should never be hit. */
		    break;
	    }
	}
	list->nbItems = 0;
    }
}

/**
 * Port of xmlSchemaFree: deallocate a Schema structure.
 *
 * @param schema  a schema structure
 */
void xml_xsd10_free(XmlXsd10Schema *schema)
{
    if (schema == NULL)
        return;
    /*
    * Note that those slots are not responsible for freeing
    * schema components anymore; this will now be done by
    * the schema buckets.
    */
    if (schema->notaDecl != NULL)
        xml_xsd10_hash_free(schema->notaDecl, NULL);
    if (schema->attrDecl != NULL)
        xml_xsd10_hash_free(schema->attrDecl, NULL);
    if (schema->attrgrpDecl != NULL)
        xml_xsd10_hash_free(schema->attrgrpDecl, NULL);
    if (schema->elemDecl != NULL)
        xml_xsd10_hash_free(schema->elemDecl, NULL);
    if (schema->typeDecl != NULL)
        xml_xsd10_hash_free(schema->typeDecl, NULL);
    if (schema->groupDecl != NULL)
        xml_xsd10_hash_free(schema->groupDecl, NULL);
    if (schema->idcDef != NULL)
        xml_xsd10_hash_free(schema->idcDef, NULL);

    if (schema->schemasImports != NULL)
	xml_xsd10_hash_free(schema->schemasImports, xml_xsd10_bucket_free_entry);
    if (schema->includes != NULL) {
	XmlXsd10ItemList *list = (XmlXsd10ItemList *) schema->includes;
	int i;
	for (i = 0; i < list->nbItems; i++) {
	    xml_xsd10_bucket_free((XmlXsd10Bucket *) list->items[i]);
	}
	xml_xsd10_item_list_free(list);
    }
    if (schema->annot != NULL)
        xml_xsd10_free_annot(schema->annot);
    /* Never free the doc here, since this will be done by the buckets. */

    xml_xsd10_dict_free(schema->dict);
    free(schema);
}

/* The debug functions (xmlSchemaDump and friends) are not ported. */

/************************************************************************
 *									*
 *			Utilities					*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaGetPropNode.
 *
 * Seeks an attribute with a name of `name` in
 * no namespace.
 *
 * @param node  the element node
 * @param name  the name of the attribute
 * @returns the attribute (as a node) or NULL if not present.
 */
XMQNode *xml_xsd10_get_prop_node(XMQNode *node, const char *name)
{
    if ((node == NULL) || (name == NULL))
	return(NULL);
    for (XMQAttr *prop = xml_node_first_attr(node); prop != NULL; prop = xml_attr_next(prop)) {
        XMQNode *an = xml_attr_node(prop);
        if ((xsd10_schemas_ns_href(an) == NULL) && xml_xsd10_str_equal(xml_node_name(an), name))
	    return(an);
    }
    return (NULL);
}

/**
 * Port of xmlSchemaGetPropNodeNs.
 *
 * Seeks an attribute with a local name of `name` and
 * a namespace URI of `uri`.
 *
 * @param node  the element node
 * @param uri  the uri
 * @param name  the name of the attribute
 * @returns the attribute (as a node) or NULL if not present.
 */
XMQNode *xml_xsd10_get_prop_node_ns(XMQNode *node, const char *uri, const char *name)
{
    if ((node == NULL) || (name == NULL))
	return(NULL);
    for (XMQAttr *prop = xml_node_first_attr(node); prop != NULL; prop = xml_attr_next(prop)) {
        XMQNode *an = xml_attr_node(prop);
	if ((xsd10_schemas_ns_href(an) != NULL) &&
	    xml_xsd10_str_equal(xml_node_name(an), name) &&
	    xml_xsd10_str_equal(xsd10_schemas_ns_href(an), uri))
	    return(an);
    }
    return (NULL);
}

/** Port of xmlSchemaGetNodeContent: the content interned in the dict. */
const char *xml_xsd10_get_node_content(XmlXsd10ParserCtxt *ctxt, XMQNode *node)
{
    char *val;
    const char *ret;

    val = xml_xsd10_tree_node_get_content(node);
    if (val == NULL)
	val = xml_xsd10_strdup("");
    ret = xml_xsd10_dict_lookup(ctxt->dict, val, -1);
    free(val);
    if (ret == NULL)
        xml_xsd10_p_err_memory(ctxt);
    return(ret);
}

/** Port of xmlSchemaGetNodeContentNoDict: malloced. */
const char *xml_xsd10_get_node_content_no_dict(XMQNode *node)
{
    return((const char*) xml_xsd10_tree_node_get_content(node));
}

/**
 * Port of xmlSchemaGetProp: read a attribute value and internalize the string
 *
 * @param ctxt  the parser context
 * @param node  the node
 * @param name  the property name
 * @returns the string or NULL if not present.
 */
const char *xml_xsd10_get_prop(XmlXsd10ParserCtxt *ctxt, XMQNode *node, const char *name)
{
    char *val;
    const char *ret;

    val = xml_xsd10_tree_get_no_ns_prop(node, name);
    if (val == NULL)
        return(NULL);
    ret = xml_xsd10_dict_lookup(ctxt->dict, val, -1);
    free(val);
    return(ret);
}

/************************************************************************
 *									*
 *		The context lifecycle					*
 *									*
 ************************************************************************/

/** Port of xmlSchemaSubstGroupFree. */
void xml_xsd10_subst_group_free(XmlXsd10SubstGroup *group)
{
    if (group == NULL)
	return;
    if (group->members != NULL)
	xml_xsd10_item_list_free(group->members);
    free(group);
}

/** Port of xmlSchemaSubstGroupFreeEntry. */
void xml_xsd10_subst_group_free_entry(void *group, const char *name)
{
    (void) name;
    xml_xsd10_subst_group_free((XmlXsd10SubstGroup *) group);
}

/** Port of xmlSchemaRedefListFree. */
void xml_xsd10_redef_list_free(XmlXsd10Redef *redef)
{
    XmlXsd10Redef *prev;

    while (redef != NULL) {
	prev = redef;
	redef = redef->next;
	free(prev);
    }
}

/** Port of xmlSchemaConstructionCtxtFree. */
void xml_xsd10_construction_ctxt_free(XmlXsd10ConstructionCtxt *con)
{
    /*
    * After the construction context has been freed, there will be
    * no schema graph available any more. Only the schema buckets
    * will stay alive, which are put into the "schemasImports" and
    * "includes" slots of the xmlSchema.
    */
    if (con->buckets != NULL)
	xml_xsd10_item_list_free(con->buckets);
    if (con->pending != NULL)
	xml_xsd10_item_list_free(con->pending);
    if (con->substGroups != NULL)
	xml_xsd10_hash_free(con->substGroups, xml_xsd10_subst_group_free_entry);
    if (con->redefs != NULL)
	xml_xsd10_redef_list_free(con->redefs);
    if (con->dict != NULL)
	xml_xsd10_dict_free(con->dict);
    free(con);
}

/** Port of xmlSchemaConstructionCtxtCreate. */
XmlXsd10ConstructionCtxt *xml_xsd10_construction_ctxt_create(XmlXsd10Dict *dict)
{
    XmlXsd10ConstructionCtxt *ret;

    ret = (XmlXsd10ConstructionCtxt *)
	malloc(sizeof(XmlXsd10ConstructionCtxt));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(NULL);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ConstructionCtxt));

    ret->buckets = xml_xsd10_item_list_create();
    if (ret->buckets == NULL) {
	xml_xsd10_p_err_memory(NULL);
	free(ret);
        return (NULL);
    }
    ret->pending = xml_xsd10_item_list_create();
    if (ret->pending == NULL) {
	xml_xsd10_p_err_memory(NULL);
	xml_xsd10_construction_ctxt_free(ret);
        return (NULL);
    }
    ret->dict = dict;
    xml_xsd10_dict_reference(dict);
    return(ret);
}

/** Port of xmlSchemaParserCtxtCreate (also installs the regexp hooks of xml_xsd10_types). */
XmlXsd10ParserCtxt *xml_xsd10_parser_ctxt_create(void)
{
    XmlXsd10ParserCtxt *ret;

    xml_xsd10_install_regexp_hooks();
    ret = (XmlXsd10ParserCtxt *) malloc(sizeof(XmlXsd10ParserCtxt));
    if (ret == NULL) {
        xml_xsd10_p_err_memory(NULL);
        return (NULL);
    }
    memset(ret, 0, sizeof(XmlXsd10ParserCtxt));
    ret->type = XML_XSD10_SCHEMA_CTXT_PARSER;
    ret->attrProhibs = xml_xsd10_item_list_create();
    if (ret->attrProhibs == NULL) {
	free(ret);
	return(NULL);
    }
    return(ret);
}

/**
 * Port of xmlSchemaNewParserCtxtUseDict.
 *
 * Create an XML Schemas parse context for that file/resource expected
 * to contain an XML Schemas file.
 *
 * @param URL  the location of the schema
 * @param dict  the dictionary to be used
 * @returns the parser context or NULL in case of error
 */
XmlXsd10ParserCtxt *xml_xsd10_new_parser_ctxt_use_dict(const char *URL, XmlXsd10Dict *dict)
{
    XmlXsd10ParserCtxt *ret;

    ret = xml_xsd10_parser_ctxt_create();
    if (ret == NULL)
        return (NULL);
    ret->dict = dict;
    xml_xsd10_dict_reference(dict);
    if (URL != NULL)
	ret->URL = xml_xsd10_dict_lookup(dict, (const char *) URL, -1);
    return (ret);
}

/** Port of xmlSchemaCreatePCtxtOnVCtxt. */
int xml_xsd10_create_p_ctxt_on_v_ctxt(XmlXsd10ValidCtxt *vctxt)
{
    if (vctxt->pctxt == NULL) {
        if (vctxt->schema != NULL)
	    vctxt->pctxt =
		xml_xsd10_new_parser_ctxt_use_dict("*", vctxt->schema->dict);
	else
	    vctxt->pctxt = xml_xsd10_new_parser_ctxt("*");
	if (vctxt->pctxt == NULL) {
	    XSD10_VERROR_INT("xmlSchemaCreatePCtxtOnVCtxt",
		"failed to create a temp. parser context");
	    return (-1);
	}
	/* TODO: Pass user data. */
	xml_xsd10_set_parser_errors(vctxt->pctxt, vctxt->error,
	    vctxt->warning, vctxt->errCtxt);
	xml_xsd10_set_parser_structured_errors(vctxt->pctxt, vctxt->serror,
	    vctxt->errCtxt);
    }
    return (0);
}

/************************************************************************
 *									*
 *			Reading/Writing Schemas				*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaNewParserCtxt.
 *
 * Create an XML Schemas parse context for that file/resource expected
 * to contain an XML Schemas file.
 *
 * @param URL  the location of the schema
 * @returns the parser context or NULL in case of error
 */
XmlXsd10ParserCtxt *xml_xsd10_new_parser_ctxt(const char *URL)
{
    XmlXsd10ParserCtxt *ret;

    if (URL == NULL)
        return (NULL);

    ret = xml_xsd10_parser_ctxt_create();
    if (ret == NULL)
	return(NULL);
    ret->dict = xml_xsd10_dict_create();
    ret->URL = xml_xsd10_dict_lookup(ret->dict, (const char *) URL, -1);
    return (ret);
}

/**
 * Port of xmlSchemaNewMemParserCtxt.
 *
 * Create an XML Schemas parse context for that memory buffer expected
 * to contain an XML Schemas file.
 *
 * @param buffer  a pointer to a char array containing the schemas
 * @param size  the size of the array
 * @returns the parser context or NULL in case of error
 */
XmlXsd10ParserCtxt *xml_xsd10_new_mem_parser_ctxt(const char *buffer, int size)
{
    XmlXsd10ParserCtxt *ret;

    if ((buffer == NULL) || (size <= 0))
        return (NULL);
    ret = xml_xsd10_parser_ctxt_create();
    if (ret == NULL)
	return(NULL);
    ret->buffer = buffer;
    ret->size = size;
    ret->dict = xml_xsd10_dict_create();
    return (ret);
}

/**
 * Port of xmlSchemaNewDocParserCtxt.
 *
 * Create an XML Schemas parse context for that document.
 * NB. The document may be modified during the parsing process.
 *
 * @param doc  a preparsed document tree
 * @returns the parser context or NULL in case of error
 */
XmlXsd10ParserCtxt *xml_xsd10_new_doc_parser_ctxt(XmlDom *doc)
{
    XmlXsd10ParserCtxt *ret;

    if (doc == NULL)
      return (NULL);
    ret = xml_xsd10_parser_ctxt_create();
    if (ret == NULL)
	return(NULL);
    ret->doc = doc;
    ret->dict = xml_xsd10_dict_create();
    /* The application has responsibility for the document */
    ret->preserve = 1;

    return (ret);
}

/**
 * Port of xmlSchemaFreeParserCtxt.
 *
 * Free the resources associated to the schema parser context
 *
 * @param ctxt  the schema parser context
 */
void xml_xsd10_free_parser_ctxt(XmlXsd10ParserCtxt *ctxt)
{
    if (ctxt == NULL)
        return;
    if (ctxt->doc != NULL && !ctxt->preserve)
        xml_dom_free(ctxt->doc);
    if (ctxt->vctxt != NULL) {
	xml_xsd10_free_valid_ctxt(ctxt->vctxt);
    }
    if (ctxt->ownsConstructor && (ctxt->constructor != NULL)) {
	xml_xsd10_construction_ctxt_free(ctxt->constructor);
	ctxt->constructor = NULL;
	ctxt->ownsConstructor = 0;
    }
    if (ctxt->attrProhibs != NULL)
	xml_xsd10_item_list_free(ctxt->attrProhibs);
    xml_xsd10_dict_free(ctxt->dict);
    free(ctxt);
}

/**
 * Port of xmlSchemaSetParserErrors.
 *
 * Set the callback functions used to handle errors for a validation context
 *
 * @param ctxt  a schema validation context
 * @param err  the error callback
 * @param warn  the warning callback
 * @param ctx  contextual data for the callbacks
 */
void xml_xsd10_set_parser_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10ValidityErrorFunc err, XmlXsd10ValidityWarningFunc warn, void *ctx)
{
    if (ctxt == NULL)
        return;
    ctxt->error = err;
    ctxt->warning = warn;
    ctxt->errCtxt = ctx;
    if (ctxt->vctxt != NULL)
	xml_xsd10_set_valid_errors(ctxt->vctxt, err, warn, ctx);
}

/**
 * Port of xmlSchemaSetParserStructuredErrors.
 *
 * Set the structured error callback
 *
 * @param ctxt  a schema parser context
 * @param serror  the structured error function
 * @param ctx  the functions context
 */
void xml_xsd10_set_parser_structured_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10StructuredErrorFunc serror, void *ctx)
{
    if (ctxt == NULL)
	return;
    ctxt->serror = serror;
    ctxt->errCtxt = ctx;
    if (ctxt->vctxt != NULL)
	xml_xsd10_set_valid_structured_errors(ctxt->vctxt, serror, ctx);
}

/**
 * Port of xmlSchemaGetParserErrors.
 *
 * Get the callback information used to handle errors for a parser context
 *
 * @param ctxt  a XMl-Schema parser context
 * @param err  the error callback result
 * @param warn  the warning callback result
 * @param ctx  contextual data for the callbacks result
 * @returns -1 in case of failure, 0 otherwise
 */
int xml_xsd10_get_parser_errors(XmlXsd10ParserCtxt *ctxt, XmlXsd10ValidityErrorFunc *err, XmlXsd10ValidityWarningFunc *warn, void **ctx)
{
	if (ctxt == NULL)
		return(-1);
	if (err != NULL)
		*err = ctxt->error;
	if (warn != NULL)
		*warn = ctxt->warning;
	if (ctx != NULL)
		*ctx = ctxt->errCtxt;
	return(0);
}

/**
 * Port of xmlSchemaSetResourceLoader.
 *
 * Register a callback function that will be called to load documents
 * or external entities.
 *
 * @param ctxt  schema parser
 * @param loader  resource loader
 * @param data  user data which will be passed to the loader
 */
void xml_xsd10_set_resource_loader(XmlXsd10ParserCtxt *ctxt, XmlXsd10Loader loader, void *data)
{
    if (ctxt == NULL)
        return;
    ctxt->resourceLoader = loader;
    ctxt->resourceCtxt = data;
}

/**
 * Port of xmlSchemaFacetTypeToString.
 *
 * Convert the xmlSchemaTypeType to a char string.
 *
 * @param type  the facet type
 * @returns the char string representation of the facet type if the
 *     type is a facet and an "Internal Error" string otherwise.
 */
const char *xml_xsd10_facet_type_to_string(XmlXsd10TypeType type)
{
    switch (type) {
        case XML_XSD10_SCHEMA_FACET_PATTERN:
            return ("pattern");
        case XML_XSD10_SCHEMA_FACET_MAXEXCLUSIVE:
            return ("maxExclusive");
        case XML_XSD10_SCHEMA_FACET_MAXINCLUSIVE:
            return ("maxInclusive");
        case XML_XSD10_SCHEMA_FACET_MINEXCLUSIVE:
            return ("minExclusive");
        case XML_XSD10_SCHEMA_FACET_MININCLUSIVE:
            return ("minInclusive");
        case XML_XSD10_SCHEMA_FACET_WHITESPACE:
            return ("whiteSpace");
        case XML_XSD10_SCHEMA_FACET_ENUMERATION:
            return ("enumeration");
        case XML_XSD10_SCHEMA_FACET_LENGTH:
            return ("length");
        case XML_XSD10_SCHEMA_FACET_MAXLENGTH:
            return ("maxLength");
        case XML_XSD10_SCHEMA_FACET_MINLENGTH:
            return ("minLength");
        case XML_XSD10_SCHEMA_FACET_TOTALDIGITS:
            return ("totalDigits");
        case XML_XSD10_SCHEMA_FACET_FRACTIONDIGITS:
            return ("fractionDigits");
        default:
            break;
    }
    return ("Internal Error");
}

/** Port of xmlSchemaGetWhiteSpaceFacetValue. */
XmlXsd10WhitespaceValueType xml_xsd10_get_white_space_facet_value(XmlXsd10Type *type)
{
    /*
    * The normalization type can be changed only for types which are derived
    * from xsd:string.
    */
    if (type->type == XML_XSD10_SCHEMA_TYPE_BASIC) {
	/*
	* Note that we assume a whitespace of preserve for anySimpleType.
	*/
	if ((type->builtInType == XML_XSD10_SCHEMAS_STRING) ||
	    (type->builtInType == XML_XSD10_SCHEMAS_ANYSIMPLETYPE))
	    return(XML_XSD10_SCHEMA_WHITESPACE_PRESERVE);
	else if (type->builtInType == XML_XSD10_SCHEMAS_NORMSTRING)
	    return(XML_XSD10_SCHEMA_WHITESPACE_REPLACE);
	else {
	    /*
	    * For all `atomic` datatypes other than string (and types `derived`
	    * by `restriction` from it) the value of whiteSpace is fixed to
	    * collapse
	    * Note that this includes built-in list datatypes.
	    */
	    return(XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE);
	}
    } else if (XSD10_WXS_IS_LIST(type)) {
	/*
	* For list types the facet "whiteSpace" is fixed to "collapse".
	*/
	return (XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE);
    } else if (XSD10_WXS_IS_UNION(type)) {
	return (XML_XSD10_SCHEMA_WHITESPACE_UNKNOWN);
    } else if (XSD10_WXS_IS_ATOMIC(type)) {
	if (type->flags & XML_XSD10_SCHEMAS_TYPE_WHITESPACE_PRESERVE)
	    return (XML_XSD10_SCHEMA_WHITESPACE_PRESERVE);
	else if (type->flags & XML_XSD10_SCHEMAS_TYPE_WHITESPACE_REPLACE)
	    return (XML_XSD10_SCHEMA_WHITESPACE_REPLACE);
	else
	    return (XML_XSD10_SCHEMA_WHITESPACE_COLLAPSE);
    }
    return ((XmlXsd10WhitespaceValueType) -1);
}

/************************************************************************
 *									*
 *		The validation context setters				*
 *									*
 ************************************************************************/

/**
 * Port of xmlSchemaIsValid.
 *
 * Check if any error was detected during validation.
 *
 * @param ctxt  the schema validation context
 * @returns 1 if valid so far, 0 if errors were detected, and -1 in case
 *         of internal error.
 */
int xml_xsd10_is_valid(XmlXsd10ValidCtxt *ctxt)
{
    if (ctxt == NULL)
        return(-1);
    return(ctxt->err == 0);
}

/**
 * Port of xmlSchemaSetValidErrors.
 *
 * Set the error and warning callback information
 *
 * @param ctxt  a schema validation context
 * @param err  the error function
 * @param warn  the warning function
 * @param ctx  the functions context
 */
void xml_xsd10_set_valid_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10ValidityErrorFunc err, XmlXsd10ValidityWarningFunc warn, void *ctx)
{
    if (ctxt == NULL)
        return;
    ctxt->error = err;
    ctxt->warning = warn;
    ctxt->errCtxt = ctx;
    if (ctxt->pctxt != NULL)
	xml_xsd10_set_parser_errors(ctxt->pctxt, err, warn, ctx);
}

/**
 * Port of xmlSchemaSetValidStructuredErrors.
 *
 * Set the structured error callback
 *
 * @param ctxt  a schema validation context
 * @param serror  the structured error function
 * @param ctx  the functions context
 */
void xml_xsd10_set_valid_structured_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10StructuredErrorFunc serror, void *ctx)
{
    if (ctxt == NULL)
        return;
    ctxt->serror = serror;
    ctxt->error = NULL;
    ctxt->warning = NULL;
    ctxt->errCtxt = ctx;
    if (ctxt->pctxt != NULL)
	xml_xsd10_set_parser_structured_errors(ctxt->pctxt, serror, ctx);
}

/**
 * Port of xmlSchemaGetValidErrors.
 *
 * Get the error and warning callback information
 *
 * @param ctxt  a XML-Schema validation context
 * @param err  the error function result
 * @param warn  the warning function result
 * @param ctx  the functions context result
 * @returns -1 in case of error and 0 otherwise
 */
int xml_xsd10_get_valid_errors(XmlXsd10ValidCtxt *ctxt, XmlXsd10ValidityErrorFunc *err, XmlXsd10ValidityWarningFunc *warn, void **ctx)
{
	if (ctxt == NULL)
		return (-1);
	if (err != NULL)
		*err = ctxt->error;
	if (warn != NULL)
		*warn = ctxt->warning;
	if (ctx != NULL)
		*ctx = ctxt->errCtxt;
	return (0);
}


/**
 * Port of xmlSchemaSetValidOptions.
 *
 * Sets the options to be used during the validation.
 *
 * @param ctxt  a schema validation context
 * @param options  a combination of xmlSchemaValidOption
 * @returns 0 in case of success, -1 in case of an
 * API error.
 */
int xml_xsd10_set_valid_options(XmlXsd10ValidCtxt *ctxt, int options)
{
    int i;

    if (ctxt == NULL)
	return (-1);
    /*
    * WARNING: Change the start value if adding to the
    * xmlSchemaValidOption.
    * TODO: Is there an other, more easy to maintain,
    * way?
    */
    for (i = 1; i < (int) sizeof(int) * 8; i++) {
        if ((unsigned) options & (1U << i))
	    return (-1);
    }
    ctxt->options = options;
    return (0);
}

/**
 * Port of xmlSchemaValidCtxtGetOptions.
 *
 * Get the validation context options.
 *
 * @param ctxt  a schema validation context
 * @returns the option combination or -1 on error.
 */
int xml_xsd10_valid_ctxt_get_options(XmlXsd10ValidCtxt *ctxt)
{
    if (ctxt == NULL)
	return (-1);
    else
	return (ctxt->options);
}

/************************************************************************
 *									*
 *	TEMPORARY STUBS of functions of the other xml_xsd10 parts	*
 *	called from this part. Remove each stub when its part		*
 *	implements the function.					*
 *									*
 ************************************************************************/

#undef XSD10_SCHEMAS_IS_BLANK_CH

#endif // XML_XSD10_SCHEMAS_MODULE
