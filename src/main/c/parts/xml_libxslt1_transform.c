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

#ifndef BUILDING_DIST_XMQ

#define XML_LIBXSLT1_INTERNALS
#include"xml_libxslt1_transform.h"
#include"xml_libxslt1_xslt.h"
#include"xml_dom.h"
#include"xml_xpath.h"
#include"xml_uri.h"
#include"xml_chars.h"
#include"xml_save.h"
#include"xml_html_save.h"
#include"hashmap.h"
#include"membuffer.h"

#include<errno.h>
#include<float.h>
#include<limits.h>
#include<math.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_LIBXSLT1_TRANSFORM_MODULE

// The limits of libxslt (xsltMaxDepth, xsltMaxVars, XSLT_MAX_SORT).
#define XSLT_MAX_DEPTH 3000
#define XSLT_MAX_VARS 15000
#define XSLT_MAX_SORT 15

#define XSLT_VAR_GLOBAL (1 << 0)
#define XSLT_VAR_IN_SELECT (1 << 1)

// libxslt marks a global variable being computed by replacing its name (xsltComputingGlobalVarMarker).
static const char xslt_computing_global_var_marker_[] = " var/param being computed";

static void xslt_apply_sequence_constructor(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list, XsltTemplate *templ);
static void xslt_apply_xslt_template(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list,
                                     XsltTemplate *templ, XsltStackElem *with_params);
static void xslt_process_one_node(XmlLibxslt1Transform *t, XMQNode *context_node, XsltStackElem *with_params);
static void xslt_apply_one_template(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list, XsltStackElem *params);
static void xslt_apply_attribute_set(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, const char *attr_sets);
static void xslt_attribute(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst, XsltStylePreComp *comp);
static void xslt_do_sort_function(XmlLibxslt1Transform *t, XMQNode **sorts, int nbsorts);
static void xslt_message(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst);
static void xslt_debug(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst);

// libxml2 tree operations, with the semantics the libxslt code relies on ///

static bool lx_is_noenc(const XMQNode *n)
{
    const char *name = xml_node_name(n);
    return name && !strcmp(name, "textnoenc");
}

static bool lx_is_element(const XMQNode *n)
{
    return n && xml_node_kind(n) == XML_DOM_ELEMENT;
}

/** The parent, NULL for nodes whose parent is not a node of the tree (attributes have their owner). */
static XMQNode *lx_parent(const XMQNode *n)
{
    return n ? xml_node_parent(n) : NULL;
}

/** Port of xmlSearchNs: the declaration of prefix in scope of node, also looking at the
    namespaces of the ancestors (libxslt result trees can use undeclared namespaces). */
static XmlNs *lx_search_ns(XMQNode *node, const char *prefix)
{
    if (!node || xml_node_kind(node) == XML_DOM_NAMESPACE) return NULL;
    if (prefix && !strcmp(prefix, "xml")) return xml_ns_lookup_prefix(node, "xml");
    XMQNode *orig = node;
    while (xml_node_kind(node) != XML_DOM_ELEMENT)
    {
        node = xml_node_parent(node);
        if (!node) return NULL;
    }
    while (node && xml_node_kind(node) == XML_DOM_ELEMENT)
    {
        for (XmlNs *cur = xml_node_first_ns_decl(node); cur; cur = xml_ns_next_decl(cur))
        {
            if (xslt1_streq(xml_ns_prefix(cur), prefix) && xml_ns_href(cur)) return cur;
        }
        if (orig != node)
        {
            XmlNs *cur = xml_node_ns(node);
            if (cur && xslt1_streq(xml_ns_prefix(cur), prefix) && xml_ns_href(cur)) return cur;
        }
        node = xml_node_parent(node);
    }
    return NULL;
}

/** Port of xmlNsInScope. */
static int lx_ns_in_scope(XMQNode *node, XMQNode *ancestor, const char *prefix)
{
    while (node && node != ancestor)
    {
        if (xml_node_kind(node) == XML_DOM_ELEMENT)
        {
            for (XmlNs *tst = xml_node_first_ns_decl(node); tst; tst = xml_ns_next_decl(tst))
            {
                if (!xml_ns_prefix(tst) && !prefix) return 0;
                if (xml_ns_prefix(tst) && prefix && !strcmp(xml_ns_prefix(tst), prefix)) return 0;
            }
        }
        node = xml_node_parent(node);
    }
    if (node != ancestor) return -1;
    return 1;
}

/** Port of xmlSearchNsByHref (for elements). */
static XmlNs *lx_search_ns_by_href(XMQNode *node, const char *href)
{
    if (!node || xml_node_kind(node) == XML_DOM_NAMESPACE) return NULL;
    if (xslt1_streq(href, XML_DOM_XML_NAMESPACE)) return xml_ns_lookup_prefix(node, "xml");
    bool is_attr = xml_node_kind(node) == XML_DOM_ATTRIBUTE;
    XMQNode *orig = node;
    while (xml_node_kind(node) != XML_DOM_ELEMENT)
    {
        node = xml_node_parent(node);
        if (!node) return NULL;
    }
    while (node && xml_node_kind(node) == XML_DOM_ELEMENT)
    {
        for (XmlNs *cur = xml_node_first_ns_decl(node); cur; cur = xml_ns_next_decl(cur))
        {
            if (xslt1_streq(xml_ns_href(cur), href))
            {
                if ((!is_attr || xml_ns_prefix(cur)) && lx_ns_in_scope(orig, node, xml_ns_prefix(cur)) == 1) return cur;
            }
        }
        if (orig != node)
        {
            XmlNs *cur = xml_node_ns(node);
            if (cur && xslt1_streq(xml_ns_href(cur), href))
            {
                if ((!is_attr || xml_ns_prefix(cur)) && lx_ns_in_scope(orig, node, xml_ns_prefix(cur)) == 1) return cur;
            }
        }
        node = xml_node_parent(node);
    }
    return NULL;
}

/** Port of xmlNewNs on an element: NULL if the prefix is already declared there. */
static XmlNs *lx_new_ns(XMQNode *elem, const char *href, const char *prefix)
{
    if (!lx_is_element(elem)) return NULL;
    return xml_ns_declare(elem, prefix, href);
}

/** Port of xmlAddChild: text is merged with a preceding text node of the same kind
    (escaped or not), then cur is freed and the text node is returned. */
static XMQNode *lx_add_child(XMQNode *parent, XMQNode *cur)
{
    if (!cur) return NULL;
    if (!parent)
    {
        xml_node_free(cur);
        return NULL;
    }
    if (xml_node_kind(parent) == XML_DOM_TEXT)
    {
        const char *c = xml_node_content(cur);
        if (c) xml_node_append_content(parent, c, strlen(c));
        xml_node_free(cur);
        return parent;
    }
    XMQNode *prev = xml_node_last_child(parent);
    if (cur == prev) return cur;
    if (xml_node_kind(cur) == XML_DOM_TEXT && prev && xml_node_kind(prev) == XML_DOM_TEXT && lx_is_noenc(prev) == lx_is_noenc(cur))
    {
        const char *c = xml_node_content(cur);
        if (c) xml_node_append_content(prev, c, strlen(c));
        xml_node_free(cur);
        return prev;
    }
    return xml_node_append_child(parent, cur);
}

/** libxml2 gives an attribute set to "" an empty text child (the html serializer writes it as a=""). */
static XMQAttr *lx_attr_text_child(XMQAttr *a, const char *value)
{
    if (a && value && !xml_node_first_child(xml_attr_node(a)))
    {
        XMQNode *an = xml_attr_node(a);
        xml_node_append_child(an, xml_node_new_text(xml_node_dom(an), ""));
    }
    return a;
}

/** Port of xmlSetNsProp: replace the value of an attribute with the same name and namespace or add one. */
static XMQAttr *lx_set_ns_prop(XMQNode *elem, XmlNs *ns, const char *name, const char *value)
{
    if (!lx_is_element(elem) || !name) return NULL;
    return lx_attr_text_child(xml_node_set_attr(elem, name, ns, value ? value : ""), value);
}

/** Port of xmlNewNsProp: add an attribute without looking for duplicates. */
static XMQAttr *lx_new_ns_prop(XMQNode *elem, XmlNs *ns, const char *name, const char *value)
{
    if (!lx_is_element(elem) || !name) return NULL;
    return lx_attr_text_child(xml_node_append_attr(elem, name, ns, value ? value : ""), value);
}

/** The attribute value with the entity references substituted (xmlNodeListGetString). */
static char *lx_attr_value(XMQNode *attr)
{
    return xml_node_string_value(attr);
}

/** Port of xmlDocCopyNode(node, doc, 0): a copy of the node itself (no attributes, namespaces or children). */
static XMQNode *lx_shallow_copy(XmlDom *dom, XMQNode *node)
{
    XMQNode *copy = NULL;
    switch (xml_node_kind(node))
    {
    case XML_DOM_ELEMENT:
        copy = xml_node_new_element(dom, xml_node_name(node), NULL);
        break;
    case XML_DOM_PI:
        copy = xml_node_new_pi(dom, xml_node_name(node), xml_node_content(node));
        break;
    case XML_DOM_COMMENT:
        copy = xml_node_new_comment(dom, xml_node_content(node));
        break;
    case XML_DOM_ENTITY_REF:
        copy = xml_node_new_entity_ref(dom, xml_node_name(node));
        break;
    default:
        return NULL;
    }
    if (copy) xml_node_set_line(copy, xml_node_line(node));
    return copy;
}

static bool lx_is_real_node(const XMQNode *n)
{
    if (!n) return false;
    switch (xml_node_kind(n))
    {
    case XML_DOM_ELEMENT:
    case XML_DOM_TEXT:
    case XML_DOM_CDATA:
    case XML_DOM_ATTRIBUTE:
    case XML_DOM_DOCUMENT:
    case XML_DOM_COMMENT:
    case XML_DOM_PI:
        return true;
    default:
        return false;
    }
}

static bool lx_is_blank_node(const XMQNode *n)
{
    return xml_node_kind(n) == XML_DOM_TEXT && xslt1_is_blank_str(xml_node_content(n));
}

/** The first child, NULL for nodes without children in libxml2 (namespace nodes). */
static XMQNode *lx_children(const XMQNode *n)
{
    if (!n || xml_node_kind(n) == XML_DOM_NAMESPACE || xml_node_kind(n) == XML_DOM_ATTRIBUTE) return NULL;
    return xml_node_first_child(n);
}

static bool lx_has_children(const XMQNode *n)
{
    return lx_children(n) != NULL;
}

/** Port of xmlNodeGetContent for an element: the concatenated text. */
static char *lx_node_get_content(XMQNode *n)
{
    return xml_node_string_value(n);
}

// Result tree fragments, port of libxslt variables.c ///////////

static XsltRVT *xslt_rvt_of(XmlDom *dom)
{
    if (!dom) return NULL;
    XsltRVT *rvt = (XsltRVT*)xml_node_user_data(xml_dom_document(dom));
    if (rvt && rvt->magic == XSLT_RVT_MAGIC && rvt->dom == dom) return rvt;
    return NULL;
}

/** Is the document a result tree fragment (libxslt XSLT_IS_RES_TREE_FRAG)? */
bool xslt1_is_rtf(XmlLibxslt1Transform *t, XmlDom *dom)
{
    (void)t;
    return xslt_rvt_of(dom) != NULL;
}

/** Port of xsltCreateRVT. */
static XsltRVT *xslt_create_rvt(XmlLibxslt1Transform *t)
{
    (void)t;
    XsltRVT *rvt = (XsltRVT*)calloc(1, sizeof(XsltRVT));
    rvt->magic = XSLT_RVT_MAGIC;
    rvt->dom = xml_dom_new();
    xml_node_set_user_data(xml_dom_document(rvt->dom), rvt);
    return rvt;
}

static void xslt_free_doc_keys(struct XsltDocInfo *info);

/** Port of xsltReleaseRVT (without the cache). */
static void xslt_release_rvt(XmlLibxslt1Transform *t, XsltRVT *rvt)
{
    if (!rvt) return;
    if (rvt->keys)
    {
        xslt_free_doc_keys(rvt->keys);
        free(rvt->keys);
    }
    xml_xpath_context_forget(t->xpath, xml_dom_document(rvt->dom));
    xml_dom_free(rvt->dom);
    free(rvt);
}

/** Port of xsltRegisterTmpRVT. */
static void xslt_register_tmp_rvt(XmlLibxslt1Transform *t, XsltRVT *rvt)
{
    rvt->prev = NULL;
    rvt->flag = XSLT_RVT_LOCAL;
    if (t->context_variable)
    {
        rvt->next = t->context_variable->fragment;
        t->context_variable->fragment = rvt;
        return;
    }
    rvt->next = t->tmp_rvt;
    if (t->tmp_rvt) t->tmp_rvt->prev = rvt;
    t->tmp_rvt = rvt;
}

/** Port of xsltRegisterLocalRVT. */
static void xslt_register_local_rvt(XmlLibxslt1Transform *t, XsltRVT *rvt)
{
    rvt->prev = NULL;
    rvt->flag = XSLT_RVT_LOCAL;
    if (t->context_variable && (t->context_variable->flags & XSLT_VAR_IN_SELECT))
    {
        rvt->next = t->context_variable->fragment;
        t->context_variable->fragment = rvt;
        return;
    }
    rvt->next = t->local_rvt;
    if (t->local_rvt) t->local_rvt->prev = rvt;
    t->local_rvt = rvt;
}

/** Port of xsltRegisterPersistRVT. */
static void xslt_register_persist_rvt(XmlLibxslt1Transform *t, XsltRVT *rvt)
{
    rvt->flag = XSLT_RVT_GLOBAL;
    rvt->prev = NULL;
    rvt->next = t->persist_rvt;
    if (t->persist_rvt) t->persist_rvt->prev = rvt;
    t->persist_rvt = rvt;
}

/** Port of xsltFlagRVTs. */
static int xslt_flag_rvts(XmlLibxslt1Transform *t, const XmlXPathResult *obj, int val)
{
    if (!obj || xml_xpath_result_type(obj) != XML_XPATH_NODESET) return 0;
    size_t n = xml_xpath_result_size(obj);
    for (size_t i = 0; i < n; i++)
    {
        XMQNode *cur = xml_xpath_result_node(obj, i);
        XmlDom *doc = xml_node_dom(cur);
        if (!doc)
        {
            xslt1_transform_error(t, NULL, t->inst, "Internal error in xsltFlagRVTs(): Cannot retrieve the doc of a node.\n");
            return -1;
        }
        XsltRVT *rvt = xslt_rvt_of(doc);
        if (rvt && rvt->flag != XSLT_RVT_GLOBAL)
        {
            if (val == XSLT_RVT_LOCAL)
            {
                if (rvt->flag == XSLT_RVT_FUNC_RESULT) rvt->flag = XSLT_RVT_LOCAL;
            }
            else if (val == XSLT_RVT_GLOBAL)
            {
                if (rvt->flag != XSLT_RVT_LOCAL)
                {
                    xslt1_generic_error(&t->style->settings, "xsltFlagRVTs: Invalid transition %d => GLOBAL\n", rvt->flag);
                    rvt->flag = XSLT_RVT_GLOBAL;
                    return -1;
                }
                // Registered as persistent in xsltReleaseLocalRVTs.
                rvt->flag = XSLT_RVT_GLOBAL;
            }
            else if (val == XSLT_RVT_FUNC_RESULT)
            {
                rvt->flag = val;
            }
        }
    }
    return 0;
}

/** Port of xsltReleaseLocalRVTs. */
static void xslt_release_local_rvts(XmlLibxslt1Transform *t, XsltRVT *base)
{
    XsltRVT *cur = t->local_rvt;
    if (cur == base) return;
    if (cur->prev) xslt1_transform_error(t, NULL, NULL, "localRVT not head of list\n");
    // Reset localRVT early because some RVTs might be registered again.
    t->local_rvt = base;
    if (base) base->prev = NULL;
    do
    {
        XsltRVT *tmp = cur;
        cur = cur->next;
        if (tmp->flag == XSLT_RVT_LOCAL)
        {
            xslt_release_rvt(t, tmp);
        }
        else if (tmp->flag == XSLT_RVT_GLOBAL)
        {
            xslt_register_persist_rvt(t, tmp);
        }
        else if (tmp->flag == XSLT_RVT_FUNC_RESULT)
        {
            xslt_register_local_rvt(t, tmp);
            tmp->flag = XSLT_RVT_FUNC_RESULT;
        }
        else
        {
            xslt1_generic_error(&t->style->settings, "xsltReleaseLocalRVTs: Unexpected RVT flag %p\n", (void*)NULL);
        }
    } while (cur != base);
}

static void xslt_free_rvt_list(XmlLibxslt1Transform *t, XsltRVT *cur)
{
    while (cur)
    {
        XsltRVT *next = cur->next;
        xslt_release_rvt(t, cur);
        cur = next;
    }
}

/** Port of xsltFreeRVTs. */
static void xslt_free_rvts(XmlLibxslt1Transform *t)
{
    xslt_free_rvt_list(t, t->local_rvt);
    t->local_rvt = NULL;
    xslt_free_rvt_list(t, t->tmp_rvt);
    t->tmp_rvt = NULL;
    xslt_free_rvt_list(t, t->persist_rvt);
    t->persist_rvt = NULL;
}

/** A node set holding the document node of the fragment (xmlXPathNewValueTree). */
static XmlXPathResult *xslt_new_value_tree(XsltRVT *rvt)
{
    XmlXPathResult *r = xml_xpath_result_new_nodeset();
    xml_xpath_result_add_node(r, xml_dom_document(rvt->dom));
    xml_xpath_result_set_tree(r, true);
    return r;
}

// XPath evaluation /////////////////////////////////////////////////

/** Report an error of an evaluation like libxml2 (xmlXPathErr). */
static void xslt_report_eval_error(XmlLibxslt1Transform *t, const XmlXPathComp *comp, const XmlXPathError *err)
{
    if (!err || err->code == XML_XPATH_ERR_OK) return;
    xslt1_report_xpath_error(&t->style->settings, comp ? xml_xpath_comp_source(comp) : NULL, err);
}

/** Evaluate with the context position and size of the transformation (xmlXPathCompiledEval). */
static XmlXPathResult *xslt_xpath_eval(XmlLibxslt1Transform *t, const XmlXPathComp *comp, XMQNode *node)
{
    if (!comp) return NULL;
    XmlXPathError err;
    XmlXPathResult *r = xml_xpath_eval_comp(t->xpath, comp, node, t->xp_position, t->xp_size, &err);
    if (!r) xslt_report_eval_error(t, comp, &err);
    return r;
}

/** Port of xsltPreCompEval: the context position and size are restored afterwards. */
static XmlXPathResult *xslt_pre_comp_eval(XmlLibxslt1Transform *t, XMQNode *node, XsltStylePreComp *comp)
{
    long pos = t->xp_position, size = t->xp_size;
    XmlXPathResult *res = xslt_xpath_eval(t, comp->comp, node);
    t->xp_position = pos;
    t->xp_size = size;
    return res;
}

/** Port of xsltPreCompEvalToBoolean: -1 on an error. */
static int xslt_pre_comp_eval_to_boolean(XmlLibxslt1Transform *t, XMQNode *node, XsltStylePreComp *comp)
{
    XmlXPathResult *res = xslt_pre_comp_eval(t, node, comp);
    if (!res) return -1;
    int b = xml_xpath_result_to_boolean(res) ? 1 : 0;
    xml_xpath_result_free(res);
    return b;
}

// Variables, port of libxslt variables.c ////////////////////////////

static XsltStackElem *xslt_new_stack_elem(void)
{
    return (XsltStackElem*)calloc(1, sizeof(XsltStackElem));
}

/** Port of xsltCopyStackElem: a runtime copy of a global variable declaration. */
static XsltStackElem *xslt_copy_stack_elem(XsltStackElem *elem)
{
    XsltStackElem *cur = xslt_new_stack_elem();
    cur->name = elem->name;
    cur->name_uri = elem->name_uri;
    cur->select = elem->select;
    cur->tree = elem->tree;
    cur->comp = elem->comp;
    return cur;
}

/** Port of xsltFreeStackElem (for runtime elements). */
static void xslt_free_runtime_stack_elem(XmlLibxslt1Transform *t, XsltStackElem *elem)
{
    if (!elem) return;
    xml_xpath_result_free(elem->value);
    while (elem->fragment)
    {
        XsltRVT *cur = elem->fragment;
        elem->fragment = cur->next;
        if (cur->flag == XSLT_RVT_LOCAL)
        {
            xslt_release_rvt(t, cur);
        }
        else if (cur->flag == XSLT_RVT_FUNC_RESULT)
        {
            xslt_register_local_rvt(t, cur);
            cur->flag = XSLT_RVT_FUNC_RESULT;
        }
        else
        {
            xslt1_generic_error(&t->style->settings, "xsltFreeStackElem: Unexpected RVT flag %d\n", cur->flag);
        }
    }
    if (elem->owns_strings)
    {
        free(elem->name);
        free(elem->name_uri);
        free(elem->select);
    }
    free(elem);
}

static void xslt_free_runtime_stack_elem_list(XmlLibxslt1Transform *t, XsltStackElem *elem)
{
    while (elem)
    {
        XsltStackElem *next = elem->next;
        xslt_free_runtime_stack_elem(t, elem);
        elem = next;
    }
}

/** Port of xsltStackLookup. */
static XsltStackElem *xslt_stack_lookup(XmlLibxslt1Transform *t, const char *name, const char *name_uri)
{
    if (!name || t->vars_nr == 0) return NULL;
    for (int i = t->vars_nr; i > t->vars_base; i--)
    {
        for (XsltStackElem *cur = t->vars_tab[i - 1]; cur; cur = cur->next)
        {
            if (xslt1_streq(cur->name, name) && xslt1_streq(cur->name_uri, name_uri)) return cur;
        }
    }
    return NULL;
}

/** Port of xsltCheckStackElem: 0 not found, 1 variable, 2 param, 3 with-param. */
static int xslt_check_stack_elem(XmlLibxslt1Transform *t, const char *name, const char *name_uri)
{
    if (!name) return -1;
    XsltStackElem *cur = xslt_stack_lookup(t, name, name_uri);
    if (!cur) return 0;
    if (cur->comp)
    {
        if (cur->comp->type == XSLT_FUNC_WITHPARAM) return 3;
        else if (cur->comp->type == XSLT_FUNC_PARAM) return 2;
    }
    return 1;
}

static void xslt_vars_push(XmlLibxslt1Transform *t, XsltStackElem *elem)
{
    if (t->vars_nr >= t->vars_max)
    {
        t->vars_max = t->vars_max == 0 ? 10 : 2 * t->vars_max;
        t->vars_tab = (XsltStackElem**)realloc(t->vars_tab, (size_t)t->vars_max * sizeof(XsltStackElem*));
    }
    t->vars_tab[t->vars_nr++] = elem;
    t->vars = elem;
}

/** Port of xsltAddStackElem: every element of the list is pushed. */
static int xslt_add_stack_elem(XmlLibxslt1Transform *t, XsltStackElem *elem)
{
    if (!elem) return -1;
    do
    {
        xslt_vars_push(t, elem);
        elem = elem->next;
    } while (elem);
    return 0;
}

/** Port of xsltLocalVariablePush. */
static int xslt_local_variable_push(XmlLibxslt1Transform *t, XsltStackElem *variable, int level)
{
    xslt_vars_push(t, variable);
    variable->level = level;
    return 0;
}

/** Port of xsltLocalVariablePop. */
static void xslt_local_variable_pop(XmlLibxslt1Transform *t, int limit_nr, int level)
{
    if (t->vars_nr <= 0) return;
    do
    {
        if (t->vars_nr <= limit_nr) break;
        XsltStackElem *variable = t->vars_tab[t->vars_nr - 1];
        if (variable->level <= level) break;
        if (variable->level >= 0) xslt_free_runtime_stack_elem_list(t, variable);
        t->vars_nr--;
    } while (t->vars_nr != 0);
    t->vars = t->vars_nr > 0 ? t->vars_tab[t->vars_nr - 1] : NULL;
}

/** Port of xsltTemplateParamsCleanup: xsl:with-param items (level -1, -2) are not freed. */
static void xslt_template_params_cleanup(XmlLibxslt1Transform *t)
{
    for (; t->vars_nr > t->vars_base; t->vars_nr--)
    {
        XsltStackElem *param = t->vars_tab[t->vars_nr - 1];
        if (param->level >= 0) xslt_free_runtime_stack_elem_list(t, param);
    }
    t->vars = t->vars_nr > 0 ? t->vars_tab[t->vars_nr - 1] : NULL;
}

/** Instantiate content into a new result tree fragment (the content part of xsltEvalVariable). */
static XmlXPathResult *xslt_eval_content_into(XmlLibxslt1Transform *t, XsltRVT *container, XMQNode *tree)
{
    XmlDom *old_output = t->output;
    XMQNode *old_insert = t->insert;
    t->output = container->dom;
    t->insert = xml_dom_document(container->dom);
    xslt_apply_one_template(t, t->node, tree, NULL);
    t->insert = old_insert;
    t->output = old_output;
    return xslt_new_value_tree(container);
}

/** Port of xsltEvalVariable. */
static XmlXPathResult *xslt_eval_variable(XmlLibxslt1Transform *t, XsltStackElem *variable, XsltStylePreComp *comp)
{
    XmlXPathResult *result = NULL;
    if (!variable) return NULL;
    XMQNode *old_inst = t->inst;
    if (variable->select)
    {
        XmlXPathComp *xp_expr = NULL;
        bool own = false;
        XsltStackElem *old_var = t->context_variable;
        if (comp && comp->comp)
        {
            xp_expr = comp->comp;
        }
        else
        {
            xp_expr = xslt1_xpath_compile(&t->style->settings, NULL, variable->select, false);
            own = true;
        }
        if (!xp_expr) return NULL;
        long pos = t->xp_position, size = t->xp_size;
        t->context_variable = variable;
        variable->flags |= XSLT_VAR_IN_SELECT;
        result = xslt_xpath_eval(t, xp_expr, t->node);
        variable->flags ^= XSLT_VAR_IN_SELECT;
        t->context_variable = old_var;
        t->xp_position = pos;
        t->xp_size = size;
        if (own) xml_xpath_comp_free(xp_expr);
        if (!result)
        {
            xslt1_transform_error(t, NULL, comp ? comp->inst : NULL, "Failed to evaluate the expression of variable '%s'.\n",
                                  variable->name);
            t->state = XSLT_STATE_STOPPED;
        }
    }
    else
    {
        if (!variable->tree)
        {
            result = xml_xpath_result_new_string("");
        }
        else
        {
            XsltStackElem *old_var = t->context_variable;
            XsltRVT *container = xslt_create_rvt(t);
            variable->fragment = container;
            container->flag = XSLT_RVT_LOCAL;
            t->context_variable = variable;
            result = xslt_eval_content_into(t, container, variable->tree);
            t->context_variable = old_var;
            if (!result) result = xml_xpath_result_new_string("");
        }
    }
    t->inst = old_inst;
    return result;
}

/** Port of xsltEvalGlobalVariable. */
static XmlXPathResult *xslt_eval_global_variable(XsltStackElem *elem, XmlLibxslt1Transform *t)
{
    XmlXPathResult *result = NULL;
    if (!elem) return NULL;
    if (elem->computed) return elem->value;
    XMQNode *old_inst = t->inst;
    XsltStylePreComp *comp = elem->comp;
    char *old_var_name = elem->name;
    elem->name = (char*)xslt_computing_global_var_marker_;
    XsltStackElem *old_ctxt_var = t->context_variable;
    t->context_variable = NULL;
    if (elem->select)
    {
        XmlXPathComp *xp_expr = NULL;
        bool own = false;
        if (comp && comp->comp)
        {
            xp_expr = comp->comp;
        }
        else
        {
            xp_expr = xslt1_xpath_compile(&t->style->settings, NULL, elem->select, false);
            own = true;
        }
        if (!xp_expr) goto error;
        t->inst = comp ? comp->inst : NULL;
        long pos = t->xp_position, size = t->xp_size;
        t->xp_size = 1;
        t->xp_position = 1;
        result = xslt_xpath_eval(t, xp_expr, t->initial_context_node);
        t->xp_position = pos;
        t->xp_size = size;
        if (own) xml_xpath_comp_free(xp_expr);
        if (!result)
        {
            xslt1_transform_error(t, NULL, comp ? comp->inst : NULL, "Evaluating global variable %s failed\n", elem->name);
            t->state = XSLT_STATE_STOPPED;
            goto error;
        }
        xslt_flag_rvts(t, result, XSLT_RVT_GLOBAL);
    }
    else
    {
        if (!elem->tree)
        {
            result = xml_xpath_result_new_string("");
        }
        else
        {
            XsltRVT *container = xslt_create_rvt(t);
            xslt_register_persist_rvt(t, container);
            result = xslt_eval_content_into(t, container, elem->tree);
            if (!result) result = xml_xpath_result_new_string("");
        }
    }
error:
    t->context_variable = old_ctxt_var;
    elem->name = old_var_name;
    t->inst = old_inst;
    if (result)
    {
        elem->value = result;
        elem->computed = 1;
    }
    return result;
}

/** Port of xsltEvalGlobalVariables. */
static int xslt_eval_global_variables(XmlLibxslt1Transform *t)
{
    XsltStackElem *head = NULL;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        for (XsltStackElem *elem = style->variables; elem; elem = elem->next)
        {
            char *key = xslt1_key2(elem->name, elem->name_uri);
            XsltStackElem *def = (XsltStackElem*)hashmap_get(t->global_vars, key);
            if (!def)
            {
                def = xslt_copy_stack_elem(elem);
                hashmap_put(t->global_vars, key, def);
                def->next = head;
                head = def;
                // Keep all of them for freeing.
                def->fragment = NULL;
            }
            else if (elem->comp && elem->comp->type == XSLT_FUNC_VARIABLE)
            {
                if (elem->comp->inst && def->comp && def->comp->inst &&
                    xml_node_dom(elem->comp->inst) == xml_node_dom(def->comp->inst))
                {
                    xslt1_transform_error(t, style, elem->comp->inst, "Global variable %s already defined\n", elem->name);
                    style->errors++;
                }
            }
            free(key);
        }
    }
    // Evaluate them in declaration order (the list is reversed).
    XsltStackElem *elem = head;
    while (elem)
    {
        XsltStackElem *next = elem->next;
        xslt_eval_global_variable(elem, t);
        elem->next = NULL;
        elem = next;
    }
    return 0;
}

/** Port of xsltBuildVariable. */
static XsltStackElem *xslt_build_variable(XmlLibxslt1Transform *t, XsltStylePreComp *comp, XMQNode *tree)
{
    XsltStackElem *elem = xslt_new_stack_elem();
    elem->comp = comp;
    elem->name = comp->name;
    elem->select = comp->select;
    elem->name_uri = comp->ns;
    elem->tree = tree;
    elem->value = xslt_eval_variable(t, elem, comp);
    elem->computed = 1;
    return elem;
}

/** Port of xsltRegisterVariable. */
static int xslt_register_variable(XmlLibxslt1Transform *t, XsltStylePreComp *comp, XMQNode *tree, int is_param)
{
    int present = xslt_check_stack_elem(t, comp->name, comp->ns);
    if (is_param == 0)
    {
        if (present != 0 && present != 3)
        {
            xslt1_transform_error(t, NULL, comp->inst, "XSLT-variable: Redefinition of variable '%s'.\n", comp->name);
            return 0;
        }
    }
    else if (present != 0)
    {
        if (present == 1 || present == 2)
        {
            xslt1_transform_error(t, NULL, comp->inst, "XSLT-param: Redefinition of parameter '%s'.\n", comp->name);
            return 0;
        }
        return 0;
    }
    XsltStackElem *variable = xslt_build_variable(t, comp, tree);
    xslt_add_stack_elem(t, variable);
    return 0;
}

/** Port of xsltGlobalVariableLookup (the value is borrowed). */
static const XmlXPathResult *xslt_global_variable_lookup(XmlLibxslt1Transform *t, const char *name, const char *ns_uri)
{
    if (!t->global_vars) return NULL;
    char *key = xslt1_key2(name, ns_uri);
    XsltStackElem *elem = (XsltStackElem*)hashmap_get(t->global_vars, key);
    free(key);
    if (!elem) return NULL;
    if (elem->computed == 0)
    {
        if (elem->name == xslt_computing_global_var_marker_)
        {
            xslt1_transform_error(t, NULL, elem->comp ? elem->comp->inst : NULL, "Recursive definition of %s\n", name);
            return NULL;
        }
        return xslt_eval_global_variable(elem, t);
    }
    return elem->value;
}

/** Port of xsltParseStylesheetCallerParam (xsl:with-param). */
static XsltStackElem *xslt_parse_stylesheet_caller_param(XmlLibxslt1Transform *t, XMQNode *inst)
{
    if (!inst || xml_node_kind(inst) != XML_DOM_ELEMENT) return NULL;
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(inst);
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltParseStylesheetCallerParam(): "
                              "The XSLT 'with-param' instruction was not compiled.\n");
        return NULL;
    }
    if (!comp->name)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltParseStylesheetCallerParam(): "
                              "XSLT 'with-param': The attribute 'name' was not compiled.\n");
        return NULL;
    }
    XMQNode *tree = comp->select ? inst : xml_node_first_child(inst);
    return xslt_build_variable(t, comp, tree);
}

/** Port of xsltParseStylesheetVariable (a local xsl:variable). */
static void xslt_parse_stylesheet_variable(XmlLibxslt1Transform *t, XMQNode *inst)
{
    if (!inst || xml_node_kind(inst) != XML_DOM_ELEMENT) return;
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(inst);
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltParseStylesheetVariable(): "
                              "The XSLT 'variable' instruction was not compiled.\n");
        return;
    }
    if (!comp->name)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltParseStylesheetVariable(): "
                              "The attribute 'name' was not compiled.\n");
        return;
    }
    xslt_register_variable(t, comp, xml_node_first_child(inst), 0);
}

/** Port of xsltParseStylesheetParam (a template xsl:param). */
static void xslt_parse_stylesheet_param(XmlLibxslt1Transform *t, XMQNode *cur)
{
    if (!cur || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(cur);
    if (!comp || comp == XSLT_EXT_MARKER || !comp->name)
    {
        xslt1_transform_error(t, NULL, cur, "Internal error in xsltParseStylesheetParam(): "
                              "The XSLT 'param' declaration was not compiled correctly.\n");
        return;
    }
    xslt_register_variable(t, comp, xml_node_first_child(cur), 1);
}

/** Port of xsltXPathVariableLookup, the variable callback of the xpath context. Only the
    first element of each stack entry is looked at, like libxslt. */
static const XmlXPathResult *xslt_xpath_variable_lookup(void *user_data, const char *name, const char *ns_uri)
{
    XmlLibxslt1Transform *t = (XmlLibxslt1Transform*)user_data;
    if (!name) return NULL;
    if (t->vars_nr != 0)
    {
        XsltStackElem *variable = NULL;
        for (int i = t->vars_nr; i > t->vars_base; i--)
        {
            XsltStackElem *cur = t->vars_tab[i - 1];
            if (xslt1_streq(cur->name, name) && xslt1_streq(cur->name_uri, ns_uri))
            {
                variable = cur;
                break;
            }
        }
        if (variable)
        {
            if (variable->computed == 0)
            {
                variable->value = xslt_eval_variable(t, variable, NULL);
                variable->computed = 1;
            }
            return variable->value;
        }
    }
    const XmlXPathResult *value = NULL;
    if (t->global_vars) value = xslt_global_variable_lookup(t, name, ns_uri);
    if (!value)
    {
        if (ns_uri) xslt1_transform_error(t, NULL, t->inst, "Variable '{%s}%s' has not been declared.\n", ns_uri, name);
        else xslt1_transform_error(t, NULL, t->inst, "Variable '%s' has not been declared.\n", name);
    }
    return value;
}

// Namespaces at runtime, port of libxslt namespaces.c //////////////

/** The alias of a literal namespace over the import cascade, NULL if none (XSLT_UNDEFINED_DEFAULT_NS for #default). */
static const char *xslt_lookup_alias(XmlLibxslt1Transform *t, const char *href)
{
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        if (style->ns_aliases && href)
        {
            const char *uri = (const char*)hashmap_get(style->ns_aliases, href);
            if (uri) return uri;
        }
    }
    return NULL;
}

/** Port of xsltGetSpecialNamespace: find or declare a namespace binding for nsName and
    nsPrefix in scope of target (an element of the result). */
static XmlNs *xslt_get_special_namespace(XmlLibxslt1Transform *t, XMQNode *invoc_node, const char *ns_name,
                                         const char *ns_prefix, XMQNode *target)
{
    if (!lx_is_element(target)) return NULL;
    XmlNs *ns;
    int prefix_occupied = 0;
    if (!ns_prefix && (!ns_name || !ns_name[0]))
    {
        // Ensure that the default namespace is not in scope (an undeclaration if needed).
        for (ns = xml_node_first_ns_decl(target); ns; ns = xml_ns_next_decl(ns))
        {
            if (!xml_ns_prefix(ns))
            {
                if (xml_ns_href(ns) && xml_ns_href(ns)[0])
                {
                    xslt1_transform_error(t, NULL, invoc_node, "Namespace normalization error: Cannot undeclare "
                                          "the default namespace, since the default namespace '%s' is already declared on the result element "
                                          "'%s'.\n", xml_ns_href(ns), xml_node_name(target));
                }
                return NULL;
            }
        }
        XMQNode *parent = lx_parent(target);
        if (lx_is_element(parent))
        {
            if (!xml_node_ns(parent)) return NULL;
            ns = lx_search_ns(parent, NULL);
            if (!ns || !xml_ns_href(ns) || !xml_ns_href(ns)[0]) return NULL;
            lx_new_ns(target, "", NULL);
            return NULL;
        }
        return NULL;
    }
    if (ns_prefix && !strcmp(ns_prefix, "xml")) return lx_search_ns(target, ns_prefix);
    for (ns = xml_node_first_ns_decl(target); ns; ns = xml_ns_next_decl(ns))
    {
        if ((xml_ns_prefix(ns) == NULL) == (ns_prefix == NULL))
        {
            if (xslt1_streq(xml_ns_prefix(ns), ns_prefix))
            {
                if (xslt1_streq(xml_ns_href(ns), ns_name)) return ns;
                prefix_occupied = 1;
                break;
            }
        }
    }
    XMQNode *parent = lx_parent(target);
    if (prefix_occupied)
    {
        ns = lx_search_ns_by_href(target, ns_name);
        if (ns) return ns;
    }
    else if (lx_is_element(parent))
    {
        XmlNs *pns = xml_node_ns(parent);
        if (pns && ((xml_ns_prefix(pns) != NULL) == (ns_prefix != NULL)))
        {
            if (!ns_prefix)
            {
                if (xslt1_streq(xml_ns_href(pns), ns_name)) return pns;
            }
            else if (xslt1_streq(xml_ns_prefix(pns), ns_prefix) && xslt1_streq(xml_ns_href(pns), ns_name))
            {
                return pns;
            }
        }
        ns = lx_search_ns(parent, ns_prefix);
        if (ns)
        {
            if (xslt1_streq(xml_ns_href(ns), ns_name)) return ns;
            // The prefix is bound to another namespace, check that no attribute of the target uses it.
            for (XMQAttr *attr = xml_node_first_attr(target); attr; attr = xml_attr_next(attr))
            {
                XmlNs *ans = xml_node_ns(xml_attr_node(attr));
                if (ans && xslt1_streq(xml_ns_prefix(ans), ns_prefix))
                {
                    ns = lx_search_ns_by_href(target, ns_name);
                    if (ns) return ns;
                    goto declare_new_prefix;
                }
            }
        }
        return lx_new_ns(target, ns_name, ns_prefix);
    }
    else
    {
        return lx_new_ns(target, ns_name, ns_prefix);
    }
declare_new_prefix:
    {
        char pref[30];
        int counter = 1;
        if (!ns_prefix) ns_prefix = "ns";
        do
        {
            snprintf(pref, 30, "%s_%d", ns_prefix, counter++);
            ns = lx_search_ns(target, pref);
            if (counter > 1000)
            {
                xslt1_transform_error(t, NULL, invoc_node, "Internal error in xsltAcquireResultInScopeNs(): "
                                      "Failed to compute a unique ns-prefix for the generated element");
                return NULL;
            }
        } while (ns);
        return lx_new_ns(target, ns_name, pref);
    }
}

/** Port of xsltGetNamespace: the namespace of a literal result element or attribute, with aliases applied. */
static XmlNs *xslt_get_namespace(XmlLibxslt1Transform *t, XMQNode *cur, XmlNs *ns, XMQNode *out)
{
    if (!ns || !cur || !out) return NULL;
    const char *uri = xslt_lookup_alias(t, xml_ns_href(ns));
    if (uri && !strcmp(uri, XSLT_UNDEFINED_DEFAULT_NS)) return xslt_get_special_namespace(t, cur, NULL, NULL, out);
    if (!uri) uri = xml_ns_href(ns);
    return xslt_get_special_namespace(t, cur, uri, xml_ns_prefix(ns), out);
}

/** Port of xsltCopyNamespaceList: copy the declarations of a literal result element (not the
    xslt namespace, with aliases applied, only those not in scope). */
static void xslt_copy_namespace_list(XmlLibxslt1Transform *t, XMQNode *node, XmlNs *cur)
{
    if (!lx_is_element(node)) node = NULL;
    for (; cur; cur = xml_ns_next_decl(cur))
    {
        if (node)
        {
            XmlNs *nns = xml_node_ns(node);
            if (nns && xslt1_streq(xml_ns_prefix(nns), xml_ns_prefix(cur)) && xslt1_streq(xml_ns_href(nns), xml_ns_href(cur))) continue;
            XmlNs *tmp = lx_search_ns(node, xml_ns_prefix(cur));
            if (tmp && xslt1_streq(xml_ns_href(tmp), xml_ns_href(cur))) continue;
        }
        if (!xslt1_streq(xml_ns_href(cur), XSLT_NAMESPACE))
        {
            // Only the aliases of the top stylesheet (TODO in libxslt: apply cascading).
            const char *uri = (t->style->ns_aliases && xml_ns_href(cur)) ? (const char*)hashmap_get(t->style->ns_aliases, xml_ns_href(cur)) : NULL;
            if (uri && !strcmp(uri, XSLT_UNDEFINED_DEFAULT_NS)) continue;
            if (node) lx_new_ns(node, uri ? uri : xml_ns_href(cur), xml_ns_prefix(cur));
        }
    }
}

/** Port of xsltCopyNamespaceListInternal: copy declarations that are not in scope. */
static void xslt_copy_namespace_list_internal(XMQNode *elem, XmlNs *ns)
{
    if (!lx_is_element(elem)) elem = NULL;
    for (; ns; ns = xml_ns_next_decl(ns))
    {
        if (elem)
        {
            XmlNs *ens = xml_node_ns(elem);
            if (ens && xslt1_streq(xml_ns_prefix(ens), xml_ns_prefix(ns)) && xslt1_streq(xml_ns_href(ens), xml_ns_href(ns))) continue;
            XmlNs *lu = lx_search_ns(elem, xml_ns_prefix(ns));
            if (lu && xslt1_streq(xml_ns_href(lu), xml_ns_href(ns))) continue;
            lx_new_ns(elem, xml_ns_href(ns), xml_ns_prefix(ns));
        }
    }
}

// Copying, port of libxslt transform.c //////////////////////////////

/** Is target an element whose text goes into cdata sections (xsl:output cdata-section-elements)? */
static bool xslt_is_cdata_target(XmlLibxslt1Transform *t, XMQNode *target)
{
    if (t->type != XSLT_OUTPUT_XML || !t->style->cdata_section || !lx_is_element(target)) return false;
    XmlNs *ns = xml_node_ns(target);
    char *key = xslt1_key2(xml_node_name(target), ns ? xml_ns_href(ns) : NULL);
    bool r = hashmap_get(t->style->cdata_section, key) != NULL;
    free(key);
    return r;
}

/** Port of xsltAddTextString: append the string to the text (or cdata) node target. */
static XMQNode *xslt_add_text_string(XMQNode *target, const char *string, size_t len)
{
    if (len == 0 || !string || !target) return target;
    xml_node_append_content(target, string, len);
    return target;
}

/** Port of xsltCopyTextString. */
static XMQNode *xslt_copy_text_string(XmlLibxslt1Transform *t, XMQNode *target, const char *string, int noescape)
{
    if (!string) return NULL;
    size_t len = strlen(string);
    XMQNode *copy;
    XMQNode *last = target ? xml_node_last_child(target) : NULL;
    if (xslt_is_cdata_target(t, target))
    {
        // Coalesce with a preceding cdata section.
        if (last && xml_node_kind(last) == XML_DOM_CDATA) return xslt_add_text_string(last, string, len);
        copy = xml_node_new_cdata(t->output, string, len);
    }
    else if (noescape)
    {
        if (last && xml_node_kind(last) == XML_DOM_TEXT && lx_is_noenc(last)) return xslt_add_text_string(last, string, len);
        copy = xml_node_new_text_len(t->output, string, len);
        if (copy) xml_node_set_name(copy, "textnoenc");
    }
    else
    {
        if (last && xml_node_kind(last) == XML_DOM_TEXT && !lx_is_noenc(last)) return xslt_add_text_string(last, string, len);
        copy = xml_node_new_text_len(t->output, string, len);
    }
    if (copy && target) copy = lx_add_child(target, copy);
    if (!copy) xslt1_transform_error(t, NULL, target, "xsltCopyTextString: text copy failed\n");
    return copy;
}

/** Port of xsltCopyText: copy a text or cdata node. */
static XMQNode *xslt_copy_text(XmlLibxslt1Transform *t, XMQNode *target, XMQNode *cur)
{
    XmlNodeKind k = xml_node_kind(cur);
    if (k != XML_DOM_TEXT && k != XML_DOM_CDATA) return NULL;
    const char *content = xml_node_content(cur);
    if (!content) return NULL;
    XMQNode *copy = NULL;
    XMQNode *last = target ? xml_node_last_child(target) : NULL;
    size_t len = strlen(content);
    if (xslt_is_cdata_target(t, target))
    {
        if (last && xml_node_kind(last) == XML_DOM_CDATA)
        {
            copy = xslt_add_text_string(last, content, len);
            goto exit;
        }
        copy = xml_node_new_cdata(t->output, content, len);
    }
    else if (last &&
             ((xml_node_kind(last) == XML_DOM_TEXT && k == XML_DOM_TEXT && lx_is_noenc(last) == lx_is_noenc(cur)) ||
              // non-escaped text nodes and cdata sections
              (xml_node_kind(last) == XML_DOM_CDATA && k == XML_DOM_TEXT && lx_is_noenc(cur))))
    {
        copy = xslt_add_text_string(last, content, len);
        goto exit;
    }
    else
    {
        copy = xml_node_new_text_len(t->output, content, len);
        if (copy && k == XML_DOM_TEXT && lx_is_noenc(cur)) xml_node_set_name(copy, "textnoenc");
    }
    if (copy)
    {
        if (target) copy = lx_add_child(target, copy);
    }
    else
    {
        xslt1_transform_error(t, NULL, target, "xsltCopyText: text copy failed\n");
    }
exit:
    if (!copy)
    {
        xslt1_transform_error(t, NULL, target, "Internal error in xsltCopyText(): Failed to copy the string.\n");
        t->state = XSLT_STATE_STOPPED;
    }
    return copy;
}

/** Port of xsltShallowCopyAttr. */
static XMQAttr *xslt_shallow_copy_attr(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *target, XMQNode *attr)
{
    if (!attr) return NULL;
    if (!lx_is_element(target))
    {
        xslt1_transform_error(t, NULL, invoc_node, "Cannot add an attribute node to a non-element node.\n");
        return NULL;
    }
    if (xml_node_first_child(target))
    {
        xslt1_transform_error(t, NULL, invoc_node, "Attribute nodes must be added before any child nodes to an element.\n");
        return NULL;
    }
    char *value = lx_attr_value(attr);
    XMQAttr *copy;
    XmlNs *ans = xml_node_ns(attr);
    if (ans)
    {
        XmlNs *ns = xslt_get_special_namespace(t, invoc_node, xml_ns_href(ans), xml_ns_prefix(ans), target);
        if (!ns)
        {
            xslt1_transform_error(t, NULL, invoc_node, "Namespace fixup error: Failed to acquire an in-scope "
                                  "namespace binding of the copied attribute '{%s}%s'.\n", xml_ns_href(ans), xml_node_name(attr));
        }
        copy = lx_set_ns_prop(target, ns, xml_node_name(attr), value);
    }
    else
    {
        copy = lx_set_ns_prop(target, NULL, xml_node_name(attr), value);
    }
    free(value);
    return copy;
}

/** Port of xsltCopyAttrListNoOverwrite. */
static int xslt_copy_attr_list_no_overwrite(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *target, XMQAttr *attr)
{
    XmlNs *orig_ns = NULL, *copy_ns = NULL;
    for (; attr; attr = xml_attr_next(attr))
    {
        XMQNode *an = xml_attr_node(attr);
        XmlNs *ans = xml_node_ns(an);
        if (ans != orig_ns)
        {
            orig_ns = ans;
            if (ans)
            {
                copy_ns = xslt_get_special_namespace(t, invoc_node, xml_ns_href(ans), xml_ns_prefix(ans), target);
                if (!copy_ns) return -1;
            }
            else
            {
                copy_ns = NULL;
            }
        }
        char *value = lx_attr_value(an);
        lx_new_ns_prop(target, copy_ns, xml_attr_name(attr), value);
        free(value);
    }
    return 0;
}

/** Port of xsltShallowCopyElem. */
static XMQNode *xslt_shallow_copy_elem(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *insert, int is_lre)
{
    XmlNodeKind k = xml_node_kind(node);
    if (k == XML_DOM_DTD || !insert) return NULL;
    if (k == XML_DOM_TEXT || k == XML_DOM_CDATA) return xslt_copy_text(t, insert, node);
    XMQNode *copy = lx_shallow_copy(t->output, node);
    if (copy)
    {
        copy = lx_add_child(insert, copy);
        if (!copy)
        {
            xslt1_transform_error(t, NULL, node, "xsltShallowCopyElem: copy failed\n");
            return copy;
        }
        if (k == XML_DOM_ELEMENT)
        {
            XmlNs *defs = xml_node_first_ns_decl(node);
            if (defs)
            {
                if (is_lre) xslt_copy_namespace_list(t, copy, defs);
                else xslt_copy_namespace_list_internal(copy, defs);
            }
            XmlNs *nns = xml_node_ns(node);
            if (nns)
            {
                if (is_lre) xml_node_set_ns(copy, xslt_get_namespace(t, node, nns, copy));
                else xml_node_set_ns(copy, xslt_get_special_namespace(t, node, xml_ns_href(nns), xml_ns_prefix(nns), copy));
            }
            else if (lx_is_element(insert) && xml_node_ns(insert))
            {
                xslt_get_special_namespace(t, node, NULL, NULL, copy);
            }
        }
    }
    else
    {
        xslt1_transform_error(t, NULL, node, "xsltShallowCopyElem: copy %s failed\n", xml_node_name(node));
    }
    return copy;
}

static XMQNode *xslt_copy_tree(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *node, XMQNode *insert,
                               int is_lre, int top_elem_visited);

/** Port of xsltCopyTreeList. */
static XMQNode *xslt_copy_tree_list(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *list, XMQNode *insert,
                                    int is_lre, int top_elem_visited)
{
    XMQNode *ret = NULL;
    while (list)
    {
        XMQNode *next = xml_node_next(list);
        XMQNode *copy = xslt_copy_tree(t, invoc_node, list, insert, is_lre, top_elem_visited);
        if (copy && !ret) ret = copy;
        list = next;
    }
    return ret;
}

/** Port of xsltShallowCopyNsNode: copy a namespace node as a declaration on insert. */
static XmlNs *xslt_shallow_copy_ns_node(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *insert, XMQNode *ns)
{
    if (!lx_is_element(insert)) return NULL;
    if (xml_node_first_child(insert))
    {
        xslt1_transform_error(t, NULL, invoc_node, "Namespace nodes must be added before any child nodes are added to an element.\n");
        return NULL;
    }
    // A namespace node has the prefix as name and the uri as content.
    const char *prefix = xml_node_name(ns);
    const char *href = xml_node_content(ns);
    if (!prefix)
    {
        if (!xml_node_ns(insert)) return NULL;
    }
    else if (!strcmp(prefix, "xml"))
    {
        return NULL;
    }
    for (XmlNs *tmp = xml_node_first_ns_decl(insert); tmp; tmp = xml_ns_next_decl(tmp))
    {
        if ((xml_ns_prefix(tmp) == NULL) == (prefix == NULL))
        {
            if (xslt1_streq(xml_ns_prefix(tmp), prefix))
            {
                if (xslt1_streq(xml_ns_href(tmp), href)) return NULL;
                return NULL; // occupied
            }
        }
    }
    XmlNs *tmp = lx_search_ns(insert, prefix);
    if (tmp && xslt1_streq(xml_ns_href(tmp), href)) return NULL;
    return lx_new_ns(insert, href, prefix);
}

/** Collect the namespaces in scope of node like xmlGetNsList: innermost first, unique prefixes. */
static XmlNs **lx_get_ns_list(XMQNode *node, int *num)
{
    XmlNs **list = NULL;
    int n = 0, max = 0;
    for (; node; node = xml_node_parent(node))
    {
        if (xml_node_kind(node) != XML_DOM_ELEMENT) continue;
        for (XmlNs *cur = xml_node_first_ns_decl(node); cur; cur = xml_ns_next_decl(cur))
        {
            int i;
            for (i = 0; i < n; i++)
            {
                if (xslt1_streq(xml_ns_prefix(cur), xml_ns_prefix(list[i]))) break;
            }
            if (i >= n)
            {
                if (n >= max)
                {
                    max = max ? 2 * max : 10;
                    list = (XmlNs**)realloc(list, (size_t)(max + 1) * sizeof(XmlNs*));
                }
                list[n++] = cur;
                list[n] = NULL;
            }
        }
    }
    *num = n;
    return list;
}

/** Port of xsltCopyTree: a deep copy of node into insert. */
static XMQNode *xslt_copy_tree(XmlLibxslt1Transform *t, XMQNode *invoc_node, XMQNode *node, XMQNode *insert,
                               int is_lre, int top_elem_visited)
{
    if (!node) return NULL;
    switch (xml_node_kind(node))
    {
    case XML_DOM_ELEMENT:
    case XML_DOM_ENTITY_REF:
    case XML_DOM_PI:
    case XML_DOM_COMMENT:
    case XML_DOM_DOCUMENT:
        break;
    case XML_DOM_TEXT:
        return xslt_copy_text_string(t, insert, xml_node_content(node), lx_is_noenc(node));
    case XML_DOM_CDATA:
        return xslt_copy_text_string(t, insert, xml_node_content(node), 0);
    case XML_DOM_ATTRIBUTE:
        return xslt_shallow_copy_attr(t, invoc_node, insert, node) ? insert : NULL;
    case XML_DOM_NAMESPACE:
        return xslt_shallow_copy_ns_node(t, invoc_node, insert, node) ? insert : NULL;
    default:
        return NULL;
    }
    if (xml_node_kind(node) == XML_DOM_DOCUMENT)
    {
        // libxslt only copies the children of result tree fragments, xmlDocCopyNode of a document fails.
        if (xslt1_is_rtf(t, xml_node_dom(node)))
        {
            XMQNode *c = xml_node_first_child(node);
            return c ? xslt_copy_tree_list(t, invoc_node, c, insert, 0, 0) : NULL;
        }
        xslt1_transform_error(t, NULL, invoc_node, "xsltCopyTree: Copying of '%s' failed.\n", "(null)");
        return NULL;
    }
    XMQNode *copy = lx_shallow_copy(t->output, node);
    if (copy)
    {
        copy = lx_add_child(insert, copy);
        if (!copy)
        {
            xslt1_transform_error(t, NULL, invoc_node, "xsltCopyTree: Copying of '%s' failed.\n", xml_node_name(node));
            return copy;
        }
        if (xml_node_last_child(insert) != copy) return xml_node_last_child(insert);
        if (xml_node_kind(node) == XML_DOM_ELEMENT)
        {
            XMQNode *parent = xml_node_parent(node);
            XmlNs *nns = xml_node_ns(node);
            if (top_elem_visited == 0 && parent && xml_node_kind(parent) != XML_DOM_DOCUMENT)
            {
                // The top element of a copied subtree gets all namespaces in scope.
                int num = 0;
                XmlNs **ns_list = lx_get_ns_list(node, &num);
                for (int i = 0; i < num; i++)
                {
                    XmlNs *ns = lx_search_ns(insert, xml_ns_prefix(ns_list[i]));
                    if (!ns || !xslt1_streq(xml_ns_href(ns), xml_ns_href(ns_list[i]))) ns = NULL;
                    if (!ns) ns = lx_new_ns(copy, xml_ns_href(ns_list[i]), xml_ns_prefix(ns_list[i]));
                    if (nns == ns_list[i]) xml_node_set_ns(copy, ns);
                }
                free(ns_list);
            }
            else if (xml_node_first_ns_decl(node))
            {
                if (is_lre) xslt_copy_namespace_list(t, copy, xml_node_first_ns_decl(node));
                else xslt_copy_namespace_list_internal(copy, xml_node_first_ns_decl(node));
            }
            if (nns)
            {
                if (!xml_node_ns(copy))
                {
                    xml_node_set_ns(copy, xslt_get_special_namespace(t, invoc_node, xml_ns_href(nns), xml_ns_prefix(nns), copy));
                }
            }
            else if (lx_is_element(insert) && xml_node_ns(insert))
            {
                xslt_get_special_namespace(t, invoc_node, NULL, NULL, copy);
            }
            if (xml_node_first_attr(node)) xslt_copy_attr_list_no_overwrite(t, invoc_node, copy, xml_node_first_attr(node));
            if (top_elem_visited == 0) top_elem_visited = 1;
        }
        if (lx_children(node)) xslt_copy_tree_list(t, invoc_node, lx_children(node), copy, is_lre, top_elem_visited);
    }
    else
    {
        xslt1_transform_error(t, NULL, invoc_node, "xsltCopyTree: Copying of '%s' failed.\n", xml_node_name(node));
    }
    return copy;
}

// Templates and attribute value templates, port of libxslt templates.c and attrvt.c

/** Port of xsltEvalXPathStringNs, with the namespaces compiled into the expression. */
static char *xslt_eval_xpath_string(XmlLibxslt1Transform *t, const XmlXPathComp *comp)
{
    if (!t->inst)
    {
        xslt1_transform_error(t, NULL, NULL, "xsltEvalXPathStringNs: No context or instruction\n");
        return NULL;
    }
    XMQNode *old_inst = t->inst;
    long pos = t->xp_position, size = t->xp_size;
    char *ret = NULL;
    XmlXPathResult *res = xslt_xpath_eval(t, comp, t->node);
    if (res)
    {
        ret = xml_xpath_result_to_string(res);
        xml_xpath_result_free(res);
    }
    else
    {
        t->state = XSLT_STATE_STOPPED;
    }
    t->inst = old_inst;
    t->xp_position = pos;
    t->xp_size = size;
    return ret;
}

/** Port of xsltEvalTemplateString: instantiate the content of inst and return its text. */
static char *xslt_eval_template_string(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst)
{
    if (!context_node || !inst || xml_node_kind(inst) != XML_DOM_ELEMENT) return NULL;
    if (!xml_node_first_child(inst)) return NULL;
    // A temporary element to add the resulting text content to.
    XMQNode *insert = xml_node_new_element(t->output, "fake", NULL);
    XMQNode *old_insert = t->insert;
    t->insert = insert;
    xslt_apply_one_template(t, context_node, xml_node_first_child(inst), NULL);
    t->insert = old_insert;
    char *ret = lx_node_get_content(insert);
    xml_node_free(insert);
    return ret;
}

/** Port of xsltAttrTemplateValueProcessNode: evaluate the attribute value template str at runtime. */
static char *xslt_attr_template_value_process_node(XmlLibxslt1Transform *t, const char *str, XMQNode *inst)
{
    char *ret = NULL;
    if (!str) return NULL;
    if (!*str) return xslt1_strdup("");
    const char *cur = str;
    while (*cur)
    {
        if (*cur == '{')
        {
            if (cur[1] == '{')
            {
                // An escaped {.
                cur++;
                ret = xslt1_strncat(ret, str, (size_t)(cur - str));
                cur++;
                str = cur;
                continue;
            }
            ret = xslt1_strncat(ret, str, (size_t)(cur - str));
            str = cur;
            cur++;
            while (*cur && *cur != '}')
            {
                // Need to check for literals (bug539741).
                if (*cur == '\'' || *cur == '"')
                {
                    char delim = *cur++;
                    while (*cur && *cur != delim) cur++;
                    if (*cur) cur++;
                }
                else
                {
                    cur++;
                }
            }
            if (!*cur)
            {
                xslt1_transform_error(t, NULL, inst, "xsltAttrTemplateValueProcessNode: unmatched '{'\n");
                ret = xslt1_strncat(ret, str, (size_t)(cur - str));
                goto exit;
            }
            str++;
            char *expr = xslt1_strndup(str, (size_t)(cur - str));
            if (*expr == '{')
            {
                ret = xslt1_strcat(ret, expr);
                free(expr);
            }
            else
            {
                XmlXPathComp *comp = xslt1_xpath_compile(&t->style->settings, inst, expr, false);
                char *val = comp ? xslt_eval_xpath_string(t, comp) : NULL;
                if (!comp) t->state = XSLT_STATE_STOPPED;
                xml_xpath_comp_free(comp);
                free(expr);
                if (val)
                {
                    ret = xslt1_strcat(ret, val);
                    free(val);
                }
            }
            cur++;
            str = cur;
        }
        else if (*cur == '}')
        {
            cur++;
            if (*cur == '}')
            {
                // An escaped }.
                ret = xslt1_strncat(ret, str, (size_t)(cur - str));
                cur++;
                str = cur;
                continue;
            }
            else
            {
                xslt1_transform_error(t, NULL, inst, "xsltAttrTemplateValueProcessNode: unmatched '}'\n");
            }
        }
        else
        {
            cur++;
        }
    }
    if (cur != str) ret = xslt1_strncat(ret, str, (size_t)(cur - str));
exit:
    return ret;
}

/** Port of xsltEvalAttrValueTemplate: the attribute name (in namespace ns) of inst as an AVT. */
static char *xslt_eval_attr_value_template(XmlLibxslt1Transform *t, XMQNode *inst, const char *name, const char *ns)
{
    if (!inst || !name || xml_node_kind(inst) != XML_DOM_ELEMENT) return NULL;
    char *expr = xslt1_get_ns_prop(inst, name, ns);
    if (!expr) return NULL;
    char *ret = xslt_attr_template_value_process_node(t, expr, inst);
    free(expr);
    return ret;
}

/** Port of xsltEvalAVT: evaluate a precompiled attribute value template. */
static char *xslt_eval_avt(XmlLibxslt1Transform *t, XsltAttrVT *avt, XMQNode *node)
{
    if (!avt || !node) return NULL;
    char *ret = NULL;
    int str = avt->strstart;
    for (int i = 0; i < avt->nb_seg; i++)
    {
        if (str)
        {
            ret = xslt1_strcat(ret, (const char*)avt->segments[i]);
        }
        else
        {
            XmlXPathComp *comp = (XmlXPathComp*)avt->segments[i];
            char *tmp = NULL;
            if (comp) tmp = xslt_eval_xpath_string(t, comp);
            else t->state = XSLT_STATE_STOPPED;
            if (tmp)
            {
                if (ret)
                {
                    ret = xslt1_strcat(ret, tmp);
                    free(tmp);
                }
                else
                {
                    ret = tmp;
                }
            }
        }
        str = !str;
    }
    return ret;
}

/** Port of xsltAttrListTemplateProcess: the attributes of a literal result element. */
static void xslt_attr_list_template_process(XmlLibxslt1Transform *t, XMQNode *target, XMQAttr *attrs)
{
    if (!lx_is_element(target) || !attrs) return;
    XMQNode *old_insert = t->insert;
    t->insert = target;
    // Apply attribute sets.
    for (XMQAttr *attr = attrs; attr; attr = xml_attr_next(attr))
    {
        XmlNs *ans = xml_node_ns(xml_attr_node(attr));
        if (ans && !strcmp(xml_attr_name(attr), "use-attribute-sets") && xslt1_streq(xml_ns_href(ans), XSLT_NAMESPACE))
        {
            xslt_apply_attribute_set(t, t->node, xml_attr_node(attr), NULL);
        }
    }
    bool has_attr = xml_node_first_attr(target) != NULL;
    // Instantiate the literal result element attributes.
    XmlNs *orig_ns = NULL, *copy_ns = NULL;
    for (XMQAttr *attr = attrs; attr; attr = xml_attr_next(attr))
    {
        XMQNode *an = xml_attr_node(attr);
        XmlNs *ans = xml_node_ns(an);
        if (ans && xslt1_streq(xml_ns_href(ans), XSLT_NAMESPACE)) continue;
        XMQNode *first = xml_node_first_child(an);
        const char *value = "";
        if (first)
        {
            if (xml_node_kind(first) != XML_DOM_TEXT || xml_node_next(first))
            {
                xslt1_transform_error(t, NULL, xml_attr_owner(attr), "Internal error: The children of an attribute node of a "
                                      "literal result element are not in the expected form.\n");
                goto error;
            }
            value = xml_node_content(first) ? xml_node_content(first) : "";
        }
        // Get the namespace, avoid lookups of the same namespaces.
        if (ans != orig_ns)
        {
            orig_ns = ans;
            if (ans)
            {
                copy_ns = xslt_get_namespace(t, xml_attr_owner(attr), ans, target);
                if (!copy_ns) goto error;
            }
            else
            {
                copy_ns = NULL;
            }
        }
        char *val;
        XsltAttrVT *avt = (XsltAttrVT*)xml_node_user_data(an);
        if (avt)
        {
            val = xslt_eval_avt(t, avt, xml_attr_owner(attr));
            if (!val)
            {
                if (ans) xslt1_transform_error(t, NULL, xml_attr_owner(attr), "Internal error: Failed to evaluate the AVT "
                                               "of attribute '{%s}%s'.\n", xml_ns_href(ans), xml_attr_name(attr));
                else xslt1_transform_error(t, NULL, xml_attr_owner(attr), "Internal error: Failed to evaluate the AVT "
                                           "of attribute '%s'.\n", xml_attr_name(attr));
                // libxslt has created the attribute with an empty value before failing.
                if (has_attr) lx_set_ns_prop(target, copy_ns, xml_attr_name(attr), "");
                else lx_new_ns_prop(target, copy_ns, xml_attr_name(attr), "");
                goto error;
            }
        }
        else
        {
            val = xslt1_strdup(value);
        }
        if (has_attr) lx_set_ns_prop(target, copy_ns, xml_attr_name(attr), val);
        else lx_new_ns_prop(target, copy_ns, xml_attr_name(attr), val);
        free(val);
    }
error:
    t->insert = old_insert;
}

// Attributes, port of libxslt attributes.c (xsltAttribute, xsltApplyAttributeSet).

/** Port of xsltAttribute (xsl:attribute). */
static void xslt_attribute(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!context_node || !inst || xml_node_kind(inst) != XML_DOM_ELEMENT) return;
    if (!comp || comp == XSLT_EXT_MARKER)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltAttribute(): The XSLT 'attribute' instruction was not compiled.\n");
        return;
    }
    if (!comp->has_name) return;
    if (!t->insert) return;
    XMQNode *target_elem = t->insert;
    if (xml_node_kind(target_elem) != XML_DOM_ELEMENT) return;
    if (xml_node_first_child(target_elem))
    {
        xslt1_transform_error(t, NULL, inst, "xsl:attribute: Cannot add attributes to an element if children have been already added "
                              "to the element.\n");
        return;
    }
    char *name = NULL, *prefix = NULL;
    const char *ns_name = NULL;
    char *ns_name_owned = NULL;
    if (!comp->name)
    {
        char *prop = xslt_eval_attr_value_template(t, inst, "name", XSLT_NAMESPACE);
        if (!prop)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: The attribute 'name' is missing.\n");
            goto error;
        }
        if (xslt1_validate_qname(prop))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: The effective name '%s' is not a valid QName.\n", prop);
            // Fall through to catch any further errors, if possible.
        }
        if (!strcmp(prop, "xmlns"))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: The effective name 'xmlns' is not allowed.\n");
            free(prop);
            goto error;
        }
        name = xslt1_split_qname(prop, &prefix);
        free(prop);
    }
    else
    {
        name = xslt1_split_qname(comp->name, &prefix);
    }
    if (comp->has_ns)
    {
        if (comp->ns)
        {
            if (comp->ns[0]) ns_name = comp->ns;
        }
        else
        {
            char *tmp = xslt_eval_attr_value_template(t, inst, "namespace", XSLT_NAMESPACE);
            if (tmp && tmp[0]) ns_name = ns_name_owned = tmp;
            else free(tmp);
        }
        if (xslt1_streq(ns_name, "http://www.w3.org/2000/xmlns/"))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: Namespace http://www.w3.org/2000/xmlns/ forbidden.\n");
            goto error;
        }
        if (xslt1_streq(ns_name, XML_DOM_XML_NAMESPACE))
        {
            free(prefix);
            prefix = xslt1_strdup("xml");
        }
        else if (xslt1_streq(prefix, "xml"))
        {
            free(prefix);
            prefix = NULL;
        }
    }
    else if (prefix)
    {
        XmlNs *ns = lx_search_ns(inst, prefix);
        if (!ns)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: The QName '%s:%s' has no namespace binding in scope in the stylesheet; "
                                  "this is an error, since the namespace was not specified by the instruction itself.\n", prefix, name);
        }
        else
        {
            ns_name = xml_ns_href(ns);
        }
    }
    XmlNs *ns;
    ns = NULL;
    if (ns_name)
    {
        if (!prefix || !strcmp(prefix, "xmlns")) ns = xslt_get_special_namespace(t, inst, ns_name, "ns_1", target_elem);
        else ns = xslt_get_special_namespace(t, inst, ns_name, prefix, target_elem);
        if (!ns)
        {
            xslt1_transform_error(t, NULL, inst, "Namespace fixup error: Failed to acquire an in-scope "
                                  "namespace binding for the generated attribute '{%s}%s'.\n", ns_name, name);
            goto error;
        }
    }
    XMQNode *first;
    first = xml_node_first_child(inst);
    if (!first)
    {
        lx_set_ns_prop(t->insert, ns, name, "");
    }
    else if (!xml_node_next(first) && (xml_node_kind(first) == XML_DOM_TEXT || xml_node_kind(first) == XML_DOM_CDATA))
    {
        lx_set_ns_prop(t->insert, ns, name, xml_node_content(first));
    }
    else
    {
        char *value = xslt_eval_template_string(t, context_node, inst);
        lx_set_ns_prop(t->insert, ns, name, value ? value : "");
        free(value);
    }
error:
    free(name);
    free(prefix);
    free(ns_name_owned);
}

/** Port of xsltApplyAttributeSet: the attribute sets named in attr_sets (or in the
    xsl:use-attribute-sets attribute inst). */
static void xslt_apply_attribute_set(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, const char *attr_sets)
{
    char *owned = NULL;
    if (!attr_sets)
    {
        if (!inst) return;
        if (xml_node_kind(inst) == XML_DOM_ATTRIBUTE)
        {
            XMQNode *first = xml_node_first_child(inst);
            if (first) attr_sets = xml_node_content(first);
        }
        if (!attr_sets) return;
    }
    // xmlSearchNs from an attribute starts at its element (and looks at the element's namespace).
    XMQNode *ns_node = inst;
    const char *curstr = attr_sets;
    while (*curstr)
    {
        while (xslt1_is_blank((unsigned char)*curstr)) curstr++;
        if (!*curstr) break;
        const char *endstr = curstr;
        while (*endstr && !xslt1_is_blank((unsigned char)*endstr)) endstr++;
        char *qname = xslt1_strndup(curstr, (size_t)(endstr - curstr));
        const char *ns_uri = NULL;
        if (xslt1_validate_qname(qname))
        {
            xslt1_transform_error(t, NULL, inst, "The name '%s' in use-attribute-sets is not a valid QName.\n", qname);
            free(qname);
            free(owned);
            return;
        }
        char *prefix = NULL;
        char *ncname = xslt1_split_qname(qname, &prefix);
        if (prefix)
        {
            XmlNs *ns = lx_search_ns(ns_node, prefix);
            if (!ns)
            {
                xslt1_transform_error(t, NULL, inst, "use-attribute-set : No namespace found for QName '%s:%s'\n", prefix, ncname);
                free(prefix);
                free(ncname);
                free(qname);
                free(owned);
                return;
            }
            ns_uri = xml_ns_href(ns);
        }
        XsltAttrSet *set = NULL;
        if (t->style->attribute_sets)
        {
            char *key = xslt1_key2(ncname, ns_uri);
            set = (XsltAttrSet*)hashmap_get(t->style->attribute_sets, key);
            free(key);
        }
        if (set)
        {
            for (XsltAttrElem *cur = set->attrs; cur; cur = cur->next)
            {
                if (cur->attr) xslt_attribute(t, node, cur->attr, (XsltStylePreComp*)xml_node_user_data(cur->attr));
            }
        }
        free(prefix);
        free(ncname);
        free(qname);
        curstr = endstr;
    }
    free(owned);
}

// Template application, port of libxslt transform.c ////////////////

static void templ_push(XmlLibxslt1Transform *t, XsltTemplate *value)
{
    if (t->templ_nr >= t->templ_max)
    {
        t->templ_max = t->templ_max == 0 ? 4 : t->templ_max * 2;
        t->templ_tab = (XsltTemplate**)realloc(t->templ_tab, (size_t)t->templ_max * sizeof(XsltTemplate*));
    }
    t->templ_tab[t->templ_nr++] = value;
    t->templ = value;
}

static void templ_pop(XmlLibxslt1Transform *t)
{
    if (t->templ_nr <= 0) return;
    t->templ_nr--;
    t->templ = t->templ_nr > 0 ? t->templ_tab[t->templ_nr - 1] : NULL;
    t->templ_tab[t->templ_nr] = NULL;
}

/** Port of xsltApplyFallbacks. */
static int xslt_apply_fallbacks(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst)
{
    int ret = 0;
    if (!node || !inst || !xml_node_first_child(inst)) return 0;
    for (XMQNode *child = xml_node_first_child(inst); child; child = xml_node_next(child))
    {
        if (xslt1_is_xslt_elem(child) && xslt1_is_xslt_name(child, "fallback"))
        {
            ret++;
            xslt_apply_sequence_constructor(t, node, xml_node_first_child(child), NULL);
        }
    }
    return ret;
}

/** Count the children libxslt counts as real nodes. */
static int xslt_count_real_children(XMQNode *node)
{
    int n = 0;
    for (XMQNode *cur = lx_children(node); cur; cur = xml_node_next(cur))
    {
        if (lx_is_real_node(cur)) n++;
    }
    return n;
}

/** Port of xsltDefaultProcessOneNode: the built in template rules. */
static void xslt_default_process_one_node(XmlLibxslt1Transform *t, XMQNode *node, XsltStackElem *params)
{
    if (t->state == XSLT_STATE_STOPPED) return;
    switch (xml_node_kind(node))
    {
    case XML_DOM_DOCUMENT:
    case XML_DOM_ELEMENT:
        break;
    case XML_DOM_CDATA:
        if (!xslt_copy_text(t, t->insert, node)) xslt1_transform_error(t, NULL, node, "xsltDefaultProcessOneNode: cdata copy failed\n");
        return;
    case XML_DOM_TEXT:
        if (!xslt_copy_text(t, t->insert, node)) xslt1_transform_error(t, NULL, node, "xsltDefaultProcessOneNode: text copy failed\n");
        return;
    case XML_DOM_ATTRIBUTE:
    {
        XMQNode *cur = xml_node_first_child(node);
        while (cur && xml_node_kind(cur) != XML_DOM_TEXT) cur = xml_node_next(cur);
        if (!cur)
        {
            xslt1_transform_error(t, NULL, node, "xsltDefaultProcessOneNode: no text for attribute\n");
        }
        else if (!xslt_copy_text(t, t->insert, cur))
        {
            xslt1_transform_error(t, NULL, node, "xsltDefaultProcessOneNode: text copy failed\n");
        }
        return;
    }
    default:
        return;
    }
    int nbchild = xslt_count_real_children(node);
    long old_size = t->xp_size, old_pos = t->xp_position;
    int childno = 0;
    for (XMQNode *cur = lx_children(node); cur; cur = xml_node_next(cur))
    {
        childno++;
        XsltTemplate *templ;
        switch (xml_node_kind(cur))
        {
        case XML_DOM_DOCUMENT:
        case XML_DOM_ELEMENT:
            t->xp_size = nbchild;
            t->xp_position = childno;
            if (t->depth >= t->max_template_depth)
            {
                xslt1_transform_error(t, NULL, cur, "xsltDefaultProcessOneNode: Maximum template depth exceeded.\n"
                                      "You can adjust xsltMaxDepth (--maxdepth) in order to raise the maximum number of nested template calls and "
                                      "variables/params (currently set to %d).\n", t->max_template_depth);
                t->state = XSLT_STATE_STOPPED;
                return;
            }
            t->depth++;
            xslt_process_one_node(t, cur, params);
            t->depth--;
            break;
        case XML_DOM_CDATA:
            templ = xslt1_get_template(t, cur, NULL);
            if (templ)
            {
                xslt_apply_xslt_template(t, cur, templ->content, templ, params);
            }
            else if (!xslt_copy_text(t, t->insert, cur))
            {
                xslt1_transform_error(t, NULL, cur, "xsltDefaultProcessOneNode: cdata copy failed\n");
            }
            break;
        case XML_DOM_TEXT:
            templ = xslt1_get_template(t, cur, NULL);
            if (templ)
            {
                t->xp_size = nbchild;
                t->xp_position = childno;
                xslt_apply_xslt_template(t, cur, templ->content, templ, params);
            }
            else if (!xslt_copy_text(t, t->insert, cur))
            {
                xslt1_transform_error(t, NULL, cur, "xsltDefaultProcessOneNode: text copy failed\n");
            }
            break;
        case XML_DOM_PI:
        case XML_DOM_COMMENT:
            templ = xslt1_get_template(t, cur, NULL);
            if (templ)
            {
                t->xp_size = nbchild;
                t->xp_position = childno;
                xslt_apply_xslt_template(t, cur, templ->content, templ, params);
            }
            break;
        default:
            break;
        }
    }
    t->xp_size = old_size;
    t->xp_position = old_pos;
}

/** Port of xsltProcessOneNode. */
static void xslt_process_one_node(XmlLibxslt1Transform *t, XMQNode *context_node, XsltStackElem *with_params)
{
    XsltTemplate *templ = xslt1_get_template(t, context_node, NULL);
    if (!templ)
    {
        XMQNode *old_node = t->node;
        t->node = context_node;
        xslt_default_process_one_node(t, context_node, with_params);
        t->node = old_node;
        return;
    }
    XsltTemplate *old_rule = t->current_template_rule;
    t->current_template_rule = templ;
    xslt_apply_xslt_template(t, context_node, templ->content, templ, with_params);
    t->current_template_rule = old_rule;
}

/** Port of xsltApplyXSLTTemplate. */
static void xslt_apply_xslt_template(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list,
                                     XsltTemplate *templ, XsltStackElem *with_params)
{
    if (!templ)
    {
        xslt1_transform_error(t, NULL, list, "xsltApplyXSLTTemplate: Bad arguments; @templ is mandatory.\n");
        return;
    }
    if (!list) return;
    if (t->state == XSLT_STATE_STOPPED) return;
    if (t->vars_nr >= t->max_template_vars)
    {
        xslt1_transform_error(t, NULL, list, "xsltApplyXSLTTemplate: A potential infinite template recursion was detected.\n"
                              "You can adjust maxTemplateVars (--maxvars) in order to raise the maximum number of variables/params (currently set to %d).\n",
                              t->max_template_vars);
        xslt_debug(t, context_node, list);
        t->state = XSLT_STATE_STOPPED;
        return;
    }
    XsltRVT *old_user_fragment_top = t->tmp_rvt;
    t->tmp_rvt = NULL;
    int old_vars_base = t->vars_base;
    t->vars_base = t->vars_nr;
    t->node = context_node;
    templ_push(t, templ);
    // The template parameters.
    XMQNode *cur = list;
    do
    {
        if (xml_node_kind(cur) == XML_DOM_TEXT)
        {
            cur = xml_node_next(cur);
            continue;
        }
        XsltStylePreComp *iparam = (XsltStylePreComp*)xml_node_user_data(cur);
        if (xml_node_kind(cur) != XML_DOM_ELEMENT || !iparam || !xslt1_streq(xml_node_name(cur), "param") || !xslt1_is_xslt_elem(cur))
        {
            break;
        }
        list = xml_node_next(cur);
        XsltStackElem *tmp_param = NULL;
        if (with_params && iparam != XSLT_EXT_MARKER)
        {
            for (tmp_param = with_params; tmp_param; tmp_param = tmp_param->next)
            {
                if (xslt1_streq(tmp_param->name, iparam->name) && xslt1_streq(tmp_param->name_uri, iparam->ns))
                {
                    xslt_local_variable_push(t, tmp_param, -1);
                    break;
                }
            }
        }
        if (!tmp_param) xslt_parse_stylesheet_param(t, cur);
        cur = xml_node_next(cur);
    } while (cur);
    xslt_apply_sequence_constructor(t, context_node, list, templ);
    if (t->vars_nr > t->vars_base) xslt_template_params_cleanup(t);
    t->vars_base = old_vars_base;
    // Fragments created by extension functions in the template.
    while (t->tmp_rvt)
    {
        XsltRVT *tmp = t->tmp_rvt;
        t->tmp_rvt = tmp->next;
        xslt_release_rvt(t, tmp);
    }
    t->tmp_rvt = old_user_fragment_top;
    templ_pop(t);
}

/** Port of xsltApplyOneTemplate. */
static void xslt_apply_one_template(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list, XsltStackElem *params)
{
    if (!list) return;
    if (t->state == XSLT_STATE_STOPPED) return;
    if (params)
    {
        int old_vars_nr = t->vars_nr;
        for (; params; params = params->next) xslt_local_variable_push(t, params, -1);
        xslt_apply_sequence_constructor(t, context_node, list, NULL);
        xslt_local_variable_pop(t, old_vars_nr, -2);
    }
    else
    {
        xslt_apply_sequence_constructor(t, context_node, list, NULL);
    }
}

// The instructions.

/** Port of xsltSort: xsl:sort outside apply-templates and for-each. */
static void xslt_sort(XmlLibxslt1Transform *t, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:sort : compilation failed\n");
        return;
    }
    xslt1_transform_error(t, NULL, inst, "xsl:sort : improper use this should not be reached\n");
}

/** Port of xsltCopy (xsl:copy). */
static void xslt_copy(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    XMQNode *old_insert = t->insert;
    if (t->insert)
    {
        switch (xml_node_kind(node))
        {
        case XML_DOM_TEXT:
        case XML_DOM_CDATA:
            xslt_copy_text(t, t->insert, node);
            break;
        case XML_DOM_DOCUMENT:
            break;
        case XML_DOM_ELEMENT:
        {
            XMQNode *copy = xslt_shallow_copy_elem(t, node, t->insert, 0);
            t->insert = copy;
            if (comp->use) xslt_apply_attribute_set(t, node, inst, comp->use);
            break;
        }
        case XML_DOM_ATTRIBUTE:
            xslt_shallow_copy_attr(t, inst, t->insert, node);
            break;
        case XML_DOM_PI:
            lx_add_child(t->insert, xml_node_new_pi(t->output, xml_node_name(node), xml_node_content(node)));
            break;
        case XML_DOM_COMMENT:
            lx_add_child(t->insert, xml_node_new_comment(t->output, xml_node_content(node)));
            break;
        case XML_DOM_NAMESPACE:
            xslt_shallow_copy_ns_node(t, inst, t->insert, node);
            break;
        default:
            break;
        }
    }
    switch (xml_node_kind(node))
    {
    case XML_DOM_DOCUMENT:
    case XML_DOM_ELEMENT:
        xslt_apply_sequence_constructor(t, t->node, xml_node_first_child(inst), NULL);
        break;
    default:
        break;
    }
    t->insert = old_insert;
}

/** Port of xsltText (xsl:text, only reached when it could not be replaced by its content). */
static void xslt_text(XmlLibxslt1Transform *t, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!xml_node_first_child(inst) || !comp) return;
    for (XMQNode *text = xml_node_first_child(inst); text; text = xml_node_next(text))
    {
        if (xml_node_kind(text) != XML_DOM_TEXT && xml_node_kind(text) != XML_DOM_CDATA)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:text content problem\n");
            break;
        }
        XMQNode *copy = xml_node_new_text(t->output, xml_node_content(text));
        if (xml_node_kind(text) != XML_DOM_CDATA) xml_node_set_name(copy, "textnoenc");
        lx_add_child(t->insert, copy);
    }
}

/** Port of xsltElement (xsl:element). */
static void xslt_element(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!t->insert) return;
    if (!comp->has_name) return;
    XMQNode *old_insert = t->insert;
    char *name = NULL, *prefix = NULL;
    const char *ns_name = NULL;
    char *ns_name_owned = NULL;
    if (!comp->name)
    {
        char *prop = xslt_eval_attr_value_template(t, inst, "name", XSLT_NAMESPACE);
        if (!prop)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:element: The attribute 'name' is missing.\n");
            goto error;
        }
        if (xslt1_validate_qname(prop))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:element: The effective name '%s' is not a valid QName.\n", prop);
            // Fall through to catch any further errors, if possible.
        }
        name = xslt1_split_qname(prop, &prefix);
        free(prop);
    }
    else
    {
        name = xslt1_split_qname(comp->name, &prefix);
    }
    XMQNode *copy;
    copy = xml_node_new_element(t->output, name, NULL);
    if (!copy)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:element : creation of %s failed\n", name);
        goto error;
    }
    copy = lx_add_child(t->insert, copy);
    if (!copy)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:element : xsltAddChild failed\n");
        goto error;
    }
    if (comp->has_ns)
    {
        if (comp->ns)
        {
            if (comp->ns[0]) ns_name = comp->ns;
        }
        else
        {
            char *tmp = xslt_eval_attr_value_template(t, inst, "namespace", XSLT_NAMESPACE);
            if (tmp && tmp[0]) ns_name = ns_name_owned = tmp;
            else free(tmp);
        }
        if (xslt1_streq(ns_name, "http://www.w3.org/2000/xmlns/"))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:attribute: Namespace http://www.w3.org/2000/xmlns/ forbidden.\n");
            goto error;
        }
        if (xslt1_streq(ns_name, XML_DOM_XML_NAMESPACE))
        {
            free(prefix);
            prefix = xslt1_strdup("xml");
        }
        else if (xslt1_streq(prefix, "xml"))
        {
            free(prefix);
            prefix = NULL;
        }
    }
    else
    {
        XmlNs *ns = lx_search_ns(inst, prefix);
        if (!ns)
        {
            if (prefix)
            {
                xslt1_transform_error(t, NULL, inst, "xsl:element: The QName '%s:%s' has no namespace binding in scope in the stylesheet; "
                                      "this is an error, since the namespace was not specified by the instruction itself.\n", prefix, name);
            }
        }
        else
        {
            ns_name = xml_ns_href(ns);
        }
    }
    if (ns_name)
    {
        // Do not use a prefix of "xmlns".
        if (xslt1_streq(prefix, "xmlns")) xml_node_set_ns(copy, xslt_get_special_namespace(t, inst, ns_name, "ns_1", copy));
        else xml_node_set_ns(copy, xslt_get_special_namespace(t, inst, ns_name, prefix, copy));
    }
    else if (lx_is_element(xml_node_parent(copy)) && xml_node_ns(xml_node_parent(copy)))
    {
        xslt_get_special_namespace(t, inst, NULL, NULL, copy);
    }
    t->insert = copy;
    if (comp->has_use)
    {
        if (comp->use)
        {
            xslt_apply_attribute_set(t, node, inst, comp->use);
        }
        else
        {
            char *attr_sets = xslt_eval_attr_value_template(t, inst, "use-attribute-sets", NULL);
            if (attr_sets)
            {
                xslt_apply_attribute_set(t, node, inst, attr_sets);
                free(attr_sets);
            }
        }
    }
    if (xml_node_first_child(inst)) xslt_apply_sequence_constructor(t, t->node, xml_node_first_child(inst), NULL);
error:
    t->insert = old_insert;
    free(name);
    free(prefix);
    free(ns_name_owned);
}

/** Port of xsltComment (xsl:comment). */
static void xslt_comment(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst)
{
    char *value = xslt_eval_template_string(t, node, inst);
    size_t len = value ? strlen(value) : 0;
    if (len > 0)
    {
        if (value[len - 1] == '-' || strstr(value, "--"))
        {
            xslt1_transform_error(t, NULL, inst, "xsl:comment : '--' or ending '-' not allowed in comment\n");
            // Fall through to try to catch further errors.
        }
    }
    lx_add_child(t->insert, xml_node_new_comment(t->output, value));
    free(value);
}

/** Port of xsltProcessingInstruction (xsl:processing-instruction). */
static void xslt_processing_instruction(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!t->insert) return;
    if (comp->has_name == 0) return;
    char *name_owned = NULL;
    const char *name;
    char *value = NULL;
    if (!comp->name)
    {
        name = name_owned = xslt_eval_attr_value_template(t, inst, "name", NULL);
        if (!name)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:processing-instruction : name is missing\n");
            goto error;
        }
    }
    else
    {
        name = comp->name;
    }
    value = xslt_eval_template_string(t, node, inst);
    if (value && strstr(value, "?>"))
    {
        xslt1_transform_error(t, NULL, inst, "xsl:processing-instruction: '?>' not allowed within PI content\n");
        goto error;
    }
    lx_add_child(t->insert, xml_node_new_pi(t->output, name, value));
error:
    free(name_owned);
    free(value);
}

/** Port of xsltCopyOf (xsl:copy-of). */
static void xslt_copy_of(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!node || !inst) return;
    if (!comp || !comp->select || !comp->comp)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:copy-of : compilation failed\n");
        return;
    }
    XmlXPathResult *res = xslt_pre_comp_eval(t, node, comp);
    if (res)
    {
        if (xml_xpath_result_type(res) == XML_XPATH_NODESET && !xml_xpath_result_is_tree(res))
        {
            size_t n = xml_xpath_result_size(res);
            for (size_t i = 0; i < n; i++)
            {
                XMQNode *cur = xml_xpath_result_node(res, i);
                if (!cur) continue;
                if (xml_node_kind(cur) == XML_DOM_DOCUMENT)
                {
                    xslt_copy_tree_list(t, inst, xml_node_first_child(cur), t->insert, 0, 0);
                }
                else if (xml_node_kind(cur) == XML_DOM_ATTRIBUTE)
                {
                    xslt_shallow_copy_attr(t, inst, t->insert, cur);
                }
                else
                {
                    xslt_copy_tree(t, inst, cur, t->insert, 0, 0);
                }
            }
        }
        else if (xml_xpath_result_is_tree(res))
        {
            XMQNode *first = xml_xpath_result_size(res) ? xml_xpath_result_node(res, 0) : NULL;
            if (first && lx_is_real_node(first)) xslt_copy_tree_list(t, inst, lx_children(first), t->insert, 0, 0);
        }
        else
        {
            char *value = xml_xpath_result_to_string(res);
            if (!value)
            {
                xslt1_transform_error(t, NULL, inst, "Internal error in xsltCopyOf(): failed to cast an XPath object to string.\n");
                t->state = XSLT_STATE_STOPPED;
            }
            else
            {
                if (value[0]) xslt_copy_text_string(t, t->insert, value, 0);
                free(value);
            }
        }
    }
    else
    {
        t->state = XSLT_STATE_STOPPED;
    }
    xml_xpath_result_free(res);
}

/** Port of xsltValueOf (xsl:value-of). */
static void xslt_value_of(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!node || !inst) return;
    if (!comp || !comp->select || !comp->comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltValueOf(): The XSLT 'value-of' instruction was not compiled.\n");
        return;
    }
    char *value = NULL;
    XmlXPathResult *res = xslt_pre_comp_eval(t, node, comp);
    if (res)
    {
        value = xml_xpath_result_to_string(res);
        if (!value)
        {
            xslt1_transform_error(t, NULL, inst, "Internal error in xsltValueOf(): failed to cast an XPath object to string.\n");
            t->state = XSLT_STATE_STOPPED;
            goto error;
        }
        if (value[0]) xslt_copy_text_string(t, t->insert, value, comp->noescape);
    }
    else
    {
        xslt1_transform_error(t, NULL, inst, "XPath evaluation returned no result.\n");
        t->state = XSLT_STATE_STOPPED;
    }
error:
    free(value);
    xml_xpath_result_free(res);
}

static void xslt_number_format(XmlLibxslt1Transform *t, XsltNumberData *data, XMQNode *node);

/** Port of xsltNumber (xsl:number). */
static void xslt_number(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:number : compilation failed\n");
        return;
    }
    if (!node || !inst) return;
    comp->numdata.doc = xml_node_dom(inst);
    comp->numdata.node = inst;
    xslt_number_format(t, &comp->numdata, node);
}

/** Port of xsltApplyImports (xsl:apply-imports). */
static void xslt_apply_imports(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!inst) return;
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltApplyImports(): The XSLT 'apply-imports' instruction was not compiled.\n");
        return;
    }
    if (!t->current_template_rule)
    {
        xslt1_transform_error(t, NULL, inst, "It is an error to call 'apply-imports' when there's no current template rule.\n");
        return;
    }
    XsltTemplate *templ = xslt1_get_template(t, context_node, t->current_template_rule->style);
    if (templ)
    {
        XsltTemplate *old_rule = t->current_template_rule;
        t->current_template_rule = templ;
        xslt_apply_xslt_template(t, context_node, templ->content, templ, NULL);
        t->current_template_rule = old_rule;
    }
    else
    {
        // Use the built in templates.
        xslt_default_process_one_node(t, context_node, NULL);
    }
}

/** Port of xsltFindTemplate: a named template over the import cascade. */
static XsltTemplate *xslt_find_template(XmlLibxslt1Transform *t, const char *name, const char *name_uri)
{
    if (!name) return NULL;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        if (style->named_templates)
        {
            char *key = xslt1_hash3_key(name, name_uri, NULL);
            XsltTemplate *cur = (XsltTemplate*)hashmap_get(style->named_templates, key);
            free(key);
            if (cur) return cur;
        }
    }
    return NULL;
}

/** Port of xsltCallTemplate (xsl:call-template). */
static void xslt_call_template(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    XsltStackElem *with_params = NULL;
    if (!t->insert) return;
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "The XSLT 'call-template' instruction was not compiled.\n");
        return;
    }
    if (!comp->templ)
    {
        comp->templ = xslt_find_template(t, comp->name, comp->ns);
        if (!comp->templ)
        {
            if (comp->ns) xslt1_transform_error(t, NULL, inst, "The called template '{%s}%s' was not found.\n", comp->ns, comp->name);
            else xslt1_transform_error(t, NULL, inst, "The called template '%s' was not found.\n", comp->name);
            return;
        }
    }
    for (XMQNode *cur = xml_node_first_child(inst); cur; cur = xml_node_next(cur))
    {
        if (t->state == XSLT_STATE_STOPPED) break;
        if (xslt1_is_xslt_elem(cur))
        {
            if (xslt1_is_xslt_name(cur, "with-param"))
            {
                XsltStackElem *param = xslt_parse_stylesheet_caller_param(t, cur);
                if (param)
                {
                    param->next = with_params;
                    with_params = param;
                }
            }
            else
            {
                xslt1_generic_error(&t->style->settings, "xsl:call-template: misplaced xsl:%s\n", xml_node_name(cur));
            }
        }
        else
        {
            xslt1_generic_error(&t->style->settings, "xsl:call-template: misplaced %s element\n",
                                xslt1_libxml2_node_name(cur) ? xslt1_libxml2_node_name(cur) : "(null)");
        }
    }
    xslt_apply_xslt_template(t, node, comp->templ->content, comp->templ, with_params);
    if (with_params) xslt_free_runtime_stack_elem_list(t, with_params);
}

static XsltNodeList *xslt_node_list_from(const XmlXPathResult *res)
{
    XsltNodeList *list = (XsltNodeList*)calloc(1, sizeof(XsltNodeList));
    size_t n = res ? xml_xpath_result_size(res) : 0;
    if (n)
    {
        list->nodes = (XMQNode**)malloc(n * sizeof(XMQNode*));
        for (size_t i = 0; i < n; ++i) list->nodes[i] = xml_xpath_result_node(res, i);
    }
    list->nr = (int)n;
    return list;
}

static void xslt_node_list_free(XsltNodeList *list)
{
    if (!list) return;
    free(list->nodes);
    free(list);
}

/** Port of xsltApplyTemplates (xsl:apply-templates). */
static void xslt_apply_templates(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:apply-templates : compilation failed\n");
        return;
    }
    if (!node || !inst) return;
    XsltNodeList *list = NULL;
    XsltStackElem *with_params = NULL;
    XMQNode *old_context_node = t->node;
    const char *old_mode = t->mode, *old_mode_uri = t->mode_uri;
    XsltDocInfo *old_doc_info = t->document;
    XsltNodeList *old_list = t->node_list;
    long old_size = t->xp_size, old_pos = t->xp_position;
    t->mode = comp->mode;
    t->mode_uri = comp->mode_uri;
    if (comp->select)
    {
        if (!comp->comp)
        {
            xslt1_transform_error(t, NULL, inst, "xsl:apply-templates : compilation failed\n");
            goto error;
        }
        XmlXPathResult *res = xslt_pre_comp_eval(t, node, comp);
        if (res)
        {
            if (xml_xpath_result_type(res) == XML_XPATH_NODESET && !xml_xpath_result_is_tree(res))
            {
                list = xslt_node_list_from(res);
            }
            else
            {
                xslt1_transform_error(t, NULL, inst, "The 'select' expression did not evaluate to a node set.\n");
                t->state = XSLT_STATE_STOPPED;
                xml_xpath_result_free(res);
                goto error;
            }
            xml_xpath_result_free(res);
        }
        else
        {
            xslt1_transform_error(t, NULL, inst, "Failed to evaluate the 'select' expression.\n");
            t->state = XSLT_STATE_STOPPED;
            goto error;
        }
    }
    else
    {
        list = (XsltNodeList*)calloc(1, sizeof(XsltNodeList));
        int n = 0;
        for (XMQNode *cur = lx_children(node); cur; cur = xml_node_next(cur)) if (lx_is_real_node(cur)) n++;
        if (n) list->nodes = (XMQNode**)malloc((size_t)n * sizeof(XMQNode*));
        for (XMQNode *cur = lx_children(node); cur; cur = xml_node_next(cur)) if (lx_is_real_node(cur)) list->nodes[list->nr++] = cur;
    }
    if (!list || list->nr == 0) goto exit;
    t->node_list = list;
    // Parameters and sorts.
    for (XMQNode *cur = xml_node_first_child(inst); cur; )
    {
        if (t->state == XSLT_STATE_STOPPED) break;
        if (xml_node_kind(cur) == XML_DOM_TEXT)
        {
            cur = xml_node_next(cur);
            continue;
        }
        if (!xslt1_is_xslt_elem(cur)) break;
        if (xslt1_is_xslt_name(cur, "with-param"))
        {
            XsltStackElem *param = xslt_parse_stylesheet_caller_param(t, cur);
            if (param)
            {
                param->next = with_params;
                with_params = param;
            }
        }
        if (xslt1_is_xslt_name(cur, "sort"))
        {
            XsltTemplate *old_rule = t->current_template_rule;
            int nbsorts = 0;
            XMQNode *sorts[XSLT_MAX_SORT];
            sorts[nbsorts++] = cur;
            cur = xml_node_next(cur);
            while (cur)
            {
                if (t->state == XSLT_STATE_STOPPED) break;
                if (xml_node_kind(cur) == XML_DOM_TEXT)
                {
                    cur = xml_node_next(cur);
                    continue;
                }
                if (!xslt1_is_xslt_elem(cur)) break;
                if (xslt1_is_xslt_name(cur, "with-param"))
                {
                    XsltStackElem *param = xslt_parse_stylesheet_caller_param(t, cur);
                    if (param)
                    {
                        param->next = with_params;
                        with_params = param;
                    }
                }
                if (xslt1_is_xslt_name(cur, "sort"))
                {
                    if (nbsorts >= XSLT_MAX_SORT)
                    {
                        xslt1_transform_error(t, NULL, cur, "The number (%d) of xsl:sort instructions exceeds the "
                                              "maximum allowed by this processor's settings.\n", nbsorts);
                        t->state = XSLT_STATE_STOPPED;
                        break;
                    }
                    sorts[nbsorts++] = cur;
                }
                cur = xml_node_next(cur);
            }
            t->current_template_rule = NULL;
            xslt_do_sort_function(t, sorts, nbsorts);
            t->current_template_rule = old_rule;
            break;
        }
        cur = xml_node_next(cur);
    }
    t->xp_size = list->nr;
    for (int i = 0; i < list->nr; i++)
    {
        XMQNode *cur = list->nodes[i];
        t->node = cur;
        t->xp_position = i + 1;
        xslt_process_one_node(t, cur, with_params);
    }
exit:
error:
    if (with_params) xslt_free_runtime_stack_elem_list(t, with_params);
    xslt_node_list_free(list);
    t->xp_size = old_size;
    t->xp_position = old_pos;
    t->document = old_doc_info;
    t->node_list = old_list;
    t->node = old_context_node;
    t->mode = old_mode;
    t->mode_uri = old_mode_uri;
}

/** Port of xsltChoose (xsl:choose). */
static void xslt_choose(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst)
{
    if (!context_node || !inst) return;
    XMQNode *cur = xml_node_first_child(inst);
    if (!cur)
    {
        xslt1_transform_error(t, NULL, inst, "xsl:choose: The instruction has no content.\n");
        return;
    }
    if (!xslt1_is_xslt_elem(cur) || !xslt1_is_xslt_name(cur, "when"))
    {
        xslt1_transform_error(t, NULL, inst, "xsl:choose: xsl:when expected first\n");
        return;
    }
    while (cur && xslt1_is_xslt_elem(cur) && xslt1_is_xslt_name(cur, "when"))
    {
        XsltStylePreComp *wcomp = (XsltStylePreComp*)xml_node_user_data(cur);
        if (!wcomp || wcomp == XSLT_EXT_MARKER || !wcomp->test || !wcomp->comp)
        {
            xslt1_transform_error(t, NULL, cur, "Internal error in xsltChoose(): The XSLT 'when' instruction was not compiled.\n");
            return;
        }
        int res = xslt_pre_comp_eval_to_boolean(t, context_node, wcomp);
        if (res == -1)
        {
            t->state = XSLT_STATE_STOPPED;
            return;
        }
        if (res == 1) goto process_sequence;
        cur = xml_node_next(cur);
    }
    if (cur && xslt1_is_xslt_elem(cur) && xslt1_is_xslt_name(cur, "otherwise")) goto process_sequence;
    return;
process_sequence:
    xslt_apply_sequence_constructor(t, t->node, xml_node_first_child(cur), NULL);
}

/** Port of xsltIf (xsl:if). */
static void xslt_if(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!context_node || !inst) return;
    if (!comp || !comp->test || !comp->comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltIf(): The XSLT 'if' instruction was not compiled.\n");
        return;
    }
    XsltRVT *old_local_fragment_top = t->local_rvt;
    int res = xslt_pre_comp_eval_to_boolean(t, context_node, comp);
    if (old_local_fragment_top != t->local_rvt) xslt_release_local_rvts(t, old_local_fragment_top);
    if (res == -1)
    {
        t->state = XSLT_STATE_STOPPED;
        return;
    }
    if (res == 1) xslt_apply_sequence_constructor(t, context_node, xml_node_first_child(inst), NULL);
}

/** Port of xsltForEach (xsl:for-each). */
static void xslt_for_each(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!context_node || !inst)
    {
        xslt1_generic_error(&t->style->settings, "xsltForEach(): Bad arguments.\n");
        return;
    }
    if (!comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltForEach(): The XSLT 'for-each' instruction was not compiled.\n");
        return;
    }
    if (!comp->select || !comp->comp)
    {
        xslt1_transform_error(t, NULL, inst, "Internal error in xsltForEach(): The selecting expression of the XSLT 'for-each' "
                              "instruction was not compiled correctly.\n");
        return;
    }
    XsltDocInfo *old_doc_info = t->document;
    XsltNodeList *old_list = t->node_list;
    XMQNode *old_context_node = t->node;
    XsltTemplate *old_rule = t->current_template_rule;
    t->current_template_rule = NULL;
    long old_size = t->xp_size, old_pos = t->xp_position;
    XsltNodeList *list = NULL;
    XmlXPathResult *res = xslt_pre_comp_eval(t, context_node, comp);
    if (res)
    {
        if (xml_xpath_result_type(res) == XML_XPATH_NODESET && !xml_xpath_result_is_tree(res))
        {
            list = xslt_node_list_from(res);
        }
        else
        {
            xslt1_transform_error(t, NULL, inst, "The 'select' expression does not evaluate to a node set.\n");
            goto error;
        }
    }
    else
    {
        xslt1_transform_error(t, NULL, inst, "Failed to evaluate the 'select' expression.\n");
        t->state = XSLT_STATE_STOPPED;
        goto error;
    }
    if (!list || list->nr <= 0) goto exit;
    t->node_list = list;
    XMQNode *cur_inst;
    cur_inst = xml_node_first_child(inst);
    if (cur_inst && xslt1_is_xslt_elem(cur_inst) && xslt1_is_xslt_name(cur_inst, "sort"))
    {
        int nbsorts = 0;
        XMQNode *sorts[XSLT_MAX_SORT];
        sorts[nbsorts++] = cur_inst;
        cur_inst = xml_node_next(cur_inst);
        while (cur_inst && xslt1_is_xslt_elem(cur_inst) && xslt1_is_xslt_name(cur_inst, "sort"))
        {
            if (nbsorts >= XSLT_MAX_SORT)
            {
                xslt1_transform_error(t, NULL, cur_inst, "The number of xsl:sort instructions exceeds the "
                                      "maximum (%d) allowed by this processor.\n", XSLT_MAX_SORT);
                goto error;
            }
            sorts[nbsorts++] = cur_inst;
            cur_inst = xml_node_next(cur_inst);
        }
        xslt_do_sort_function(t, sorts, nbsorts);
    }
    t->xp_size = list->nr;
    for (int i = 0; i < list->nr; i++)
    {
        XMQNode *cur = list->nodes[i];
        t->node = cur;
        t->xp_position = i + 1;
        xslt_apply_sequence_constructor(t, cur, cur_inst, NULL);
    }
exit:
error:
    xml_xpath_result_free(res);
    xslt_node_list_free(list);
    t->document = old_doc_info;
    t->node_list = old_list;
    t->node = old_context_node;
    t->current_template_rule = old_rule;
    t->xp_size = old_size;
    t->xp_position = old_pos;
}

static void xslt_document_elem(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp);

/** Run the function of a precomputed instruction (comp->func in libxslt). Returns false if there is none. */
static bool xslt_call_func(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    switch (comp->type)
    {
    case XSLT_FUNC_COPY: xslt_copy(t, node, inst, comp); return true;
    case XSLT_FUNC_SORT: xslt_sort(t, inst, comp); return true;
    case XSLT_FUNC_TEXT: xslt_text(t, inst, comp); return true;
    case XSLT_FUNC_ELEMENT: xslt_element(t, node, inst, comp); return true;
    case XSLT_FUNC_ATTRIBUTE: xslt_attribute(t, node, inst, comp); return true;
    case XSLT_FUNC_COMMENT: xslt_comment(t, node, inst); return true;
    case XSLT_FUNC_PI: xslt_processing_instruction(t, node, inst, comp); return true;
    case XSLT_FUNC_COPYOF: xslt_copy_of(t, node, inst, comp); return true;
    case XSLT_FUNC_VALUEOF: xslt_value_of(t, node, inst, comp); return true;
    case XSLT_FUNC_NUMBER: xslt_number(t, node, inst, comp); return true;
    case XSLT_FUNC_APPLYIMPORTS: xslt_apply_imports(t, node, inst, comp); return true;
    case XSLT_FUNC_CALLTEMPLATE: xslt_call_template(t, node, inst, comp); return true;
    case XSLT_FUNC_APPLYTEMPLATES: xslt_apply_templates(t, node, inst, comp); return true;
    case XSLT_FUNC_CHOOSE: xslt_choose(t, node, inst); return true;
    case XSLT_FUNC_IF: xslt_if(t, node, inst, comp); return true;
    case XSLT_FUNC_FOREACH: xslt_for_each(t, node, inst, comp); return true;
    case XSLT_FUNC_DOCUMENT: xslt_document_elem(t, node, inst, comp); return true;
    case XSLT_FUNC_EXTENSION:
        if (comp->ext == XSLT_EXT_DEBUG)
        {
            xslt_debug(t, node, inst);
            return true;
        }
        if (comp->ext == XSLT_EXT_DOCUMENT)
        {
            xslt_document_elem(t, node, inst, comp);
            return true;
        }
        return false;
    default:
        return false;
    }
}

/** Port of xsltApplySequenceConstructor: instantiate the nodes of list. */
static void xslt_apply_sequence_constructor(XmlLibxslt1Transform *t, XMQNode *context_node, XMQNode *list, XsltTemplate *templ)
{
    if (!list) return;
    if (t->state == XSLT_STATE_STOPPED) return;
    if (t->depth >= t->max_template_depth)
    {
        xslt1_transform_error(t, NULL, list, "xsltApplySequenceConstructor: A potential infinite template recursion was detected.\n"
                              "You can adjust xsltMaxDepth (--maxdepth) in order to raise the maximum number of nested template calls and "
                              "variables/params (currently set to %d).\n", t->max_template_depth);
        xslt_debug(t, context_node, list);
        t->state = XSLT_STATE_STOPPED;
        return;
    }
    t->depth++;
    XsltRVT *old_local_fragment_top = t->local_rvt;
    XMQNode *old_insert = t->insert, *insert = t->insert;
    XMQNode *old_inst = t->inst, *old_cur_inst;
    XMQNode *old_context_node = t->node;
    int old_vars_nr = t->vars_nr;
    int level = 0;
    XMQNode *list_parent = xml_node_parent(list);
    XMQNode *cur = list;
    XMQNode *copy = NULL;
    while (cur)
    {
        if (t->op_limit != 0)
        {
            if (t->op_count >= t->op_limit)
            {
                xslt1_transform_error(t, NULL, cur, "xsltApplySequenceConstructor: Operation limit exceeded\n");
                t->state = XSLT_STATE_STOPPED;
                goto error;
            }
            t->op_count += 1;
        }
        t->inst = cur;
        if (!insert) goto error;
        copy = NULL;
        if (xslt1_is_xslt_elem(cur))
        {
            XsltStylePreComp *info = (XsltStylePreComp*)xml_node_user_data(cur);
            if (!info)
            {
                if (xslt1_is_xslt_name(cur, "message"))
                {
                    xslt_message(t, context_node, cur);
                }
                else
                {
                    // Something really went wrong, try the fallbacks.
                    t->insert = insert;
                    if (!xslt_apply_fallbacks(t, context_node, cur))
                    {
                        xslt1_generic_error(&t->style->settings, "xsltApplySequenceConstructor: %s was not compiled\n", xml_node_name(cur));
                    }
                    t->insert = old_insert;
                }
                goto skip_children;
            }
            if (info != XSLT_EXT_MARKER)
            {
                old_cur_inst = t->inst;
                t->inst = cur;
                t->insert = insert;
                bool done = xslt_call_func(t, context_node, cur, info);
                if (done)
                {
                    if (old_local_fragment_top != t->local_rvt) xslt_release_local_rvts(t, old_local_fragment_top);
                    t->insert = old_insert;
                    t->inst = old_cur_inst;
                    goto skip_children;
                }
                t->insert = old_insert;
                t->inst = old_cur_inst;
            }
            if (xslt1_is_xslt_name(cur, "variable"))
            {
                XsltStackElem *tmpvar = t->vars;
                old_cur_inst = t->inst;
                t->inst = cur;
                xslt_parse_stylesheet_variable(t, cur);
                t->inst = old_cur_inst;
                if (tmpvar != t->vars)
                {
                    // The variable was registered, it lives until the end of its parent.
                    t->vars->level = level;
                }
            }
            else if (xslt1_is_xslt_name(cur, "message"))
            {
                xslt_message(t, context_node, cur);
            }
            else
            {
                xslt1_transform_error(t, NULL, cur, "Unexpected XSLT element '%s'.\n", xml_node_name(cur));
            }
            goto skip_children;
        }
        else if (xml_node_kind(cur) == XML_DOM_TEXT || xml_node_kind(cur) == XML_DOM_CDATA)
        {
            if (!xslt_copy_text(t, insert, cur)) goto error;
        }
        else if (xml_node_kind(cur) == XML_DOM_ELEMENT && xml_node_ns(cur) && xml_node_user_data(cur))
        {
            // An extension element.
            old_cur_inst = t->inst;
            t->inst = cur;
            XsltStylePreComp *info = (XsltStylePreComp*)xml_node_user_data(cur);
            bool done = false;
            if (info != XSLT_EXT_MARKER)
            {
                t->insert = insert;
                done = xslt_call_func(t, context_node, cur, info);
                if (done && old_local_fragment_top != t->local_rvt) xslt_release_local_rvts(t, old_local_fragment_top);
                t->insert = old_insert;
            }
            if (!done)
            {
                int found = 0;
                t->insert = insert;
                for (XMQNode *child = xml_node_first_child(cur); child; child = xml_node_next(child))
                {
                    if (xslt1_is_xslt_elem(child) && xslt1_is_xslt_name(child, "fallback"))
                    {
                        found = 1;
                        xslt_apply_sequence_constructor(t, context_node, xml_node_first_child(child), NULL);
                    }
                }
                t->insert = old_insert;
                if (!found)
                {
                    xslt1_transform_error(t, NULL, cur, "xsltApplySequenceConstructor: failed to find extension %s\n", xml_node_name(cur));
                }
            }
            t->inst = old_cur_inst;
            goto skip_children;
        }
        else if (xml_node_kind(cur) == XML_DOM_ELEMENT)
        {
            // A literal result element.
            old_cur_inst = t->inst;
            t->inst = cur;
            copy = xslt_shallow_copy_elem(t, cur, insert, 1);
            if (!copy) goto error;
            // Add the namespaces in scope of the template (only for its top level elements).
            if (templ && old_insert == insert && t->templ && t->templ->inherited_ns)
            {
                for (int i = 0; i < t->templ->inherited_ns_nr; i++)
                {
                    XmlNs *ns = t->templ->inherited_ns[i];
                    const char *uri = xslt_lookup_alias(t, xml_ns_href(ns));
                    if (uri && !strcmp(uri, XSLT_UNDEFINED_DEFAULT_NS)) continue;
                    if (!uri) uri = xml_ns_href(ns);
                    XmlNs *ret = lx_search_ns(copy, xml_ns_prefix(ns));
                    if (!ret || !xslt1_streq(xml_ns_href(ret), uri)) lx_new_ns(copy, uri, xml_ns_prefix(ns));
                }
                if (xml_node_ns(copy)) xml_node_set_ns(copy, xslt_get_namespace(t, cur, xml_node_ns(copy), copy));
            }
            if (xml_node_first_attr(cur)) xslt_attr_list_template_process(t, copy, xml_node_first_attr(cur));
            t->inst = old_cur_inst;
        }
        // Descend into the content.
        if (xml_node_first_child(cur) && xml_node_kind(xml_node_first_child(cur)) != XML_DOM_ENTITY_DECL)
        {
            cur = xml_node_first_child(cur);
            level++;
            if (copy) insert = copy;
            continue;
        }
skip_children:
        if (t->state == XSLT_STATE_STOPPED) break;
        if (xml_node_next(cur))
        {
            cur = xml_node_next(cur);
            continue;
        }
        do
        {
            cur = xml_node_parent(cur);
            level--;
            // Pop the variables of the leaving level.
            if (t->vars_nr > old_vars_nr && t->vars->level > level) xslt_local_variable_pop(t, old_vars_nr, level);
            insert = xml_node_parent(insert);
            if (!cur) break;
            if (cur == list_parent)
            {
                cur = NULL;
                break;
            }
            if (xml_node_next(cur))
            {
                cur = xml_node_next(cur);
                break;
            }
        } while (cur);
    }
error:
    if (t->vars_nr > old_vars_nr) xslt_local_variable_pop(t, old_vars_nr, -1);
    t->node = old_context_node;
    t->inst = old_inst;
    t->insert = old_insert;
    t->depth--;
}

// Sorting, port of libxslt xsltutils.c xsltDefaultSortFunction ////

typedef struct
{
    bool valid;
    double num;
    char *str;
    int index;              // The original position, for duplicates.
} XsltSortKey;

static void xslt_free_sort_keys(XsltSortKey *keys, int len)
{
    if (!keys) return;
    for (int i = 0; i < len; i++) free(keys[i].str);
    free(keys);
}

/** Port of xsltComputeSortResultInternal: the sort keys of the current node list. */
static XsltSortKey *xslt_compute_sort_result(XmlLibxslt1Transform *t, XMQNode *sort, int number)
{
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(sort);
    if (!comp || comp == XSLT_EXT_MARKER)
    {
        xslt1_generic_error(&t->style->settings, "xsl:sort : compilation failed\n");
        return NULL;
    }
    if (!comp->select || !comp->comp) return NULL;
    XsltNodeList *list = t->node_list;
    if (!list || list->nr <= 1) return NULL;
    int len = list->nr;
    XsltSortKey *results = (XsltSortKey*)calloc((size_t)len, sizeof(XsltSortKey));
    XMQNode *old_inst = t->inst;
    XMQNode *old_node = t->node;
    long old_pos = t->xp_position, old_size = t->xp_size;
    for (int i = 0; i < len; i++)
    {
        t->inst = sort;
        t->xp_size = len;
        t->xp_position = i + 1;
        t->node = list->nodes[i];
        XmlXPathResult *res = xslt_xpath_eval(t, comp->comp, t->node);
        if (res)
        {
            results[i].index = i;
            results[i].valid = true;
            char *s = xml_xpath_result_to_string(res);
            if (number)
            {
                results[i].num = xml_xpath_string_to_number(s);
                free(s);
            }
            else
            {
                results[i].str = s;
            }
            xml_xpath_result_free(res);
        }
        else
        {
            t->state = XSLT_STATE_STOPPED;
            results[i].valid = false;
        }
    }
    t->inst = old_inst;
    t->node = old_node;
    t->xp_size = old_size;
    t->xp_position = old_pos;
    return results;
}

static int xslt_sort_compare(XsltSortKey *a, XsltSortKey *b, int number)
{
    if (number)
    {
        // NaN is smaller than numbers, like the XSLT spec says.
        if (isnan(a->num)) return isnan(b->num) ? 0 : -1;
        if (isnan(b->num)) return 1;
        if (a->num == b->num) return 0;
        return a->num > b->num ? 1 : -1;
    }
    return strcmp(a->str ? a->str : "", b->str ? b->str : "");
}

/** Port of xsltDefaultSortFunction: a shell sort of the node list by the sort keys. */
static void xslt_do_sort_function(XmlLibxslt1Transform *t, XMQNode **sorts, int nbsorts)
{
    XsltSortKey *results_tab[XSLT_MAX_SORT];
    int number[XSLT_MAX_SORT], desc[XSLT_MAX_SORT];
    if (!sorts || nbsorts <= 0 || nbsorts >= XSLT_MAX_SORT) return;
    if (!sorts[0]) return;
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(sorts[0]);
    if (!comp) return;
    XsltNodeList *list = t->node_list;
    if (!list || list->nr <= 1) return;
    for (int j = 0; j < nbsorts; j++)
    {
        comp = (XsltStylePreComp*)xml_node_user_data(sorts[j]);
        if (!comp->stype && comp->has_stype != 0)
        {
            char *stype = xslt_eval_attr_value_template(t, sorts[j], "data-type", NULL);
            number[j] = 0;
            if (stype)
            {
                if (!strcmp(stype, "text")) {}
                else if (!strcmp(stype, "number")) number[j] = 1;
                else xslt1_transform_error(t, NULL, sorts[j], "xsltDoSortFunction: no support for data-type = %s\n", stype);
                free(stype);
            }
        }
        else
        {
            number[j] = comp->number;
        }
        if (!comp->order && comp->has_order != 0)
        {
            char *order = xslt_eval_attr_value_template(t, sorts[j], "order", NULL);
            desc[j] = 0;
            if (order)
            {
                if (!strcmp(order, "ascending")) {}
                else if (!strcmp(order, "descending")) desc[j] = 1;
                else xslt1_transform_error(t, NULL, sorts[j], "xsltDoSortFunction: invalid value %s for order\n", order);
                free(order);
            }
        }
        else
        {
            desc[j] = comp->descending;
        }
        if (!comp->lang && comp->has_lang != 0)
        {
            char *lang = xslt_eval_attr_value_template(t, sorts[j], "lang", NULL);
            // TODO: locale specific collation (xsltNewLocale, xsltStrxfrm).
            free(lang);
        }
    }
    int len = list->nr;
    results_tab[0] = xslt_compute_sort_result(t, sorts[0], number[0]);
    for (int i = 1; i < XSLT_MAX_SORT; i++) results_tab[i] = NULL;
    XsltSortKey *results = results_tab[0];
    if (!results) goto cleanup;
    // Shell's sort of the node set.
    for (int incr = len / 2; incr > 0; incr /= 2)
    {
        for (int i = incr; i < len; i++)
        {
            int j = i - incr;
            if (!results[i].valid) continue;
            while (j >= 0)
            {
                int tst;
                if (!results[j].valid)
                {
                    tst = 1;
                }
                else
                {
                    tst = xslt_sort_compare(&results[j], &results[j + incr], number[0]);
                    if (desc[0]) tst = -tst;
                }
                if (tst == 0)
                {
                    // Multi level sorts.
                    int depth = 1;
                    while (depth < nbsorts)
                    {
                        if (!sorts[depth]) break;
                        comp = (XsltStylePreComp*)xml_node_user_data(sorts[depth]);
                        if (!comp) break;
                        if (!results_tab[depth]) results_tab[depth] = xslt_compute_sort_result(t, sorts[depth], number[depth]);
                        XsltSortKey *res = results_tab[depth];
                        if (!res) break;
                        if (!res[j].valid)
                        {
                            if (res[j + incr].valid) tst = 1;
                        }
                        else if (!res[j + incr].valid)
                        {
                            tst = -1;
                        }
                        else
                        {
                            tst = xslt_sort_compare(&res[j], &res[j + incr], number[depth]);
                            if (desc[depth]) tst = -tst;
                        }
                        if (tst != 0) break;
                        depth++;
                    }
                }
                if (tst == 0) tst = results[j].index > results[j + incr].index;
                if (tst > 0)
                {
                    XsltSortKey tmp = results[j];
                    results[j] = results[j + incr];
                    results[j + incr] = tmp;
                    XMQNode *node = list->nodes[j];
                    list->nodes[j] = list->nodes[j + incr];
                    list->nodes[j + incr] = node;
                    for (int depth = 1; depth < nbsorts; depth++)
                    {
                        if (!sorts[depth] || !results_tab[depth]) break;
                        XsltSortKey *res = results_tab[depth];
                        XsltSortKey tmp2 = res[j];
                        res[j] = res[j + incr];
                        res[j + incr] = tmp2;
                    }
                    j -= incr;
                }
                else
                {
                    break;
                }
            }
        }
    }
cleanup:
    for (int j = 0; j < nbsorts; j++) xslt_free_sort_keys(results_tab[j], len);
}

/** Port of xsltMessage (xsl:message). */
static void xslt_message(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst)
{
    if (!inst) return;
    int terminate = 0;
    char *prop = xslt1_get_ns_prop_exact(inst, "terminate", NULL);
    if (prop)
    {
        if (!strcmp(prop, "yes")) terminate = 1;
        else if (!strcmp(prop, "no")) terminate = 0;
        else xslt1_transform_error(t, NULL, inst, "xsl:message : terminate expecting 'yes' or 'no'\n");
        free(prop);
    }
    char *message = xslt_eval_template_string(t, node, inst);
    if (message)
    {
        size_t len = strlen(message);
        xslt1_generic_error(&t->style->settings, "%s", message);
        if (len > 0 && message[len - 1] != '\n') xslt1_generic_error(&t->style->settings, "\n");
        free(message);
    }
    if (terminate) t->state = XSLT_STATE_STOPPED;
}

/** Port of xsltDebug (libxslt:debug and potential infinite recursions): dump the template and variable stacks. */
static void xslt_debug(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst)
{
    (void)node;
    (void)inst;
    const XmlLibxslt1Settings *s = &t->style->settings;
    xslt1_generic_error(s, "Templates:\n");
    for (int i = 0, j = t->templ_nr - 1; i < 15 && j >= 0; i++, j--)
    {
        xslt1_generic_error(s, "#%d ", i);
        if (t->templ_tab[j]->name) xslt1_generic_error(s, "name %s ", t->templ_tab[j]->name);
        if (t->templ_tab[j]->match) xslt1_generic_error(s, "name %s ", t->templ_tab[j]->match);
        if (t->templ_tab[j]->mode) xslt1_generic_error(s, "name %s ", t->templ_tab[j]->mode);
        xslt1_generic_error(s, "\n");
    }
    xslt1_generic_error(s, "Variables:\n");
    for (int i = 0, j = t->vars_nr - 1; i < 15 && j >= 0; i++, j--)
    {
        if (!t->vars_tab[j]) continue;
        xslt1_generic_error(s, "#%d\n", i);
        for (XsltStackElem *cur = t->vars_tab[j]; cur; cur = cur->next)
        {
            if (!cur->comp) xslt1_generic_error(s, "corrupted !!!\n");
            else if (cur->comp->type == XSLT_FUNC_PARAM) xslt1_generic_error(s, "param ");
            else if (cur->comp->type == XSLT_FUNC_VARIABLE) xslt1_generic_error(s, "var ");
            if (cur->name) xslt1_generic_error(s, "%s ", cur->name);
            else xslt1_generic_error(s, "noname !!!!");
            // The value is only dumped when the debug output is stdout or stderr (not by default).
            if (!cur->value) xslt1_generic_error(s, "NULL !!!!");
            xslt1_generic_error(s, "\n");
        }
    }
}

static bool xslt_save_result_to(MemBuffer *buf, XmlDom *result, XmlLibxslt1Stylesheet *style, int type);

/** Port of xsltSaveResultToFilename: save a result document (xsl:document), -1 on failure. */
static int xslt_save_result_to_filename(XmlLibxslt1Transform *t, const char *filename, XmlDom *res, XmlLibxslt1Stylesheet *style, bool append)
{
    (void)t;
    if (!filename || !res || !style) return -1;
    if (!xml_node_first_child(xml_dom_document(res))) return 0;
    size_t len = 0;
    char *out = xml_libxslt1_save_result_to_string(res, style, t->type, &len);
    const char *path = filename;
    if (!strncmp(path, "file://", 7))
    {
        path += 7;
        if (!strncmp(path, "localhost/", 10)) path += 9;
    }
    FILE *f = fopen(path, append ? "ab" : "wb");
    int ret = -1;
    if (f)
    {
        ret = fwrite(out, 1, len, f) == len ? (int)len : -1;
        if (fclose(f) != 0) ret = -1;
    }
    free(out);
    return ret;
}

static XmlDom *xslt_html_new_doc(const char *system_id, const char *public_id);
static int xslt_get_html_ids(const char *version, const char **public_id, const char **system_id);
static const char *xslt_import_method(XmlLibxslt1Stylesheet *s);
static const char *xslt_import_doctype_public(XmlLibxslt1Stylesheet *s);
static const char *xslt_import_doctype_system(XmlLibxslt1Stylesheet *s);
static const char *xslt_import_version(XmlLibxslt1Stylesheet *s);
static const char *xslt_import_encoding(XmlLibxslt1Stylesheet *s);
static bool xslt_is_blank_text_node(XMQNode *n);

/** Port of xsltDocumentElem: xsl:document (XSLT 1.1 draft), saxon:output, xalan:write and xt:document. */
static void xslt_document_elem(XmlLibxslt1Transform *t, XMQNode *node, XMQNode *inst, XsltStylePreComp *comp)
{
    if (!node || !inst || !comp) return;
    char *url = NULL, *filename = NULL;
    XmlDom *res = NULL;
    XmlLibxslt1Stylesheet *style = NULL;
    const char *name = xml_node_name(inst);
    if (!comp->filename)
    {
        if (!strcmp(name, "output"))
        {
            // saxon:output, the file attribute is in no namespace.
            url = xslt_eval_attr_value_template(t, inst, "file", XSLT_SAXON_NAMESPACE);
            if (!url) url = xslt_eval_attr_value_template(t, inst, "href", XSLT_SAXON_NAMESPACE);
        }
        else if (!strcmp(name, "write"))
        {
            url = xslt_eval_attr_value_template(t, inst, "select", XSLT_XALAN_NAMESPACE);
            if (url)
            {
                // The value of the select attribute is an XPath expression (bug #59212).
                XmlXPathComp *cmp = xslt1_xpath_compile(&t->style->settings, NULL, url, false);
                char *val = cmp ? xslt_eval_xpath_string(t, cmp) : NULL;
                if (!cmp) t->state = XSLT_STATE_STOPPED;
                xml_xpath_comp_free(cmp);
                free(url);
                url = val;
            }
            if (!url) url = xslt_eval_attr_value_template(t, inst, "file", XSLT_XALAN_NAMESPACE);
            if (!url) url = xslt_eval_attr_value_template(t, inst, "href", XSLT_XALAN_NAMESPACE);
        }
        else if (!strcmp(name, "document"))
        {
            url = xslt_eval_attr_value_template(t, inst, "href", NULL);
        }
    }
    else
    {
        url = xslt1_strdup(comp->filename);
    }
    if (!url)
    {
        xslt1_transform_error(t, NULL, inst, "xsltDocumentElem: href/URI-Reference not found\n");
        return;
    }
    // If the computation failed, the url was probably not escaped.
    filename = xml_uri_build(url, t->output_file);
    if (!filename)
    {
        char *esc = xml_uri_escape_str(url, ":/.?,");
        if (esc)
        {
            filename = xml_uri_build(esc, t->output_file);
            free(esc);
        }
    }
    if (!filename)
    {
        xslt1_transform_error(t, NULL, inst, "xsltDocumentElem: URL computation failed for %s\n", url);
        free(url);
        return;
    }
    const char *old_output_file = t->output_file;
    XmlDom *old_output = t->output;
    XMQNode *old_insert = t->insert;
    int old_type = t->type;
    t->output_file = filename;
    style = xml_libxslt1_stylesheet_new_empty(NULL, &t->style->settings);
    // The 1.1 draft allows full parameterization of the output.
    char *prop;
    if ((prop = xslt_eval_attr_value_template(t, inst, "version", NULL)))
    {
        free(style->version);
        style->version = prop;
    }
    if ((prop = xslt_eval_attr_value_template(t, inst, "encoding", NULL)))
    {
        free(style->encoding);
        style->encoding = prop;
    }
    if ((prop = xslt_eval_attr_value_template(t, inst, "method", NULL)))
    {
        free(style->method);
        style->method = NULL;
        free(style->method_uri);
        style->method_uri = NULL;
        const char *uri = xslt1_get_qname_uri(&t->style->settings, inst, &prop);
        if (!prop)
        {
            style->errors++;
        }
        else if (!uri)
        {
            if (!strcmp(prop, "xml") || !strcmp(prop, "html") || !strcmp(prop, "text"))
            {
                style->method = prop;
            }
            else
            {
                xslt1_transform_error(t, NULL, inst, "invalid value for method: %s\n", prop);
                style->warnings++;
                free(prop);
            }
        }
        else
        {
            style->method = prop;
            style->method_uri = xslt1_strdup(uri);
        }
    }
    if ((prop = xslt_eval_attr_value_template(t, inst, "doctype-system", NULL)))
    {
        free(style->doctype_system);
        style->doctype_system = prop;
    }
    if ((prop = xslt_eval_attr_value_template(t, inst, "doctype-public", NULL)))
    {
        free(style->doctype_public);
        style->doctype_public = prop;
    }
    const char *yesno[] = { "standalone", "indent", "omit-xml-declaration" };
    int *targets[] = { &style->standalone, &style->indent, &style->omit_xml_declaration };
    for (int i = 0; i < 3; i++)
    {
        if ((prop = xslt_eval_attr_value_template(t, inst, yesno[i], NULL)))
        {
            if (!strcmp(prop, "yes")) *targets[i] = 1;
            else if (!strcmp(prop, "no")) *targets[i] = 0;
            else
            {
                xslt1_transform_error(t, NULL, inst, "invalid value for %s: %s\n", yesno[i], prop);
                style->warnings++;
            }
            free(prop);
        }
    }
    char *elements = xslt_eval_attr_value_template(t, inst, "cdata-section-elements", NULL);
    if (elements)
    {
        // Like libxslt they go into the strip-space table.
        if (!style->strip_spaces) style->strip_spaces = hashmap_create(10);
        const char *element = elements;
        while (*element)
        {
            while (xslt1_is_blank((unsigned char)*element)) element++;
            if (!*element) break;
            const char *end = element;
            while (*end && !xslt1_is_blank((unsigned char)*end)) end++;
            char *e = xslt1_strndup(element, (size_t)(end - element));
            const char *uri = xslt1_get_qname_uri(&t->style->settings, inst, &e);
            if (e)
            {
                char *key = xslt1_key2(e, uri);
                if (!hashmap_get(style->strip_spaces, key)) hashmap_put(style->strip_spaces, key, (void*)"cdata");
                free(key);
                free(e);
            }
            element = end;
        }
        free(elements);
    }
    // Create a new document tree and process the element template.
    const char *method = xslt_import_method(style);
    const char *doctype_public = xslt_import_doctype_public(style);
    const char *doctype_system = xslt_import_doctype_system(style);
    const char *version = xslt_import_version(style);
    const char *encoding = xslt_import_encoding(style);
    if (method && strcmp(method, "xml"))
    {
        if (!strcmp(method, "html"))
        {
            t->type = XSLT_OUTPUT_HTML;
            if (doctype_public || doctype_system)
            {
                res = xslt_html_new_doc(doctype_system, doctype_public);
            }
            else
            {
                if (version) xslt_get_html_ids(version, &doctype_public, &doctype_system);
                // htmlNewDocNoDtD.
                res = xml_dom_new();
                if (doctype_public || doctype_system) xml_dom_set_dtd(res, "html", doctype_public, doctype_system);
            }
        }
        else if (!strcmp(method, "xhtml"))
        {
            xslt1_transform_error(t, NULL, inst, "xsltDocumentElem: unsupported method xhtml\n");
            t->type = XSLT_OUTPUT_HTML;
            res = xml_dom_new();
            if (doctype_public || doctype_system) xml_dom_set_dtd(res, "html", doctype_public, doctype_system);
        }
        else if (!strcmp(method, "text"))
        {
            t->type = XSLT_OUTPUT_TEXT;
            res = xml_dom_new();
            xml_dom_set_version(res, style->version ? style->version : "1.0");
        }
        else
        {
            xslt1_transform_error(t, NULL, inst, "xsltDocumentElem: unsupported method (%s)\n", method);
            goto error;
        }
    }
    else
    {
        t->type = XSLT_OUTPUT_XML;
        res = xml_dom_new();
        xml_dom_set_version(res, style->version ? style->version : "1.0");
    }
    if (encoding) xml_dom_set_encoding(res, encoding);
    t->output = res;
    t->insert = xml_dom_document(res);
    xslt_apply_sequence_constructor(t, node, xml_node_first_child(inst), NULL);
    // Post processing depending on the generated output.
    XMQNode *root;
    root = xml_dom_root_element(res);
    if (root)
    {
        char *doctype;
        XmlNs *rns = xml_node_ns(root);
        if (rns && xml_ns_prefix(rns))
        {
            doctype = (char*)malloc(strlen(xml_ns_prefix(rns)) + strlen(xml_node_name(root)) + 2);
            sprintf(doctype, "%s:%s", xml_ns_prefix(rns), xml_node_name(root));
        }
        else
        {
            doctype = xslt1_strdup(xml_node_name(root));
        }
        if (!method && !rns && !strcasecmp(xml_node_name(root), "html"))
        {
            XMQNode *tmp = xml_node_first_child(xml_dom_document(res));
            while (tmp && tmp != root)
            {
                if (xml_node_kind(tmp) == XML_DOM_ELEMENT) break;
                if (xml_node_kind(tmp) == XML_DOM_TEXT && !xslt_is_blank_text_node(tmp)) break;
                tmp = xml_node_next(tmp);
            }
            if (tmp == root)
            {
                t->type = XSLT_OUTPUT_HTML;
                if (doctype_public || doctype_system)
                {
                    xml_dom_set_dtd(res, doctype, doctype_public, doctype_system);
                }
                else if (version)
                {
                    xslt_get_html_ids(version, &doctype_public, &doctype_system);
                    if (doctype_public || doctype_system) xml_dom_set_dtd(res, doctype, doctype_public, doctype_system);
                }
            }
        }
        if (t->type == XSLT_OUTPUT_XML)
        {
            doctype_public = xslt_import_doctype_public(style);
            doctype_system = xslt_import_doctype_system(style);
            if (doctype_public || doctype_system) xml_dom_set_dtd(res, doctype, doctype_public, doctype_system);
        }
        free(doctype);
    }
    // redirect:write can append to the file (append="true|yes").
    bool append;
    append = false;
    if ((prop = xslt_eval_attr_value_template(t, inst, "append", NULL)))
    {
        if (!strcmp(prop, "true") || !strcmp(prop, "yes"))
        {
            style->omit_xml_declaration = 1;
            append = true;
        }
        else
        {
            style->omit_xml_declaration = 0;
        }
        free(prop);
    }
    if (xslt_save_result_to_filename(t, filename, res, style, append) < 0)
    {
        xslt1_transform_error(t, NULL, inst, "xsltDocumentElem: unable to save to %s\n", filename);
    }
error:
    t->output = old_output;
    t->insert = old_insert;
    t->type = old_type;
    t->output_file = old_output_file;
    free(url);
    free(filename);
    if (style) xml_libxslt1_stylesheet_free(style);
    if (res) xml_dom_free(res);
}

// Numbers, port of libxslt numbers.c ////////////////////////////

#define XSLT_NUM_MAX_TOKENS 1024

typedef struct
{
    char *separator;
    int token;
    int width;
} XsltFormatToken;

typedef struct
{
    char *start;
    XsltFormatToken tokens[XSLT_NUM_MAX_TOKENS];
    int n_tokens;
    char *end;
} XsltFormat;

static const char xslt_alpha_upper_list_[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char xslt_alpha_lower_list_[] = "abcdefghijklmnopqrstuvwxyz";

/** xmlUTF8Strsize(utf, 1): the bytes of the first character. */
static int xslt_utf8_size1(const char *utf)
{
    if (!utf) return 0;
    const unsigned char *ptr = (const unsigned char*)utf;
    if (!*ptr) return 0;
    int ch = *ptr++;
    if (ch & 0x80)
    {
        while ((ch <<= 1) & 0x80)
        {
            if (*ptr == 0) break;
            ptr++;
        }
    }
    return (int)(ptr - (const unsigned char*)utf);
}

/** Port of xsltUTF8Charcmp: compare the first character of utf1 with utf2. */
static int xslt_utf8_charcmp(const char *utf1, const char *utf2)
{
    int len = xslt_utf8_size1(utf1);
    if (len < 1) return -1;
    if (!utf2) return 1;
    return strncmp(utf1, utf2, (size_t)len);
}

/** xmlUTF8Strloc: the index (in characters) of utfchar in utf, -1 if not found. */
static int xslt_utf8_strloc(const char *utf, const char *utfchar)
{
    if (!utf || !utfchar) return -1;
    int size = xslt_utf8_size1(utfchar);
    for (int i = 0; *utf; i++)
    {
        if (!strncmp(utf, utfchar, (size_t)size)) return i;
        int ch = (unsigned char)*utf++;
        if (ch & 0x80)
        {
            if ((ch & 0xc0) != 0xc0) return -1;
            while ((ch <<= 1) & 0x80)
            {
                if ((*utf++ & 0xc0) != 0x80) return -1;
            }
        }
    }
    return -1;
}

/** xmlUTF8Strpos: the pos-th character. */
static const char *xslt_utf8_strpos(const char *utf, int pos)
{
    if (!utf || pos < 0) return NULL;
    while (pos--)
    {
        int ch = (unsigned char)*utf++;
        if (ch == 0) return NULL;
        if (ch & 0x80)
        {
            if ((ch & 0xc0) != 0xc0) return NULL;
            while ((ch <<= 1) & 0x80)
            {
                if ((*utf++ & 0xc0) != 0x80) return NULL;
            }
        }
    }
    return utf;
}

/** Port of xsltCopyCharMultiByte. */
static int xslt_copy_char_multi_byte(char *out, int val)
{
    if (!out || val < 0) return 0;
    if (val >= 0x80)
    {
        char *savedout = out;
        int bits;
        if (val < 0x800) { *out++ = (char)((val >> 6) | 0xC0); bits = 0; }
        else if (val < 0x10000) { *out++ = (char)((val >> 12) | 0xE0); bits = 6; }
        else if (val < 0x110000) { *out++ = (char)((val >> 18) | 0xF0); bits = 12; }
        else return 0;
        for (; bits >= 0; bits -= 6) *out++ = (char)(((val >> bits) & 0x3F) | 0x80);
        return (int)(out - savedout);
    }
    *out = (char)val;
    return 1;
}

static bool xslt_is_letter_digit(int val)
{
    return xml_char_is_base_char(val) || xml_char_is_ideographic(val) || xml_char_is_digit(val);
}

static bool xslt_is_digit_zero(int ch)
{
    switch (ch)
    {
    case 0x0030: case 0x0660: case 0x06F0: case 0x0966:
    case 0x09E6: case 0x0A66: case 0x0AE6: case 0x0B66:
    case 0x0C66: case 0x0CE6: case 0x0D66: case 0x0E50:
    case 0x0ED0: case 0x0F20:
        return true;
    default:
        return false;
    }
}

#define XSLT_IS_DIGIT_ZERO(x) xslt_is_digit_zero(x)
#define XSLT_IS_DIGIT_ONE(x) xslt_is_digit_zero((x) - 1)

/** Port of xsltNumberFormatDecimal. */
static void xslt_number_format_decimal(MemBuffer *buffer, double number, int digit_zero, int width, int digits_per_group,
                                       int grouping_character, int grouping_character_len, const XmlLibxslt1Settings *settings)
{
    char temp_string[500];
    char temp_char[6];
    // Build the buffer from the back.
    char *pointer = &temp_string[sizeof(temp_string)] - 1;
    *pointer = 0;
    int i = 0;
    while (pointer > temp_string)
    {
        if (i >= width && fabs(number) < 1.0) break;
        if (i > 0 && grouping_character != 0 && digits_per_group > 0 && (i % digits_per_group) == 0)
        {
            if (pointer - grouping_character_len < temp_string)
            {
                i = -1;
                break;
            }
            pointer -= grouping_character_len;
            xslt_copy_char_multi_byte(pointer, grouping_character);
        }
        int val = digit_zero + (int)fmod(number, 10.0);
        if (val < 0x80)
        {
            if (pointer <= temp_string)
            {
                i = -1;
                break;
            }
            *(--pointer) = (char)val;
        }
        else
        {
            int len = xslt_copy_char_multi_byte(temp_char, val);
            if (pointer - len < temp_string)
            {
                i = -1;
                break;
            }
            pointer -= len;
            memcpy(pointer, temp_char, (size_t)len);
        }
        number /= 10.0;
        ++i;
    }
    if (i < 0) xslt1_generic_error(settings, "xsltNumberFormatDecimal: Internal buffer size exceeded\n");
    membuffer_append(buffer, pointer);
}

/** Port of xsltNumberFormatAlpha. */
static void xslt_number_format_alpha(XsltNumberData *data, MemBuffer *buffer, double number, int is_upper,
                                     const XmlLibxslt1Settings *settings)
{
    char temp_string[sizeof(double) * 8 + 1];
    double alpha_size = (double)(sizeof(xslt_alpha_upper_list_) - 1);
    // The "a" token has an intrinsic lower limit of 1.
    if (number < 1.0)
    {
        xslt_number_format_decimal(buffer, number, '0', 1, data->digits_per_group, data->grouping_character,
                                   data->grouping_character_len, settings);
        return;
    }
    char *pointer = &temp_string[sizeof(temp_string)];
    *(--pointer) = 0;
    const char *alpha_list = is_upper ? xslt_alpha_upper_list_ : xslt_alpha_lower_list_;
    for (int i = 1; i < (int)sizeof(temp_string); i++)
    {
        number--;
        *(--pointer) = alpha_list[((int)fmod(number, alpha_size))];
        number /= alpha_size;
        if (number < 1.0) break;
    }
    membuffer_append(buffer, pointer);
}

/** Port of xsltNumberFormatRoman. */
static void xslt_number_format_roman(XsltNumberData *data, MemBuffer *buffer, double number, int is_upper,
                                     const XmlLibxslt1Settings *settings)
{
    // A reasonable upper bound to avoid denial of service.
    if (number < 1.0 || number > 5000.0)
    {
        xslt_number_format_decimal(buffer, number, '0', 1, data->digits_per_group, data->grouping_character,
                                   data->grouping_character_len, settings);
        return;
    }
    static const struct { double v; const char *u; const char *l; bool repeat; } steps[] = {
        { 1000.0, "M", "m", true }, { 900.0, "CM", "cm", false }, { 500.0, "D", "d", true }, { 400.0, "CD", "cd", false },
        { 100.0, "C", "c", true }, { 90.0, "XC", "xc", false }, { 50.0, "L", "l", true }, { 40.0, "XL", "xl", false },
        { 10.0, "X", "x", true }, { 9.0, "IX", "ix", false }, { 5.0, "V", "v", true }, { 4.0, "IV", "iv", false },
        { 1.0, "I", "i", true } };
    for (size_t k = 0; k < sizeof(steps) / sizeof(steps[0]); k++)
    {
        if (steps[k].repeat)
        {
            while (number >= steps[k].v)
            {
                membuffer_append(buffer, is_upper ? steps[k].u : steps[k].l);
                number -= steps[k].v;
            }
        }
        else if (number >= steps[k].v)
        {
            membuffer_append(buffer, is_upper ? steps[k].u : steps[k].l);
            number -= steps[k].v;
        }
    }
}

/** Port of xsltNumberFormatTokenize. */
static void xslt_number_format_tokenize(const char *format, XsltFormat *tokens)
{
    int ix = 0;
    int val, len;
    tokens->start = NULL;
    tokens->tokens[0].separator = NULL;
    tokens->end = NULL;
    // The initial non-alphanumeric token (always there, even if NULL).
    while (!xslt_is_letter_digit(val = xslt1_utf8_char(format + ix, &len)))
    {
        if (format[ix] == 0) break;
        ix += len;
    }
    if (ix > 0) tokens->start = xslt1_strndup(format, (size_t)ix);
    for (tokens->n_tokens = 0; tokens->n_tokens < XSLT_NUM_MAX_TOKENS; tokens->n_tokens++)
    {
        if (format[ix] == 0) break;
        // The separator has already been parsed (except for the first number) in tokens->end.
        if (tokens->n_tokens > 0)
        {
            tokens->tokens[tokens->n_tokens].separator = tokens->end;
            tokens->end = NULL;
        }
        val = xslt1_utf8_char(format + ix, &len);
        if (XSLT_IS_DIGIT_ONE(val) || XSLT_IS_DIGIT_ZERO(val))
        {
            tokens->tokens[tokens->n_tokens].width = 1;
            while (XSLT_IS_DIGIT_ZERO(val))
            {
                tokens->tokens[tokens->n_tokens].width++;
                ix += len;
                val = xslt1_utf8_char(format + ix, &len);
            }
            if (XSLT_IS_DIGIT_ONE(val))
            {
                tokens->tokens[tokens->n_tokens].token = val - 1;
                ix += len;
                val = xslt1_utf8_char(format + ix, &len);
            }
            else
            {
                tokens->tokens[tokens->n_tokens].token = '0';
                tokens->tokens[tokens->n_tokens].width = 1;
            }
        }
        else if (val == 'A' || val == 'a' || val == 'I' || val == 'i')
        {
            tokens->tokens[tokens->n_tokens].token = val;
            ix += len;
            val = xslt1_utf8_char(format + ix, &len);
        }
        else
        {
            // Any other format token: a format token of 1 (XSLT 7.7).
            tokens->tokens[tokens->n_tokens].token = '0';
            tokens->tokens[tokens->n_tokens].width = 1;
        }
        // Skip the remaining alphanumeric characters.
        while (xslt_is_letter_digit(val))
        {
            ix += len;
            val = xslt1_utf8_char(format + ix, &len);
        }
        // The temporary non-alphanumeric final token.
        int j = ix;
        while (!xslt_is_letter_digit(val))
        {
            if (val == 0) break;
            ix += len;
            val = xslt1_utf8_char(format + ix, &len);
        }
        if (ix > j) tokens->end = xslt1_strndup(&format[j], (size_t)(ix - j));
    }
}

/** Port of xsltNumberFormatInsertNumbers. */
static void xslt_number_format_insert_numbers(XmlLibxslt1Transform *t, XsltNumberData *data, double *numbers, int numbers_max,
                                              XsltFormat *tokens, MemBuffer *buffer)
{
    static const XsltFormatToken default_token = { (char*)".", '0', 1 };
    const XmlLibxslt1Settings *settings = &t->style->settings;
    if (tokens->start) membuffer_append(buffer, tokens->start);
    for (int i = 0; i < numbers_max; i++)
    {
        double number = numbers[(numbers_max - 1) - i];
        // Round to nearest like XSLT 2.0.
        number = floor(number + 0.5);
        if (number < 0.0)
        {
            xslt1_settings_error(settings, NULL, "xsl-number : negative value\n");
            // Recover by treating negative values as zero.
            number = 0.0;
        }
        const XsltFormatToken *token;
        if (i < tokens->n_tokens) token = &tokens->tokens[i];
        else if (tokens->n_tokens > 0) token = &tokens->tokens[tokens->n_tokens - 1];
        else token = &default_token;
        // The separator, except for the first number.
        if (i > 0) membuffer_append(buffer, token->separator ? token->separator : ".");
        if (isinf(number))
        {
            membuffer_append(buffer, number < 0 ? "-Infinity" : "Infinity");
        }
        else if (isnan(number))
        {
            membuffer_append(buffer, "NaN");
        }
        else
        {
            switch (token->token)
            {
            case 'A': xslt_number_format_alpha(data, buffer, number, 1, settings); break;
            case 'a': xslt_number_format_alpha(data, buffer, number, 0, settings); break;
            case 'I': xslt_number_format_roman(data, buffer, number, 1, settings); break;
            case 'i': xslt_number_format_roman(data, buffer, number, 0, settings); break;
            default:
                if (XSLT_IS_DIGIT_ZERO(token->token))
                {
                    xslt_number_format_decimal(buffer, number, token->token, token->width, data->digits_per_group,
                                               data->grouping_character, data->grouping_character_len, settings);
                }
                break;
            }
        }
    }
    if (tokens->end) membuffer_append(buffer, tokens->end);
}

/** Port of xsltTestCompMatchCount. */
static int xslt_test_comp_match_count(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Pattern *count_pat, XMQNode *cur)
{
    if (count_pat) return xml_libxslt1_pattern_match(t, node, count_pat);
    // The default: the same node type and expanded name as the current node.
    if (xml_node_kind(node) != xml_node_kind(cur)) return 0;
    if (xml_node_kind(node) == XML_DOM_NAMESPACE) return 1;
    if (!xslt1_streq(xml_node_name(node), xml_node_name(cur))) return 0;
    XmlNs *a = xml_node_ns(node), *b = xml_node_ns(cur);
    if (a == b) return 1;
    if (!a || !b) return 0;
    return xslt1_streq(xml_ns_href(a), xml_ns_href(b));
}

/** The previous sibling as libxslt sees it (skipping nothing). */
static XMQNode *xslt_prev(XMQNode *n)
{
    if (xml_node_kind(n) == XML_DOM_NAMESPACE || xml_node_kind(n) == XML_DOM_ATTRIBUTE) return NULL;
    return xml_node_prev(n);
}

/** Port of xsltNumberFormatGetAnyLevel. */
static int xslt_number_format_get_any_level(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Pattern *count_pat,
                                            XmlLibxslt1Pattern *from_pat, double *array)
{
    int amount = 0, cnt = 0;
    XMQNode *cur = node;
    while (cur)
    {
        if (xslt_test_comp_match_count(t, cur, count_pat, node)) cnt++;
        if (from_pat && xml_libxslt1_pattern_match(t, cur, from_pat) == 1) break;
        // Skip to the next preceding or ancestor.
        if (xml_node_kind(cur) == XML_DOM_DOCUMENT) break;
        if (xml_node_kind(cur) == XML_DOM_NAMESPACE || xml_node_kind(cur) == XML_DOM_ATTRIBUTE)
        {
            cur = xml_node_parent(cur);
        }
        else
        {
            while (xslt_prev(cur) && xml_node_kind(xslt_prev(cur)) == XML_DOM_DTD) cur = xslt_prev(cur);
            if (xslt_prev(cur))
            {
                for (cur = xslt_prev(cur); lx_children(cur) && xml_node_last_child(cur); cur = xml_node_last_child(cur)) {}
            }
            else
            {
                cur = xml_node_parent(cur);
            }
        }
    }
    array[amount++] = (double)cnt;
    return amount;
}

/** Port of xsltNumberFormatGetMultipleLevel. */
static int xslt_number_format_get_multiple_level(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Pattern *count_pat,
                                                 XmlLibxslt1Pattern *from_pat, double *array, int max)
{
    int amount = 0;
    // ancestor-or-self::*[count]
    XMQNode *ancestor = node;
    while (ancestor && xml_node_kind(ancestor) != XML_DOM_DOCUMENT)
    {
        if (from_pat && xml_libxslt1_pattern_match(t, ancestor, from_pat) == 1) break;
        if (xslt_test_comp_match_count(t, ancestor, count_pat, node))
        {
            // count(preceding-sibling::*)
            int cnt = 1;
            XMQNode *preceding = xml_node_kind(ancestor) != XML_DOM_NAMESPACE ? xslt_prev(ancestor) : NULL;
            while (preceding)
            {
                if (xslt_test_comp_match_count(t, preceding, count_pat, node)) cnt++;
                preceding = xslt_prev(preceding);
            }
            array[amount++] = (double)cnt;
            if (amount >= max) break;
        }
        ancestor = xml_node_parent(ancestor);
    }
    return amount;
}

/** Port of xsltNumberFormatGetValue: number(value) evaluated with node as the context node. */
static int xslt_number_format_get_value(XmlLibxslt1Transform *t, XMQNode *node, const char *value, XMQNode *inst, double *number)
{
    int amount = 0;
    size_t n = strlen(value);
    char *expr = (char*)malloc(n + 9);
    snprintf(expr, n + 9, "number(%s)", value);
    XmlXPathComp *comp = xslt1_xpath_compile(&t->style->settings, inst, expr, false);
    if (comp)
    {
        XmlXPathResult *obj = xslt_xpath_eval(t, comp, node);
        if (obj)
        {
            *number = xml_xpath_result_to_number(obj);
            amount++;
            xml_xpath_result_free(obj);
        }
        xml_xpath_comp_free(comp);
    }
    free(expr);
    return amount;
}

/** Port of xsltNumberFormat (xsl:number). */
static void xslt_number_format(XmlLibxslt1Transform *t, XsltNumberData *data, XMQNode *node)
{
    XsltFormat tokens;
    if (data->format)
    {
        xslt_number_format_tokenize(data->format, &tokens);
    }
    else
    {
        // The format needs to be recomputed each time.
        if (data->has_format == 0) return;
        char *format = xslt_eval_attr_value_template(t, data->node, "format", XSLT_NAMESPACE);
        if (!format) return;
        xslt_number_format_tokenize(format, &tokens);
        free(format);
    }
    MemBuffer *output = new_membuffer();
    double number;
    int amount;
    if (data->value)
    {
        amount = xslt_number_format_get_value(t, node, data->value, data->node, &number);
        if (amount == 1) xslt_number_format_insert_numbers(t, data, &number, 1, &tokens, output);
    }
    else if (data->level)
    {
        if (!strcmp(data->level, "single"))
        {
            amount = xslt_number_format_get_multiple_level(t, node, data->count_pat, data->from_pat, &number, 1);
            if (amount == 1) xslt_number_format_insert_numbers(t, data, &number, 1, &tokens, output);
        }
        else if (!strcmp(data->level, "multiple"))
        {
            double numarray[1024];
            int max = (int)(sizeof(numarray) / sizeof(numarray[0]));
            amount = xslt_number_format_get_multiple_level(t, node, data->count_pat, data->from_pat, numarray, max);
            if (amount > 0) xslt_number_format_insert_numbers(t, data, numarray, amount, &tokens, output);
        }
        else if (!strcmp(data->level, "any"))
        {
            amount = xslt_number_format_get_any_level(t, node, data->count_pat, data->from_pat, &number);
            if (amount > 0) xslt_number_format_insert_numbers(t, data, &number, 1, &tokens, output);
        }
        // count and from patterns can contain variable references, clear the cache of direct matches.
        if (data->count_pat) xslt1_comp_match_clear_cache(t, data->count_pat);
        if (data->from_pat) xslt1_comp_match_clear_cache(t, data->from_pat);
    }
    // Insert the number as a text node.
    membuffer_append_null(output);
    char *text = free_membuffer_but_return_trimmed_content(output);
    xslt_copy_text_string(t, t->insert, text, 0);
    free(text);
    free(tokens.start);
    free(tokens.end);
    for (int i = 0; i < tokens.n_tokens; i++) free(tokens.tokens[i].separator);
}

typedef struct
{
    int integer_hash;
    int integer_digits;
    int frac_digits;
    int frac_hash;
    int group;
    int multiplier;
    char add_decimal;
    char is_multiplier_set;
    char is_negative_pattern;
} XsltFormatNumberInfo;

#define XSLT_IS_SPECIAL(self, letter) \
    ((xslt_utf8_charcmp((letter), (self)->zero_digit) == 0) || \
     (xslt_utf8_charcmp((letter), (self)->digit) == 0) || \
     (xslt_utf8_charcmp((letter), (self)->decimal_point) == 0) || \
     (xslt_utf8_charcmp((letter), (self)->grouping) == 0) || \
     (xslt_utf8_charcmp((letter), (self)->pattern_separator) == 0))

/** Port of xsltFormatNumberPreSuffix. */
static int xslt_format_number_pre_suffix(XsltDecimalFormat *self, const char **format, XsltFormatNumberInfo *info)
{
    int count = 0;
    int len;
    while (1)
    {
        if (**format == 0) return count;
        if (**format == '\'')
        {
            // An escaped character, just count it.
            if (*++(*format) == 0) return -1;
        }
        else if (XSLT_IS_SPECIAL(self, *format))
        {
            return count;
        }
        else
        {
            // Percent and per-mille, only one of them.
            if (xslt_utf8_charcmp(*format, self->percent) == 0)
            {
                if (info->is_multiplier_set) return -1;
                info->multiplier = 100;
                info->is_multiplier_set = 1;
            }
            else if (xslt_utf8_charcmp(*format, self->permille) == 0)
            {
                if (info->is_multiplier_set) return -1;
                info->multiplier = 1000;
                info->is_multiplier_set = 1;
            }
        }
        if ((len = xslt_utf8_size1(*format)) < 1) return -1;
        count += len;
        *format += len;
    }
}

/** Port of xsltFormatNumberConversion: format-number(). Returns the malloced result. */
static char *xslt_format_number_conversion(XsltDecimalFormat *self, const char *format, double number, const XmlLibxslt1Settings *settings)
{
    const char *the_format, *prefix = NULL, *suffix = NULL;
    const char *nprefix, *nsuffix = NULL;
    int prefix_length, suffix_length = 0, nprefix_length, nsuffix_length;
    int j, len = 0;
    XsltFormatNumberInfo format_info;
    int delayed_multiplier = 0;
    char default_sign = 0;
    char found_error = 0;
    if (strlen(format) == 0)
    {
        xslt1_settings_error(settings, NULL, "xsltFormatNumberConversion : Invalid format (0-length)\n");
    }
    if (isnan(number))
    {
        return xslt1_strdup((!self || !self->no_number) ? "NaN" : self->no_number);
    }
    format_info.integer_hash = 0;
    format_info.integer_digits = 0;
    format_info.frac_digits = 0;
    format_info.frac_hash = 0;
    format_info.group = -1;
    format_info.multiplier = 1;
    format_info.add_decimal = 0;
    format_info.is_multiplier_set = 0;
    format_info.is_negative_pattern = 0;
    the_format = format;
    // First the positive pattern, for percent and per-mille and the main format.
    prefix = the_format;
    prefix_length = xslt_format_number_pre_suffix(self, &the_format, &format_info);
    if (prefix_length < 0)
    {
        found_error = 1;
        goto OUTPUT_NUMBER;
    }
    // The number part, a trailing percent or per-mille may be part of the suffix.
    int self_grouping_len;
    self_grouping_len = self->grouping ? (int)strlen(self->grouping) : 0;
    while (*the_format != 0 && xslt_utf8_charcmp(the_format, self->decimal_point) != 0 &&
           xslt_utf8_charcmp(the_format, self->pattern_separator) != 0)
    {
        if (delayed_multiplier != 0)
        {
            format_info.multiplier = delayed_multiplier;
            format_info.is_multiplier_set = 1;
            delayed_multiplier = 0;
        }
        if (xslt_utf8_charcmp(the_format, self->digit) == 0)
        {
            if (format_info.integer_digits > 0)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            format_info.integer_hash++;
            if (format_info.group >= 0) format_info.group++;
        }
        else if (xslt_utf8_charcmp(the_format, self->zero_digit) == 0)
        {
            format_info.integer_digits++;
            if (format_info.group >= 0) format_info.group++;
        }
        else if (self_grouping_len > 0 && !strncmp(the_format, self->grouping, (size_t)self_grouping_len))
        {
            // Reset the group count.
            format_info.group = 0;
            the_format += self_grouping_len;
            continue;
        }
        else if (xslt_utf8_charcmp(the_format, self->percent) == 0)
        {
            if (format_info.is_multiplier_set)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            delayed_multiplier = 100;
        }
        else if (xslt_utf8_charcmp(the_format, self->permille) == 0)
        {
            if (format_info.is_multiplier_set)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            delayed_multiplier = 1000;
        }
        else
        {
            break;
        }
        if ((len = xslt_utf8_size1(the_format)) < 1)
        {
            found_error = 1;
            goto OUTPUT_NUMBER;
        }
        the_format += len;
    }
    // The fraction.
    if (*the_format != 0 && xslt_utf8_charcmp(the_format, self->decimal_point) == 0)
    {
        format_info.add_decimal = 1;
        if ((len = xslt_utf8_size1(the_format)) < 1)
        {
            found_error = 1;
            goto OUTPUT_NUMBER;
        }
        the_format += len;
    }
    while (*the_format != 0)
    {
        if (xslt_utf8_charcmp(the_format, self->zero_digit) == 0)
        {
            if (format_info.frac_hash != 0)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            format_info.frac_digits++;
        }
        else if (xslt_utf8_charcmp(the_format, self->digit) == 0)
        {
            format_info.frac_hash++;
        }
        else if (xslt_utf8_charcmp(the_format, self->percent) == 0)
        {
            if (format_info.is_multiplier_set)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            delayed_multiplier = 100;
            if ((len = xslt_utf8_size1(the_format)) < 1)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            the_format += len;
            continue;
        }
        else if (xslt_utf8_charcmp(the_format, self->permille) == 0)
        {
            if (format_info.is_multiplier_set)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            delayed_multiplier = 1000;
            if ((len = xslt_utf8_size1(the_format)) < 1)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            the_format += len;
            continue;
        }
        else if (xslt_utf8_charcmp(the_format, self->grouping) != 0)
        {
            break;
        }
        if ((len = xslt_utf8_size1(the_format)) < 1)
        {
            found_error = 1;
            goto OUTPUT_NUMBER;
        }
        the_format += len;
        if (delayed_multiplier != 0)
        {
            format_info.multiplier = delayed_multiplier;
            delayed_multiplier = 0;
            format_info.is_multiplier_set = 1;
        }
    }
    // A delayed multiplier after the number part belongs to the suffix.
    if (delayed_multiplier != 0)
    {
        the_format -= len;
        delayed_multiplier = 0;
    }
    suffix = the_format;
    suffix_length = xslt_format_number_pre_suffix(self, &the_format, &format_info);
    if (suffix_length < 0 || (*the_format != 0 && xslt_utf8_charcmp(the_format, self->pattern_separator) != 0))
    {
        found_error = 1;
        goto OUTPUT_NUMBER;
    }
    // A negative number uses the negative prefix and suffix.
    if (number < 0)
    {
        // j is the number of utf-8 characters before the separator (bug 151975).
        j = xslt_utf8_strloc(format, self->pattern_separator);
        if (j < 0)
        {
            // No negative pattern, use the default sign.
            default_sign = 1;
        }
        else
        {
            the_format = xslt_utf8_strpos(format, j + 1);
            format_info.is_negative_pattern = 1;
            format_info.is_multiplier_set = 0;
            // The negative prefix.
            nprefix = the_format;
            nprefix_length = xslt_format_number_pre_suffix(self, &the_format, &format_info);
            if (nprefix_length < 0)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            while (*the_format != 0)
            {
                if (xslt_utf8_charcmp(the_format, self->percent) == 0 || xslt_utf8_charcmp(the_format, self->permille) == 0)
                {
                    if (format_info.is_multiplier_set)
                    {
                        found_error = 1;
                        goto OUTPUT_NUMBER;
                    }
                    format_info.is_multiplier_set = 1;
                    delayed_multiplier = 1;
                }
                else if (XSLT_IS_SPECIAL(self, the_format))
                {
                    delayed_multiplier = 0;
                }
                else
                {
                    break;
                }
                if ((len = xslt_utf8_size1(the_format)) < 1)
                {
                    found_error = 1;
                    goto OUTPUT_NUMBER;
                }
                the_format += len;
            }
            if (delayed_multiplier != 0)
            {
                format_info.is_multiplier_set = 0;
                the_format -= len;
            }
            // The negative suffix.
            if (*the_format != 0)
            {
                nsuffix = the_format;
                nsuffix_length = xslt_format_number_pre_suffix(self, &the_format, &format_info);
                if (nsuffix_length < 0)
                {
                    found_error = 1;
                    goto OUTPUT_NUMBER;
                }
            }
            else
            {
                nsuffix_length = 0;
            }
            if (*the_format != 0)
            {
                found_error = 1;
                goto OUTPUT_NUMBER;
            }
            // A Java peculiarity: if the negative prefix and suffix are the positive ones, use the default.
            if (nprefix_length != prefix_length || nsuffix_length != suffix_length ||
                (nprefix_length > 0 && strncmp(nprefix, prefix, (size_t)prefix_length) != 0) ||
                (nsuffix_length > 0 && strncmp(nsuffix, suffix, (size_t)suffix_length) != 0))
            {
                prefix = nprefix;
                prefix_length = nprefix_length;
                suffix = nsuffix;
                suffix_length = nsuffix_length;
            }
        }
    }
OUTPUT_NUMBER:
    if (found_error != 0)
    {
        xslt1_settings_error(settings, NULL, "xsltFormatNumberConversion : error in format string '%s', using default\n", format);
        default_sign = (number < 0.0) ? 1 : 0;
        prefix_length = suffix_length = 0;
        format_info.integer_hash = 0;
        format_info.integer_digits = 1;
        format_info.frac_digits = 1;
        format_info.frac_hash = 4;
        format_info.group = -1;
        format_info.multiplier = 1;
        format_info.add_decimal = 1;
    }
    // Apply the multiplier.
    number *= (double)format_info.multiplier;
    if (isinf(number))
    {
        MemBuffer *mb = new_membuffer();
        if (number < 0) membuffer_append(mb, self->minus_sign ? self->minus_sign : "-");
        membuffer_append(mb, (!self || !self->infinity) ? "Infinity" : self->infinity);
        membuffer_append_null(mb);
        return free_membuffer_but_return_trimmed_content(mb);
    }
    MemBuffer *buffer = new_membuffer();
    // The default sign.
    if (default_sign != 0) membuffer_append_region(buffer, self->minus_sign, self->minus_sign + xslt_utf8_size1(self->minus_sign));
    // The prefix.
    for (j = 0; j < prefix_length;)
    {
        if (*prefix == '\'') prefix++;
        len = xslt_utf8_size1(prefix);
        membuffer_append_region(buffer, prefix, prefix + len);
        prefix += len;
        j += len;
    }
    // Round to n digits.
    number = fabs(number);
    int exp10 = format_info.frac_digits + format_info.frac_hash;
    if (exp10 > DBL_MAX_10_EXP)
    {
        if (format_info.frac_digits > DBL_MAX_10_EXP)
        {
            format_info.frac_digits = DBL_MAX_10_EXP;
            format_info.frac_hash = 0;
        }
        else
        {
            format_info.frac_hash = DBL_MAX_10_EXP - format_info.frac_digits;
        }
        exp10 = DBL_MAX_10_EXP;
    }
    double scale = pow(10.0, (double)exp10);
    number += .5 / scale;
    number -= fmod(number, 1 / scale);
    // The integer part.
    if (self->grouping && self->grouping[0] != 0)
    {
        int glen;
        int gchar = xslt1_utf8_char(self->grouping, &glen);
        len = (int)strlen(self->grouping);
        if (glen > 0) len = glen;
        xslt_number_format_decimal(buffer, floor(number), (unsigned char)self->zero_digit[0], format_info.integer_digits,
                                   format_info.group, gchar, len, settings);
    }
    else
    {
        xslt_number_format_decimal(buffer, floor(number), (unsigned char)self->zero_digit[0], format_info.integer_digits,
                                   format_info.group, ',', 1, settings);
    }
    // Special case: java treats '.#' like '.0', '.##' like '.0#', etc.
    if (format_info.integer_digits + format_info.integer_hash + format_info.frac_digits == 0 && format_info.frac_hash > 0)
    {
        ++format_info.frac_digits;
        --format_info.frac_hash;
    }
    // A leading zero, if required.
    if (floor(number) == 0 && format_info.integer_digits + format_info.frac_digits == 0)
    {
        membuffer_append_region(buffer, self->zero_digit, self->zero_digit + xslt_utf8_size1(self->zero_digit));
    }
    // The fractional part, if required.
    if (format_info.frac_digits + format_info.frac_hash == 0)
    {
        if (format_info.add_decimal)
            membuffer_append_region(buffer, self->decimal_point, self->decimal_point + xslt_utf8_size1(self->decimal_point));
    }
    else
    {
        number -= floor(number);
        if (number != 0 || format_info.frac_digits != 0)
        {
            membuffer_append_region(buffer, self->decimal_point, self->decimal_point + xslt_utf8_size1(self->decimal_point));
            number = floor(scale * number + 0.5);
            for (j = format_info.frac_hash; j > 0; j--)
            {
                if (fmod(number, 10.0) >= 1.0) break;
                number /= 10.0;
            }
            xslt_number_format_decimal(buffer, floor(number), (unsigned char)self->zero_digit[0], format_info.frac_digits + j, 0, 0, 0, settings);
        }
    }
    // The suffix.
    for (j = 0; j < suffix_length;)
    {
        if (*suffix == '\'') suffix++;
        len = xslt_utf8_size1(suffix);
        membuffer_append_region(buffer, suffix, suffix + len);
        suffix += len;
        j += len;
    }
    membuffer_append_null(buffer);
    return free_membuffer_but_return_trimmed_content(buffer);
}

/** Port of xsltFormatNumberFunction: format-number(). */
static XmlXPathResult *xslt_format_number_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = (XmlLibxslt1Transform*)func_data;
    // Without an instruction libxslt returns without a value, which is a stack error.
    if (!t->inst)
    {
        xml_xpath_context_set_error(ctx, (XmlXPathErrorCode)23);
        return NULL;
    }
    XmlLibxslt1Stylesheet *sheet = t->style;
    XsltDecimalFormat *format_values = sheet->decimal_format;
    if (nargs != 2 && nargs != 3)
    {
        xml_xpath_context_set_error(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    if (nargs == 3)
    {
        char *qname = xml_xpath_result_to_string(args[2]);
        char *prefix = NULL;
        char *ncname = xslt1_split_qname(qname, &prefix);
        const char *ns_uri = NULL;
        if (prefix)
        {
            XmlNs *ns = lx_search_ns(t->inst, prefix);
            if (!ns)
            {
                xslt1_transform_error(t, NULL, NULL, "format-number : No namespace found for QName '%s:%s'\n", prefix, ncname);
                sheet->errors++;
                free(ncname);
                ncname = NULL;
            }
            else
            {
                ns_uri = xml_ns_href(ns);
            }
        }
        if (ncname) format_values = xslt1_decimal_format_get_by_qname(sheet, ns_uri, ncname);
        if (!format_values) xslt1_transform_error(t, NULL, NULL, "format-number() : undeclared decimal format '%s'\n", qname);
        free(ncname);
        free(prefix);
        free(qname);
    }
    if (!format_values)
    {
        // Nothing is pushed, a stack error in libxml2.
        xml_xpath_context_set_error(ctx, (XmlXPathErrorCode)23);
        return NULL;
    }
    char *format = xml_xpath_result_to_string(args[1]);
    double number = xml_xpath_result_to_number(args[0]);
    char *result = xslt_format_number_conversion(format_values, format, number, &sheet->settings);
    free(format);
    XmlXPathResult *r = xml_xpath_result_new_string(result ? result : "");
    free(result);
    return r;
}

// Keys, port of libxslt keys.c //////////////////////////////////

/** A key table of a document (libxslt xsltKeyTable): value -> node set. */
typedef struct XsltKeyTable XsltKeyTable;
struct XsltKeyTable
{
    XsltKeyTable *next;
    char *name;
    char *name_uri;
    HashMap *keys;
};

/** A set of nodes (the source nodes having keys, libxslt XSLT_SOURCE_NODE_HAS_KEY). */
typedef struct XsltNodeFlags
{
    const void **slots;
    size_t cap;
    size_t n;
} XsltNodeFlags;

static size_t xslt_ptr_hash(const void *p)
{
    size_t x = (size_t)p;
    x ^= x >> 17;
    x *= 0xed5ad4bbu;
    x ^= x >> 11;
    return x;
}

static bool xslt_flags_has(XsltNodeFlags *f, const void *p)
{
    if (!f || !f->cap) return false;
    for (size_t i = xslt_ptr_hash(p) & (f->cap - 1); f->slots[i]; i = (i + 1) & (f->cap - 1))
    {
        if (f->slots[i] == p) return true;
    }
    return false;
}

static void xslt_flags_add(XsltNodeFlags *f, const void *p)
{
    if (xslt_flags_has(f, p)) return;
    if ((f->n + 1) * 2 > f->cap)
    {
        size_t ncap = f->cap ? f->cap * 2 : 64;
        const void **ns = (const void**)calloc(ncap, sizeof(void*));
        for (size_t i = 0; i < f->cap; i++)
        {
            if (!f->slots[i]) continue;
            size_t j = xslt_ptr_hash(f->slots[i]) & (ncap - 1);
            while (ns[j]) j = (j + 1) & (ncap - 1);
            ns[j] = f->slots[i];
        }
        free(f->slots);
        f->slots = ns;
        f->cap = ncap;
    }
    size_t j = xslt_ptr_hash(p) & (f->cap - 1);
    while (f->slots[j]) j = (j + 1) & (f->cap - 1);
    f->slots[j] = p;
    f->n++;
}

static void xslt_free_key_table_list(XsltKeyTable *table)
{
    while (table)
    {
        XsltKeyTable *next = table->next;
        if (table->keys)
        {
            HashMapIterator *it = hashmap_iterate(table->keys);
            const char *key;
            void *val;
            while (hashmap_next_key_value(it, &key, &val)) xml_xpath_result_free((XmlXPathResult*)val);
            hashmap_free_iterator(it);
            hashmap_free(table->keys);
        }
        free(table->name);
        free(table->name_uri);
        free(table);
        table = next;
    }
}

/** Port of xsltFreeDocumentKeys. */
static void xslt_free_doc_keys(struct XsltDocInfo *info)
{
    if (!info) return;
    xslt_free_key_table_list(info->keys);
    info->keys = NULL;
}

/** Port of xsltInitCtxtKey: compute the key table of keyDef for the document idoc. */
static int xslt_init_ctxt_key(XmlLibxslt1Transform *t, XsltDocInfo *idoc, XsltKeyDef *key_def)
{
    if (!key_def->comp || !key_def->usecomp) return -1;
    // Detect recursive keys.
    if (t->key_init_level > t->nb_keys)
    {
        xslt1_transform_error(t, NULL, key_def->inst, "Key definition for %s is recursive\n", key_def->name);
        t->state = XSLT_STATE_STOPPED;
        return -1;
    }
    t->key_init_level++;
    idoc->nb_keys_computed++;
    XMQNode *old_inst = t->inst;
    XsltDocInfo *old_doc_info = t->document;
    XMQNode *old_context_node = t->node;
    long old_pos = t->xp_position, old_size = t->xp_size;
    XmlXPathResult *use_res = NULL;
    t->document = idoc;
    t->node = xml_dom_document(idoc->doc);
    t->inst = key_def->inst;
    // Evaluate the match expression of the xsl:key (with the context size and position as they are).
    XmlXPathResult *match_res = xslt_xpath_eval(t, key_def->comp, xml_dom_document(idoc->doc));
    if (!match_res)
    {
        xslt1_transform_error(t, NULL, key_def->inst, "Failed to evaluate the 'match' expression.\n");
        t->state = XSLT_STATE_STOPPED;
        goto error;
    }
    if (xml_xpath_result_type(match_res) != XML_XPATH_NODESET || xml_xpath_result_is_tree(match_res))
    {
        xslt1_transform_error(t, NULL, key_def->inst, "The 'match' expression did not evaluate to a node set.\n");
        t->state = XSLT_STATE_STOPPED;
        goto error;
    }
    size_t nmatch;
    nmatch = xml_xpath_result_size(match_res);
    if (nmatch == 0) goto error;
    // Multiple key definitions for the same name are allowed.
    XsltKeyTable *table;
    table = idoc->keys;
    while (table)
    {
        if (xslt1_streq(table->name, key_def->name) && xslt1_streq(table->name_uri, key_def->name_uri)) break;
        table = table->next;
    }
    if (!table)
    {
        table = (XsltKeyTable*)calloc(1, sizeof(XsltKeyTable));
        table->name = xslt1_strdup(key_def->name);
        table->name_uri = xslt1_strdup(key_def->name_uri);
        table->keys = hashmap_create(1024);
        table->next = idoc->keys;
        idoc->keys = table;
    }
    // The use expression is evaluated with a node list containing just the node.
    t->xp_size = 1;
    t->xp_position = 1;
    for (size_t i = 0; i < nmatch; i++)
    {
        XMQNode *cur = xml_xpath_result_node(match_res, i);
        if (!lx_is_real_node(cur)) continue;
        t->node = cur;
        xml_xpath_result_free(use_res);
        use_res = xslt_xpath_eval(t, key_def->usecomp, cur);
        if (!use_res)
        {
            xslt1_transform_error(t, NULL, key_def->inst, "Failed to evaluate the 'use' expression.\n");
            t->state = XSLT_STATE_STOPPED;
            break;
        }
        size_t len;
        bool nodes = xml_xpath_result_type(use_res) == XML_XPATH_NODESET;
        if (nodes)
        {
            len = xml_xpath_result_size(use_res);
            if (len == 0) continue;
        }
        else
        {
            len = 1;
        }
        for (size_t k = 0; k < len; k++)
        {
            char *str = nodes ? xml_node_string_value(xml_xpath_result_node(use_res, k)) : xml_xpath_result_to_string(use_res);
            if (!str) continue;
            XmlXPathResult *keylist = (XmlXPathResult*)hashmap_get(table->keys, str);
            if (!keylist)
            {
                keylist = xml_xpath_result_new_nodeset();
                xml_xpath_result_add_node(keylist, cur);
                hashmap_put(table->keys, str, keylist);
            }
            else
            {
                // xmlXPathNodeSetAdd checks for duplicates.
                size_t n = xml_xpath_result_size(keylist);
                bool dup = false;
                for (size_t j = 0; j < n && !dup; j++) dup = xml_xpath_result_node(keylist, j) == cur;
                if (!dup) xml_xpath_result_add_node(keylist, cur);
            }
            if (!t->keyed_nodes) t->keyed_nodes = calloc(1, sizeof(XsltNodeFlags));
            xslt_flags_add((XsltNodeFlags*)t->keyed_nodes, cur);
            free(str);
        }
    }
error:
    t->key_init_level--;
    t->xp_position = old_pos;
    t->xp_size = old_size;
    t->node = old_context_node;
    t->document = old_doc_info;
    t->inst = old_inst;
    xml_xpath_result_free(use_res);
    xml_xpath_result_free(match_res);
    return 0;
}

/** Port of xsltInitDocKeyTable. */
static int xslt_init_doc_key_table(XmlLibxslt1Transform *t, const char *name, const char *name_uri)
{
    XsltKeyDef *keyd = NULL;
    int found = 0;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        for (keyd = style->keys; keyd; keyd = keyd->next)
        {
            if (xslt1_streq(keyd->name, name) && xslt1_streq(keyd->name_uri, name_uri))
            {
                xslt_init_ctxt_key(t, t->document, keyd);
                if (t->document->nb_keys_computed == t->nb_keys) return 0;
                found = 1;
            }
        }
    }
    if (found == 0)
    {
        xslt1_transform_error(t, NULL, keyd ? keyd->inst : NULL, "Failed to find key definition for %s\n", name);
        t->state = XSLT_STATE_STOPPED;
        return -1;
    }
    return 0;
}

/** Port of xsltInitAllDocKeys. */
static int xslt_init_all_doc_keys(XmlLibxslt1Transform *t)
{
    if (t->document->nb_keys_computed == t->nb_keys) return 0;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        for (XsltKeyDef *keyd = style->keys; keyd; keyd = keyd->next)
        {
            // Check if keys with this QName have been already computed.
            XsltKeyTable *table = t->document->keys;
            while (table)
            {
                if (xslt1_streq(keyd->name, table->name) && xslt1_streq(keyd->name_uri, table->name_uri)) break;
                table = table->next;
            }
            if (!table) xslt_init_doc_key_table(t, keyd->name, keyd->name_uri);
        }
    }
    return 0;
}

/** Port of xsltGetKey: the nodes of the current document with the key value (borrowed). */
XmlXPathResult *xslt1_get_key(XmlLibxslt1Transform *t, const char *name, const char *name_uri, const char *value)
{
    int init_table = 0;
    if (!name || !value || !t->document) return NULL;
    // Keys are computed on demand, on the first key access for a document.
    if (t->document->nb_keys_computed < t->nb_keys && t->key_init_level == 0)
    {
        if (xslt_init_all_doc_keys(t)) return NULL;
    }
retry:
    for (XsltKeyTable *table = t->document->keys; table; table = table->next)
    {
        if (xslt1_streq(table->name, name) && xslt1_streq(table->name_uri, name_uri))
        {
            return (XmlXPathResult*)hashmap_get(table->keys, value);
        }
    }
    if (t->key_init_level != 0 && init_table == 0)
    {
        // A recursive key, initialize just this one and retry.
        xslt_init_doc_key_table(t, name, name_uri);
        init_table = 1;
        goto retry;
    }
    return NULL;
}

/** Does the node have a key (libxslt XSLT_SOURCE_NODE_HAS_KEY)? */
bool xslt1_node_has_key(XmlLibxslt1Transform *t, XMQNode *node)
{
    return xslt_flags_has((XsltNodeFlags*)t->keyed_nodes, node);
}

/** Port of xsltComputeAllKeys (for the keyed template patterns). */
int xslt1_compute_all_keys(XmlLibxslt1Transform *t, XMQNode *context_node)
{
    if (!context_node)
    {
        xslt1_transform_error(t, NULL, t->inst, "Internal error in xsltComputeAllKeys(): Bad arguments.\n");
        return -1;
    }
    if (!t->document) return -1;
    return xslt_init_all_doc_keys(t);
}

/** Port of xsltFindDocument. */
static XsltDocInfo *xslt_find_document(XmlLibxslt1Transform *t, XmlDom *doc)
{
    for (XsltDocInfo *ret = t->doc_list; ret; ret = ret->next)
    {
        if (ret->doc == doc) return ret;
    }
    if (doc == t->style->doc) return t->document;
    return NULL;
}

// Documents, port of libxslt documents.c (xsltLoadDocument) /////

static XsltDocInfo *xslt_new_document(XmlLibxslt1Transform *t, XmlDom *doc, int owned)
{
    XsltDocInfo *cur = (XsltDocInfo*)calloc(1, sizeof(XsltDocInfo));
    cur->doc = doc;
    cur->owned = owned;
    if (!xslt1_is_rtf(t, doc))
    {
        cur->next = t->doc_list;
        t->doc_list = cur;
    }
    return cur;
}

static int xslt_find_elem_space_handling(XmlLibxslt1Transform *t, XMQNode *node);
static int xslt_need_elem_space_handling(XmlLibxslt1Transform *t);
static void xslt_apply_strip_spaces(XmlLibxslt1Transform *t, XMQNode *node);

/** Port of xsltLoadDocument. */
static XsltDocInfo *xslt_load_document(XmlLibxslt1Transform *t, const char *uri)
{
    if (!uri) return NULL;
    for (XsltDocInfo *ret = t->doc_list; ret; ret = ret->next)
    {
        if (ret->doc && xml_dom_url(ret->doc) && !strcmp(xml_dom_url(ret->doc), uri)) return ret;
    }
    XmlDom *doc = xslt1_load(t->style, uri, XML_LIBXSLT1_LOAD_DOCUMENT);
    if (!doc) return NULL;
    // Apply white-space stripping if asked for.
    if (xslt_need_elem_space_handling(t)) xslt_apply_strip_spaces(t, xml_dom_root_element(doc));
    return xslt_new_document(t, doc, 1);
}

/** A copy of the stylesheet document (xmlCopyDoc), for document(''). */
static XmlDom *xslt_copy_doc(XmlDom *src)
{
    XmlDom *doc = xml_dom_new();
    xml_dom_set_url(doc, xml_dom_url(src));
    if (xml_dom_version(src)) xml_dom_set_version(doc, xml_dom_version(src));
    if (xml_dom_encoding(src)) xml_dom_set_encoding(doc, xml_dom_encoding(src));
    for (XMQNode *c = xml_node_first_child(xml_dom_document(src)); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_DTD)
        {
            xml_dom_set_dtd(doc, xml_node_name(c), xml_dtd_public_id(c), xml_dtd_system_id(c));
            continue;
        }
        XMQNode *copy = xml_node_copy(doc, c, true);
        if (copy) xml_node_append_child(xml_dom_document(doc), copy);
    }
    // The copied namespaces are references, make every subtree self contained.
    for (XMQNode *c = xml_node_first_child(xml_dom_document(doc)); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_ELEMENT) xml_node_reconcile_ns(c);
    }
    return doc;
}

XmlDom *xml_libxslt1_copy_stylesheet_doc(const XmlLibxslt1Stylesheet *style)
{
    return (style && style->doc) ? xslt_copy_doc(style->doc) : NULL;
}

// Functions, port of libxslt functions.c and extra.c ////////////

static XmlLibxslt1Transform *xslt_tctxt(void *func_data)
{
    return (XmlLibxslt1Transform*)func_data;
}

static void xslt_nodeset_merge(XmlXPathResult *to, const XmlXPathResult *from);

// XPointer, port of the parts of libxml2 xpointer.c used by document() /////

typedef struct
{
    XmlDom *doc;
    const char *cur;
    bool error;
    XmlXPathNamespace *ns;  // From xmlns() parts.
    size_t num_ns;
} XsltXPtr;

static bool xslt_xptr_is_name_start(int c)
{
    return xml_char_is_letter(c) || c == '_' || c == ':';
}

static bool xslt_xptr_is_name_char(int c)
{
    return xml_char_is_letter(c) || xml_char_is_digit(c) || c == '.' || c == '-' || c == '_' || c == ':' ||
           xml_char_is_combining(c) || xml_char_is_extender(c);
}

/** xmlXPathParseName: a QName at cur, NULL if there is none. */
static char *xslt_xptr_parse_name(XsltXPtr *x)
{
    int len;
    const char *start = x->cur;
    int c = xslt1_utf8_char(x->cur, &len);
    if (c <= 0 || !xslt_xptr_is_name_start(c)) return NULL;
    while (c > 0 && xslt_xptr_is_name_char(c))
    {
        x->cur += len;
        c = xslt1_utf8_char(x->cur, &len);
    }
    return xslt1_strndup(start, (size_t)(x->cur - start));
}

static void xslt_xptr_skip_blanks(XsltXPtr *x)
{
    while (xslt1_is_blank((unsigned char)*x->cur)) x->cur++;
}

/** xmlXPtrGetNthChild: the no-th element child (counting from 1). */
static XMQNode *xslt_xptr_nth_child(XMQNode *cur, int no)
{
    if (!cur || xml_node_kind(cur) == XML_DOM_NAMESPACE) return cur;
    int i = 0;
    for (cur = xml_node_first_child(cur); i <= no; cur = xml_node_next(cur))
    {
        if (!cur) return cur;
        if (xml_node_kind(cur) == XML_DOM_ELEMENT || xml_node_kind(cur) == XML_DOM_DOCUMENT)
        {
            i++;
            if (i == no) break;
        }
    }
    return cur;
}

/** xmlXPtrEvalChildSeq: a bare name (an id) and/or /n/m child steps, from the node set. */
static XmlXPathResult *xslt_xptr_child_seq(XsltXPtr *x, XmlXPathContext *ctx, char *name, XmlXPathResult *set)
{
    if (name)
    {
        xml_xpath_result_free(set);
        set = xml_xpath_result_new_nodeset();
        XMQNode *e = xml_xpath_context_get_id(ctx, xml_dom_document(x->doc), name);
        if (e) xml_xpath_result_add_node(set, e);
        free(name);
    }
    while (*x->cur == '/')
    {
        int child = 0;
        bool overflow = false;
        x->cur++;
        while (*x->cur >= '0' && *x->cur <= '9')
        {
            int d = *x->cur - '0';
            if (child > INT_MAX / 10) overflow = true;
            else child *= 10;
            if (child > INT_MAX - d) overflow = true;
            else child += d;
            x->cur++;
        }
        if (overflow) child = 0;
        // xmlXPtrGetChildNo.
        XMQNode *cur = NULL;
        if (child > 0 && set && xml_xpath_result_size(set) == 1) cur = xslt_xptr_nth_child(xml_xpath_result_node(set, 0), child);
        xml_xpath_result_free(set);
        set = xml_xpath_result_new_nodeset();
        if (cur) xml_xpath_result_add_node(set, cur);
    }
    return set;
}

/** xmlXPtrEvalXPtrPart: one scheme(...) part, NULL if it gives nothing. */
static XmlXPathResult *xslt_xptr_part(XsltXPtr *x, XmlXPathContext *ctx, char *name)
{
    if (*x->cur != '(')
    {
        free(name);
        x->error = true;
        return NULL;
    }
    x->cur++;
    int level = 1;
    MemBuffer *mb = new_membuffer();
    while (*x->cur)
    {
        if (*x->cur == ')')
        {
            level--;
            if (level == 0)
            {
                x->cur++;
                break;
            }
        }
        else if (*x->cur == '(')
        {
            level++;
        }
        else if (*x->cur == '^')
        {
            if (x->cur[1] == ')' || x->cur[1] == '(' || x->cur[1] == '^') x->cur++;
        }
        membuffer_append_char(mb, *x->cur);
        x->cur++;
    }
    membuffer_append_null(mb);
    char *buffer = free_membuffer_but_return_trimmed_content(mb);
    if (level != 0 && *x->cur == 0)
    {
        free(name);
        free(buffer);
        x->error = true;
        return NULL;
    }
    XmlXPathResult *res = NULL;
    if (!strcmp(name, "xpointer") || !strcmp(name, "xpath1"))
    {
        // Evaluated with the document as context node, position and size 1.
        XmlXPathSettings st;
        xml_xpath_settings_default(&st);
        st.namespaces = x->ns;
        st.num_namespaces = x->num_ns;
        XmlXPathError err;
        XmlXPathComp *comp = xml_xpath_compile(buffer, &st, &err);
        if (comp)
        {
            res = xml_xpath_eval_comp(ctx, comp, xml_dom_document(x->doc), 1, 1, &err);
            xml_xpath_comp_free(comp);
        }
        if (!res) x->error = true;
    }
    else if (!strcmp(name, "element"))
    {
        XsltXPtr sub = *x;
        sub.cur = buffer;
        if (buffer[0] == '/')
        {
            XmlXPathResult *root = xml_xpath_result_new_nodeset();
            xml_xpath_result_add_node(root, xml_dom_document(x->doc));
            res = xslt_xptr_child_seq(&sub, ctx, NULL, root);
        }
        else
        {
            char *name2 = xslt_xptr_parse_name(&sub);
            if (!name2) x->error = true;
            else res = xslt_xptr_child_seq(&sub, ctx, name2, NULL);
        }
    }
    else if (!strcmp(name, "xmlns"))
    {
        XsltXPtr sub = *x;
        sub.cur = buffer;
        char *prefix = xslt_xptr_parse_name(&sub);
        if (prefix && !strchr(prefix, ':'))
        {
            xslt_xptr_skip_blanks(&sub);
            if (*sub.cur == '=')
            {
                sub.cur++;
                xslt_xptr_skip_blanks(&sub);
                x->ns = (XmlXPathNamespace*)realloc(x->ns, (x->num_ns + 1) * sizeof(XmlXPathNamespace));
                x->ns[x->num_ns].prefix = prefix;
                x->ns[x->num_ns].uri = xslt1_strdup(sub.cur);
                x->num_ns++;
                prefix = NULL;
            }
            else
            {
                x->error = true;
            }
        }
        else
        {
            x->error = true;
        }
        free(prefix);
    }
    // Unknown schemes are skipped (XML_XPTR_UNKNOWN_SCHEME).
    free(buffer);
    free(name);
    return res;
}

/** Port of xmlXPtrEval: the nodes the fragment identifier selects in doc, NULL on errors. */
static XmlXPathResult *xslt_xptr_eval(XmlDom *doc, const char *str)
{
    XsltXPtr x = { doc, str, false, NULL, 0 };
    XmlXPathContext *ctx = xml_xpath_context_new();
    XmlXPathResult *res = NULL;
    xslt_xptr_skip_blanks(&x);
    if (*x.cur == '/')
    {
        XmlXPathResult *root = xml_xpath_result_new_nodeset();
        xml_xpath_result_add_node(root, xml_dom_document(doc));
        res = xslt_xptr_child_seq(&x, ctx, NULL, root);
    }
    else
    {
        char *name = xslt_xptr_parse_name(&x);
        if (!name)
        {
            x.error = true;
        }
        else if (*x.cur == '(')
        {
            // xmlXPtrEvalFullXPtr: the parts until one gives a non empty node set.
            while (name)
            {
                xml_xpath_result_free(res);
                res = xslt_xptr_part(&x, ctx, name);
                if (x.error) break;
                if (res && xml_xpath_result_type(res) == XML_XPATH_NODESET && xml_xpath_result_size(res) > 0) break;
                xml_xpath_result_free(res);
                res = NULL;
                xslt_xptr_skip_blanks(&x);
                name = xslt_xptr_parse_name(&x);
            }
            goto done;
        }
        else
        {
            // Bare names and child sequences.
            res = xslt_xptr_child_seq(&x, ctx, name, NULL);
        }
    }
    xslt_xptr_skip_blanks(&x);
    if (*x.cur != 0) x.error = true;
done:
    if (x.error || (res && xml_xpath_result_type(res) != XML_XPATH_NODESET))
    {
        xml_xpath_result_free(res);
        res = NULL;
    }
    for (size_t i = 0; i < x.num_ns; i++)
    {
        free((char*)x.ns[i].prefix);
        free((char*)x.ns[i].uri);
    }
    free(x.ns);
    xml_xpath_context_free(ctx);
    return res;
}

/** Port of xsltDocumentFunctionLoadDocument: add the nodes of document uri (with fragment) to ret. */
static void xslt_document_function_load_document(XmlLibxslt1Transform *t, XmlXPathResult *ret, const char *uri, const char *fragment)
{
    XsltDocInfo *idoc = xslt_load_document(t, uri);
    XmlDom *doc;
    if (!idoc)
    {
        if (!uri || uri[0] == '#' || (t->style->doc && xslt1_streq(xml_dom_url(t->style->doc), uri)))
        {
            // This selects the stylesheet's document itself (a copy without the precomputed data).
            doc = xslt_copy_doc(t->style->doc);
            idoc = xslt_new_document(t, doc, 1);
        }
        else
        {
            return;
        }
    }
    else
    {
        doc = idoc->doc;
    }
    if (!fragment)
    {
        xml_xpath_result_add_node(ret, xml_dom_document(doc));
        return;
    }
    // The fragment identifier is an XPointer.
    XmlXPathResult *res_obj = xslt_xptr_eval(doc, fragment);
    if (res_obj)
    {
        xslt_nodeset_merge(ret, res_obj);
        xml_xpath_result_free(res_obj);
    }
}

/** xmlXPathNodeSetMerge: add the nodes of from that are not in to. */
static void xslt_nodeset_merge(XmlXPathResult *to, const XmlXPathResult *from)
{
    size_t nf = from ? xml_xpath_result_size(from) : 0;
    for (size_t i = 0; i < nf; i++)
    {
        XMQNode *n = xml_xpath_result_node(from, i);
        size_t nt = xml_xpath_result_size(to);
        bool dup = false;
        for (size_t j = 0; j < nt && !dup; j++) dup = xml_xpath_result_node(to, j) == n;
        if (!dup) xml_xpath_result_add_node(to, n);
    }
}

static bool xslt_is_strict_nodeset(const XmlXPathResult *r)
{
    return r && xml_xpath_result_type(r) == XML_XPATH_NODESET && !xml_xpath_result_is_tree(r);
}

/** document() for one uri string with an optional base node set (obj2). */
static XmlXPathResult *xslt_document_string(XmlLibxslt1Transform *t, const char *url_in, const XmlXPathResult *obj2)
{
    XmlXPathResult *ret = xml_xpath_result_new_nodeset();
    XmlURI *uri = xml_uri_parse(url_in);
    if (!uri)
    {
        xslt1_transform_error(t, NULL, NULL, "document() : failed to parse URI '%s'\n", url_in);
        return ret;
    }
    // Check for and remove the fragment identifier.
    char *fragment = uri->fragment;
    char *new_uri = NULL;
    const char *url = url_in;
    if (fragment)
    {
        uri->fragment = NULL;
        new_uri = xml_uri_save(uri);
        url = new_uri;
    }
    xml_uri_free(uri);
    char *base = NULL;
    if (obj2 && xml_xpath_result_size(obj2) > 0 && lx_is_real_node(xml_xpath_result_node(obj2, 0)))
    {
        XMQNode *target = xml_xpath_result_node(obj2, 0);
        if (xml_node_kind(target) == XML_DOM_ATTRIBUTE || xml_node_kind(target) == XML_DOM_PI) target = xml_node_parent(target);
        base = xslt1_node_get_base(xml_node_dom(target), target);
    }
    else if (t->inst)
    {
        base = xslt1_node_get_base(xml_node_dom(t->inst), t->inst);
    }
    else if (t->style->doc)
    {
        base = xslt1_node_get_base(t->style->doc, xml_dom_document(t->style->doc));
    }
    char *full = xml_uri_build(url, base);
    free(base);
    if (full)
    {
        xslt_document_function_load_document(t, ret, full, fragment);
        free(full);
    }
    free(new_uri);
    free(fragment);
    return ret;
}

/** Port of xsltDocumentFunction: document(). */
static XmlXPathResult *xslt_document_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs < 1 || nargs > 2)
    {
        xslt1_transform_error(t, NULL, NULL, "document() : invalid number of args %d\n", (int)nargs);
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    const XmlXPathResult *obj2 = NULL;
    if (nargs == 2)
    {
        if (!xslt_is_strict_nodeset(args[1]))
        {
            xslt1_transform_error(t, NULL, NULL, "document() : invalid arg expecting a nodeset\n");
            xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_TYPE);
            return NULL;
        }
        obj2 = args[1];
    }
    if (xslt_is_strict_nodeset(args[0]))
    {
        XmlXPathResult *ret = xml_xpath_result_new_nodeset();
        size_t n = xml_xpath_result_size(args[0]);
        for (size_t i = 0; i < n; i++)
        {
            XMQNode *node = xml_xpath_result_node(args[0], i);
            char *s = xml_node_string_value(node);
            XmlXPathResult *base = xml_xpath_result_new_nodeset();
            if (nargs == 2)
            {
                for (size_t j = 0; j < xml_xpath_result_size(obj2); j++) xml_xpath_result_add_node(base, xml_xpath_result_node(obj2, j));
            }
            else
            {
                xml_xpath_result_add_node(base, node);
            }
            XmlXPathResult *newobj = xslt_document_string(t, s, base);
            xslt_nodeset_merge(ret, newobj);
            xml_xpath_result_free(newobj);
            xml_xpath_result_free(base);
            free(s);
        }
        return ret;
    }
    char *s = xml_xpath_result_to_string(args[0]);
    XmlXPathResult *ret = xslt_document_string(t, s, obj2);
    free(s);
    return ret;
}

/** Port of xsltKeyFunction: key(). */
static XmlXPathResult *xslt_key_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 2)
    {
        xslt1_transform_error(t, NULL, NULL, "key() : expects two arguments\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    char *qname = xml_xpath_result_to_string(args[0]);
    XmlXPathResult *ret = xml_xpath_result_new_nodeset();
    const XmlXPathResult *obj2 = args[1];
    char **values = NULL;
    size_t nvalues = 0;
    if (xml_xpath_result_type(obj2) == XML_XPATH_NODESET)
    {
        nvalues = xml_xpath_result_size(obj2);
        values = (char**)calloc(nvalues ? nvalues : 1, sizeof(char*));
        for (size_t i = 0; i < nvalues; i++) values[i] = xml_node_string_value(xml_xpath_result_node(obj2, i));
    }
    else
    {
        nvalues = 1;
        values = (char**)calloc(1, sizeof(char*));
        values[0] = xml_xpath_result_to_string(obj2);
    }
    for (size_t vi = 0; vi < nvalues; vi++)
    {
        XsltDocInfo *old_doc_info = t->document;
        XMQNode *node = xml_xpath_context_node(ctx);
        if (!node)
        {
            xslt1_transform_error(t, NULL, t->inst, "Internal error in xsltKeyFunction(): The context node is not set on the XPath context.\n");
            t->state = XSLT_STATE_STOPPED;
            t->document = old_doc_info;
            continue;
        }
        // The name, with the namespace of a prefix.
        char *prefix = NULL;
        char *key = NULL;
        const char *key_uri = NULL;
        const char *colon = strchr(qname, ':');
        if (colon && colon != qname && colon[1])
        {
            prefix = xslt1_strndup(qname, (size_t)(colon - qname));
            key = xslt1_strdup(colon + 1);
            key_uri = xml_xpath_context_lookup_namespace(ctx, prefix);
            if (!key_uri) xslt1_transform_error(t, NULL, t->inst, "key() : prefix %s is not bound\n", prefix);
        }
        else
        {
            key = xslt1_strdup(qname);
        }
        XMQNode *tmp_node = node;
        if (xml_node_kind(node) == XML_DOM_NAMESPACE) tmp_node = xml_node_parent(node);
        XmlXPathResult *nodelist = NULL;
        if (!tmp_node || !xml_node_dom(tmp_node))
        {
            xslt1_transform_error(t, NULL, t->inst, "Internal error in xsltKeyFunction(): Couldn't get the doc of the XPath context node.\n");
        }
        else
        {
            XmlDom *doc = xml_node_dom(tmp_node);
            bool ok = true;
            if (!t->document || t->document->doc != doc)
            {
                XsltRVT *rvt = xslt_rvt_of(doc);
                if (rvt)
                {
                    // A result tree fragment.
                    if (!rvt->keys) rvt->keys = xslt_new_document(t, doc, 0);
                    t->document = rvt->keys;
                }
                else
                {
                    // The source document or one from document().
                    t->document = xslt_find_document(t, doc);
                }
                if (!t->document)
                {
                    xslt1_transform_error(t, NULL, t->inst, "Internal error in xsltKeyFunction(): Could not get the document info of a context doc.\n");
                    t->state = XSLT_STATE_STOPPED;
                    ok = false;
                }
            }
            if (ok) nodelist = xslt1_get_key(t, key, key_uri, values[vi]);
        }
        t->document = old_doc_info;
        if (nodelist) xslt_nodeset_merge(ret, nodelist);
        free(prefix);
        free(key);
    }
    for (size_t i = 0; i < nvalues; i++) free(values[i]);
    free(values);
    free(qname);
    return ret;
}

/** Port of xsltUnparsedEntityURIFunction: unparsed-entity-uri(). */
static XmlXPathResult *xslt_unparsed_entity_uri_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 1)
    {
        xslt1_generic_error(&t->style->settings, "unparsed-entity-uri() : expects one string arg\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    char *str = xml_xpath_result_to_string(args[0]);
    XMQNode *node = xml_xpath_context_node(ctx);
    XmlDom *doc = node ? xml_node_dom(node) : NULL;
    XMQNode *entity = (doc && str) ? xml_dom_get_entity(doc, str) : NULL;
    XmlXPathResult *ret;
    if (entity && xml_entity_decl_system_id(entity))
    {
        // libxml2 entity->URI: the system id resolved against the document.
        char *uri = xml_uri_build(xml_entity_decl_system_id(entity), xml_dom_url(doc));
        ret = xml_xpath_result_new_string(uri ? uri : xml_entity_decl_system_id(entity));
        free(uri);
    }
    else
    {
        ret = xml_xpath_result_new_string("");
    }
    free(str);
    return ret;
}

/** Port of xsltGenerateIdFunction: generate-id() gives id1, id2, ... in the order of the calls. */
static XmlXPathResult *xslt_generate_id_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    XMQNode *cur = NULL;
    if (nargs == 0)
    {
        cur = xml_xpath_context_node(ctx);
    }
    else if (nargs == 1)
    {
        if (!xslt_is_strict_nodeset(args[0]))
        {
            xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_TYPE);
            xslt1_transform_error(t, NULL, NULL, "generate-id() : invalid arg expecting a node-set\n");
            return NULL;
        }
        size_t n = xml_xpath_result_size(args[0]);
        if (n == 0) return xml_xpath_result_new_string("");
        // The first node in document order.
        cur = xml_xpath_result_node(args[0], 0);
        for (size_t i = 1; i < n; i++)
        {
            XMQNode *o = xml_xpath_result_node(args[0], i);
            if (xml_node_compare_order(o, cur) < 0) cur = o;
        }
    }
    else
    {
        xslt1_transform_error(t, NULL, NULL, "generate-id() : invalid number of args %d\n", (int)nargs);
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    const char *ns_prefix = NULL;
    bool is_ns = false;
    if (xml_node_kind(cur) == XML_DOM_NAMESPACE)
    {
        is_ns = true;
        ns_prefix = xml_node_name(cur) ? xml_node_name(cur) : "";
        cur = xml_node_parent(cur);
    }
    if (!t->generated_ids) t->generated_ids = hashmap_create(1024);
    char key[32];
    snprintf(key, sizeof(key), "%p", (void*)cur);
    unsigned long id = (unsigned long)(size_t)hashmap_get(t->generated_ids, key);
    if (!id)
    {
        id = ++t->current_id;
        hashmap_put(t->generated_ids, key, (void*)(size_t)id);
    }
    MemBuffer *mb = new_membuffer();
    if (!is_ns)
    {
        membuffer_printf(mb, "id%lu", id);
    }
    else
    {
        // Only ascii alphanumerics are allowed, the prefix is hex encoded.
        membuffer_printf(mb, "id%luns", id);
        for (const unsigned char *p = (const unsigned char*)ns_prefix; *p; p++)
        {
            int v = *p >> 4;
            membuffer_append_char(mb, (char)(v < 10 ? '0' + v : 'A' + (v - 10)));
            v = *p & 15;
            membuffer_append_char(mb, (char)(v < 10 ? '0' + v : 'A' + (v - 10)));
        }
    }
    membuffer_append_null(mb);
    char *str = free_membuffer_but_return_trimmed_content(mb);
    XmlXPathResult *ret = xml_xpath_result_new_string(str);
    free(str);
    return ret;
}

/** Split a QName like xmlSplitQName2: NULL if there is no (valid) prefix. */
static char *xslt_split_qname2(const char *qname, char **prefix)
{
    *prefix = NULL;
    if (!qname || qname[0] == ':') return NULL;
    const char *colon = strchr(qname, ':');
    if (!colon || !colon[1]) return NULL;
    *prefix = xslt1_strndup(qname, (size_t)(colon - qname));
    return xslt1_strdup(colon + 1);
}

/** Port of xsltSystemPropertyFunction: system-property(). */
static XmlXPathResult *xslt_system_property_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 1)
    {
        xslt1_transform_error(t, NULL, NULL, "system-property() : expects one string arg\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    if (xml_xpath_result_type(args[0]) != XML_XPATH_STRING)
    {
        xslt1_transform_error(t, NULL, NULL, "system-property() : invalid arg expecting a string\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_TYPE);
        return NULL;
    }
    const char *s = xml_xpath_result_string(args[0]);
    char *prefix = NULL;
    const char *ns_uri = NULL;
    char *name = xslt_split_qname2(s, &prefix);
    if (!name)
    {
        name = xslt1_strdup(s);
    }
    else
    {
        ns_uri = xml_xpath_context_lookup_namespace(ctx, prefix);
        if (!ns_uri) xslt1_transform_error(t, NULL, NULL, "system-property() : prefix %s is not bound\n", prefix);
    }
    const char *value = "";
    if (xslt1_streq(ns_uri, XSLT_NAMESPACE))
    {
        if (!strcmp(name, "vendor")) value = "libxslt";
        else if (!strcmp(name, "version")) value = "1.0";
        else if (!strcmp(name, "vendor-url")) value = "http://xmlsoft.org/XSLT/";
    }
    free(name);
    free(prefix);
    return xml_xpath_result_new_string(value);
}

/** The extension elements libxslt knows (xsltExtElementLookup with the elements of extra.c). */
static bool xslt_ext_element_available(const char *name, const char *uri)
{
    if (!name || !uri) return false;
    // The xslt elements registered by xsltRegisterAllElement.
    static const char *xslt_elements[] = { "apply-templates", "apply-imports", "call-template", "element", "attribute",
        "text", "processing-instruction", "comment", "copy", "value-of", "number", "for-each", "if", "choose", "sort",
        "copy-of", "message", "variable", "param", "with-param", "decimal-format", "when", "otherwise", "fallback", NULL };
    if (!strcmp(uri, XSLT_NAMESPACE))
    {
        for (int i = 0; xslt_elements[i]; i++) if (!strcmp(name, xslt_elements[i])) return true;
    }
    if (!strcmp(uri, XSLT_LIBXSLT_NAMESPACE) && !strcmp(name, "debug")) return true;
    if (!strcmp(uri, XSLT_SAXON_NAMESPACE) && !strcmp(name, "output")) return true;
    if (!strcmp(uri, XSLT_XALAN_NAMESPACE) && !strcmp(name, "write")) return true;
    if (!strcmp(uri, XSLT_XT_NAMESPACE) && !strcmp(name, "document")) return true;
    if (!strcmp(uri, XSLT_NAMESPACE) && !strcmp(name, "document")) return true;
    return false;
}

/** Port of xsltElementAvailableFunction: element-available(). */
static XmlXPathResult *xslt_element_available_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 1)
    {
        xslt1_transform_error(t, NULL, NULL, "element-available() : expects one string arg\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    char *s = xml_xpath_result_to_string(args[0]);
    if (!t->inst)
    {
        xslt1_transform_error(t, NULL, NULL, "element-available() : internal error tctxt == NULL\n");
        free(s);
        return xml_xpath_result_new_boolean(false);
    }
    char *prefix = NULL;
    const char *ns_uri = NULL;
    char *name = xslt_split_qname2(s, &prefix);
    if (!name)
    {
        name = xslt1_strdup(s);
        XmlNs *ns = lx_search_ns(t->inst, NULL);
        if (ns) ns_uri = xml_ns_href(ns);
    }
    else
    {
        ns_uri = xml_xpath_context_lookup_namespace(ctx, prefix);
        if (!ns_uri) xslt1_transform_error(t, NULL, NULL, "element-available() : prefix %s is not bound\n", prefix);
    }
    bool r = xslt_ext_element_available(name, ns_uri);
    free(name);
    free(prefix);
    free(s);
    return xml_xpath_result_new_boolean(r);
}

static XmlXPathFunction xslt_function_lookup(void *user_data, const char *name, const char *ns_uri, void **func_data);

/** Is the function a core XPath 1.0 function of libxml2? */
static bool xslt_is_core_function(const char *name, const char *ns_uri)
{
    static const char *core[] = { "boolean", "ceiling", "count", "concat", "contains", "id", "false", "floor", "last",
        "lang", "local-name", "not", "name", "namespace-uri", "normalize-space", "number", "position", "round", "string",
        "string-length", "starts-with", "substring", "substring-before", "substring-after", "sum", "true", "translate", NULL };
    if (ns_uri) return !strcmp(ns_uri, "http://www.w3.org/2002/08/xquery-functions") && !strcmp(name, "escape-uri");
    for (int i = 0; core[i]; i++) if (!strcmp(core[i], name)) return true;
    return false;
}

/** Port of xsltFunctionAvailableFunction: function-available(). */
static XmlXPathResult *xslt_function_available_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 1)
    {
        xslt1_transform_error(t, NULL, NULL, "function-available() : expects one string arg\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    char *s = xml_xpath_result_to_string(args[0]);
    char *prefix = NULL;
    const char *ns_uri = NULL;
    char *name = xslt_split_qname2(s, &prefix);
    if (!name)
    {
        name = xslt1_strdup(s);
    }
    else
    {
        ns_uri = xml_xpath_context_lookup_namespace(ctx, prefix);
        if (!ns_uri) xslt1_transform_error(t, NULL, NULL, "function-available() : prefix %s is not bound\n", prefix);
    }
    void *fd;
    bool r = xslt_is_core_function(name, ns_uri) || xslt_function_lookup(t, name, ns_uri, &fd) != NULL;
    free(name);
    free(prefix);
    free(s);
    return xml_xpath_result_new_boolean(r);
}

/** Port of xsltCurrentFunction: current(). */
static XmlXPathResult *xslt_current_function(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    (void)args;
    if (nargs != 0)
    {
        xslt1_transform_error(t, NULL, NULL, "current() : function uses no argument\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    XmlXPathResult *r = xml_xpath_result_new_nodeset();
    if (t->node) xml_xpath_result_add_node(r, t->node);
    return r;
}

/** Port of xsltFunctionNodeSet (extra.c): node-set() in the libxslt, saxon and xt namespaces. */
static XmlXPathResult *xslt_function_node_set(XmlXPathContext *ctx, void *func_data, XmlXPathResult **args, size_t nargs)
{
    XmlLibxslt1Transform *t = xslt_tctxt(func_data);
    if (nargs != 1)
    {
        xslt1_transform_error(t, NULL, NULL, "node-set() : expects one result-tree arg\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_ARITY);
        return NULL;
    }
    if (xml_xpath_result_type(args[0]) != XML_XPATH_NODESET)
    {
        xslt1_transform_error(t, NULL, NULL, "node-set() invalid arg expecting a result tree\n");
        xml_xpath_context_set_error_quiet(ctx, XML_XPATH_ERR_INVALID_TYPE);
        return NULL;
    }
    XmlXPathResult *r = xml_xpath_result_copy(args[0]);
    xml_xpath_result_set_tree(r, false);
    return r;
}


/** The function lookup of the xpath context: the XSLT functions (xsltRegisterAllFunctions) and
    the extension functions libxslt registers by default (extra.c). */
static XmlXPathFunction xslt_function_lookup(void *user_data, const char *name, const char *ns_uri, void **func_data)
{
    *func_data = user_data;
    if (!ns_uri)
    {
        if (!strcmp(name, "current")) return xslt_current_function;
        if (!strcmp(name, "document")) return xslt_document_function;
        if (!strcmp(name, "key")) return xslt_key_function;
        if (!strcmp(name, "unparsed-entity-uri")) return xslt_unparsed_entity_uri_function;
        if (!strcmp(name, "format-number")) return xslt_format_number_function;
        if (!strcmp(name, "generate-id")) return xslt_generate_id_function;
        if (!strcmp(name, "system-property")) return xslt_system_property_function;
        if (!strcmp(name, "element-available")) return xslt_element_available_function;
        if (!strcmp(name, "function-available")) return xslt_function_available_function;
        return NULL;
    }
    if (!strcmp(name, "node-set") &&
        (!strcmp(ns_uri, XSLT_LIBXSLT_NAMESPACE) || !strcmp(ns_uri, XSLT_SAXON_NAMESPACE) || !strcmp(ns_uri, XSLT_XT_NAMESPACE)))
    {
        return xslt_function_node_set;
    }
    return NULL;
}

// The transformation context ////////////////////////////////////////

XmlLibxslt1Transform *xml_libxslt1_transform_new(XmlLibxslt1Stylesheet *style, XmlDom *source)
{
    XmlLibxslt1Transform *t = (XmlLibxslt1Transform*)calloc(1, sizeof(XmlLibxslt1Transform));
    t->style = style;
    t->source = source;
    t->xpath = xml_xpath_context_new();
    // Like a fresh libxml2 xpath context, the position and size are unknown.
    t->xp_position = -1;
    t->xp_size = -1;
    t->extras_max = style->extras_nr + 20;
    t->extras = (XsltRuntimeExtra*)calloc((size_t)t->extras_max, sizeof(XsltRuntimeExtra));
    t->extras_nr = style->extras_nr;
    t->max_template_depth = XSLT_MAX_DEPTH;
    t->max_template_vars = XSLT_MAX_VARS;
    t->op_limit = style->op_limit;
    xml_xpath_context_set_variable_lookup(t->xpath, xslt_xpath_variable_lookup, t);
    xml_xpath_context_set_function_lookup(t->xpath, xslt_function_lookup, t);
    XsltDocInfo *docu = (XsltDocInfo*)calloc(1, sizeof(XsltDocInfo));
    docu->doc = source;
    docu->main = 1;
    t->doc_list = docu;
    t->document = docu;
    return t;
}

void xml_libxslt1_transform_free(XmlLibxslt1Transform *t)
{
    if (!t) return;
    for (int i = 0; i < t->extras_nr; ++i)
    {
        if (t->extras[i].deallocate && t->extras[i].info) t->extras[i].deallocate(t->extras[i].info);
    }
    free(t->extras);
    if (t->global_vars)
    {
        HashMapIterator *it = hashmap_iterate(t->global_vars);
        const char *key;
        void *val;
        while (hashmap_next_key_value(it, &key, &val)) xslt_free_runtime_stack_elem(t, (XsltStackElem*)val);
        hashmap_free_iterator(it);
        hashmap_free(t->global_vars);
    }
    xslt_free_rvts(t);
    XsltDocInfo *d = t->doc_list;
    while (d)
    {
        XsltDocInfo *next = d->next;
        xslt_free_doc_keys(d);
        if (d->owned)
        {
            xml_xpath_context_forget(t->xpath, xml_dom_document(d->doc));
            xml_dom_free(d->doc);
        }
        free(d);
        d = next;
    }
    xml_xpath_context_free(t->xpath);
    if (t->keyed_nodes)
    {
        free(((XsltNodeFlags*)t->keyed_nodes)->slots);
        free(t->keyed_nodes);
    }
    if (t->generated_ids) hashmap_free(t->generated_ids);
    free(t->templ_tab);
    free(t->vars_tab);
    free(t);
}

/** Port of xsltFindElemSpaceHandling: are blank text children of node stripped? */
static int xslt_find_elem_space_handling(XmlLibxslt1Transform *t, XMQNode *node)
{
    if (!node) return 0;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        const char *val = NULL;
        if (style->strip_spaces && xml_node_kind(node) == XML_DOM_ELEMENT)
        {
            XmlNs *ns = xml_node_ns(node);
            if (ns)
            {
                char *key = xslt1_key2(xml_node_name(node), xml_ns_href(ns));
                val = (const char*)hashmap_get(style->strip_spaces, key);
                free(key);
                if (!val)
                {
                    key = xslt1_key2("*", xml_ns_href(ns));
                    val = (const char*)hashmap_get(style->strip_spaces, key);
                    free(key);
                }
            }
            else
            {
                char *key = xslt1_key2(xml_node_name(node), NULL);
                val = (const char*)hashmap_get(style->strip_spaces, key);
                free(key);
            }
        }
        if (val)
        {
            if (!strcmp(val, "strip")) return 1;
            if (!strcmp(val, "preserve")) return 0;
        }
        if (style->strip_all == 1) return 1;
        if (style->strip_all == -1) return 0;
    }
    return 0;
}

/** Port of xsltNeedElemSpaceHandling. */
static int xslt_need_elem_space_handling(XmlLibxslt1Transform *t)
{
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        if (style->strip_spaces) return 1;
    }
    return 0;
}

/** Port of xsltApplyStripSpaces: strip the blank text nodes of the source document. */
static void xslt_apply_strip_spaces(XmlLibxslt1Transform *t, XMQNode *node)
{
    XMQNode *current = node;
    while (current)
    {
        if (lx_is_real_node(current) && lx_has_children(current) && xslt_find_elem_space_handling(t, current))
        {
            XMQNode *del = NULL, *cur = lx_children(current);
            while (cur)
            {
                if (lx_is_blank_node(cur)) del = cur;
                cur = xml_node_next(cur);
                if (del)
                {
                    xml_node_free(del);
                    del = NULL;
                }
            }
        }
        if (lx_has_children(current) && xml_node_kind(current) != XML_DOM_ENTITY_REF)
        {
            current = lx_children(current);
        }
        else if (xml_node_next(current))
        {
            current = xml_node_next(current);
        }
        else
        {
            do
            {
                current = xml_node_parent(current);
                if (!current) break;
                if (current == node) return;
                if (xml_node_next(current))
                {
                    current = xml_node_next(current);
                    break;
                }
            } while (current);
        }
    }
}

/** Port of xsltCountKeys. */
static int xslt_count_keys(XmlLibxslt1Transform *t)
{
    t->has_templ_key_patterns = 0;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        if (style->key_match)
        {
            t->has_templ_key_patterns = 1;
            break;
        }
    }
    t->nb_keys = 0;
    for (XmlLibxslt1Stylesheet *style = t->style; style; style = xslt1_next_import(style))
    {
        for (XsltKeyDef *keyd = style->keys; keyd; keyd = keyd->next) t->nb_keys++;
    }
    return t->nb_keys;
}

static const char *xslt_import_method(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->method) return s->method; return NULL; }
static const char *xslt_import_doctype_public(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->doctype_public) return s->doctype_public; return NULL; }
static const char *xslt_import_doctype_system(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->doctype_system) return s->doctype_system; return NULL; }
static const char *xslt_import_version(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->version) return s->version; return NULL; }
static const char *xslt_import_encoding(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->encoding) return s->encoding; return NULL; }

/** Port of xsltGetHTMLIDs. */
static int xslt_get_html_ids(const char *version, const char **public_id, const char **system_id)
{
    static const struct { const char *version; const char *public_id; const char *system_id; } versions[] = {
        { "5", NULL, "about:legacy-compat" },
        { "4.01frame", "-//W3C//DTD HTML 4.01 Frameset//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/frameset.dtd" },
        { "4.01strict", "-//W3C//DTD HTML 4.01//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/strict.dtd" },
        { "4.01trans", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd" },
        { "4.01", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd" },
        { "4.0strict", "-//W3C//DTD HTML 4.01//EN", "http://www.w3.org/TR/html4/strict.dtd" },
        { "4.0trans", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/html4/loose.dtd" },
        { "4.0frame", "-//W3C//DTD HTML 4.01 Frameset//EN", "http://www.w3.org/TR/html4/frameset.dtd" },
        { "4.0", "-//W3C//DTD HTML 4.01 Transitional//EN", "http://www.w3.org/TR/html4/loose.dtd" },
        { "3.2", "-//W3C//DTD HTML 3.2//EN", NULL }
    };
    if (!version) return -1;
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++)
    {
        if (!strcasecmp(version, versions[i].version))
        {
            if (public_id) *public_id = versions[i].public_id;
            if (system_id) *system_id = versions[i].system_id;
            return 0;
        }
    }
    return -1;
}

/** htmlNewDoc: an html document with a doctype (the html 4.0 transitional one if none is given). */
static XmlDom *xslt_html_new_doc(const char *system_id, const char *public_id)
{
    XmlDom *res = xml_dom_new();
    if (!system_id && !public_id)
    {
        system_id = "http://www.w3.org/TR/REC-html40/loose.dtd";
        public_id = "-//W3C//DTD HTML 4.0 Transitional//EN";
    }
    xml_dom_set_dtd(res, "html", public_id, system_id);
    return res;
}

static bool xslt_is_blank_text_node(XMQNode *n)
{
    return xml_node_kind(n) == XML_DOM_TEXT && xslt1_is_blank_str(xml_node_content(n));
}

/** Put the dtd first among the children of the document (libxslt's "hack" before comments). */
static void xslt_set_dtd_first(XmlDom *res, const char *name, const char *public_id, const char *system_id)
{
    XMQNode *dtd = xml_dom_set_dtd(res, name, public_id, system_id);
    XMQNode *doc = xml_dom_document(res);
    XMQNode *first = xml_node_first_child(doc);
    if (dtd && first && first != dtd)
    {
        xml_node_unlink(dtd);
        xml_node_add_prev_sibling(first, dtd);
    }
}

/** Port of xsltProcessUserParamInternal (with eval): a parameter given to the transformation. */
static int xslt_process_user_param(XmlLibxslt1Transform *t, const char *qname, const char *value)
{
    XmlLibxslt1Stylesheet *style = t->style;
    if (!qname || !value) return 0;
    char *name = NULL, *href = NULL;
    if (qname[0] == '{')
    {
        size_t len = 0;
        while (qname[len] && qname[len] != '}') len++;
        if (!qname[len])
        {
            xslt1_transform_error(t, style, NULL, "user param : malformed parameter name : %s\n", qname);
            name = xslt1_strdup(qname);
        }
        else
        {
            href = xslt1_strndup(qname + 1, len - 1);
            name = xslt1_strdup(qname + len + 1);
        }
    }
    else
    {
        char *prefix = NULL;
        name = xslt1_split_qname(qname, &prefix);
        if (prefix)
        {
            XmlNs *ns = lx_search_ns(xml_dom_root_element(style->doc), prefix);
            if (!ns) xslt1_transform_error(t, style, NULL, "user param : no namespace bound to prefix %s\n", prefix);
            else href = xslt1_strdup(xml_ns_href(ns));
            free(prefix);
        }
    }
    if (!name)
    {
        free(href);
        return -1;
    }
    char *key = xslt1_key2(name, href);
    if (hashmap_get(t->global_vars, key)) xslt1_transform_error(t, style, NULL, "Global parameter %s already defined\n", name);
    // Like libxslt, only the variables of the top stylesheet are looked at (for every import).
    for (XmlLibxslt1Stylesheet *st = style; st; st = xslt1_next_import(st))
    {
        for (XsltStackElem *elem = t->style->variables; elem; elem = elem->next)
        {
            if (elem->comp && elem->comp->type == XSLT_FUNC_VARIABLE && xslt1_streq(elem->name, name) && xslt1_streq(elem->name_uri, href))
            {
                free(key);
                free(name);
                free(href);
                return 0;
            }
        }
    }
    XmlXPathResult *result = NULL;
    XmlXPathComp *xp_expr = xslt1_xpath_compile(&style->settings, NULL, value, false);
    if (xp_expr)
    {
        long pos = t->xp_position, size = t->xp_size;
        t->xp_size = 1;
        t->xp_position = 1;
        result = xslt_xpath_eval(t, xp_expr, t->initial_context_node);
        t->xp_position = pos;
        t->xp_size = size;
        xml_xpath_comp_free(xp_expr);
    }
    if (!result)
    {
        xslt1_transform_error(t, style, NULL, "Evaluating user parameter %s failed\n", name);
        t->state = XSLT_STATE_STOPPED;
        free(key);
        free(name);
        free(href);
        return -1;
    }
    XsltStackElem *elem = xslt_new_stack_elem();
    elem->owns_strings = true;
    elem->name = name;
    elem->select = xslt1_strdup(value);
    elem->name_uri = href;
    elem->computed = 1;
    elem->value = result;
    if (hashmap_get(t->global_vars, key))
    {
        xslt1_transform_error(t, style, NULL, "Global parameter %s already defined\n", name);
        xslt_free_runtime_stack_elem(t, elem);
    }
    else
    {
        hashmap_put(t->global_vars, key, elem);
    }
    free(key);
    return 0;
}

/** Port of xsltEvalUserParams. */
static int xslt_eval_user_params(XmlLibxslt1Transform *t, const char **params)
{
    if (!params) return 0;
    for (size_t i = 0; params[i]; i += 2)
    {
        if (xslt_process_user_param(t, params[i], params[i + 1]) != 0) return -1;
    }
    return 0;
}

// Output, port of libxslt xsltutils.c xsltSaveResultTo ///////////////

static int xslt_import_int_indent(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->indent != -1) return s->indent; return -1; }
static int xslt_import_int_omit(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->omit_xml_declaration != -1) return s->omit_xml_declaration; return -1; }
static int xslt_import_int_standalone(XmlLibxslt1Stylesheet *s) { for (; s; s = xslt1_next_import(s)) if (s->standalone != -1) return s->standalone; return -1; }

/** Port of xsltSaveResultTo: serialize the result in UTF-8 (the encoding is applied by the caller). */
static bool xslt_save_result_to(MemBuffer *buf, XmlDom *result, XmlLibxslt1Stylesheet *style, int type)
{
    if (!buf || !result || !style) return false;
    XMQNode *doc_node = xml_dom_document(result);
    XMQNode *first = xml_node_first_child(doc_node);
    if (!first || (xml_node_kind(first) == XML_DOM_DTD && !xml_node_next(first))) return true;
    if (style->method_uri && (!style->method || strcmp(style->method, "xhtml")))
    {
        xslt1_generic_error(&style->settings, "xsltSaveResultTo : unknown output method\n");
        return false;
    }
    const char *method = xslt_import_method(style);
    const char *encoding = xslt_import_encoding(style);
    int indent = xslt_import_int_indent(style);
    if (!method && type == XSLT_OUTPUT_HTML) method = "html";
#ifdef XMQ_NO_HTML
    if (method && (!strcmp(method, "html") || !strcmp(method, "xhtml")))
    {
        xslt1_generic_error(&style->settings, "xmq: the html output method was excluded when xmq was built (XMQ_NO_HTML)\n");
        return false;
    }
#else
    if (method && !strcmp(method, "html"))
    {
        xml_html_set_meta_encoding(result, encoding ? encoding : "UTF-8");
        if (indent == -1) indent = 1;
        xml_html_save_doc_to(buf, result, indent != 0);
    }
    else if (method && !strcmp(method, "xhtml"))
    {
        xml_html_set_meta_encoding(result, encoding ? encoding : "UTF-8");
        xml_html_save_doc_to(buf, result, true);
    }
#endif
    else if (method && !strcmp(method, "text"))
    {
        XMQNode *cur = first;
        while (cur)
        {
            if (xml_node_kind(cur) == XML_DOM_TEXT && xml_node_content(cur)) membuffer_append(buf, xml_node_content(cur));
            XMQNode *child = lx_children(cur);
            if (child && xml_node_kind(child) != XML_DOM_ENTITY_DECL && xml_node_kind(child) != XML_DOM_ENTITY_REF)
            {
                cur = child;
                continue;
            }
            if (xml_node_next(cur))
            {
                cur = xml_node_next(cur);
                continue;
            }
            do
            {
                cur = xml_node_parent(cur);
                if (!cur || cur == doc_node)
                {
                    cur = NULL;
                    break;
                }
                if (xml_node_next(cur))
                {
                    cur = xml_node_next(cur);
                    break;
                }
            } while (cur);
        }
    }
    else
    {
        int omit_xml_decl = xslt_import_int_omit(style);
        int standalone = xslt_import_int_standalone(style);
        if (omit_xml_decl != 1)
        {
            membuffer_append(buf, "<?xml version=");
            const char *version = xml_dom_version(result);
            membuffer_append(buf, "\"");
            membuffer_append(buf, version ? version : "1.0");
            membuffer_append(buf, "\"");
            if (!encoding) encoding = xml_dom_encoding(result);
            if (encoding)
            {
                membuffer_append(buf, " encoding=\"");
                membuffer_append(buf, encoding);
                membuffer_append(buf, "\"");
            }
            if (standalone == 0) membuffer_append(buf, " standalone=\"no\"");
            else if (standalone == 1) membuffer_append(buf, " standalone=\"yes\"");
            membuffer_append(buf, "?>\n");
        }
        for (XMQNode *child = first; child; child = xml_node_next(child))
        {
            xml_save_node_dump_output(buf, result, child, 0, indent == 1, encoding);
            if (indent && (xml_node_kind(child) == XML_DOM_DTD || (xml_node_kind(child) == XML_DOM_COMMENT && xml_node_next(child))))
                membuffer_append(buf, "\n");
        }
        if (indent) membuffer_append(buf, "\n");
    }
    return true;
}

char *xml_libxslt1_save_result_to_string(XmlDom *result, XmlLibxslt1Stylesheet *style, int output_type, size_t *len)
{
    MemBuffer *utf8 = new_membuffer();
    if (result && xml_node_first_child(xml_dom_document(result))) xslt_save_result_to(utf8, result, style, output_type);
    const char *encoding = xslt_import_encoding(style);
    size_t n = utf8->used_;
    MemBuffer *out = utf8;
    // Like libxslt, no encoder for UTF-8; an encoding without an encoder is written as UTF-8.
    if (encoding && strcasecmp(encoding, "UTF-8") && strcasecmp(encoding, "UTF8") && xml_save_encoding_supported(encoding))
    {
        membuffer_append_null(utf8);
        char *u = free_membuffer_but_return_trimmed_content(utf8);
        out = new_membuffer();
        xml_save_transcode(out, u, n, encoding);
        free(u);
    }
    if (len) *len = out->used_;
    membuffer_append_null(out);
    return free_membuffer_but_return_trimmed_content(out);
}

/** Port of xsltApplyStylesheetInternal. */
XmlDom *xml_libxslt1_apply_stylesheet(XmlLibxslt1Stylesheet *style, XmlDom *doc, const char **params, int *output_type)
{
    if (!style || !doc) return NULL;
    XmlDom *res = NULL;
    XmlLibxslt1Transform *t = xml_libxslt1_transform_new(style, doc);
    t->params = params;
    t->initial_context_doc = doc;
    t->initial_context_node = xml_dom_document(doc);
    const char *method = xslt_import_method(style);
    const char *doctype_public = xslt_import_doctype_public(style);
    const char *doctype_system = xslt_import_doctype_system(style);
    const char *version = xslt_import_version(style);
    const char *encoding = xslt_import_encoding(style);
    if (method && strcmp(method, "xml"))
    {
        if (!strcmp(method, "html"))
        {
            t->type = XSLT_OUTPUT_HTML;
            if (doctype_public || doctype_system)
            {
                res = xslt_html_new_doc(doctype_system, doctype_public);
            }
            else if (!version)
            {
                // htmlNewDoc then the dtd is removed.
                res = xml_dom_new();
            }
            else
            {
                xslt_get_html_ids(version, &doctype_public, &doctype_system);
                res = xslt_html_new_doc(doctype_system, doctype_public);
            }
        }
        else if (!strcmp(method, "xhtml"))
        {
            xslt1_transform_error(t, NULL, xml_dom_document(doc), "xsltApplyStylesheetInternal: unsupported method xhtml, using html\n");
            t->type = XSLT_OUTPUT_HTML;
            res = xslt_html_new_doc(doctype_system, doctype_public);
        }
        else if (!strcmp(method, "text"))
        {
            t->type = XSLT_OUTPUT_TEXT;
            res = xml_dom_new();
            xml_dom_set_version(res, style->version ? style->version : "1.0");
        }
        else
        {
            xslt1_transform_error(t, NULL, xml_dom_document(doc), "xsltApplyStylesheetInternal: unsupported method (%s)\n", method);
            goto error;
        }
    }
    else
    {
        t->type = XSLT_OUTPUT_XML;
        res = xml_dom_new();
        xml_dom_set_version(res, style->version ? style->version : "1.0");
    }
    if (encoding) xml_dom_set_encoding(res, encoding);
    t->node = xml_dom_document(doc);
    t->output = res;
    t->xp_size = 1;
    t->xp_position = 1;
    if (xslt_need_elem_space_handling(t)) xslt_apply_strip_spaces(t, xml_dom_root_element(doc));
    t->global_vars = hashmap_create(64);
    if (params) xslt_eval_user_params(t, params);
    xslt_count_keys(t);
    xslt_eval_global_variables(t);
    // Clean up any unused fragments.
    if (t->local_rvt) xslt_release_local_rvts(t, NULL);
    t->insert = xml_dom_document(res);
    t->vars_base = t->vars_nr - 1;
    xslt_process_one_node(t, t->node, NULL);
    xslt_local_variable_pop(t, 0, -2);

    XMQNode *root;
    root = xml_dom_root_element(res);
    if (root)
    {
        char *doctype = NULL;
        XmlNs *rns = xml_node_ns(root);
        if (rns && xml_ns_prefix(rns))
        {
            doctype = (char*)malloc(strlen(xml_ns_prefix(rns)) + strlen(xml_node_name(root)) + 2);
            sprintf(doctype, "%s:%s", xml_ns_prefix(rns), xml_node_name(root));
        }
        else
        {
            doctype = xslt1_strdup(xml_node_name(root));
        }
        // Apply the default html output method when the root element is html.
        if (!method && !rns && !strcasecmp(xml_node_name(root), "html"))
        {
            XMQNode *tmp = xml_node_first_child(xml_dom_document(res));
            while (tmp && tmp != root)
            {
                if (xml_node_kind(tmp) == XML_DOM_ELEMENT) break;
                if (xml_node_kind(tmp) == XML_DOM_TEXT && !xslt_is_blank_text_node(tmp)) break;
                tmp = xml_node_next(tmp);
            }
            if (tmp == root)
            {
                t->type = XSLT_OUTPUT_HTML;
                if (doctype_public || doctype_system)
                {
                    xml_dom_set_dtd(res, doctype, doctype_public, doctype_system);
                }
                else if (version)
                {
                    xslt_get_html_ids(version, &doctype_public, &doctype_system);
                    if (doctype_public || doctype_system) xml_dom_set_dtd(res, doctype, doctype_public, doctype_system);
                }
            }
        }
        if (t->type == XSLT_OUTPUT_XML)
        {
            doctype_public = xslt_import_doctype_public(style);
            doctype_system = xslt_import_doctype_system(style);
            if (doctype_public || doctype_system) xslt_set_dtd_first(res, doctype, doctype_public, doctype_system);
        }
        free(doctype);
    }
    if (t->state != XSLT_STATE_OK)
    {
        xml_dom_free(res);
        res = NULL;
    }
    if (output_type) *output_type = t->type;
    xml_libxslt1_transform_free(t);
    return res;
error:
    if (res) xml_dom_free(res);
    xml_libxslt1_transform_free(t);
    return NULL;
}

#endif // XML_LIBXSLT1_TRANSFORM_MODULE
