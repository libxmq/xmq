/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
   Differential test of the schema construction part of the native XML Schema 1.0
   structures (parts/xml_xsd10_build.c) against libxml2 xmlschemas.c.

   libxml2's static functions are reached by including 3rdparty/libxml2/xmlschemas.c
   into this file (its public functions then replace the ones of libxml2.a).
   The parts of the test that build components construct them identically on both
   sides (this file is included twice, once per side, see XSD10_BUILD_DIFF_SIDE).

   Compared with random inputs (-r count:seed):
   - the wildcard operations: xmlSchemaUnionWildcards, xmlSchemaIntersectWildcards,
     xmlSchemaCheckCOSNSSubset, xmlSchemaCloneWildcardNsConstraints and
     xmlSchemaCheckCVCWildcardNamespace (results, the resulting namespace constraints,
     the messages),
   - the content models: random particle trees (sequence, choice, all, elements,
     substitution groups, abstract elements, wildcards, random occurrences) are built
     with xmlSchemaBuildContentModel, the messages (determinism) must be the same and
     random element sequences are run through both automata (results of every push and
     the transition data of the callbacks), xmlSchemaIsParticleEmptiable and
     xmlSchemaGetParticleEmptiable,
   - the derivation checks on the built in types (xmlSchemaCheckCOSSTDerivedOK,
     xmlSchemaCheckCOSDerivedOK, xmlSchemaIsDerivedFromBuiltInType,
     xmlSchemaGetPrimitiveType, xmlSchemaTypeFinalContains),
   - xmlSchemaCheckFacet on facets of built in types (return value, messages, value,
     regexp and whitespace results),
   - xmlSchemaAreValuesEqual on values of built in types.
   And with -p files: xmlSchemaParse of every schema file (normally
   3rdparty/libxml2/test/schemas/ *.xsd): the same messages (structured fields and
   formatted text), the same success/failure and the same dump of the fixed up
   global components of the main schema.

   Built with -DXSD10_BUILD_DIFF_STUBS the parse and validation parts are replaced by
   stubs (to test the build part while they do not exist): the tests that need them
   (facets that validate values, -p) are skipped then.

   And with -m count:seed files: xmlSchemaParse (from memory) of count random mutations of
   the files (attribute values replaced or removed, elements renamed), compared like -p.

   Usage: xml_xsd10_build_diff [-v] [-r count:seed] [-t wcbfv] [-p] [-m count:seed] files...
   (-t selects the random tests: wildcards, content models, built in types, facets, values)
   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#ifndef XSD10_BUILD_DIFF_SIDE

#include"xmlschemas.c"

#define XML_XSD10_INTERNALS
#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_regexp.h"
#include"parts/xml_xsd10_types.h"
#include"parts/xml_xsd10_schemas.h"

#include<stdarg.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

const char *__lsan_default_suppressions(void);
const char *__lsan_default_suppressions(void)
{
    return "leak:xmlSchemaFormatItemForReport\nleak:xmlSchemaValAtomicType\n";
}

#ifdef XSD10_BUILD_DIFF_STUBS
/* Stand ins for the functions of the parse and validation parts that the build part
   calls. The two used by the tests here are ports, the others are never called. */
XmlXsd10WildcardNs *xml_xsd10_new_wildcard_ns_constraint(XmlXsd10ParserCtxt *ctxt)
{
    XmlXsd10WildcardNs *ret = (XmlXsd10WildcardNs *) malloc(sizeof(XmlXsd10WildcardNs));
    if (ret == NULL) { xml_xsd10_p_err_memory(ctxt); return NULL; }
    ret->value = NULL;
    ret->next = NULL;
    return ret;
}
XmlXsd10SubstGroup *xml_xsd10_subst_group_get(XmlXsd10ParserCtxt *pctxt, XmlXsd10Element *head)
{
    if (XSD10_WXS_SUBST_GROUPS(pctxt) == NULL) return NULL;
    return (XmlXsd10SubstGroup *) xml_xsd10_hash_lookup2(XSD10_WXS_SUBST_GROUPS(pctxt), head->name, head->targetNamespace);
}
#define XSD10_BUILD_DIFF_STUB(ret, name, args) ret name args { fprintf(stderr, "stub %s called\n", #name); abort(); }
XSD10_BUILD_DIFF_STUB(int, xml_xsd10_add_element_substitution_member, (XmlXsd10ParserCtxt *a, XmlXsd10Element *b, XmlXsd10Element *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10ModelGroup *, xml_xsd10_add_model_group, (XmlXsd10ParserCtxt *a, XmlXsd10Schema *b, XmlXsd10TypeType c, XMQNode *d))
XSD10_BUILD_DIFF_STUB(XmlXsd10Particle *, xml_xsd10_add_particle, (XmlXsd10ParserCtxt *a, XMQNode *b, int c, int d))
XSD10_BUILD_DIFF_STUB(int, xml_xsd10_add_schema_doc, (XmlXsd10ParserCtxt *a, int b, const char *c, XmlDom *d, const char *e, int f, XMQNode *g, const char *h, const char *i, XmlXsd10Bucket **j))
XSD10_BUILD_DIFF_STUB(XmlXsd10Type *, xml_xsd10_add_type, (XmlXsd10ParserCtxt *a, XmlXsd10Schema *b, XmlXsd10TypeType c, const char *d, const char *e, XMQNode *f, int g))
XSD10_BUILD_DIFF_STUB(XmlXsd10Wildcard *, xml_xsd10_add_wildcard, (XmlXsd10ParserCtxt *a, XmlXsd10Schema *b, XmlXsd10TypeType c, XMQNode *d))
XSD10_BUILD_DIFF_STUB(XmlXsd10BasicItem *, xml_xsd10_find_redef_comp_in_graph, (XmlXsd10Bucket *a, XmlXsd10TypeType b, const char *c, const char *d))
XSD10_BUILD_DIFF_STUB(void, xml_xsd10_free_valid_ctxt, (XmlXsd10ValidCtxt *a))
XSD10_BUILD_DIFF_STUB(XmlXsd10Attribute *, xml_xsd10_get_attribute_decl, (XmlXsd10Schema *a, const char *b, const char *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10AttributeGroup *, xml_xsd10_get_attribute_group, (XmlXsd10Schema *a, const char *b, const char *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10Element *, xml_xsd10_get_elem, (XmlXsd10Schema *a, const char *b, const char *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10IDC *, xml_xsd10_get_idc, (XmlXsd10Schema *a, const char *b, const char *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10BasicItem *, xml_xsd10_get_named_component, (XmlXsd10Schema *a, XmlXsd10TypeType b, const char *c, const char *d))
XSD10_BUILD_DIFF_STUB(XmlXsd10Type *, xml_xsd10_get_type, (XmlXsd10Schema *a, const char *b, const char *c))
XSD10_BUILD_DIFF_STUB(XmlXsd10ValidCtxt *, xml_xsd10_new_valid_ctxt, (XmlXsd10Schema *a))
XSD10_BUILD_DIFF_STUB(int, xml_xsd10_parse_new_doc_with_context, (XmlXsd10ParserCtxt *a, XmlXsd10Schema *b, XmlXsd10Bucket *c))
XSD10_BUILD_DIFF_STUB(int, xml_xsd10_v_check_cvc_simple_type, (XmlXsd10AbstractCtxt *a, XMQNode *b, XmlXsd10Type *c, const char *d, XmlXsd10Val **e, int f, int g, int h))
#undef XSD10_BUILD_DIFF_STUB
#endif

static int verbose_ = 0;
static long same_ = 0, diff_ = 0;
static int max_shown_ = 40;
static unsigned long long rng_ = 1;

static unsigned rnd(void)
{
    rng_ = rng_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_ >> 33);
}

static void check_str(const char *what, const char *l, const char *n)
{
    if ((l == NULL && n == NULL) || (l && n && !strcmp(l, n)))
    {
        same_++;
        return;
    }
    diff_++;
    if (diff_ <= max_shown_)
        printf("DIFF %s\n  libxml2: %s\n  native:  %s\n", what, l ? l : "(NULL)", n ? n : "(NULL)");
}

static void check_int(const char *what, long l, long n)
{
    char a[64], b[64];
    snprintf(a, sizeof(a), "%ld", l);
    snprintf(b, sizeof(b), "%ld", n);
    check_str(what, a, b);
}

// Capturing //////////////////////////////////////////////////////

typedef struct { char *s; size_t len, max; } Cap;
static Cap lcap_, ncap_;

static void cap_add(Cap *c, const char *text)
{
    size_t b = strlen(text);
    if (c->s == NULL) c->len = c->max = 0;
    if (c->len + b + 1 > c->max)
    {
        c->max = 2 * (c->len + b + 1);
        c->s = (char *) realloc(c->s, c->max);
    }
    memcpy(c->s + c->len, text, b + 1);
    c->len += b;
}

static void cap_printf(Cap *c, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cap_add(c, buf);
}

static void l_fmt(void *ctx, const char *msg, ...)
{
    char buf[70000];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);
    cap_add((Cap *) ctx, buf);
}

static void l_serr(void *ctx, const xmlError *e)
{
    Cap f = { NULL, 0, 0 };
    (void) ctx;
    xmlFormatError(e, l_fmt, &f);
    const xmlNode *node = (const xmlNode *) e->node;
    cap_printf(&lcap_, "[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%s] ",
               e->domain, e->code, (int) e->level, e->file ? e->file : "(NULL)", e->line,
               e->str1 ? e->str1 : "(NULL)", e->str2 ? e->str2 : "(NULL)", e->str3 ? e->str3 : "(NULL)",
               e->int1, e->int2, node ? (node->name ? (const char *) node->name : "(noname)") : "(NULL)");
    cap_add(&lcap_, e->message ? e->message : "(NULL)");
    cap_add(&lcap_, "|FORMATTED|");
    cap_add(&lcap_, f.s ? f.s : "");
    free(f.s);
}

static void n_serr(void *ctx, const XmlXsd10Error *e)
{
    char *f = xml_xsd10_format_error_string(e);
    const char *nn = NULL;
    (void) ctx;
    if (e->node) nn = xml_node_name(e->node);
    cap_printf(&ncap_, "[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%s] ",
               e->domain, e->code, (int) e->level, e->file ? e->file : "(NULL)", e->line,
               e->str1 ? e->str1 : "(NULL)", e->str2 ? e->str2 : "(NULL)", e->str3 ? e->str3 : "(NULL)",
               e->int1, e->int2, e->node ? (nn ? nn : "(noname)") : "(NULL)");
    cap_add(&ncap_, e->message ? e->message : "(NULL)");
    cap_add(&ncap_, "|FORMATTED|");
    cap_add(&ncap_, f ? f : "");
    free(f);
}

static void compare_caps(const char *what)
{
    if (verbose_ > 1 && lcap_.s) printf("%s: %s\n", what, lcap_.s);
    check_str(what, lcap_.s ? lcap_.s : "", ncap_.s ? ncap_.s : "");
    free(lcap_.s);
    free(ncap_.s);
    lcap_.s = ncap_.s = NULL;
}

// Random component descriptions, built identically on both sides //

#define NSPOOL 5 /* index 0 is the absent namespace (NULL) */
static const char *ns_pool_[NSPOOL] = { NULL, "urn:a", "urn:b", "urn:c", "urn:d" };
#define NAMEPOOL 5
static const char *name_pool_[NAMEPOOL] = { "a", "b", "c", "d", "e" };

/* A wildcard: ##any, a set of namespaces (bit i: ns_pool_[i]) or not(ns_pool_[neg]). */
typedef struct { int any; unsigned set; int neg; int pc; } WSpec;

static void gen_wspec(WSpec *w)
{
    memset(w, 0, sizeof(*w));
    w->neg = -1;
    w->pc = 1 + rnd() % 3;
    switch (rnd() % 4)
    {
    case 0: w->any = 1; break;
    case 1: w->neg = rnd() % NSPOOL; break;
    default:
        while (w->set == 0) w->set = rnd() & ((1u << NSPOOL) - 1);
        break;
    }
}

#define MAX_ELEMS 8
#define MAX_PARTS 64
#define MAX_CHILDREN 4
/* An element declaration. */
typedef struct { int name, ns, abstract, head, nmembers, members[3]; } ESpec;
/* A particle: kind 0 element, 1 wildcard, 2 sequence, 3 choice, 4 all. max -1 is unbounded. */
typedef struct { int kind, min, max, elem; WSpec wild; int n, child[MAX_CHILDREN]; } PSpec;

typedef struct
{
    int nelems;
    ESpec elems[MAX_ELEMS];
    int nparts;
    PSpec parts[MAX_PARTS];
    int root;
    int mixed;
} CMSpec;

static void gen_occurs(int *min, int *max)
{
    int r = rnd() % 10;
    if (r < 5) { *min = rnd() % 2; *max = 1; }
    else if (r < 8) { *min = rnd() % 3; *max = -1; }
    else { *min = rnd() % 4; *max = *min + rnd() % 3; if (*max < 1) *max = 1; }
}

static int gen_particle(CMSpec *s, int depth, int top)
{
    if (s->nparts >= MAX_PARTS - MAX_CHILDREN) depth = 0;
    int i = s->nparts++;
    PSpec *p = &s->parts[i];
    memset(p, 0, sizeof(*p));
    int r = rnd() % 10;
    if (depth <= 0) r = rnd() % 5;
    if (r < 3) { p->kind = 0; p->elem = rnd() % s->nelems; gen_occurs(&p->min, &p->max); }
    else if (r < 5) { p->kind = 1; gen_wspec(&p->wild); gen_occurs(&p->min, &p->max); }
    else if (r == 9 && top)
    {
        // xs:all: element children with maxOccurs 1.
        p->kind = 4;
        p->min = rnd() % 2;
        p->max = 1;
        p->n = rnd() % (MAX_CHILDREN + 1);
        for (int c = 0; c < p->n; c++)
        {
            int j = s->nparts++;
            PSpec *q = &s->parts[j];
            memset(q, 0, sizeof(*q));
            q->kind = 0;
            q->elem = rnd() % s->nelems;
            q->min = rnd() % 2;
            q->max = 1;
            s->parts[i].child[c] = j;
        }
        return i;
    }
    else
    {
        p->kind = (r < 7) ? 2 : 3;
        gen_occurs(&p->min, &p->max);
        int n = rnd() % (MAX_CHILDREN + 1);
        if (rnd() % 4) n = n ? n : 1;
        for (int c = 0; c < n; c++)
        {
            int j = gen_particle(s, depth - 1, 0);
            s->parts[i].child[s->parts[i].n++] = j;
        }
    }
    return i;
}

static void gen_cm(CMSpec *s)
{
    memset(s, 0, sizeof(*s));
    s->nelems = 1 + rnd() % MAX_ELEMS;
    for (int i = 0; i < s->nelems; i++)
    {
        ESpec *e = &s->elems[i];
        e->name = rnd() % NAMEPOOL;
        e->ns = rnd() % 3;
        e->abstract = (rnd() % 8) == 0;
    }
    // Substitution group heads with members (members are not heads).
    for (int i = 0; i < s->nelems; i++)
    {
        ESpec *e = &s->elems[i];
        if (rnd() % 5) continue;
        e->head = 1;
        e->nmembers = rnd() % 4;
        for (int m = 0; m < e->nmembers; m++) e->members[m] = rnd() % s->nelems;
    }
    for (int i = 0; i < s->nelems; i++)
        for (int m = 0; m < s->elems[i].nmembers; m++)
            if (s->elems[s->elems[i].members[m]].head) s->elems[i].members[m] = -1;
    s->root = gen_particle(s, 3, 1);
    s->mixed = (rnd() % 4) == 0;
}

/* The random element sequences pushed through the automata. */
#define MAX_PUSH 16
typedef struct { int n; int name[MAX_PUSH], ns[MAX_PUSH]; } PushSpec;

static void push_token(PushSpec *p, int name, int ns)
{
    if (p->n >= MAX_PUSH) return;
    p->name[p->n] = name;
    p->ns[p->n] = ns;
    p->n++;
}

/* An instance of the particle (valid unless the spec is not deterministic). */
static void gen_valid(PushSpec *p, const CMSpec *s, int idx)
{
    const PSpec *ps = &s->parts[idx];
    int max = ps->max < 0 ? ps->min + 3 : ps->max;
    int times = ps->min + (max > ps->min ? (int) (rnd() % (max - ps->min + 1)) : 0);
    for (int t = 0; t < times && p->n < MAX_PUSH; t++)
    {
        switch (ps->kind)
        {
        case 0:
        {
            const ESpec *e = &s->elems[ps->elem];
            if (e->head && e->nmembers && rnd() % 2)
            {
                int m = e->members[rnd() % e->nmembers];
                if (m >= 0) e = &s->elems[m];
            }
            push_token(p, e->name, e->ns);
            break;
        }
        case 1:
        {
            int ns = 0;
            for (int tries = 0; tries < 10; tries++)
            {
                ns = rnd() % NSPOOL;
                if (ps->wild.any) break;
                if (ps->wild.neg >= 0 && ns != ps->wild.neg && ns != 0) break;
                if (ps->wild.set & (1u << ns)) break;
            }
            push_token(p, rnd() % NAMEPOOL, ns);
            break;
        }
        case 2:
            for (int c = 0; c < ps->n; c++) gen_valid(p, s, ps->child[c]);
            break;
        case 3:
            if (ps->n) gen_valid(p, s, ps->child[rnd() % ps->n]);
            break;
        case 4:
        {
            int order[MAX_CHILDREN];
            for (int c = 0; c < ps->n; c++) order[c] = ps->child[c];
            for (int c = ps->n - 1; c > 0; c--)
            {
                int j = rnd() % (c + 1), x = order[c];
                order[c] = order[j];
                order[j] = x;
            }
            for (int c = 0; c < ps->n; c++) gen_valid(p, s, order[c]);
            break;
        }
        }
    }
}

static void gen_push(PushSpec *p, const CMSpec *s)
{
    if (rnd() % 3)
    {
        p->n = 0;
        gen_valid(p, s, s->root);
        // Sometimes break it a little.
        if (p->n && rnd() % 3 == 0)
        {
            int i = rnd() % p->n;
            switch (rnd() % 3)
            {
            case 0: memmove(&p->name[i], &p->name[i + 1], (p->n - i - 1) * sizeof(int));
                    memmove(&p->ns[i], &p->ns[i + 1], (p->n - i - 1) * sizeof(int)); p->n--; break;
            case 1: p->name[i] = rnd() % NAMEPOOL; break;
            default: p->ns[i] = rnd() % NSPOOL; break;
            }
        }
        return;
    }
    p->n = rnd() % (MAX_PUSH + 1);
    for (int i = 0; i < p->n; i++)
    {
        if (rnd() % 4)
        {
            // Mostly the declared elements.
            const ESpec *e = &s->elems[rnd() % s->nelems];
            p->name[i] = e->name;
            p->ns[i] = e->ns;
        }
        else
        {
            p->name[i] = rnd() % NAMEPOOL;
            p->ns[i] = rnd() % NSPOOL;
        }
    }
}

/* Facet test values. */
static const char *facet_values_[] = {
    "0", "1", "2", "5", "-1", "abc", "1.5", "preserve", "replace", "collapse", "[a-z]+", "(",
    "\\p{L}+", "a{2,3}", "007", "", " 3 ", "99999999999999999999", "+4", "true", "2001-01-01",
    "\\d{3}-\\d{4}", "[", "x|y", "1e3", "P1Y", "a b", "INF", "NaN"
};
#define NFACET_VALUES ((int) (sizeof(facet_values_) / sizeof(facet_values_[0])))

#ifdef XSD10_BUILD_DIFF_STUBS
#define NFACET_TYPES 7
#else
#define NFACET_TYPES 12
#endif
static const int facet_types_[12] = {
    XML_SCHEMA_FACET_LENGTH, XML_SCHEMA_FACET_MINLENGTH, XML_SCHEMA_FACET_MAXLENGTH,
    XML_SCHEMA_FACET_TOTALDIGITS, XML_SCHEMA_FACET_FRACTIONDIGITS, XML_SCHEMA_FACET_WHITESPACE,
    XML_SCHEMA_FACET_PATTERN,
    XML_SCHEMA_FACET_ENUMERATION, XML_SCHEMA_FACET_MININCLUSIVE, XML_SCHEMA_FACET_MAXINCLUSIVE,
    XML_SCHEMA_FACET_MINEXCLUSIVE, XML_SCHEMA_FACET_MAXEXCLUSIVE
};

// The sides //////////////////////////////////////////////////////

#define XSD10_BUILD_DIFF_SIDE 1
#include __FILE__
#undef XSD10_BUILD_DIFF_SIDE
#define XSD10_BUILD_DIFF_SIDE 2
#include __FILE__
#undef XSD10_BUILD_DIFF_SIDE

// Tests //////////////////////////////////////////////////////////

static void test_wildcards(int count)
{
    xmlSchemaParserCtxtPtr lc = l_new_ctxt();
    XmlXsd10ParserCtxt *nc = n_new_ctxt();
    char what[128];
    for (int i = 0; i < count; i++)
    {
        WSpec a, b;
        gen_wspec(&a);
        gen_wspec(&b);
        for (int op = 0; op < 5; op++)
        {
            char *ld = NULL, *nd = NULL;
            int lr = l_wild_op(lc, op, &a, &b, &ld);
            int nr = n_wild_op(nc, op, &a, &b, &nd);
            snprintf(what, sizeof(what), "wildcard op %d #%d", op, i);
            check_int(what, lr, nr);
            check_str(what, ld, nd);
            compare_caps(what);
            free(ld);
            free(nd);
        }
    }
    xmlSchemaFreeParserCtxt(lc);
    xml_xsd10_free_parser_ctxt(nc);
}

static void test_content_models(int count)
{
    char what[128];
    for (int i = 0; i < count; i++)
    {
        CMSpec s;
        PushSpec pushes[8];
        gen_cm(&s);
        for (int p = 0; p < 8; p++) gen_push(&pushes[p], &s);
        char *ld = l_content_model(&s, pushes, 8);
        char *nd = n_content_model(&s, pushes, 8);
        snprintf(what, sizeof(what), "content model #%d", i);
        check_str(what, ld, nd);
        compare_caps(what);
        if (verbose_ > 2) printf("%s: %s\n", what, ld);
        free(ld);
        free(nd);
    }
}

static void test_builtin_derivation(void)
{
    xmlSchemaParserCtxtPtr lc = l_new_ctxt();
    XmlXsd10ParserCtxt *nc = n_new_ctxt();
    char *ld = l_builtin_derivation(lc);
    char *nd = n_builtin_derivation(nc);
    check_str("built in derivation", ld, nd);
    compare_caps("built in derivation");
    free(ld);
    free(nd);
    xmlSchemaFreeParserCtxt(lc);
    xml_xsd10_free_parser_ctxt(nc);
}

static void test_facets(int count)
{
    xmlSchemaParserCtxtPtr lc = l_new_ctxt();
    XmlXsd10ParserCtxt *nc = n_new_ctxt();
    char what[128];
    for (int i = 0; i < count; i++)
    {
        int type = 1 + rnd() % XML_SCHEMAS_ANYSIMPLETYPE;
        int ft = facet_types_[rnd() % NFACET_TYPES];
        const char *value = facet_values_[rnd() % NFACET_VALUES];
        char *ld = l_facet(lc, type, ft, value);
        char *nd = n_facet(nc, type, ft, value);
        snprintf(what, sizeof(what), "facet %d on type %d value '%s'", ft, type, value);
        check_str(what, ld, nd);
        compare_caps(what);
        free(ld);
        free(nd);
    }
    xmlSchemaFreeParserCtxt(lc);
    xml_xsd10_free_parser_ctxt(nc);
}

static const char *equal_values_[] = {
    "1", "01", "1.0", "+1", "-0", "0", "abc", " abc ", "a b", "a  b", "1 2", "1 2 3", "true", "1e0",
    "2001-01-01", "2001-01-01Z", "P1Y", "P12M", "INF", "x:y", "a:b", "", "QQ=="
};
#define NEQUAL_VALUES ((int) (sizeof(equal_values_) / sizeof(equal_values_[0])))

static void test_values(int count)
{
    char what[256];
    for (int i = 0; i < count; i++)
    {
        int tx = 1 + (int) (rnd() % XML_SCHEMAS_ANYSIMPLETYPE);
        int ty = rnd() % 2 ? tx : 1 + (int) (rnd() % XML_SCHEMAS_ANYSIMPLETYPE);
        const char *vx = equal_values_[rnd() % NEQUAL_VALUES], *vy = equal_values_[rnd() % NEQUAL_VALUES];
        char *ld = l_values_equal(tx, vx, ty, vy);
        char *nd = n_values_equal(tx, vx, ty, vy);
        snprintf(what, sizeof(what), "values equal %d '%s' %d '%s'", tx, vx, ty, vy);
        check_str(what, ld, nd);
        free(ld);
        free(nd);
    }
}

#ifndef XSD10_BUILD_DIFF_STUBS
static void test_parse(const char *file)
{
    char what[1024];
    char *ld = l_parse(file);
    char *nd = n_parse(file);
    snprintf(what, sizeof(what), "parse %s", file);
    check_str(what, ld, nd);
    compare_caps(what);
    if (verbose_) printf("%s %s\n", (ld && nd && !strcmp(ld, nd)) ? "SAME" : "DIFF", file);
    free(ld);
    free(nd);
}

#endif

// Mutated schemas ////////////////////////////////////////////////

#ifndef XSD10_BUILD_DIFF_STUBS

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *) malloc(n + 1);
    if (fread(buf, 1, n, f) != (size_t) n) { free(buf); fclose(f); return NULL; }
    buf[n] = 0;
    fclose(f);
    *len = (size_t) n;
    return buf;
}

/* Replace len bytes at pos of *buf with rep. */
static void splice(char **buf, size_t pos, size_t len, const char *rep)
{
    size_t old = strlen(*buf), r = strlen(rep);
    char *n = (char *) malloc(old - len + r + 1);
    memcpy(n, *buf, pos);
    memcpy(n + pos, rep, r);
    memcpy(n + pos + r, *buf + pos + len, old - pos - len + 1);
    free(*buf);
    *buf = n;
}

/* The attribute values (a="v") of the text: offsets and lengths of the values. */
typedef struct { size_t name, name_len, value, value_len; } AttrPos;

static int find_attrs(const char *buf, AttrPos *out, int max)
{
    int n = 0;
    for (const char *p = buf; *p && n < max; p++)
    {
        if (*p != '=' || (p[1] != '"' && p[1] != '\'')) continue;
        const char *ne = p;
        const char *nb = p;
        while (nb > buf && (nb[-1] == '_' || nb[-1] == ':' || nb[-1] == '-' || (nb[-1] >= 'a' && nb[-1] <= 'z') || (nb[-1] >= 'A' && nb[-1] <= 'Z'))) nb--;
        if (nb == ne) continue;
        char q = p[1];
        const char *vb = p + 2, *ve = strchr(vb, q);
        if (!ve) break;
        // Only the schema: not the xml declaration and the namespace declarations.
        if (!strncmp(nb, "xmlns", 5) || !strncmp(nb, "version", 7) || !strncmp(nb, "encoding", 8) ||
            !strncmp(nb, "standalone", 10))
        {
            p = ve;
            continue;
        }
        out[n].name = nb - buf;
        out[n].name_len = ne - nb;
        out[n].value = vb - buf;
        out[n].value_len = ve - vb;
        n++;
        p = ve;
    }
    return n;
}

static const char *mut_values_[] = {
    "0", "1", "2", "3", "unbounded", "-1", "true", "false", "qualified", "unqualified", "#all", "extension",
    "restriction", "substitution", "list union", "##any", "##other", "##local ##targetNamespace", "lax", "skip",
    "strict", "prohibited", "required", "optional", "xs:string", "xs:int", "xs:decimal", "xs:anyType",
    "xs:anySimpleType", "xs:NMTOKENS", "xs:QName", "xs:ID", "xs:boolean", "xsd:string", "xsd:anyType", "foo",
    "5", "abc", "preserve", "collapse", "[a-z]*"
};
static const char *mut_names_[] = {
    "sequence", "choice", "all", "group", "element", "any", "restriction", "extension", "simpleContent",
    "complexContent", "attribute", "attributeGroup", "anyAttribute", "simpleType", "complexType", "list", "union"
};

static void mutate(char **buf)
{
    AttrPos attrs[4096];
    int n = find_attrs(*buf, attrs, 4096);
    switch (rnd() % 4)
    {
    case 0: case 1:
    {
        // A value: one of the pool or the value of another attribute.
        if (!n) return;
        AttrPos *a = &attrs[rnd() % n];
        char rep[512];
        if (rnd() % 2)
        {
            snprintf(rep, sizeof(rep), "%s", mut_values_[rnd() % (sizeof(mut_values_) / sizeof(mut_values_[0]))]);
        }
        else
        {
            AttrPos *b = &attrs[rnd() % n];
            size_t l = b->value_len < sizeof(rep) - 1 ? b->value_len : sizeof(rep) - 1;
            memcpy(rep, *buf + b->value, l);
            rep[l] = 0;
        }
        splice(buf, a->value, a->value_len, rep);
        break;
    }
    case 2:
    {
        // Remove an attribute.
        if (!n) return;
        AttrPos *a = &attrs[rnd() % n];
        splice(buf, a->name, a->value + a->value_len + 1 - a->name, "");
        break;
    }
    default:
    {
        // Rename all the elements with a local name (start and end tags).
        size_t nn = sizeof(mut_names_) / sizeof(mut_names_[0]);
        const char *from = mut_names_[rnd() % nn], *to = mut_names_[rnd() % nn];
        char f1[64], t1[64];
        snprintf(f1, sizeof(f1), ":%s", from);
        snprintf(t1, sizeof(t1), ":%s", to);
        size_t pos = 0;
        for (;;)
        {
            char *p = strstr(*buf + pos, f1);
            if (!p) break;
            size_t at = p - *buf;
            char after = p[strlen(f1)];
            int tag = (after == ' ' || after == '>' || after == '/' || after == '\n' || after == '\t' || after == '\r');
            const char *b = p;
            while (b > *buf && b[-1] != '<' && b[-1] != '/' && b[-1] != ' ' && b[-1] != '"') b--;
            if (tag && b > *buf && (b[-1] == '<' || b[-1] == '/'))
            {
                splice(buf, at, strlen(f1), t1);
                pos = at + strlen(t1);
            }
            else pos = at + 1;
        }
        break;
    }
    }
}

static void test_mutations(int count, char **files, int nfiles)
{
    char what[1200];
    for (int i = 0; i < count && nfiles > 0; i++)
    {
        const char *file = files[rnd() % nfiles];
        size_t len;
        char *buf = read_file(file, &len);
        if (!buf) continue;
        int k = 1 + rnd() % 3;
        for (int m = 0; m < k; m++) mutate(&buf);
        char *ld = l_parse_mem(buf, (int) strlen(buf));
        char *nd = n_parse_mem(buf, (int) strlen(buf));
        snprintf(what, sizeof(what), "mutation #%d of %s", i, file);
        check_str(what, ld, nd);
        if (verbose_ > 2 || (verbose_ && (!ld || !nd || strcmp(ld, nd)))) printf("--- %s\n%s\n", what, buf);
        compare_caps(what);
        free(ld);
        free(nd);
        free(buf);
    }
}

#endif

int main(int argc, char **argv)
{
    int count = 0, i = 1, parse = 0, mutations = 0;
    unsigned long long mseed = 1;
    const char *tests = "wcbfv";
    while (i < argc && argv[i][0] == '-')
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
        {
            unsigned long long seed = 1;
            if (sscanf(argv[++i], "%d:%llu", &count, &seed) < 1) count = 0;
            rng_ = seed;
        }
        else if (!strcmp(argv[i], "-p")) parse = 1;
        else if (!strcmp(argv[i], "-m") && i + 1 < argc)
        {
            unsigned long long seed = 1;
            if (sscanf(argv[++i], "%d:%llu", &mutations, &seed) < 1) mutations = 0;
            mseed = seed;
        }
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) tests = argv[++i];
        else break;
        i++;
    }
    xmlSchemaInitTypes();
    xml_xsd10_init_types();
    xml_xsd10_install_regexp_hooks();

    if (count > 0)
    {
        if (strchr(tests, 'w')) test_wildcards(count);
        if (strchr(tests, 'c')) test_content_models(count);
        if (strchr(tests, 'b')) test_builtin_derivation();
        if (strchr(tests, 'f')) test_facets(count);
        if (strchr(tests, 'v')) test_values(count);
    }
#ifndef XSD10_BUILD_DIFF_STUBS
    if (parse)
        for (int j = i; j < argc; j++) test_parse(argv[j]);
    if (mutations > 0)
    {
        rng_ = mseed;
        test_mutations(mutations, argv + i, argc - i);
    }
#else
    (void) parse;
    (void) mutations;
    (void) mseed;
#endif

    printf("xml_xsd10_build_diff: %ld same %ld diff\n", same_, diff_);
    xmlSchemaCleanupTypes();
    xmlCleanupParser();
    return diff_ ? 1 : 0;
}

#else // XSD10_BUILD_DIFF_SIDE ///////////////////////////////////

#if XSD10_BUILD_DIFF_SIDE == 1
/* libxml2 */
#define S(x) l_##x
#define SCHEMA_T xmlSchema
#define T(x) xmlSchema##x
#define K(x) XML_SCHEMA_##x
#define KS(x) XML_SCHEMAS_##x
#define UNB UNBOUNDED
#define CAP lcap_
#define STR const xmlChar
#define DICT(ctxt, s) xmlDictLookup((ctxt)->dict, (const xmlChar *) (s), -1)
#define PCTXT xmlSchemaParserCtxt
#define NEW_CTXT(url) xmlSchemaNewParserCtxt(url)
#define NEW_MEM_CTXT(b, l) xmlSchemaNewMemParserCtxt(b, l)
#define SET_SERR(ctxt) xmlSchemaSetParserStructuredErrors(ctxt, l_serr, NULL)
#define FREE_CTXT(ctxt) xmlSchemaFreeParserCtxt(ctxt)
#define CONSTRUCTION_CREATE(d) xmlSchemaConstructionCtxtCreate(d)
#define HASH_CREATE(d) xmlHashCreateDict(10, d)
#define HASH_LOOKUP2(h, a, b) xmlHashLookup2(h, a, b)
#define HASH_ADD2(h, a, b, p) xmlHashAddEntry2(h, a, b, p)
#define ITEM_LIST_CREATE() xmlSchemaItemListCreate()
#define ITEM_LIST_ADD(l, i) xmlSchemaItemListAdd(l, i)
#define FREE_WILDCARD(w) xmlSchemaFreeWildcard(w)
#define REGEXP_FREE(r) xmlRegFreeRegexp((xmlRegexpPtr) (r))
#define EXEC_CTXT xmlRegExecCtxt
#define EXEC_NEW(r, cb, d) xmlRegNewExecCtxt((xmlRegexpPtr) (r), cb, d)
#define EXEC_PUSH2(e, a, b, d) xmlRegExecPushString2(e, (const xmlChar *) (a), (const xmlChar *) (b), d)
#define EXEC_FREE(e) xmlRegFreeExecCtxt(e)
#define EXEC_CB_ARG xmlRegExecCtxtPtr
#define F_UNION xmlSchemaUnionWildcards
#define F_INTERSECT xmlSchemaIntersectWildcards
#define F_SUBSET xmlSchemaCheckCOSNSSubset
#define F_CLONE xmlSchemaCloneWildcardNsConstraints
#define F_CVC_NS xmlSchemaCheckCVCWildcardNamespace
#define F_BUILD_CM xmlSchemaBuildContentModel
#define F_IS_EMPTIABLE xmlSchemaIsParticleEmptiable
#define F_GET_EMPTIABLE xmlSchemaGetParticleEmptiable
#define F_BUILTIN(i) xmlSchemaGetBuiltInType((xmlSchemaValType) (i))
#define F_COSST xmlSchemaCheckCOSSTDerivedOK
#define F_COS xmlSchemaCheckCOSDerivedOK
#define F_IS_DERIVED_BUILTIN xmlSchemaIsDerivedFromBuiltInType
#define F_PRIMITIVE xmlSchemaGetPrimitiveType
#define F_FINAL_CONTAINS xmlSchemaTypeFinalContains
#define F_ARE_EQUAL_TYPES xmlSchemaAreEqualTypes
#define ACTXT(c) ((xmlSchemaAbstractCtxtPtr) (c))
#define NEW_FACET() xmlSchemaNewFacet()
#define FREE_FACET(f) xmlSchemaFreeFacet(f)
#define F_CHECK_FACET(f, t, c) xmlSchemaCheckFacet(f, t, c, NULL)
#define F_PARSE(c) xmlSchemaParse(c)
#define F_VAL_PREDEF(t, v, r) xmlSchemaValPredefTypeNode(t, (const xmlChar *) (v), r, NULL)
#define F_ARE_VALUES_EQUAL xmlSchemaAreValuesEqual
#define FREE_VALUE xmlSchemaFreeValue
#define FREE_SCHEMA(s) xmlSchemaFree(s)
#define HASH_SCAN_FULL(h, f, d) xmlHashScanFull(h, f, d)
#define HASH_SIZE(h) xmlHashSize(h)
#else
/* native */
#define S(x) n_##x
#define SCHEMA_T XmlXsd10Schema
#define T(x) XmlXsd10##x
#define K(x) XML_XSD10_SCHEMA_##x
#define KS(x) XML_XSD10_SCHEMAS_##x
#define UNB XML_XSD10_UNBOUNDED
#define CAP ncap_
#define STR const char
#define DICT(ctxt, s) xml_xsd10_dict_lookup((ctxt)->dict, (s), -1)
#define PCTXT XmlXsd10ParserCtxt
#define NEW_CTXT(url) xml_xsd10_new_parser_ctxt(url)
#define NEW_MEM_CTXT(b, l) xml_xsd10_new_mem_parser_ctxt(b, l)
#define SET_SERR(ctxt) xml_xsd10_set_parser_structured_errors(ctxt, n_serr, NULL)
#define FREE_CTXT(ctxt) xml_xsd10_free_parser_ctxt(ctxt)
#define CONSTRUCTION_CREATE(d) xml_xsd10_construction_ctxt_create(d)
#define HASH_CREATE(d) xml_xsd10_hash_create_dict(10, d)
#define HASH_LOOKUP2(h, a, b) xml_xsd10_hash_lookup2(h, a, b)
#define HASH_ADD2(h, a, b, p) xml_xsd10_hash_add_entry2(h, a, b, p)
#define ITEM_LIST_CREATE() xml_xsd10_item_list_create()
#define ITEM_LIST_ADD(l, i) xml_xsd10_item_list_add(l, i)
#define FREE_WILDCARD(w) xml_xsd10_free_wildcard(w)
#define REGEXP_FREE(r) xml_regexp_free((XmlRegexp *) (r))
#define EXEC_CTXT XmlRegExecCtxt
#define EXEC_NEW(r, cb, d) xml_reg_exec_new((XmlRegexp *) (r), cb, d)
#define EXEC_PUSH2(e, a, b, d) xml_reg_exec_push_string2(e, a, b, d)
#define EXEC_FREE(e) xml_reg_exec_free(e)
#define EXEC_CB_ARG void *
#define F_UNION xml_xsd10_union_wildcards
#define F_INTERSECT xml_xsd10_intersect_wildcards
#define F_SUBSET xml_xsd10_check_cosns_subset
#define F_CLONE xml_xsd10_clone_wildcard_ns_constraints
#define F_CVC_NS xml_xsd10_check_cvc_wildcard_namespace
#define F_BUILD_CM xml_xsd10_build_content_model
#define F_IS_EMPTIABLE xml_xsd10_is_particle_emptiable
#define F_GET_EMPTIABLE xml_xsd10_get_particle_emptiable
#define F_BUILTIN(i) xml_xsd10_get_built_in_type((XmlXsd10ValType) (i))
#define F_COSST xml_xsd10_check_cosst_derived_ok
#define F_COS xml_xsd10_check_cos_derived_ok
#define F_IS_DERIVED_BUILTIN xml_xsd10_is_derived_from_built_in_type
#define F_PRIMITIVE xml_xsd10_get_primitive_type
#define F_FINAL_CONTAINS xml_xsd10_type_final_contains
#define F_ARE_EQUAL_TYPES xml_xsd10_are_equal_types
#define ACTXT(c) ((XmlXsd10AbstractCtxt *) (c))
#define NEW_FACET() xml_xsd10_new_facet()
#define FREE_FACET(f) xml_xsd10_free_facet(f)
#define F_CHECK_FACET(f, t, c) xml_xsd10_check_facet_ctxt(f, t, c, NULL)
#define F_PARSE(c) xml_xsd10_parse(c)
#define F_VAL_PREDEF(t, v, r) xml_xsd10_val_predef_type_node(t, v, r, NULL)
#define F_ARE_VALUES_EQUAL xml_xsd10_are_values_equal
#define FREE_VALUE xml_xsd10_free_value
#define FREE_SCHEMA(s) xml_xsd10_free(s)
#define HASH_SCAN_FULL(h, f, d) xml_xsd10_hash_scan_full(h, f, d)
#define HASH_SIZE(h) xml_xsd10_hash_size(h)
#endif

static PCTXT *S(new_ctxt)(void)
{
    PCTXT *ctxt = NEW_CTXT("build_diff");
    SET_SERR(ctxt);
    return ctxt;
}

static T(Wildcard) *S(make_wild)(PCTXT *ctxt, const WSpec *w, int type)
{
    T(Wildcard) *r = (T(Wildcard) *) calloc(1, sizeof(*r));
    r->type = type;
    r->processContents = w->pc;
    r->minOccurs = 1;
    r->maxOccurs = 1;
    if (w->any) r->any = 1;
    else if (w->neg >= 0)
    {
        r->negNsSet = (T(WildcardNs) *) calloc(1, sizeof(T(WildcardNs)));
        r->negNsSet->value = DICT(ctxt, ns_pool_[w->neg]);
    }
    else
    {
        T(WildcardNs) *last = NULL;
        for (int i = 0; i < NSPOOL; i++)
        {
            if (!(w->set & (1u << i))) continue;
            T(WildcardNs) *ns = (T(WildcardNs) *) calloc(1, sizeof(T(WildcardNs)));
            ns->value = DICT(ctxt, ns_pool_[i]);
            if (last) last->next = ns; else r->nsSet = ns;
            last = ns;
        }
    }
    return r;
}

static void S(describe_wild)(Cap *c, T(Wildcard) *w)
{
    cap_printf(c, "any=%d pc=%d", w->any, w->processContents);
    if (w->negNsSet) cap_printf(c, " not=%s", w->negNsSet->value ? (const char *) w->negNsSet->value : "(absent)");
    if (w->negNsSet && w->negNsSet->next) cap_add(c, " NEGNEXT");
    if (w->nsSet)
    {
        cap_add(c, " set=");
        for (T(WildcardNs) *ns = w->nsSet; ns; ns = ns->next)
            cap_printf(c, "%s,", ns->value ? (const char *) ns->value : "(absent)");
    }
}

/* op 0 union, 1 intersection, 2 subset, 3 clone, 4 cvc-wildcard-namespace. */
static int S(wild_op)(PCTXT *ctxt, int op, const WSpec *a, const WSpec *b, char **desc)
{
    Cap c = { NULL, 0, 0 };
    int ret = 0;
    T(Wildcard) *wa = S(make_wild)(ctxt, a, K(TYPE_ANY_ATTRIBUTE));
    T(Wildcard) *wb = S(make_wild)(ctxt, b, K(TYPE_ANY_ATTRIBUTE));
    switch (op)
    {
    case 0: ret = F_UNION(ctxt, wa, wb); break;
    case 1: ret = F_INTERSECT(ctxt, wa, wb); break;
    case 2: ret = F_SUBSET(wa, wb); break;
    case 3:
    {
        // libxml2 assigns the set of dest without freeing it, dest has none (like in libxml2).
        WSpec da = *a;
        da.set = 0;
        T(Wildcard) *d = S(make_wild)(ctxt, &da, K(TYPE_ANY_ATTRIBUTE));
        ret = F_CLONE(ctxt, d, wb);
        S(describe_wild)(&c, d);
        cap_add(&c, " | ");
        FREE_WILDCARD(d);
        break;
    }
    case 4:
        for (int i = 0; i < NSPOOL; i++)
        {
            // Dict strings and (for the native side) equal strings of another origin.
            char buf[32];
            snprintf(buf, sizeof(buf), "%s", ns_pool_[i] ? ns_pool_[i] : "");
            ret = ret * 4 + F_CVC_NS(wa, DICT(ctxt, ns_pool_[i])) + 1;
            ret = ret * 4 + F_CVC_NS(wa, ns_pool_[i] ? (STR *) buf : NULL) + 1;
        }
        break;
    }
    S(describe_wild)(&c, wa);
    cap_add(&c, " | ");
    S(describe_wild)(&c, wb);
    FREE_WILDCARD(wa);
    FREE_WILDCARD(wb);
    *desc = c.s;
    return ret;
}

// Content models

typedef struct
{
    PCTXT *ctxt;
    T(Element) *elems[MAX_ELEMS];
    void *parts[MAX_PARTS];   /* the particles */
    void *terms[MAX_PARTS];   /* the model groups and wildcards (index of the particle) */
    int nelems;
    Cap log;
} S(Env);

static T(Particle) *S(build_particle)(S(Env) *env, const CMSpec *s, int idx)
{
    const PSpec *p = &s->parts[idx];
    T(Particle) *part = (T(Particle) *) calloc(1, sizeof(T(Particle)));
    env->parts[idx] = part;
    part->type = K(TYPE_PARTICLE);
    part->minOccurs = p->min;
    part->maxOccurs = p->max < 0 ? UNB : p->max;
    if (p->kind == 0)
        part->children = (T(TreeItem) *) env->elems[p->elem];
    else if (p->kind == 1)
    {
        T(Wildcard) *w = S(make_wild)(env->ctxt, &p->wild, K(TYPE_ANY));
        env->terms[idx] = w;
        part->children = (T(TreeItem) *) w;
    }
    else
    {
        T(ModelGroup) *mg = (T(ModelGroup) *) calloc(1, sizeof(T(ModelGroup)));
        env->terms[idx] = mg;
        mg->type = p->kind == 2 ? K(TYPE_SEQUENCE) : p->kind == 3 ? K(TYPE_CHOICE) : K(TYPE_ALL);
        T(Particle) *prev = NULL;
        for (int c = 0; c < p->n; c++)
        {
            T(Particle) *child = S(build_particle)(env, s, p->child[c]);
            if (prev) prev->next = (T(TreeItem) *) child;
            else mg->children = (T(TreeItem) *) child;
            prev = child;
        }
        part->children = (T(TreeItem) *) mg;
    }
    return part;
}

#define cur_env S(cur_env_)
static S(Env) *cur_env;

static void S(exec_cb)(EXEC_CB_ARG exec, STR *token, void *transdata, void *inputdata)
{
    (void) exec; (void) inputdata;
    int idx = -1;
    for (int i = 0; i < cur_env->nelems; i++) if (cur_env->elems[i] == transdata) idx = i;
    for (int i = 0; i < MAX_PARTS; i++) if (transdata && cur_env->terms[i] == transdata) idx = 100 + i;
    cap_printf(&cur_env->log, " cb(%s,%d)", token ? (const char *) token : "(NULL)", idx);
}

static char *S(content_model)(const CMSpec *s, const PushSpec *pushes, int npushes)
{
    S(Env) env;
    memset(&env, 0, sizeof(env));
    env.ctxt = S(new_ctxt)();
    PCTXT *ctxt = env.ctxt;
    ctxt->constructor = CONSTRUCTION_CREATE(ctxt->dict);
    ctxt->ownsConstructor = 1;
    ctxt->constructor->substGroups = HASH_CREATE(ctxt->dict);

    env.nelems = s->nelems;
    for (int i = 0; i < s->nelems; i++)
    {
        T(Element) *e = (T(Element) *) calloc(1, sizeof(T(Element)));
        e->type = K(TYPE_ELEMENT);
        e->name = DICT(ctxt, name_pool_[s->elems[i].name]);
        e->targetNamespace = DICT(ctxt, ns_pool_[s->elems[i].ns]);
        e->flags = KS(ELEM_GLOBAL);
        if (s->elems[i].abstract) e->flags |= KS(ELEM_ABSTRACT);
        env.elems[i] = e;
    }
    for (int i = 0; i < s->nelems; i++)
    {
        if (!s->elems[i].head) continue;
        T(Element) *e = env.elems[i];
        e->flags |= KS(ELEM_SUBST_GROUP_HEAD);
        if (HASH_LOOKUP2(ctxt->constructor->substGroups, e->name, e->targetNamespace) != NULL) continue;
        T(SubstGroup) *g = (T(SubstGroup) *) calloc(1, sizeof(T(SubstGroup)));
        g->head = e;
        g->members = ITEM_LIST_CREATE();
        for (int m = 0; m < s->elems[i].nmembers; m++)
            if (s->elems[i].members[m] >= 0) ITEM_LIST_ADD(g->members, env.elems[s->elems[i].members[m]]);
        HASH_ADD2(ctxt->constructor->substGroups, e->name, e->targetNamespace, g);
    }

    T(Particle) *root = S(build_particle)(&env, s, s->root);
    T(Type) *type = (T(Type) *) calloc(1, sizeof(T(Type)));
    type->type = K(TYPE_COMPLEX);
    type->name = DICT(ctxt, "T");
    type->targetNamespace = DICT(ctxt, "urn:a");
    type->flags = KS(TYPE_GLOBAL);
    type->contentType = s->mixed ? K(CONTENT_MIXED) : K(CONTENT_ELEMENTS);
    type->subtypes = (T(Type) *) root;

    cap_printf(&env.log, "emptiable=%d/%d", F_IS_EMPTIABLE(root), F_GET_EMPTIABLE(root));
    size_t errs = CAP.s ? CAP.len : 0;
    F_BUILD_CM(type, ctxt);
    cap_printf(&env.log, " compiled=%d", type->contModel != NULL);
    // Like libxml2, only validate with deterministic content models (the schema is not
    // usable otherwise, and the backtracking of libxml2 can explode on the others).
    if ((type->contModel != NULL) && ((CAP.s ? CAP.len : 0) == errs))
    {
        cur_env = &env;
        for (int p = 0; p < npushes; p++)
        {
            EXEC_CTXT *exec = EXEC_NEW(type->contModel, S(exec_cb), NULL);
            cap_add(&env.log, " |");
            for (int i = 0; i < pushes[p].n; i++)
            {
                int ret = EXEC_PUSH2(exec, name_pool_[pushes[p].name[i]], ns_pool_[pushes[p].ns[i]], NULL);
                cap_printf(&env.log, " %d", ret);
                if (ret < 0) break;
            }
            cap_printf(&env.log, " end=%d", EXEC_PUSH2(exec, NULL, NULL, NULL));
            EXEC_FREE(exec);
        }
        cur_env = NULL;
    }
    if (type->contModel != NULL) REGEXP_FREE(type->contModel);
    free(type);
    for (int i = 0; i < MAX_PARTS; i++)
    {
        if (env.terms[i] && s->parts[i].kind == 1) FREE_WILDCARD((T(Wildcard) *) env.terms[i]);
        else free(env.terms[i]);
        free(env.parts[i]);
    }
    for (int i = 0; i < s->nelems; i++) free(env.elems[i]);
    FREE_CTXT(ctxt);
    return env.log.s;
}

// Built in types

static char *S(builtin_derivation)(PCTXT *ctxt)
{
    Cap c = { NULL, 0, 0 };
    for (int i = 0; i <= KS(ANYSIMPLETYPE) + 1; i++)
    {
        T(Type) *ti = F_BUILTIN(i);
        cap_printf(&c, "\n%d:%d", i, ti != NULL);
        if (ti == NULL) continue;
        // anyType is its own base type, libxml2 only asks for the primitive type of simple types.
        T(Type) *prim = ti->builtInType == KS(ANYTYPE) ? NULL : F_PRIMITIVE(ti);
        cap_printf(&c, " prim=%d", prim ? (int) prim->builtInType : -1);
        for (int f = 0; f < 8; f++) cap_printf(&c, "%d", F_FINAL_CONTAINS(ti, 1 << f) ? 1 : 0);
        cap_add(&c, " der=");
        for (int v = 0; v <= KS(ANYSIMPLETYPE); v++) cap_printf(&c, "%d", F_IS_DERIVED_BUILTIN(ti, v));
        for (int j = 0; j <= KS(ANYSIMPLETYPE); j++)
        {
            T(Type) *tj = F_BUILTIN(j);
            if (tj == NULL) continue;
            cap_printf(&c, " %d:%d", j, F_ARE_EQUAL_TYPES(ti, tj));
            for (int subset = 0; subset < 32; subset++)
            {
                if (ti->type != K(TYPE_BASIC) || ti->builtInType != KS(ANYTYPE))
                    cap_printf(&c, ",%d", F_COSST(ACTXT(ctxt), ti, tj, subset));
                cap_printf(&c, "/%d", F_COS(ACTXT(ctxt), ti, tj, subset));
            }
        }
    }
    return c.s;
}

// Facets

static char *S(facet)(PCTXT *ctxt, int typeIdx, int facetType, const char *value)
{
    Cap c = { NULL, 0, 0 };
    T(Type) *type = F_BUILTIN(typeIdx);
    if (type == NULL) return strdup("no type");
    T(Facet) *f = NEW_FACET();
    f->type = (T(TypeType)) facetType;
    f->value = DICT(ctxt, value);
    int ret = F_CHECK_FACET(f, type, ctxt);
    cap_printf(&c, "ret=%d val=%d regexp=%d ws=%d", ret, f->val != NULL, f->regexp != NULL, f->whitespace);
    FREE_FACET(f);
    return c.s;
}

// Values

static char *S(values_equal)(int tx, const char *vx, int ty, const char *vy)
{
    Cap c = { NULL, 0, 0 };
    T(Type) *x = F_BUILTIN(tx), *y = F_BUILTIN(ty);
    T(Val) *a = NULL, *b = NULL;
    if (x == NULL || y == NULL) return strdup("no type");
    int ra = F_VAL_PREDEF(x, vx, &a);
    int rb = F_VAL_PREDEF(y, vy, &b);
    cap_printf(&c, "%d %d", ra, rb);
    if (a && b) cap_printf(&c, " equal=%d", F_ARE_VALUES_EQUAL(a, b));
    if (a) FREE_VALUE(a);
    if (b) FREE_VALUE(b);
    return c.s;
}

// Parsing a schema

typedef struct { char **lines; int n, max; } S(Lines);

static void S(add_line)(S(Lines) *l, char *s)
{
    if (l->n == l->max)
    {
        l->max = l->max ? 2 * l->max : 64;
        l->lines = (char **) realloc(l->lines, l->max * sizeof(char *));
    }
    l->lines[l->n++] = s;
}

#define NM(x) ((x) ? (const char *) (x) : "(NULL)")

static void S(dump_item)(void *payload, void *data, STR *name, STR *name2, STR *name3)
{
    S(Lines) *l = (S(Lines) *) data;
    Cap c = { NULL, 0, 0 };
    T(BasicItem) *item = (T(BasicItem) *) payload;
    (void) name2; (void) name3;
    cap_printf(&c, "%s t=%d", NM(name), (int) item->type);
    switch (item->type)
    {
    case K(TYPE_COMPLEX):
    case K(TYPE_SIMPLE):
    {
        T(Type) *t = (T(Type) *) item;
        int nfacets = 0, nlinks = 0, nmembers = 0;
        for (T(Facet) *f = t->facets; f; f = f->next) nfacets++;
        for (T(FacetLink) *f = t->facetSet; f; f = f->next) nlinks++;
        for (T(TypeLink) *m = t->memberTypes; m; m = m->next) nmembers++;
        cap_printf(&c, " ns=%s flags=%x ct=%d bt=%d base=%s/%s cm=%d uses=%d facets=%d/%d members=%d wild=%d",
                   NM(t->targetNamespace), t->flags, (int) t->contentType, t->builtInType,
                   t->baseType ? NM(t->baseType->name) : "-", t->baseType ? NM(t->baseType->targetNamespace) : "-",
                   t->contModel != NULL,
                   t->attrUses ? ((T(ItemList) *) t->attrUses)->nbItems : -1, nfacets, nlinks, nmembers,
                   t->attributeWildcard != NULL);
        if (t->subtypes) cap_printf(&c, " sub=%d", (int) t->subtypes->type);
        if (t->contentTypeDef) cap_printf(&c, " ctd=%s/%d", NM(t->contentTypeDef->name), (int) t->contentTypeDef->type);
        if (t->attrUses)
        {
            T(ItemList) *uses = (T(ItemList) *) t->attrUses;
            for (int i = 0; i < uses->nbItems; i++)
            {
                T(AttributeUse) *u = (T(AttributeUse) *) uses->items[i];
                cap_printf(&c, " use(%s,%s,%d,%s)", NM(u->attrDecl ? u->attrDecl->name : NULL),
                           NM(u->attrDecl ? u->attrDecl->targetNamespace : NULL), u->occurs, NM(u->defValue));
            }
        }
        break;
    }
    case K(TYPE_ELEMENT):
    {
        T(Element) *e = (T(Element) *) item;
        cap_printf(&c, " ns=%s flags=%x type=%s/%d subst=%s value=%s val=%d idcs=%d",
                   NM(e->targetNamespace), e->flags,
                   e->subtypes ? NM(e->subtypes->name) : "-", e->subtypes ? (int) e->subtypes->type : -1,
                   e->refDecl ? NM(e->refDecl->name) : "-", NM(e->value), e->defVal != NULL, e->idcs != NULL);
        break;
    }
    case K(TYPE_ATTRIBUTE):
    {
        T(Attribute) *a = (T(Attribute) *) item;
        cap_printf(&c, " ns=%s flags=%x type=%s def=%s val=%d", NM(a->targetNamespace), a->flags,
                   a->subtypes ? NM(a->subtypes->name) : "-", NM(a->defValue), a->defVal != NULL);
        break;
    }
    case K(TYPE_ATTRIBUTEGROUP):
    {
        T(AttributeGroup) *g = (T(AttributeGroup) *) item;
        cap_printf(&c, " ns=%s flags=%x uses=%d wild=%d", NM(g->targetNamespace), g->flags,
                   g->attrUses ? ((T(ItemList) *) g->attrUses)->nbItems : -1, g->attributeWildcard != NULL);
        break;
    }
    case K(TYPE_GROUP):
    {
        T(ModelGroupDef) *g = (T(ModelGroupDef) *) item;
        cap_printf(&c, " ns=%s flags=%x model=%d", NM(g->targetNamespace), g->flags,
                   g->children ? (int) g->children->type : -1);
        break;
    }
    default:
        break;
    }
    S(add_line)(l, c.s);
}

static int S(cmp_lines)(const void *a, const void *b)
{
    return strcmp(*(char * const *) a, *(char * const *) b);
}

static char *S(parse_ctxt)(PCTXT *ctxt)
{
    Cap c = { NULL, 0, 0 };
    SET_SERR(ctxt);
    SCHEMA_T *schema = F_PARSE(ctxt);
    cap_printf(&c, "schema=%d", schema != NULL);
    if (schema != NULL)
    {
        S(Lines) l = { NULL, 0, 0 };
        cap_printf(&c, " sizes=%d,%d,%d,%d,%d,%d,%d", HASH_SIZE(schema->typeDecl), HASH_SIZE(schema->elemDecl),
                   HASH_SIZE(schema->attrDecl), HASH_SIZE(schema->attrgrpDecl), HASH_SIZE(schema->groupDecl),
                   HASH_SIZE(schema->notaDecl), HASH_SIZE(schema->idcDef));
        if (schema->typeDecl) HASH_SCAN_FULL(schema->typeDecl, S(dump_item), &l);
        if (schema->elemDecl) HASH_SCAN_FULL(schema->elemDecl, S(dump_item), &l);
        if (schema->attrDecl) HASH_SCAN_FULL(schema->attrDecl, S(dump_item), &l);
        if (schema->attrgrpDecl) HASH_SCAN_FULL(schema->attrgrpDecl, S(dump_item), &l);
        if (schema->groupDecl) HASH_SCAN_FULL(schema->groupDecl, S(dump_item), &l);
        if (l.n > 0) qsort(l.lines, l.n, sizeof(char *), S(cmp_lines));
        for (int i = 0; i < l.n; i++)
        {
            cap_add(&c, "\n");
            cap_add(&c, l.lines[i]);
            free(l.lines[i]);
        }
        free(l.lines);
        FREE_SCHEMA(schema);
    }
    FREE_CTXT(ctxt);
    return c.s;
}

static char *S(parse)(const char *file)
{
    return S(parse_ctxt)(NEW_CTXT(file));
}

static char *S(parse_mem)(const char *buf, int len)
{
    return S(parse_ctxt)(NEW_MEM_CTXT(buf, len));
}

#undef NM
#undef cur_env
#undef S
#undef SCHEMA_T
#undef T
#undef K
#undef KS
#undef UNB
#undef CAP
#undef STR
#undef DICT
#undef PCTXT
#undef NEW_CTXT
#undef NEW_MEM_CTXT
#undef SET_SERR
#undef FREE_CTXT
#undef CONSTRUCTION_CREATE
#undef HASH_CREATE
#undef HASH_LOOKUP2
#undef HASH_ADD2
#undef ITEM_LIST_CREATE
#undef ITEM_LIST_ADD
#undef FREE_WILDCARD
#undef REGEXP_FREE
#undef EXEC_CTXT
#undef EXEC_NEW
#undef EXEC_PUSH2
#undef EXEC_FREE
#undef EXEC_CB_ARG
#undef F_UNION
#undef F_INTERSECT
#undef F_SUBSET
#undef F_CLONE
#undef F_CVC_NS
#undef F_BUILD_CM
#undef F_IS_EMPTIABLE
#undef F_GET_EMPTIABLE
#undef F_BUILTIN
#undef F_COSST
#undef F_COS
#undef F_IS_DERIVED_BUILTIN
#undef F_PRIMITIVE
#undef F_FINAL_CONTAINS
#undef F_ARE_EQUAL_TYPES
#undef ACTXT
#undef NEW_FACET
#undef FREE_FACET
#undef F_CHECK_FACET
#undef F_PARSE
#undef F_VAL_PREDEF
#undef F_ARE_VALUES_EQUAL
#undef FREE_VALUE
#undef FREE_SCHEMA
#undef HASH_SCAN_FULL
#undef HASH_SIZE

#endif // XSD10_BUILD_DIFF_SIDE
