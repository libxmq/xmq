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

#include"xml_xslt.h"
#include"xml_dom.h"
#include"xml_xpath.h"
#include"xml_chars.h"
#include"hashmap.h"
#include"membuffer.h"

#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_XSLT_MODULE

// Types //////////////////////////////////////////////////////////

// The priority of a template without a priority attribute (XSLT_PAT_NO_PRIORITY).
#define XSLT_PAT_NO_PRIORITY -12345789.0f

typedef enum
{
    XSLT_STATE_OK = 0,
    XSLT_STATE_ERROR,
    XSLT_STATE_STOPPED
} XsltState;

typedef struct XsltTemplate XsltTemplate;

/** A template (libxslt xsltTemplate), the parts used by the patterns so far. */
struct XsltTemplate
{
    XsltTemplate *next;     // The next template in the stylesheet.
    char *match;
    float priority;
    char *name;
    char *name_uri;
    char *mode;
    char *mode_uri;
    XMQNode *elem;          // The xsl:template element.
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

struct XmlXsltStylesheet
{
    XmlXsltSettings settings;
    XmlDom *doc;
    int errors;
    int warnings;
    int extras_nr;          // The number of runtime extras allocated by the patterns.

    // Imports (libxslt imports, next, parent).
    XmlXsltStylesheet *imports;
    XmlXsltStylesheet *next;
    XmlXsltStylesheet *parent;

    // Template rules by the name of their last step, mode and mode uri.
    HashMap *templates_hash;
    XmlXsltPattern *root_match;
    XmlXsltPattern *key_match;
    XmlXsltPattern *elem_match;
    XmlXsltPattern *attr_match;
    XmlXsltPattern *parent_match;
    XmlXsltPattern *text_match;
    XmlXsltPattern *pi_match;
    XmlXsltPattern *comment_match;
    HashMap *named_templates;   // name and uri -> XsltTemplate.
    XsltTemplate *templates;
};

struct XmlXsltTransform
{
    XmlXsltStylesheet *style;
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

    // Result tree fragments (libxslt XSLT_IS_RES_TREE_FRAG).
    XmlDom **rtfs;
    size_t num_rtfs;
};

// Errors /////////////////////////////////////////////////////////

static void xslt_emit(const XmlXsltSettings *s, const char *msg)
{
    if (s && s->message_callback) s->message_callback(s->message_user_data, msg);
    else fputs(msg, stderr);
}

static char *xslt_vformat(const char *fmt, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    char *b = (char*)malloc((size_t)(n < 0 ? 0 : n) + 1);
    vsnprintf(b, (size_t)(n < 0 ? 0 : n) + 1, fmt, ap);
    return b;
}

/** Like xsltGenericError, the message without context. */
static void xslt_generic_error(const XmlXsltSettings *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *m = xslt_vformat(fmt, ap);
    va_end(ap);
    xslt_emit(s, m);
    free(m);
}

/** Port of xsltPrintErrorContext. */
static void xslt_print_error_context(const XmlXsltSettings *s, XmlXsltTransform *t, XmlXsltStylesheet *style, XMQNode *node)
{
    int line = 0;
    const char *file = NULL;
    const char *name = NULL;
    const char *type = "error";

    if (t && t->state == XSLT_STATE_OK) t->state = XSLT_STATE_ERROR;
    if (!node && t) node = t->inst;
    if (node)
    {
        if (xml_node_kind(node) == XML_DOM_DOCUMENT)
        {
            file = xml_dom_url(xml_node_dom(node));
        }
        else
        {
            line = xml_node_line(node);
            file = xml_dom_url(xml_node_dom(node));
            name = xml_node_name(node);
        }
    }
    if (t) type = "runtime error";
    else if (style) type = "compilation error";

    if (file && line && name) xslt_generic_error(s, "%s: file %s line %d element %s\n", type, file, line, name);
    else if (file && name) xslt_generic_error(s, "%s: file %s element %s\n", type, file, name);
    else if (file && line) xslt_generic_error(s, "%s: file %s line %d\n", type, file, line);
    else if (file) xslt_generic_error(s, "%s: file %s\n", type, file);
    else if (name) xslt_generic_error(s, "%s: element %s\n", type, name);
    else xslt_generic_error(s, "%s\n", type);
}

static const XmlXsltSettings *xslt_settings_of(XmlXsltTransform *t, XmlXsltStylesheet *style)
{
    if (t) return &t->style->settings;
    if (style) return &style->settings;
    return NULL;
}

static void xslt_vtransform_error(const XmlXsltSettings *s, XmlXsltTransform *t, XmlXsltStylesheet *style, XMQNode *node,
                                  const char *fmt, va_list ap)
{
    if (t && t->state == XSLT_STATE_OK) t->state = XSLT_STATE_ERROR;
    if (!node && t) node = t->inst;
    xslt_print_error_context(s, t, style, node);
    char *m = xslt_vformat(fmt, ap);
    xslt_emit(s, m);
    free(m);
}

/** Port of xsltTransformError: the context, then the message. */
static void xslt_transform_error(XmlXsltTransform *t, XmlXsltStylesheet *style, XMQNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xslt_vtransform_error(xslt_settings_of(t, style), t, style, node, fmt, ap);
    va_end(ap);
}

/** xsltTransformError without transformation or stylesheet, like libxslt (the error context
    is then just "error"), but reported through these settings. */
static void xslt_settings_error(const XmlXsltSettings *s, XMQNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xslt_vtransform_error(s, NULL, NULL, node, fmt, ap);
    va_end(ap);
}

void xml_xslt_settings_default(XmlXsltSettings *settings)
{
    memset(settings, 0, sizeof(*settings));
}

// Helpers ////////////////////////////////////////////////////////

static char *xslt_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char*)malloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

static char *xslt_strndup(const char *s, size_t n)
{
    char *r = (char*)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static bool xslt_streq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return !strcmp(a, b);
}

static bool xslt_is_blank(int c)
{
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
}

/** Port of xsltGetUTF8CharZ: one UTF-8 char from a nul terminated string, -1 on error. */
static int xslt_utf8_char(const char *s, int *len)
{
    const unsigned char *utf = (const unsigned char*)s;
    unsigned int c = utf[0];
    if (c & 0x80)
    {
        if ((utf[1] & 0xc0) != 0x80) goto error;
        if ((c & 0xe0) == 0xe0)
        {
            if ((utf[2] & 0xc0) != 0x80) goto error;
            if ((c & 0xf0) == 0xf0)
            {
                if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80) goto error;
                *len = 4;
                c = (utf[0] & 0x7) << 18;
                c |= (utf[1] & 0x3f) << 12;
                c |= (utf[2] & 0x3f) << 6;
                c |= utf[3] & 0x3f;
            }
            else
            {
                *len = 3;
                c = (utf[0] & 0xf) << 12;
                c |= (utf[1] & 0x3f) << 6;
                c |= utf[2] & 0x3f;
            }
        }
        else
        {
            *len = 2;
            c = (utf[0] & 0x1f) << 6;
            c |= utf[1] & 0x3f;
        }
    }
    else
    {
        *len = 1;
    }
    return (int)c;
error:
    *len = 0;
    return -1;
}

/** libxml2 xmlIsCharQ. */
static bool xslt_is_char(int c)
{
    return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) ||
           (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
}

/** The namespaces in scope of an element, innermost first, like xmlGetNsList. */
static XmlXPathNamespace *xslt_ns_list(XMQNode *node, size_t *num)
{
    *num = 0;
    size_t cap = 0;
    XmlXPathNamespace *list = NULL;
    for (XMQNode *e = node; e; e = xml_node_parent(e))
    {
        if (xml_node_kind(e) != XML_DOM_ELEMENT) continue;
        for (XmlNs *ns = xml_node_first_ns_decl(e); ns; ns = xml_ns_next_decl(ns))
        {
            const char *prefix = xml_ns_prefix(ns);
            bool seen = false;
            for (size_t i = 0; i < *num; ++i) if (xslt_streq(list[i].prefix, prefix)) seen = true;
            if (seen) continue;
            if (*num == cap)
            {
                cap = cap ? cap * 2 : 8;
                list = (XmlXPathNamespace*)realloc(list, cap * sizeof(XmlXPathNamespace));
            }
            list[*num].prefix = prefix;
            list[*num].uri = xml_ns_href(ns);
            (*num)++;
        }
    }
    return list;
}

/** The namespace uri of a node (NULL if none or empty). */
static const char *xslt_node_ns_uri(XMQNode *node)
{
    XmlNs *ns = xml_node_ns(node);
    return ns ? xml_ns_href(ns) : NULL;
}

/** Port of xsltGetQNameURI: split name into its local part and return the namespace
    uri of the prefix, NULL if unprefixed. *name is NULL after an unbound prefix. */
static const char *xslt_get_qname_uri(const XmlXsltSettings *s, XMQNode *node, char **name)
{
    char *qname = *name;
    if (!qname || !*qname) return NULL;
    if (!node)
    {
        xslt_generic_error(s, "QName: no element for namespace lookup %s\n", qname);
        free(qname);
        *name = NULL;
        return NULL;
    }
    // Nasty but valid.
    if (qname[0] == ':') return NULL;
    size_t len = 0;
    while (qname[len] && qname[len] != ':') len++;
    if (!qname[len]) return NULL;
    if (qname[0] == 'x' && qname[1] == 'm' && qname[2] == 'l' && qname[3] == ':')
    {
        if (!qname[4]) return NULL;
        *name = xslt_strdup(&qname[4]);
        free(qname);
        return XML_DOM_XML_NAMESPACE;
    }
    qname[len] = 0;
    XmlNs *ns = xml_ns_lookup_prefix(node, qname);
    if (!ns)
    {
        xslt_generic_error(s, "%s:%s : no namespace bound to prefix %s\n", qname, &qname[len + 1], qname);
        *name = NULL;
        free(qname);
        return NULL;
    }
    *name = xslt_strdup(&qname[len + 1]);
    free(qname);
    return xml_ns_href(ns);
}

/** Report an xpath error like libxml2 xmlXPathErr with the default handler: the message,
    then the expression and a marker when the position is inside it. */
static void xslt_report_xpath_error(const XmlXsltSettings *s, const char *expr, const XmlXPathError *err)
{
    xslt_generic_error(s, "XPath error : %s\n", err->message);
    size_t len = expr ? strlen(expr) : 0;
    if (expr && err->offset < 100 && err->offset < len)
    {
        char buf[150];
        size_t i;
        for (i = 0; i < err->offset; i++) buf[i] = ' ';
        buf[i++] = '^';
        buf[i] = 0;
        xslt_generic_error(s, "%s\n%s\n", expr, buf);
    }
}

static XmlXPathComp *xslt_xpath_compile(const XmlXsltSettings *s, XMQNode *elem, const char *str, bool no_variables)
{
    XmlXPathSettings st;
    xml_xpath_settings_default(&st);
    size_t nn = 0;
    XmlXPathNamespace *nss = elem ? xslt_ns_list(elem, &nn) : NULL;
    st.namespaces = nss;
    st.num_namespaces = nn;
    st.no_variables = no_variables;
    XmlXPathError err;
    XmlXPathComp *comp = xml_xpath_compile(str, &st, &err);
    if (!comp) xslt_report_xpath_error(s, str, &err);
    free(nss);
    return comp;
}

static bool xslt_is_rtf(XmlXsltTransform *t, XmlDom *dom)
{
    for (size_t i = 0; i < t->num_rtfs; ++i) if (t->rtfs[i] == dom) return true;
    return false;
}

static int xslt_allocate_extra(XmlXsltStylesheet *style)
{
    return style->extras_nr++;
}

/** Port of xsltAllocateExtraCtxt. */
static int xslt_allocate_extra_ctxt(XmlXsltTransform *t)
{
    if (t->extras_nr >= t->extras_max)
    {
        int nm = t->extras_max ? t->extras_max * 2 : 20;
        if (nm <= t->extras_nr) nm = t->extras_nr + 1;
        t->extras = (XsltRuntimeExtra*)realloc(t->extras, (size_t)nm * sizeof(XsltRuntimeExtra));
        memset(t->extras + t->extras_max, 0, (size_t)(nm - t->extras_max) * sizeof(XsltRuntimeExtra));
        t->extras_max = nm;
    }
    return t->extras_nr++;
}

#define XSLT_RUNTIME_EXTRA(t, nr) ((t)->extras[(nr)])

// Patterns, port of libxslt pattern.c /////////////////////////////

typedef enum
{
    XSLT_OP_END = 0,
    XSLT_OP_ROOT,
    XSLT_OP_ELEM,
    XSLT_OP_ATTR,
    XSLT_OP_PARENT,
    XSLT_OP_ANCESTOR,
    XSLT_OP_ID,
    XSLT_OP_KEY,
    XSLT_OP_NS,
    XSLT_OP_ALL,
    XSLT_OP_PI,
    XSLT_OP_COMMENT,
    XSLT_OP_TEXT,
    XSLT_OP_NODE,
    XSLT_OP_PREDICATE
} XsltOp;

typedef enum
{
    XSLT_AXIS_NONE = 0,
    XSLT_AXIS_CHILD = 1,
    XSLT_AXIS_ATTRIBUTE
} XsltAxis;

typedef struct
{
    int step;
    XMQNode *node;
} XsltStepState;

typedef struct
{
    int nbstates;
    int maxstates;
    XsltStepState *states;
} XsltStepStates;

typedef struct
{
    XsltOp op;
    char *value;
    char *value2;
    char *value3;
    XmlXPathComp *comp;
    // Optimisations for count.
    int previous_extra;
    int index_extra;
    int len_extra;
} XsltStepOp;

struct XmlXsltPattern
{
    XmlXsltPattern *next;   // Siblings in the name hash.
    float priority;
    char *pattern;
    char *mode;
    char *mode_uri;
    XsltTemplate *templ;
    XMQNode *node;          // The containing element.

    int direct;
    int nb_step;
    int max_step;
    XmlXPathNamespace *ns_list; // The namespaces in scope.
    size_t ns_nr;
    XsltStepOp *steps;
};

typedef struct
{
    XmlXsltStylesheet *style;
    XmlXsltTransform *ctxt; // The transformation or NULL.
    const char *cur;
    const char *base;
    XMQNode *elem;
    int error;
    XmlXsltPattern *comp;
} XsltParserContext;

static const XmlXsltSettings *xslt_pctx_settings(XsltParserContext *ctxt)
{
    return ctxt->style ? &ctxt->style->settings : NULL;
}

static XmlXsltPattern *xslt_new_comp_match(void)
{
    XmlXsltPattern *cur = (XmlXsltPattern*)calloc(1, sizeof(XmlXsltPattern));
    cur->max_step = 10;
    cur->steps = (XsltStepOp*)calloc((size_t)cur->max_step, sizeof(XsltStepOp));
    return cur;
}

static void xslt_free_comp_match(XmlXsltPattern *comp)
{
    if (!comp) return;
    free(comp->pattern);
    free(comp->ns_list);
    free(comp->mode);
    free(comp->mode_uri);
    for (int i = 0; i < comp->nb_step; ++i)
    {
        XsltStepOp *op = &comp->steps[i];
        free(op->value);
        free(op->value2);
        free(op->value3);
        xml_xpath_comp_free(op->comp);
    }
    free(comp->steps);
    free(comp);
}

static void xslt_free_comp_match_list(XmlXsltPattern *comp)
{
    while (comp)
    {
        XmlXsltPattern *cur = comp;
        comp = comp->next;
        xslt_free_comp_match(cur);
    }
}

void xml_xslt_pattern_free(XmlXsltPattern *pattern)
{
    xslt_free_comp_match_list(pattern);
}

double xml_xslt_pattern_priority(const XmlXsltPattern *pattern)
{
    return pattern ? pattern->priority : 0;
}

/** Port of xsltCompMatchAdd. Takes ownership of value and value2. */
static int xslt_comp_match_add(XsltParserContext *ctxt, XmlXsltPattern *comp, XsltOp op, char *value, char *value2, int novar)
{
    if (comp->nb_step >= comp->max_step)
    {
        comp->steps = (XsltStepOp*)realloc(comp->steps, (size_t)comp->max_step * 2 * sizeof(XsltStepOp));
        comp->max_step *= 2;
    }
    XsltStepOp *s = &comp->steps[comp->nb_step];
    memset(s, 0, sizeof(*s));
    s->op = op;
    s->value = value;
    s->value2 = value2;
    if (ctxt->ctxt)
    {
        s->previous_extra = xslt_allocate_extra_ctxt(ctxt->ctxt);
        s->index_extra = xslt_allocate_extra_ctxt(ctxt->ctxt);
        s->len_extra = xslt_allocate_extra_ctxt(ctxt->ctxt);
    }
    else
    {
        s->previous_extra = xslt_allocate_extra(ctxt->style);
        s->index_extra = xslt_allocate_extra(ctxt->style);
        s->len_extra = xslt_allocate_extra(ctxt->style);
    }
    if (op == XSLT_OP_PREDICATE)
    {
        s->comp = xslt_xpath_compile(xslt_pctx_settings(ctxt), ctxt->elem, value, novar != 0);
        if (!s->comp)
        {
            xslt_transform_error(NULL, ctxt->style, ctxt->elem, "Failed to compile predicate\n");
            if (ctxt->style) ctxt->style->errors++;
        }
    }
    comp->nb_step++;
    return 0;
}

static void xslt_swap_steps(XsltStepOp *a, XsltStepOp *b)
{
    XsltStepOp t = *a;
    *a = *b;
    *b = t;
}

/** Port of xsltSwapTopCompMatch: reverse the two top steps. */
static void xslt_swap_top_comp_match(XmlXsltPattern *comp)
{
    int j = comp->nb_step - 1;
    if (j > 0) xslt_swap_steps(&comp->steps[j - 1], &comp->steps[j]);
}

/** Port of xsltReverseCompMatch. */
static void xslt_reverse_comp_match(XsltParserContext *ctxt, XmlXsltPattern *comp)
{
    int i = 0;
    int j = comp->nb_step - 1;
    while (j > i)
    {
        xslt_swap_steps(&comp->steps[i], &comp->steps[j]);
        j--;
        i++;
    }
    xslt_comp_match_add(ctxt, comp, XSLT_OP_END, NULL, NULL, 0);

    // Consecutive predicates: match by evaluating the whole pattern.
    for (i = 0; i < comp->nb_step - 1; i++)
    {
        if (comp->steps[i].op == XSLT_OP_PREDICATE && comp->steps[i + 1].op == XSLT_OP_PREDICATE)
        {
            comp->direct = 1;
            if (comp->pattern[0] != '/')
            {
                size_t n = strlen(comp->pattern);
                char *query = (char*)malloc(n + 3);
                memcpy(query, "//", 2);
                memcpy(query + 2, comp->pattern, n + 1);
                free(comp->pattern);
                comp->pattern = query;
            }
            break;
        }
    }
}

// The interpreter for the precompiled patterns.

static int xslt_pat_push_state(XmlXsltTransform *t, XsltStepStates *states, int step, XMQNode *node)
{
    if (states->maxstates <= states->nbstates)
    {
        int nm = states->maxstates == 0 ? 4 : 2 * states->maxstates;
        XsltStepState *tmp = (XsltStepState*)realloc(states->states, (size_t)nm * sizeof(XsltStepState));
        if (!tmp)
        {
            xslt_generic_error(&t->style->settings, "xsltPatPushState: memory re-allocation failure.\n");
            t->state = XSLT_STATE_STOPPED;
            return -1;
        }
        states->states = tmp;
        states->maxstates = nm;
    }
    states->states[states->nbstates].step = step;
    states->states[states->nbstates++].node = node;
    return 0;
}

static bool xslt_is_document(XMQNode *n)
{
    return xml_node_kind(n) == XML_DOM_DOCUMENT;
}

/** Evaluate an expression with namespaces from a pattern, like libxml2 xmlXPathEval with the
    namespaces of the xpath context set to nsList. */
static XmlXPathResult *xslt_eval_with_ns(XmlXsltTransform *t, const char *expr, XmlXPathNamespace *ns_list, size_t ns_nr,
                                         XMQNode *node, long position, long size)
{
    XmlXPathSettings st;
    xml_xpath_settings_default(&st);
    st.namespaces = ns_list;
    st.num_namespaces = ns_nr;
    XmlXPathError err;
    XmlXPathComp *comp = xml_xpath_compile(expr, &st, &err);
    if (!comp) return NULL;
    XmlXPathResult *r = xml_xpath_eval_comp(t->xpath, comp, node, position, size, &err);
    xml_xpath_comp_free(comp);
    return r;
}

static void xslt_free_result_wrapper(void *obj)
{
    xml_xpath_result_free((XmlXPathResult*)obj);
}

/** Port of xsltTestCompMatchDirect: evaluate the pattern as an expression and look for the node. */
static int xslt_test_comp_match_direct(XmlXsltTransform *t, XmlXsltPattern *comp, XMQNode *node,
                                       XmlXPathNamespace *ns_list, size_t ns_nr)
{
    XmlDom *doc = xml_node_dom(node);
    bool is_rvt = xslt_is_rtf(t, doc);
    XsltStepOp *sel = &comp->steps[0]; // Store extra in first step arbitrarily.
    bool nocache = false;

    XmlDom *prevdoc = (XmlDom*)XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr;
    XmlXPathResult *list = (XmlXPathResult*)XSLT_RUNTIME_EXTRA(t, sel->len_extra).info;

    if (!list || prevdoc != doc)
    {
        XMQNode *parent = xml_node_parent(node);
        XmlXPathResult *newlist = xslt_eval_with_ns(t, comp->pattern, ns_list, ns_nr, node, t->xp_position, t->xp_size);
        if (!newlist) return -1;
        if (xml_xpath_result_type(newlist) != XML_XPATH_NODESET)
        {
            xml_xpath_result_free(newlist);
            return -1;
        }
        if (!parent || is_rvt) nocache = true;
        if (!nocache)
        {
            if (list) xml_xpath_result_free(list);
            list = newlist;
            XSLT_RUNTIME_EXTRA(t, sel->len_extra).info = list;
            XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr = doc;
            XSLT_RUNTIME_EXTRA(t, sel->index_extra).val.ival = 0;
            XSLT_RUNTIME_EXTRA(t, sel->len_extra).deallocate = xslt_free_result_wrapper;
        }
        else
        {
            list = newlist;
        }
    }
    int found = 0;
    for (size_t j = 0; j < xml_xpath_result_size(list); ++j)
    {
        if (xml_xpath_result_node(list, j) == node)
        {
            found = 1;
            break;
        }
    }
    if (nocache) xml_xpath_result_free(list);
    return found;
}

/** Find the nodes of a key (keys.c, xsltGetKey), not yet ported. */
static XmlXPathResult *xslt_get_key(XmlXsltTransform *t, const char *name, const char *uri, const char *value)
{
    (void)t; (void)name; (void)uri; (void)value;
    return NULL;
}

/** Port of xsltTestStepMatch. Returns 1 if the node matches the step, 0 if not, -1 on an error. */
static int xslt_test_step_match(XmlXsltTransform *t, XMQNode *node, XsltStepOp *step)
{
    XmlNodeKind k = xml_node_kind(node);
    switch (step->op)
    {
    case XSLT_OP_ROOT:
        return xslt_is_document(node) ? 1 : 0;
    case XSLT_OP_ELEM:
    {
        if (k != XML_DOM_ELEMENT) return 0;
        if (!step->value) return 1;
        if (strcmp(step->value, xml_node_name(node))) return 0;
        // Namespace test.
        XmlNs *ns = xml_node_ns(node);
        if (!ns)
        {
            if (step->value2) return 0;
        }
        else if (xml_ns_href(ns))
        {
            if (!step->value2) return 0;
            if (strcmp(step->value2, xml_ns_href(ns))) return 0;
        }
        return 1;
    }
    case XSLT_OP_ATTR:
    {
        if (k != XML_DOM_ATTRIBUTE) return 0;
        if (step->value && strcmp(step->value, xml_node_name(node))) return 0;
        XmlNs *ns = xml_node_ns(node);
        if (!ns)
        {
            if (step->value2) return 0;
        }
        else if (step->value2)
        {
            if (!xslt_streq(step->value2, xml_ns_href(ns))) return 0;
        }
        return 1;
    }
    case XSLT_OP_ID:
    {
        if (k != XML_DOM_ELEMENT) return 0;
        XMQNode *e = xml_xpath_context_get_id(t->xpath, node, step->value);
        if (e != node) return 0;
        break;
    }
    case XSLT_OP_KEY:
    {
        XmlXPathResult *list = xslt_get_key(t, step->value, step->value3, step->value2);
        if (!list) return 0;
        bool found = false;
        for (size_t i = 0; i < xml_xpath_result_size(list); ++i)
        {
            if (xml_xpath_result_node(list, i) == node) found = true;
        }
        if (!found) return 0;
        break;
    }
    case XSLT_OP_NS:
    {
        if (k != XML_DOM_ELEMENT) return 0;
        XmlNs *ns = xml_node_ns(node);
        if (!ns)
        {
            if (step->value) return 0;
        }
        else if (xml_ns_href(ns))
        {
            if (!step->value) return 0;
            if (strcmp(step->value, xml_ns_href(ns))) return 0;
        }
        break;
    }
    case XSLT_OP_ALL:
        if (k != XML_DOM_ELEMENT) return 0;
        break;
    case XSLT_OP_PI:
        if (k != XML_DOM_PI) return 0;
        if (step->value && !xslt_streq(step->value, xml_node_name(node))) return 0;
        break;
    case XSLT_OP_COMMENT:
        if (k != XML_DOM_COMMENT) return 0;
        break;
    case XSLT_OP_TEXT:
        if (k != XML_DOM_TEXT && k != XML_DOM_CDATA) return 0;
        break;
    case XSLT_OP_NODE:
        switch (k)
        {
        case XML_DOM_ELEMENT:
        case XML_DOM_CDATA:
        case XML_DOM_PI:
        case XML_DOM_COMMENT:
        case XML_DOM_TEXT:
            break;
        default:
            return 0;
        }
        break;
    default:
        xslt_transform_error(t, NULL, node, "xsltTestStepMatch: unexpected step op %d\n", step->op);
        return -1;
    }
    return 1;
}

/** Port of xsltEvalXPathPredicate (templates.c): a number is compared with the context position. */
static int xslt_eval_xpath_predicate(XmlXsltTransform *t, XmlXPathComp *comp)
{
    if (!t->inst)
    {
        xslt_transform_error(t, NULL, NULL, "xsltEvalXPathPredicate: No context or instruction\n");
        return 0;
    }
    XmlXPathError err;
    XmlXPathResult *res = xml_xpath_eval_comp(t->xpath, comp, t->node, t->xp_position, t->xp_size, &err);
    int ret;
    if (res)
    {
        // libxml2 xmlXPathEvalPredicate.
        if (xml_xpath_result_type(res) == XML_XPATH_NUMBER) ret = xml_xpath_result_number(res) == (double)t->xp_position;
        else ret = xml_xpath_result_to_boolean(res);
        xml_xpath_result_free(res);
    }
    else
    {
        t->state = XSLT_STATE_STOPPED;
        ret = 0;
    }
    return ret;
}

/** Port of xsltTestPredicateMatch. */
static int xslt_test_predicate_match(XmlXsltTransform *t, XmlXsltPattern *comp, XMQNode *node, XsltStepOp *step, XsltStepOp *sel)
{
    (void)comp;
    int pos = 0, len = 0;
    if (!step->value || !step->comp || !sel) return 0;

    XmlDom *doc = xml_node_dom(node);
    bool is_rvt = xslt_is_rtf(t, doc);

    // Recompute the context size and position.
    long old_cs = t->xp_size;
    long old_cp = t->xp_position;
    {
        bool nocache = false;
        XMQNode *previous = (XMQNode*)XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr;
        if (previous && xml_node_parent(previous) == xml_node_parent(node))
        {
            // Just walk back to adjust the index.
            int indx = 0;
            XMQNode *sibling = node;
            while (sibling)
            {
                if (sibling == previous) break;
                if (xslt_test_step_match(t, sibling, sel)) indx++;
                sibling = xml_node_prev(sibling);
            }
            if (!sibling)
            {
                // Going backward in document order.
                indx = 0;
                sibling = node;
                while (sibling)
                {
                    if (sibling == previous) break;
                    if (xslt_test_step_match(t, sibling, sel)) indx--;
                    sibling = xml_node_next(sibling);
                }
            }
            if (sibling)
            {
                pos = XSLT_RUNTIME_EXTRA(t, sel->index_extra).val.ival + indx;
                // In a result tree fragment len is kept but the node cannot be cached.
                len = XSLT_RUNTIME_EXTRA(t, sel->len_extra).val.ival;
                if (!is_rvt)
                {
                    XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr = node;
                    XSLT_RUNTIME_EXTRA(t, sel->index_extra).val.ival = pos;
                }
            }
            else
            {
                pos = 0;
            }
        }
        else
        {
            // Recompute the index.
            XMQNode *parent = xml_node_parent(node);
            // Like libxml2 parent->children: an attribute is never found among them, then
            // the position stays 0 and the context position and size are not changed.
            XMQNode *siblings = parent ? xml_node_first_child(parent) : NULL;
            while (siblings)
            {
                if (siblings == node)
                {
                    len++;
                    pos = len;
                }
                else if (xslt_test_step_match(t, siblings, sel))
                {
                    len++;
                }
                siblings = xml_node_next(siblings);
            }
            if (!parent)
            {
                nocache = true;
            }
            else
            {
                XMQNode *top = parent;
                while (xml_node_parent(top)) top = xml_node_parent(top);
                if (!xslt_is_document(top) || top != xml_dom_document(doc)) nocache = true;
            }
        }
        if (pos != 0)
        {
            t->xp_size = len;
            t->xp_position = pos;
            if (!is_rvt && !nocache)
            {
                XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr = node;
                XSLT_RUNTIME_EXTRA(t, sel->index_extra).val.ival = pos;
                XSLT_RUNTIME_EXTRA(t, sel->len_extra).val.ival = len;
            }
        }
    }

    XMQNode *old_node = t->node;
    t->node = node;
    int match = xslt_eval_xpath_predicate(t, step->comp);
    if (pos != 0)
    {
        t->xp_size = old_cs;
        t->xp_position = old_cp;
    }
    t->node = old_node;
    return match;
}

static bool xslt_name_and_ns_match(XsltStepOp *step, XMQNode *node)
{
    if (strcmp(step->value, xml_node_name(node))) return false;
    XmlNs *ns = xml_node_ns(node);
    if (!ns) return step->value2 == NULL;
    if (xml_ns_href(ns))
    {
        if (!step->value2) return false;
        if (strcmp(step->value2, xml_ns_href(ns))) return false;
    }
    return true;
}

/** Port of xsltTestCompMatch. Returns 1 if the node matches, 0 if not, -1 on an error. */
static int xslt_test_comp_match(XmlXsltTransform *t, XmlXsltPattern *comp, XMQNode *match_node,
                                const char *mode, const char *mode_uri)
{
    int i;
    int found = 0;
    XMQNode *node = match_node;
    XsltStepOp *step, *sel = NULL;
    XsltStepStates states = { 0, 0, NULL };

    if (!comp || !node || !t)
    {
        xslt_transform_error(t, NULL, node, "xsltTestCompMatch: null arg\n");
        return -1;
    }
    if (!xslt_streq(comp->mode, mode)) return 0;
    if (!xslt_streq(comp->mode_uri, mode_uri)) return 0;

    // Some XPath functions rely on inst being set correctly.
    XMQNode *old_inst = t->inst;
    t->inst = comp->node;

    i = 0;
restart:
    for (; i < comp->nb_step; i++)
    {
        step = &comp->steps[i];
        if (step->op != XSLT_OP_PREDICATE) sel = step;
        switch (step->op)
        {
        case XSLT_OP_END:
            goto found;
        case XSLT_OP_PARENT:
            if (xslt_is_document(node) || xml_node_kind(node) == XML_DOM_NAMESPACE) goto rollback;
            node = xml_node_parent(node);
            if (!node) goto rollback;
            if (!step->value) continue;
            if (xml_node_kind(node) != XML_DOM_ELEMENT || !xml_node_name(node)) goto rollback;
            if (!xslt_name_and_ns_match(step, node)) goto rollback;
            continue;
        case XSLT_OP_ANCESTOR:
            // TODO: implement coalescing of ANCESTOR/NODE ops.
            if (!step->value)
            {
                step = &comp->steps[i + 1];
                if (step->op == XSLT_OP_ROOT) goto found;
                // Added NS, ID and KEY as a result of bug 168208.
                if (step->op != XSLT_OP_ELEM && step->op != XSLT_OP_ALL && step->op != XSLT_OP_NS &&
                    step->op != XSLT_OP_ID && step->op != XSLT_OP_KEY)
                {
                    goto rollback;
                }
            }
            if (!node) goto rollback;
            if (xslt_is_document(node) || xml_node_kind(node) == XML_DOM_NAMESPACE) goto rollback;
            node = xml_node_parent(node);
            if (step->op != XSLT_OP_ELEM && step->op != XSLT_OP_ALL)
            {
                xslt_pat_push_state(t, &states, i, node);
                continue;
            }
            i++;
            sel = step;
            if (!step->value)
            {
                xslt_pat_push_state(t, &states, i - 1, node);
                continue;
            }
            while (node)
            {
                if (xml_node_kind(node) == XML_DOM_ELEMENT && !strcmp(step->value, xml_node_name(node)))
                {
                    XmlNs *ns = xml_node_ns(node);
                    if (!ns)
                    {
                        if (!step->value2) break;
                    }
                    else if (xml_ns_href(ns))
                    {
                        if (step->value2 && !strcmp(step->value2, xml_ns_href(ns))) break;
                    }
                }
                node = xml_node_parent(node);
            }
            if (!node) goto rollback;
            xslt_pat_push_state(t, &states, i - 1, node);
            continue;
        case XSLT_OP_PREDICATE:
            // Cascading predicates, or a predicate after an op that is not optimized yet:
            // evaluate the whole pattern.
            if (comp->direct)
            {
                found = xslt_test_comp_match_direct(t, comp, match_node, comp->ns_list, comp->ns_nr);
                goto exit;
            }
            if (!xslt_test_predicate_match(t, comp, node, step, sel)) goto rollback;
            break;
        default:
            if (xslt_test_step_match(t, node, step) != 1) goto rollback;
            break;
        }
    }
found:
    found = 1;
exit:
    t->inst = old_inst;
    free(states.states);
    return found;
rollback:
    // Got an error, try to roll back.
    if (!states.states || states.nbstates <= 0)
    {
        found = 0;
        goto exit;
    }
    states.nbstates--;
    i = states.states[states.nbstates].step;
    node = states.states[states.nbstates].node;
    goto restart;
}

int xml_xslt_pattern_match(XmlXsltTransform *t, XMQNode *node, XmlXsltPattern *comp)
{
    if (!t || !node) return -1;
    while (comp)
    {
        int ret = xslt_test_comp_match(t, comp, node, NULL, NULL);
        if (ret == 1) return 1;
        comp = comp->next;
    }
    return 0;
}

// The pattern parser (xsltCompileLocationPathPattern etc).

#define CUR (*ctxt->cur)
#define NXT(val) ctxt->cur[(val)]
#define NEXT (*ctxt->cur ? ctxt->cur++ : ctxt->cur)
#define SKIP_BLANKS while (xslt_is_blank((unsigned char)CUR)) NEXT
#define PUSH(op, val, val2, novar) if (xslt_comp_match_add(ctxt, ctxt->comp, (op), (val), (val2), (novar))) goto error;
#define SWAP() xslt_swap_top_comp_match(ctxt->comp);

/** Port of xsltScanLiteral. */
static char *xslt_scan_literal(XsltParserContext *ctxt)
{
    SKIP_BLANKS;
    char q = CUR;
    if (q != '"' && q != '\'')
    {
        ctxt->error = 1;
        return NULL;
    }
    NEXT;
    const char *start = ctxt->cur;
    const char *cur = start;
    int len;
    int val = xslt_utf8_char(cur, &len);
    while (xslt_is_char(val) && val != q)
    {
        cur += len;
        val = xslt_utf8_char(cur, &len);
    }
    if (!xslt_is_char(val))
    {
        ctxt->error = 1;
        return NULL;
    }
    char *ret = xslt_strndup(start, (size_t)(cur - start));
    cur += len;
    ctxt->cur = cur;
    return ret;
}

/** Port of xsltScanNCName. */
static char *xslt_scan_ncname(XsltParserContext *ctxt)
{
    SKIP_BLANKS;
    const char *q = ctxt->cur, *cur = ctxt->cur;
    int len;
    int val = xslt_utf8_char(cur, &len);
    if (val < 0 || (!xml_char_is_base_char(val) && !xml_char_is_ideographic(val) && val != '_')) return NULL;
    while (val >= 0 && (xml_char_is_base_char(val) || xml_char_is_ideographic(val) || xml_char_is_digit(val) ||
                        val == '.' || val == '-' || val == '_' || xml_char_is_combining(val) || xml_char_is_extender(val)))
    {
        cur += len;
        val = xslt_utf8_char(cur, &len);
    }
    char *ret = xslt_strndup(q, (size_t)(cur - q));
    ctxt->cur = cur;
    return ret;
}

/** libxml2 xmlXPathIsNodeType. */
static bool xslt_is_node_type(const char *name)
{
    return !strcmp(name, "node") || !strcmp(name, "comment") || !strcmp(name, "text") ||
           !strcmp(name, "processing-instruction");
}

/** Port of xsltCompileIdKeyPattern. */
static void xslt_compile_id_key_pattern(XsltParserContext *ctxt, const char *name, int aid, int novar, XsltAxis axis)
{
    char *lit = NULL;
    char *lit2 = NULL;
    if (CUR != '(')
    {
        xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ( expected\n");
        ctxt->error = 1;
        return;
    }
    if (aid && !strcmp(name, "id"))
    {
        if (axis != 0)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : NodeTest expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
            free(lit);
            ctxt->error = 1;
            return;
        }
        NEXT;
        PUSH(XSLT_OP_ID, lit, NULL, novar);
        lit = NULL;
    }
    else if (aid && !strcmp(name, "key"))
    {
        if (axis != 0)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : NodeTest expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ',')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : , expected\n");
            free(lit);
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit2 = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
            free(lit);
            free(lit2);
            ctxt->error = 1;
            return;
        }
        NEXT;
        // URGENT TODO (libxslt): support namespace in keys.
        PUSH(XSLT_OP_KEY, lit, lit2, novar);
        lit = NULL;
        lit2 = NULL;
    }
    else if (!strcmp(name, "processing-instruction"))
    {
        NEXT;
        SKIP_BLANKS;
        if (CUR != ')')
        {
            lit = xslt_scan_literal(ctxt);
            if (ctxt->error)
            {
                xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
                free(lit);
                return;
            }
            SKIP_BLANKS;
            if (CUR != ')')
            {
                xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
                ctxt->error = 1;
                free(lit);
                return;
            }
        }
        NEXT;
        PUSH(XSLT_OP_PI, lit, NULL, novar);
        lit = NULL;
    }
    else if (!strcmp(name, "text"))
    {
        NEXT;
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        PUSH(XSLT_OP_TEXT, NULL, NULL, novar);
    }
    else if (!strcmp(name, "comment"))
    {
        NEXT;
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        PUSH(XSLT_OP_COMMENT, NULL, NULL, novar);
    }
    else if (!strcmp(name, "node"))
    {
        NEXT;
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        if (axis == XSLT_AXIS_ATTRIBUTE)
        {
            PUSH(XSLT_OP_ATTR, NULL, NULL, novar);
        }
        else
        {
            PUSH(XSLT_OP_NODE, NULL, NULL, novar);
        }
    }
    else if (aid)
    {
        xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : expecting 'key' or 'id' or node type\n");
        ctxt->error = 1;
        return;
    }
    else
    {
        xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : node type\n");
        ctxt->error = 1;
        return;
    }
error:
    return;
}

/** Port of xsltCompileStepPattern. Takes ownership of token. */
static void xslt_compile_step_pattern(XsltParserContext *ctxt, char *token, int novar)
{
    char *url = NULL;
    char *ret = NULL;
    int level;
    XsltAxis axis = XSLT_AXIS_NONE;

    SKIP_BLANKS;
    if (!token && CUR == '@')
    {
        NEXT;
        axis = XSLT_AXIS_ATTRIBUTE;
    }
parse_node_test:
    if (!token) token = xslt_scan_ncname(ctxt);
    if (!token)
    {
        if (CUR == '*')
        {
            NEXT;
            if (axis == XSLT_AXIS_ATTRIBUTE)
            {
                PUSH(XSLT_OP_ATTR, NULL, NULL, novar);
            }
            else
            {
                PUSH(XSLT_OP_ALL, NULL, NULL, novar);
            }
            goto parse_predicate;
        }
        else
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : Name expected\n");
            ctxt->error = 1;
            goto error;
        }
    }

    SKIP_BLANKS;
    if (CUR == '(')
    {
        xslt_compile_id_key_pattern(ctxt, token, 0, novar, axis);
        free(token);
        token = NULL;
        if (ctxt->error) goto error;
    }
    else if (CUR == ':')
    {
        NEXT;
        if (CUR != ':')
        {
            char *prefix = token;
            // This is a namespace match.
            token = xslt_scan_ncname(ctxt);
            XmlNs *ns = ctxt->elem ? xml_ns_lookup_prefix(ctxt->elem, prefix) : NULL;
            if (!ns)
            {
                xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : no namespace bound to prefix %s\n", prefix);
                free(prefix);
                ctxt->error = 1;
                goto error;
            }
            url = xslt_strdup(xml_ns_href(ns));
            free(prefix);
            if (!token)
            {
                if (CUR == '*')
                {
                    NEXT;
                    if (axis == XSLT_AXIS_ATTRIBUTE)
                    {
                        PUSH(XSLT_OP_ATTR, NULL, url, novar);
                        url = NULL;
                    }
                    else
                    {
                        PUSH(XSLT_OP_NS, url, NULL, novar);
                        url = NULL;
                    }
                }
                else
                {
                    xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : Name expected\n");
                    ctxt->error = 1;
                    goto error;
                }
            }
            else
            {
                if (axis == XSLT_AXIS_ATTRIBUTE)
                {
                    PUSH(XSLT_OP_ATTR, token, url, novar);
                    token = NULL;
                    url = NULL;
                }
                else
                {
                    PUSH(XSLT_OP_ELEM, token, url, novar);
                    token = NULL;
                    url = NULL;
                }
            }
        }
        else
        {
            if (axis != 0)
            {
                xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : NodeTest expected\n");
                ctxt->error = 1;
                goto error;
            }
            NEXT;
            if (!strcmp(token, "child"))
            {
                axis = XSLT_AXIS_CHILD;
            }
            else if (!strcmp(token, "attribute"))
            {
                axis = XSLT_AXIS_ATTRIBUTE;
            }
            else
            {
                xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : 'child' or 'attribute' expected\n");
                ctxt->error = 1;
                goto error;
            }
            free(token);
            token = NULL;
            SKIP_BLANKS;
            token = xslt_scan_ncname(ctxt);
            goto parse_node_test;
        }
    }
    else
    {
        const char *uri = xslt_get_qname_uri(xslt_pctx_settings(ctxt), ctxt->elem, &token);
        if (!token)
        {
            ctxt->error = 1;
            goto error;
        }
        if (uri) url = xslt_strdup(uri);
        if (axis == XSLT_AXIS_ATTRIBUTE)
        {
            PUSH(XSLT_OP_ATTR, token, url, novar);
            token = NULL;
            url = NULL;
        }
        else
        {
            PUSH(XSLT_OP_ELEM, token, url, novar);
            token = NULL;
            url = NULL;
        }
    }
parse_predicate:
    SKIP_BLANKS;
    level = 0;
    while (CUR == '[')
    {
        level++;
        NEXT;
        const char *q = ctxt->cur;
        while (CUR != 0)
        {
            // Skip over nested predicates.
            if (CUR == '[')
            {
                level++;
            }
            else if (CUR == ']')
            {
                level--;
                if (level == 0) break;
            }
            else if (CUR == '"')
            {
                NEXT;
                while (CUR != 0 && CUR != '"') NEXT;
            }
            else if (CUR == '\'')
            {
                NEXT;
                while (CUR != 0 && CUR != '\'') NEXT;
            }
            NEXT;
        }
        if (CUR == 0)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : ']' expected\n");
            ctxt->error = 1;
            return;
        }
        ret = xslt_strndup(q, (size_t)(ctxt->cur - q));
        PUSH(XSLT_OP_PREDICATE, ret, NULL, novar);
        ret = NULL;
        // Push the predicate lower than the local test.
        SWAP();
        NEXT;
        SKIP_BLANKS;
    }
    return;
error:
    free(token);
    free(url);
    free(ret);
}

/** Port of xsltCompileRelativePathPattern. */
static void xslt_compile_relative_path_pattern(XsltParserContext *ctxt, char *token, int novar)
{
    xslt_compile_step_pattern(ctxt, token, novar);
    if (ctxt->error) goto error;
    SKIP_BLANKS;
    while (CUR != 0 && CUR != '|')
    {
        if (CUR == '/' && NXT(1) == '/')
        {
            PUSH(XSLT_OP_ANCESTOR, NULL, NULL, novar);
            NEXT;
            NEXT;
            SKIP_BLANKS;
            xslt_compile_step_pattern(ctxt, NULL, novar);
        }
        else if (CUR == '/')
        {
            PUSH(XSLT_OP_PARENT, NULL, NULL, novar);
            NEXT;
            SKIP_BLANKS;
            xslt_compile_step_pattern(ctxt, NULL, novar);
        }
        else
        {
            ctxt->error = 1;
        }
        if (ctxt->error) goto error;
        SKIP_BLANKS;
    }
error:
    return;
}

/** Port of xsltCompileLocationPathPattern. */
static void xslt_compile_location_path_pattern(XsltParserContext *ctxt, int novar)
{
    SKIP_BLANKS;
    if (CUR == '/' && NXT(1) == '/')
    {
        // Since the query is reversed a leading // can be safely ignored.
        NEXT;
        NEXT;
        ctxt->comp->priority = 0.5; // '//' means not 0 priority.
        xslt_compile_relative_path_pattern(ctxt, NULL, novar);
    }
    else if (CUR == '/')
    {
        // We need to find root as the parent.
        NEXT;
        SKIP_BLANKS;
        PUSH(XSLT_OP_ROOT, NULL, NULL, novar);
        if (CUR != 0 && CUR != '|')
        {
            PUSH(XSLT_OP_PARENT, NULL, NULL, novar);
            xslt_compile_relative_path_pattern(ctxt, NULL, novar);
        }
    }
    else if (CUR == '*')
    {
        xslt_compile_relative_path_pattern(ctxt, NULL, novar);
    }
    else if (CUR == '@')
    {
        xslt_compile_relative_path_pattern(ctxt, NULL, novar);
    }
    else
    {
        char *name = xslt_scan_ncname(ctxt);
        if (!name)
        {
            xslt_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileLocationPathPattern : Name expected\n");
            ctxt->error = 1;
            return;
        }
        SKIP_BLANKS;
        if (CUR == '(' && !xslt_is_node_type(name))
        {
            xslt_compile_id_key_pattern(ctxt, name, 1, novar, XSLT_AXIS_NONE);
            free(name);
            name = NULL;
            if (ctxt->error) return;
            if (CUR == '/' && NXT(1) == '/')
            {
                PUSH(XSLT_OP_ANCESTOR, NULL, NULL, novar);
                NEXT;
                NEXT;
                SKIP_BLANKS;
                xslt_compile_relative_path_pattern(ctxt, NULL, novar);
            }
            else if (CUR == '/')
            {
                PUSH(XSLT_OP_PARENT, NULL, NULL, novar);
                NEXT;
                SKIP_BLANKS;
                xslt_compile_relative_path_pattern(ctxt, NULL, novar);
            }
            return;
        }
        xslt_compile_relative_path_pattern(ctxt, name, novar);
    }
error:
    return;
}

/** Port of xsltCompilePatternInternal. */
static XmlXsltPattern *xslt_compile_pattern_internal(const char *pattern, XMQNode *node, XmlXsltStylesheet *style,
                                                     XmlXsltTransform *runtime, int novar)
{
    XmlXsltPattern *element, *first = NULL, *previous = NULL;
    int current, start, end, level;

    if (!pattern)
    {
        xslt_settings_error(style ? &style->settings : NULL, node, "xsltCompilePattern : NULL pattern\n");
        return NULL;
    }

    XsltParserContext pctx;
    memset(&pctx, 0, sizeof(pctx));
    XsltParserContext *ctxt = &pctx;
    ctxt->style = style;
    ctxt->ctxt = runtime;
    ctxt->elem = node;
    current = end = 0;
    while (pattern[current] != 0)
    {
        start = current;
        while (xslt_is_blank((unsigned char)pattern[current])) current++;
        end = current;
        level = 0;
        while (pattern[end] != 0 && (pattern[end] != '|' || level != 0))
        {
            if (pattern[end] == '[')
            {
                level++;
            }
            else if (pattern[end] == ']')
            {
                level--;
            }
            else if (pattern[end] == '\'')
            {
                end++;
                while (pattern[end] != 0 && pattern[end] != '\'') end++;
            }
            else if (pattern[end] == '"')
            {
                end++;
                while (pattern[end] != 0 && pattern[end] != '"') end++;
            }
            if (pattern[end] == 0) break;
            end++;
        }
        if (current == end)
        {
            xslt_settings_error(style ? &style->settings : NULL, node, "xsltCompilePattern : NULL pattern\n");
            goto error;
        }
        element = xslt_new_comp_match();
        if (!first) first = element;
        else if (previous) previous->next = element;
        previous = element;

        ctxt->comp = element;
        ctxt->base = xslt_strndup(&pattern[start], (size_t)(end - start));
        ctxt->cur = &ctxt->base[current - start];
        element->pattern = (char*)ctxt->base;
        element->node = node;
        element->ns_list = node ? xslt_ns_list(node, &element->ns_nr) : NULL;

        // Preset default priority to be zero, may be changed by the location path.
        element->priority = 0;
        xslt_compile_location_path_pattern(ctxt, novar);
        if (ctxt->error)
        {
            xslt_transform_error(NULL, style, node, "xsltCompilePattern : failed to compile '%s'\n", element->pattern);
            if (style) style->errors++;
            goto error;
        }

        // Reverse for faster interpretation.
        xslt_reverse_comp_match(ctxt, element);

        // Set-up the priority.
        if (element->priority == 0)
        {
            XsltStepOp *s0 = &element->steps[0];
            XsltStepOp *s1 = &element->steps[1];
            if ((s0->op == XSLT_OP_ELEM || s0->op == XSLT_OP_ATTR || s0->op == XSLT_OP_PI) &&
                s0->value && s1->op == XSLT_OP_END)
            {
                ; // Previously preset.
            }
            else if (s0->op == XSLT_OP_ATTR && s0->value2 && s1->op == XSLT_OP_END)
            {
                element->priority = -0.25;
            }
            else if (s0->op == XSLT_OP_NS && s0->value && s1->op == XSLT_OP_END)
            {
                element->priority = -0.25;
            }
            else if (s0->op == XSLT_OP_ATTR && !s0->value && !s0->value2 && s1->op == XSLT_OP_END)
            {
                element->priority = -0.5;
            }
            else if ((s0->op == XSLT_OP_PI || s0->op == XSLT_OP_TEXT || s0->op == XSLT_OP_ALL ||
                      s0->op == XSLT_OP_NODE || s0->op == XSLT_OP_COMMENT) && s1->op == XSLT_OP_END)
            {
                element->priority = -0.5;
            }
            else
            {
                element->priority = 0.5;
            }
        }
        if (pattern[end] == '|') end++;
        current = end;
    }
    if (end == 0)
    {
        xslt_transform_error(NULL, style, node, "xsltCompilePattern : NULL pattern\n");
        if (style) style->errors++;
        goto error;
    }
    return first;

error:
    xslt_free_comp_match_list(first);
    return NULL;
}

XmlXsltPattern *xml_xslt_pattern_compile(XmlXsltStylesheet *style, const char *pattern, XMQNode *element, bool no_variables)
{
    return xslt_compile_pattern_internal(pattern, element, style, NULL, no_variables ? 1 : 0);
}

#undef CUR
#undef NXT
#undef NEXT
#undef SKIP_BLANKS
#undef PUSH
#undef SWAP

// Template registration and lookup (pattern.c xsltAddTemplate, xsltGetTemplate).

/** The key of the template hash, like xmlHashAddEntry3 with name, mode and mode uri. */
static char *xslt_hash3_key(const char *a, const char *b, const char *c)
{
    MemBuffer *mb = new_membuffer();
    const char *parts[3] = { a, b, c };
    for (int i = 0; i < 3; ++i)
    {
        if (i) membuffer_append_char(mb, 1);
        if (parts[i]) membuffer_append(mb, parts[i]);
        else membuffer_append_char(mb, 2);
    }
    membuffer_append_null(mb);
    return free_membuffer_but_return_trimmed_content(mb);
}

/** Insert pat into a priority ordered list (equal priorities: the later one first). */
static void xslt_insert_by_priority(XmlXsltPattern **top, XmlXsltPattern *pat)
{
    XmlXsltPattern *list = *top;
    if (!list)
    {
        *top = pat;
        pat->next = NULL;
    }
    else if (list->priority <= pat->priority)
    {
        pat->next = list;
        *top = pat;
    }
    else
    {
        while (list->next)
        {
            if (list->next->priority <= pat->priority) break;
            list = list->next;
        }
        pat->next = list->next;
        list->next = pat;
    }
}

/** Port of xsltAddTemplate. */
static int xslt_add_template(XmlXsltStylesheet *style, XsltTemplate *cur, const char *mode, const char *mode_uri)
{
    if (!style || !cur) return -1;
    if (cur->next) cur->position = cur->next->position + 1;

    // Register a named template.
    if (cur->name)
    {
        if (!style->named_templates) style->named_templates = hashmap_create(64);
        char *key = xslt_hash3_key(cur->name, cur->name_uri, NULL);
        if (hashmap_get(style->named_templates, key))
        {
            free(key);
            xslt_transform_error(NULL, style, cur->elem, "xsl:template: error duplicate name '%s'\n", cur->name);
            style->errors++;
            return -1;
        }
        hashmap_put(style->named_templates, key, cur);
        free(key);
    }

    if (!cur->match)
    {
        if (!cur->name)
        {
            xslt_transform_error(NULL, style, cur->elem, "xsl:template: need to specify match or name attribute\n");
            style->errors++;
            return -1;
        }
        return 0;
    }

    float priority = cur->priority;
    XmlXsltPattern *pat = xslt_compile_pattern_internal(cur->match, cur->elem, style, NULL, 1);
    if (!pat) return -1;
    while (pat)
    {
        XmlXsltPattern *next = pat->next;
        pat->next = NULL;
        const char *name = NULL;
        XmlXsltPattern **top = NULL;

        pat->templ = cur;
        if (mode) pat->mode = xslt_strdup(mode);
        if (mode_uri) pat->mode_uri = xslt_strdup(mode_uri);
        if (priority != XSLT_PAT_NO_PRIORITY) pat->priority = priority;

        // Insert it in the hash table list corresponding to its lookup name.
        switch (pat->steps[0].op)
        {
        case XSLT_OP_ATTR:
            if (pat->steps[0].value) name = pat->steps[0].value;
            else top = &style->attr_match;
            break;
        case XSLT_OP_PARENT:
        case XSLT_OP_ANCESTOR:
            top = &style->elem_match;
            break;
        case XSLT_OP_ROOT:
            top = &style->root_match;
            break;
        case XSLT_OP_KEY:
            top = &style->key_match;
            break;
        case XSLT_OP_ID:
        case XSLT_OP_NS:
        case XSLT_OP_ALL:
            top = &style->elem_match;
            break;
        case XSLT_OP_END:
        case XSLT_OP_PREDICATE:
            xslt_transform_error(NULL, style, NULL, "xsltAddTemplate: invalid compiled pattern\n");
            xslt_free_comp_match(pat);
            xslt_free_comp_match_list(next);
            return -1;
        case XSLT_OP_PI:
            if (pat->steps[0].value) name = pat->steps[0].value;
            else top = &style->pi_match;
            break;
        case XSLT_OP_COMMENT:
            top = &style->comment_match;
            break;
        case XSLT_OP_TEXT:
            top = &style->text_match;
            break;
        case XSLT_OP_ELEM:
        case XSLT_OP_NODE:
            if (pat->steps[0].value) name = pat->steps[0].value;
            else top = &style->elem_match;
            break;
        }
        if (name)
        {
            if (!style->templates_hash) style->templates_hash = hashmap_create(1024);
            char *key = xslt_hash3_key(name, mode, mode_uri);
            XmlXsltPattern *list = (XmlXsltPattern*)hashmap_get(style->templates_hash, key);
            if (!list)
            {
                hashmap_put(style->templates_hash, key, pat);
            }
            else
            {
                // Note <= since one must choose, among the matching template rules that are
                // left, the one that occurs last in the stylesheet.
                XmlXsltPattern *head = list;
                xslt_insert_by_priority(&head, pat);
                if (head != list) hashmap_put(style->templates_hash, key, head);
            }
            free(key);
        }
        else if (top)
        {
            xslt_insert_by_priority(top, pat);
        }
        pat = next;
    }
    return 0;
}

/** Port of xsltNextImport (imports.c). */
static XmlXsltStylesheet *xslt_next_import(XmlXsltStylesheet *cur)
{
    if (!cur) return NULL;
    if (cur->imports) return cur->imports;
    if (cur->next) return cur->next;
    do
    {
        cur = cur->parent;
        if (!cur) break;
        if (cur->next) return cur->next;
    } while (cur);
    return cur;
}

static XmlXsltPattern *xslt_first_match(XmlXsltTransform *t, XmlXsltPattern *list, XMQNode *node,
                                        XsltTemplate **ret, float *priority)
{
    while (list &&
           (!*ret || list->priority > *priority ||
            (list->priority == *priority && list->templ->position > (*ret)->position)))
    {
        if (xslt_test_comp_match(t, list, node, t->mode, t->mode_uri) == 1)
        {
            *ret = list->templ;
            *priority = list->priority;
            break;
        }
        list = list->next;
    }
    return list;
}

/** Port of xsltGetTemplate: the template for the node, with style the next import in scope. */
static XsltTemplate *xslt_get_template(XmlXsltTransform *t, XMQNode *node, XmlXsltStylesheet *style)
{
    if (!t || !node) return NULL;
    XmlXsltStylesheet *curstyle = style ? xslt_next_import(style) : t->style;
    while (curstyle && curstyle != style)
    {
        float priority = XSLT_PAT_NO_PRIORITY;
        XsltTemplate *ret = NULL;
        const char *name = NULL;
        XmlNodeKind k = xml_node_kind(node);
        if (curstyle->templates_hash)
        {
            // Use the top name as selector.
            if (k == XML_DOM_ELEMENT || k == XML_DOM_ATTRIBUTE || k == XML_DOM_PI) name = xml_node_name(node);
        }
        if (name)
        {
            // The list of applicable expressions based on the name.
            char *key = xslt_hash3_key(name, t->mode, t->mode_uri);
            XmlXsltPattern *list = (XmlXsltPattern*)hashmap_get(curstyle->templates_hash, key);
            free(key);
            while (list)
            {
                if (xslt_test_comp_match(t, list, node, t->mode, t->mode_uri) == 1)
                {
                    ret = list->templ;
                    priority = list->priority;
                    break;
                }
                list = list->next;
            }
        }

        // Alternate generic matches.
        XmlXsltPattern *list = NULL;
        switch (k)
        {
        case XML_DOM_ELEMENT: list = curstyle->elem_match; break;
        case XML_DOM_ATTRIBUTE: list = curstyle->attr_match; break;
        case XML_DOM_PI: list = curstyle->pi_match; break;
        case XML_DOM_DOCUMENT: list = curstyle->root_match; break;
        case XML_DOM_TEXT:
        case XML_DOM_CDATA: list = curstyle->text_match; break;
        case XML_DOM_COMMENT: list = curstyle->comment_match; break;
        default: break;
        }
        xslt_first_match(t, list, node, &ret, &priority);
        // Some of the tests for elements can also apply to documents.
        if (k == XML_DOM_DOCUMENT || k == XML_DOM_TEXT || k == XML_DOM_PI || k == XML_DOM_COMMENT)
        {
            xslt_first_match(t, curstyle->elem_match, node, &ret, &priority);
        }
        // TODO (step 7d): keyed matches, like libxslt XSLT_SOURCE_NODE_HAS_KEY and xsltComputeAllKeys.
        if (ret) return ret;
        // Cycle on the next stylesheet import.
        curstyle = xslt_next_import(curstyle);
    }
    return NULL;
}

static void xslt_free_template_hashes(XmlXsltStylesheet *style)
{
    if (style->templates_hash)
    {
        HashMapIterator *i = hashmap_iterate(style->templates_hash);
        const char *key;
        void *val;
        while (hashmap_next_key_value(i, &key, &val)) xslt_free_comp_match_list((XmlXsltPattern*)val);
        hashmap_free_iterator(i);
        hashmap_free(style->templates_hash);
    }
    xslt_free_comp_match_list(style->root_match);
    xslt_free_comp_match_list(style->key_match);
    xslt_free_comp_match_list(style->elem_match);
    xslt_free_comp_match_list(style->attr_match);
    xslt_free_comp_match_list(style->parent_match);
    xslt_free_comp_match_list(style->text_match);
    xslt_free_comp_match_list(style->pi_match);
    xslt_free_comp_match_list(style->comment_match);
    if (style->named_templates) hashmap_free(style->named_templates);
}

// Stylesheets and transformations ///////////////////////////////

XmlXsltStylesheet *xml_xslt_stylesheet_new_empty(XmlDom *style_dom, const XmlXsltSettings *settings)
{
    XmlXsltStylesheet *style = (XmlXsltStylesheet*)calloc(1, sizeof(XmlXsltStylesheet));
    if (settings) style->settings = *settings;
    style->doc = style_dom;
    return style;
}

static void xslt_free_templates(XsltTemplate *t)
{
    while (t)
    {
        XsltTemplate *next = t->next;
        free(t->match);
        free(t->name);
        free(t->name_uri);
        free(t->mode);
        free(t->mode_uri);
        free(t);
        t = next;
    }
}

void xml_xslt_stylesheet_free(XmlXsltStylesheet *style)
{
    if (!style) return;
    xslt_free_template_hashes(style);
    xslt_free_templates(style->templates);
    free(style);
}

XmlXsltTransform *xml_xslt_transform_new(XmlXsltStylesheet *style, XmlDom *source)
{
    XmlXsltTransform *t = (XmlXsltTransform*)calloc(1, sizeof(XmlXsltTransform));
    t->style = style;
    t->source = source;
    t->xpath = xml_xpath_context_new();
    // Like a fresh libxml2 xpath context, the position and size are unknown.
    t->xp_position = -1;
    t->xp_size = -1;
    t->extras_max = style->extras_nr + 20;
    t->extras = (XsltRuntimeExtra*)calloc((size_t)t->extras_max, sizeof(XsltRuntimeExtra));
    t->extras_nr = style->extras_nr;
    return t;
}

void xml_xslt_transform_free(XmlXsltTransform *t)
{
    if (!t) return;
    for (int i = 0; i < t->extras_nr; ++i)
    {
        if (t->extras[i].deallocate && t->extras[i].info) t->extras[i].deallocate(t->extras[i].info);
    }
    free(t->extras);
    xml_xpath_context_free(t->xpath);
    free(t->rtfs);
    free(t);
}

// Not used yet, until the stylesheet compiler registers templates (step 7c).
void xml_xslt_unused_(void);
void xml_xslt_unused_(void)
{
    (void)xslt_add_template;
    (void)xslt_get_template;
}

#endif // XML_XSLT_MODULE
