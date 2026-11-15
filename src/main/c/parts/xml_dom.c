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

#ifndef BUILDING_DIST_XMQ

#include"xml_dom.h"

#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_DOM_MODULE

// The data structures are private to this file, see xml_dom.h.
//
// The behaviour (text merging, attribute values as child nodes, entity references,
// dtd model, namespace search rules) is modelled after libxml2's tree.c
// (Copyright (C) 1998-2012 Daniel Veillard, Copyright (C) The Libxml2 Contributors,
// MIT license) so that xmq output stays the same. The code is written for the
// native dom and is not a port of tree.c.

struct XmlNs
{
    XmlNs *next;    // Next declaration on the same element (only used in ns_defs lists).
    char *prefix;   // NULL for the default namespace.
    char *href;     // "" for an undeclaration xmlns="", NULL for a prefix declared without uri.
    int refcount;   // Number of owners: declaring element + nodes using it. <0 means static.
    void *user_data; // Free for the user of the dom (not copied).
};

struct XMQNode
{
    XmlNodeKind kind;
    XMQNode *parent;
    XMQNode *first_child;
    XMQNode *last_child;
    XMQNode *next;
    XMQNode *prev;
    XmlDom *dom;
    char *name;
    char *content;
    XmlNs *ns;           // Namespace of element or attribute (a reference is held).
    XmlNs *ns_defs;      // Namespaces declared on this element (owned).
    XMQAttr *first_attr;
    XMQAttr *last_attr;
    int line;
    void *user_data;
};

struct XMQAttr
{
    XMQNode node; // node.kind == XML_DOM_ATTRIBUTE, node.parent is the owner element.
};

typedef struct
{
    XMQNode node;            // node.kind == XML_DOM_DTD, node.name is the doctype name.
    char *public_id;
    char *system_id;
    XMQNode *first_notation; // Notation declarations, linked with next/prev, parent is NULL.
    XMQNode *last_notation;
} XmlDtd;

typedef struct
{
    XMQNode node;            // node.kind == XML_DOM_ENTITY_DECL, content = value with char refs expanded.
    XmlEntityType etype;
    char *orig;
    char *public_id;
    char *system_id;
    char *notation;
} XmlEntityDecl;

typedef struct
{
    XMQNode node;            // node.kind == XML_DOM_ELEMENT_DECL/ATTRIBUTE_DECL/NOTATION_DECL
    char *default_value;
} XmlDtdDecl;

struct XmlDom
{
    XMQNode document;
    char *version;
    char *encoding;
    char *url;           // Where the document was loaded from, for messages and document().
    int standalone;
    XmlNs xml_ns;        // The predefined xml: namespace, static.
};

// Internal helpers ///////////////////////////////////////////////

static char *xml_dom_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *r = (char*)malloc(len+1);
    memcpy(r, s, len+1);
    return r;
}

static char *xml_dom_strndup(const char *s, size_t len)
{
    if (!s) return NULL;
    char *r = (char*)malloc(len+1);
    memcpy(r, s, len);
    r[len] = 0;
    return r;
}

static bool xml_dom_streq(const char *a, const char *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return !strcmp(a, b);
}

static XmlNs *xml_ns_new(const char *prefix, const char *href)
{
    XmlNs *ns = (XmlNs*)calloc(1, sizeof(XmlNs));
    ns->prefix = xml_dom_strdup(prefix);
    // A NULL href is a prefix declared without uri (the xmq parser does that for
    // undeclared prefixes), it is not serialized. "" is an undeclaration.
    ns->href = href ? xml_dom_strdup(href) : NULL;
    ns->refcount = 1;
    return ns;
}

static XmlNs *xml_ns_ref(XmlNs *ns)
{
    if (ns && ns->refcount >= 0) ns->refcount++;
    return ns;
}

static void xml_ns_unref(XmlNs *ns)
{
    if (!ns || ns->refcount < 0) return;
    assert(ns->refcount > 0);
    if (--ns->refcount == 0)
    {
        free(ns->prefix);
        free(ns->href);
        free(ns);
    }
}

typedef struct
{
    char *buf;
    size_t len;
    size_t cap;
} XmlDomStrBuf;

static void xml_dom_sb_add(XmlDomStrBuf *sb, const char *s, size_t len)
{
    if (sb->len + len + 1 > sb->cap)
    {
        size_t nc = sb->cap ? sb->cap * 2 : 64;
        while (nc < sb->len + len + 1) nc *= 2;
        sb->buf = (char*)realloc(sb->buf, nc);
        sb->cap = nc;
    }
    memcpy(sb->buf + sb->len, s, len);
    sb->len += len;
    sb->buf[sb->len] = 0;
}

static XMQNode *xml_node_alloc(XmlDom *dom, XmlNodeKind kind, size_t size)
{
    XMQNode *n = (XMQNode*)calloc(1, size);
    n->kind = kind;
    n->dom = dom;
    return n;
}

static size_t xml_node_size(XmlNodeKind kind)
{
    switch (kind)
    {
    case XML_DOM_ATTRIBUTE: return sizeof(XMQAttr);
    case XML_DOM_DTD: return sizeof(XmlDtd);
    case XML_DOM_ENTITY_DECL: return sizeof(XmlEntityDecl);
    case XML_DOM_ELEMENT_DECL:
    case XML_DOM_ATTRIBUTE_DECL:
    case XML_DOM_NOTATION_DECL: return sizeof(XmlDtdDecl);
    default: return sizeof(XMQNode);
    }
}

static void xml_node_set_dom_recursive(XMQNode *n, XmlDom *dom)
{
    n->dom = dom;
    for (XMQAttr *a = n->first_attr; a; a = (XMQAttr*)a->node.next) xml_node_set_dom_recursive(&a->node, dom);
    for (XMQNode *c = n->first_child; c; c = c->next) xml_node_set_dom_recursive(c, dom);
    if (n->kind == XML_DOM_DTD)
    {
        for (XMQNode *c = ((XmlDtd*)n)->first_notation; c; c = c->next) c->dom = dom;
    }
}

static void xml_node_free_contents(XMQNode *n);

static void xml_node_free_unlinked(XMQNode *n)
{
    xml_node_free_contents(n);
    free(n);
}

static void xml_node_free_contents(XMQNode *n)
{
    XMQNode *c = n->first_child;
    while (c)
    {
        XMQNode *next = c->next;
        xml_node_free_unlinked(c);
        c = next;
    }
    n->first_child = n->last_child = NULL;

    XMQAttr *a = n->first_attr;
    while (a)
    {
        XMQAttr *next = (XMQAttr*)a->node.next;
        xml_node_free_unlinked(&a->node);
        a = next;
    }
    n->first_attr = n->last_attr = NULL;

    XmlNs *d = n->ns_defs;
    while (d)
    {
        XmlNs *next = d->next;
        d->next = NULL;
        xml_ns_unref(d);
        d = next;
    }
    n->ns_defs = NULL;

    xml_ns_unref(n->ns);
    n->ns = NULL;

    switch (n->kind)
    {
    case XML_DOM_DTD:
    {
        XmlDtd *dtd = (XmlDtd*)n;
        free(dtd->public_id);
        free(dtd->system_id);
        XMQNode *i = dtd->first_notation;
        while (i)
        {
            XMQNode *next = i->next;
            xml_node_free_unlinked(i);
            i = next;
        }
        dtd->public_id = dtd->system_id = NULL;
        dtd->first_notation = dtd->last_notation = NULL;
        break;
    }
    case XML_DOM_ENTITY_DECL:
    {
        XmlEntityDecl *e = (XmlEntityDecl*)n;
        free(e->orig);
        free(e->public_id);
        free(e->system_id);
        free(e->notation);
        e->orig = e->public_id = e->system_id = e->notation = NULL;
        break;
    }
    case XML_DOM_ELEMENT_DECL:
    case XML_DOM_ATTRIBUTE_DECL:
    case XML_DOM_NOTATION_DECL:
        free(((XmlDtdDecl*)n)->default_value);
        ((XmlDtdDecl*)n)->default_value = NULL;
        break;
    default:
        break;
    }

    free(n->name);
    n->name = NULL;
    free(n->content);
    n->content = NULL;
}

// Dom ////////////////////////////////////////////////////////////

XmlDom *xml_dom_new()
{
    XmlDom *dom = (XmlDom*)calloc(1, sizeof(XmlDom));
    dom->document.kind = XML_DOM_DOCUMENT;
    dom->document.dom = dom;
    dom->standalone = -1;
    dom->xml_ns.prefix = (char*)"xml";
    dom->xml_ns.href = (char*)XML_DOM_XML_NAMESPACE;
    dom->xml_ns.refcount = -1; // Static, never freed.
    return dom;
}

void xml_dom_free(XmlDom *dom)
{
    if (!dom) return;
    xml_node_free_contents(&dom->document);
    free(dom->version);
    free(dom->encoding);
    free(dom->url);
    free(dom);
}

const char *xml_dom_url(const XmlDom *dom)
{
    return dom->url;
}

void xml_dom_set_url(XmlDom *dom, const char *url)
{
    char *n = xml_dom_strdup(url);
    free(dom->url);
    dom->url = n;
}

XMQNode *xml_dom_document(XmlDom *dom)
{
    return &dom->document;
}

const char *xml_dom_version(const XmlDom *dom)
{
    return dom->version;
}

void xml_dom_set_version(XmlDom *dom, const char *version)
{
    char *v = xml_dom_strdup(version);
    free(dom->version);
    dom->version = v;
}

const char *xml_dom_encoding(const XmlDom *dom)
{
    return dom->encoding;
}

void xml_dom_set_encoding(XmlDom *dom, const char *encoding)
{
    char *e = xml_dom_strdup(encoding);
    free(dom->encoding);
    dom->encoding = e;
}

int xml_dom_standalone(const XmlDom *dom)
{
    return dom->standalone;
}

void xml_dom_set_standalone(XmlDom *dom, int standalone)
{
    dom->standalone = standalone < 0 ? -1 : (standalone ? 1 : 0);
}


XMQNode *xml_dom_root_element(XmlDom *dom)
{
    for (XMQNode *n = dom->document.first_child; n; n = n->next)
    {
        if (n->kind == XML_DOM_ELEMENT) return n;
    }
    return NULL;
}

void xml_dom_set_root_element(XmlDom *dom, XMQNode *root)
{
    XMQNode *old = xml_dom_root_element(dom);
    if (old == root) return;
    if (old)
    {
        xml_node_replace(old, root);
        xml_node_free(old);
    }
    else
    {
        xml_node_append_child(&dom->document, root);
    }
}

// DTD ////////////////////////////////////////////////////////////

static XmlDtd *xml_dom_dtd_internal(XmlDom *dom)
{
    for (XMQNode *n = dom->document.first_child; n; n = n->next)
    {
        if (n->kind == XML_DOM_DTD) return (XmlDtd*)n;
    }
    return NULL;
}

XMQNode *xml_dom_dtd(XmlDom *dom)
{
    XmlDtd *dtd = xml_dom_dtd_internal(dom);
    return dtd ? &dtd->node : NULL;
}

const char *xml_dtd_public_id(const XMQNode *dtd)
{
    assert(dtd->kind == XML_DOM_DTD);
    return ((const XmlDtd*)dtd)->public_id;
}

const char *xml_dtd_system_id(const XMQNode *dtd)
{
    assert(dtd->kind == XML_DOM_DTD);
    return ((const XmlDtd*)dtd)->system_id;
}

XMQNode *xml_dtd_first_notation(const XMQNode *dtd)
{
    assert(dtd->kind == XML_DOM_DTD);
    return ((const XmlDtd*)dtd)->first_notation;
}

XMQNode *xml_dom_set_dtd(XmlDom *dom, const char *name, const char *public_id, const char *system_id)
{
    XmlDtd *old = xml_dom_dtd_internal(dom);
    if (old) xml_node_free(&old->node);

    XmlDtd *dtd = (XmlDtd*)xml_node_alloc(dom, XML_DOM_DTD, sizeof(XmlDtd));
    dtd->node.name = xml_dom_strdup(name);
    dtd->public_id = xml_dom_strdup(public_id);
    dtd->system_id = xml_dom_strdup(system_id);

    XMQNode *root = xml_dom_root_element(dom);
    if (root) xml_node_add_prev_sibling(root, &dtd->node);
    else xml_node_append_child(&dom->document, &dtd->node);
    return &dtd->node;
}

static bool xml_entity_is_parameter(XmlEntityType t)
{
    return t == XML_ENTITY_INTERNAL_PARAMETER || t == XML_ENTITY_EXTERNAL_PARAMETER;
}

static XmlEntityDecl *xml_dtd_find_entity(const XmlDtd *dtd, const char *name, bool parameter)
{
    if (!dtd || !name) return NULL;
    for (XMQNode *n = dtd->node.first_child; n; n = n->next)
    {
        if (n->kind != XML_DOM_ENTITY_DECL) continue;
        XmlEntityDecl *e = (XmlEntityDecl*)n;
        if (xml_entity_is_parameter(e->etype) == parameter && xml_dom_streq(n->name, name)) return e;
    }
    return NULL;
}

XMQNode *xml_dtd_add_entity(XMQNode *dtd_node, const char *name, XmlEntityType etype,
                            const char *public_id, const char *system_id,
                            const char *notation, const char *content, const char *orig)
{
    assert(dtd_node->kind == XML_DOM_DTD);
    XmlDtd *dtd = (XmlDtd*)dtd_node;
    if (xml_dtd_find_entity(dtd, name, xml_entity_is_parameter(etype))) return NULL;
    XmlEntityDecl *e = (XmlEntityDecl*)xml_node_alloc(dtd->node.dom, XML_DOM_ENTITY_DECL, sizeof(XmlEntityDecl));
    e->node.name = xml_dom_strdup(name);
    e->node.content = xml_dom_strdup(content);
    e->etype = etype;
    e->orig = xml_dom_strdup(orig);
    e->public_id = xml_dom_strdup(public_id);
    e->system_id = xml_dom_strdup(system_id);
    e->notation = xml_dom_strdup(notation);
    xml_node_append_child(&dtd->node, &e->node);
    return &e->node;
}

XMQNode *xml_dtd_add_decl(XMQNode *dtd_node, XmlNodeKind kind, const char *name, const char *text, const char *default_value)
{
    assert(dtd_node->kind == XML_DOM_DTD);
    XmlDtd *dtd = (XmlDtd*)dtd_node;
    assert(kind == XML_DOM_ELEMENT_DECL || kind == XML_DOM_ATTRIBUTE_DECL);
    XmlDtdDecl *d = (XmlDtdDecl*)xml_node_alloc(dtd->node.dom, kind, sizeof(XmlDtdDecl));
    d->node.name = xml_dom_strdup(name);
    d->node.content = xml_dom_strdup(text);
    d->default_value = xml_dom_strdup(default_value);
    xml_node_append_child(&dtd->node, &d->node);
    return &d->node;
}

static XmlDtdDecl *xml_dtd_add_notation_internal(XmlDtd *dtd, const char *name, const char *text)
{
    for (XMQNode *n = dtd->first_notation; n; n = n->next)
    {
        if (xml_dom_streq(n->name, name)) return NULL;
    }
    XmlDtdDecl *d = (XmlDtdDecl*)xml_node_alloc(dtd->node.dom, XML_DOM_NOTATION_DECL, sizeof(XmlDtdDecl));
    d->node.name = xml_dom_strdup(name);
    d->node.content = xml_dom_strdup(text);
    if (dtd->last_notation)
    {
        dtd->last_notation->next = &d->node;
        d->node.prev = dtd->last_notation;
        dtd->last_notation = &d->node;
    }
    else
    {
        dtd->first_notation = dtd->last_notation = &d->node;
    }
    return d;
}

XMQNode *xml_dtd_add_notation(XMQNode *dtd, const char *name, const char *text)
{
    assert(dtd->kind == XML_DOM_DTD);
    XmlDtdDecl *d = xml_dtd_add_notation_internal((XmlDtd*)dtd, name, text);
    return d ? &d->node : NULL;
}

XMQNode *xml_dtd_add_pe_ref(XMQNode *dtd, const char *name)
{
    assert(dtd->kind == XML_DOM_DTD);
    XMQNode *n = xml_node_alloc(dtd->dom, XML_DOM_PE_REF, sizeof(XMQNode));
    n->name = xml_dom_strdup(name);
    xml_node_append_child(dtd, n);
    return n;
}

bool xml_dtd_has_declarations(const XMQNode *dtd_node)
{
    assert(dtd_node->kind == XML_DOM_DTD);
    const XmlDtd *dtd = (const XmlDtd*)dtd_node;
    if (dtd->first_notation) return true;
    for (XMQNode *n = dtd->node.first_child; n; n = n->next)
    {
        if (n->kind == XML_DOM_ENTITY_DECL || n->kind == XML_DOM_ELEMENT_DECL || n->kind == XML_DOM_ATTRIBUTE_DECL ||
            n->kind == XML_DOM_PE_REF) return true;
    }
    return false;
}

static XmlEntityDecl *xml_dom_get_entity_internal(XmlDom *dom, const char *name)
{
    return xml_dtd_find_entity(xml_dom_dtd_internal(dom), name, false);
}

XMQNode *xml_dom_get_entity(XmlDom *dom, const char *name)
{
    XmlEntityDecl *e = xml_dom_get_entity_internal(dom, name);
    return e ? &e->node : NULL;
}

XMQNode *xml_dom_get_parameter_entity(XmlDom *dom, const char *name)
{
    XmlEntityDecl *e = xml_dtd_find_entity(xml_dom_dtd_internal(dom), name, true);
    return e ? &e->node : NULL;
}

XmlEntityType xml_entity_decl_type(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ENTITY_DECL);
    return ((const XmlEntityDecl*)decl)->etype;
}

const char *xml_entity_decl_orig(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ENTITY_DECL);
    return ((const XmlEntityDecl*)decl)->orig;
}

const char *xml_entity_decl_public_id(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ENTITY_DECL);
    return ((const XmlEntityDecl*)decl)->public_id;
}

const char *xml_entity_decl_system_id(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ENTITY_DECL);
    return ((const XmlEntityDecl*)decl)->system_id;
}

const char *xml_entity_decl_notation(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ENTITY_DECL);
    return ((const XmlEntityDecl*)decl)->notation;
}

const char *xml_decl_default_value(const XMQNode *decl)
{
    assert(decl->kind == XML_DOM_ELEMENT_DECL || decl->kind == XML_DOM_ATTRIBUTE_DECL || decl->kind == XML_DOM_NOTATION_DECL);
    return ((const XmlDtdDecl*)decl)->default_value;
}

// Node creation //////////////////////////////////////////////////

XMQNode *xml_node_new_element(XmlDom *dom, const char *name, XmlNs *ns)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_ELEMENT, sizeof(XMQNode));
    n->name = xml_dom_strdup(name);
    n->ns = xml_ns_ref(ns);
    return n;
}

XMQNode *xml_node_new_text(XmlDom *dom, const char *text)
{
    return xml_node_new_text_len(dom, text ? text : "", text ? strlen(text) : 0);
}

XMQNode *xml_node_new_text_len(XmlDom *dom, const char *text, size_t len)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_TEXT, sizeof(XMQNode));
    n->content = xml_dom_strndup(text ? text : "", text ? len : 0);
    return n;
}

XMQNode *xml_node_new_cdata(XmlDom *dom, const char *text, size_t len)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_CDATA, sizeof(XMQNode));
    n->content = xml_dom_strndup(text ? text : "", text ? len : 0);
    return n;
}

XMQNode *xml_node_new_comment(XmlDom *dom, const char *text)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_COMMENT, sizeof(XMQNode));
    n->content = xml_dom_strdup(text);
    return n;
}

XMQNode *xml_node_new_pi(XmlDom *dom, const char *target, const char *data)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_PI, sizeof(XMQNode));
    n->name = xml_dom_strdup(target);
    n->content = xml_dom_strdup(data);
    return n;
}

XMQNode *xml_node_new_entity_ref(XmlDom *dom, const char *name)
{
    XMQNode *n = xml_node_alloc(dom, XML_DOM_ENTITY_REF, sizeof(XMQNode));
    n->name = xml_dom_strdup(name);
    return n;
}

XMQNode *xml_node_new_namespace_node(XMQNode *element, const char *prefix, const char *href, int index)
{
    XMQNode *n = xml_node_alloc(element->dom, XML_DOM_NAMESPACE, sizeof(XMQNode));
    n->name = xml_dom_strdup(prefix);
    n->content = xml_dom_strdup(href);
    n->parent = element; // Not linked into the element.
    n->line = index;
    return n;
}

void xml_node_free(XMQNode *node)
{
    if (!node) return;
    assert(node->kind != XML_DOM_DOCUMENT);
    xml_node_unlink(node);
    xml_node_free_unlinked(node);
}

// Node accessors /////////////////////////////////////////////////

XmlNodeKind xml_node_kind(const XMQNode *node)
{
    return node->kind;
}

XmlDom *xml_node_dom(const XMQNode *node)
{
    return node->dom;
}

XMQNode *xml_node_parent(const XMQNode *node)
{
    return node->parent;
}

XMQNode *xml_node_first_child(const XMQNode *node)
{
    return node->first_child;
}

XMQNode *xml_node_last_child(const XMQNode *node)
{
    return node->last_child;
}

XMQNode *xml_node_next(const XMQNode *node)
{
    return node->next;
}

XMQNode *xml_node_prev(const XMQNode *node)
{
    return node->prev;
}

XmlNs *xml_node_ns(const XMQNode *node)
{
    return node->ns;
}

int xml_node_line(const XMQNode *node)
{
    return node->line;
}

void xml_node_set_line(XMQNode *node, int line)
{
    node->line = line;
}

void *xml_node_user_data(const XMQNode *node)
{
    return node->user_data;
}

void xml_node_set_user_data(XMQNode *node, void *user_data)
{
    node->user_data = user_data;
}

const char *xml_node_name(const XMQNode *node)
{
    return node->name;
}

void xml_node_set_name(XMQNode *node, const char *name)
{
    char *n = xml_dom_strdup(name);
    free(node->name);
    node->name = n;
}

static bool xml_node_has_child_content(const XMQNode *node)
{
    return node->kind == XML_DOM_ELEMENT || node->kind == XML_DOM_DOCUMENT || node->kind == XML_DOM_ATTRIBUTE;
}

const char *xml_node_content(const XMQNode *node)
{
    if (xml_node_has_child_content(node) || node->kind == XML_DOM_ENTITY_REF) return NULL;
    return node->content;
}

void xml_node_set_content(XMQNode *node, const char *content)
{
    if (xml_node_has_child_content(node))
    {
        XMQNode *c = node->first_child;
        while (c)
        {
            XMQNode *next = c->next;
            xml_node_free(c);
            c = next;
        }
        if (content && *content)
        {
            xml_node_append_child(node, xml_node_new_text(node->dom, content));
        }
        return;
    }
    char *n = xml_dom_strdup(content);
    free(node->content);
    node->content = n;
}

void xml_node_append_content(XMQNode *node, const char *content, size_t len)
{
    assert(!xml_node_has_child_content(node));
    size_t old = node->content ? strlen(node->content) : 0;
    char *n = (char*)malloc(old + len + 1);
    if (old) memcpy(n, node->content, old);
    memcpy(n + old, content, len);
    n[old+len] = 0;
    free(node->content);
    node->content = n;
}

/** A character reference node (&#10; &#x41;) as utf8, returns the length, 0 if not a valid character reference. */
static size_t xml_char_ref_utf8(const char *name, char *out)
{
    if (!name || name[0] != '#') return 0;
    long v;
    char *end = NULL;
    if (name[1] == 'x' || name[1] == 'X') v = strtol(name + 2, &end, 16);
    else v = strtol(name + 1, &end, 10);
    if (!end || *end || v <= 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;
    if (v < 0x80) { out[0] = (char)v; return 1; }
    if (v < 0x800) { out[0] = (char)(0xC0 | (v >> 6)); out[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    if (v < 0x10000)
    {
        out[0] = (char)(0xE0 | (v >> 12)); out[1] = (char)(0x80 | ((v >> 6) & 0x3F)); out[2] = (char)(0x80 | (v & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (v >> 18)); out[1] = (char)(0x80 | ((v >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((v >> 6) & 0x3F)); out[3] = (char)(0x80 | (v & 0x3F));
    return 4;
}

static void xml_node_collect_text(const XMQNode *node, XmlDomStrBuf *sb)
{
    for (const XMQNode *c = node->first_child; c; c = c->next)
    {
        switch (c->kind)
        {
        case XML_DOM_TEXT:
        case XML_DOM_CDATA:
            if (c->content) xml_dom_sb_add(sb, c->content, strlen(c->content));
            break;
        case XML_DOM_ENTITY_REF:
        {
            // A character reference kept as a node (xmq can do that) is a character, not an entity.
            char u[4];
            size_t n = xml_char_ref_utf8(c->name, u);
            if (n) xml_dom_sb_add(sb, u, n);
            break;
        }
        case XML_DOM_ELEMENT:
            xml_node_collect_text(c, sb);
            break;
        default:
            break;
        }
    }
}

char *xml_node_string_value(const XMQNode *node)
{
    // Entities are never expanded implicitly: an entity reference contributes nothing.
    // But a character reference kept as a node is its character.
    if (node->kind == XML_DOM_ENTITY_REF)
    {
        char u[5];
        size_t n = xml_char_ref_utf8(node->name, u);
        u[n] = 0;
        return xml_dom_strdup(u);
    }
    if (xml_node_has_child_content(node))
    {
        XmlDomStrBuf sb = { NULL, 0, 0 };
        xml_node_collect_text(node, &sb);
        if (!sb.buf) return xml_dom_strdup("");
        return sb.buf;
    }
    return xml_dom_strdup(node->content ? node->content : "");
}

bool xml_node_is_element(const XMQNode *node)
{
    return node && node->kind == XML_DOM_ELEMENT;
}

bool xml_node_is_text(const XMQNode *node)
{
    return node && node->kind == XML_DOM_TEXT;
}

const char *xml_node_ns_uri(const XMQNode *node)
{
    if (!node->ns || !node->ns->href || !*node->ns->href) return NULL;
    return node->ns->href;
}

const char *xml_node_ns_prefix(const XMQNode *node)
{
    if (!node->ns) return NULL;
    return node->ns->prefix;
}

// Tree editing ///////////////////////////////////////////////////

void xml_node_unlink(XMQNode *node)
{
    if (node->kind == XML_DOM_NAMESPACE)
    {
        node->parent = NULL;
        return;
    }
    XMQNode *p = node->parent;
    if (node->kind == XML_DOM_ATTRIBUTE)
    {
        if (p)
        {
            if ((XMQNode*)p->first_attr == node) p->first_attr = (XMQAttr*)node->next;
            if ((XMQNode*)p->last_attr == node) p->last_attr = (XMQAttr*)node->prev;
        }
    }
    else if (p)
    {
        if (p->first_child == node) p->first_child = node->next;
        if (p->last_child == node) p->last_child = node->prev;
    }
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    node->parent = node->next = node->prev = NULL;
}

static void xml_node_adopt(XMQNode *n, XmlDom *dom)
{
    if (n->dom != dom) xml_node_set_dom_recursive(n, dom);
}

XMQNode *xml_node_append_child(XMQNode *parent, XMQNode *child)
{
    assert(child->kind != XML_DOM_ATTRIBUTE && child->kind != XML_DOM_DOCUMENT);
    xml_node_unlink(child);
    xml_node_adopt(child, parent->dom);
    child->parent = parent;
    if (!parent->last_child)
    {
        parent->first_child = parent->last_child = child;
    }
    else
    {
        parent->last_child->next = child;
        child->prev = parent->last_child;
        parent->last_child = child;
    }
    return child;
}

XMQNode *xml_node_add_child(XMQNode *parent, XMQNode *child)
{
    XMQNode *last = parent->last_child;
    if (child->kind == XML_DOM_TEXT && last && last != child && last->kind == XML_DOM_TEXT)
    {
        const char *c = child->content ? child->content : "";
        xml_node_append_content(last, c, strlen(c));
        xml_node_free(child);
        return last;
    }
    return xml_node_append_child(parent, child);
}

XMQNode *xml_node_add_prev_sibling(XMQNode *node, XMQNode *n)
{
    if (n == node) return n;
    assert(n->kind != XML_DOM_ATTRIBUTE && node->kind != XML_DOM_ATTRIBUTE);
    xml_node_unlink(n);
    xml_node_adopt(n, node->dom);
    n->parent = node->parent;
    n->next = node;
    n->prev = node->prev;
    if (node->prev) node->prev->next = n;
    else if (node->parent) node->parent->first_child = n;
    node->prev = n;
    return n;
}

XMQNode *xml_node_add_next_sibling(XMQNode *node, XMQNode *n)
{
    if (n == node) return n;
    assert(n->kind != XML_DOM_ATTRIBUTE && node->kind != XML_DOM_ATTRIBUTE);
    xml_node_unlink(n);
    xml_node_adopt(n, node->dom);
    n->parent = node->parent;
    n->prev = node;
    n->next = node->next;
    if (node->next) node->next->prev = n;
    else if (node->parent) node->parent->last_child = n;
    node->next = n;
    return n;
}

XMQNode *xml_node_replace(XMQNode *node, XMQNode *n)
{
    if (n == node || !node->parent) return node;
    xml_node_add_prev_sibling(node, n);
    xml_node_unlink(node);
    return node;
}

size_t xml_node_num_children(const XMQNode *node)
{
    size_t n = 0;
    for (const XMQNode *c = node->first_child; c; c = c->next) n++;
    return n;
}

// Copy ///////////////////////////////////////////////////////////

typedef struct
{
    XmlNs **from;
    XmlNs **to;
    size_t len;
    size_t cap;
} XmlDomNsMap;

static void xml_dom_nsmap_add(XmlDomNsMap *m, XmlNs *from, XmlNs *to)
{
    if (m->len == m->cap)
    {
        m->cap = m->cap ? m->cap*2 : 8;
        m->from = (XmlNs**)realloc(m->from, m->cap*sizeof(XmlNs*));
        m->to = (XmlNs**)realloc(m->to, m->cap*sizeof(XmlNs*));
    }
    m->from[m->len] = from;
    m->to[m->len] = to;
    m->len++;
}

static XmlNs *xml_dom_nsmap_get(XmlDomNsMap *m, XmlNs *ns, XmlDom *dom)
{
    if (!ns) return NULL;
    // The static xml namespace belongs to a dom, translate it.
    if (ns->refcount < 0) return &dom->xml_ns;
    for (size_t i = m->len; i > 0; --i)
    {
        if (m->from[i-1] == ns) return m->to[i-1];
    }
    return ns;
}

static XMQNode *xml_node_copy_internal(XmlDom *dom, const XMQNode *node, bool deep, XmlDomNsMap *map)
{
    XMQNode *n = xml_node_alloc(dom, node->kind, xml_node_size(node->kind));
    n->name = xml_dom_strdup(node->name);
    n->content = xml_dom_strdup(node->content);
    n->line = node->line;
    n->user_data = NULL;

    switch (node->kind)
    {
    case XML_DOM_DTD:
    {
        const XmlDtd *from = (const XmlDtd*)node;
        XmlDtd *to = (XmlDtd*)n;
        to->public_id = xml_dom_strdup(from->public_id);
        to->system_id = xml_dom_strdup(from->system_id);
        for (const XMQNode *i = from->first_notation; i; i = i->next)
        {
            xml_dtd_add_notation_internal(to, i->name, i->content);
        }
        deep = true;
        break;
    }
    case XML_DOM_ENTITY_DECL:
    {
        const XmlEntityDecl *from = (const XmlEntityDecl*)node;
        XmlEntityDecl *to = (XmlEntityDecl*)n;
        to->etype = from->etype;
        to->orig = xml_dom_strdup(from->orig);
        to->public_id = xml_dom_strdup(from->public_id);
        to->system_id = xml_dom_strdup(from->system_id);
        to->notation = xml_dom_strdup(from->notation);
        break;
    }
    case XML_DOM_ELEMENT_DECL:
    case XML_DOM_ATTRIBUTE_DECL:
    case XML_DOM_NOTATION_DECL:
        ((XmlDtdDecl*)n)->default_value = xml_dom_strdup(((const XmlDtdDecl*)node)->default_value);
        break;
    case XML_DOM_ATTRIBUTE:
        deep = true; // The value is stored as children.
        break;
    default:
        break;
    }

    // Copy the namespace declarations first since the node itself might use one of them.
    XmlNs *last = NULL;
    for (XmlNs *d = node->ns_defs; d; d = d->next)
    {
        XmlNs *nd = xml_ns_new(d->prefix, d->href);
        xml_dom_nsmap_add(map, d, nd);
        if (last) last->next = nd; else n->ns_defs = nd;
        last = nd;
    }

    n->ns = xml_ns_ref(xml_dom_nsmap_get(map, node->ns, dom));

    for (const XMQAttr *a = node->first_attr; a; a = (const XMQAttr*)a->node.next)
    {
        XMQAttr *na = (XMQAttr*)xml_node_copy_internal(dom, &a->node, false, map);
        na->node.parent = n;
        if (n->last_attr)
        {
            n->last_attr->node.next = &na->node;
            na->node.prev = &n->last_attr->node;
            n->last_attr = na;
        }
        else
        {
            n->first_attr = n->last_attr = na;
        }
    }

    if (deep)
    {
        for (const XMQNode *c = node->first_child; c; c = c->next)
        {
            XMQNode *nc = xml_node_copy_internal(dom, c, true, map);
            xml_node_append_child(n, nc);
        }
    }
    return n;
}

XMQNode *xml_node_copy(XmlDom *dom, const XMQNode *node, bool deep)
{
    assert(node->kind != XML_DOM_DOCUMENT);
    XmlDomNsMap map = { NULL, NULL, 0, 0 };
    XMQNode *n = xml_node_copy_internal(dom, node, deep, &map);
    free(map.from);
    free(map.to);
    return n;
}

// Document order /////////////////////////////////////////////////

static size_t xml_node_depth(const XMQNode *n)
{
    size_t d = 0;
    while (n->parent) { n = n->parent; d++; }
    return d;
}

int xml_node_compare_order(const XMQNode *a, const XMQNode *b)
{
    if (a == b) return 0;

    size_t da = xml_node_depth(a);
    size_t db = xml_node_depth(b);
    const XMQNode *ia = a;
    const XMQNode *ib = b;

    while (da > db) { ia = ia->parent; da--; }
    while (db > da) { ib = ib->parent; db--; }

    // One is an ancestor (or owner element) of the other, the ancestor comes first.
    if (ia == ib) return (a == ia) ? -1 : 1;

    while (ia->parent != ib->parent)
    {
        ia = ia->parent;
        ib = ib->parent;
    }

    if (!ia->parent)
    {
        // Different trees, use a stable but arbitrary order.
        return (ia < ib) ? -1 : 1;
    }

    // ia and ib are siblings (children or attributes) of the same parent.
    bool aa = ia->kind == XML_DOM_ATTRIBUTE;
    bool ba = ib->kind == XML_DOM_ATTRIBUTE;
    if (aa && !ba) return -1; // Attributes come before children.
    if (ba && !aa) return 1;

    for (const XMQNode *i = ia->next; i; i = i->next)
    {
        if (i == ib) return -1;
    }
    return 1;
}

// Attributes /////////////////////////////////////////////////////

static bool xml_attr_matches(const XMQAttr *a, const char *name, const char *ns_uri)
{
    if (!xml_dom_streq(a->node.name, name)) return false;
    return xml_dom_streq(xml_node_ns_uri(&a->node), ns_uri);
}

XMQAttr *xml_node_move_attr(XMQNode *element, XMQAttr *attr)
{
    XMQNode *a = (XMQNode*)attr;
    assert(a->kind == XML_DOM_ATTRIBUTE && element->kind == XML_DOM_ELEMENT);
    if (a->parent == element) return attr;
    // Like libxml2 xmlAddChild of an attribute: an attribute with the same name and namespace is replaced.
    xml_node_remove_attr(element, a->name, a->ns && a->ns->href ? a->ns->href : NULL);
    xml_node_unlink(a);
    xml_node_adopt(a, element->dom);
    a->parent = element;
    if (!element->last_attr)
    {
        element->first_attr = element->last_attr = attr;
    }
    else
    {
        XMQNode *last = (XMQNode*)element->last_attr;
        last->next = a;
        a->prev = last;
        element->last_attr = attr;
    }
    return attr;
}

XMQAttr *xml_node_get_attr(const XMQNode *element, const char *name, const char *ns_uri)
{
    if (ns_uri && !*ns_uri) ns_uri = NULL;
    for (XMQAttr *a = element->first_attr; a; a = (XMQAttr*)a->node.next)
    {
        if (xml_attr_matches(a, name, ns_uri)) return a;
    }
    return NULL;
}

XMQAttr *xml_node_append_attr(XMQNode *element, const char *name, XmlNs *ns, const char *value)
{
    assert(element->kind == XML_DOM_ELEMENT);
    XMQAttr *a = (XMQAttr*)xml_node_alloc(element->dom, XML_DOM_ATTRIBUTE, sizeof(XMQAttr));
    a->node.name = xml_dom_strdup(name);
    a->node.ns = xml_ns_ref(ns);
    xml_node_set_content(&a->node, value);
    a->node.parent = element;
    if (element->last_attr)
    {
        element->last_attr->node.next = &a->node;
        a->node.prev = &element->last_attr->node;
        element->last_attr = a;
    }
    else
    {
        element->first_attr = element->last_attr = a;
    }
    return a;
}

XMQAttr *xml_node_set_attr(XMQNode *element, const char *name, XmlNs *ns, const char *value)
{
    assert(element->kind == XML_DOM_ELEMENT);
    const char *uri = (ns && ns->href && *ns->href) ? ns->href : NULL;
    XMQAttr *a = xml_node_get_attr(element, name, uri);
    if (a)
    {
        xml_node_set_content(&a->node, value);
        if (a->node.ns != ns)
        {
            xml_ns_ref(ns);
            xml_ns_unref(a->node.ns);
            a->node.ns = ns;
        }
        return a;
    }

    return xml_node_append_attr(element, name, ns, value);
}

bool xml_node_remove_attr(XMQNode *element, const char *name, const char *ns_uri)
{
    XMQAttr *a = xml_node_get_attr(element, name, ns_uri);
    if (!a) return false;
    xml_node_free(&a->node);
    return true;
}

XMQAttr *xml_node_first_attr(const XMQNode *element)
{
    return element->first_attr;
}

XMQAttr *xml_attr_next(const XMQAttr *attr)
{
    return (XMQAttr*)attr->node.next;
}

const char *xml_attr_name(const XMQAttr *attr)
{
    return attr->node.name;
}

const char *xml_attr_simple_value(const XMQAttr *attr)
{
    const XMQNode *c = attr->node.first_child;
    if (!c) return "";
    if (c->next || c->kind != XML_DOM_TEXT) return NULL;
    return c->content ? c->content : "";
}

XMQNode *xml_attr_owner(const XMQAttr *attr)
{
    return attr->node.parent;
}

XMQNode *xml_attr_node(XMQAttr *attr)
{
    return attr ? &attr->node : NULL;
}

XMQAttr *xml_node_as_attr(XMQNode *node)
{
    return (node && node->kind == XML_DOM_ATTRIBUTE) ? (XMQAttr*)node : NULL;
}

// Namespaces /////////////////////////////////////////////////////

const char *xml_ns_prefix(const XmlNs *ns)
{
    return ns->prefix;
}

const char *xml_ns_href(const XmlNs *ns)
{
    return ns->href;
}

XmlNs *xml_node_first_ns_decl(const XMQNode *element)
{
    return element->ns_defs;
}

XmlNs *xml_ns_next_decl(const XmlNs *ns)
{
    return ns->next;
}

XmlNs *xml_ns_declare(XMQNode *element, const char *prefix, const char *href)
{
    assert(element->kind == XML_DOM_ELEMENT);
    XmlNs *last = NULL;
    for (XmlNs *d = element->ns_defs; d; d = d->next)
    {
        if (xml_dom_streq(d->prefix, prefix)) return NULL;
        last = d;
    }
    XmlNs *ns = xml_ns_new(prefix, href);
    if (last) last->next = ns;
    else element->ns_defs = ns;
    return ns;
}

void *xml_ns_user_data(const XmlNs *ns)
{
    return ns->user_data;
}

void xml_ns_set_user_data(XmlNs *ns, void *user_data)
{
    if (ns->refcount >= 0) ns->user_data = user_data;
}

void xml_ns_move_decl_last(XMQNode *element, XmlNs *ns)
{
    XmlNs **pp = &element->ns_defs;
    while (*pp && *pp != ns) pp = &(*pp)->next;
    if (!*pp || !ns->next) return;
    *pp = ns->next;
    XmlNs *last = element->ns_defs;
    while (last->next) last = last->next;
    last->next = ns;
    ns->next = NULL;
}

void xml_ns_move_decl_to(XMQNode *from, XmlNs *ns, XMQNode *to)
{
    XmlNs **pp = &from->ns_defs;
    while (*pp && *pp != ns) pp = &(*pp)->next;
    if (!*pp) return;
    *pp = ns->next;
    ns->next = to->ns_defs;
    to->ns_defs = ns;
}

static void xml_node_repoint_ns(XMQNode *node, XmlNs *from, XmlNs *to)
{
    if (node->kind != XML_DOM_ELEMENT) return;
    if (node->ns == from) xml_node_set_ns(node, to);
    for (XMQAttr *a = node->first_attr; a; a = (XMQAttr*)a->node.next)
    {
        if (a->node.ns == from) xml_node_set_ns(&a->node, to);
    }
    for (XMQNode *c = node->first_child; c; c = c->next) xml_node_repoint_ns(c, from, to);
}

void xml_ns_remove_decl(XMQNode *element, XmlNs *ns, XmlNs *replacement)
{
    XmlNs **pp = &element->ns_defs;
    while (*pp && *pp != ns) pp = &(*pp)->next;
    if (!*pp) return;
    *pp = ns->next;
    ns->next = NULL;
    xml_ns_ref(ns);
    xml_node_repoint_ns(element, ns, replacement);
    xml_ns_unref(ns); // The temporary reference.
    xml_ns_unref(ns); // The declaring element.
}

void xml_ns_set_href(XmlNs *ns, const char *href)
{
    if (ns->refcount < 0) return; // The static xml namespace.
    char *h = href ? xml_dom_strdup(href) : NULL;
    free(ns->href);
    ns->href = h;
}

void xml_node_set_ns(XMQNode *node, XmlNs *ns)
{
    if (node->ns == ns) return;
    xml_ns_ref(ns);
    xml_ns_unref(node->ns);
    node->ns = ns;
}

XmlNs *xml_ns_lookup_prefix(const XMQNode *node, const char *prefix)
{
    if (prefix && !strcmp(prefix, "xml")) return &node->dom->xml_ns;
    if (node->kind == XML_DOM_ATTRIBUTE) node = node->parent;
    for (const XMQNode *i = node; i; i = i->parent)
    {
        if (i->kind != XML_DOM_ELEMENT) continue;
        for (XmlNs *d = i->ns_defs; d; d = d->next)
        {
            if (xml_dom_streq(d->prefix, prefix)) return d;
        }
    }
    return NULL;
}

XmlNs *xml_ns_lookup_href(const XMQNode *node, const char *href)
{
    if (!href) return NULL;
    if (!strcmp(href, XML_DOM_XML_NAMESPACE)) return &node->dom->xml_ns;
    const XMQNode *start = node->kind == XML_DOM_ATTRIBUTE ? node->parent : node;
    for (const XMQNode *i = start; i; i = i->parent)
    {
        if (i->kind != XML_DOM_ELEMENT) continue;
        for (XmlNs *d = i->ns_defs; d; d = d->next)
        {
            if (!xml_dom_streq(d->href, href)) continue;
            // Check that this prefix is not shadowed by a closer declaration.
            if (xml_ns_lookup_prefix(start, d->prefix) == d) return d;
        }
    }
    return NULL;
}

static XmlNs *xml_ns_declare_unique(XMQNode *check_node, XMQNode *decl_node, const char *href)
{
    char prefix[32];
    for (unsigned i = 1; i < 1000000000; i++)
    {
        snprintf(prefix, sizeof(prefix), "ns%u", i);
        if (xml_ns_lookup_prefix(check_node, prefix) == NULL &&
            xml_ns_lookup_prefix(decl_node, prefix) == NULL)
        {
            return xml_ns_declare(decl_node, prefix, href);
        }
    }
    assert(false);
    return NULL;
}

static void xml_node_reconcile_one(XMQNode *root, XMQNode *n)
{
    XmlNs *ns = n->ns;
    if (!ns || ns->refcount < 0) return;
    bool is_attr = n->kind == XML_DOM_ATTRIBUTE;

    // An attribute cannot use the default namespace, it needs a prefix.
    if (!(is_attr && !ns->prefix))
    {
        XmlNs *found = xml_ns_lookup_prefix(n, ns->prefix);
        if (found == ns) return;
        if (found && xml_dom_streq(found->href, ns->href))
        {
            xml_node_set_ns(n, found);
            return;
        }
    }
    else
    {
        XmlNs *found = xml_ns_lookup_href(n, ns->href);
        if (found && found->prefix)
        {
            xml_node_set_ns(n, found);
            return;
        }
    }

    // Not in scope. Declare it on the root of the subtree, if the prefix is free there.
    XmlNs *nd = NULL;
    if (!(is_attr && !ns->prefix) && xml_ns_lookup_prefix(root, ns->prefix) == NULL)
    {
        nd = xml_ns_declare(root, ns->prefix, ns->href);
    }
    if (!nd)
    {
        XmlNs *existing = xml_ns_lookup_href(root, ns->href);
        if (existing && (existing->prefix || !is_attr)) nd = existing;
        else nd = xml_ns_declare_unique(n, root, ns->href);
    }
    xml_node_set_ns(n, nd);
}

static void xml_node_reconcile_recursive(XMQNode *root, XMQNode *n)
{
    if (n->kind == XML_DOM_ELEMENT)
    {
        xml_node_reconcile_one(root, n);
        for (XMQAttr *a = n->first_attr; a; a = (XMQAttr*)a->node.next)
        {
            xml_node_reconcile_one(root, &a->node);
        }
    }
    for (XMQNode *c = n->first_child; c; c = c->next)
    {
        xml_node_reconcile_recursive(root, c);
    }
}

void xml_node_reconcile_ns(XMQNode *node)
{
    if (node->kind != XML_DOM_ELEMENT) return;
    xml_node_reconcile_recursive(node, node);
}

// xmq.h XMQNS support ////////////////////////////////////////////

const char *xml_ns_split_puri(const char *puri, char **out_prefix)
{
    *out_prefix = NULL;
    if (!puri) return NULL;
    if (*puri != '{') return puri;

    const char *rb = puri;
    while (*rb && *rb != '}') rb++;
    if (*rb != '}') return NULL;

    *out_prefix = xml_dom_strndup(puri+1, rb-puri-1);
    return rb+1;
}

static XmlNs *xml_ns_prep_ancestor(XMQNode *node, const char *uri, const char *prefix)
{
    XmlNs *ns = prefix ? xml_ns_lookup_prefix(node, prefix) : xml_ns_lookup_href(node, uri);
    if (ns) return ns;

    // No ancestor {prefix}uri found. Create the namespace on the top element.
    XMQNode *top = node->kind == XML_DOM_ATTRIBUTE ? node->parent : node;
    while (top->parent && top->parent->kind == XML_DOM_ELEMENT) top = top->parent;

    if (!prefix) return xml_ns_declare_unique(node, top, uri);
    ns = xml_ns_declare(top, prefix, uri);
    if (!ns) ns = xml_ns_declare_unique(node, top, uri);
    return ns;
}

XMQStatus xml_node_apply_xmqns(XMQNode *element, XMQNS ns)
{
    if (!element || element->kind != XML_DOM_ELEMENT) return XMQ_ERROR_BAD_VALUE;

    XMQNode *parent = element->parent;
    XmlNs *pns = (parent && parent->kind == XML_DOM_ELEMENT) ? parent->ns : NULL;

    switch (ns.action)
    {
    case XMQ_NS_PARENT:
        xml_node_set_ns(element, pns);
        return XMQ_OK;
    case XMQ_NS_NONE:
        if (pns && pns->href && *pns->href)
        {
            // The parent has a namespace, explicitly undeclare the default namespace.
            XmlNs *d = xml_ns_declare(element, NULL, "");
            if (!d) d = xml_ns_lookup_prefix(element, NULL);
            xml_node_set_ns(element, d);
        }
        else
        {
            xml_node_set_ns(element, NULL);
        }
        return XMQ_OK;
    case XMQ_NS_HERE:
    {
        char *prefix = NULL;
        const char *uri = xml_ns_split_puri(ns.uri, &prefix);
        if (!uri) return XMQ_ERROR_BAD_VALUE;
        XmlNs *d = xml_ns_declare(element, prefix, uri);
        if (!d)
        {
            // Already declared here, accept it if it is the same uri.
            XmlNs *e = xml_ns_lookup_prefix(element, prefix);
            if (!e || !xml_dom_streq(e->href, uri)) { free(prefix); return XMQ_ERROR_BAD_VALUE; }
            d = e;
        }
        free(prefix);
        xml_node_set_ns(element, d);
        return XMQ_OK;
    }
    case XMQ_NS_ANCESTOR:
    {
        char *prefix = NULL;
        const char *uri = xml_ns_split_puri(ns.uri, &prefix);
        if (!uri) return XMQ_ERROR_BAD_VALUE;
        XmlNs *d = xml_ns_prep_ancestor(element, uri, prefix);
        free(prefix);
        xml_node_set_ns(element, d);
        return XMQ_OK;
    }
    }
    return XMQ_ERROR_BAD_VALUE;
}

XMQReturnAttr xml_node_set_attr_xmqns(XMQNode *element, const char *name, const char *value, XMQNS ns)
{
    if (!element || element->kind != XML_DOM_ELEMENT || !name || !value)
    {
        return (XMQReturnAttr){ XMQ_ERROR_BAD_VALUE, NULL };
    }

    XmlNs *ans = NULL;
    switch (ns.action)
    {
    case XMQ_NS_NONE:
        break;
    case XMQ_NS_PARENT:
        ans = element->ns;
        break;
    case XMQ_NS_ANCESTOR:
    {
        char *prefix = NULL;
        const char *uri = xml_ns_split_puri(ns.uri, &prefix);
        if (!uri) return (XMQReturnAttr){ XMQ_ERROR_BAD_VALUE, NULL };
        ans = xml_ns_prep_ancestor(element, uri, prefix);
        free(prefix);
        break;
    }
    default:
        // You cannot use NS_HERE for an attribute.
        return (XMQReturnAttr){ XMQ_ERROR_BAD_VALUE, NULL };
    }

    return (XMQReturnAttr){ XMQ_OK, xml_node_set_attr(element, name, ans, value) };
}

#endif // XML_DOM_MODULE
