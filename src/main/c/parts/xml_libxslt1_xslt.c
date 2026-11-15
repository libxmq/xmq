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
#include"xml_libxslt1_xslt.h"
#include"xml_dom.h"
#include"xml_parser.h"
#include"xml_xpath.h"
#include"xml_uri.h"
#include"xml_chars.h"
#include"hashmap.h"
#include"membuffer.h"

#include<errno.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_LIBXSLT1_XSLT_MODULE

// The ext marker of the precomputed instructions (XSLT_EXT_MARKER).
char xslt1_ext_marker_;

// Errors /////////////////////////////////////////////////////////

static void xslt_emit(const XmlLibxslt1Settings *s, const char *msg)
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

static char *xslt_format(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *m = xslt_vformat(fmt, ap);
    va_end(ap);
    return m;
}

/** Like xsltGenericError, the message without context. */
void xslt1_generic_error(const XmlLibxslt1Settings *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *m = xslt_vformat(fmt, ap);
    va_end(ap);
    xslt_emit(s, m);
    free(m);
}

/** Port of xsltPrintErrorContext. */
/** Port of xmlGetLineNo (without XML_PARSE_BIG_LINES the lines are 16 bits): attributes and
    other nodes without lines use a neighbour or the parent. -1 if unknown. */
static long xslt_libxml2_line_no(const XMQNode *node, int depth)
{
    if (depth >= 5 || !node) return -1;
    XmlNodeKind k = xml_node_kind(node);
    if (k == XML_DOM_ELEMENT || k == XML_DOM_TEXT || k == XML_DOM_COMMENT || k == XML_DOM_PI)
    {
        long line = xml_node_line(node);
        return line > 65535 ? 65535 : line;
    }
    const XMQNode *prev = k == XML_DOM_ATTRIBUTE ? NULL : xml_node_prev(node);
    if (prev)
    {
        XmlNodeKind pk = xml_node_kind(prev);
        if (pk == XML_DOM_ELEMENT || pk == XML_DOM_TEXT || pk == XML_DOM_COMMENT || pk == XML_DOM_PI)
            return xslt_libxml2_line_no(prev, depth + 1);
    }
    const XMQNode *parent = xml_node_parent(node);
    if (parent && xml_node_kind(parent) == XML_DOM_ELEMENT) return xslt_libxml2_line_no(parent, depth + 1);
    return -1;
}

/** The name libxml2 gives a node (node->name): text and comment nodes have names too. */
const char *xslt1_libxml2_node_name(const XMQNode *node)
{
    const char *name = xml_node_name(node);
    XmlNodeKind k = xml_node_kind(node);
    if (k == XML_DOM_TEXT && !name) return "text";
    if (k == XML_DOM_COMMENT) return "comment";
    if (k == XML_DOM_CDATA) return NULL;
    return name;
}

static void xslt_print_error_context(const XmlLibxslt1Settings *s, XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style, XMQNode *node)
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
            line = xslt_libxml2_line_no(node, 0);
            file = xml_dom_url(xml_node_dom(node));
            name = xslt1_libxml2_node_name(node);
        }
    }
    if (t) type = "runtime error";
    else if (style) type = "compilation error";

    if (file && line && name) xslt1_generic_error(s, "%s: file %s line %d element %s\n", type, file, line, name);
    else if (file && name) xslt1_generic_error(s, "%s: file %s element %s\n", type, file, name);
    else if (file && line) xslt1_generic_error(s, "%s: file %s line %d\n", type, file, line);
    else if (file) xslt1_generic_error(s, "%s: file %s\n", type, file);
    else if (name) xslt1_generic_error(s, "%s: element %s\n", type, name);
    else xslt1_generic_error(s, "%s\n", type);
}

const XmlLibxslt1Settings *xslt1_settings_of(XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style)
{
    if (t) return &t->style->settings;
    if (style) return &style->settings;
    return NULL;
}

static void xslt_vtransform_error(const XmlLibxslt1Settings *s, XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style, XMQNode *node,
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
void xslt1_transform_error(XmlLibxslt1Transform *t, XmlLibxslt1Stylesheet *style, XMQNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xslt_vtransform_error(xslt1_settings_of(t, style), t, style, node, fmt, ap);
    va_end(ap);
}

/** xsltTransformError without transformation or stylesheet, like libxslt (the error context
    is then just "error"), but reported through these settings. */
void xslt1_settings_error(const XmlLibxslt1Settings *s, XMQNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xslt_vtransform_error(s, NULL, NULL, node, fmt, ap);
    va_end(ap);
}

void xml_libxslt1_settings_default(XmlLibxslt1Settings *settings)
{
    memset(settings, 0, sizeof(*settings));
}

// Helpers ////////////////////////////////////////////////////////

char *xslt1_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char*)malloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

char *xslt1_strndup(const char *s, size_t n)
{
    char *r = (char*)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

bool xslt1_streq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return !strcmp(a, b);
}

bool xslt1_is_blank(int c)
{
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
}

/** Port of xsltGetUTF8CharZ: one UTF-8 char from a nul terminated string, -1 on error. */
int xslt1_utf8_char(const char *s, int *len)
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
XmlXPathNamespace *xslt1_ns_list(XMQNode *node, size_t *num)
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
            for (size_t i = 0; i < *num; ++i) if (xslt1_streq(list[i].prefix, prefix)) seen = true;
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
const char *xslt1_get_qname_uri(const XmlLibxslt1Settings *s, XMQNode *node, char **name)
{
    char *qname = *name;
    if (!qname || !*qname) return NULL;
    if (!node)
    {
        xslt1_generic_error(s, "QName: no element for namespace lookup %s\n", qname);
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
        *name = xslt1_strdup(&qname[4]);
        free(qname);
        return XML_DOM_XML_NAMESPACE;
    }
    qname[len] = 0;
    XmlNs *ns = xml_ns_lookup_prefix(node, qname);
    if (!ns)
    {
        xslt1_generic_error(s, "%s:%s : no namespace bound to prefix %s\n", qname, &qname[len + 1], qname);
        *name = NULL;
        free(qname);
        return NULL;
    }
    *name = xslt1_strdup(&qname[len + 1]);
    free(qname);
    return xml_ns_href(ns);
}

/** Report an xpath error like libxml2 xmlXPathErr with the default handler: the message,
    then the expression and a marker when the position is inside it. */
void xslt1_report_xpath_error(const XmlLibxslt1Settings *s, const char *expr, const XmlXPathError *err)
{
    if (err->quiet) return;
    xslt1_generic_error(s, "XPath error : %s\n", err->detail[0] ? err->detail : err->message);
    // libxml2 reports errors of compiled expressions without the expression.
    if (err->evaluating) return;
    size_t len = expr ? strlen(expr) : 0;
    if (expr && err->offset < 100 && err->offset < len)
    {
        char buf[150];
        size_t i;
        for (i = 0; i < err->offset; i++) buf[i] = ' ';
        buf[i++] = '^';
        buf[i] = 0;
        xslt1_generic_error(s, "%s\n%s\n", expr, buf);
    }
}

XmlXPathComp *xslt1_xpath_compile(const XmlLibxslt1Settings *s, XMQNode *elem, const char *str, bool no_variables)
{
    XmlXPathSettings st;
    xml_xpath_settings_default(&st);
    size_t nn = 0;
    XmlXPathNamespace *nss = elem ? xslt1_ns_list(elem, &nn) : NULL;
    st.namespaces = nss;
    st.num_namespaces = nn;
    st.no_variables = no_variables;
    XmlXPathError err;
    XmlXPathComp *comp = xml_xpath_compile(str, &st, &err);
    if (!comp) xslt1_report_xpath_error(s, str, &err);
    free(nss);
    return comp;
}


static int xslt_allocate_extra(XmlLibxslt1Stylesheet *style)
{
    return style->extras_nr++;
}

/** Port of xsltAllocateExtraCtxt. */
int xslt1_allocate_extra_ctxt(XmlLibxslt1Transform *t)
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

struct XmlLibxslt1Pattern
{
    XmlLibxslt1Pattern *next;   // Siblings in the name hash.
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
    XmlLibxslt1Stylesheet *style;
    XmlLibxslt1Transform *ctxt; // The transformation or NULL.
    const char *cur;
    const char *base;
    XMQNode *elem;
    int error;
    XmlLibxslt1Pattern *comp;
} XsltParserContext;

static const XmlLibxslt1Settings *xslt_pctx_settings(XsltParserContext *ctxt)
{
    return ctxt->style ? &ctxt->style->settings : NULL;
}

static XmlLibxslt1Pattern *xslt_new_comp_match(void)
{
    XmlLibxslt1Pattern *cur = (XmlLibxslt1Pattern*)calloc(1, sizeof(XmlLibxslt1Pattern));
    cur->max_step = 10;
    cur->steps = (XsltStepOp*)calloc((size_t)cur->max_step, sizeof(XsltStepOp));
    return cur;
}

static void xslt_free_comp_match(XmlLibxslt1Pattern *comp)
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

static void xslt_free_comp_match_list(XmlLibxslt1Pattern *comp)
{
    while (comp)
    {
        XmlLibxslt1Pattern *cur = comp;
        comp = comp->next;
        xslt_free_comp_match(cur);
    }
}

void xml_libxslt1_pattern_free(XmlLibxslt1Pattern *pattern)
{
    xslt_free_comp_match_list(pattern);
}

double xml_libxslt1_pattern_priority(const XmlLibxslt1Pattern *pattern)
{
    return pattern ? pattern->priority : 0;
}

/** Port of xsltCompMatchAdd. Takes ownership of value and value2. */
static int xslt_comp_match_add(XsltParserContext *ctxt, XmlLibxslt1Pattern *comp, XsltOp op, char *value, char *value2, int novar)
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
        s->previous_extra = xslt1_allocate_extra_ctxt(ctxt->ctxt);
        s->index_extra = xslt1_allocate_extra_ctxt(ctxt->ctxt);
        s->len_extra = xslt1_allocate_extra_ctxt(ctxt->ctxt);
    }
    else
    {
        s->previous_extra = xslt_allocate_extra(ctxt->style);
        s->index_extra = xslt_allocate_extra(ctxt->style);
        s->len_extra = xslt_allocate_extra(ctxt->style);
    }
    if (op == XSLT_OP_PREDICATE)
    {
        s->comp = xslt1_xpath_compile(xslt_pctx_settings(ctxt), ctxt->elem, value, novar != 0);
        if (!s->comp)
        {
            xslt1_transform_error(NULL, ctxt->style, ctxt->elem, "Failed to compile predicate\n");
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
static void xslt_swap_top_comp_match(XmlLibxslt1Pattern *comp)
{
    int j = comp->nb_step - 1;
    if (j > 0) xslt_swap_steps(&comp->steps[j - 1], &comp->steps[j]);
}

/** Port of xsltReverseCompMatch. */
static void xslt_reverse_comp_match(XsltParserContext *ctxt, XmlLibxslt1Pattern *comp)
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

static int xslt_pat_push_state(XmlLibxslt1Transform *t, XsltStepStates *states, int step, XMQNode *node)
{
    if (states->maxstates <= states->nbstates)
    {
        int nm = states->maxstates == 0 ? 4 : 2 * states->maxstates;
        XsltStepState *tmp = (XsltStepState*)realloc(states->states, (size_t)nm * sizeof(XsltStepState));
        if (!tmp)
        {
            xslt1_generic_error(&t->style->settings, "xsltPatPushState: memory re-allocation failure.\n");
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
static XmlXPathResult *xslt_eval_with_ns(XmlLibxslt1Transform *t, const char *expr, XmlXPathNamespace *ns_list, size_t ns_nr,
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
static int xslt_test_comp_match_direct(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *comp, XMQNode *node,
                                       XmlXPathNamespace *ns_list, size_t ns_nr)
{
    XmlDom *doc = xml_node_dom(node);
    bool is_rvt = xslt1_is_rtf(t, doc);
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

/** Port of xsltTestStepMatch. Returns 1 if the node matches the step, 0 if not, -1 on an error. */
static int xslt_test_step_match(XmlLibxslt1Transform *t, XMQNode *node, XsltStepOp *step)
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
            if (!xslt1_streq(step->value2, xml_ns_href(ns))) return 0;
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
        XmlXPathResult *list = xslt1_get_key(t, step->value, step->value3, step->value2);
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
        if (step->value && !xslt1_streq(step->value, xml_node_name(node))) return 0;
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
        xslt1_transform_error(t, NULL, node, "xsltTestStepMatch: unexpected step op %d\n", step->op);
        return -1;
    }
    return 1;
}

/** Port of xsltEvalXPathPredicate (templates.c): a number is compared with the context position. */
static int xslt_eval_xpath_predicate(XmlLibxslt1Transform *t, XmlXPathComp *comp)
{
    if (!t->inst)
    {
        xslt1_transform_error(t, NULL, NULL, "xsltEvalXPathPredicate: No context or instruction\n");
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
static int xslt_test_predicate_match(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *comp, XMQNode *node, XsltStepOp *step, XsltStepOp *sel)
{
    (void)comp;
    int pos = 0, len = 0;
    if (!step->value || !step->comp || !sel) return 0;

    XmlDom *doc = xml_node_dom(node);
    bool is_rvt = xslt1_is_rtf(t, doc);

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
static int xslt_test_comp_match(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *comp, XMQNode *match_node,
                                const char *mode, const char *mode_uri)
{
    int i;
    int found = 0;
    XMQNode *node = match_node;
    XsltStepOp *step, *sel = NULL;
    XsltStepStates states = { 0, 0, NULL };

    if (!comp || !node || !t)
    {
        xslt1_transform_error(t, NULL, node, "xsltTestCompMatch: null arg\n");
        return -1;
    }
    if (!xslt1_streq(comp->mode, mode)) return 0;
    if (!xslt1_streq(comp->mode_uri, mode_uri)) return 0;

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

int xml_libxslt1_pattern_match(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Pattern *comp)
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

/** Port of xsltCompMatchClearCache: drop the cached node list of a direct match. */
void xslt1_comp_match_clear_cache(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *comp)
{
    if (!t || !comp || comp->nb_step == 0) return;
    XsltStepOp *sel = &comp->steps[0];
    XmlXPathResult *list = (XmlXPathResult*)XSLT_RUNTIME_EXTRA(t, sel->len_extra).info;
    if (list)
    {
        xml_xpath_result_free(list);
        XSLT_RUNTIME_EXTRA(t, sel->len_extra).info = NULL;
        XSLT_RUNTIME_EXTRA(t, sel->previous_extra).val.ptr = NULL;
        XSLT_RUNTIME_EXTRA(t, sel->index_extra).val.ival = 0;
        XSLT_RUNTIME_EXTRA(t, sel->len_extra).deallocate = NULL;
    }
}

// The pattern parser (xsltCompileLocationPathPattern etc).

#define CUR (*ctxt->cur)
#define NXT(val) ctxt->cur[(val)]
#define NEXT (*ctxt->cur ? ctxt->cur++ : ctxt->cur)
#define SKIP_BLANKS while (xslt1_is_blank((unsigned char)CUR)) NEXT
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
    int val = xslt1_utf8_char(cur, &len);
    while (xslt_is_char(val) && val != q)
    {
        cur += len;
        val = xslt1_utf8_char(cur, &len);
    }
    if (!xslt_is_char(val))
    {
        ctxt->error = 1;
        return NULL;
    }
    char *ret = xslt1_strndup(start, (size_t)(cur - start));
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
    int val = xslt1_utf8_char(cur, &len);
    if (val < 0 || (!xml_char_is_base_char(val) && !xml_char_is_ideographic(val) && val != '_')) return NULL;
    while (val >= 0 && (xml_char_is_base_char(val) || xml_char_is_ideographic(val) || xml_char_is_digit(val) ||
                        val == '.' || val == '-' || val == '_' || xml_char_is_combining(val) || xml_char_is_extender(val)))
    {
        cur += len;
        val = xslt1_utf8_char(cur, &len);
    }
    char *ret = xslt1_strndup(q, (size_t)(cur - q));
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
        xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ( expected\n");
        ctxt->error = 1;
        return;
    }
    if (aid && !strcmp(name, "id"))
    {
        if (axis != 0)
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : NodeTest expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : NodeTest expected\n");
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ',')
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : , expected\n");
            free(lit);
            ctxt->error = 1;
            return;
        }
        NEXT;
        SKIP_BLANKS;
        lit2 = xslt_scan_literal(ctxt);
        if (ctxt->error)
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
            free(lit);
            return;
        }
        SKIP_BLANKS;
        if (CUR != ')')
        {
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
                xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : Literal expected\n");
                free(lit);
                return;
            }
            SKIP_BLANKS;
            if (CUR != ')')
            {
                xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : ) expected\n");
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
        xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : expecting 'key' or 'id' or node type\n");
        ctxt->error = 1;
        return;
    }
    else
    {
        xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileIdKeyPattern : node type\n");
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : Name expected\n");
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
                xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : no namespace bound to prefix %s\n", prefix);
                free(prefix);
                ctxt->error = 1;
                goto error;
            }
            url = xslt1_strdup(xml_ns_href(ns));
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
                    xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : Name expected\n");
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
                xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : NodeTest expected\n");
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
                xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : 'child' or 'attribute' expected\n");
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
        const char *uri = xslt1_get_qname_uri(xslt_pctx_settings(ctxt), ctxt->elem, &token);
        if (!token)
        {
            ctxt->error = 1;
            goto error;
        }
        if (uri) url = xslt1_strdup(uri);
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileStepPattern : ']' expected\n");
            ctxt->error = 1;
            return;
        }
        ret = xslt1_strndup(q, (size_t)(ctxt->cur - q));
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
            xslt1_settings_error(xslt_pctx_settings(ctxt), NULL, "xsltCompileLocationPathPattern : Name expected\n");
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
static XmlLibxslt1Pattern *xslt_compile_pattern_internal(const char *pattern, XMQNode *node, XmlLibxslt1Stylesheet *style,
                                                     XmlLibxslt1Transform *runtime, int novar)
{
    XmlLibxslt1Pattern *element, *first = NULL, *previous = NULL;
    int current, start, end, level;

    if (!pattern)
    {
        xslt1_settings_error(style ? &style->settings : NULL, node, "xsltCompilePattern : NULL pattern\n");
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
        while (xslt1_is_blank((unsigned char)pattern[current])) current++;
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
            xslt1_settings_error(style ? &style->settings : NULL, node, "xsltCompilePattern : NULL pattern\n");
            goto error;
        }
        element = xslt_new_comp_match();
        if (!first) first = element;
        else if (previous) previous->next = element;
        previous = element;

        ctxt->comp = element;
        ctxt->base = xslt1_strndup(&pattern[start], (size_t)(end - start));
        ctxt->cur = &ctxt->base[current - start];
        element->pattern = (char*)ctxt->base;
        element->node = node;
        element->ns_list = node ? xslt1_ns_list(node, &element->ns_nr) : NULL;

        // Preset default priority to be zero, may be changed by the location path.
        element->priority = 0;
        xslt_compile_location_path_pattern(ctxt, novar);
        if (ctxt->error)
        {
            xslt1_transform_error(NULL, style, node, "xsltCompilePattern : failed to compile '%s'\n", element->pattern);
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
        xslt1_transform_error(NULL, style, node, "xsltCompilePattern : NULL pattern\n");
        if (style) style->errors++;
        goto error;
    }
    return first;

error:
    xslt_free_comp_match_list(first);
    return NULL;
}

XmlLibxslt1Pattern *xml_libxslt1_pattern_compile(XmlLibxslt1Stylesheet *style, const char *pattern, XMQNode *element, bool no_variables)
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
char *xslt1_hash3_key(const char *a, const char *b, const char *c)
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
static void xslt_insert_by_priority(XmlLibxslt1Pattern **top, XmlLibxslt1Pattern *pat)
{
    XmlLibxslt1Pattern *list = *top;
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
static int xslt_add_template(XmlLibxslt1Stylesheet *style, XsltTemplate *cur, const char *mode, const char *mode_uri)
{
    if (!style || !cur) return -1;
    if (cur->next) cur->position = cur->next->position + 1;

    // Register a named template.
    if (cur->name)
    {
        if (!style->named_templates) style->named_templates = hashmap_create(64);
        char *key = xslt1_hash3_key(cur->name, cur->name_uri, NULL);
        if (hashmap_get(style->named_templates, key))
        {
            free(key);
            xslt1_transform_error(NULL, style, cur->elem, "xsl:template: error duplicate name '%s'\n", cur->name);
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
            xslt1_transform_error(NULL, style, cur->elem, "xsl:template: need to specify match or name attribute\n");
            style->errors++;
            return -1;
        }
        return 0;
    }

    float priority = cur->priority;
    XmlLibxslt1Pattern *pat = xslt_compile_pattern_internal(cur->match, cur->elem, style, NULL, 1);
    if (!pat) return -1;
    while (pat)
    {
        XmlLibxslt1Pattern *next = pat->next;
        pat->next = NULL;
        const char *name = NULL;
        XmlLibxslt1Pattern **top = NULL;

        pat->templ = cur;
        if (mode) pat->mode = xslt1_strdup(mode);
        if (mode_uri) pat->mode_uri = xslt1_strdup(mode_uri);
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
            xslt1_transform_error(NULL, style, NULL, "xsltAddTemplate: invalid compiled pattern\n");
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
            char *key = xslt1_hash3_key(name, mode, mode_uri);
            XmlLibxslt1Pattern *list = (XmlLibxslt1Pattern*)hashmap_get(style->templates_hash, key);
            if (!list)
            {
                hashmap_put(style->templates_hash, key, pat);
            }
            else
            {
                // Note <= since one must choose, among the matching template rules that are
                // left, the one that occurs last in the stylesheet.
                XmlLibxslt1Pattern *head = list;
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
XmlLibxslt1Stylesheet *xslt1_next_import(XmlLibxslt1Stylesheet *cur)
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

static XmlLibxslt1Pattern *xslt_first_match(XmlLibxslt1Transform *t, XmlLibxslt1Pattern *list, XMQNode *node,
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
XsltTemplate *xslt1_get_template(XmlLibxslt1Transform *t, XMQNode *node, XmlLibxslt1Stylesheet *style)
{
    if (!t || !node) return NULL;
    XmlLibxslt1Stylesheet *curstyle = style ? xslt1_next_import(style) : t->style;
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
            char *key = xslt1_hash3_key(name, t->mode, t->mode_uri);
            XmlLibxslt1Pattern *list = (XmlLibxslt1Pattern*)hashmap_get(curstyle->templates_hash, key);
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
        XmlLibxslt1Pattern *list = NULL;
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
    keyed_match:
        if (xslt1_node_has_key(t, node))
        {
            xslt_first_match(t, curstyle->key_match, node, &ret, &priority);
        }
        else if (t->has_templ_key_patterns && (!t->document || t->document->nb_keys_computed < t->nb_keys))
        {
            // Compute all remaining keys for this document.
            if (xslt1_compute_all_keys(t, node) == -1) return NULL;
            if (xslt1_node_has_key(t, node)) goto keyed_match;
        }
        if (ret) return ret;
        // Cycle on the next stylesheet import.
        curstyle = xslt1_next_import(curstyle);
    }
    return NULL;
}

static void xslt_free_template_hashes(XmlLibxslt1Stylesheet *style)
{
    if (style->templates_hash)
    {
        HashMapIterator *i = hashmap_iterate(style->templates_hash);
        const char *key;
        void *val;
        while (hashmap_next_key_value(i, &key, &val)) xslt_free_comp_match_list((XmlLibxslt1Pattern*)val);
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

// Compilation helpers ////////////////////////////////////////////

bool xslt1_is_xslt_elem(const XMQNode *n)
{
    if (!n || xml_node_kind(n) != XML_DOM_ELEMENT) return false;
    XmlNs *ns = xml_node_ns(n);
    return ns && xslt1_streq(xml_ns_href(ns), XSLT_NAMESPACE);
}

bool xslt1_is_xslt_name(const XMQNode *n, const char *name)
{
    return xslt1_streq(xml_node_name(n), name);
}

bool xslt1_is_blank_str(const char *s)
{
    if (!s) return true;
    for (; *s; s++) if (!xslt1_is_blank((unsigned char)*s)) return false;
    return true;
}

static bool xslt_is_blank_node(const XMQNode *n)
{
    return xml_node_kind(n) == XML_DOM_TEXT && xslt1_is_blank_str(xml_node_content(n));
}

static char *xslt_attr_value(XMQAttr *a)
{
    return xml_node_string_value(xml_attr_node(a));
}

/** libxml2 xmlGetNsProp(node, name, ns): the attribute in exactly that namespace (none if ns is NULL). */
char *xslt1_get_ns_prop_exact(XMQNode *node, const char *name, const char *ns)
{
    if (!node || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
    {
        if (strcmp(xml_attr_name(a), name)) continue;
        const char *u = xslt_node_ns_uri(xml_attr_node(a));
        if (u && !*u) u = NULL;
        if (xslt1_streq(u, ns)) return xslt_attr_value(a);
    }
    return NULL;
}

/** libxml2 xmlGetProp: the first attribute with this name, in any namespace. */
static char *xslt_get_prop(XMQNode *node, const char *name)
{
    if (!node || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
    {
        if (!strcmp(xml_attr_name(a), name)) return xslt_attr_value(a);
    }
    return NULL;
}

/** Port of xsltGetNsProp (also xsltGetCNsProp): an attribute in the namespace, or without
    namespace when the element is in that namespace. ns NULL is xmlGetProp. */
char *xslt1_get_ns_prop(XMQNode *node, const char *name, const char *ns)
{
    if (!node) return NULL;
    if (!ns) return xslt_get_prop(node, name);
    if (xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    const char *nuri = xslt_node_ns_uri(node);
    for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
    {
        if (strcmp(xml_attr_name(a), name)) continue;
        XmlNs *an = xml_node_ns(xml_attr_node(a));
        if ((!an && nuri && !strcmp(nuri, ns)) || (an && xslt1_streq(xml_ns_href(an), ns)))
        {
            char *v = xslt_attr_value(a);
            return v ? v : xslt1_strdup("");
        }
    }
    return NULL;
}

/** libxml2 xmlNodeGetSpacePreserve: 1 for preserve, 0 for default, -1 if no xml:space applies. */
static int xslt_get_space_preserve(XMQNode *node)
{
    for (XMQNode *n = node; n; n = xml_node_parent(n))
    {
        if (xml_node_kind(n) != XML_DOM_ELEMENT) continue;
        char *v = xslt1_get_ns_prop_exact(n, "space", XML_DOM_XML_NAMESPACE);
        if (v)
        {
            int r = -1;
            if (!strcmp(v, "preserve")) r = 1;
            else if (!strcmp(v, "default")) r = 0;
            free(v);
            if (r >= 0) return r;
        }
    }
    return -1;
}

static bool xslt_name_start_char(int c)
{
    return xml_char_is_letter(c) || c == '_';
}

static bool xslt_name_char(int c)
{
    return xml_char_is_letter(c) || xml_char_is_digit(c) || c == '.' || c == '-' || c == '_' ||
           xml_char_is_combining(c) || xml_char_is_extender(c);
}

/** libxml2 xmlValidateNCName(value, 0): 0 if valid. */
static int xslt_validate_ncname(const char *value)
{
    if (!value) return -1;
    const char *cur = value;
    int len;
    int c = xslt1_utf8_char(cur, &len);
    if (c < 0 || !xslt_name_start_char(c)) return 1;
    cur += len;
    c = xslt1_utf8_char(cur, &len);
    while (c > 0 && xslt_name_char(c))
    {
        cur += len;
        c = xslt1_utf8_char(cur, &len);
    }
    return c == 0 ? 0 : 1;
}

/** libxml2 xmlValidateQName(value, 0): 0 if valid. */
int xslt1_validate_qname(const char *value)
{
    if (!value) return -1;
    const char *cur = value;
    int len;
    int c = xslt1_utf8_char(cur, &len);
    if (c < 0 || !xslt_name_start_char(c)) return 1;
    cur += len;
    c = xslt1_utf8_char(cur, &len);
    while (c > 0 && xslt_name_char(c))
    {
        cur += len;
        c = xslt1_utf8_char(cur, &len);
    }
    if (c == ':')
    {
        cur += len;
        c = xslt1_utf8_char(cur, &len);
        if (c < 0 || !xslt_name_start_char(c)) return 1;
        cur += len;
        c = xslt1_utf8_char(cur, &len);
        while (c > 0 && xslt_name_char(c))
        {
            cur += len;
            c = xslt1_utf8_char(cur, &len);
        }
    }
    return c == 0 ? 0 : 1;
}

/** Port of xsltSplitQName: the local name, *prefix gets the prefix (both malloced, prefix NULL if none). */
char *xslt1_split_qname(const char *name, char **prefix)
{
    *prefix = NULL;
    if (!name) return NULL;
    if (name[0] == ':') return xslt1_strdup(name);
    size_t len = 0;
    while (name[len] && name[len] != ':') len++;
    if (!name[len]) return xslt1_strdup(name);
    *prefix = xslt1_strndup(name, len);
    return xslt1_strdup(name + len + 1);
}

/** Port of xsltGetQNameURI2: like xsltGetQNameURI but reported on the stylesheet. */
static const char *xslt_get_qname_uri2(XmlLibxslt1Stylesheet *style, XMQNode *node, char **name)
{
    char *qname = *name;
    if (!qname || !*qname) return NULL;
    if (!node)
    {
        xslt1_generic_error(&style->settings, "QName: no element for namespace lookup %s\n", qname);
        free(qname);
        *name = NULL;
        return NULL;
    }
    size_t len = 0;
    while (qname[len] && qname[len] != ':') len++;
    if (!qname[len]) return NULL;
    if (qname[0] == 'x' && qname[1] == 'm' && qname[2] == 'l' && qname[3] == ':')
    {
        if (!qname[4]) return NULL;
        *name = xslt1_strdup(&qname[4]);
        free(qname);
        return XML_DOM_XML_NAMESPACE;
    }
    char *prefix = xslt1_strndup(qname, len);
    XmlNs *ns = xml_ns_lookup_prefix(node, prefix);
    if (!ns)
    {
        if (style)
        {
            xslt1_transform_error(NULL, style, node, "No namespace bound to prefix '%s'.\n", prefix);
            style->errors++;
        }
        else
        {
            xslt1_generic_error(NULL, "%s : no namespace bound to prefix %s\n", qname, prefix);
        }
        free(qname);
        *name = NULL;
        free(prefix);
        return NULL;
    }
    *name = xslt1_strdup(qname + len + 1);
    free(qname);
    free(prefix);
    return xml_ns_href(ns);
}

// URIs, resolved with xml_uri (libxml2 uri.c).

/** Port of xmlNodeGetBase: xml:base attributes and the document url, NULL on errors. */
char *xslt1_node_get_base(XmlDom *doc, XMQNode *cur)
{
    char *ret = NULL;
    for (XMQNode *n = cur; n; n = xml_node_parent(n))
    {
        if (xml_node_kind(n) != XML_DOM_ELEMENT) continue;
        char *base = xslt1_get_ns_prop_exact(n, "base", XML_DOM_XML_NAMESPACE);
        if (!base) continue;
        if (ret)
        {
            char *newbase = NULL;
            int res = xml_uri_build_safe(ret, base, &newbase);
            free(ret);
            free(base);
            if (res != 0) return NULL;
            ret = newbase;
        }
        else
        {
            ret = base;
        }
        if (!strncmp(ret, "http://", 7) || !strncmp(ret, "ftp://", 6) || !strncmp(ret, "urn:", 4)) return ret;
    }
    const char *url = doc ? xml_dom_url(doc) : NULL;
    if (url)
    {
        if (!ret) return xslt1_strdup(url);
        char *newbase = NULL;
        int res = xml_uri_build_safe(ret, url, &newbase);
        free(ret);
        if (res != 0) return NULL;
        ret = newbase;
    }
    return ret;
}

// The default loader.

/** Read a whole file, NULL with errno set on failure, *read_failed if it was opened but
    could not be read (a directory gives EISDIR when read). */
static char *xslt_read_file(const char *path, size_t *len, bool *read_failed)
{
    *read_failed = false;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 65536, n = 0;
    char *b = (char*)malloc(cap + 1);
    while (true)
    {
        size_t r = fread(b + n, 1, cap - n, f);
        n += r;
        if (n < cap)
        {
            if (ferror(f))
            {
                int e = errno;
                *read_failed = true;
                fclose(f);
                free(b);
                errno = e;
                return NULL;
            }
            break;
        }
        cap *= 2;
        b = (char*)realloc(b, cap + 1);
    }
    fclose(f);
    b[n] = 0;
    *len = n;
    return b;
}

static void xslt_loader_message(void *user_data, const char *message)
{
    xslt_emit((const XmlLibxslt1Settings*)user_data, message);
}

XmlDom *xml_libxslt1_default_loader(void *user_data, const char *uri, XmlLibxslt1LoadType type)
{
    (void)type;
    if (!uri) return NULL;
    const char *path = uri;
    if (!strncmp(path, "file://", 7))
    {
        path += 7;
        if (!strncmp(path, "localhost/", 10)) path += 9;
    }
    size_t len = 0;
    bool read_failed;
    char *buf = xslt_read_file(path, &len, &read_failed);
    if (!buf)
    {
        // Like the libxml2 parser: an error while reading is reported as a parse error at line 1.
        char *m = read_failed ? xslt_format("%s:1: I/O error : %s\n\n^\n", uri, strerror(errno))
                              : xslt_format("I/O warning : failed to load \"%s\": %s\n", uri, strerror(errno));
        xslt_emit((const XmlLibxslt1Settings*)user_data, m);
        free(m);
        return NULL;
    }
    XmlParseSettings ps;
    xml_parse_settings_default(&ps);
    ps.source_name = uri;
    ps.no_cdata = true; // XML_PARSE_NOCDATA, part of XSLT_PARSE_OPTIONS.
    ps.dtd_attr = true; // XML_PARSE_DTDATTR, part of XSLT_PARSE_OPTIONS.
    ps.message_callback = xslt_loader_message;
    ps.message_user_data = user_data;
    XmlParseError err;
    XmlDom *dom = xml_parse_buffer(buf, buf + len, &ps, &err);
    if (!dom && err.message) xslt_emit((const XmlLibxslt1Settings*)user_data, err.message);
    xml_parse_error_free(&err);
    if (dom)
    {
        // XML_PARSE_NOENT: entities are substituted.
        if (!xml_expand_entities(dom, NULL, NULL, len, &ps, &err))
        {
            if (err.message) xslt_emit((const XmlLibxslt1Settings*)user_data, err.message);
            xml_dom_free(dom);
            dom = NULL;
        }
        xml_parse_error_free(&err);
    }
    if (dom) xml_dom_set_url(dom, uri);
    free(buf);
    return dom;
}

XmlDom *xslt1_load(XmlLibxslt1Stylesheet *style, const char *uri, XmlLibxslt1LoadType type)
{
    if (style->settings.loader) return style->settings.loader(style->settings.loader_user_data, uri, type);
    return xml_libxslt1_default_loader((void*)&style->settings, uri, type);
}

// Precomputation of instructions, port of libxslt preproc.c ///////

static void xslt_parse_template_content(XmlLibxslt1Stylesheet *style, XMQNode *templ);
static int xslt_check_ext_prefix(XmlLibxslt1Stylesheet *style, const char *uri);

/** Port of xsltCheckTopLevelElement. */
static int xslt_check_top_level_element(XmlLibxslt1Stylesheet *style, XMQNode *inst, int err)
{
    if (!style || !inst || !xml_node_ns(inst)) return -1;
    XMQNode *parent = xml_node_parent(inst);
    if (!parent)
    {
        if (err)
        {
            xslt1_transform_error(NULL, style, inst, "internal problem: element has no parent\n");
            style->errors++;
        }
        return 0;
    }
    if (xml_node_kind(parent) != XML_DOM_ELEMENT || !xml_node_ns(parent) ||
        (xml_node_ns(parent) != xml_node_ns(inst) && !xslt1_streq(xslt_node_ns_uri(parent), xslt_node_ns_uri(inst))) ||
        (!xslt1_streq(xml_node_name(parent), "stylesheet") && !xslt1_streq(xml_node_name(parent), "transform")))
    {
        if (err)
        {
            xslt1_transform_error(NULL, style, inst, "element %s only allowed as child of stylesheet\n", xml_node_name(inst));
            style->errors++;
        }
        return 0;
    }
    return 1;
}

static bool xslt_same_ns(XMQNode *a, XMQNode *b)
{
    XmlNs *na = xml_node_ns(a), *nb = xml_node_ns(b);
    return na == nb || (na && nb && xslt1_streq(xml_ns_href(na), xml_ns_href(nb)));
}

/** Port of xsltCheckInstructionElement. */
static void xslt_check_instruction_element(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    if (!style || !inst || !xml_node_ns(inst) || style->literal_result) return;
    XMQNode *parent = xml_node_parent(inst);
    if (!parent)
    {
        xslt1_transform_error(NULL, style, inst, "internal problem: element has no parent\n");
        style->errors++;
        return;
    }
    while (parent && xml_node_kind(parent) != XML_DOM_DOCUMENT)
    {
        if (xml_node_kind(parent) == XML_DOM_ELEMENT && xml_node_ns(parent) && xslt_same_ns(parent, inst) &&
            (xslt1_streq(xml_node_name(parent), "template") || xslt1_streq(xml_node_name(parent), "param") ||
             xslt1_streq(xml_node_name(parent), "attribute") || xslt1_streq(xml_node_name(parent), "variable")))
        {
            return;
        }
        // No extension module has stylesheet data (extInfos), so there are no extension elements to skip.
        parent = xml_node_parent(parent);
    }
    xslt1_transform_error(NULL, style, inst, "element %s only allowed within a template, variable or param\n", xml_node_name(inst));
    style->errors++;
}

/** Port of xsltCheckParentElement. */
static void xslt_check_parent_element(XmlLibxslt1Stylesheet *style, XMQNode *inst, const char *allow1, const char *allow2)
{
    if (!style || !inst || !xml_node_ns(inst) || style->literal_result) return;
    XMQNode *parent = xml_node_parent(inst);
    if (!parent)
    {
        xslt1_transform_error(NULL, style, inst, "internal problem: element has no parent\n");
        style->errors++;
        return;
    }
    if (xml_node_kind(parent) == XML_DOM_ELEMENT && xslt_same_ns(parent, inst) &&
        (xslt1_streq(xml_node_name(parent), allow1) || xslt1_streq(xml_node_name(parent), allow2)))
    {
        return;
    }
    xslt1_transform_error(NULL, style, inst, "element %s is not allowed within that context\n", xml_node_name(inst));
    style->errors++;
}

/** Port of xsltNewStylePreComp. */
static XsltStylePreComp *xslt_new_style_pre_comp(XmlLibxslt1Stylesheet *style, XsltStyleType type)
{
    if (!style) return NULL;
    XsltStylePreComp *cur = (XsltStylePreComp*)calloc(1, sizeof(XsltStylePreComp));
    cur->type = type;
    cur->next = style->pre_comps;
    style->pre_comps = cur;
    return cur;
}

static void xslt_free_style_pre_comp(XsltStylePreComp *comp)
{
    if (!comp) return;
    xml_xpath_comp_free(comp->comp);
    xslt_free_comp_match_list(comp->numdata.count_pat);
    xslt_free_comp_match_list(comp->numdata.from_pat);
    free(comp->ns_list);
    free(comp->stype);
    free(comp->order);
    free(comp->lang);
    free(comp->case_order);
    free(comp->use);
    free(comp->name);
    free(comp->ns);
    free(comp->mode);
    free(comp->mode_uri);
    free(comp->test);
    free(comp->select);
    free(comp->filename);
    for (int i = 0; i < 5; ++i) free(comp->num_strings[i]);
    free(comp);
}

static void xslt_free_style_pre_comps(XmlLibxslt1Stylesheet *style)
{
    XsltStylePreComp *cur = style->pre_comps;
    while (cur)
    {
        XsltStylePreComp *next = cur->next;
        xslt_free_style_pre_comp(cur);
        cur = next;
    }
    style->pre_comps = NULL;
}

/** Port of xsltEvalStaticAttrValueTemplate: the value, unless it is an AVT (then NULL with found set). */
static char *xslt_eval_static_avt(XmlLibxslt1Stylesheet *style, XMQNode *inst, const char *name, const char *ns, int *found)
{
    if (!style || !inst || !name || xml_node_kind(inst) != XML_DOM_ELEMENT) return NULL;
    char *expr = xslt1_get_ns_prop(inst, name, ns);
    if (!expr)
    {
        *found = 0;
        return NULL;
    }
    *found = 1;
    if (strchr(expr, '{'))
    {
        free(expr);
        return NULL;
    }
    return expr;
}

static XmlXPathComp *xslt_xpath_compile_inst(XmlLibxslt1Stylesheet *style, XMQNode *inst, const char *str, bool novar)
{
    return xslt1_xpath_compile(&style->settings, inst, str, novar);
}

/** Port of xsltDocumentComp, for xsl:document (XSLT 1.1 draft), saxon:output, xalan:write and xt:document. */
static XsltStylePreComp *xslt_document_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_DOCUMENT);
    if (!comp) return NULL;
    comp->ext = XSLT_EXT_DOCUMENT;
    comp->inst = inst;
    comp->ver11 = 0;
    char *filename = NULL;
    const char *name = xml_node_name(inst);
    if (xslt1_streq(name, "output"))
    {
        filename = xslt_eval_static_avt(style, inst, "file", NULL, &comp->has_filename);
    }
    else if (xslt1_streq(name, "write"))
    {
        // The filename needs to be interpreted.
    }
    else if (xslt1_streq(name, "document"))
    {
        if (xml_node_ns(inst) && xslt1_streq(xslt_node_ns_uri(inst), XSLT_NAMESPACE)) comp->ver11 = 1;
        filename = xslt_eval_static_avt(style, inst, "href", NULL, &comp->has_filename);
    }
    if (!comp->has_filename)
    {
        free(filename);
        return comp;
    }
    comp->filename = filename;
    return comp;
}

/** Port of xsltSortComp. */
static void xslt_sort_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_SORT);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;

    comp->stype = xslt_eval_static_avt(style, inst, "data-type", NULL, &comp->has_stype);
    if (comp->stype)
    {
        if (!strcmp(comp->stype, "text")) comp->number = 0;
        else if (!strcmp(comp->stype, "number")) comp->number = 1;
        else
        {
            xslt1_transform_error(NULL, style, inst, "xsltSortComp: no support for data-type = %s\n", comp->stype);
            comp->number = 0;
            style->warnings++;
        }
    }
    comp->order = xslt_eval_static_avt(style, inst, "order", NULL, &comp->has_order);
    if (comp->order)
    {
        if (!strcmp(comp->order, "ascending")) comp->descending = 0;
        else if (!strcmp(comp->order, "descending")) comp->descending = 1;
        else
        {
            xslt1_transform_error(NULL, style, inst, "xsltSortComp: invalid value %s for order\n", comp->order);
            comp->descending = 0;
            style->warnings++;
        }
    }
    comp->case_order = xslt_eval_static_avt(style, inst, "case-order", NULL, &comp->has_use);
    if (comp->case_order)
    {
        if (!strcmp(comp->case_order, "upper-first")) comp->lower_first = 0;
        else if (!strcmp(comp->case_order, "lower-first")) comp->lower_first = 1;
        else
        {
            // Like libxslt, the message shows the order.
            xslt1_transform_error(NULL, style, inst, "xsltSortComp: invalid value %s for order\n", comp->order ? comp->order : "(null)");
            comp->lower_first = 0;
            style->warnings++;
        }
    }
    comp->lang = xslt_eval_static_avt(style, inst, "lang", NULL, &comp->has_lang);

    comp->select = xslt1_get_ns_prop(inst, "select", XSLT_NAMESPACE);
    // The default is ., the string-value of the current node is the sort key.
    if (!comp->select) comp->select = xslt1_strdup(".");
    comp->comp = xslt_xpath_compile_inst(style, inst, comp->select, false);
    if (!comp->comp)
    {
        xslt1_transform_error(NULL, style, inst, "xsltSortComp: could not compile select expression '%s'\n", comp->select);
        style->errors++;
    }
    if (xml_node_first_child(inst))
    {
        xslt1_transform_error(NULL, style, inst, "xsl:sort : is not empty\n");
        style->errors++;
    }
}

/** Port of xsltCopyComp. */
static void xslt_copy_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_COPY);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    comp->use = xslt1_get_ns_prop(inst, "use-attribute-sets", XSLT_NAMESPACE);
    comp->has_use = comp->use != NULL;
}

/** Port of xsltTextComp. */
static void xslt_text_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_TEXT);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    comp->noescape = 0;
    char *prop = xslt1_get_ns_prop(inst, "disable-output-escaping", XSLT_NAMESPACE);
    if (prop)
    {
        if (!strcmp(prop, "yes")) comp->noescape = 1;
        else if (strcmp(prop, "no"))
        {
            xslt1_transform_error(NULL, style, inst, "xsl:text: disable-output-escaping allows only yes or no\n");
            style->warnings++;
        }
        free(prop);
    }
}

/** Port of xsltElementComp. */
static void xslt_element_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_ELEMENT);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;

    comp->name = xslt_eval_static_avt(style, inst, "name", NULL, &comp->has_name);
    if (!comp->has_name)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:element: The attribute 'name' is missing.\n");
        style->errors++;
        return;
    }
    comp->ns = xslt_eval_static_avt(style, inst, "namespace", NULL, &comp->has_ns);
    if (comp->name)
    {
        if (xslt1_validate_qname(comp->name))
        {
            xslt1_transform_error(NULL, style, inst, "xsl:element: The value '%s' of the attribute 'name' is not a valid QName.\n", comp->name);
            style->errors++;
        }
        else
        {
            char *prefix = NULL;
            char *name = xslt1_split_qname(comp->name, &prefix);
            if (comp->has_ns == 0)
            {
                // The QName is expanded with the namespace declarations in effect, including the default one.
                XmlNs *ns = xml_ns_lookup_prefix(inst, prefix);
                if (ns)
                {
                    free(comp->ns);
                    comp->ns = xslt1_strdup(xml_ns_href(ns));
                    comp->has_ns = 1;
                }
                else if (prefix)
                {
                    xslt1_transform_error(NULL, style, inst,
                        "xsl:element: The prefixed QName '%s' has no namespace binding in scope in the stylesheet; "
                        "this is an error, since the namespace was not specified by the instruction itself.\n", comp->name);
                    style->errors++;
                }
            }
            if (prefix && !strncasecmp(prefix, "xml", 3))
            {
                // Mark it to be skipped.
                comp->has_name = 0;
            }
            free(prefix);
            free(name);
        }
    }
    comp->use = xslt_eval_static_avt(style, inst, "use-attribute-sets", NULL, &comp->has_use);
}

/** Port of xsltAttributeComp. */
static void xslt_attribute_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_ATTRIBUTE);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;

    comp->name = xslt_eval_static_avt(style, inst, "name", NULL, &comp->has_name);
    if (!comp->has_name)
    {
        xslt1_transform_error(NULL, style, inst, "XSLT-attribute: The attribute 'name' is missing.\n");
        style->errors++;
        return;
    }
    comp->ns = xslt_eval_static_avt(style, inst, "namespace", NULL, &comp->has_ns);
    if (comp->name)
    {
        if (xslt1_validate_qname(comp->name))
        {
            xslt1_transform_error(NULL, style, inst, "xsl:attribute: The value '%s' of the attribute 'name' is not a valid QName.\n", comp->name);
            style->errors++;
        }
        else if (!strcmp(comp->name, "xmlns"))
        {
            xslt1_transform_error(NULL, style, inst, "xsl:attribute: The attribute name 'xmlns' is not allowed.\n");
            style->errors++;
        }
        else
        {
            char *prefix = NULL;
            char *name = xslt1_split_qname(comp->name, &prefix);
            if (prefix && comp->has_ns == 0)
            {
                XmlNs *ns = xml_ns_lookup_prefix(inst, prefix);
                if (ns)
                {
                    free(comp->ns);
                    comp->ns = xslt1_strdup(xml_ns_href(ns));
                    comp->has_ns = 1;
                }
                else
                {
                    xslt1_transform_error(NULL, style, inst,
                        "xsl:attribute: The prefixed QName '%s' has no namespace binding in scope in the stylesheet; "
                        "this is an error, since the namespace was not specified by the instruction itself.\n", comp->name);
                    style->errors++;
                }
            }
            free(prefix);
            free(name);
        }
    }
}

static void xslt_simple_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst, XsltStyleType type)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, type);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
}

/** Port of xsltProcessingInstructionComp. */
static void xslt_pi_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_PI);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    comp->name = xslt_eval_static_avt(style, inst, "name", XSLT_NAMESPACE, &comp->has_name);
}

/** Port of xsltCopyOfComp and xsltValueOfComp. */
static void xslt_select_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst, XsltStyleType type)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, type);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    const char *what = type == XSLT_FUNC_COPYOF ? "xsl:copy-of" : "xsl:value-of";
    if (type == XSLT_FUNC_VALUEOF)
    {
        char *prop = xslt1_get_ns_prop(inst, "disable-output-escaping", XSLT_NAMESPACE);
        if (prop)
        {
            if (!strcmp(prop, "yes")) comp->noescape = 1;
            else if (strcmp(prop, "no"))
            {
                xslt1_transform_error(NULL, style, inst, "xsl:value-of : disable-output-escaping allows only yes or no\n");
                style->warnings++;
            }
            free(prop);
        }
    }
    comp->select = xslt1_get_ns_prop(inst, "select", XSLT_NAMESPACE);
    if (!comp->select)
    {
        xslt1_transform_error(NULL, style, inst, "%s : select is missing\n", what);
        style->errors++;
        return;
    }
    comp->comp = xslt_xpath_compile_inst(style, inst, comp->select, false);
    if (!comp->comp)
    {
        xslt1_transform_error(NULL, style, inst, "%s : could not compile select expression '%s'\n", what, comp->select);
        style->errors++;
    }
}

/** Port of xsltGetQNameProperty. */
static void xslt_get_qname_property(XmlLibxslt1Stylesheet *style, XMQNode *inst, const char *prop_name, int mandatory,
                                    int *has_prop, char **ns_name, char **local_name)
{
    if (ns_name) *ns_name = NULL;
    if (local_name) *local_name = NULL;
    if (has_prop) *has_prop = 0;
    char *prop = xslt1_get_ns_prop(inst, prop_name, XSLT_NAMESPACE);
    if (!prop)
    {
        if (mandatory)
        {
            xslt1_transform_error(NULL, style, inst, "The attribute '%s' is missing.\n", prop_name);
            style->errors++;
        }
        return;
    }
    if (xslt1_validate_qname(prop))
    {
        xslt1_transform_error(NULL, style, inst, "The value '%s' of the attribute '%s' is not a valid QName.\n", prop, prop_name);
        style->errors++;
        free(prop);
        return;
    }
    const char *uri = xslt_get_qname_uri2(style, inst, &prop);
    if (!prop)
    {
        style->errors++;
        return;
    }
    if (local_name) *local_name = prop;
    else free(prop);
    if (has_prop) *has_prop = 1;
    if (uri && ns_name) *ns_name = xslt1_strdup(uri);
}

/** Port of xsltWithParamComp, xsltVariableComp and xsltParamComp. */
static void xslt_variable_like_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst, XsltStyleType type)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, type);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    xslt_get_qname_property(style, inst, "name", 1, &comp->has_name, &comp->ns, &comp->name);
    if (comp->ns) comp->has_ns = 1;
    comp->select = xslt1_get_ns_prop(inst, "select", XSLT_NAMESPACE);
    if (!comp->select) return;
    comp->comp = xslt_xpath_compile_inst(style, inst, comp->select, false);
    if (type == XSLT_FUNC_WITHPARAM)
    {
        if (!comp->comp)
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-with-param: Failed to compile select expression '%s'\n", comp->select);
            style->errors++;
        }
        if (xml_node_first_child(inst))
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-with-param: The content should be empty since the attribute select is present.\n");
            style->warnings++;
        }
    }
    else if (type == XSLT_FUNC_VARIABLE)
    {
        if (!comp->comp)
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-variable: Failed to compile the XPath expression '%s'.\n", comp->select);
            style->errors++;
        }
        for (XMQNode *cur = xml_node_first_child(inst); cur; cur = xml_node_next(cur))
        {
            XmlNodeKind k = xml_node_kind(cur);
            if (k != XML_DOM_COMMENT && (k != XML_DOM_TEXT || !xslt1_is_blank_str(xml_node_content(cur))))
            {
                xslt1_transform_error(NULL, style, inst, "XSLT-variable: There must be no child nodes, since the attribute 'select' was specified.\n");
                style->errors++;
            }
        }
    }
    else
    {
        if (!comp->comp)
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-param: could not compile select expression '%s'.\n", comp->select);
            style->errors++;
        }
        if (xml_node_first_child(inst))
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-param: The content should be empty since the attribute 'select' is present.\n");
            style->warnings++;
        }
    }
}

/** Port of xsltNumberComp. */
static void xslt_number_comp(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_NUMBER);
    xml_node_set_user_data(cur, comp);

    comp->numdata.doc = xml_node_dom(cur);
    comp->numdata.node = cur;
    comp->num_strings[0] = xslt1_get_ns_prop(cur, "value", XSLT_NAMESPACE);
    comp->numdata.value = comp->num_strings[0];

    char *prop = xslt_eval_static_avt(style, cur, "format", XSLT_NAMESPACE, &comp->numdata.has_format);
    if (comp->numdata.has_format == 0)
    {
        free(prop);
        comp->num_strings[1] = xslt1_strdup("");
    }
    else
    {
        comp->num_strings[1] = prop;
    }
    comp->numdata.format = comp->num_strings[1];

    comp->num_strings[2] = xslt1_get_ns_prop(cur, "count", XSLT_NAMESPACE);
    comp->numdata.count = comp->num_strings[2];
    comp->num_strings[3] = xslt1_get_ns_prop(cur, "from", XSLT_NAMESPACE);
    comp->numdata.from = comp->num_strings[3];

    if (comp->numdata.count) comp->numdata.count_pat = xslt_compile_pattern_internal(comp->numdata.count, cur, style, NULL, 0);
    if (comp->numdata.from) comp->numdata.from_pat = xslt_compile_pattern_internal(comp->numdata.from, cur, style, NULL, 0);

    prop = xslt1_get_ns_prop(cur, "level", XSLT_NAMESPACE);
    if (prop)
    {
        if (!strcmp(prop, "single") || !strcmp(prop, "multiple") || !strcmp(prop, "any"))
        {
            comp->num_strings[4] = prop;
            comp->numdata.level = prop;
            prop = NULL;
        }
        else
        {
            xslt1_transform_error(NULL, style, cur, "xsl:number : invalid value %s for level\n", prop);
            style->warnings++;
        }
        free(prop);
    }

    prop = xslt1_get_ns_prop(cur, "lang", XSLT_NAMESPACE);
    if (prop)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:number : lang attribute not implemented\n");
        // XSLT_TODO, reported with the position in libxslt.
        xslt1_generic_error(&style->settings, "Unimplemented block at preproc.c:1459\n");
        free(prop);
    }

    prop = xslt1_get_ns_prop(cur, "letter-value", XSLT_NAMESPACE);
    if (prop)
    {
        if (!strcmp(prop, "alphabetic"))
        {
            xslt1_transform_error(NULL, style, cur, "xsl:number : letter-value 'alphabetic' not implemented\n");
            style->warnings++;
            xslt1_generic_error(&style->settings, "Unimplemented block at preproc.c:1468\n");
        }
        else if (!strcmp(prop, "traditional"))
        {
            xslt1_transform_error(NULL, style, cur, "xsl:number : letter-value 'traditional' not implemented\n");
            style->warnings++;
            xslt1_generic_error(&style->settings, "Unimplemented block at preproc.c:1473\n");
        }
        else
        {
            xslt1_transform_error(NULL, style, cur, "xsl:number : invalid value %s for letter-value\n", prop);
            style->warnings++;
        }
        free(prop);
    }

    prop = xslt1_get_ns_prop(cur, "grouping-separator", XSLT_NAMESPACE);
    if (prop)
    {
        int len = (int)strlen(prop);
        int c = xslt1_utf8_char(prop, &len);
        comp->numdata.grouping_character_len = len;
        comp->numdata.grouping_character = c < 0 ? 0 : c;
        free(prop);
    }

    prop = xslt1_get_ns_prop(cur, "grouping-size", XSLT_NAMESPACE);
    if (prop)
    {
        sscanf(prop, "%d", &comp->numdata.digits_per_group);
        free(prop);
    }
    else
    {
        comp->numdata.grouping_character = 0;
    }

    // Default values.
    if (!comp->numdata.value && !comp->numdata.level)
    {
        comp->num_strings[4] = xslt1_strdup("single");
        comp->numdata.level = comp->num_strings[4];
    }
}

/** Port of xsltApplyTemplatesComp. */
static void xslt_apply_templates_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_APPLYTEMPLATES);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    xslt_get_qname_property(style, inst, "mode", 0, NULL, &comp->mode_uri, &comp->mode);
    comp->select = xslt1_get_ns_prop(inst, "select", XSLT_NAMESPACE);
    if (comp->select)
    {
        comp->comp = xslt_xpath_compile_inst(style, inst, comp->select, false);
        if (!comp->comp)
        {
            xslt1_transform_error(NULL, style, inst, "XSLT-apply-templates: could not compile select expression '%s'\n", comp->select);
            style->errors++;
        }
    }
}

/** Port of xsltCallTemplateComp. */
static void xslt_call_template_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_CALLTEMPLATE);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    xslt_get_qname_property(style, inst, "name", 1, &comp->has_name, &comp->ns, &comp->name);
    if (comp->ns) comp->has_ns = 1;
}

/** Port of xsltIfComp and xsltWhenComp. */
static void xslt_test_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst, XsltStyleType type)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, type);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    const char *what = type == XSLT_FUNC_IF ? "xsl:if" : "xsl:when";
    comp->test = xslt1_get_ns_prop(inst, "test", XSLT_NAMESPACE);
    if (!comp->test)
    {
        xslt1_transform_error(NULL, style, inst, "%s : test is not defined\n", what);
        style->errors++;
        return;
    }
    comp->comp = xslt_xpath_compile_inst(style, inst, comp->test, false);
    if (!comp->comp)
    {
        xslt1_transform_error(NULL, style, inst, "%s : could not compile test expression '%s'\n", what, comp->test);
        style->errors++;
    }
}

/** Port of xsltForEachComp. */
static void xslt_for_each_comp(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_FOREACH);
    xml_node_set_user_data(inst, comp);
    comp->inst = inst;
    comp->select = xslt1_get_ns_prop(inst, "select", XSLT_NAMESPACE);
    if (!comp->select)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:for-each : select is missing\n");
        style->errors++;
        return;
    }
    comp->comp = xslt_xpath_compile_inst(style, inst, comp->select, false);
    if (!comp->comp)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:for-each : could not compile select expression '%s'\n", comp->select);
        style->errors++;
    }
}

/** The built in extension elements of libxslt (extra.c xsltRegisterAllExtras). */
static XsltExtElement xslt_lookup_ext_element(const char *name, const char *uri)
{
    if (!name || !uri) return XSLT_EXT_NONE;
    if (!strcmp(uri, XSLT_LIBXSLT_NAMESPACE) && !strcmp(name, "debug")) return XSLT_EXT_DEBUG;
    if (!strcmp(uri, XSLT_SAXON_NAMESPACE) && !strcmp(name, "output")) return XSLT_EXT_DOCUMENT;
    if (!strcmp(uri, XSLT_XALAN_NAMESPACE) && !strcmp(name, "write")) return XSLT_EXT_DOCUMENT;
    if (!strcmp(uri, XSLT_XT_NAMESPACE) && !strcmp(name, "document")) return XSLT_EXT_DOCUMENT;
    if (!strcmp(uri, XSLT_NAMESPACE) && !strcmp(name, "document")) return XSLT_EXT_DOCUMENT;
    return XSLT_EXT_NONE;
}

/** Port of xsltPreComputeExtModuleElement. */
static void *xslt_pre_compute_ext_module_element(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    if (!style || !inst || xml_node_kind(inst) != XML_DOM_ELEMENT || !xml_node_ns(inst)) return NULL;
    XsltExtElement ext = xslt_lookup_ext_element(xml_node_name(inst), xslt_node_ns_uri(inst));
    if (ext == XSLT_EXT_NONE) return NULL;
    if (ext == XSLT_EXT_DOCUMENT) return xslt_document_comp(style, inst);
    // Default creation (xsltNewElemPreComp).
    XsltStylePreComp *comp = xslt_new_style_pre_comp(style, XSLT_FUNC_EXTENSION);
    comp->ext = ext;
    comp->inst = inst;
    return comp;
}

/** Port of xsltStylePreCompute. */
static void xslt_style_pre_compute(XmlLibxslt1Stylesheet *style, XMQNode *inst)
{
    if (!inst || xml_node_kind(inst) != XML_DOM_ELEMENT || xml_node_user_data(inst)) return;

    if (xslt1_is_xslt_elem(inst))
    {
        if (xslt1_is_xslt_name(inst, "apply-templates"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_apply_templates_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "with-param"))
        {
            xslt_check_parent_element(style, inst, "apply-templates", "call-template");
            xslt_variable_like_comp(style, inst, XSLT_FUNC_WITHPARAM);
        }
        else if (xslt1_is_xslt_name(inst, "value-of"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_select_comp(style, inst, XSLT_FUNC_VALUEOF);
        }
        else if (xslt1_is_xslt_name(inst, "copy"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_copy_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "copy-of"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_select_comp(style, inst, XSLT_FUNC_COPYOF);
        }
        else if (xslt1_is_xslt_name(inst, "if"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_test_comp(style, inst, XSLT_FUNC_IF);
        }
        else if (xslt1_is_xslt_name(inst, "when"))
        {
            xslt_check_parent_element(style, inst, "choose", NULL);
            xslt_test_comp(style, inst, XSLT_FUNC_WHEN);
        }
        else if (xslt1_is_xslt_name(inst, "choose"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_simple_comp(style, inst, XSLT_FUNC_CHOOSE);
        }
        else if (xslt1_is_xslt_name(inst, "for-each"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_for_each_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "apply-imports"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_simple_comp(style, inst, XSLT_FUNC_APPLYIMPORTS);
        }
        else if (xslt1_is_xslt_name(inst, "attribute"))
        {
            XMQNode *parent = xml_node_parent(inst);
            if (!parent || xml_node_kind(parent) != XML_DOM_ELEMENT || !xml_node_ns(parent) ||
                !xslt_same_ns(parent, inst) || !xslt1_streq(xml_node_name(parent), "attribute-set"))
            {
                xslt_check_instruction_element(style, inst);
            }
            xslt_attribute_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "element"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_element_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "text"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_text_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "sort"))
        {
            xslt_check_parent_element(style, inst, "apply-templates", "for-each");
            xslt_sort_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "comment"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_simple_comp(style, inst, XSLT_FUNC_COMMENT);
        }
        else if (xslt1_is_xslt_name(inst, "number"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_number_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "processing-instruction"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_pi_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "call-template"))
        {
            xslt_check_instruction_element(style, inst);
            xslt_call_template_comp(style, inst);
        }
        else if (xslt1_is_xslt_name(inst, "param"))
        {
            if (xslt_check_top_level_element(style, inst, 0) == 0) xslt_check_instruction_element(style, inst);
            xslt_variable_like_comp(style, inst, XSLT_FUNC_PARAM);
        }
        else if (xslt1_is_xslt_name(inst, "variable"))
        {
            if (xslt_check_top_level_element(style, inst, 0) == 0) xslt_check_instruction_element(style, inst);
            xslt_variable_like_comp(style, inst, XSLT_FUNC_VARIABLE);
        }
        else if (xslt1_is_xslt_name(inst, "otherwise"))
        {
            xslt_check_parent_element(style, inst, "choose", NULL);
            xslt_check_instruction_element(style, inst);
            return;
        }
        else if (xslt1_is_xslt_name(inst, "template") || xslt1_is_xslt_name(inst, "output") ||
                 xslt1_is_xslt_name(inst, "preserve-space") || xslt1_is_xslt_name(inst, "strip-space"))
        {
            xslt_check_top_level_element(style, inst, 1);
            return;
        }
        else if (xslt1_is_xslt_name(inst, "stylesheet") || xslt1_is_xslt_name(inst, "transform"))
        {
            XMQNode *parent = xml_node_parent(inst);
            if (!parent || xml_node_kind(parent) != XML_DOM_DOCUMENT)
            {
                xslt1_transform_error(NULL, style, inst, "element %s only allowed only as root element\n", xml_node_name(inst));
                style->errors++;
            }
            return;
        }
        else if (xslt1_is_xslt_name(inst, "key") || xslt1_is_xslt_name(inst, "attribute-set") ||
                 xslt1_is_xslt_name(inst, "namespace-alias") || xslt1_is_xslt_name(inst, "include") ||
                 xslt1_is_xslt_name(inst, "import") || xslt1_is_xslt_name(inst, "decimal-format"))
        {
            xslt_check_top_level_element(style, inst, 1);
            return;
        }
        else if (xslt1_is_xslt_name(inst, "message") || xslt1_is_xslt_name(inst, "fallback"))
        {
            xslt_check_instruction_element(style, inst);
            return;
        }
        else if (xslt1_is_xslt_name(inst, "document"))
        {
            xslt_check_instruction_element(style, inst);
            xml_node_set_user_data(inst, xslt_document_comp(style, inst));
        }
        else if (!style || style->forwards_compatible == 0)
        {
            xslt1_transform_error(NULL, style, inst, "xsltStylePreCompute: unknown xsl:%s\n", xml_node_name(inst));
            if (style) style->warnings++;
        }

        // A namespace list is built for every xslt instruction, for its xpath expressions.
        XsltStylePreComp *cur = (XsltStylePreComp*)xml_node_user_data(inst);
        if (cur && cur != XSLT_EXT_MARKER) cur->ns_list = xslt1_ns_list(inst, &cur->ns_nr);
    }
    else
    {
        void *comp = xslt_pre_compute_ext_module_element(style, inst);
        // An unknown element, maybe registered at the context level: mark it for later recognition.
        xml_node_set_user_data(inst, comp ? comp : XSLT_EXT_MARKER);
    }
}

// Attribute value templates, port of libxslt attrvt.c (the compilation).

static XsltAttrVT *xslt_new_attr_vt(XmlLibxslt1Stylesheet *style)
{
    XsltAttrVT *cur = (XsltAttrVT*)calloc(1, sizeof(XsltAttrVT));
    cur->max_seg = 10;
    cur->segments = (void**)calloc((size_t)cur->max_seg, sizeof(void*));
    cur->next = style->att_vts;
    style->att_vts = cur;
    return cur;
}

static void xslt_free_attr_vt(XsltAttrVT *avt)
{
    if (!avt) return;
    for (int i = 0; i < avt->nb_seg; ++i)
    {
        bool is_str = (avt->strstart == 1) == (i % 2 == 0);
        if (is_str) free(avt->segments[i]);
        else xml_xpath_comp_free((XmlXPathComp*)avt->segments[i]);
    }
    free(avt->segments);
    free(avt->ns_list);
    free(avt);
}

static void xslt_free_avt_list(XsltAttrVT *avt)
{
    while (avt)
    {
        XsltAttrVT *next = avt->next;
        xslt_free_attr_vt(avt);
        avt = next;
    }
}

static void xslt_set_attr_vt_segment(XsltAttrVT *avt, void *val)
{
    if (avt->nb_seg >= avt->max_seg)
    {
        avt->max_seg += 10;
        avt->segments = (void**)realloc(avt->segments, (size_t)avt->max_seg * sizeof(void*));
    }
    avt->segments[avt->nb_seg++] = val;
}

/** Like xmlStrncat: a is returned unchanged (even NULL) when there is nothing to add. */
char *xslt1_strncat(char *a, const char *b, size_t n)
{
    if (!b || n == 0) return a;
    size_t la = a ? strlen(a) : 0;
    char *r = (char*)realloc(a, la + n + 1);
    memcpy(r + la, b, n);
    r[la + n] = 0;
    return r;
}

/** Like xmlStrcat: NULL a gives a copy of b (also of ""). */
char *xslt1_strcat(char *a, const char *b)
{
    if (!b) return a;
    if (!a) return xslt1_strdup(b);
    return xslt1_strncat(a, b, strlen(b));
}

/** Port of xsltCompileAttr: compile the attribute value template of a literal result element attribute. */
static void xslt_compile_attr(XmlLibxslt1Stylesheet *style, XMQAttr *attr)
{
    XMQNode *an = xml_attr_node(attr);
    XMQNode *first = xml_node_first_child(an);
    if (!style || !first) return;
    if (xml_node_kind(first) != XML_DOM_TEXT || xml_node_next(first))
    {
        xslt1_transform_error(NULL, style, xml_attr_owner(attr),
            "Attribute '%s': The content is expected to be a single text node when compiling an AVT.\n", xml_attr_name(attr));
        style->errors++;
        return;
    }
    const char *str = xml_node_content(first);
    if (!str || (!strchr(str, '{') && !strchr(str, '}'))) return;
    if (xml_node_user_data(an)) return;

    XsltAttrVT *avt = xslt_new_attr_vt(style);
    xml_node_set_user_data(an, avt);
    avt->ns_list = xslt1_ns_list(xml_attr_owner(attr), &avt->ns_nr);

    char *ret = NULL;
    char *expr = NULL;
    int lastavt = 0;
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
            if (cur[1] == '}')
            {
                // Skip an empty AVT.
                ret = xslt1_strncat(ret, str, (size_t)(cur - str));
                cur += 2;
                str = cur;
                continue;
            }
            if (ret || cur - str > 0)
            {
                ret = xslt1_strncat(ret, str, (size_t)(cur - str));
                str = cur;
                if (avt->nb_seg == 0) avt->strstart = 1;
                xslt_set_attr_vt_segment(avt, ret);
                ret = NULL;
                lastavt = 0;
            }
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
                xslt1_transform_error(NULL, style, xml_attr_owner(attr), "Attribute '%s': The AVT has an unmatched '{'.\n", xml_attr_name(attr));
                style->errors++;
                goto error;
            }
            str++;
            expr = xslt1_strndup(str, (size_t)(cur - str));
            XmlXPathComp *comp = xslt1_xpath_compile(&style->settings, xml_attr_owner(attr), expr, false);
            if (!comp)
            {
                xslt1_transform_error(NULL, style, xml_attr_owner(attr), "Attribute '%s': Failed to compile the expression '%s' in the AVT.\n",
                                     xml_attr_name(attr), expr);
                style->errors++;
                goto error;
            }
            if (avt->nb_seg == 0) avt->strstart = 0;
            if (lastavt == 1) xslt_set_attr_vt_segment(avt, NULL);
            xslt_set_attr_vt_segment(avt, comp);
            lastavt = 1;
            free(expr);
            expr = NULL;
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
            xslt1_transform_error(NULL, style, xml_attr_owner(attr), "Attribute '%s': The AVT has an unmatched '}'.\n", xml_attr_name(attr));
            goto error;
        }
        else
        {
            cur++;
        }
    }
    if (ret || cur - str > 0)
    {
        ret = xslt1_strncat(ret, str, (size_t)(cur - str));
        if (avt->nb_seg == 0) avt->strstart = 1;
        xslt_set_attr_vt_segment(avt, ret);
        ret = NULL;
    }
error:
    free(ret);
    free(expr);
}

// Stylesheets, port of libxslt xslt.c ///////////////////////////

/** Port of xsltParseContentError. */
static void xslt_parse_content_error(XmlLibxslt1Stylesheet *style, XMQNode *node)
{
    if (!style || !node) return;
    if (xslt1_is_xslt_elem(node))
        xslt1_transform_error(NULL, style, node, "The XSLT-element '%s' is not allowed at this position.\n", xml_node_name(node));
    else
        xslt1_transform_error(NULL, style, node, "The element '%s' is not allowed at this position.\n",
                             xslt1_libxml2_node_name(node) ? xslt1_libxml2_node_name(node) : "(null)");
    style->errors++;
}

/** Port of exclPrefixPush: returns the index, -1 for duplicates. */
static int xslt_excl_prefix_push(XmlLibxslt1Stylesheet *style, const char *orig)
{
    for (int i = 0; i < style->excl_prefix_nr; i++)
    {
        if (xslt1_streq(style->excl_prefix_tab[i], orig)) return -1;
    }
    if (style->excl_prefix_nr >= style->excl_prefix_max)
    {
        int max = style->excl_prefix_max ? style->excl_prefix_max * 2 : 4;
        style->excl_prefix_tab = (char**)realloc(style->excl_prefix_tab, (size_t)max * sizeof(char*));
        style->excl_prefix_max = max;
    }
    style->excl_prefix_tab[style->excl_prefix_nr] = xslt1_strdup(orig);
    style->excl_prefix = style->excl_prefix_tab[style->excl_prefix_nr];
    return style->excl_prefix_nr++;
}

/** Port of exclPrefixPop. */
static void xslt_excl_prefix_pop(XmlLibxslt1Stylesheet *style)
{
    if (style->excl_prefix_nr <= 0) return;
    style->excl_prefix_nr--;
    style->excl_prefix = style->excl_prefix_nr > 0 ? style->excl_prefix_tab[style->excl_prefix_nr - 1] : NULL;
    free(style->excl_prefix_tab[style->excl_prefix_nr]);
    style->excl_prefix_tab[style->excl_prefix_nr] = NULL;
}

/** Port of xsltNewDecimalFormat. */
static XsltDecimalFormat *xslt_new_decimal_format(const char *ns_uri, char *name)
{
    XsltDecimalFormat *self = (XsltDecimalFormat*)calloc(1, sizeof(XsltDecimalFormat));
    self->ns_uri = xslt1_strdup(ns_uri);
    self->name = name;
    self->digit = xslt1_strdup("#");
    self->pattern_separator = xslt1_strdup(";");
    self->decimal_point = xslt1_strdup(".");
    self->grouping = xslt1_strdup(",");
    self->percent = xslt1_strdup("%");
    self->permille = xslt1_strdup("\xe2\x80\xb0");
    self->zero_digit = xslt1_strdup("0");
    self->minus_sign = xslt1_strdup("-");
    self->infinity = xslt1_strdup("Infinity");
    self->no_number = xslt1_strdup("NaN");
    return self;
}

static void xslt_free_decimal_format_list(XsltDecimalFormat *f)
{
    while (f)
    {
        XsltDecimalFormat *next = f->next;
        free(f->name);
        free(f->digit);
        free(f->pattern_separator);
        free(f->minus_sign);
        free(f->infinity);
        free(f->no_number);
        free(f->decimal_point);
        free(f->grouping);
        free(f->percent);
        free(f->permille);
        free(f->zero_digit);
        free(f->ns_uri);
        free(f);
        f = next;
    }
}

/** Port of xsltDecimalFormatGetByQName. */
XsltDecimalFormat *xslt1_decimal_format_get_by_qname(XmlLibxslt1Stylesheet *style, const char *ns_uri, const char *name)
{
    if (!name) return style->decimal_format;
    while (style)
    {
        for (XsltDecimalFormat *r = style->decimal_format->next; r; r = r->next)
        {
            if (xslt1_streq(ns_uri, r->ns_uri) && xslt1_streq(name, r->name)) return r;
        }
        style = xslt1_next_import(style);
    }
    return NULL;
}

static XsltTemplate *xslt_new_template(void)
{
    XsltTemplate *cur = (XsltTemplate*)calloc(1, sizeof(XsltTemplate));
    cur->priority = XSLT_PAT_NO_PRIORITY;
    return cur;
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
        free(t->inherited_ns);
        free(t);
        t = next;
    }
}

/** Port of xsltNewStylesheetInternal. */
static XmlLibxslt1Stylesheet *xslt_new_stylesheet_internal(XmlLibxslt1Stylesheet *parent, const XmlLibxslt1Settings *settings)
{
    XmlLibxslt1Stylesheet *ret = (XmlLibxslt1Stylesheet*)calloc(1, sizeof(XmlLibxslt1Stylesheet));
    if (settings) ret->settings = *settings;
    else if (parent) ret->settings = parent->settings;
    ret->parent = parent;
    ret->omit_xml_declaration = -1;
    ret->standalone = -1;
    ret->decimal_format = xslt_new_decimal_format(NULL, NULL);
    ret->indent = -1;
    ret->principal = parent ? parent->principal : ret;
    return ret;
}

XmlLibxslt1Stylesheet *xml_libxslt1_stylesheet_new_empty(XmlDom *style_dom, const XmlLibxslt1Settings *settings)
{
    XmlLibxslt1Stylesheet *style = xslt_new_stylesheet_internal(NULL, settings);
    style->doc = style_dom;
    return style;
}

static void xslt_free_stack_elem_list(XsltStackElem *e)
{
    while (e)
    {
        XsltStackElem *next = e->next;
        free(e->name);
        free(e->name_uri);
        free(e->select);
        xml_xpath_result_free(e->value);
        free(e);
        e = next;
    }
}

static void xslt_free_keys(XmlLibxslt1Stylesheet *style)
{
    XsltKeyDef *k = style->keys;
    while (k)
    {
        XsltKeyDef *next = k->next;
        free(k->name);
        free(k->name_uri);
        free(k->match);
        free(k->use);
        xml_xpath_comp_free(k->comp);
        xml_xpath_comp_free(k->usecomp);
        free(k);
        k = next;
    }
    style->keys = NULL;
}

static void xslt_free_attr_set(XsltAttrSet *set)
{
    if (!set) return;
    XsltAttrElem *a = set->attrs;
    while (a)
    {
        XsltAttrElem *next = a->next;
        free(a);
        a = next;
    }
    XsltUseAttrSet *u = set->use_attr_sets;
    while (u)
    {
        XsltUseAttrSet *next = u->next;
        free(u->ncname);
        free(u->ns);
        free(u);
        u = next;
    }
    free(set->name);
    free(set->ns);
    free(set);
}

static void xslt_free_hashmap_strings(HashMap *m)
{
    if (!m) return;
    HashMapIterator *i = hashmap_iterate(m);
    const char *key;
    void *val;
    while (hashmap_next_key_value(i, &key, &val)) free(val);
    hashmap_free_iterator(i);
    hashmap_free(m);
}

static void xslt_free_stylesheet(XmlLibxslt1Stylesheet *style);

static void xslt_free_stylesheet_list(XmlLibxslt1Stylesheet *style)
{
    while (style)
    {
        XmlLibxslt1Stylesheet *next = style->next;
        xslt_free_stylesheet(style);
        style = next;
    }
}

static void xslt_free_style_documents(XmlLibxslt1Stylesheet *style)
{
    XsltDocument *cur = style->doc_list;
    while (cur)
    {
        XsltDocument *d = cur;
        cur = cur->next;
        if (!d->main) xml_dom_free(d->doc);
        free(d);
    }
    style->doc_list = NULL;
}

/** Port of xsltFreeStylesheet. */
static void xslt_free_stylesheet(XmlLibxslt1Stylesheet *style)
{
    if (!style) return;
    xslt_free_keys(style);
    while (style->ns_defs)
    {
        XsltExtDef *next = style->ns_defs->next;
        free(style->ns_defs->prefix);
        free(style->ns_defs->uri);
        free(style->ns_defs);
        style->ns_defs = next;
    }
    xslt_free_template_hashes(style);
    xslt_free_decimal_format_list(style->decimal_format);
    xslt_free_templates(style->templates);
    // Attribute sets.
    XsltAttrSet *set = style->attribute_set_list;
    while (set)
    {
        XsltAttrSet *next = set->next_in_order;
        xslt_free_attr_set(set);
        set = next;
    }
    if (style->attribute_sets) hashmap_free(style->attribute_sets);
    xslt_free_hashmap_strings(style->ns_aliases);
    xslt_free_style_pre_comps(style);
    xslt_free_style_documents(style);
    xslt_free_stack_elem_list(style->variables);
    if (style->cdata_section) hashmap_free(style->cdata_section);
    if (style->strip_spaces) hashmap_free(style->strip_spaces);
    for (int i = 0; i < style->excl_prefix_nr; ++i) free(style->excl_prefix_tab[i]);
    free(style->excl_prefix_tab);
    free(style->method);
    free(style->method_uri);
    free(style->version);
    free(style->encoding);
    free(style->doctype_public);
    free(style->doctype_system);
    free(style->media_type);
    free(style->default_alias);
    xslt_free_avt_list(style->att_vts);
    xslt_free_stylesheet_list(style->imports);
    // The main document of this stylesheet level is freed last.
    if (style->doc && style->owns_doc) xml_dom_free(style->doc);
    free(style);
}

void xml_libxslt1_stylesheet_free(XmlLibxslt1Stylesheet *style)
{
    xslt_free_stylesheet(style);
}

/** Port of xsltGetInheritedNsList: the namespaces in scope of a template, for literal result elements. */
static int xslt_get_inherited_ns_list(XmlLibxslt1Stylesheet *style, XsltTemplate *templ, XMQNode *node)
{
    if (!style || !templ || !node || templ->inherited_ns_nr != 0 || templ->inherited_ns) return 0;
    XmlNs **ret = NULL;
    int nbns = 0, maxns = 0;
    for (; node; node = xml_node_parent(node))
    {
        if (xml_node_kind(node) != XML_DOM_ELEMENT) continue;
        for (XmlNs *cur = xml_node_first_ns_decl(node); cur; cur = xml_ns_next_decl(cur))
        {
            if (xslt1_streq(xml_ns_href(cur), XSLT_NAMESPACE)) continue;
            if (xml_ns_prefix(cur) && xslt_check_ext_prefix(style, xml_ns_prefix(cur))) continue;
            // Namespaces excluded on the topmost stylesheet element.
            bool skip = false;
            for (int i = 0; i < style->excl_prefix_nr; i++)
            {
                if (xslt1_streq(xml_ns_href(cur), style->excl_prefix_tab[i])) skip = true;
            }
            if (skip) continue;
            // Skip shadowed namespace bindings.
            int i;
            for (i = 0; i < nbns; i++)
            {
                if (xslt1_streq(xml_ns_prefix(cur), xml_ns_prefix(ret[i]))) break;
            }
            if (i >= nbns)
            {
                if (nbns >= maxns)
                {
                    maxns = maxns == 0 ? 10 : 2 * maxns;
                    ret = (XmlNs**)realloc(ret, (size_t)(maxns + 1) * sizeof(XmlNs*));
                }
                ret[nbns++] = cur;
                ret[nbns] = NULL;
            }
        }
    }
    if (nbns != 0)
    {
        templ->inherited_ns_nr = nbns;
        templ->inherited_ns = ret;
    }
    return nbns;
}

char *xslt1_key2(const char *a, const char *b)
{
    return xslt1_hash3_key(a, b, NULL);
}

/** Parse a whitespace separated list of tokens, calling f for each. */
typedef void (*XsltTokenFn)(XmlLibxslt1Stylesheet *style, XMQNode *cur, char *token, void *data);

static void xslt_for_each_token(XmlLibxslt1Stylesheet *style, XMQNode *cur, const char *list, XsltTokenFn f, void *data)
{
    const char *element = list;
    while (*element)
    {
        while (xslt1_is_blank((unsigned char)*element)) element++;
        if (!*element) break;
        const char *end = element;
        while (*end && !xslt1_is_blank((unsigned char)*end)) end++;
        f(style, cur, xslt1_strndup(element, (size_t)(end - element)), data);
        element = end;
    }
}

static void xslt_cdata_token(XmlLibxslt1Stylesheet *style, XMQNode *cur, char *element, void *data)
{
    (void)data;
    if (xslt1_validate_qname(element) != 0)
    {
        xslt1_transform_error(NULL, style, cur, "Attribute 'cdata-section-elements': The value '%s' is not a valid QName.\n", element);
        free(element);
        style->errors++;
        return;
    }
    const char *uri = xslt1_get_qname_uri(&style->settings, cur, &element);
    if (!element)
    {
        xslt1_transform_error(NULL, style, cur, "Attribute 'cdata-section-elements': Not a valid QName.\n");
        style->errors++;
        return;
    }
    // Unprefixed names use the default namespace (bug #339570).
    if (!uri)
    {
        XmlNs *ns = xml_ns_lookup_prefix(cur, NULL);
        if (ns) uri = xml_ns_href(ns);
    }
    char *key = xslt1_key2(element, uri);
    if (!hashmap_get(style->cdata_section, key)) hashmap_put(style->cdata_section, key, (void*)"cdata");
    free(key);
    free(element);
}

static void xslt_set_string(char **dst, char *value)
{
    free(*dst);
    *dst = value;
}

/** Port of xsltParseStylesheetOutput. */
static void xslt_parse_stylesheet_output(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    char *prop;
    if ((prop = xslt1_get_ns_prop_exact(cur, "version", NULL))) xslt_set_string(&style->version, prop);
    if ((prop = xslt1_get_ns_prop_exact(cur, "encoding", NULL))) xslt_set_string(&style->encoding, prop);
    if ((prop = xslt1_get_ns_prop_exact(cur, "method", NULL)))
    {
        xslt_set_string(&style->method, NULL);
        xslt_set_string(&style->method_uri, NULL);
        const char *uri = xslt1_get_qname_uri(&style->settings, cur, &prop);
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
                xslt1_transform_error(NULL, style, cur, "invalid value for method: %s\n", prop);
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
    if ((prop = xslt1_get_ns_prop_exact(cur, "doctype-system", NULL))) xslt_set_string(&style->doctype_system, prop);
    if ((prop = xslt1_get_ns_prop_exact(cur, "doctype-public", NULL))) xslt_set_string(&style->doctype_public, prop);
    const char *yesno[] = { "standalone", "indent", "omit-xml-declaration" };
    int *targets[] = { &style->standalone, &style->indent, &style->omit_xml_declaration };
    for (int i = 0; i < 3; ++i)
    {
        if ((prop = xslt1_get_ns_prop_exact(cur, yesno[i], NULL)))
        {
            if (!strcmp(prop, "yes")) *targets[i] = 1;
            else if (!strcmp(prop, "no")) *targets[i] = 0;
            else
            {
                xslt1_transform_error(NULL, style, cur, "invalid value for %s: %s\n", yesno[i], prop);
                style->errors++;
            }
            free(prop);
        }
    }
    char *elements = xslt1_get_ns_prop_exact(cur, "cdata-section-elements", NULL);
    if (elements)
    {
        if (!style->cdata_section) style->cdata_section = hashmap_create(10);
        xslt_for_each_token(style, cur, elements, xslt_cdata_token, NULL);
        free(elements);
    }
    if ((prop = xslt1_get_ns_prop_exact(cur, "media-type", NULL))) xslt_set_string(&style->media_type, prop);
    if (xml_node_first_child(cur)) xslt_parse_content_error(style, xml_node_first_child(cur));
}

/** Port of xsltParseStylesheetDecimalFormat. */
static void xslt_parse_stylesheet_decimal_format(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    XsltDecimalFormat *format = style->decimal_format;
    char *prop = xslt1_get_ns_prop_exact(cur, "name", NULL);
    if (prop)
    {
        if (xslt1_validate_qname(prop) != 0)
        {
            xslt1_transform_error(NULL, style, cur, "xsl:decimal-format: Invalid QName '%s'.\n", prop);
            style->warnings++;
            free(prop);
            return;
        }
        const char *ns_uri = xslt1_get_qname_uri(&style->settings, cur, &prop);
        if (!prop)
        {
            style->warnings++;
            return;
        }
        format = xslt1_decimal_format_get_by_qname(style, ns_uri, prop);
        if (format)
        {
            xslt1_transform_error(NULL, style, cur, "xsltParseStylestyleDecimalFormat: %s already exists\n", prop);
            style->warnings++;
            free(prop);
            return;
        }
        format = xslt_new_decimal_format(ns_uri, prop);
        // Append the new decimal-format structure.
        XsltDecimalFormat *iter = style->decimal_format;
        while (iter->next) iter = iter->next;
        iter->next = format;
    }
    struct { const char *name; char **field; } props[] = {
        { "decimal-separator", &format->decimal_point }, { "grouping-separator", &format->grouping },
        { "infinity", &format->infinity }, { "minus-sign", &format->minus_sign }, { "NaN", &format->no_number },
        { "percent", &format->percent }, { "per-mille", &format->permille }, { "zero-digit", &format->zero_digit },
        { "digit", &format->digit }, { "pattern-separator", &format->pattern_separator }
    };
    for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); ++i)
    {
        if ((prop = xslt1_get_ns_prop_exact(cur, props[i].name, NULL))) xslt_set_string(props[i].field, prop);
    }
    if (xml_node_first_child(cur)) xslt_parse_content_error(style, xml_node_first_child(cur));
}

static void xslt_space_token(XmlLibxslt1Stylesheet *style, XMQNode *cur, char *element, void *data)
{
    const char *what = (const char*)data;
    if (!strcmp(element, "*"))
    {
        style->strip_all = !strcmp(what, "strip") ? 1 : -1;
    }
    else
    {
        const char *uri = xslt1_get_qname_uri(&style->settings, cur, &element);
        if (element)
        {
            char *key = xslt1_key2(element, uri);
            if (!hashmap_get(style->strip_spaces, key)) hashmap_put(style->strip_spaces, key, (void*)what);
            free(key);
        }
    }
    free(element);
}

/** Port of xsltParseStylesheetPreserveSpace and xsltParseStylesheetStripSpace. */
static void xslt_parse_stylesheet_space(XmlLibxslt1Stylesheet *style, XMQNode *cur, bool strip)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    // libxslt creates the table before checking the attribute for strip-space, after for preserve-space.
    if (strip && !style->strip_spaces) style->strip_spaces = hashmap_create(10);
    char *elements = xslt1_get_ns_prop_exact(cur, "elements", NULL);
    if (!elements)
    {
        xslt1_transform_error(NULL, style, cur, strip ? "xsltParseStylesheetStripSpace: missing elements attribute\n"
                                                     : "xsltParseStylesheetPreserveSpace: missing elements attribute\n");
        style->warnings++;
        return;
    }
    if (!style->strip_spaces) style->strip_spaces = hashmap_create(10);
    xslt_for_each_token(style, cur, elements, xslt_space_token, (void*)(strip ? "strip" : "preserve"));
    free(elements);
    if (xml_node_first_child(cur)) xslt_parse_content_error(style, xml_node_first_child(cur));
}

/** Port of xsltRegisterExtPrefix (extensions.c). */
static int xslt_register_ext_prefix(XmlLibxslt1Stylesheet *style, const char *prefix, const char *uri)
{
    if (!style || !uri) return -1;
    for (XsltExtDef *def = style->ns_defs; def; def = def->next)
    {
        if (xslt1_streq(prefix, def->prefix)) return -1;
    }
    XsltExtDef *ret = (XsltExtDef*)calloc(1, sizeof(XsltExtDef));
    ret->prefix = xslt1_strdup(prefix);
    ret->uri = xslt1_strdup(uri);
    ret->next = style->ns_defs;
    style->ns_defs = ret;
    // No extension module has a stylesheet initialization function.
    return 0;
}

/** Port of xsltCheckExtPrefix (compares with the registered prefixes, see libxslt). */
static int xslt_check_ext_prefix(XmlLibxslt1Stylesheet *style, const char *uri)
{
    if (!style || !style->ns_defs) return 0;
    if (!uri) uri = "#default";
    for (XsltExtDef *cur = style->ns_defs; cur; cur = cur->next)
    {
        if (xslt1_streq(uri, cur->prefix)) return 1;
    }
    return 0;
}

/** Port of xsltCheckExtURI. */
static int xslt_check_ext_uri(XmlLibxslt1Stylesheet *style, const char *uri)
{
    if (!style || !style->ns_defs || !uri) return 0;
    for (XsltExtDef *cur = style->ns_defs; cur; cur = cur->next)
    {
        if (xslt1_streq(uri, cur->uri)) return 1;
    }
    return 0;
}

/** Port of xsltParseStylesheetExtPrefix. */
static void xslt_parse_stylesheet_ext_prefix(XmlLibxslt1Stylesheet *style, XMQNode *cur, int is_xslt_elem)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    char *prefixes = is_xslt_elem ? xslt1_get_ns_prop_exact(cur, "extension-element-prefixes", NULL)
                                  : xslt1_get_ns_prop_exact(cur, "extension-element-prefixes", XSLT_NAMESPACE);
    if (!prefixes) return;
    const char *prefix = prefixes;
    while (*prefix)
    {
        while (xslt1_is_blank((unsigned char)*prefix)) prefix++;
        if (!*prefix) break;
        const char *end = prefix;
        while (*end && !xslt1_is_blank((unsigned char)*end)) end++;
        char *p = xslt1_strndup(prefix, (size_t)(end - prefix));
        XmlNs *ns = !strcmp(p, "#default") ? xml_ns_lookup_prefix(cur, NULL) : xml_ns_lookup_prefix(cur, p);
        if (!ns)
        {
            xslt1_transform_error(NULL, style, cur, "xsl:extension-element-prefix : undefined namespace %s\n", p);
            style->warnings++;
        }
        else
        {
            xslt_register_ext_prefix(style, p, xml_ns_href(ns));
        }
        free(p);
        prefix = end;
    }
    free(prefixes);
}

/** Port of xsltParseStylesheetExcludePrefix: returns the number of prefixes pushed. */
static int xslt_parse_stylesheet_exclude_prefix(XmlLibxslt1Stylesheet *style, XMQNode *cur, int is_xslt_elem)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return 0;
    char *prefixes = is_xslt_elem ? xslt1_get_ns_prop_exact(cur, "exclude-result-prefixes", NULL)
                                  : xslt1_get_ns_prop_exact(cur, "exclude-result-prefixes", XSLT_NAMESPACE);
    if (!prefixes) return 0;
    int nb = 0;
    const char *prefix = prefixes;
    while (*prefix)
    {
        while (xslt1_is_blank((unsigned char)*prefix)) prefix++;
        if (!*prefix) break;
        const char *end = prefix;
        while (*end && !xslt1_is_blank((unsigned char)*end)) end++;
        char *p = xslt1_strndup(prefix, (size_t)(end - prefix));
        XmlNs *ns = !strcmp(p, "#default") ? xml_ns_lookup_prefix(cur, NULL) : xml_ns_lookup_prefix(cur, p);
        if (!ns)
        {
            xslt1_transform_error(NULL, style, cur, "xsl:exclude-result-prefixes : undefined namespace %s\n", p);
            style->warnings++;
        }
        else if (xslt_excl_prefix_push(style, xml_ns_href(ns)) >= 0)
        {
            nb++;
        }
        free(p);
        prefix = end;
    }
    free(prefixes);
    return nb;
}

static void xslt_delete_node(XMQNode *n)
{
    xml_node_unlink(n);
    xml_node_free(n);
}

/** Port of xsltPreprocessStylesheet: strip whitespace and comments from the stylesheet. */
static void xslt_preprocess_stylesheet(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    if (!style || !cur) return;
    XMQNode *styleelem = (xslt1_is_xslt_elem(cur) && xslt1_is_xslt_name(cur, "stylesheet")) ? cur : NULL;
    XMQNode *doc_node = xml_dom_document(style->doc);
    XMQNode *delete_node = NULL;
    while (cur)
    {
        if (delete_node)
        {
            xslt_delete_node(delete_node);
            delete_node = NULL;
        }
        XmlNodeKind k = xml_node_kind(cur);
        if (k == XML_DOM_ELEMENT)
        {
            int excl_prefixes = 0;
            if (xslt1_is_xslt_elem(cur))
            {
                if (xslt1_is_xslt_name(cur, "text")) goto skip_children;
            }
            else
            {
                excl_prefixes = xslt_parse_stylesheet_exclude_prefix(style, cur, 0);
            }
            if (xml_node_first_ns_decl(cur) && style->excl_prefix_nr > 0)
            {
                XMQNode *root = xml_dom_root_element(style->doc);
                if (root && root != cur)
                {
                    XmlNs *ns = xml_node_first_ns_decl(cur);
                    while (ns)
                    {
                        XmlNs *next = xml_ns_next_decl(ns);
                        for (int i = 0; i < style->excl_prefix_nr; i++)
                        {
                            if (xml_ns_prefix(ns) && xslt1_streq(xml_ns_href(ns), style->excl_prefix_tab[i]))
                            {
                                // Move the declaration to the root, to avoid duplicating it without losing it.
                                xml_ns_move_decl_to(cur, ns, root);
                                break;
                            }
                        }
                        ns = next;
                    }
                }
            }
            // Prefixes declared locally are popped when going back.
            if (excl_prefixes > 0)
            {
                xslt_preprocess_stylesheet(style, xml_node_first_child(cur));
                for (; excl_prefixes > 0; excl_prefixes--) xslt_excl_prefix_pop(style);
                goto skip_children;
            }
        }
        else if (k == XML_DOM_TEXT)
        {
            if (xslt_is_blank_node(cur))
            {
                if (xslt_get_space_preserve(xml_node_parent(cur)) != 1) delete_node = cur;
            }
        }
        else if (k != XML_DOM_CDATA)
        {
            delete_node = cur;
            goto skip_children;
        }

        // A namespaced child of the stylesheet that is neither xslt nor an extension: skip its content.
        if (k == XML_DOM_ELEMENT && xml_node_ns(cur) && styleelem && xml_node_parent(cur) == styleelem &&
            !xslt1_streq(xslt_node_ns_uri(cur), XSLT_NAMESPACE) && !xslt_check_ext_uri(style, xslt_node_ns_uri(cur)))
        {
            goto skip_children;
        }
        else if (xml_node_first_child(cur))
        {
            cur = xml_node_first_child(cur);
            continue;
        }
skip_children:
        if (xml_node_next(cur))
        {
            cur = xml_node_next(cur);
            continue;
        }
        do
        {
            cur = xml_node_parent(cur);
            if (!cur) break;
            if (cur == doc_node)
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
    if (delete_node) xslt_delete_node(delete_node);
}

/** Port of xsltParseTemplateContent: precompute the instructions of a template body. */
static void xslt_parse_template_content(XmlLibxslt1Stylesheet *style, XMQNode *templ)
{
    if (!style || !templ || xml_node_kind(templ) == XML_DOM_NAMESPACE) return;
    XMQNode *cur = xml_node_first_child(templ);
    XMQNode *del = NULL;
    while (cur)
    {
        style->principal->op_count += 1;
        if (del)
        {
            xslt_delete_node(del);
            del = NULL;
        }
        if (xslt1_is_xslt_elem(cur))
        {
            xslt_style_pre_compute(style, cur);
            if (xslt1_is_xslt_name(cur, "text"))
            {
                // TODO (libxslt): move the processing of xsl:text to xsltPreprocessStylesheet.
                if (xml_node_first_child(cur))
                {
                    XMQNode *text = xml_node_first_child(cur);
                    int noesc = 0;
                    char *prop = xslt1_get_ns_prop_exact(cur, "disable-output-escaping", NULL);
                    if (prop)
                    {
                        if (!strcmp(prop, "yes")) noesc = 1;
                        else if (strcmp(prop, "no"))
                        {
                            xslt1_transform_error(NULL, style, cur, "xsl:text: disable-output-escaping allows only yes or no\n");
                            style->warnings++;
                        }
                        free(prop);
                    }
                    while (text)
                    {
                        if (xml_node_kind(text) == XML_DOM_COMMENT)
                        {
                            text = xml_node_next(text);
                            continue;
                        }
                        if (xml_node_kind(text) != XML_DOM_TEXT && xml_node_kind(text) != XML_DOM_CDATA)
                        {
                            xslt1_transform_error(NULL, style, cur, "xsltParseTemplateContent: xslt:text content problem\n");
                            style->errors++;
                            break;
                        }
                        // libxml2 xmlStringTextNoenc: the text is output without escaping.
                        if (noesc && xml_node_kind(text) != XML_DOM_CDATA) xml_node_set_name(text, "textnoenc");
                        text = xml_node_next(text);
                    }
                    // Replace xsl:text by its children.
                    if (!text)
                    {
                        text = xml_node_first_child(cur);
                        while (text)
                        {
                            XMQNode *next = xml_node_next(text);
                            xml_node_unlink(text);
                            xml_node_add_prev_sibling(cur, text);
                            text = next;
                        }
                    }
                }
                del = cur;
                goto skip_children;
            }
        }
        else if (xml_node_kind(cur) == XML_DOM_ELEMENT && xml_node_ns(cur) && style->ns_defs &&
                 xslt_check_ext_prefix(style, xml_ns_prefix(xml_node_ns(cur))))
        {
            // An extension element, compile it too.
            xslt_style_pre_compute(style, cur);
        }
        else if (xml_node_kind(cur) == XML_DOM_ELEMENT)
        {
            // A literal result element, precompile its attribute value templates.
            if (!xml_node_ns(cur) && style->default_alias)
            {
                xml_node_set_ns(cur, xml_ns_lookup_href(cur, style->default_alias));
            }
            for (XMQAttr *attr = xml_node_first_attr(cur); attr; attr = xml_attr_next(attr)) xslt_compile_attr(style, attr);
        }
        if (xml_node_first_child(cur))
        {
            cur = xml_node_first_child(cur);
            continue;
        }
skip_children:
        if (xml_node_next(cur))
        {
            cur = xml_node_next(cur);
            continue;
        }
        do
        {
            cur = xml_node_parent(cur);
            if (!cur) break;
            if (cur == templ)
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
    if (del) xslt_delete_node(del);

    // Skip the first params, then report and remove misplaced params.
    cur = xml_node_first_child(templ);
    while (cur)
    {
        if (xslt1_is_xslt_elem(cur) && !xslt1_is_xslt_name(cur, "param")) break;
        cur = xml_node_next(cur);
    }
    while (cur)
    {
        if (xslt1_is_xslt_elem(cur) && xslt1_is_xslt_name(cur, "param"))
        {
            XMQNode *param = cur;
            xslt1_transform_error(NULL, style, cur, "xsltParseTemplateContent: ignoring misplaced param element\n");
            style->warnings++;
            cur = xml_node_next(cur);
            xslt_delete_node(param);
        }
        else
        {
            break;
        }
    }
}

// Keys, port of xsltAddKey (keys.c).

static int xslt_skip_string(const char *cur, int end)
{
    if (!cur || end < 0) return -1;
    char limit;
    if (cur[end] == '\'' || cur[end] == '"') limit = cur[end];
    else return end;
    end++;
    while (cur[end])
    {
        if (cur[end] == limit) return end + 1;
        end++;
    }
    return -1;
}

static int xslt_skip_predicate(const char *cur, int end)
{
    if (!cur || end < 0) return -1;
    if (cur[end] != '[') return end;
    end++;
    int level = 0;
    while (cur[end])
    {
        if (cur[end] == '\'' || cur[end] == '"')
        {
            end = xslt_skip_string(cur, end);
            if (end <= 0) return -1;
            continue;
        }
        else if (cur[end] == '[')
        {
            level += 1;
        }
        else if (cur[end] == ']')
        {
            if (level == 0) return end + 1;
            level -= 1;
        }
        end++;
    }
    return -1;
}

static void xslt_free_key_def(XsltKeyDef *k)
{
    free(k->name);
    free(k->name_uri);
    free(k->match);
    free(k->use);
    xml_xpath_comp_free(k->comp);
    xml_xpath_comp_free(k->usecomp);
    free(k);
}

/** Port of xsltAddKey. */
static int xslt_add_key(XmlLibxslt1Stylesheet *style, const char *name, const char *name_uri, const char *match, const char *use, XMQNode *inst)
{
    if (!style || !name || !match || !use) return -1;
    XsltKeyDef *key = (XsltKeyDef*)calloc(1, sizeof(XsltKeyDef));
    key->name = xslt1_strdup(name);
    key->name_uri = xslt1_strdup(name_uri);
    key->match = xslt1_strdup(match);
    key->use = xslt1_strdup(use);
    key->inst = inst;

    // Split the | and register it as as many keys.
    char *pattern = NULL;
    int current = 0, end = 0, start;
    while (match[current])
    {
        start = current;
        while (xslt1_is_blank((unsigned char)match[current])) current++;
        end = current;
        while (match[end] && match[end] != '|')
        {
            if (match[end] == '[')
            {
                end = xslt_skip_predicate(match, end);
                if (end <= 0)
                {
                    xslt1_transform_error(NULL, style, inst, "xsl:key : 'match' pattern is malformed: %s", key->match);
                    style->errors++;
                    goto error;
                }
            }
            else
            {
                end++;
            }
        }
        if (current == end)
        {
            xslt1_transform_error(NULL, style, inst, "xsl:key : 'match' pattern is empty\n");
            style->errors++;
            goto error;
        }
        if (match[start] != '/') pattern = xslt1_strncat(pattern, "//", 2);
        pattern = xslt1_strncat(pattern, &match[start], (size_t)(end - start));
        if (match[end] == '|')
        {
            pattern = xslt1_strncat(pattern, "|", 1);
            end++;
        }
        current = end;
    }
    if (!pattern)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:key : 'match' pattern is empty\n");
        style->errors++;
        goto error;
    }
    // XSLT 1.0: it is an error for use or match to contain a variable reference.
    key->comp = xslt1_xpath_compile(&style->settings, inst, pattern, true);
    if (!key->comp)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:key : 'match' pattern compilation failed '%s'\n", pattern);
        style->errors++;
    }
    key->usecomp = xslt1_xpath_compile(&style->settings, inst, use, true);
    if (!key->usecomp)
    {
        xslt1_transform_error(NULL, style, inst, "xsl:key : 'use' expression compilation failed '%s'\n", use);
        style->errors++;
    }
    // Keep the provided order, add the new one at the end.
    if (!style->keys)
    {
        style->keys = key;
    }
    else
    {
        XsltKeyDef *prev = style->keys;
        while (prev->next) prev = prev->next;
        prev->next = key;
    }
    key->next = NULL;
    key = NULL;
error:
    free(pattern);
    if (key) xslt_free_key_def(key);
    return 0;
}

/** Port of xsltParseStylesheetKey. */
static void xslt_parse_stylesheet_key(XmlLibxslt1Stylesheet *style, XMQNode *key)
{
    if (!style || !key || xml_node_kind(key) != XML_DOM_ELEMENT) return;
    char *name = NULL, *name_uri = NULL, *match = NULL, *use = NULL;
    char *prop = xslt1_get_ns_prop_exact(key, "name", NULL);
    if (prop)
    {
        const char *uri = xslt1_get_qname_uri(&style->settings, key, &prop);
        if (!prop)
        {
            style->errors++;
            goto error;
        }
        name = prop;
        if (uri) name_uri = xslt1_strdup(uri);
    }
    else
    {
        xslt1_transform_error(NULL, style, key, "xsl:key : error missing name\n");
        style->errors++;
        goto error;
    }
    match = xslt1_get_ns_prop_exact(key, "match", NULL);
    if (!match)
    {
        xslt1_transform_error(NULL, style, key, "xsl:key : error missing match\n");
        style->errors++;
        goto error;
    }
    use = xslt1_get_ns_prop_exact(key, "use", NULL);
    if (!use)
    {
        xslt1_transform_error(NULL, style, key, "xsl:key : error missing use\n");
        style->errors++;
        goto error;
    }
    xslt_add_key(style, name, name_uri, match, use, key);
error:
    free(use);
    free(match);
    free(name);
    free(name_uri);
    if (xml_node_first_child(key)) xslt_parse_content_error(style, xml_node_first_child(key));
}

/** Port of xsltParseStylesheetTemplate. */
static void xslt_parse_stylesheet_template(XmlLibxslt1Stylesheet *style, XMQNode *template_elem)
{
    if (!style || !template_elem || xml_node_kind(template_elem) != XML_DOM_ELEMENT) return;
    XsltTemplate *ret = xslt_new_template();
    ret->next = style->templates;
    style->templates = ret;
    ret->style = style;
    xslt_get_inherited_ns_list(style, ret, template_elem);

    char *prop = xslt1_get_ns_prop_exact(template_elem, "mode", NULL);
    if (prop)
    {
        const char *uri = xslt1_get_qname_uri(&style->settings, template_elem, &prop);
        if (!prop)
        {
            style->errors++;
            return;
        }
        ret->mode = prop;
        ret->mode_uri = xslt1_strdup(uri);
    }
    prop = xslt1_get_ns_prop_exact(template_elem, "match", NULL);
    if (prop) xslt_set_string(&ret->match, prop);
    prop = xslt1_get_ns_prop_exact(template_elem, "priority", NULL);
    if (prop)
    {
        ret->priority = (float)xml_xpath_string_to_number(prop);
        free(prop);
    }
    prop = xslt1_get_ns_prop_exact(template_elem, "name", NULL);
    if (prop)
    {
        const char *uri = xslt1_get_qname_uri(&style->settings, template_elem, &prop);
        if (!prop)
        {
            style->errors++;
            return;
        }
        if (xslt_validate_ncname(prop))
        {
            xslt1_transform_error(NULL, style, template_elem, "xsl:template : error invalid name '%s'\n", prop);
            style->errors++;
            free(prop);
            return;
        }
        ret->name = prop;
        ret->name_uri = uri ? xslt1_strdup(uri) : NULL;
    }
    // Parse the content and register the pattern.
    xslt_parse_template_content(style, template_elem);
    ret->elem = template_elem;
    ret->content = xml_node_first_child(template_elem);
    xslt_add_template(style, ret, ret->mode, ret->mode_uri);
}

// Attribute sets, port of libxslt attributes.c (the compilation).

static XsltAttrSet *xslt_find_attr_set(XmlLibxslt1Stylesheet *style, const char *ncname, const char *ns)
{
    if (!style->attribute_sets) return NULL;
    char *key = xslt1_key2(ncname, ns);
    XsltAttrSet *set = (XsltAttrSet*)hashmap_get(style->attribute_sets, key);
    free(key);
    return set;
}

static void xslt_add_attr_set(XmlLibxslt1Stylesheet *style, XsltAttrSet *set)
{
    if (!style->attribute_sets) style->attribute_sets = hashmap_create(10);
    char *key = xslt1_key2(set->name, set->ns);
    hashmap_put(style->attribute_sets, key, set);
    free(key);
    // Keep the declaration order.
    set->next_in_order = NULL;
    XsltAttrSet **pp = &style->attribute_set_list;
    while (*pp) pp = &(*pp)->next_in_order;
    *pp = set;
}

static void xslt_remove_attr_set(XmlLibxslt1Stylesheet *style, XsltAttrSet *set)
{
    char *key = xslt1_key2(set->name, set->ns);
    hashmap_remove(style->attribute_sets, key);
    free(key);
    XsltAttrSet **pp = &style->attribute_set_list;
    while (*pp && *pp != set) pp = &(*pp)->next_in_order;
    if (*pp) *pp = set->next_in_order;
}

/** Port of xsltParseStylesheetAttributeSet. */
static void xslt_parse_stylesheet_attribute_set(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    char *value = xslt1_get_ns_prop_exact(cur, "name", NULL);
    if (!value || !*value)
    {
        xslt1_generic_error(&style->settings, "xsl:attribute-set : name is missing\n");
        free(value);
        return;
    }
    if (xslt1_validate_qname(value))
    {
        xslt1_transform_error(NULL, style, cur, "xsl:attribute-set : The name '%s' is not a valid QName.\n", value);
        style->errors++;
        free(value);
        return;
    }
    char *prefix = NULL;
    char *ncname = xslt1_split_qname(value, &prefix);
    free(value);
    const char *ns_uri = NULL;
    if (prefix)
    {
        XmlNs *ns = xml_ns_lookup_prefix(cur, prefix);
        if (!ns)
        {
            xslt1_transform_error(NULL, style, cur, "xsl:attribute-set : No namespace found for QName '%s:%s'\n", prefix, ncname);
            style->errors++;
            free(prefix);
            free(ncname);
            return;
        }
        ns_uri = xml_ns_href(ns);
    }
    XsltAttrSet *set = xslt_find_attr_set(style, ncname, ns_uri);
    if (!set)
    {
        set = (XsltAttrSet*)calloc(1, sizeof(XsltAttrSet));
        set->name = xslt1_strdup(ncname);
        set->ns = xslt1_strdup(ns_uri);
        xslt_add_attr_set(style, set);
    }
    free(prefix);
    free(ncname);

    // Only xsl:attribute elements are allowed.
    for (XMQNode *child = xml_node_first_child(cur); child; child = xml_node_next(child))
    {
        if (xml_node_kind(child) != XML_DOM_ELEMENT || !xml_node_ns(child) || !xslt1_is_xslt_elem(child))
        {
            if (xml_node_kind(child) == XML_DOM_ELEMENT)
                xslt1_transform_error(NULL, style, child, "xsl:attribute-set : unexpected child %s\n", xml_node_name(child));
            else
                xslt1_transform_error(NULL, style, child, "xsl:attribute-set : child of unexpected type\n");
        }
        else if (!xslt1_is_xslt_name(child, "attribute"))
        {
            xslt1_transform_error(NULL, style, child, "xsl:attribute-set : unexpected child xsl:%s\n", xml_node_name(child));
        }
        else
        {
            xslt_style_pre_compute(style, child);
            if (xml_node_first_child(child)) xslt_parse_template_content(style, child);
            if (!xml_node_user_data(child))
            {
                xslt1_transform_error(NULL, style, child, "xsl:attribute-set : internal error, attribute %s not compiled\n", xml_node_name(child));
            }
            else
            {
                XsltAttrElem *e = (XsltAttrElem*)calloc(1, sizeof(XsltAttrElem));
                e->attr = child;
                XsltAttrElem **pp = &set->attrs;
                while (*pp) pp = &(*pp)->next;
                *pp = e;
            }
        }
    }

    // The attribute use-attribute-sets.
    value = xslt1_get_ns_prop_exact(cur, "use-attribute-sets", NULL);
    if (value)
    {
        const char *curval = value;
        while (*curval)
        {
            while (xslt1_is_blank((unsigned char)*curval)) curval++;
            if (!*curval) break;
            const char *endval = curval;
            while (*endval && !xslt1_is_blank((unsigned char)*endval)) endval++;
            char *qn = xslt1_strndup(curval, (size_t)(endval - curval));
            if (xslt1_validate_qname(qn))
            {
                xslt1_transform_error(NULL, style, cur, "xsl:attribute-set : The name '%s' in use-attribute-sets is not a valid QName.\n", qn);
                style->errors++;
                free(qn);
                free(value);
                return;
            }
            char *prefix2 = NULL;
            char *ncname2 = xslt1_split_qname(qn, &prefix2);
            const char *ns_uri2 = NULL;
            if (prefix2)
            {
                XmlNs *ns2 = xml_ns_lookup_prefix(cur, prefix2);
                if (!ns2)
                {
                    xslt1_transform_error(NULL, style, cur, "xsl:attribute-set : No namespace found for QName '%s:%s' in use-attribute-sets\n",
                                         prefix2, ncname2);
                    style->errors++;
                    free(prefix2);
                    free(ncname2);
                    free(qn);
                    free(value);
                    return;
                }
                ns_uri2 = xml_ns_href(ns2);
            }
            // xsltAddUseAttrSetList: no duplicates.
            bool dup = false;
            XsltUseAttrSet **pp = &set->use_attr_sets;
            while (*pp)
            {
                if (xslt1_streq((*pp)->ncname, ncname2) && xslt1_streq((*pp)->ns, ns_uri2)) dup = true;
                pp = &(*pp)->next;
            }
            if (!dup)
            {
                XsltUseAttrSet *u = (XsltUseAttrSet*)calloc(1, sizeof(XsltUseAttrSet));
                u->ncname = ncname2;
                u->ns = xslt1_strdup(ns_uri2);
                *pp = u;
                ncname2 = NULL;
            }
            free(prefix2);
            free(ncname2);
            free(qn);
            curval = endval;
        }
        free(value);
    }
}

/** Port of xsltMergeAttrSets. */
static void xslt_merge_attr_sets(XsltAttrSet *set, XsltAttrSet *other)
{
    for (XsltAttrElem *old = other->attrs; old; old = old->next)
    {
        // Check that the attribute is not yet in the list.
        XsltAttrElem *cur = set->attrs;
        int add = 1;
        while (cur)
        {
            XsltStylePreComp *cur_comp = (XsltStylePreComp*)xml_node_user_data(cur->attr);
            XsltStylePreComp *old_comp = (XsltStylePreComp*)xml_node_user_data(old->attr);
            if (xslt1_streq(cur_comp->name, old_comp->name) && xslt1_streq(cur_comp->ns, old_comp->ns))
            {
                add = 0;
                break;
            }
            if (!cur->next) break;
            cur = cur->next;
        }
        if (add)
        {
            XsltAttrElem *e = (XsltAttrElem*)calloc(1, sizeof(XsltAttrElem));
            e->attr = old->attr;
            if (!cur) set->attrs = e;
            else cur->next = e;
        }
    }
}

static void xslt_resolve_attr_set(XsltAttrSet *set, XmlLibxslt1Stylesheet *top_style, XmlLibxslt1Stylesheet *style,
                                  const char *name, const char *ns, int depth);

/** Port of xsltResolveUseAttrSets. */
static void xslt_resolve_use_attr_sets(XsltAttrSet *set, XmlLibxslt1Stylesheet *top_style, int depth)
{
    XsltUseAttrSet *use = set->use_attr_sets;
    while (use)
    {
        // Iterate the top stylesheet and all imports.
        for (XmlLibxslt1Stylesheet *cur = top_style; cur; cur = xslt1_next_import(cur))
        {
            XsltAttrSet *other = xslt_find_attr_set(cur, use->ncname, use->ns);
            if (other)
            {
                xslt_resolve_attr_set(other, top_style, cur, use->ncname, use->ns, depth + 1);
                xslt_merge_attr_sets(set, other);
                break;
            }
        }
        XsltUseAttrSet *next = use->next;
        free(use->ncname);
        free(use->ns);
        free(use);
        use = next;
    }
    set->use_attr_sets = NULL;
}

/** Port of xsltResolveAttrSet. */
static void xslt_resolve_attr_set(XsltAttrSet *set, XmlLibxslt1Stylesheet *top_style, XmlLibxslt1Stylesheet *style,
                                  const char *name, const char *ns, int depth)
{
    if (set->state == XSLT_ATTRSET_RESOLVED) return;
    if (set->state == XSLT_ATTRSET_RESOLVING)
    {
        xslt1_transform_error(NULL, top_style, NULL, "xsl:attribute-set : use-attribute-sets recursion detected on %s\n", name);
        top_style->errors++;
        set->state = XSLT_ATTRSET_RESOLVED;
        return;
    }
    if (depth > 100)
    {
        xslt1_transform_error(NULL, top_style, NULL, "xsl:attribute-set : use-attribute-sets maximum recursion depth exceeded on %s\n", name);
        top_style->errors++;
        return;
    }
    set->state = XSLT_ATTRSET_RESOLVING;
    xslt_resolve_use_attr_sets(set, top_style, depth);
    // Merge the imported sets.
    for (XmlLibxslt1Stylesheet *cur = xslt1_next_import(style); cur; cur = xslt1_next_import(cur))
    {
        XsltAttrSet *other = xslt_find_attr_set(cur, name, ns);
        if (other)
        {
            xslt_resolve_use_attr_sets(other, top_style, depth);
            xslt_merge_attr_sets(set, other);
            xslt_remove_attr_set(cur, other);
            xslt_free_attr_set(other);
        }
    }
    set->state = XSLT_ATTRSET_RESOLVED;
}

/** Port of xsltResolveStylesheetAttributeSet: resolve all sets and move them to the top stylesheet.
    Resolving a set can free sets of later imports, so the sets are looked up by name. */
static void xslt_resolve_stylesheet_attribute_set(XmlLibxslt1Stylesheet *style)
{
    int error = 0;
    for (XmlLibxslt1Stylesheet *cur = style; cur; cur = xslt1_next_import(cur))
    {
        if (!cur->attribute_set_list) continue;
        size_t n = 0;
        for (XsltAttrSet *set = cur->attribute_set_list; set; set = set->next_in_order) n++;
        char **names = (char**)calloc(2 * n, sizeof(char*));
        size_t i = 0;
        for (XsltAttrSet *set = cur->attribute_set_list; set; set = set->next_in_order, i++)
        {
            names[2 * i] = xslt1_strdup(set->name);
            names[2 * i + 1] = xslt1_strdup(set->ns);
        }
        for (i = 0; i < n; i++)
        {
            const char *name = names[2 * i];
            const char *ns = names[2 * i + 1];
            XsltAttrSet *set = xslt_find_attr_set(cur, name, ns);
            if (!set) continue;
            if (error)
            {
                if (cur != style)
                {
                    xslt_remove_attr_set(cur, set);
                    xslt_free_attr_set(set);
                }
                continue;
            }
            xslt_resolve_attr_set(set, style, cur, name, ns, 1);
            if (cur != style)
            {
                // Move the attribute set to the top stylesheet.
                xslt_remove_attr_set(cur, set);
                if (xslt_find_attr_set(style, name, ns))
                {
                    xslt1_generic_error(&style->settings, "xsl:attribute-set : internal error, can't move imported  attribute set %s\n", name);
                    error = 1;
                    xslt_free_attr_set(set);
                }
                else
                {
                    xslt_add_attr_set(style, set);
                }
            }
        }
        for (i = 0; i < 2 * n; i++) free(names[i]);
        free(names);
    }
}

// Namespace aliases, port of xsltNamespaceAlias (namespaces.c).

static void xslt_namespace_alias(XmlLibxslt1Stylesheet *style, XMQNode *node)
{
    if (!style || !node) return;
    char *style_prefix = xslt1_get_ns_prop_exact(node, "stylesheet-prefix", NULL);
    char *result_prefix = NULL;
    if (!style_prefix)
    {
        xslt1_transform_error(NULL, style, node, "namespace-alias: stylesheet-prefix attribute missing\n");
        return;
    }
    result_prefix = xslt1_get_ns_prop_exact(node, "result-prefix", NULL);
    if (!result_prefix)
    {
        xslt1_transform_error(NULL, style, node, "namespace-alias: result-prefix attribute missing\n");
        goto error;
    }
    const char *literal_ns_name;
    const char *target_ns_name;
    XmlNs *literal_ns, *target_ns;
    if (!strcmp(style_prefix, "#default"))
    {
        literal_ns = xml_ns_lookup_prefix(node, NULL);
        literal_ns_name = literal_ns ? xml_ns_href(literal_ns) : NULL;
    }
    else
    {
        literal_ns = xml_ns_lookup_prefix(node, style_prefix);
        if (!literal_ns || !xml_ns_href(literal_ns))
        {
            xslt1_transform_error(NULL, style, node, "namespace-alias: prefix %s not bound to any namespace\n", style_prefix);
            goto error;
        }
        literal_ns_name = xml_ns_href(literal_ns);
    }
    // For #default without a default namespace, UNDEFINED_DEFAULT_NS is used.
    if (!strcmp(result_prefix, "#default"))
    {
        target_ns = xml_ns_lookup_prefix(node, NULL);
        target_ns_name = target_ns ? xml_ns_href(target_ns) : XSLT_UNDEFINED_DEFAULT_NS;
    }
    else
    {
        target_ns = xml_ns_lookup_prefix(node, result_prefix);
        if (!target_ns || !xml_ns_href(target_ns))
        {
            xslt1_transform_error(NULL, style, node, "namespace-alias: prefix %s not bound to any namespace\n", result_prefix);
            goto error;
        }
        target_ns_name = xml_ns_href(target_ns);
    }
    if (!literal_ns_name)
    {
        // #default without a default namespace in scope.
        if (target_ns) xslt_set_string(&style->default_alias, xslt1_strdup(xml_ns_href(target_ns)));
    }
    else
    {
        if (!style->ns_aliases) style->ns_aliases = hashmap_create(10);
        // xmlHashAddEntry: the first alias wins.
        if (!hashmap_get(style->ns_aliases, literal_ns_name))
        {
            hashmap_put(style->ns_aliases, literal_ns_name, xslt1_strdup(target_ns_name ? target_ns_name : ""));
        }
    }
error:
    free(style_prefix);
    free(result_prefix);
}

// Global variables, port of xsltParseGlobalVariable/Param (variables.c).

static int xslt_register_global_variable(XmlLibxslt1Stylesheet *style, const char *name, const char *ns_uri, const char *sel,
                                         XMQNode *tree, XsltStylePreComp *comp, const char *value)
{
    if (!style || !name || !comp) return -1;
    XsltStackElem *elem = (XsltStackElem*)calloc(1, sizeof(XsltStackElem));
    elem->comp = comp;
    elem->name = xslt1_strdup(name);
    elem->select = xslt1_strdup(sel);
    elem->name_uri = xslt1_strdup(ns_uri);
    elem->tree = tree;
    for (XsltStackElem *tmp = style->variables; tmp; tmp = tmp->next)
    {
        if (elem->comp->type == XSLT_FUNC_VARIABLE && tmp->comp->type == XSLT_FUNC_VARIABLE &&
            xslt1_streq(elem->name, tmp->name) && xslt1_streq(elem->name_uri, tmp->name_uri))
        {
            xslt1_transform_error(NULL, style, comp->inst, "redefinition of global variable %s\n", elem->name);
            style->errors++;
        }
    }
    elem->next = style->variables;
    style->variables = elem;
    if (value)
    {
        elem->computed = 1;
        elem->value = xml_xpath_result_new_string(value);
    }
    return 0;
}

static void xslt_parse_global_variable(XmlLibxslt1Stylesheet *style, XMQNode *cur, bool param)
{
    if (!cur || !style || xml_node_kind(cur) != XML_DOM_ELEMENT) return;
    const char *what = param ? "xsl:param" : "xsl:variable";
    xslt_style_pre_compute(style, cur);
    XsltStylePreComp *comp = (XsltStylePreComp*)xml_node_user_data(cur);
    if (!comp || comp == XSLT_EXT_MARKER)
    {
        xslt1_transform_error(NULL, style, cur, "%s : compilation failed\n", what);
        return;
    }
    if (!comp->name)
    {
        xslt1_transform_error(NULL, style, cur, "%s : missing name attribute\n", what);
        return;
    }
    // Parse the content (a sequence constructor).
    if (xml_node_first_child(cur)) xslt_parse_template_content(style, cur);
    xslt_register_global_variable(style, comp->name, comp->ns, comp->select, xml_node_first_child(cur), comp, NULL);
}

// Imports and includes, port of libxslt imports.c.

static int xslt_parse_stylesheet_user(XmlLibxslt1Stylesheet *style, XmlDom *doc);
static XmlLibxslt1Stylesheet *xslt_parse_stylesheet_process(XmlLibxslt1Stylesheet *ret, XmlDom *doc);

/** Port of xsltNormalizeCompSteps and xsltFixImportedCompSteps: only the first pattern of
    each name list of the template hash is renumbered, like libxslt. */
static void xslt_fix_imported_comp_steps(XmlLibxslt1Stylesheet *master, XmlLibxslt1Stylesheet *style)
{
    if (style->templates_hash)
    {
        HashMapIterator *i = hashmap_iterate(style->templates_hash);
        const char *key;
        void *val;
        while (hashmap_next_key_value(i, &key, &val))
        {
            XmlLibxslt1Pattern *comp = (XmlLibxslt1Pattern*)val;
            if (!comp) continue;
            for (int ix = 0; ix < comp->nb_step; ix++)
            {
                comp->steps[ix].previous_extra += master->extras_nr;
                comp->steps[ix].index_extra += master->extras_nr;
                comp->steps[ix].len_extra += master->extras_nr;
            }
        }
        hashmap_free_iterator(i);
    }
    master->extras_nr += style->extras_nr;
    for (XmlLibxslt1Stylesheet *res = style->imports; res; res = res->next) xslt_fix_imported_comp_steps(master, res);
}

/** Port of xsltCheckCycle. */
static int xslt_check_cycle(XmlLibxslt1Stylesheet *style, XMQNode *cur, const char *uri)
{
    int depth = 0;
    for (XmlLibxslt1Stylesheet *ancestor = style; ancestor; ancestor = ancestor->parent)
    {
        if (++depth >= XSLT_MAX_NESTING)
        {
            xslt1_transform_error(NULL, style, cur, "maximum nesting depth exceeded: %s\n", uri);
            return -1;
        }
        if (ancestor->doc && xslt1_streq(xml_dom_url(ancestor->doc), uri))
        {
            xslt1_transform_error(NULL, style, cur, "recursion detected on imported URL %s\n", uri);
            return -1;
        }
        for (XsltDocument *docptr = ancestor->includes; docptr; docptr = docptr->includes)
        {
            if (++depth >= XSLT_MAX_NESTING)
            {
                xslt1_transform_error(NULL, style, cur, "maximum nesting depth exceeded: %s\n", uri);
                return -1;
            }
            if (xslt1_streq(xml_dom_url(docptr->doc), uri))
            {
                xslt1_transform_error(NULL, style, cur, "recursion detected on included URL %s\n", uri);
                return -1;
            }
        }
    }
    return 0;
}

/** Port of xsltParseStylesheetImportedDoc. */
static XmlLibxslt1Stylesheet *xslt_parse_stylesheet_imported_doc(XmlDom *doc, XmlLibxslt1Stylesheet *parent_style, const XmlLibxslt1Settings *settings)
{
    if (!doc) return NULL;
    XmlLibxslt1Stylesheet *ret = xslt_new_stylesheet_internal(parent_style, settings);
    if (xslt_parse_stylesheet_user(ret, doc) != 0)
    {
        xslt_free_stylesheet(ret);
        return NULL;
    }
    ret->owns_doc = 1;
    return ret;
}

/** Port of xsltParseStylesheetImport. */
static int xslt_parse_stylesheet_import(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    int ret = -1;
    char *base = NULL, *uri = NULL;
    char *uri_ref = xslt1_get_ns_prop_exact(cur, "href", NULL);
    if (!uri_ref)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:import : missing href attribute\n");
        goto error;
    }
    base = xslt1_node_get_base(style->doc, cur);
    uri = xml_uri_build(uri_ref, base);
    if (!uri)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:import : invalid URI reference %s\n", uri_ref);
        goto error;
    }
    if (xslt_check_cycle(style, cur, uri) < 0) goto error;
    XmlDom *import;
    import = xslt1_load(style, uri, XML_LIBXSLT1_LOAD_STYLESHEET);
    if (!import)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:import : unable to load %s\n", uri);
        goto error;
    }
    XmlLibxslt1Stylesheet *res;
    res = xslt_parse_stylesheet_imported_doc(import, style, NULL);
    if (res)
    {
        res->next = style->imports;
        style->imports = res;
        if (!style->parent) xslt_fix_imported_comp_steps(style, res);
        ret = 0;
    }
    else
    {
        xml_dom_free(import);
    }
error:
    free(uri_ref);
    free(base);
    free(uri);
    return ret;
}

/** Port of xsltLoadStyleDocument (documents.c). */
static XsltDocument *xslt_load_style_document(XmlLibxslt1Stylesheet *style, const char *uri)
{
    for (XsltDocument *ret = style->doc_list; ret; ret = ret->next)
    {
        if (ret->doc && xml_dom_url(ret->doc) && xslt1_streq(xml_dom_url(ret->doc), uri)) return ret;
    }
    XmlDom *doc = xslt1_load(style, uri, XML_LIBXSLT1_LOAD_STYLESHEET);
    if (!doc) return NULL;
    XsltDocument *ret = (XsltDocument*)calloc(1, sizeof(XsltDocument));
    ret->doc = doc;
    ret->next = style->doc_list;
    style->doc_list = ret;
    return ret;
}

/** Port of xsltParseStylesheetInclude. */
static int xslt_parse_stylesheet_include(XmlLibxslt1Stylesheet *style, XMQNode *cur)
{
    int ret = -1;
    char *base = NULL, *uri = NULL;
    char *uri_ref = xslt1_get_ns_prop_exact(cur, "href", NULL);
    if (!uri_ref)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:include : missing href attribute\n");
        goto error;
    }
    base = xslt1_node_get_base(style->doc, cur);
    uri = xml_uri_build(uri_ref, base);
    if (!uri)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:include : invalid URI reference %s\n", uri_ref);
        goto error;
    }
    if (xslt_check_cycle(style, cur, uri) < 0) goto error;
    XsltDocument *include;
    include = xslt_load_style_document(style, uri);
    if (!include)
    {
        xslt1_transform_error(NULL, style, cur, "xsl:include : unable to load %s\n", uri);
        goto error;
    }
    XmlDom *old_doc;
    old_doc = style->doc;
    style->doc = include->doc;
    // Chain to the stylesheet for the recursion check.
    include->includes = style->includes;
    style->includes = include;
    int old_nopreproc;
    old_nopreproc = style->nopreproc;
    style->nopreproc = include->preproc;
    XmlLibxslt1Stylesheet *result;
    result = xslt_parse_stylesheet_process(style, include->doc);
    style->nopreproc = old_nopreproc;
    include->preproc = 1;
    style->includes = include->includes;
    style->doc = old_doc;
    ret = result ? 0 : -1;
error:
    free(uri_ref);
    free(base);
    free(uri);
    return ret;
}

/** Port of xsltParseStylesheetTop. */
static void xslt_parse_stylesheet_top(XmlLibxslt1Stylesheet *style, XMQNode *top)
{
    if (!top || xml_node_kind(top) != XML_DOM_ELEMENT) return;
    char *prop = xslt1_get_ns_prop_exact(top, "version", NULL);
    if (!prop)
    {
        xslt1_transform_error(NULL, style, top, "xsl:version is missing: document may not be a stylesheet\n");
        style->warnings++;
    }
    else
    {
        if (strcmp(prop, "1.0") && strcmp(prop, "1.1"))
        {
            xslt1_transform_error(NULL, style, top, "xsl:version: only 1.1 features are supported\n");
            style->forwards_compatible = 1;
            style->warnings++;
        }
        free(prop);
    }

    // The xsl:import elements.
    XMQNode *cur = xml_node_first_child(top);
    while (cur)
    {
        style->principal->op_count += 1;
        if (xslt_is_blank_node(cur))
        {
            cur = xml_node_next(cur);
            continue;
        }
        if (xslt1_is_xslt_elem(cur) && xslt1_is_xslt_name(cur, "import"))
        {
            if (xslt_parse_stylesheet_import(style, cur) != 0) style->errors++;
        }
        else
        {
            break;
        }
        cur = xml_node_next(cur);
    }

    // The other top level elements.
    while (cur)
    {
        style->principal->op_count += 1;
        if (xslt_is_blank_node(cur))
        {
            cur = xml_node_next(cur);
            continue;
        }
        if (xml_node_kind(cur) == XML_DOM_TEXT)
        {
            if (xml_node_content(cur)) xslt1_transform_error(NULL, style, cur, "misplaced text node: '%s'\n", xml_node_content(cur));
            style->errors++;
            cur = xml_node_next(cur);
            continue;
        }
        if (xml_node_kind(cur) == XML_DOM_ELEMENT && !xml_node_ns(cur))
        {
            xslt1_generic_error(&style->settings, "Found a top-level element %s with null namespace URI\n", xml_node_name(cur));
            style->errors++;
            cur = xml_node_next(cur);
            continue;
        }
        if (xml_node_kind(cur) == XML_DOM_ELEMENT && !xslt1_is_xslt_elem(cur))
        {
            // No extension module has top level elements.
            cur = xml_node_next(cur);
            continue;
        }
        if (xml_node_kind(cur) != XML_DOM_ELEMENT)
        {
            // libxslt compares the names of other nodes too.
        }
        if (xslt1_is_xslt_name(cur, "import"))
        {
            xslt1_transform_error(NULL, style, cur, "xsltParseStylesheetTop: ignoring misplaced import element\n");
            style->errors++;
        }
        else if (xslt1_is_xslt_name(cur, "include"))
        {
            if (xslt_parse_stylesheet_include(style, cur) != 0) style->errors++;
        }
        else if (xslt1_is_xslt_name(cur, "strip-space")) xslt_parse_stylesheet_space(style, cur, true);
        else if (xslt1_is_xslt_name(cur, "preserve-space")) xslt_parse_stylesheet_space(style, cur, false);
        else if (xslt1_is_xslt_name(cur, "output")) xslt_parse_stylesheet_output(style, cur);
        else if (xslt1_is_xslt_name(cur, "key")) xslt_parse_stylesheet_key(style, cur);
        else if (xslt1_is_xslt_name(cur, "decimal-format")) xslt_parse_stylesheet_decimal_format(style, cur);
        else if (xslt1_is_xslt_name(cur, "attribute-set")) xslt_parse_stylesheet_attribute_set(style, cur);
        else if (xslt1_is_xslt_name(cur, "variable")) xslt_parse_global_variable(style, cur, false);
        else if (xslt1_is_xslt_name(cur, "param")) xslt_parse_global_variable(style, cur, true);
        else if (xslt1_is_xslt_name(cur, "template")) xslt_parse_stylesheet_template(style, cur);
        else if (xslt1_is_xslt_name(cur, "namespace-alias")) xslt_namespace_alias(style, cur);
        else
        {
            if (style->forwards_compatible == 0)
            {
                xslt1_transform_error(NULL, style, cur, "xsltParseStylesheetTop: unknown %s element\n",
                                     xml_node_name(cur) ? xml_node_name(cur) : "(null)");
                style->errors++;
            }
        }
        cur = xml_node_next(cur);
    }
}

/** Port of xsltParseStylesheetProcess. */
static XmlLibxslt1Stylesheet *xslt_parse_stylesheet_process(XmlLibxslt1Stylesheet *ret, XmlDom *doc)
{
    if (!doc || !ret) return NULL;
    XMQNode *cur = xml_dom_root_element(doc);
    if (!cur)
    {
        xslt1_transform_error(NULL, ret, xml_dom_document(doc), "xsltParseStylesheetProcess : empty stylesheet\n");
        return NULL;
    }
    if (xslt1_is_xslt_elem(cur) && (xslt1_is_xslt_name(cur, "stylesheet") || xslt1_is_xslt_name(cur, "transform")))
    {
        ret->literal_result = 0;
        xslt_parse_stylesheet_exclude_prefix(ret, cur, 1);
        xslt_parse_stylesheet_ext_prefix(ret, cur, 1);
    }
    else
    {
        xslt_parse_stylesheet_exclude_prefix(ret, cur, 0);
        xslt_parse_stylesheet_ext_prefix(ret, cur, 0);
        ret->literal_result = 1;
    }
    if (!ret->nopreproc) xslt_preprocess_stylesheet(ret, cur);
    if (ret->literal_result == 0)
    {
        xslt_parse_stylesheet_top(ret, cur);
    }
    else
    {
        // The document itself might be the template, check xsl:version.
        char *prop = xslt1_get_ns_prop_exact(cur, "version", XSLT_NAMESPACE);
        if (!prop)
        {
            xslt1_transform_error(NULL, ret, cur, "xsltParseStylesheetProcess : document is not a stylesheet\n");
            return NULL;
        }
        if (strcmp(prop, "1.0") && strcmp(prop, "1.1"))
        {
            xslt1_transform_error(NULL, ret, cur, "xsl:version: only 1.1 features are supported\n");
            ret->forwards_compatible = 1;
            ret->warnings++;
        }
        free(prop);
        XsltTemplate *templ = xslt_new_template();
        templ->next = ret->templates;
        ret->templates = templ;
        templ->match = xslt1_strdup("/");
        // Parse the content and register the pattern.
        xslt_parse_template_content(ret, xml_dom_document(doc));
        templ->elem = xml_dom_document(doc);
        templ->content = xml_node_first_child(xml_dom_document(doc));
        xslt_add_template(ret, templ, NULL, NULL);
        ret->literal_result = 1;
    }
    return ret;
}

/** Port of xsltParseStylesheetUser. */
static int xslt_parse_stylesheet_user(XmlLibxslt1Stylesheet *style, XmlDom *doc)
{
    if (!style || !doc) return -1;
    // libxslt calls xsltGatherNamespaces here, before style->doc is set, so it does nothing.
    style->doc = doc;
    if (!xslt_parse_stylesheet_process(style, doc))
    {
        style->doc = NULL;
        return -1;
    }
    if (!style->parent) xslt_resolve_stylesheet_attribute_set(style);
    if (style->errors != 0)
    {
        // Detach the doc, it is not freed with the stylesheet.
        style->doc = NULL;
        return -1;
    }
    return 0;
}

XmlLibxslt1Stylesheet *xml_libxslt1_compile(XmlDom *doc, const XmlLibxslt1Settings *settings)
{
    XmlLibxslt1Settings defaults;
    if (!settings)
    {
        xml_libxslt1_settings_default(&defaults);
        settings = &defaults;
    }
    return xslt_parse_stylesheet_imported_doc(doc, NULL, settings);
}

// Description of a compiled stylesheet, for the differential tests.

static void xslt_d_str(MemBuffer *mb, const char *label, const char *s)
{
    membuffer_printf(mb, " %s=", label);
    if (!s)
    {
        membuffer_append(mb, "(null)");
        return;
    }
    membuffer_append_char(mb, '"');
    for (const char *p = s; *p; ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\n') membuffer_append(mb, "\\n");
        else if (c == '"') membuffer_append(mb, "\\\"");
        else if (c == '\\') membuffer_append(mb, "\\\\");
        else if (c < 32) membuffer_printf(mb, "\\x%02x", c);
        else membuffer_append_char(mb, (char)c);
    }
    membuffer_append_char(mb, '"');
}

static void xslt_d_indent(MemBuffer *mb, int depth)
{
    for (int i = 0; i < depth; ++i) membuffer_append(mb, "  ");
}

static const char *xslt_style_type_name(int type)
{
    static const char *names[] = { "?", "copy", "sort", "text", "element", "attribute", "comment", "pi", "copy-of",
        "value-of", "number", "apply-imports", "call-template", "apply-templates", "choose", "if", "for-each",
        "document", "with-param", "param", "variable", "when", "extension" };
    if (type < 0 || type > XSLT_FUNC_EXTENSION) return "?";
    return names[type];
}

/** The hash keys are printed with | between the parts and (null) for missing ones. */
static char *xslt_d_key(const char *key)
{
    MemBuffer *mb = new_membuffer();
    for (const char *p = key; *p; ++p)
    {
        if (*p == 1) membuffer_append_char(mb, '|');
        else if (*p == 2) membuffer_append(mb, "(null)");
        else membuffer_append_char(mb, *p);
    }
    membuffer_append_null(mb);
    return free_membuffer_but_return_trimmed_content(mb);
}

static int xslt_d_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/** Print the lines collected, sorted. */
static void xslt_d_sorted(MemBuffer *mb, char **lines, size_t n)
{
    qsort(lines, n, sizeof(char*), xslt_d_cmp);
    for (size_t i = 0; i < n; ++i)
    {
        membuffer_append(mb, lines[i]);
        free(lines[i]);
    }
    free(lines);
}

static char *xslt_d_take(MemBuffer *mb)
{
    membuffer_append_null(mb);
    return free_membuffer_but_return_trimmed_content(mb);
}

static void xslt_d_precomp(MemBuffer *mb, const XsltStylePreComp *c)
{
    if ((const void*)c == XSLT_EXT_MARKER)
    {
        membuffer_append(mb, " precomp=ext-marker");
        return;
    }
    membuffer_printf(mb, " precomp=%s", xslt_style_type_name(c->type));
    if (c->type == XSLT_FUNC_EXTENSION || c->type == XSLT_FUNC_DOCUMENT)
    {
        if (c->type == XSLT_FUNC_DOCUMENT)
        {
            membuffer_printf(mb, " ver11=%d", c->ver11);
            xslt_d_str(mb, "filename", c->filename);
        }
        return;
    }
    if (c->stype) xslt_d_str(mb, "stype", c->stype);
    if (c->has_stype) membuffer_printf(mb, " has_stype=%d", c->has_stype);
    if (c->number) membuffer_printf(mb, " number=%d", c->number);
    if (c->order) xslt_d_str(mb, "order", c->order);
    if (c->has_order) membuffer_printf(mb, " has_order=%d", c->has_order);
    if (c->descending) membuffer_printf(mb, " descending=%d", c->descending);
    if (c->lang) xslt_d_str(mb, "lang", c->lang);
    if (c->has_lang) membuffer_printf(mb, " has_lang=%d", c->has_lang);
    if (c->case_order) xslt_d_str(mb, "case_order", c->case_order);
    if (c->lower_first) membuffer_printf(mb, " lower_first=%d", c->lower_first);
    if (c->use) xslt_d_str(mb, "use", c->use);
    if (c->has_use) membuffer_printf(mb, " has_use=%d", c->has_use);
    if (c->noescape) membuffer_printf(mb, " noescape=%d", c->noescape);
    if (c->name) xslt_d_str(mb, "name", c->name);
    if (c->has_name) membuffer_printf(mb, " has_name=%d", c->has_name);
    if (c->ns) xslt_d_str(mb, "ns", c->ns);
    if (c->has_ns) membuffer_printf(mb, " has_ns=%d", c->has_ns);
    if (c->mode) xslt_d_str(mb, "mode", c->mode);
    if (c->mode_uri) xslt_d_str(mb, "mode_uri", c->mode_uri);
    if (c->test) xslt_d_str(mb, "test", c->test);
    if (c->select) xslt_d_str(mb, "select", c->select);
    if (c->comp) membuffer_append(mb, " comp");
    if (c->ns_nr) membuffer_printf(mb, " ns_nr=%d", (int)c->ns_nr);
    if (c->type == XSLT_FUNC_NUMBER)
    {
        const XsltNumberData *n = &c->numdata;
        xslt_d_str(mb, "level", n->level);
        xslt_d_str(mb, "count", n->count);
        xslt_d_str(mb, "from", n->from);
        xslt_d_str(mb, "value", n->value);
        xslt_d_str(mb, "format", n->format);
        membuffer_printf(mb, " has_format=%d digits_per_group=%d grouping_character=%d count_pat=%d from_pat=%d",
                         n->has_format, n->digits_per_group, n->grouping_character, n->count_pat != NULL, n->from_pat != NULL);
    }
}

static void xslt_d_avt(MemBuffer *mb, const XsltAttrVT *avt)
{
    membuffer_printf(mb, " avt=[strstart=%d", avt->strstart);
    int str = avt->strstart;
    for (int i = 0; i < avt->nb_seg; ++i)
    {
        if (str && avt->segments[i]) xslt_d_str(mb, "s", (const char*)avt->segments[i]);
        else if (str) membuffer_append(mb, " s=(null)");
        else membuffer_append(mb, avt->segments[i] ? " x" : " x=(null)");
        str = !str;
    }
    membuffer_append(mb, "]");
}

static void xslt_d_tree(MemBuffer *mb, const XMQNode *node, int depth)
{
    for (; node; node = xml_node_next(node))
    {
        if (xml_node_kind(node) == XML_DOM_DTD) continue;
        xslt_d_indent(mb, depth);
        switch (xml_node_kind(node))
        {
        case XML_DOM_ELEMENT:
            membuffer_append(mb, "element");
            xslt_d_str(mb, "name", xml_node_name(node));
            if (xml_node_ns(node)) xslt_d_str(mb, "ns", xml_node_ns_uri(node));
            if (xml_node_user_data(node)) xslt_d_precomp(mb, (const XsltStylePreComp*)xml_node_user_data(node));
            for (const XmlNs *ns = xml_node_first_ns_decl(node); ns; ns = xml_ns_next_decl(ns))
            {
                membuffer_append(mb, " xmlns");
                xslt_d_str(mb, "p", xml_ns_prefix(ns));
                xslt_d_str(mb, "h", xml_ns_href(ns));
            }
            membuffer_append_char(mb, '\n');
            for (const XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
            {
                xslt_d_indent(mb, depth + 1);
                membuffer_append(mb, "attr");
                xslt_d_str(mb, "name", xml_attr_name(a));
                XMQNode *an = xml_attr_node((XMQAttr*)a);
                if (xml_node_ns(an)) xslt_d_str(mb, "ns", xml_node_ns_uri(an));
                if (xml_node_user_data(an)) xslt_d_avt(mb, (const XsltAttrVT*)xml_node_user_data(an));
                membuffer_append_char(mb, '\n');
            }
            xslt_d_tree(mb, xml_node_first_child(node), depth + 1);
            continue;
        case XML_DOM_TEXT:
            membuffer_append(mb, "text");
            if (xml_node_name(node) && !strcmp(xml_node_name(node), "textnoenc")) membuffer_append(mb, " noenc");
            xslt_d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_CDATA:
            membuffer_append(mb, "cdata");
            xslt_d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_COMMENT:
            membuffer_append(mb, "comment");
            break;
        case XML_DOM_PI:
            membuffer_append(mb, "pi");
            break;
        default:
            membuffer_printf(mb, "node kind=%d", (int)xml_node_kind(node));
            break;
        }
        membuffer_append_char(mb, '\n');
    }
}

static void xslt_d_patterns(MemBuffer *mb, const char *label, const XmlLibxslt1Pattern *p, int depth)
{
    if (!p) return;
    xslt_d_indent(mb, depth);
    membuffer_append(mb, label);
    for (; p; p = p->next)
    {
        membuffer_append(mb, " [");
        xslt_d_str(mb, "p", p->pattern);
        membuffer_printf(mb, " prio=%g", (double)p->priority);
        if (p->mode) xslt_d_str(mb, "mode", p->mode);
        if (p->mode_uri) xslt_d_str(mb, "mode_uri", p->mode_uri);
        if (p->templ)
        {
            int ix = 0;
            const XsltTemplate *t = p->templ->style ? p->templ->style->templates : NULL;
            while (t && t != p->templ)
            {
                t = t->next;
                ix++;
            }
            membuffer_printf(mb, " tmpl=%d", t ? ix : -1);
        }
        membuffer_append(mb, " ]");
    }
    membuffer_append_char(mb, '\n');
}

static void xslt_d_style(MemBuffer *mb, const XmlLibxslt1Stylesheet *style, int depth)
{
    xslt_d_indent(mb, depth);
    membuffer_printf(mb, "stylesheet errors=%d warnings=%d literal_result=%d forwards_compatible=%d extras_nr=%d strip_all=%d\n",
                     style->errors, style->warnings, style->literal_result, style->forwards_compatible, style->extras_nr, style->strip_all);
    xslt_d_indent(mb, depth);
    membuffer_append(mb, "output");
    xslt_d_str(mb, "method", style->method);
    xslt_d_str(mb, "method_uri", style->method_uri);
    xslt_d_str(mb, "version", style->version);
    xslt_d_str(mb, "encoding", style->encoding);
    membuffer_printf(mb, " omit=%d standalone=%d indent=%d", style->omit_xml_declaration, style->standalone, style->indent);
    xslt_d_str(mb, "doctype_public", style->doctype_public);
    xslt_d_str(mb, "doctype_system", style->doctype_system);
    xslt_d_str(mb, "media_type", style->media_type);
    xslt_d_str(mb, "default_alias", style->default_alias);
    membuffer_append_char(mb, '\n');

    HashMap *maps[] = { style->strip_spaces, style->cdata_section, style->ns_aliases };
    const char *labels[] = { "space", "cdata", "alias" };
    for (int m = 0; m < 3; ++m)
    {
        if (!maps[m]) continue;
        size_t n = 0, i = 0;
        HashMapIterator *it = hashmap_iterate(maps[m]);
        const char *key;
        void *val;
        while (hashmap_next_key_value(it, &key, &val)) n++;
        hashmap_free_iterator(it);
        char **lines = (char**)calloc(n + 1, sizeof(char*));
        it = hashmap_iterate(maps[m]);
        while (hashmap_next_key_value(it, &key, &val))
        {
            MemBuffer *l = new_membuffer();
            xslt_d_indent(l, depth);
            membuffer_append(l, labels[m]);
            char *k = xslt_d_key(key);
            xslt_d_str(l, "key", k);
            free(k);
            xslt_d_str(l, "value", (const char*)val);
            membuffer_append_char(l, '\n');
            lines[i++] = xslt_d_take(l);
        }
        hashmap_free_iterator(it);
        xslt_d_sorted(mb, lines, i);
    }
    for (int i = 0; i < style->excl_prefix_nr; ++i)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "exclude");
        xslt_d_str(mb, "uri", style->excl_prefix_tab[i]);
        membuffer_append_char(mb, '\n');
    }
    for (const XsltExtDef *d = style->ns_defs; d; d = d->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "extension");
        xslt_d_str(mb, "prefix", d->prefix);
        xslt_d_str(mb, "uri", d->uri);
        membuffer_append_char(mb, '\n');
    }
    for (const XsltDecimalFormat *f = style->decimal_format; f; f = f->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "decimal-format");
        xslt_d_str(mb, "name", f->name);
        xslt_d_str(mb, "ns", f->ns_uri);
        xslt_d_str(mb, "digit", f->digit);
        xslt_d_str(mb, "pattern_separator", f->pattern_separator);
        xslt_d_str(mb, "minus_sign", f->minus_sign);
        xslt_d_str(mb, "infinity", f->infinity);
        xslt_d_str(mb, "nan", f->no_number);
        xslt_d_str(mb, "decimal_point", f->decimal_point);
        xslt_d_str(mb, "grouping", f->grouping);
        xslt_d_str(mb, "percent", f->percent);
        xslt_d_str(mb, "permille", f->permille);
        xslt_d_str(mb, "zero_digit", f->zero_digit);
        membuffer_append_char(mb, '\n');
    }
    for (const XsltKeyDef *k = style->keys; k; k = k->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "key");
        xslt_d_str(mb, "name", k->name);
        xslt_d_str(mb, "name_uri", k->name_uri);
        xslt_d_str(mb, "match", k->match);
        xslt_d_str(mb, "use", k->use);
        membuffer_printf(mb, " comp=%d usecomp=%d\n", k->comp != NULL, k->usecomp != NULL);
    }
    for (const XsltStackElem *v = style->variables; v; v = v->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_printf(mb, "global %s", xslt_style_type_name(v->comp ? v->comp->type : 0));
        xslt_d_str(mb, "name", v->name);
        xslt_d_str(mb, "name_uri", v->name_uri);
        xslt_d_str(mb, "select", v->select);
        membuffer_printf(mb, " tree=%d computed=%d\n", v->tree != NULL, v->computed);
    }
    if (style->attribute_sets)
    {
        size_t n = 0, i = 0;
        for (const XsltAttrSet *set = style->attribute_set_list; set; set = set->next_in_order) n++;
        char **lines = (char**)calloc(n + 1, sizeof(char*));
        for (const XsltAttrSet *set = style->attribute_set_list; set; set = set->next_in_order)
        {
            MemBuffer *l = new_membuffer();
            xslt_d_indent(l, depth);
            membuffer_append(l, "attribute-set");
            xslt_d_str(l, "name", set->name);
            xslt_d_str(l, "ns", set->ns);
            membuffer_printf(l, " state=%d", (int)set->state);
            for (const XsltAttrElem *a = set->attrs; a; a = a->next)
            {
                const XsltStylePreComp *c = (const XsltStylePreComp*)xml_node_user_data(a->attr);
                membuffer_append(l, " [");
                xslt_d_str(l, "name", c ? c->name : NULL);
                xslt_d_str(l, "ns", c ? c->ns : NULL);
                membuffer_printf(l, " line=%d ]", xml_node_line(a->attr));
            }
            for (const XsltUseAttrSet *u = set->use_attr_sets; u; u = u->next)
            {
                membuffer_append(l, " use");
                xslt_d_str(l, "ncname", u->ncname);
                xslt_d_str(l, "ns", u->ns);
            }
            membuffer_append_char(l, '\n');
            lines[i++] = xslt_d_take(l);
        }
        xslt_d_sorted(mb, lines, i);
    }
    for (const XsltTemplate *t = style->templates; t; t = t->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "template");
        xslt_d_str(mb, "match", t->match);
        xslt_d_str(mb, "name", t->name);
        xslt_d_str(mb, "name_uri", t->name_uri);
        xslt_d_str(mb, "mode", t->mode);
        xslt_d_str(mb, "mode_uri", t->mode_uri);
        membuffer_printf(mb, " prio=%g position=%d content=%d inherited=%d", (double)t->priority, t->position,
                         t->content != NULL, t->inherited_ns_nr);
        for (int i = 0; i < t->inherited_ns_nr; ++i) xslt_d_str(mb, "ns", xml_ns_prefix(t->inherited_ns[i]));
        membuffer_append_char(mb, '\n');
    }
    xslt_d_patterns(mb, "root_match", style->root_match, depth);
    xslt_d_patterns(mb, "key_match", style->key_match, depth);
    xslt_d_patterns(mb, "elem_match", style->elem_match, depth);
    xslt_d_patterns(mb, "attr_match", style->attr_match, depth);
    xslt_d_patterns(mb, "parent_match", style->parent_match, depth);
    xslt_d_patterns(mb, "text_match", style->text_match, depth);
    xslt_d_patterns(mb, "pi_match", style->pi_match, depth);
    xslt_d_patterns(mb, "comment_match", style->comment_match, depth);
    if (style->templates_hash)
    {
        size_t n = 0, i = 0;
        HashMapIterator *it = hashmap_iterate(style->templates_hash);
        const char *key;
        void *val;
        while (hashmap_next_key_value(it, &key, &val)) n++;
        hashmap_free_iterator(it);
        char **lines = (char**)calloc(n + 1, sizeof(char*));
        it = hashmap_iterate(style->templates_hash);
        while (hashmap_next_key_value(it, &key, &val))
        {
            if (!val) continue;
            MemBuffer *l = new_membuffer();
            char *k = xslt_d_key(key);
            MemBuffer *label = new_membuffer();
            membuffer_append(label, "hash");
            xslt_d_str(label, "key", k);
            char *lab = xslt_d_take(label);
            xslt_d_patterns(l, lab, (const XmlLibxslt1Pattern*)val, depth);
            free(lab);
            free(k);
            lines[i++] = xslt_d_take(l);
        }
        hashmap_free_iterator(it);
        xslt_d_sorted(mb, lines, i);
    }
    if (style->doc)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "document");
        XMQNode *doc_node = xml_dom_document(style->doc);
        if (xml_node_user_data(doc_node)) xslt_d_precomp(mb, (const XsltStylePreComp*)xml_node_user_data(doc_node));
        membuffer_append_char(mb, '\n');
        xslt_d_tree(mb, xml_node_first_child(doc_node), depth + 1);
    }
    for (const XsltDocument *d = style->doc_list; d; d = d->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_printf(mb, "include preproc=%d\n", d->preproc);
        xslt_d_tree(mb, xml_node_first_child(xml_dom_document(d->doc)), depth + 1);
    }
    for (const XmlLibxslt1Stylesheet *imp = style->imports; imp; imp = imp->next)
    {
        xslt_d_indent(mb, depth);
        membuffer_append(mb, "import\n");
        xslt_d_style(mb, imp, depth + 1);
    }
}

char *xml_libxslt1_stylesheet_describe(const XmlLibxslt1Stylesheet *style)
{
    MemBuffer *mb = new_membuffer();
    if (style) xslt_d_style(mb, style, 0);
    return xslt_d_take(mb);
}




#endif // XML_LIBXSLT1_XSLT_MODULE
