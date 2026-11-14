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

/* Portions of this file are ported from libxml2 (xpath.c and pattern.c, version 2.16.0 as found
   in the libxmq fork in 3rdparty/libxml2), partly through the java port in
   src/main/java/org/libxmq/imp/xml/XPath*.java.
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

#include"xml_xpath.h"
#include"xml_chars.h"
#include"hashmap.h"

#include<float.h>
#include<limits.h>
#include<math.h>
#include<setjmp.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_XPATH_MODULE

#define XP_XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"
#define XP_XQUERY_FUNCTIONS_NS "http://www.w3.org/2002/08/xquery-functions"
#define XP_INDEX_SORT_THRESHOLD 32

// Axes, same as libxml2 xmlXPathAxisVal.
enum
{
    XP_AXIS_ANCESTOR = 1, XP_AXIS_ANCESTOR_OR_SELF, XP_AXIS_ATTRIBUTE, XP_AXIS_CHILD, XP_AXIS_DESCENDANT,
    XP_AXIS_DESCENDANT_OR_SELF, XP_AXIS_FOLLOWING, XP_AXIS_FOLLOWING_SIBLING, XP_AXIS_NAMESPACE,
    XP_AXIS_PARENT, XP_AXIS_PRECEDING, XP_AXIS_PRECEDING_SIBLING, XP_AXIS_SELF
};

// Node tests, same as libxml2 xmlXPathTestVal.
enum { XP_TEST_TYPE = 1, XP_TEST_PI = 2, XP_TEST_ALL = 3, XP_TEST_NAME = 5 };
enum { XP_TYPE_NODE = 0, XP_TYPE_TEXT = 3, XP_TYPE_PI = 7, XP_TYPE_COMMENT = 8 };

// Path starts.
enum { XP_START_ROOT, XP_START_CONTEXT, XP_START_FILTER };

// Arithmetic operators.
enum { XP_OP_SUB, XP_OP_ADD, XP_OP_MUL, XP_OP_DIV, XP_OP_MOD };

// Expression kinds.
typedef enum
{
    XP_LITERAL, XP_NUMBER, XP_VAR, XP_OR, XP_AND, XP_EQUAL, XP_COMPARE, XP_ARITH, XP_NEGATE,
    XP_UNION, XP_SORT, XP_FILTER, XP_PATH, XP_CALL
} XpKind;

struct XpExpr;
typedef struct XpExpr XpExpr;

typedef struct
{
    int axis;
    int test;
    int type;
    const char *prefix;
    const char *name;
    XpExpr **preds;
    size_t num_preds;
} XpStep;

struct XpExpr
{
    XpKind kind;
    XpExpr *l;          // Binary operators, negate/sort/filter operand.
    XpExpr *r;          // Binary operators, filter predicate.
    int op;             // Arith op, equal: 1 for =, 0 for !=; compare: bit0 inf, bit1 strict; negate: minus.
    const char *str;    // Literal value.
    double num;         // Number literal.
    const char *prefix; // Variables and calls.
    const char *name;
    XpExpr **args;      // Call arguments.
    size_t num_args;
    int start;          // Path start.
    XpExpr *filter;     // Path filter (XP_START_FILTER).
    XpStep *steps;
    size_t num_steps;
};

// Values ////////////////////////////////////////////////////////

typedef struct
{
    XMQNode **nodes;
    size_t len;
    size_t cap;
} XpNodeSet;

typedef struct
{
    XmlXPathType type;
    bool b;
    double num;
    const char *str;
    XpNodeSet ns;
} XpValue;

// Arena: every allocation of an evaluation, freed together.
typedef struct XpBlock
{
    struct XpBlock *next;
    // Followed by the data.
} XpBlock;

typedef union { long double ld; void *p; long long ll; } XpAlign;

// Pointer -> integer map, open addressing.
typedef struct
{
    const void **keys;
    long *vals;
    size_t cap;
    size_t len;
} XpPtrMap;

typedef struct
{
    const XmlXPathSettings *settings;
    const char *expr;
    size_t pos;           // Compile position.
    size_t depth;         // Compile nesting.

    XMQNode *node;
    XMQNode *doc;
    long size;
    long position;

    XpBlock *blocks;
    jmp_buf on_error;
    XmlXPathErrorCode error;

    XpPtrMap order;       // Document order index.
    XMQNode *indexed_root;
    HashMap *ids;         // id value -> element.
    bool ids_built;
    XpPtrMap ns_cache;    // element -> index into ns_lists.
    XMQNode ***ns_lists;  // Namespace nodes per element, NULL terminated.
    size_t num_ns_lists;
    size_t ns_lists_cap;
    XMQNode **ns_nodes;   // All namespace nodes created (owned by the result).
    size_t num_ns_nodes;
    size_t ns_nodes_cap;
} XpCtx;

struct XmlXPathResult
{
    XmlXPathType type;
    bool b;
    double num;
    char *str;
    XMQNode **nodes;
    size_t len;
    XMQNode **ns_nodes;   // Owned namespace nodes.
    size_t num_ns_nodes;
};

static const char *xp_messages_[] = {
    "Ok", "Number encoding", "Unfinished literal", "Start of literal", "Expected $ for variable reference",
    "Undefined variable", "Invalid predicate", "Invalid expression", "Missing closing curly brace",
    "Unregistered function", "Invalid operand", "Invalid type", "Invalid number of arguments",
    "Invalid context size", "Invalid context position", "Memory allocation error", "Syntax error",
    "Resource error", "Sub resource error", "Undefined namespace prefix", "Encoding error",
    "Char out of XML range", "Invalid or incomplete context", "Stack usage error", "Forbidden variable",
    "Operation limit exceeded", "Recursion limit exceeded"
};

static void xp_error(XpCtx *c, XmlXPathErrorCode code)
{
    c->error = code;
    longjmp(c->on_error, 1);
}

static void *xp_alloc(XpCtx *c, size_t size)
{
    XpBlock *b = (XpBlock*)malloc(sizeof(XpAlign) + size);
    if (!b) xp_error(c, XML_XPATH_ERR_MEMORY);
    b->next = c->blocks;
    c->blocks = b;
    return (char*)b + sizeof(XpAlign);
}

static void *xp_calloc(XpCtx *c, size_t size)
{
    void *p = xp_alloc(c, size);
    memset(p, 0, size);
    return p;
}

static const char *xp_strndup(XpCtx *c, const char *s, size_t n)
{
    char *r = (char*)xp_alloc(c, n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static const char *xp_strdup(XpCtx *c, const char *s)
{
    return xp_strndup(c, s, strlen(s));
}

// String builder in the arena.
typedef struct
{
    char *b;
    size_t len;
    size_t cap;
} XpStr;

static void xp_str_add(XpCtx *c, XpStr *s, const char *t, size_t n)
{
    if (s->len + n + 1 > s->cap)
    {
        size_t nc = s->cap ? s->cap * 2 : 64;
        while (nc < s->len + n + 1) nc *= 2;
        char *nb = (char*)xp_alloc(c, nc);
        if (s->len) memcpy(nb, s->b, s->len);
        s->b = nb;
        s->cap = nc;
    }
    memcpy(s->b + s->len, t, n);
    s->len += n;
    s->b[s->len] = 0;
}

static const char *xp_str_get(XpStr *s)
{
    return s->b ? s->b : "";
}

// Pointer maps ////////////////////////////////////////////////////

static size_t xp_ptr_hash(const void *p)
{
    uintptr_t x = (uintptr_t)p;
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    return (size_t)x;
}

static long *xp_map_find(XpPtrMap *m, const void *key)
{
    if (!m->cap) return NULL;
    size_t i = xp_ptr_hash(key) & (m->cap - 1);
    while (m->keys[i])
    {
        if (m->keys[i] == key) return &m->vals[i];
        i = (i + 1) & (m->cap - 1);
    }
    return NULL;
}

static void xp_map_put(XpCtx *c, XpPtrMap *m, const void *key, long val);

static void xp_map_grow(XpCtx *c, XpPtrMap *m)
{
    XpPtrMap n = { NULL, NULL, m->cap ? m->cap * 2 : 64, 0 };
    n.keys = (const void**)xp_calloc(c, n.cap * sizeof(void*));
    n.vals = (long*)xp_alloc(c, n.cap * sizeof(long));
    for (size_t i = 0; i < m->cap; ++i) if (m->keys[i]) xp_map_put(c, &n, m->keys[i], m->vals[i]);
    *m = n;
}

static void xp_map_put(XpCtx *c, XpPtrMap *m, const void *key, long val)
{
    if ((m->len + 1) * 2 > m->cap) xp_map_grow(c, m);
    size_t i = xp_ptr_hash(key) & (m->cap - 1);
    while (m->keys[i])
    {
        if (m->keys[i] == key)
        {
            m->vals[i] = val;
            return;
        }
        i = (i + 1) & (m->cap - 1);
    }
    m->keys[i] = key;
    m->vals[i] = val;
    m->len++;
}

// Node sets ///////////////////////////////////////////////////////

static void xp_ns_add(XpCtx *c, XpNodeSet *s, XMQNode *n)
{
    if (s->len == s->cap)
    {
        size_t nc = s->cap ? s->cap * 2 : 8;
        if (nc > 10000000) xp_error(c, XML_XPATH_ERR_MEMORY); // XPATH_MAX_NODESET_LENGTH
        XMQNode **nn = (XMQNode**)xp_alloc(c, nc * sizeof(XMQNode*));
        if (s->len) memcpy(nn, s->nodes, s->len * sizeof(XMQNode*));
        s->nodes = nn;
        s->cap = nc;
    }
    s->nodes[s->len++] = n;
}

static XpValue xp_nodeset(void)
{
    XpValue v;
    memset(&v, 0, sizeof(v));
    v.type = XML_XPATH_NODESET;
    return v;
}

static XpValue xp_boolean(bool b)
{
    XpValue v;
    memset(&v, 0, sizeof(v));
    v.type = XML_XPATH_BOOLEAN;
    v.b = b;
    return v;
}

static XpValue xp_number(double d)
{
    XpValue v;
    memset(&v, 0, sizeof(v));
    v.type = XML_XPATH_NUMBER;
    v.num = d;
    return v;
}

static XpValue xp_string(const char *s)
{
    XpValue v;
    memset(&v, 0, sizeof(v));
    v.type = XML_XPATH_STRING;
    v.str = s;
    return v;
}

// Utilities ///////////////////////////////////////////////////////

static bool xp_is_blank(int c)
{
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
}

static bool xp_is_ascii_digit(int c)
{
    return c >= '0' && c <= '9';
}

static bool xp_is_ascii_letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool xp_streq(const char *a, const char *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return !strcmp(a, b);
}

/** Decode one utf8 code point at s, returns 0 at the end. */
static int xp_utf8(const char *s, size_t *len)
{
    const unsigned char *u = (const unsigned char*)s;
    if (u[0] < 0x80) { *len = u[0] ? 1 : 0; return u[0]; }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) { *len = 2; return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F); }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80)
    {
        *len = 3;
        return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80)
    {
        *len = 4;
        return ((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12) | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F);
    }
    *len = 1;
    return u[0];
}

static size_t xp_utf8_count(const char *s)
{
    size_t n = 0;
    for (const unsigned char *u = (const unsigned char*)s; *u; ++u) if ((*u & 0xC0) != 0x80) n++;
    return n;
}

/** Byte offset of code point index cp in s (or the end). */
static size_t xp_utf8_offset(const char *s, size_t cp)
{
    const unsigned char *u = (const unsigned char*)s;
    size_t i = 0;
    while (u[i] && cp > 0)
    {
        i++;
        while (u[i] && (u[i] & 0xC0) == 0x80) i++;
        cp--;
    }
    return i;
}

#define XP_MAX_FRAC 20

/** Port of libxml2 xmlXPathStringEvalNumber. */
double xml_xpath_string_to_number(const char *str)
{
    const unsigned char *cur = (const unsigned char*)str;
    double ret;
    bool ok = false;
    bool isneg = false;
    int exponent = 0;
    bool is_exponent_negative = false;
    if (cur == NULL) return 0;
    while (xp_is_blank(*cur)) cur++;
    if (*cur == '-')
    {
        isneg = true;
        cur++;
    }
    if (*cur != '.' && (*cur < '0' || *cur > '9')) return NAN;
    ret = 0;
    while (*cur >= '0' && *cur <= '9')
    {
        ret = ret * 10;
        ret = ret + (double)(*cur - '0');
        ok = true;
        cur++;
    }
    if (*cur == '.')
    {
        int frac = 0, max;
        double fraction = 0;
        cur++;
        if ((*cur < '0' || *cur > '9') && !ok) return NAN;
        while (*cur == '0')
        {
            frac++;
            cur++;
        }
        max = frac + XP_MAX_FRAC;
        while (*cur >= '0' && *cur <= '9' && frac < max)
        {
            fraction = fraction * 10 + (*cur - '0');
            frac++;
            cur++;
        }
        fraction /= pow(10.0, frac);
        ret = ret + fraction;
        while (*cur >= '0' && *cur <= '9') cur++;
    }
    if (*cur == 'e' || *cur == 'E')
    {
        cur++;
        if (*cur == '-')
        {
            is_exponent_negative = true;
            cur++;
        }
        else if (*cur == '+')
        {
            cur++;
        }
        while (*cur >= '0' && *cur <= '9')
        {
            if (exponent < 1000000) exponent = exponent * 10 + (*cur - '0');
            cur++;
        }
    }
    while (xp_is_blank(*cur)) cur++;
    if (*cur != 0) return NAN;
    if (isneg) ret = -ret;
    if (is_exponent_negative) exponent = -exponent;
    ret *= pow(10.0, (double)exponent);
    return ret;
}

#define XP_UPPER_DOUBLE 1E9
#define XP_LOWER_DOUBLE 1E-5
#define XP_LOWER_DOUBLE_EXP 5
#define XP_EXPONENT_DIGITS (3 + 2)

/** Port of libxml2 xmlXPathFormatNumber. */
static void xp_format_number(double number, char *buffer, int buffersize)
{
    if (isinf(number))
    {
        snprintf(buffer, buffersize, number > 0 ? "Infinity" : "-Infinity");
        return;
    }
    if (isnan(number))
    {
        snprintf(buffer, buffersize, "NaN");
        return;
    }
    if (number == 0)
    {
        // Omit sign for negative zero.
        snprintf(buffer, buffersize, "0");
        return;
    }
    if (number > INT_MIN && number < INT_MAX && number == (int)number)
    {
        snprintf(buffer, buffersize, "%d", (int)number);
        return;
    }
    char work[DBL_DIG + XP_EXPONENT_DIGITS + 3 + XP_LOWER_DOUBLE_EXP];
    int integer_place, fraction_place;
    char *ptr;
    char *after_fraction;
    double absolute_value = fabs(number);
    int size;

    if ((absolute_value > XP_UPPER_DOUBLE || absolute_value < XP_LOWER_DOUBLE) && absolute_value != 0.0)
    {
        // Use scientific notation.
        integer_place = DBL_DIG + XP_EXPONENT_DIGITS + 1;
        fraction_place = DBL_DIG - 1;
        size = snprintf(work, sizeof(work), "%*.*e", integer_place, fraction_place, number);
        while (size > 0 && work[size] != 'e') size--;
    }
    else
    {
        // Use regular notation.
        if (absolute_value > 0.0)
        {
            integer_place = (int)log10(absolute_value);
            if (integer_place > 0) fraction_place = DBL_DIG - integer_place - 1;
            else fraction_place = DBL_DIG - integer_place;
        }
        else
        {
            fraction_place = 1;
        }
        size = snprintf(work, sizeof(work), "%0.*f", fraction_place, number);
    }
    // Remove leading spaces sometimes inserted by snprintf.
    while (work[0] == ' ')
    {
        for (ptr = &work[0]; (ptr[0] = ptr[1]); ptr++);
        size--;
    }
    // Remove fractional trailing zeroes.
    after_fraction = work + size;
    ptr = after_fraction;
    while (*(--ptr) == '0');
    if (*ptr != '.') ptr++;
    while ((*ptr++ = *after_fraction++) != 0);
    size = (int)strlen(work) + 1;
    if (size > buffersize)
    {
        work[buffersize - 1] = 0;
        size = buffersize;
    }
    memmove(buffer, work, size);
}

char *xml_xpath_format_number(double d)
{
    char buf[100];
    xp_format_number(d, buf, sizeof(buf));
    size_t n = strlen(buf);
    char *r = (char*)malloc(n + 1);
    memcpy(r, buf, n + 1);
    return r;
}

static bool xp_number_to_boolean(double d)
{
    return !isnan(d) && d != 0.0;
}

static const char *xp_string_value(XpCtx *c, XMQNode *n)
{
    char *s = xml_node_string_value(n);
    const char *r = xp_strdup(c, s);
    free(s);
    return r;
}

static XMQNode *xp_document_of(XMQNode *n)
{
    return n ? xml_dom_document(xml_node_dom(n)) : NULL;
}

// Document order //////////////////////////////////////////////////

static int xp_kind_rank(XMQNode *n)
{
    XmlNodeKind k = xml_node_kind(n);
    if (k == XML_DOM_NAMESPACE) return 0;
    if (k == XML_DOM_ATTRIBUTE) return 1;
    return 2;
}

static size_t xp_depth(XMQNode *n)
{
    size_t d = 0;
    for (XMQNode *i = xml_node_parent(n); i; i = xml_node_parent(i)) d++;
    return d;
}

/** Port of the java XPathContext.compareTurtle: compare by walking ancestors. */
static int xp_compare_turtle(XMQNode *a, XMQNode *b)
{
    if (a == b) return 0;
    size_t da = xp_depth(a), db = xp_depth(b);
    XMQNode *ia = a, *ib = b;
    while (da > db) { ia = xml_node_parent(ia); da--; }
    while (db > da) { ib = xml_node_parent(ib); db--; }
    if (ia == ib) return (a == ia) ? -1 : 1; // An ancestor comes first.
    while (xml_node_parent(ia) != xml_node_parent(ib))
    {
        ia = xml_node_parent(ia);
        ib = xml_node_parent(ib);
    }
    if (!xml_node_parent(ia)) return ia < ib ? -1 : 1; // Different trees.
    int ka = xp_kind_rank(ia), kb = xp_kind_rank(ib);
    if (ka != kb) return ka < kb ? -1 : 1;
    if (ka == 0) return xml_node_line(ia) < xml_node_line(ib) ? -1 : 1;
    for (XMQNode *s = xml_node_next(ia); s; s = xml_node_next(s)) if (s == ib) return -1;
    return 1;
}

static void xp_index_tree(XpCtx *c, XMQNode *root)
{
    long next = 0;
    XMQNode *n = root;
    while (n)
    {
        xp_map_put(c, &c->order, n, next);
        next += 2;
        if (xml_node_kind(n) == XML_DOM_ELEMENT)
        {
            for (XMQAttr *a = xml_node_first_attr(n); a; a = xml_attr_next(a))
            {
                xp_map_put(c, &c->order, xml_attr_node(a), next);
                next += 2;
            }
        }
        if (xml_node_first_child(n))
        {
            n = xml_node_first_child(n);
            continue;
        }
        while (n && n != root && !xml_node_next(n)) n = xml_node_parent(n);
        if (!n || n == root) break;
        n = xml_node_next(n);
    }
}

static long xp_ordinal(XpCtx *c, XMQNode *n)
{
    long *o = xp_map_find(&c->order, n);
    if (o) return *o;
    XMQNode *root = n;
    while (xml_node_parent(root)) root = xml_node_parent(root);
    if (root != c->indexed_root)
    {
        c->indexed_root = root;
        c->order.len = 0;
        if (c->order.cap) memset(c->order.keys, 0, c->order.cap * sizeof(void*));
        xp_index_tree(c, root);
        o = xp_map_find(&c->order, n);
        if (o) return *o;
    }
    return -1;
}

static int xp_compare_index(XpCtx *c, XMQNode *a, XMQNode *b)
{
    if (a == b) return 0;
    bool na = xml_node_kind(a) == XML_DOM_NAMESPACE;
    bool nb = xml_node_kind(b) == XML_DOM_NAMESPACE;
    long oa = xp_ordinal(c, na ? xml_node_parent(a) : a);
    long ob = xp_ordinal(c, nb ? xml_node_parent(b) : b);
    if (oa != ob) return oa < ob ? -1 : 1;
    if (na && nb) return xml_node_line(a) < xml_node_line(b) ? -1 : 1;
    return na ? 1 : -1;
}

typedef struct
{
    XpCtx *ctx;
    bool use_index;
    bool with_ns;  // The libxml2 comparison with namespace nodes: always "before".
} XpCmp;

static int xp_cmp(XpCmp *cmp, XMQNode *x, XMQNode *y)
{
    if (x == y) return 0;
    if (cmp->with_ns && (xml_node_kind(x) == XML_DOM_NAMESPACE || xml_node_kind(y) == XML_DOM_NAMESPACE)) return -1;
    int r = cmp->use_index ? xp_compare_index(cmp->ctx, x, y) : xp_compare_turtle(x, y);
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

static void xp_merge_sort(XpCmp *cmp, XMQNode **a, XMQNode **tmp, size_t n)
{
    if (n < 2) return;
    size_t m = n / 2;
    xp_merge_sort(cmp, a, tmp, m);
    xp_merge_sort(cmp, a + m, tmp, n - m);
    size_t i = 0, j = m, k = 0;
    while (i < m && j < n) tmp[k++] = (xp_cmp(cmp, a[j], a[i]) < 0) ? a[j++] : a[i++];
    while (i < m) tmp[k++] = a[i++];
    while (j < n) tmp[k++] = a[j++];
    memcpy(a, tmp, n * sizeof(XMQNode*));
}

/** Like the java sortWithNamespaces: libxml2's insertion sort when namespace nodes are present. */
static void xp_sort_with_ns(XpCmp *cmp, XMQNode **dst, size_t size)
{
    for (size_t i = 1; i < size; ++i)
    {
        if (xp_cmp(cmp, dst[i-1], dst[i]) <= 0) continue;
        XMQNode *x = dst[i];
        // Binary insertion find.
        size_t l = 0, r = i - 1, ci = r >> 1, location;
        if (xp_cmp(cmp, x, dst[0]) < 0) location = 0;
        else if (xp_cmp(cmp, x, dst[r]) > 0) location = r;
        else
        {
            XMQNode *cx = dst[ci];
            while (true)
            {
                int val = xp_cmp(cmp, x, cx);
                if (val < 0)
                {
                    if (ci - l <= 1) { location = ci; break; }
                    r = ci;
                }
                else
                {
                    if (r - ci <= 1) { location = ci + 1; break; }
                    l = ci;
                }
                ci = l + ((r - l) >> 1);
                cx = dst[ci];
            }
        }
        for (size_t j = i; j > location; --j) dst[j] = dst[j-1];
        dst[location] = x;
    }
}

static void xp_sort(XpCtx *c, XpNodeSet *s)
{
    if (s->len <= 1) return;
    XpCmp cmp = { c, s->len > XP_INDEX_SORT_THRESHOLD, false };
    for (size_t i = 0; i < s->len; ++i)
    {
        if (xml_node_kind(s->nodes[i]) == XML_DOM_NAMESPACE)
        {
            cmp.with_ns = true;
            xp_sort_with_ns(&cmp, s->nodes, s->len);
            return;
        }
    }
    XMQNode **tmp = (XMQNode**)xp_alloc(c, s->len * sizeof(XMQNode*));
    xp_merge_sort(&cmp, s->nodes, tmp, s->len);
}

static XMQNode *xp_first(XpCtx *c, XpNodeSet *s)
{
    XpCmp cmp = { c, s->len > XP_INDEX_SORT_THRESHOLD, false };
    XMQNode *best = NULL;
    for (size_t i = 0; i < s->len; ++i)
    {
        if (!best || xp_cmp(&cmp, s->nodes[i], best) < 0) best = s->nodes[i];
    }
    return best;
}

// Conversions /////////////////////////////////////////////////////

static bool xp_to_boolean(XpValue v)
{
    switch (v.type)
    {
    case XML_XPATH_BOOLEAN: return v.b;
    case XML_XPATH_NUMBER: return xp_number_to_boolean(v.num);
    case XML_XPATH_STRING: return v.str[0] != 0;
    case XML_XPATH_NODESET: return v.ns.len > 0;
    }
    return false;
}

static const char *xp_to_string(XpCtx *c, XpValue v)
{
    switch (v.type)
    {
    case XML_XPATH_STRING: return v.str;
    case XML_XPATH_NUMBER:
    {
        char buf[100];
        xp_format_number(v.num, buf, sizeof(buf));
        return xp_strdup(c, buf);
    }
    case XML_XPATH_BOOLEAN: return v.b ? "true" : "false";
    case XML_XPATH_NODESET:
        if (v.ns.len == 0) return "";
        return xp_string_value(c, v.ns.len == 1 ? v.ns.nodes[0] : xp_first(c, &v.ns));
    }
    return "";
}

static double xp_to_number(XpCtx *c, XpValue v)
{
    switch (v.type)
    {
    case XML_XPATH_NUMBER: return v.num;
    case XML_XPATH_BOOLEAN: return v.b ? 1.0 : 0.0;
    case XML_XPATH_STRING: return xml_xpath_string_to_number(v.str);
    case XML_XPATH_NODESET: return xml_xpath_string_to_number(xp_to_string(c, v));
    }
    return NAN;
}

// Namespaces //////////////////////////////////////////////////////

static const char *xp_lookup_ns(XpCtx *c, const char *prefix)
{
    if (!strcmp(prefix, "xml")) return XP_XML_NAMESPACE;
    for (size_t i = 0; i < c->settings->num_namespaces; ++i)
    {
        if (xp_streq(c->settings->namespaces[i].prefix, prefix)) return c->settings->namespaces[i].uri;
    }
    return NULL;
}

static XMQNode *xp_new_ns_node(XpCtx *c, XMQNode *e, const char *prefix, const char *href, int index)
{
    XMQNode *n = xml_node_new_namespace_node(e, prefix, href, index);
    if (c->num_ns_nodes == c->ns_nodes_cap)
    {
        c->ns_nodes_cap = c->ns_nodes_cap ? c->ns_nodes_cap * 2 : 16;
        c->ns_nodes = (XMQNode**)realloc(c->ns_nodes, c->ns_nodes_cap * sizeof(XMQNode*));
    }
    c->ns_nodes[c->num_ns_nodes++] = n;
    return n;
}

/** The namespace nodes of an element (NULL terminated), created once per evaluation. */
static XMQNode **xp_namespace_nodes(XpCtx *c, XMQNode *e)
{
    long *idx = xp_map_find(&c->ns_cache, e);
    if (idx) return c->ns_lists[*idx];

    // Collect the in scope declarations, nearest first, a prefix only once.
    size_t num = 0, cap = 8;
    XmlNs **list = (XmlNs**)xp_alloc(c, cap * sizeof(XmlNs*));
    for (XMQNode *n = e; n; n = xml_node_parent(n))
    {
        if (xml_node_kind(n) != XML_DOM_ELEMENT) continue;
        for (XmlNs *ns = xml_node_first_ns_decl(n); ns; ns = xml_ns_next_decl(ns))
        {
            bool dup = false;
            for (size_t k = 0; k < num; ++k) if (xp_streq(xml_ns_prefix(list[k]), xml_ns_prefix(ns))) dup = true;
            if (dup) continue;
            if (num == cap)
            {
                XmlNs **nl = (XmlNs**)xp_alloc(c, cap * 2 * sizeof(XmlNs*));
                memcpy(nl, list, num * sizeof(XmlNs*));
                list = nl;
                cap *= 2;
            }
            list[num++] = ns;
        }
    }
    XMQNode **out = (XMQNode**)xp_alloc(c, (num + 2) * sizeof(XMQNode*));
    int index = 0;
    size_t o = 0;
    out[o++] = xp_new_ns_node(c, e, "xml", XP_XML_NAMESPACE, index++);
    for (size_t k = num; k > 0; --k)
    {
        out[o++] = xp_new_ns_node(c, e, xml_ns_prefix(list[k-1]), xml_ns_href(list[k-1]), index++);
    }
    out[o] = NULL;

    if (c->num_ns_lists == c->ns_lists_cap)
    {
        size_t nc = c->ns_lists_cap ? c->ns_lists_cap * 2 : 16;
        XMQNode ***nl = (XMQNode***)xp_alloc(c, nc * sizeof(XMQNode**));
        if (c->num_ns_lists) memcpy(nl, c->ns_lists, c->num_ns_lists * sizeof(XMQNode**));
        c->ns_lists = nl;
        c->ns_lists_cap = nc;
    }
    c->ns_lists[c->num_ns_lists] = out;
    xp_map_put(c, &c->ns_cache, e, (long)c->num_ns_lists);
    c->num_ns_lists++;
    return out;
}

// Parser //////////////////////////////////////////////////////////

static int xp_cur(XpCtx *c)
{
    return (unsigned char)c->expr[c->pos];
}

static int xp_nxt(XpCtx *c, size_t n)
{
    for (size_t i = 0; i < n; ++i) if (!c->expr[c->pos + i]) return 0;
    return (unsigned char)c->expr[c->pos + n];
}

static void xp_next(XpCtx *c)
{
    if (c->expr[c->pos]) c->pos++;
}

static void xp_skip(XpCtx *c, size_t n)
{
    while (n-- && c->expr[c->pos]) c->pos++;
}

static void xp_skip_blanks(XpCtx *c)
{
    while (xp_is_blank(xp_cur(c))) c->pos++;
}

static int xp_cur_char(XpCtx *c, size_t *len)
{
    return xp_utf8(c->expr + c->pos, len);
}

static XpExpr *xp_new(XpCtx *c, XpKind k)
{
    XpExpr *e = (XpExpr*)xp_calloc(c, sizeof(XpExpr));
    e->kind = k;
    return e;
}

static XpExpr *xp_bin(XpCtx *c, XpKind k, XpExpr *l, XpExpr *r, int op)
{
    XpExpr *e = xp_new(c, k);
    e->l = l;
    e->r = r;
    e->op = op;
    return e;
}

static bool xp_is_value(XpExpr *e)
{
    return e->kind == XP_LITERAL || e->kind == XP_NUMBER;
}

static void xp_enter(XpCtx *c)
{
    if (++c->depth > c->settings->max_depth) xp_error(c, XML_XPATH_ERR_RECURSION_LIMIT_EXCEEDED);
}

static void xp_leave(XpCtx *c)
{
    c->depth--;
}

static XpExpr *xp_compile_expr(XpCtx *c, bool sort);
static void xp_compile_relative_location_path(XpCtx *c, XpStep **steps, size_t *num);

static bool xp_is_name_char(int ch)
{
    return xml_char_is_letter(ch) || xml_char_is_digit(ch) || ch == '.' || ch == '-' || ch == '_' ||
           xml_char_is_combining(ch) || xml_char_is_extender(ch);
}

/** Parse an NCName, returns NULL if none. */
static const char *xp_parse_ncname(XpCtx *c)
{
    size_t l;
    int ch = xp_cur_char(c, &l);
    if (ch == 0 || (!xml_char_is_letter(ch) && ch != '_')) return NULL;
    size_t start = c->pos;
    while (ch != 0 && xp_is_name_char(ch))
    {
        c->pos += l;
        ch = xp_cur_char(c, &l);
    }
    if (c->pos == start) return NULL;
    return xp_strndup(c, c->expr + start, c->pos - start);
}

static bool xp_parse_qname(XpCtx *c, const char **prefix, const char **name)
{
    *prefix = NULL;
    const char *ret = xp_parse_ncname(c);
    if (ret && xp_cur(c) == ':')
    {
        *prefix = ret;
        xp_next(c);
        ret = xp_parse_ncname(c);
    }
    *name = ret;
    return ret != NULL;
}

/** Scan a name (with colons) without consuming it, returns its length or 0. */
static size_t xp_scan_name(XpCtx *c)
{
    size_t l;
    int ch = xp_cur_char(c, &l);
    if (ch == ' ' || ch == '>' || ch == '/' || (!xml_char_is_letter(ch) && ch != '_' && ch != ':')) return 0;
    size_t p = c->pos;
    while (ch != 0 && (xp_is_name_char(ch) || ch == ':'))
    {
        p += l;
        ch = xp_utf8(c->expr + p, &l);
    }
    return p - c->pos;
}

static bool xp_is_char_ch(int ch)
{
    return ch >= 0x20 || ch == 0x09 || ch == 0x0A || ch == 0x0D;
}

static const char *xp_parse_literal(XpCtx *c)
{
    int q = xp_cur(c);
    if (q != '"' && q != '\'') xp_error(c, XML_XPATH_ERR_START_LITERAL);
    xp_next(c);
    size_t start = c->pos;
    while (xp_is_char_ch(xp_cur(c)) && xp_cur(c) != q) xp_next(c);
    if (!xp_is_char_ch(xp_cur(c))) xp_error(c, XML_XPATH_ERR_UNFINISHED_LITERAL);
    const char *ret = xp_strndup(c, c->expr + start, c->pos - start);
    xp_next(c);
    return ret;
}

/** Port of libxml2 xmlXPathCompNumber. */
static XpExpr *xp_compile_number(XpCtx *c)
{
    double ret = 0;
    bool ok = false;
    int exponent = 0;
    bool is_exponent_negative = false;
    if (xp_cur(c) != '.' && !xp_is_ascii_digit(xp_cur(c))) xp_error(c, XML_XPATH_ERR_NUMBER);
    while (xp_is_ascii_digit(xp_cur(c)))
    {
        ret = ret * 10;
        ret = ret + (double)(xp_cur(c) - '0');
        ok = true;
        xp_next(c);
    }
    if (xp_cur(c) == '.')
    {
        int frac = 0;
        double fraction = 0;
        xp_next(c);
        if (!xp_is_ascii_digit(xp_cur(c)) && !ok) xp_error(c, XML_XPATH_ERR_NUMBER);
        while (xp_cur(c) == '0')
        {
            frac++;
            xp_next(c);
        }
        int max = frac + XP_MAX_FRAC;
        while (xp_is_ascii_digit(xp_cur(c)) && frac < max)
        {
            fraction = fraction * 10 + (xp_cur(c) - '0');
            frac++;
            xp_next(c);
        }
        fraction /= pow(10.0, frac);
        ret = ret + fraction;
        while (xp_is_ascii_digit(xp_cur(c))) xp_next(c);
    }
    if (xp_cur(c) == 'e' || xp_cur(c) == 'E')
    {
        xp_next(c);
        if (xp_cur(c) == '-')
        {
            is_exponent_negative = true;
            xp_next(c);
        }
        else if (xp_cur(c) == '+')
        {
            xp_next(c);
        }
        while (xp_is_ascii_digit(xp_cur(c)))
        {
            if (exponent < 1000000) exponent = exponent * 10 + (xp_cur(c) - '0');
            xp_next(c);
        }
        if (is_exponent_negative) exponent = -exponent;
        ret *= pow(10.0, (double)exponent);
    }
    XpExpr *e = xp_new(c, XP_NUMBER);
    e->num = ret;
    return e;
}

static XpExpr *xp_compile_predicate(XpCtx *c, bool filter)
{
    xp_skip_blanks(c);
    if (xp_cur(c) != '[') xp_error(c, XML_XPATH_ERR_INVALID_PREDICATE);
    xp_next(c);
    xp_skip_blanks(c);
    XpExpr *e = xp_compile_expr(c, filter);
    if (xp_cur(c) != ']') xp_error(c, XML_XPATH_ERR_INVALID_PREDICATE);
    xp_next(c);
    xp_skip_blanks(c);
    return e;
}

static XpExpr *xp_compile_variable_reference(XpCtx *c)
{
    xp_skip_blanks(c);
    if (xp_cur(c) != '$') xp_error(c, XML_XPATH_ERR_VARIABLE_REF);
    xp_next(c);
    const char *prefix, *name;
    if (!xp_parse_qname(c, &prefix, &name)) xp_error(c, XML_XPATH_ERR_VARIABLE_REF);
    xp_skip_blanks(c);
    XpExpr *e = xp_new(c, XP_VAR);
    e->prefix = prefix;
    e->name = name;
    return e;
}

static XpExpr *xp_compile_function_call(XpCtx *c)
{
    const char *prefix, *name;
    if (!xp_parse_qname(c, &prefix, &name)) xp_error(c, XML_XPATH_ERR_EXPR);
    xp_skip_blanks(c);
    if (xp_cur(c) != '(') xp_error(c, XML_XPATH_ERR_EXPR);
    xp_next(c);
    xp_skip_blanks(c);
    XpExpr *e = xp_new(c, XP_CALL);
    e->prefix = prefix;
    e->name = name;
    size_t cap = 0;
    if (xp_cur(c) != ')')
    {
        while (xp_cur(c) != 0)
        {
            XpExpr *a = xp_compile_expr(c, true);
            if (e->num_args == cap)
            {
                cap = cap ? cap * 2 : 4;
                XpExpr **na = (XpExpr**)xp_alloc(c, cap * sizeof(XpExpr*));
                if (e->num_args) memcpy(na, e->args, e->num_args * sizeof(XpExpr*));
                e->args = na;
            }
            e->args[e->num_args++] = a;
            if (xp_cur(c) == ')') break;
            if (xp_cur(c) != ',') xp_error(c, XML_XPATH_ERR_EXPR);
            xp_next(c);
            xp_skip_blanks(c);
        }
    }
    xp_next(c);
    xp_skip_blanks(c);
    return e;
}

static XpExpr *xp_compile_primary_expr(XpCtx *c)
{
    XpExpr *e;
    xp_skip_blanks(c);
    if (xp_cur(c) == '$')
    {
        e = xp_compile_variable_reference(c);
    }
    else if (xp_cur(c) == '(')
    {
        xp_next(c);
        xp_skip_blanks(c);
        e = xp_compile_expr(c, true);
        if (xp_cur(c) != ')') xp_error(c, XML_XPATH_ERR_EXPR);
        xp_next(c);
        xp_skip_blanks(c);
    }
    else if (xp_is_ascii_digit(xp_cur(c)) || (xp_cur(c) == '.' && xp_is_ascii_digit(xp_nxt(c, 1))))
    {
        e = xp_compile_number(c);
    }
    else if (xp_cur(c) == '\'' || xp_cur(c) == '"')
    {
        e = xp_new(c, XP_LITERAL);
        e->str = xp_parse_literal(c);
    }
    else
    {
        e = xp_compile_function_call(c);
    }
    xp_skip_blanks(c);
    return e;
}

static XpExpr *xp_compile_filter_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_primary_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '[')
    {
        XpExpr *p = xp_compile_predicate(c, true);
        e = xp_bin(c, XP_FILTER, e, p, 0);
        xp_skip_blanks(c);
    }
    return e;
}

static void xp_add_step(XpCtx *c, XpStep **steps, size_t *num, XpStep s)
{
    // Steps are reallocated in the arena, the arrays are small.
    XpStep *n = (XpStep*)xp_alloc(c, (*num + 1) * sizeof(XpStep));
    if (*num) memcpy(n, *steps, *num * sizeof(XpStep));
    n[(*num)++] = s;
    *steps = n;
}

static XpStep xp_descendant_or_self_node(void)
{
    XpStep s;
    memset(&s, 0, sizeof(s));
    s.axis = XP_AXIS_DESCENDANT_OR_SELF;
    s.test = XP_TEST_TYPE;
    s.type = XP_TYPE_NODE;
    return s;
}

static int xp_axis_from_name(const char *name)
{
    static const struct { const char *n; int a; } axes[] = {
        { "ancestor", XP_AXIS_ANCESTOR }, { "ancestor-or-self", XP_AXIS_ANCESTOR_OR_SELF },
        { "attribute", XP_AXIS_ATTRIBUTE }, { "child", XP_AXIS_CHILD }, { "descendant", XP_AXIS_DESCENDANT },
        { "descendant-or-self", XP_AXIS_DESCENDANT_OR_SELF }, { "following", XP_AXIS_FOLLOWING },
        { "following-sibling", XP_AXIS_FOLLOWING_SIBLING }, { "namespace", XP_AXIS_NAMESPACE },
        { "parent", XP_AXIS_PARENT }, { "preceding", XP_AXIS_PRECEDING },
        { "preceding-sibling", XP_AXIS_PRECEDING_SIBLING }, { "self", XP_AXIS_SELF }
    };
    for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i) if (!strcmp(axes[i].n, name)) return axes[i].a;
    return 0;
}

static void xp_compile_step(XpCtx *c, XpStep **steps, size_t *num)
{
    xp_skip_blanks(c);
    if (xp_cur(c) == '.' && xp_nxt(c, 1) == '.')
    {
        xp_skip(c, 2);
        xp_skip_blanks(c);
        XpStep s;
        memset(&s, 0, sizeof(s));
        s.axis = XP_AXIS_PARENT;
        s.test = XP_TEST_TYPE;
        s.type = XP_TYPE_NODE;
        xp_add_step(c, steps, num, s);
        return;
    }
    if (xp_cur(c) == '.')
    {
        xp_next(c);
        xp_skip_blanks(c);
        return;
    }
    const char *name = NULL;
    int axis;
    if (xp_cur(c) == '*')
    {
        axis = XP_AXIS_CHILD;
    }
    else
    {
        name = xp_parse_ncname(c);
        if (name)
        {
            axis = xp_axis_from_name(name);
            if (axis)
            {
                xp_skip_blanks(c);
                if (xp_cur(c) == ':' && xp_nxt(c, 1) == ':')
                {
                    xp_skip(c, 2);
                    name = NULL;
                }
                else
                {
                    axis = XP_AXIS_CHILD;
                }
            }
            else
            {
                axis = XP_AXIS_CHILD;
            }
        }
        else if (xp_cur(c) == '@')
        {
            xp_next(c);
            axis = XP_AXIS_ATTRIBUTE;
        }
        else
        {
            axis = XP_AXIS_CHILD;
        }
    }

    // xmlXPathCompNodeTest
    XpStep s;
    memset(&s, 0, sizeof(s));
    s.axis = axis;
    s.type = XP_TYPE_NODE;
    xp_skip_blanks(c);
    if (!name && xp_cur(c) == '*')
    {
        xp_next(c);
        s.test = XP_TEST_ALL;
    }
    else
    {
        if (!name) name = xp_parse_ncname(c);
        if (!name) xp_error(c, XML_XPATH_ERR_EXPR);
        bool blanks = xp_is_blank(xp_cur(c));
        xp_skip_blanks(c);
        if (xp_cur(c) == '(')
        {
            xp_next(c);
            if (!strcmp(name, "comment")) s.type = XP_TYPE_COMMENT;
            else if (!strcmp(name, "node")) s.type = XP_TYPE_NODE;
            else if (!strcmp(name, "processing-instruction")) s.type = XP_TYPE_PI;
            else if (!strcmp(name, "text")) s.type = XP_TYPE_TEXT;
            else xp_error(c, XML_XPATH_ERR_EXPR);
            s.test = XP_TEST_TYPE;
            xp_skip_blanks(c);
            name = NULL;
            if (s.type == XP_TYPE_PI && xp_cur(c) != ')')
            {
                name = xp_parse_literal(c);
                s.test = XP_TEST_PI;
                xp_skip_blanks(c);
            }
            if (xp_cur(c) != ')') xp_error(c, XML_XPATH_ERR_UNCLOSED);
            xp_next(c);
        }
        else
        {
            s.test = XP_TEST_NAME;
            if (!blanks && xp_cur(c) == ':')
            {
                xp_next(c);
                s.prefix = name;
                if (xp_cur(c) == '*')
                {
                    xp_next(c);
                    s.test = XP_TEST_ALL;
                    name = NULL;
                }
                else
                {
                    name = xp_parse_ncname(c);
                    if (!name) xp_error(c, XML_XPATH_ERR_EXPR);
                }
            }
        }
    }
    s.name = name;
    xp_skip_blanks(c);
    size_t cap = 0;
    while (xp_cur(c) == '[')
    {
        XpExpr *p = xp_compile_predicate(c, false);
        if (s.num_preds == cap)
        {
            cap = cap ? cap * 2 : 2;
            XpExpr **np = (XpExpr**)xp_alloc(c, cap * sizeof(XpExpr*));
            if (s.num_preds) memcpy(np, s.preds, s.num_preds * sizeof(XpExpr*));
            s.preds = np;
        }
        s.preds[s.num_preds++] = p;
    }
    xp_add_step(c, steps, num, s);
}

static void xp_compile_relative_location_path(XpCtx *c, XpStep **steps, size_t *num)
{
    xp_skip_blanks(c);
    if (xp_cur(c) == '/' && xp_nxt(c, 1) == '/')
    {
        xp_skip(c, 2);
        xp_skip_blanks(c);
        xp_add_step(c, steps, num, xp_descendant_or_self_node());
    }
    else if (xp_cur(c) == '/')
    {
        xp_next(c);
        xp_skip_blanks(c);
    }
    xp_compile_step(c, steps, num);
    xp_skip_blanks(c);
    while (xp_cur(c) == '/')
    {
        if (xp_nxt(c, 1) == '/')
        {
            xp_skip(c, 2);
            xp_skip_blanks(c);
            xp_add_step(c, steps, num, xp_descendant_or_self_node());
            xp_compile_step(c, steps, num);
        }
        else
        {
            xp_next(c);
            xp_skip_blanks(c);
            xp_compile_step(c, steps, num);
        }
        xp_skip_blanks(c);
    }
}

static void xp_compile_location_path(XpCtx *c, XpStep **steps, size_t *num)
{
    xp_skip_blanks(c);
    if (xp_cur(c) != '/')
    {
        xp_compile_relative_location_path(c, steps, num);
        return;
    }
    while (xp_cur(c) == '/')
    {
        if (xp_nxt(c, 1) == '/')
        {
            xp_skip(c, 2);
            xp_skip_blanks(c);
            xp_add_step(c, steps, num, xp_descendant_or_self_node());
            xp_compile_relative_location_path(c, steps, num);
        }
        else
        {
            xp_next(c);
            xp_skip_blanks(c);
            int ch = xp_cur(c);
            if (ch != 0 && (xp_is_ascii_letter(ch) || ch == '_' || ch == '.' || ch == '@' || ch == '*'))
            {
                xp_compile_relative_location_path(c, steps, num);
            }
        }
    }
}

static bool xp_is_node_type(const char *name, size_t len)
{
    return (len == 4 && !strncmp(name, "node", 4)) || (len == 4 && !strncmp(name, "text", 4)) ||
           (len == 7 && !strncmp(name, "comment", 7)) || (len == 22 && !strncmp(name, "processing-instruction", 22));
}

static XpExpr *xp_compile_path_expr(XpCtx *c)
{
    bool lc = true;
    xp_skip_blanks(c);
    int ch = xp_cur(c);
    if (ch == '$' || ch == '(' || xp_is_ascii_digit(ch) || ch == '\'' || ch == '"' ||
        (ch == '.' && xp_is_ascii_digit(xp_nxt(c, 1))))
    {
        lc = false;
    }
    else if (ch == '*' || ch == '/' || ch == '@' || ch == '.')
    {
        lc = true;
    }
    else
    {
        xp_skip_blanks(c);
        size_t len = xp_scan_name(c);
        if (len == 0) xp_error(c, XML_XPATH_ERR_EXPR);
        const char *name = c->expr + c->pos;
        bool has_axis = false;
        for (size_t k = 0; k + 1 < len; ++k) if (name[k] == ':' && name[k+1] == ':') has_axis = true;
        if (has_axis)
        {
            lc = true;
        }
        else
        {
            size_t l = len;
            while (xp_nxt(c, l) != 0)
            {
                int n = xp_nxt(c, l);
                if (n == '/') { lc = true; break; }
                else if (xp_is_blank(n)) { /* ignore blanks */ }
                else if (n == ':') { lc = true; break; }
                else if (n == '(') { lc = xp_is_node_type(name, len); break; }
                else { lc = true; break; }
                l++;
            }
            if (xp_nxt(c, l) == 0) lc = true;
        }
    }
    if (lc)
    {
        XpExpr *e = xp_new(c, XP_PATH);
        e->start = xp_cur(c) == '/' ? XP_START_ROOT : XP_START_CONTEXT;
        xp_compile_location_path(c, &e->steps, &e->num_steps);
        return e;
    }
    XpExpr *f = xp_compile_filter_expr(c);
    if (xp_cur(c) == '/' && xp_nxt(c, 1) == '/')
    {
        xp_skip(c, 2);
        xp_skip_blanks(c);
        XpExpr *e = xp_new(c, XP_PATH);
        e->start = XP_START_FILTER;
        e->filter = f;
        xp_add_step(c, &e->steps, &e->num_steps, xp_descendant_or_self_node());
        xp_compile_relative_location_path(c, &e->steps, &e->num_steps);
        return e;
    }
    else if (xp_cur(c) == '/')
    {
        XpExpr *e = xp_new(c, XP_PATH);
        e->start = XP_START_FILTER;
        e->filter = f;
        xp_compile_relative_location_path(c, &e->steps, &e->num_steps);
        return e;
    }
    xp_skip_blanks(c);
    return f;
}

static XpExpr *xp_compile_union_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_path_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '|')
    {
        xp_next(c);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_path_expr(c);
        e = xp_bin(c, XP_UNION, e, r, 0);
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_unary_expr(XpCtx *c)
{
    bool minus = false, found = false;
    xp_skip_blanks(c);
    while (xp_cur(c) == '-')
    {
        minus = !minus;
        found = true;
        xp_next(c);
        xp_skip_blanks(c);
    }
    XpExpr *e = xp_compile_union_expr(c);
    if (found) e = xp_bin(c, XP_NEGATE, e, NULL, minus ? 1 : 0);
    return e;
}

static XpExpr *xp_compile_multiplicative_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_unary_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '*' || (xp_cur(c) == 'd' && xp_nxt(c, 1) == 'i' && xp_nxt(c, 2) == 'v') ||
           (xp_cur(c) == 'm' && xp_nxt(c, 1) == 'o' && xp_nxt(c, 2) == 'd'))
    {
        int op;
        if (xp_cur(c) == '*') { op = XP_OP_MUL; xp_next(c); }
        else if (xp_cur(c) == 'd') { op = XP_OP_DIV; xp_skip(c, 3); }
        else { op = XP_OP_MOD; xp_skip(c, 3); }
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_unary_expr(c);
        e = xp_bin(c, XP_ARITH, e, r, op);
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_additive_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_multiplicative_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '+' || xp_cur(c) == '-')
    {
        bool plus = xp_cur(c) == '+';
        xp_next(c);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_multiplicative_expr(c);
        e = xp_bin(c, XP_ARITH, e, r, plus ? XP_OP_ADD : XP_OP_SUB);
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_relational_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_additive_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '<' || xp_cur(c) == '>')
    {
        bool inf = xp_cur(c) == '<';
        bool strict = xp_nxt(c, 1) != '=';
        xp_next(c);
        if (!strict) xp_next(c);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_additive_expr(c);
        e = xp_bin(c, XP_COMPARE, e, r, (inf ? 1 : 0) | (strict ? 2 : 0));
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_equality_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_relational_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == '=' || (xp_cur(c) == '!' && xp_nxt(c, 1) == '='))
    {
        bool eq = xp_cur(c) == '=';
        xp_next(c);
        if (!eq) xp_next(c);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_relational_expr(c);
        e = xp_bin(c, XP_EQUAL, e, r, eq ? 1 : 0);
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_and_expr(XpCtx *c)
{
    XpExpr *e = xp_compile_equality_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == 'a' && xp_nxt(c, 1) == 'n' && xp_nxt(c, 2) == 'd')
    {
        xp_skip(c, 3);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_equality_expr(c);
        e = xp_bin(c, XP_AND, e, r, 0);
        xp_skip_blanks(c);
    }
    return e;
}

static XpExpr *xp_compile_expr(XpCtx *c, bool sort)
{
    xp_enter(c);
    XpExpr *e = xp_compile_and_expr(c);
    xp_skip_blanks(c);
    while (xp_cur(c) == 'o' && xp_nxt(c, 1) == 'r')
    {
        xp_skip(c, 2);
        xp_skip_blanks(c);
        XpExpr *r = xp_compile_and_expr(c);
        e = xp_bin(c, XP_OR, e, r, 0);
        xp_skip_blanks(c);
    }
    if (sort && !xp_is_value(e)) e = xp_bin(c, XP_SORT, e, NULL, 0);
    xp_leave(c);
    return e;
}

static bool xp_is_descendant_or_self_node(XpStep *s)
{
    return s->axis == XP_AXIS_DESCENDANT_OR_SELF && s->test == XP_TEST_TYPE && s->type == XP_TYPE_NODE &&
           s->num_preds == 0;
}

/** Like libxml2 xmlXPathOptimizeExpression: descendant-or-self::node()/child::x is descendant::x. */
static void xp_optimize(XpExpr *e)
{
    if (!e) return;
    if (e->kind == XP_PATH)
    {
        size_t i = e->num_steps ? e->num_steps - 1 : 0;
        while (i >= 1 && e->num_steps > 1)
        {
            XpStep *s = &e->steps[i];
            XpStep *prev = &e->steps[i-1];
            if (s->num_preds == 0 && xp_is_descendant_or_self_node(prev))
            {
                if (s->axis == XP_AXIS_CHILD || s->axis == XP_AXIS_DESCENDANT || s->axis == XP_AXIS_SELF ||
                    s->axis == XP_AXIS_DESCENDANT_OR_SELF)
                {
                    s->axis = (s->axis == XP_AXIS_CHILD || s->axis == XP_AXIS_DESCENDANT) ? XP_AXIS_DESCENDANT
                                                                                           : XP_AXIS_DESCENDANT_OR_SELF;
                    memmove(&e->steps[i-1], &e->steps[i], (e->num_steps - i) * sizeof(XpStep));
                    e->num_steps--;
                    if (i < 2) break;
                    i -= 2;
                    continue;
                }
            }
            i--;
        }
        for (size_t k = 0; k < e->num_steps; ++k)
        {
            for (size_t j = 0; j < e->steps[k].num_preds; ++j) xp_optimize(e->steps[k].preds[j]);
        }
        xp_optimize(e->filter);
        return;
    }
    xp_optimize(e->l);
    xp_optimize(e->r);
    for (size_t k = 0; k < e->num_args; ++k) xp_optimize(e->args[k]);
}

// Axes ////////////////////////////////////////////////////////////

typedef struct
{
    XpStep *step;
    const char *uri;
    XpNodeSet seq;
    bool range;
    long max_pos;
    bool break_on_first_hit;
    long pos;
} XpCollector;

static bool xp_matches(XpStep *st, XMQNode *cur, const char *uri)
{
    XmlNodeKind t = xml_node_kind(cur);
    switch (st->test)
    {
    case XP_TEST_TYPE:
        if (st->type == XP_TYPE_NODE)
        {
            switch (t)
            {
            case XML_DOM_DOCUMENT: case XML_DOM_ELEMENT: case XML_DOM_ATTRIBUTE: case XML_DOM_PI:
            case XML_DOM_COMMENT: case XML_DOM_CDATA: case XML_DOM_TEXT: case XML_DOM_NAMESPACE:
                return true;
            default:
                return false;
            }
        }
        if (st->type == XP_TYPE_COMMENT) return t == XML_DOM_COMMENT;
        if (st->type == XP_TYPE_PI) return t == XML_DOM_PI;
        if (st->type == XP_TYPE_TEXT) return t == XML_DOM_TEXT || t == XML_DOM_CDATA;
        return false;
    case XP_TEST_PI:
        return t == XML_DOM_PI && (!st->name || xp_streq(st->name, xml_node_name(cur)));
    case XP_TEST_ALL:
        if (st->axis == XP_AXIS_ATTRIBUTE)
        {
            if (t != XML_DOM_ATTRIBUTE) return false;
            if (!st->prefix) return true;
            return xml_node_ns(cur) && xp_streq(uri, xml_ns_href(xml_node_ns(cur)));
        }
        if (st->axis == XP_AXIS_NAMESPACE) return t == XML_DOM_NAMESPACE;
        if (t != XML_DOM_ELEMENT) return false;
        if (!st->prefix) return true;
        return xml_node_ns(cur) && xp_streq(uri, xml_ns_href(xml_node_ns(cur)));
    case XP_TEST_NAME:
        if (st->axis == XP_AXIS_ATTRIBUTE)
        {
            if (t != XML_DOM_ATTRIBUTE) return false;
            if (!xp_streq(st->name, xml_node_name(cur))) return false;
            XmlNs *ns = xml_node_ns(cur);
            if (!st->prefix) return !ns || !xml_ns_prefix(ns);
            return ns && xp_streq(uri, xml_ns_href(ns));
        }
        if (st->axis == XP_AXIS_NAMESPACE)
        {
            if (t != XML_DOM_NAMESPACE) return false;
            const char *p = xml_node_name(cur);
            return p && !strcmp(p, st->name);
        }
        if (t != XML_DOM_ELEMENT) return false;
        if (!xp_streq(st->name, xml_node_name(cur))) return false;
        if (!st->prefix) return xml_node_ns(cur) == NULL;
        return xml_node_ns(cur) && xp_streq(uri, xml_ns_href(xml_node_ns(cur)));
    }
    return false;
}

/** Returns true to stop the traversal. */
static bool xp_visit(XpCtx *c, XpCollector *col, XMQNode *n)
{
    if (!xp_matches(col->step, n, col->uri)) return false;
    if (col->range)
    {
        if (++col->pos == col->max_pos)
        {
            xp_ns_add(c, &col->seq, n);
            return true;
        }
        return false;
    }
    xp_ns_add(c, &col->seq, n);
    return col->break_on_first_hit;
}

static bool xp_is_attr_or_ns(XMQNode *n)
{
    XmlNodeKind k = xml_node_kind(n);
    return k == XML_DOM_ATTRIBUTE || k == XML_DOM_NAMESPACE;
}

/** First child, entity references have no children (entities are not expanded). */
static XMQNode *xp_fc(XMQNode *n)
{
    switch (xml_node_kind(n))
    {
    case XML_DOM_ENTITY_REF:
    case XML_DOM_ENTITY_DECL:
    case XML_DOM_NAMESPACE:
        return NULL;
    default:
        return xml_node_first_child(n);
    }
}

static XMQNode *xp_lc(XMQNode *n)
{
    switch (xml_node_kind(n))
    {
    case XML_DOM_ENTITY_REF:
    case XML_DOM_ENTITY_DECL:
    case XML_DOM_NAMESPACE:
        return NULL;
    default:
        return xml_node_last_child(n);
    }
}

static XMQNode *xp_next_child(XMQNode *ctx, XMQNode *cur)
{
    if (!cur)
    {
        switch (xml_node_kind(ctx))
        {
        case XML_DOM_ELEMENT: case XML_DOM_TEXT: case XML_DOM_CDATA: case XML_DOM_ENTITY_REF: case XML_DOM_PI:
        case XML_DOM_COMMENT: case XML_DOM_DTD: case XML_DOM_DOCUMENT:
            return xp_fc(ctx);
        default:
            return NULL;
        }
    }
    if (xml_node_kind(cur) == XML_DOM_DOCUMENT) return NULL;
    return xml_node_next(cur);
}

static XMQNode *xp_next_descendant(XMQNode *ctx, XMQNode *cur)
{
    if (!cur)
    {
        if (xp_is_attr_or_ns(ctx)) return NULL;
        return xp_fc(ctx);
    }
    if (xml_node_kind(cur) == XML_DOM_NAMESPACE) return NULL;
    XMQNode *ch = xp_fc(cur);
    if (ch && xml_node_kind(ch) != XML_DOM_ENTITY_DECL)
    {
        cur = ch;
        if (xml_node_kind(cur) != XML_DOM_DTD) return cur;
    }
    if (cur == ctx) return NULL;
    while (xml_node_next(cur))
    {
        cur = xml_node_next(cur);
        if (xml_node_kind(cur) != XML_DOM_ENTITY_DECL && xml_node_kind(cur) != XML_DOM_DTD) return cur;
    }
    do
    {
        cur = xml_node_parent(cur);
        if (!cur) break;
        if (cur == ctx) return NULL;
        if (xml_node_next(cur)) return xml_node_next(cur);
    } while (cur);
    return NULL;
}

static XMQNode *xp_parent_of(XMQNode *n, XMQNode *doc)
{
    switch (xml_node_kind(n))
    {
    case XML_DOM_DOCUMENT:
        return NULL;
    case XML_DOM_ATTRIBUTE:
    case XML_DOM_NAMESPACE:
        return xml_node_parent(n);
    default:
        if (!xml_node_parent(n)) return (doc && doc != n) ? doc : NULL;
        return xml_node_parent(n);
    }
}

static XMQNode *xp_next_ancestor(XMQNode *cur, XMQNode *doc)
{
    if (doc && cur == xml_node_first_child(doc)) return doc;
    if (cur == doc) return NULL;
    if (xml_node_kind(cur) == XML_DOM_DOCUMENT) return NULL;
    return xml_node_parent(cur);
}

static void xp_following(XpCtx *c, XMQNode *start, XpCollector *col)
{
    XMQNode *doc = c->doc;
    XMQNode *cur = NULL;
    while (true)
    {
        XMQNode *ch = (cur && !xp_is_attr_or_ns(cur)) ? xp_fc(cur) : NULL;
        if (ch)
        {
            cur = ch;
        }
        else
        {
            if (!cur)
            {
                cur = start;
                if (xp_is_attr_or_ns(cur)) cur = xml_node_parent(cur);
                if (!cur) return;
            }
            if (xml_node_next(cur))
            {
                cur = xml_node_next(cur);
            }
            else
            {
                do
                {
                    cur = xml_node_parent(cur);
                    if (!cur) return;
                    if (cur == doc) return;
                } while (!xml_node_next(cur));
                cur = xml_node_next(cur);
            }
        }
        if (xp_visit(c, col, cur)) return;
    }
}

static void xp_preceding(XpCtx *c, XMQNode *start, XpCollector *col)
{
    XMQNode *cur = start;
    if (xp_is_attr_or_ns(cur)) cur = xml_node_parent(cur);
    if (!cur) return;
    XMQNode *ancestor = xml_node_parent(cur);
    XMQNode *doc_children = c->doc ? xml_node_first_child(c->doc) : NULL;
    while (true)
    {
        if (xml_node_kind(cur) == XML_DOM_NAMESPACE) return;
        if (xml_node_prev(cur) && xml_node_kind(xml_node_prev(cur)) == XML_DOM_DTD) cur = xml_node_prev(cur);
        bool found = false;
        while (!xml_node_prev(cur))
        {
            cur = xml_node_parent(cur);
            if (!cur) return;
            if (cur == doc_children) return;
            if (cur != ancestor)
            {
                found = true;
                break;
            }
            ancestor = xml_node_parent(cur);
        }
        if (!found)
        {
            cur = xml_node_prev(cur);
            for (XMQNode *l = xp_lc(cur); l; l = xp_lc(cur)) cur = l;
        }
        if (xp_visit(c, col, cur)) return;
    }
}

static void xp_traverse(XpCtx *c, XMQNode *n, XpCollector *col)
{
    XMQNode *doc = c->doc;
    switch (col->step->axis)
    {
    case XP_AXIS_ATTRIBUTE:
        if (xml_node_kind(n) == XML_DOM_ELEMENT)
        {
            for (XMQAttr *a = xml_node_first_attr(n); a; a = xml_attr_next(a))
            {
                if (xp_visit(c, col, xml_attr_node(a))) return;
            }
        }
        return;
    case XP_AXIS_NAMESPACE:
        if (xml_node_kind(n) == XML_DOM_ELEMENT)
        {
            for (XMQNode **ns = xp_namespace_nodes(c, n); *ns; ++ns)
            {
                if (xp_visit(c, col, *ns)) return;
            }
        }
        return;
    case XP_AXIS_SELF:
        xp_visit(c, col, n);
        return;
    case XP_AXIS_CHILD:
        for (XMQNode *cur = xp_next_child(n, NULL); cur; cur = xp_next_child(n, cur))
        {
            if (xp_visit(c, col, cur)) return;
        }
        return;
    case XP_AXIS_DESCENDANT:
        for (XMQNode *cur = xp_next_descendant(n, NULL); cur; cur = xp_next_descendant(n, cur))
        {
            if (xp_visit(c, col, cur)) return;
        }
        return;
    case XP_AXIS_DESCENDANT_OR_SELF:
        if (xp_visit(c, col, n)) return;
        if (xp_is_attr_or_ns(n)) return;
        for (XMQNode *cur = xp_next_descendant(n, n); cur; cur = xp_next_descendant(n, cur))
        {
            if (xp_visit(c, col, cur)) return;
        }
        return;
    case XP_AXIS_PARENT:
    {
        XMQNode *p = xp_parent_of(n, doc);
        if (p) xp_visit(c, col, p);
        return;
    }
    case XP_AXIS_ANCESTOR:
    case XP_AXIS_ANCESTOR_OR_SELF:
    {
        XMQNode *first;
        if (col->step->axis == XP_AXIS_ANCESTOR_OR_SELF)
        {
            if (xp_visit(c, col, n)) return;
            first = xp_next_ancestor(n, doc);
        }
        else
        {
            first = xp_parent_of(n, doc);
        }
        for (XMQNode *cur = first; cur; cur = xp_next_ancestor(cur, doc))
        {
            if (xp_visit(c, col, cur)) return;
        }
        return;
    }
    case XP_AXIS_FOLLOWING_SIBLING:
        if (xp_is_attr_or_ns(n)) return;
        for (XMQNode *cur = xml_node_next(n); cur; cur = xml_node_next(cur))
        {
            if (xp_visit(c, col, cur)) return;
        }
        return;
    case XP_AXIS_PRECEDING_SIBLING:
    {
        if (xp_is_attr_or_ns(n)) return;
        XMQNode *cur = xml_node_prev(n);
        while (cur)
        {
            if (xp_visit(c, col, cur)) return;
            if (xml_node_prev(cur) && xml_node_kind(xml_node_prev(cur)) == XML_DOM_DTD) cur = xml_node_prev(cur);
            cur = xml_node_prev(cur);
        }
        return;
    }
    case XP_AXIS_FOLLOWING:
        xp_following(c, n, col);
        return;
    case XP_AXIS_PRECEDING:
        xp_preceding(c, n, col);
        return;
    }
}

// Evaluation //////////////////////////////////////////////////////

static XpValue xp_eval(XpCtx *c, XpExpr *e);
static XpValue xp_eval_path(XpCtx *c, XpExpr *e, bool to_bool);

static XpValue xp_eval_predicate(XpCtx *c, XpExpr *pred)
{
    XpExpr *e = pred;
    while (e->kind == XP_SORT) e = e->l;
    if (e->kind == XP_PATH && e->num_steps > 0) return xp_eval_path(c, e, true);
    return xp_eval(c, pred);
}

static XpNodeSet xp_apply_predicate(XpCtx *c, XpNodeSet set, XpExpr *pred)
{
    XMQNode *old_node = c->node, *old_doc = c->doc;
    long old_size = c->size, old_pos = c->position;
    XpNodeSet out = { NULL, 0, 0 };
    c->size = (long)set.len;
    for (size_t i = 0; i < set.len; ++i)
    {
        XMQNode *n = set.nodes[i];
        c->node = n;
        c->position = (long)i + 1;
        if (xml_node_kind(n) != XML_DOM_NAMESPACE) c->doc = xp_document_of(n);
        XpValue r = xp_eval_predicate(c, pred);
        bool keep;
        if (r.type == XML_XPATH_NUMBER) keep = r.num == (double)(i + 1);
        else keep = xp_to_boolean(r);
        if (keep) xp_ns_add(c, &out, n);
    }
    c->node = old_node;
    c->doc = old_doc;
    c->size = old_size;
    c->position = old_pos;
    return out;
}

/** A single numeric predicate [n] at the end of the step, as a position, else 0. */
static long xp_positional_predicate(XpStep *s)
{
    if (s->num_preds == 0) return 0;
    XpExpr *p = s->preds[s->num_preds - 1];
    if (p->kind != XP_NUMBER) return 0;
    double f = p->num;
    if (f > INT_MIN && f < INT_MAX && f == (double)(int)f) return (int)f;
    return 0;
}

static bool xp_has_positional_predicate(XpStep *s, long *pos)
{
    if (s->num_preds == 0) return false;
    XpExpr *p = s->preds[s->num_preds - 1];
    if (p->kind != XP_NUMBER) return false;
    double f = p->num;
    if (!(f > INT_MIN && f < INT_MAX && f == (double)(int)f)) return false;
    *pos = (int)f;
    return true;
}

static XpNodeSet xp_collect(XpCtx *c, XpStep *s, XpNodeSet context, bool to_bool)
{
    const char *uri = NULL;
    if (s->prefix)
    {
        uri = xp_lookup_ns(c, s->prefix);
        if (!uri) xp_error(c, XML_XPATH_ERR_UNDEF_PREFIX);
    }
    XpNodeSet out = { NULL, 0, 0 };
    if (context.len == 0) return out;
    long max_pos = 0;
    bool has_pos = xp_has_positional_predicate(s, &max_pos);
    (void)xp_positional_predicate;
    // A single [n] predicate stops the axis traversal at the n:th node.
    bool axis_range = has_pos && s->num_preds == 1;
    bool break_on_first_hit = to_bool && (s->num_preds == 0 || axis_range);
    XpPtrMap seen = { NULL, NULL, 0, 0 };
    bool use_seen = context.len > 1;
    XMQNode *old_node = c->node;
    for (size_t k = 0; k < context.len; ++k)
    {
        XMQNode *n = context.nodes[k];
        c->node = n;
        XpCollector col;
        memset(&col, 0, sizeof(col));
        col.step = s;
        col.uri = uri;
        col.range = axis_range;
        col.max_pos = axis_range ? max_pos : 0;
        col.break_on_first_hit = break_on_first_hit;
        xp_traverse(c, n, &col);
        XpNodeSet seq = col.seq;
        if (!axis_range)
        {
            for (size_t p = 0; p < s->num_preds; ++p)
            {
                if (seq.len == 0) break;
                seq = xp_apply_predicate(c, seq, s->preds[p]);
            }
        }
        if (!use_seen)
        {
            for (size_t j = 0; j < seq.len; ++j) xp_ns_add(c, &out, seq.nodes[j]);
        }
        else
        {
            for (size_t j = 0; j < seq.len; ++j)
            {
                if (xp_map_find(&seen, seq.nodes[j])) continue;
                xp_map_put(c, &seen, seq.nodes[j], 1);
                xp_ns_add(c, &out, seq.nodes[j]);
            }
        }
        if (to_bool && seq.len > 0) break;
    }
    c->node = old_node;
    return out;
}

static XpValue xp_eval_path(XpCtx *c, XpExpr *e, bool to_bool)
{
    XpValue cur;
    if (e->start == XP_START_ROOT)
    {
        cur = xp_nodeset();
        if (c->doc) xp_ns_add(c, &cur.ns, c->doc);
    }
    else if (e->start == XP_START_CONTEXT)
    {
        cur = xp_nodeset();
        if (c->node) xp_ns_add(c, &cur.ns, c->node);
    }
    else
    {
        cur = xp_eval(c, e->filter);
    }
    for (size_t i = 0; i < e->num_steps; ++i)
    {
        if (cur.type != XML_XPATH_NODESET) xp_error(c, XML_XPATH_ERR_INVALID_TYPE);
        XpNodeSet ns = xp_collect(c, &e->steps[i], cur.ns, to_bool && i == e->num_steps - 1);
        cur = xp_nodeset();
        cur.ns = ns;
    }
    return cur;
}

// Comparisons ////////////////////////////////////////////////////

static bool xp_equal_values_common(XpCtx *c, XpValue a, XpValue b)
{
    if (a.type == XML_XPATH_BOOLEAN)
    {
        if (b.type == XML_XPATH_BOOLEAN) return a.b == b.b;
        if (b.type == XML_XPATH_NUMBER) return a.b == xp_number_to_boolean(b.num);
        return a.b == (b.str[0] != 0);
    }
    if (a.type == XML_XPATH_NUMBER)
    {
        if (b.type == XML_XPATH_BOOLEAN) return b.b == xp_number_to_boolean(a.num);
        double y = b.type == XML_XPATH_STRING ? xml_xpath_string_to_number(b.str) : b.num;
        return a.num == y;
    }
    if (b.type == XML_XPATH_BOOLEAN) return b.b == (a.str[0] != 0);
    if (b.type == XML_XPATH_STRING) return !strcmp(a.str, b.str);
    (void)c;
    return xml_xpath_string_to_number(a.str) == b.num;
}

static bool xp_equal_values(XpCtx *c, XpValue a, XpValue b, bool neq)
{
    if (a.type == XML_XPATH_NODESET || b.type == XML_XPATH_NODESET)
    {
        if (a.type != XML_XPATH_NODESET)
        {
            XpValue t = a;
            a = b;
            b = t;
        }
        XpNodeSet ns = a.ns;
        if (b.type == XML_XPATH_NODESET)
        {
            XpNodeSet ns2 = b.ns;
            if (ns.len == 0 || ns2.len == 0) return false;
            if (!neq)
            {
                XpPtrMap s1 = { NULL, NULL, 0, 0 };
                for (size_t i = 0; i < ns.len; ++i) xp_map_put(c, &s1, ns.nodes[i], 1);
                for (size_t j = 0; j < ns2.len; ++j) if (xp_map_find(&s1, ns2.nodes[j])) return true;
            }
            const char **v2 = (const char**)xp_alloc(c, ns2.len * sizeof(char*));
            for (size_t j = 0; j < ns2.len; ++j) v2[j] = xp_string_value(c, ns2.nodes[j]);
            for (size_t i = 0; i < ns.len; ++i)
            {
                const char *v1 = xp_string_value(c, ns.nodes[i]);
                for (size_t j = 0; j < ns2.len; ++j) if ((strcmp(v1, v2[j]) == 0) != neq) return true;
            }
            return false;
        }
        if (b.type == XML_XPATH_BOOLEAN)
        {
            bool r = ns.len > 0;
            return neq ? (r != b.b) : (r == b.b);
        }
        if (b.type == XML_XPATH_NUMBER)
        {
            for (size_t i = 0; i < ns.len; ++i)
            {
                double v = xml_xpath_string_to_number(xp_string_value(c, ns.nodes[i]));
                if (!isnan(v))
                {
                    if (!neq && v == b.num) return true;
                    if (neq && v != b.num) return true;
                }
                else if (neq)
                {
                    return true;
                }
            }
            return false;
        }
        for (size_t i = 0; i < ns.len; ++i)
        {
            bool eq = !strcmp(xp_string_value(c, ns.nodes[i]), b.str);
            if (eq && !neq) return true;
            if (!eq && neq) return true;
        }
        return false;
    }
    bool r = xp_equal_values_common(c, a, b);
    return neq ? !r : r;
}

static bool xp_compare_numbers(double x, double y, bool inf, bool strict)
{
    if (inf && strict) return x < y;
    if (inf) return x <= y;
    if (strict) return x > y;
    return x >= y;
}

static bool xp_compare_values(XpCtx *c, XpValue a, XpValue b, bool inf, bool strict)
{
    if (a.type == XML_XPATH_NODESET && b.type == XML_XPATH_NODESET)
    {
        if (a.ns.len == 0 || b.ns.len == 0) return false;
        double *v2 = (double*)xp_alloc(c, b.ns.len * sizeof(double));
        for (size_t j = 0; j < b.ns.len; ++j) v2[j] = xml_xpath_string_to_number(xp_string_value(c, b.ns.nodes[j]));
        for (size_t i = 0; i < a.ns.len; ++i)
        {
            double v1 = xml_xpath_string_to_number(xp_string_value(c, a.ns.nodes[i]));
            if (isnan(v1)) continue;
            for (size_t j = 0; j < b.ns.len; ++j) if (xp_compare_numbers(v1, v2[j], inf, strict)) return true;
        }
        return false;
    }
    if (a.type == XML_XPATH_NODESET || b.type == XML_XPATH_NODESET)
    {
        XpNodeSet ns = a.type == XML_XPATH_NODESET ? a.ns : b.ns;
        XpValue val = a.type == XML_XPATH_NODESET ? b : a;
        if (a.type != XML_XPATH_NODESET) inf = !inf;
        if (val.type == XML_XPATH_BOOLEAN)
        {
            return xp_compare_numbers(ns.len == 0 ? 0 : 1, val.b ? 1 : 0, inf, strict);
        }
        double f = xp_to_number(c, val);
        for (size_t i = 0; i < ns.len; ++i)
        {
            double v = xml_xpath_string_to_number(xp_string_value(c, ns.nodes[i]));
            if (xp_compare_numbers(v, f, inf, strict)) return true;
        }
        return false;
    }
    return xp_compare_numbers(xp_to_number(c, a), xp_to_number(c, b), inf, strict);
}

// Functions //////////////////////////////////////////////////////

static void xp_arity(XpCtx *c, size_t have, size_t want)
{
    if (have != want) xp_error(c, XML_XPATH_ERR_INVALID_ARITY);
}

static XpNodeSet xp_nodeset_arg(XpCtx *c, XpValue v)
{
    if (v.type != XML_XPATH_NODESET) xp_error(c, XML_XPATH_ERR_INVALID_TYPE);
    return v.ns;
}

static const char *xp_local_name(XMQNode *n)
{
    switch (xml_node_kind(n))
    {
    case XML_DOM_ELEMENT: case XML_DOM_ATTRIBUTE: case XML_DOM_PI: case XML_DOM_NAMESPACE:
        return xml_node_name(n) ? xml_node_name(n) : "";
    default:
        return "";
    }
}

static const char *xp_namespace_uri(XMQNode *n)
{
    XmlNodeKind k = xml_node_kind(n);
    if (k != XML_DOM_ELEMENT && k != XML_DOM_ATTRIBUTE) return "";
    XmlNs *ns = xml_node_ns(n);
    if (!ns || !xml_ns_href(ns)) return "";
    return xml_ns_href(ns);
}

static const char *xp_qname(XpCtx *c, XMQNode *n)
{
    XmlNodeKind k = xml_node_kind(n);
    if (k != XML_DOM_ELEMENT && k != XML_DOM_ATTRIBUTE) return xp_local_name(n);
    const char *nm = xml_node_name(n) ? xml_node_name(n) : "";
    XmlNs *ns = xml_node_ns(n);
    if (!ns || !xml_ns_prefix(ns)) return nm;
    XpStr s = { NULL, 0, 0 };
    xp_str_add(c, &s, xml_ns_prefix(ns), strlen(xml_ns_prefix(ns)));
    xp_str_add(c, &s, ":", 1);
    xp_str_add(c, &s, nm, strlen(nm));
    return xp_str_get(&s);
}

static const char *xp_attr_qname(XpCtx *c, XMQNode *a)
{
    return xp_qname(c, a);
}

/** Is attribute a of element e an id, like the java XPathContext.isId. */
static bool xp_is_id(XpCtx *c, XMQNode *e, XMQNode *a, HashMap *dtd_ids)
{
    XmlNs *ns = xml_node_ns(a);
    if (ns && xml_ns_prefix(ns) && xp_streq(xml_node_name(a), "id") && !strcmp(xml_ns_prefix(ns), "xml")) return true;
    if (c->settings->html_ids)
    {
        return xp_streq(xml_node_name(a), "id") || (xp_streq(xml_node_name(a), "name") && xp_streq(xml_node_name(e), "a"));
    }
    if (!dtd_ids) return false;
    XpStr key = { NULL, 0, 0 };
    const char *eq = xp_qname(c, e);
    const char *aq = xp_attr_qname(c, a);
    xp_str_add(c, &key, eq, strlen(eq));
    xp_str_add(c, &key, "\1", 1);
    xp_str_add(c, &key, aq, strlen(aq));
    return hashmap_get(dtd_ids, xp_str_get(&key)) != NULL;
}

/** The ID attributes declared in the dtd: "element\1attribute" -> 1. */
static HashMap *xp_dtd_ids(XpCtx *c)
{
    XMQNode *dtd = c->doc ? xml_dom_dtd(xml_node_dom(c->doc)) : NULL;
    if (!dtd) return NULL;
    HashMap *m = NULL;
    for (XMQNode *d = xml_node_first_child(dtd); d; d = xml_node_next(d))
    {
        if (xml_node_kind(d) != XML_DOM_ATTRIBUTE_DECL) continue;
        // The text is "<!ATTLIST element attribute TYPE ...".
        const char *t = xml_node_content(d);
        if (!t || strncmp(t, "<!ATTLIST ", 10)) continue;
        t += 10;
        const char *sp1 = strchr(t, ' ');
        if (!sp1) continue;
        const char *sp2 = strchr(sp1 + 1, ' ');
        if (!sp2) continue;
        if (strncmp(sp2 + 1, "ID", 2) || (sp2[3] != ' ' && sp2[3] != 0)) continue;
        XpStr key = { NULL, 0, 0 };
        xp_str_add(c, &key, t, sp1 - t);
        xp_str_add(c, &key, "\1", 1);
        xp_str_add(c, &key, sp1 + 1, sp2 - sp1 - 1);
        if (!m) m = hashmap_create(64);
        hashmap_put(m, xp_str_get(&key), (void*)1);
    }
    return m;
}

static XMQNode *xp_lookup_id(XpCtx *c, const char *id)
{
    if (!c->ids_built)
    {
        c->ids_built = true;
        c->ids = hashmap_create(1024);
        HashMap *dtd_ids = xp_dtd_ids(c);
        XMQNode *n = c->doc;
        while (n)
        {
            if (xml_node_kind(n) == XML_DOM_ELEMENT)
            {
                for (XMQAttr *a = xml_node_first_attr(n); a; a = xml_attr_next(a))
                {
                    if (xp_is_id(c, n, xml_attr_node(a), dtd_ids))
                    {
                        const char *v = xp_string_value(c, xml_attr_node(a));
                        if (!hashmap_get(c->ids, v)) hashmap_put(c->ids, v, n);
                    }
                }
            }
            XmlNodeKind k = xml_node_kind(n);
            if (xml_node_first_child(n) && k != XML_DOM_DTD && k != XML_DOM_ENTITY_REF)
            {
                n = xml_node_first_child(n);
                continue;
            }
            while (n && n != c->doc && !xml_node_next(n)) n = xml_node_parent(n);
            if (!n || n == c->doc) break;
            n = xml_node_next(n);
        }
        if (dtd_ids) hashmap_free(dtd_ids);
    }
    return (XMQNode*)hashmap_get(c->ids, id);
}

static void xp_elements_by_ids(XpCtx *c, const char *ids, XpNodeSet *ret)
{
    size_t n = strlen(ids);
    size_t cur = 0, start = 0;
    while (cur < n && xp_is_blank(ids[cur])) cur++;
    start = cur;
    while (cur < n)
    {
        while (cur < n && !xp_is_blank(ids[cur])) cur++;
        XMQNode *e = xp_lookup_id(c, xp_strndup(c, ids + start, cur - start));
        if (e)
        {
            bool dup = false;
            for (size_t k = 0; k < ret->len; ++k) if (ret->nodes[k] == e) dup = true;
            if (!dup) xp_ns_add(c, ret, e);
        }
        while (cur < n && xp_is_blank(ids[cur])) cur++;
        start = cur;
    }
}

static XpValue xp_fn_substring(XpCtx *c, XpValue *a, size_t n)
{
    double le = 0;
    if (n == 3) le = xp_to_number(c, a[2]);
    double in = xp_to_number(c, a[1]);
    const char *s = xp_to_string(c, a[0]);
    long i = 1;
    long j = LONG_MAX;
    if (!(in < INT_MAX))
    {
        i = INT_MAX;
    }
    else if (in >= 1.0)
    {
        i = (int)in;
        if (in - floor(in) >= 0.5) i += 1;
    }
    if (n == 3)
    {
        double rin = floor(in);
        if (in - rin >= 0.5) rin += 1.0;
        double rle = floor(le);
        if (le - rle >= 0.5) rle += 1.0;
        double end = rin + rle;
        if (!(end >= 1.0)) j = 1;
        else if (end < INT_MAX) j = (int)end;
    }
    if (i >= j) return xp_string("");
    size_t cps = xp_utf8_count(s);
    size_t from = (size_t)(i - 1);
    if (from > cps) return xp_string("");
    size_t to = (j == LONG_MAX) ? cps : (size_t)(j - 1);
    if (to > cps) to = cps;
    size_t b = xp_utf8_offset(s, from);
    size_t e = xp_utf8_offset(s, to);
    return xp_string(xp_strndup(c, s + b, e - b));
}

static const char *xp_normalize(XpCtx *c, const char *s)
{
    XpStr out = { NULL, 0, 0 };
    xp_str_add(c, &out, "", 0);
    while (*s && xp_is_blank(*s)) s++;
    bool blank = false;
    for (; *s; ++s)
    {
        if (xp_is_blank(*s))
        {
            blank = true;
        }
        else
        {
            if (blank)
            {
                xp_str_add(c, &out, " ", 1);
                blank = false;
            }
            xp_str_add(c, &out, s, 1);
        }
    }
    return xp_str_get(&out);
}

static const char *xp_translate(XpCtx *c, const char *s, const char *from, const char *to)
{
    XpStr out = { NULL, 0, 0 };
    xp_str_add(c, &out, "", 0);
    while (*s)
    {
        size_t l;
        int cp = xp_utf8(s, &l);
        // Find cp in from.
        long offset = -1;
        long k = 0;
        for (const char *f = from; *f; ++k)
        {
            size_t fl;
            int fc = xp_utf8(f, &fl);
            if (fc == cp)
            {
                offset = k;
                break;
            }
            f += fl;
        }
        if (offset >= 0)
        {
            const char *t = to;
            for (long m = 0; *t && m < offset; ++m)
            {
                size_t tl;
                xp_utf8(t, &tl);
                t += tl;
            }
            if (*t)
            {
                size_t tl;
                xp_utf8(t, &tl);
                xp_str_add(c, &out, t, tl);
            }
        }
        else
        {
            xp_str_add(c, &out, s, l);
        }
        s += l;
    }
    return xp_str_get(&out);
}

static int xp_upper(int ch)
{
    return (ch >= 'a' && ch <= 'z') ? ch - 32 : ch;
}

static const char *xp_xml_lang(XpCtx *c, XMQNode *e)
{
    for (XMQAttr *a = xml_node_first_attr(e); a; a = xml_attr_next(a))
    {
        XMQNode *an = xml_attr_node(a);
        XmlNs *ns = xml_node_ns(an);
        if (xp_streq(xml_node_name(an), "lang") && ns && xp_streq(xml_ns_href(ns), XP_XML_NAMESPACE)) return xp_string_value(c, an);
        if (!ns && xp_streq(xml_node_name(an), "xml:lang")) return xp_string_value(c, an);
    }
    return NULL;
}

static bool xp_lang(XpCtx *c, const char *lang, XMQNode *node)
{
    const char *the_lang = NULL;
    if (node && xml_node_kind(node) != XML_DOM_NAMESPACE)
    {
        for (XMQNode *cur = node; cur; cur = xml_node_parent(cur))
        {
            if (xml_node_kind(cur) == XML_DOM_ELEMENT)
            {
                const char *v = xp_xml_lang(c, cur);
                if (v)
                {
                    the_lang = v;
                    break;
                }
            }
        }
    }
    if (!the_lang) return false;
    size_t i;
    for (i = 0; lang[i]; ++i)
    {
        if (!the_lang[i]) return false;
        if (xp_upper((unsigned char)lang[i]) != xp_upper((unsigned char)the_lang[i])) return false;
    }
    return the_lang[i] == 0 || the_lang[i] == '-';
}

static bool xp_is_hex(int ch)
{
    return (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f') || (ch >= '0' && ch <= '9');
}

static XpValue xp_escape_uri(XpCtx *c, XpValue *a, size_t n)
{
    xp_arity(c, n, 2);
    bool escape_reserved = xp_to_boolean(a[1]);
    const unsigned char *b = (const unsigned char*)xp_to_string(c, a[0]);
    XpStr out = { NULL, 0, 0 };
    xp_str_add(c, &out, "", 0);
    for (size_t i = 0; b[i]; ++i)
    {
        int ch = b[i];
        int c1 = b[i+1];
        int c2 = c1 ? b[i+2] : 0;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' ||
            ch == '(' || ch == ')' || (ch == '%' && xp_is_hex(c1) && xp_is_hex(c2)) ||
            (!escape_reserved && (ch == ';' || ch == '/' || ch == '?' || ch == ':' || ch == '@' || ch == '&' ||
                                  ch == '=' || ch == '+' || ch == '$' || ch == ',')))
        {
            char cc = (char)ch;
            xp_str_add(c, &out, &cc, 1);
        }
        else
        {
            char esc[4] = { '%', "0123456789ABCDEF"[ch >> 4], "0123456789ABCDEF"[ch & 0xF], 0 };
            xp_str_add(c, &out, esc, 3);
        }
    }
    return xp_string(xp_str_get(&out));
}

static XpValue xp_context_set(XpCtx *c)
{
    XpValue v = xp_nodeset();
    if (c->node) xp_ns_add(c, &v.ns, c->node);
    return v;
}

static XpValue xp_call(XpCtx *c, XpExpr *e)
{
    size_t n = e->num_args;
    XpValue *a = (XpValue*)xp_alloc(c, (n ? n : 1) * sizeof(XpValue));
    for (size_t i = 0; i < n; ++i) a[i] = xp_eval(c, e->args[i]);
    const char *name = e->name;
    if (e->prefix)
    {
        const char *uri = xp_lookup_ns(c, e->prefix);
        if (!uri) xp_error(c, XML_XPATH_ERR_UNDEF_PREFIX);
        if (!strcmp(uri, XP_XQUERY_FUNCTIONS_NS) && !strcmp(name, "escape-uri")) return xp_escape_uri(c, a, n);
        xp_error(c, XML_XPATH_ERR_UNKNOWN_FUNC);
    }
    if (!strcmp(name, "last"))
    {
        xp_arity(c, n, 0);
        if (c->size < 0) xp_error(c, XML_XPATH_ERR_INVALID_CTXT_SIZE);
        return xp_number((double)c->size);
    }
    if (!strcmp(name, "position"))
    {
        xp_arity(c, n, 0);
        if (c->position < 0) xp_error(c, XML_XPATH_ERR_INVALID_CTXT_POSITION);
        return xp_number((double)c->position);
    }
    if (!strcmp(name, "count"))
    {
        xp_arity(c, n, 1);
        return xp_number((double)xp_nodeset_arg(c, a[0]).len);
    }
    if (!strcmp(name, "id"))
    {
        xp_arity(c, n, 1);
        XpValue ret = xp_nodeset();
        if (a[0].type == XML_XPATH_NODESET)
        {
            for (size_t i = 0; i < a[0].ns.len; ++i) xp_elements_by_ids(c, xp_string_value(c, a[0].ns.nodes[i]), &ret.ns);
        }
        else
        {
            xp_elements_by_ids(c, xp_to_string(c, a[0]), &ret.ns);
        }
        return ret;
    }
    if (!strcmp(name, "local-name") || !strcmp(name, "namespace-uri") || !strcmp(name, "name"))
    {
        XpValue arg;
        if (n == 0) arg = xp_context_set(c);
        else
        {
            xp_arity(c, n, 1);
            arg = a[0];
        }
        XpNodeSet l = xp_nodeset_arg(c, arg);
        if (l.len == 0) return xp_string("");
        XMQNode *nd = l.nodes[0];
        if (!strcmp(name, "local-name")) return xp_string(xp_local_name(nd));
        if (!strcmp(name, "namespace-uri")) return xp_string(xp_namespace_uri(nd));
        return xp_string(xp_qname(c, nd));
    }
    if (!strcmp(name, "string"))
    {
        if (n == 0) return xp_string(c->node ? xp_string_value(c, c->node) : "");
        xp_arity(c, n, 1);
        return xp_string(xp_to_string(c, a[0]));
    }
    if (!strcmp(name, "string-length"))
    {
        const char *s;
        if (n == 0)
        {
            if (!c->node) return xp_number(0);
            s = xp_string_value(c, c->node);
        }
        else
        {
            xp_arity(c, n, 1);
            s = xp_to_string(c, a[0]);
        }
        return xp_number((double)xp_utf8_count(s));
    }
    if (!strcmp(name, "concat"))
    {
        if (n < 2) xp_error(c, XML_XPATH_ERR_INVALID_ARITY);
        XpStr s = { NULL, 0, 0 };
        xp_str_add(c, &s, "", 0);
        for (size_t i = 0; i < n; ++i)
        {
            const char *t = xp_to_string(c, a[i]);
            xp_str_add(c, &s, t, strlen(t));
        }
        return xp_string(xp_str_get(&s));
    }
    if (!strcmp(name, "contains"))
    {
        xp_arity(c, n, 2);
        return xp_boolean(strstr(xp_to_string(c, a[0]), xp_to_string(c, a[1])) != NULL);
    }
    if (!strcmp(name, "starts-with"))
    {
        xp_arity(c, n, 2);
        const char *s = xp_to_string(c, a[0]);
        const char *p = xp_to_string(c, a[1]);
        return xp_boolean(!strncmp(s, p, strlen(p)));
    }
    if (!strcmp(name, "substring"))
    {
        if (n < 2 || n > 3) xp_error(c, XML_XPATH_ERR_INVALID_ARITY);
        return xp_fn_substring(c, a, n);
    }
    if (!strcmp(name, "substring-before"))
    {
        xp_arity(c, n, 2);
        const char *s = xp_to_string(c, a[0]);
        const char *f = strstr(s, xp_to_string(c, a[1]));
        return xp_string(f ? xp_strndup(c, s, f - s) : "");
    }
    if (!strcmp(name, "substring-after"))
    {
        xp_arity(c, n, 2);
        const char *s = xp_to_string(c, a[0]);
        const char *t = xp_to_string(c, a[1]);
        const char *f = strstr(s, t);
        return xp_string(f ? f + strlen(t) : "");
    }
    if (!strcmp(name, "normalize-space"))
    {
        const char *s;
        if (n == 0) s = c->node ? xp_string_value(c, c->node) : "";
        else
        {
            xp_arity(c, n, 1);
            s = xp_to_string(c, a[0]);
        }
        return xp_string(xp_normalize(c, s));
    }
    if (!strcmp(name, "translate"))
    {
        xp_arity(c, n, 3);
        return xp_string(xp_translate(c, xp_to_string(c, a[0]), xp_to_string(c, a[1]), xp_to_string(c, a[2])));
    }
    if (!strcmp(name, "boolean"))
    {
        xp_arity(c, n, 1);
        return xp_boolean(xp_to_boolean(a[0]));
    }
    if (!strcmp(name, "not"))
    {
        xp_arity(c, n, 1);
        return xp_boolean(!xp_to_boolean(a[0]));
    }
    if (!strcmp(name, "true"))
    {
        xp_arity(c, n, 0);
        return xp_boolean(true);
    }
    if (!strcmp(name, "false"))
    {
        xp_arity(c, n, 0);
        return xp_boolean(false);
    }
    if (!strcmp(name, "lang"))
    {
        xp_arity(c, n, 1);
        return xp_boolean(xp_lang(c, xp_to_string(c, a[0]), c->node));
    }
    if (!strcmp(name, "number"))
    {
        if (n == 0)
        {
            if (!c->node) return xp_number(0);
            return xp_number(xml_xpath_string_to_number(xp_string_value(c, c->node)));
        }
        xp_arity(c, n, 1);
        return xp_number(xp_to_number(c, a[0]));
    }
    if (!strcmp(name, "sum"))
    {
        xp_arity(c, n, 1);
        XpNodeSet l = xp_nodeset_arg(c, a[0]);
        double res = 0.0;
        for (size_t i = 0; i < l.len; ++i) res += xml_xpath_string_to_number(xp_string_value(c, l.nodes[i]));
        return xp_number(res);
    }
    if (!strcmp(name, "floor"))
    {
        xp_arity(c, n, 1);
        return xp_number(floor(xp_to_number(c, a[0])));
    }
    if (!strcmp(name, "ceiling"))
    {
        xp_arity(c, n, 1);
        return xp_number(ceil(xp_to_number(c, a[0])));
    }
    if (!strcmp(name, "round"))
    {
        xp_arity(c, n, 1);
        double f = xp_to_number(c, a[0]);
        if (f >= -0.5 && f < 0.5) return xp_number(f * 0.0);
        double rounded = floor(f);
        if (f - rounded >= 0.5) rounded += 1.0;
        return xp_number(rounded);
    }
    xp_error(c, XML_XPATH_ERR_UNKNOWN_FUNC);
    return xp_boolean(false);
}

static XpValue xp_eval(XpCtx *c, XpExpr *e)
{
    switch (e->kind)
    {
    case XP_LITERAL: return xp_string(e->str);
    case XP_NUMBER: return xp_number(e->num);
    case XP_VAR:
        if (e->prefix && !xp_lookup_ns(c, e->prefix)) xp_error(c, XML_XPATH_ERR_UNDEF_PREFIX);
        xp_error(c, XML_XPATH_ERR_UNDEF_VARIABLE);
        break;
    case XP_OR:
        if (xp_to_boolean(xp_eval(c, e->l))) return xp_boolean(true);
        return xp_boolean(xp_to_boolean(xp_eval(c, e->r)));
    case XP_AND:
        if (!xp_to_boolean(xp_eval(c, e->l))) return xp_boolean(false);
        return xp_boolean(xp_to_boolean(xp_eval(c, e->r)));
    case XP_EQUAL:
    {
        XpValue a = xp_eval(c, e->l);
        XpValue b = xp_eval(c, e->r);
        return xp_boolean(xp_equal_values(c, a, b, e->op == 0));
    }
    case XP_COMPARE:
    {
        XpValue a = xp_eval(c, e->l);
        XpValue b = xp_eval(c, e->r);
        return xp_boolean(xp_compare_values(c, a, b, (e->op & 1) != 0, (e->op & 2) != 0));
    }
    case XP_ARITH:
    {
        XpValue a = xp_eval(c, e->l);
        XpValue b = xp_eval(c, e->r);
        double x = xp_to_number(c, a), y = xp_to_number(c, b);
        switch (e->op)
        {
        case XP_OP_SUB: return xp_number(x - y);
        case XP_OP_ADD: return xp_number(x + y);
        case XP_OP_MUL: return xp_number(x * y);
        case XP_OP_DIV: return xp_number(x / y);
        default: return xp_number(fmod(x, y));
        }
    }
    case XP_NEGATE:
    {
        double d = xp_to_number(c, xp_eval(c, e->l));
        return xp_number(e->op ? -d : d);
    }
    case XP_UNION:
    {
        XpValue a = xp_eval(c, e->l);
        XpValue b = xp_eval(c, e->r);
        if (a.type != XML_XPATH_NODESET || b.type != XML_XPATH_NODESET) xp_error(c, XML_XPATH_ERR_INVALID_TYPE);
        if (b.ns.len == 0) return a;
        XpValue out = xp_nodeset();
        XpPtrMap seen = { NULL, NULL, 0, 0 };
        for (size_t i = 0; i < a.ns.len; ++i)
        {
            xp_map_put(c, &seen, a.ns.nodes[i], 1);
            xp_ns_add(c, &out.ns, a.ns.nodes[i]);
        }
        for (size_t i = 0; i < b.ns.len; ++i)
        {
            if (xp_map_find(&seen, b.ns.nodes[i])) continue;
            xp_map_put(c, &seen, b.ns.nodes[i], 1);
            xp_ns_add(c, &out.ns, b.ns.nodes[i]);
        }
        return out;
    }
    case XP_SORT:
    {
        XpValue v = xp_eval(c, e->l);
        if (v.type == XML_XPATH_NODESET && v.ns.len > 1)
        {
            XpValue s = xp_nodeset();
            for (size_t i = 0; i < v.ns.len; ++i) xp_ns_add(c, &s.ns, v.ns.nodes[i]);
            xp_sort(c, &s.ns);
            return s;
        }
        return v;
    }
    case XP_FILTER:
    {
        XpValue v = xp_eval(c, e->l);
        if (v.type != XML_XPATH_NODESET) xp_error(c, XML_XPATH_ERR_INVALID_TYPE);
        if (v.ns.len == 0) return v;
        XpValue r = xp_nodeset();
        r.ns = xp_apply_predicate(c, v.ns, e->r);
        return r;
    }
    case XP_PATH:
        return xp_eval_path(c, e, false);
    case XP_CALL:
        return xp_call(c, e);
    }
    return xp_boolean(false);
}

// Streaming, port of libxml2 pattern.c as used by xmlXPathTryStreamCompile ////

enum { XS_OP_ROOT = 1, XS_OP_ELEM = 2, XS_OP_PARENT = 5, XS_OP_ANCESTOR = 6, XS_OP_ALL = 8 };
#define XS_PAT_FROM_ROOT (1 << 8)
#define XS_PAT_FROM_CUR (1 << 9)
#define XS_STEP_DESC 1
#define XS_STEP_FINAL 2
#define XS_STEP_ROOT 4
#define XS_STEP_NODE 16
#define XS_STEP_IN_SET 32
#define XS_FINAL_IS_ANY_NODE (1 << 14)
#define XS_FROM_ROOT (1 << 15)
#define XS_DESC (1 << 16)
enum { XS_ELEMENT = 1, XS_TEXT = 3, XS_CDATA = 4, XS_PI = 7, XS_COMMENT = 8, XS_ANY = 100 };

typedef struct
{
    int flags;
    const char *name;
    int node_type;
} XsStep;

typedef struct
{
    int pattern_flags;
    XsStep *steps;
    size_t num_steps;
    int flags;
} XsComp;

typedef struct
{
    XsComp *comp;
    int *states;
    size_t states_cap; // In ints.
    long nb_state;
    long level;
    long block_level;
} XsCtxt;

typedef struct
{
    int op;
    const char *value;
} XsOp;

static void xs_add_state(XpCtx *c, XsCtxt *s, int idx, long lvl)
{
    for (long i = 0; i < s->nb_state; ++i)
    {
        if (s->states[2*i] < 0)
        {
            s->states[2*i] = idx;
            s->states[2*i+1] = (int)lvl;
            return;
        }
    }
    if ((size_t)(2 * s->nb_state + 2) > s->states_cap)
    {
        size_t nc = s->states_cap ? s->states_cap * 2 : 16;
        int *ns = (int*)xp_alloc(c, nc * sizeof(int));
        if (s->nb_state) memcpy(ns, s->states, 2 * s->nb_state * sizeof(int));
        s->states = ns;
        s->states_cap = nc;
    }
    s->states[2*s->nb_state] = idx;
    s->states[2*s->nb_state+1] = (int)lvl;
    s->nb_state++;
}

typedef struct
{
    XpCtx *c;
    const char *s;
    size_t pos;
    XsOp *ops;
    size_t num_ops;
} XsParse;

static int xs_cur(XsParse *p) { return (unsigned char)p->s[p->pos]; }
static int xs_nxt(XsParse *p, size_t i)
{
    for (size_t k = 0; k < i; ++k) if (!p->s[p->pos + k]) return 0;
    return (unsigned char)p->s[p->pos + i];
}
static void xs_next(XsParse *p) { if (p->s[p->pos]) p->pos++; }
static void xs_skip_blanks(XsParse *p) { while (xp_is_blank(xs_cur(p))) p->pos++; }

static void xs_push(XsParse *p, int op, const char *value)
{
    XsOp *n = (XsOp*)xp_alloc(p->c, (p->num_ops + 1) * sizeof(XsOp));
    if (p->num_ops) memcpy(n, p->ops, p->num_ops * sizeof(XsOp));
    n[p->num_ops].op = op;
    n[p->num_ops].value = value;
    p->ops = n;
    p->num_ops++;
}

static const char *xs_scan_ncname(XsParse *p)
{
    xs_skip_blanks(p);
    size_t st = p->pos, l;
    int ch = xp_utf8(p->s + p->pos, &l);
    if (!xml_char_is_letter(ch) && ch != '_') return NULL;
    while (ch != 0 && xp_is_name_char(ch))
    {
        p->pos += l;
        ch = xp_utf8(p->s + p->pos, &l);
    }
    return xp_strndup(p->c, p->s + st, p->pos - st);
}

static bool xs_compile_step_pattern(XsParse *p)
{
    xs_skip_blanks(p);
    if (xs_cur(p) == '.')
    {
        xs_next(p);
        xs_push(p, XS_OP_ELEM, NULL);
        return true;
    }
    const char *name = xs_scan_ncname(p);
    if (!name)
    {
        if (xs_cur(p) == '*')
        {
            xs_next(p);
            xs_push(p, XS_OP_ALL, NULL);
            return true;
        }
        return false;
    }
    if (xp_is_blank(xs_cur(p))) xs_skip_blanks(p);
    if (xs_cur(p) == '*') return false;
    xs_push(p, XS_OP_ELEM, name);
    return true;
}

static bool xs_compile_path_pattern(XsParse *p, XsComp *comp)
{
    xs_skip_blanks(p);
    if (xs_cur(p) == '/') comp->pattern_flags |= XS_PAT_FROM_ROOT;
    else comp->pattern_flags |= XS_PAT_FROM_CUR;
    if (xs_cur(p) == '/' && xs_nxt(p, 1) == '/')
    {
        xs_push(p, XS_OP_ANCESTOR, NULL);
        xs_next(p);
        xs_next(p);
    }
    else if (xs_cur(p) == '.' && xs_nxt(p, 1) == '/' && xs_nxt(p, 2) == '/')
    {
        xs_push(p, XS_OP_ANCESTOR, NULL);
        xs_next(p);
        xs_next(p);
        xs_next(p);
        xs_skip_blanks(p);
        if (xs_cur(p) == 0) return false;
    }
    if (xs_cur(p) == '/')
    {
        xs_push(p, XS_OP_ROOT, NULL);
        xs_next(p);
        xs_skip_blanks(p);
        if (xs_cur(p) == 0) return false;
    }
    if (!xs_compile_step_pattern(p)) return false;
    xs_skip_blanks(p);
    while (xs_cur(p) == '/')
    {
        if (xs_nxt(p, 1) == '/')
        {
            xs_push(p, XS_OP_ANCESTOR, NULL);
            xs_next(p);
            xs_next(p);
            xs_skip_blanks(p);
            if (!xs_compile_step_pattern(p)) return false;
        }
        else
        {
            xs_push(p, XS_OP_PARENT, NULL);
            xs_next(p);
            xs_skip_blanks(p);
            if (xs_cur(p) == 0) return false;
            if (!xs_compile_step_pattern(p)) return false;
        }
    }
    return xs_cur(p) == 0;
}

static void xs_add_step(XpCtx *c, XsComp *comp, const char *name, int node_type, int flags)
{
    XsStep *n = (XsStep*)xp_alloc(c, (comp->num_steps + 1) * sizeof(XsStep));
    if (comp->num_steps) memcpy(n, comp->steps, comp->num_steps * sizeof(XsStep));
    n[comp->num_steps].name = name;
    n[comp->num_steps].node_type = node_type;
    n[comp->num_steps].flags = flags;
    comp->steps = n;
    comp->num_steps++;
}

static bool xs_stream_compile(XpCtx *c, XsComp *comp, XsOp *ops, size_t nb)
{
    if (nb == 0) return false;
    if (nb == 1 && ops[0].op == XS_OP_ELEM && ops[0].value == NULL)
    {
        comp->flags |= XS_FINAL_IS_ANY_NODE;
        return true;
    }
    if (comp->pattern_flags & XS_PAT_FROM_ROOT) comp->flags |= XS_FROM_ROOT;
    long s = 0;
    bool root = false;
    int flags = 0;
    long prevs = -1;
    for (size_t i = 0; i < nb; ++i)
    {
        int op = ops[i].op;
        const char *value = ops[i].value;
        switch (op)
        {
        case XS_OP_ROOT:
            if (i != 0) return false;
            root = true;
            break;
        case XS_OP_ELEM:
            if (!value)
            {
                if (nb == i + 1 && (flags & XS_STEP_DESC))
                {
                    comp->flags |= XS_FINAL_IS_ANY_NODE;
                    flags |= XS_STEP_NODE;
                    xs_add_step(c, comp, NULL, XS_ANY, flags);
                    s = (long)comp->num_steps - 1;
                    flags = 0;
                    if (prevs != -1)
                    {
                        comp->steps[prevs].flags |= XS_STEP_IN_SET;
                        prevs = -1;
                    }
                    break;
                }
                continue;
            }
            xs_add_step(c, comp, value, XS_ELEMENT, flags);
            s = (long)comp->num_steps - 1;
            prevs = s;
            flags = 0;
            break;
        case XS_OP_ALL:
            xs_add_step(c, comp, NULL, XS_ELEMENT, flags);
            s = (long)comp->num_steps - 1;
            prevs = s;
            flags = 0;
            break;
        case XS_OP_PARENT:
            break;
        case XS_OP_ANCESTOR:
            if (flags & XS_STEP_DESC) break;
            flags |= XS_STEP_DESC;
            comp->flags |= XS_DESC;
            break;
        default:
            return false;
        }
    }
    if ((long)comp->num_steps <= s) return false;
    comp->steps[s].flags |= XS_STEP_FINAL;
    if (root) comp->steps[0].flags |= XS_STEP_ROOT;
    return true;
}

/** Try to compile expr as a streamable pattern like xmlXPathTryStreamCompile. */
static bool xs_try_compile(XpCtx *c, const char *expr, XsComp **comps, size_t *num)
{
    if (strchr(expr, '[') || strchr(expr, '(') || strchr(expr, '@')) return false;
    // xmq does not register namespaces in the libxml2 xpath context (nsNr is 0),
    // so any colon prevents streaming.
    if (strchr(expr, ':')) return false;
    *comps = NULL;
    *num = 0;
    int type = 0;
    bool streamable = true;
    size_t start = 0, n = strlen(expr), orp = 0;
    while (orp < n)
    {
        while (orp < n && expr[orp] != '|') orp++;
        const char *alt = xp_strndup(c, expr + start, orp - start);
        if (orp < n) orp++;
        XsComp *cur = (XsComp*)xp_calloc(c, sizeof(XsComp));
        XsParse p = { c, alt, 0, NULL, 0 };
        if (!xs_compile_path_pattern(&p, cur)) return false;
        if (streamable)
        {
            if (type == 0) type = cur->pattern_flags & (XS_PAT_FROM_ROOT | XS_PAT_FROM_CUR);
            else if (type == XS_PAT_FROM_ROOT) { if (cur->pattern_flags & XS_PAT_FROM_CUR) streamable = false; }
            else if (type == XS_PAT_FROM_CUR) { if (cur->pattern_flags & XS_PAT_FROM_ROOT) streamable = false; }
        }
        if (streamable && !xs_stream_compile(c, cur, p.ops, p.num_ops)) return false;
        XsComp **nc = (XsComp**)xp_alloc(c, (*num + 1) * sizeof(XsComp*));
        if (*num) memcpy(nc, *comps, *num * sizeof(XsComp*));
        nc[(*num)++] = cur;
        *comps = (XsComp*)nc; // Array of pointers, see xs_eval.
        start = orp;
    }
    return streamable && *num > 0;
}

static bool xs_step_matches(XsStep *st, const char *name, const char *ns)
{
    if (st->node_type == XS_ANY) return true;
    if (!st->name) return true; // Patterns without prefixes have no namespace.
    return ns == NULL && name && !strcmp(st->name, name);
}

static int xs_push_node(XpCtx *c, XsCtxt *streams, size_t ns_count, const char *name, const char *ns, int node_type)
{
    int ret = 0;
    for (size_t si = 0; si < ns_count; ++si)
    {
        XsCtxt *stream = &streams[si];
        XsComp *comp = stream->comp;
        long nb_step = (long)comp->num_steps;
        if (node_type == XS_ELEMENT && !name && !ns)
        {
            // A document node (or a reset).
            stream->nb_state = 0;
            stream->level = 0;
            stream->block_level = -1;
            if (comp->flags & XS_FROM_ROOT)
            {
                if (nb_step == 0) ret = 1;
                else if (nb_step == 1 && comp->steps[0].node_type == XS_ANY && (comp->steps[0].flags & XS_STEP_DESC)) ret = 1;
                else if (comp->steps[0].flags & XS_STEP_ROOT) xs_add_state(c, stream, 0, 0);
            }
            continue;
        }
        if (nb_step == 0) continue;
        if (stream->block_level != -1)
        {
            stream->level++;
            continue;
        }
        if (node_type != XS_ELEMENT && !(comp->flags & XS_FINAL_IS_ANY_NODE))
        {
            stream->level++;
            continue;
        }
        long i = 0, m = stream->nb_state;
        bool match = false;
        int fin = 0;
        while (i < m)
        {
            int step_nr;
            if (!(comp->flags & XS_DESC))
            {
                step_nr = stream->states[2 * (stream->nb_state - 1)];
                if (stream->states[2 * (stream->nb_state - 1) + 1] < stream->level) return -1;
                i = m;
            }
            else
            {
                step_nr = stream->states[2 * i];
                if (step_nr < 0) { i++; continue; }
                long tmp = stream->states[2 * i + 1];
                if (tmp > stream->level) { i++; continue; }
                int desc = comp->steps[step_nr].flags & XS_STEP_DESC;
                if (tmp < stream->level && !desc) { i++; continue; }
            }
            XsStep *step = &comp->steps[step_nr];
            if (step->node_type != node_type && step->node_type != XS_ANY) { i++; continue; }
            match = xs_step_matches(step, name, ns);
            if (match)
            {
                fin = step->flags & XS_STEP_FINAL;
                if (fin) ret = 1;
                else xs_add_state(c, stream, step_nr + 1, stream->level + 1);
                if (ret != 1 && (step->flags & XS_STEP_IN_SET)) ret = 1;
            }
            if (!(comp->flags & XS_DESC) && (!match || fin)) stream->block_level = stream->level + 1;
            i++;
        }
        stream->level++;
        XsStep *step = &comp->steps[0];
        if (step->flags & XS_STEP_ROOT) continue;
        int desc = step->flags & XS_STEP_DESC;
        // Xpath streams are NOTPATTERN streams.
        if (stream->level != 1 && !desc) continue;
        if (step->node_type != node_type && step->node_type != XS_ANY) continue;
        match = xs_step_matches(step, name, ns);
        fin = step->flags & XS_STEP_FINAL;
        if (match)
        {
            if (fin) ret = 1;
            else xs_add_state(c, stream, 1, stream->level);
            if (ret != 1 && (step->flags & XS_STEP_IN_SET)) ret = 1;
        }
        if (!(comp->flags & XS_DESC) && (!match || fin)) stream->block_level = stream->level;
    }
    return ret;
}

static void xs_pop(XsCtxt *streams, size_t ns_count)
{
    for (size_t si = 0; si < ns_count; ++si)
    {
        XsCtxt *stream = &streams[si];
        if (stream->block_level == stream->level) stream->block_level = -1;
        if (stream->level) stream->level--;
        for (long i = stream->nb_state - 1; i >= 0; --i)
        {
            long lev = stream->states[2 * i + 1];
            if (lev > stream->level) stream->nb_state--;
            if (lev <= stream->level) break;
        }
    }
}

static int xs_type_number(XMQNode *n)
{
    switch (xml_node_kind(n))
    {
    case XML_DOM_ELEMENT: return XS_ELEMENT;
    case XML_DOM_TEXT: return XS_TEXT;
    case XML_DOM_CDATA: return XS_CDATA;
    case XML_DOM_COMMENT: return XS_COMMENT;
    case XML_DOM_PI: return XS_PI;
    default: return -1;
    }
}

static bool xs_skipped(XMQNode *n)
{
    return xml_node_kind(n) == XML_DOM_ENTITY_DECL || xml_node_kind(n) == XML_DOM_DTD;
}

static XMQNode *xs_children(XMQNode *n)
{
    return xp_fc(n);
}

static XpNodeSet xs_eval(XpCtx *c, XsComp **comps, size_t ncomps, XMQNode *context, XMQNode *doc)
{
    XpNodeSet result = { NULL, 0, 0 };
    long max_depth = 0, min_depth = 12345678;
    bool from_root = false, eval_all_nodes = false;
    for (size_t k = 0; k < ncomps; ++k)
    {
        XsComp *cm = comps[k];
        for (size_t s = 0; s < cm->num_steps; ++s) if (cm->steps[s].flags & XS_STEP_DESC) max_depth = -2;
        if (max_depth != -2 && (long)cm->num_steps > max_depth) max_depth = (long)cm->num_steps;
        if ((long)cm->num_steps < min_depth) min_depth = (long)cm->num_steps;
        if (cm->pattern_flags & XS_PAT_FROM_ROOT) from_root = true;
        if (cm->flags & XS_FINAL_IS_ANY_NODE) eval_all_nodes = true;
    }
    if (max_depth == -2) max_depth = 10000;
    if (min_depth == 0)
    {
        if (from_root)
        {
            if (doc) xp_ns_add(c, &result, doc);
        }
        else if (context)
        {
            xp_ns_add(c, &result, context);
        }
    }
    if (max_depth == 0) return result;
    XMQNode *cur = NULL, *limit = NULL;
    if (from_root)
    {
        cur = doc;
    }
    else if (context)
    {
        XmlNodeKind k = xml_node_kind(context);
        if (k == XML_DOM_ELEMENT || k == XML_DOM_DOCUMENT) cur = context;
        limit = cur;
    }
    if (!cur) return result;
    XsCtxt *streams = (XsCtxt*)xp_calloc(c, ncomps * sizeof(XsCtxt));
    for (size_t k = 0; k < ncomps; ++k)
    {
        streams[k].comp = comps[k];
        streams[k].block_level = -1;
    }
    if (from_root)
    {
        if (xs_push_node(c, streams, ncomps, NULL, NULL, XS_ELEMENT) == 1) xp_ns_add(c, &result, cur);
    }
    long depth = 0;
    bool scan_children = true;
    while (true)
    {
        if (!scan_children)
        {
            // next_node:
            int t = xs_type_number(cur);
            bool handled = false;
            if (t == XS_ELEMENT || ((t == XS_TEXT || t == XS_CDATA || t == XS_COMMENT || t == XS_PI) && eval_all_nodes))
            {
                int ret;
                if (t == XS_ELEMENT) ret = xs_push_node(c, streams, ncomps, xml_node_name(cur), xml_node_ns_uri(cur), XS_ELEMENT);
                else ret = xs_push_node(c, streams, ncomps, NULL, NULL, t);
                if (ret == 1) xp_ns_add(c, &result, cur);
                if (!xs_children(cur) || depth >= max_depth)
                {
                    xs_pop(streams, ncomps);
                    while (xml_node_next(cur))
                    {
                        cur = xml_node_next(cur);
                        if (!xs_skipped(cur))
                        {
                            handled = true;
                            break;
                        }
                    }
                }
            }
            if (handled) continue; // goto next_node
        }
        scan_children = false;
        // scan_children:
        if (xml_node_kind(cur) == XML_DOM_NAMESPACE) break;
        XMQNode *ch = xs_children(cur);
        if (ch && depth < max_depth)
        {
            if (xml_node_kind(ch) != XML_DOM_ENTITY_DECL)
            {
                cur = ch;
                depth++;
                if (xml_node_kind(cur) != XML_DOM_DTD)
                {
                    if (depth < 0) break;
                    continue;
                }
            }
        }
        if (cur == limit) break;
        bool found = false;
        while (xml_node_next(cur))
        {
            cur = xml_node_next(cur);
            if (!xs_skipped(cur))
            {
                found = true;
                break;
            }
        }
        if (found) continue;
        bool done = false;
        do
        {
            cur = xml_node_parent(cur);
            depth--;
            if (!cur || cur == limit || xml_node_kind(cur) == XML_DOM_DOCUMENT)
            {
                done = true;
                break;
            }
            if (xml_node_kind(cur) == XML_DOM_ELEMENT) xs_pop(streams, ncomps);
            else if (eval_all_nodes && xs_type_number(cur) > 1) xs_pop(streams, ncomps);
            if (xml_node_next(cur))
            {
                cur = xml_node_next(cur);
                break;
            }
        } while (cur);
        if (done || !cur || depth < 0) break;
    }
    return result;
}

// Entry //////////////////////////////////////////////////////////

void xml_xpath_settings_default(XmlXPathSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->max_depth = 1000;
}

static void xp_cleanup(XpCtx *c)
{
    XpBlock *b = c->blocks;
    while (b)
    {
        XpBlock *next = b->next;
        free(b);
        b = next;
    }
    c->blocks = NULL;
    if (c->ids) hashmap_free(c->ids);
    c->ids = NULL;
}

XmlXPathResult *xml_xpath_eval(const char *expr, XMQNode *context, const XmlXPathSettings *settings, XmlXPathError *error)
{
    XmlXPathSettings defaults;
    if (!settings)
    {
        xml_xpath_settings_default(&defaults);
        settings = &defaults;
    }
    if (error)
    {
        error->code = XML_XPATH_ERR_OK;
        error->message = xp_messages_[0];
        error->offset = 0;
    }
    XpCtx *c = (XpCtx*)calloc(1, sizeof(XpCtx));
    c->settings = settings;
    c->expr = expr ? expr : "";
    c->node = settings->no_context_node ? NULL : context;
    c->doc = xp_document_of(context);
    c->size = -1;
    c->position = -1;

    if (setjmp(c->on_error))
    {
        if (error)
        {
            error->code = c->error;
            error->message = (c->error >= 0 && (size_t)c->error < sizeof(xp_messages_) / sizeof(xp_messages_[0]))
                             ? xp_messages_[c->error] : "Unknown error";
            error->offset = c->pos;
        }
        for (size_t i = 0; i < c->num_ns_nodes; ++i) xml_node_free(c->ns_nodes[i]);
        free(c->ns_nodes);
        xp_cleanup(c);
        free(c);
        return NULL;
    }

    if (!expr) xp_error(c, XML_XPATH_ERR_EXPR);

    XpValue v;
    XsComp *comps = NULL;
    size_t ncomps = 0;
    if (xs_try_compile(c, c->expr, &comps, &ncomps))
    {
        // Like libxml2, simple expressions are evaluated by the streaming pattern engine.
        v = xp_nodeset();
        v.ns = xs_eval(c, (XsComp**)comps, ncomps, c->node, c->doc);
    }
    else
    {
        c->pos = 0;
        XpExpr *e = xp_compile_expr(c, true);
        if (xp_cur(c) != 0) xp_error(c, XML_XPATH_ERR_EXPR);
        xp_optimize(e);
        v = xp_eval(c, e);
    }

    XmlXPathResult *r = (XmlXPathResult*)calloc(1, sizeof(XmlXPathResult));
    r->type = v.type;
    r->b = v.b;
    r->num = v.num;
    if (v.type == XML_XPATH_STRING)
    {
        size_t n = strlen(v.str);
        r->str = (char*)malloc(n + 1);
        memcpy(r->str, v.str, n + 1);
    }
    if (v.type == XML_XPATH_NODESET && v.ns.len)
    {
        r->nodes = (XMQNode**)malloc(v.ns.len * sizeof(XMQNode*));
        memcpy(r->nodes, v.ns.nodes, v.ns.len * sizeof(XMQNode*));
        r->len = v.ns.len;
    }
    r->ns_nodes = c->ns_nodes;
    r->num_ns_nodes = c->num_ns_nodes;
    xp_cleanup(c);
    free(c);
    return r;
}

XmlXPathType xml_xpath_result_type(const XmlXPathResult *r)
{
    return r->type;
}

size_t xml_xpath_result_size(const XmlXPathResult *r)
{
    return r->len;
}

XMQNode *xml_xpath_result_node(const XmlXPathResult *r, size_t i)
{
    return i < r->len ? r->nodes[i] : NULL;
}

bool xml_xpath_result_boolean(const XmlXPathResult *r)
{
    return r->b;
}

double xml_xpath_result_number(const XmlXPathResult *r)
{
    return r->num;
}

const char *xml_xpath_result_string(const XmlXPathResult *r)
{
    return r->str;
}

void xml_xpath_result_free(XmlXPathResult *r)
{
    if (!r) return;
    for (size_t i = 0; i < r->num_ns_nodes; ++i) xml_node_free(r->ns_nodes[i]);
    free(r->ns_nodes);
    free(r->nodes);
    free(r->str);
    free(r);
}

#endif // XML_XPATH_MODULE
