/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native xpath (parts/xml_xpath.c) against libxml2 xmlXPathEval.

   For every xml file given on the command line, parse it with libxml2 (NONET, cdata kept) and
   with xml_parse_buffer, then evaluate a set of expressions (generic ones plus expressions built
   from the element and attribute names found in the document) with the document node and with
   the root element as context. The results must have the same type and value; node sets are
   compared by the position of each node in the tree.

   Expressions can also be given in a file with -e file (one per line), they are then evaluated
   against every document.

   Documents that declare entities are first evaluated only natively: libxml2 xpath sees the
   parsed entity content (and can loop forever on it), the native dom does not expand entities
   until xml_expand_entities. Then they are parsed again, by libxml2 with XML_PARSE_NOENT and
   natively followed by xml_expand_entities, and if the two trees serialize identically
   (external entities are loaded by libxml2 but not natively) both are compared as usual.

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xpath.h"
#include"parts/xml_save.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>
#include<libxml/hash.h>

#include<math.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

static int verbose_ = 0;
static long same_ = 0, diff_ = 0, both_fail_ = 0, native_fail_ = 0, libxml_fail_ = 0, files_ = 0, skipped_ = 0;
static int max_shown_ = 50;
static long entity_only_native_ = 0; // Expressions on entity documents, only evaluated natively.
static long entity_expanded_files_ = 0; // Entity documents also compared after expansion.
static long comp_same_ = 0, comp_diff_ = 0; // Compiled evaluation in a kept context versus xml_xpath_eval.
static XmlXPathContext *pctx_ = NULL; // Kept for all expressions of a document.
static char *nx_describe(XmlXPathResult *r);

/** The expression compiled and evaluated in the kept context must give the same as xml_xpath_eval. */
static void check_compiled(const char *file, const char *ctxname, const char *expr, XMQNode *nctx, XmlXPathResult *r)
{
    if (!pctx_) return;
    XmlXPathError cerr;
    XmlXPathComp *comp = xml_xpath_compile(expr, NULL, &cerr);
    XmlXPathResult *cr = comp ? xml_xpath_eval_comp(pctx_, comp, nctx, -1, -1, &cerr) : NULL;
    char *a = r ? nx_describe(r) : strdup("(error)");
    char *b = cr ? nx_describe(cr) : strdup("(error)");
    if (!strcmp(a, b))
    {
        comp_same_++;
    }
    else
    {
        comp_diff_++;
        printf("COMPILED DIFF %s [%s] %s\n  eval:     %.300s\n  compiled: %.300s\n", file, ctxname, expr, a, b);
    }
    free(a);
    free(b);
    xml_xpath_result_free(cr);
    xml_xpath_comp_free(comp);
}
static int has_entities_ = 0; // The current document declares entities: libxml2 expands their content for xpath.
static double lx_time_ = 0, nx_time_ = 0;
static long max_size_ = 200000;

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static char **extra_exprs_ = NULL;
static size_t num_extra_ = 0;

static const char *generic_[] = {
    "/", ".", "*", "x|", "a/b|", "* |", "|x", "x||y", "/*", "//*", "//node()", "//text()", "//comment()", "//processing-instruction()",
    "//@*", "//namespace::*", "/*/namespace::*", "//*/namespace::xml", "count(//*)", "count(//@*)",
    "count(//node())", "count(//namespace::*)", "string(/)", "string(/*)", "string(//@*)", "name(/*)",
    "local-name(/*)", "namespace-uri(/*)", "name(//@*)", "//*[1]", "//*[last()]", "//*[position() mod 2 = 0]",
    "(//*)[1]", "(//*)[last()]", "(//*)[2]", "//*[2]", "//*[@*]", "//*[not(*)]", "//*[text()]",
    "/descendant::*[3]", "/descendant-or-self::node()[5]", "//*/ancestor::*", "//*/ancestor-or-self::*[1]",
    "//text()/parent::*", "//*/following-sibling::*", "//*/preceding-sibling::*[1]", "//*/following::*",
    "//*/preceding::*", "//*/following::node()[1]", "//*/preceding::node()[2]", "//@*/..", "//@*/following::*[1]",
    "//@*/preceding::*[1]", "//@*/ancestor::*", "//*/self::*", "//node()/self::text()", "//*/child::node()[last()]",
    "//*[count(*) > 1]", "//*[string-length() > 10]", "sum(//@*)", "sum(//*)", "count(//*) div 3",
    "count(//*) mod 7", "-count(//*)", "1 div 0", "-1 div 0", "0 div 0", "0.1 + 0.2", "1e3", "123456789012",
    "1.5e-7 * 3", "number('  12.5  ')", "number('abc')", "number('-.5')", "floor(-1.5)", "ceiling(1.2)",
    "round(2.5)", "round(-2.5)", "round(-0.2)", "string(1 div 3)", "string(-0)", "string(100000000000)",
    "string(0.000001)", "concat('a', 'b', 'c')", "substring('12345', 1.5, 2.6)", "substring('12345', 0, 3)",
    "substring('12345', 0 div 0, 3)", "substring('12345', 1, 0 div 0)", "substring('12345', -42, 1 div 0)",
    "substring('12345', -1 div 0, 1 div 0)", "substring('åäö€x', 2, 2)", "substring-before('1999/04/01', '/')",
    "substring-after('1999/04/01', '/')", "translate('bar', 'abc', 'ABC')", "translate('--aaa--', 'abc-', 'ABC')",
    "normalize-space('  a  b   c ')", "normalize-space()", "string-length('åäö')", "starts-with('abc', 'ab')",
    "contains('abc', 'd')", "boolean('')", "boolean(//*)", "not(//*)", "true() and false()", "true() or false()",
    "1 = 1", "1 != 1", "'1' = 1", "true() = 'a'", "//* = //*", "//* != //*", "//* = 'x'", "//* != 'x'",
    "//@* = //@*", "//* < 3", "//* > 3", "3 > //*", "//@* >= //*", "//* = true()", "//* != false()",
    "//*[. = ../*]", "2 < 3 < 4", "1 < 2 = true()", "lang('en')", "//*[lang('en')]", "id('a')", "id('a b c')",
    "id(//@*)", "//*[@id]", "//*[@xml:id]", "//*[@*[1]]", "//*[*[2]]", "//*[1][1]", "//*[position() = last()]",
    "//*[position() < 3][last()]", "//node()[2]", "(//node())[3]", "//*|//@*", "//@*|//text()",
    "(//*|//@*)[last()]", "//*[1] | //*[2]", "/*/*", "/*/*/*", "/*//*", "/*/..", "/..", "./*", "..",
    "//*[name() = name(/*)]", "//*[local-name() != '']", "//*[namespace-uri() != '']", "//*/@xml:lang",
    "//*[starts-with(name(), 'x')]", "count(//*/*)", "count(/*/node())", "last()", "position()",
    "//processing-instruction('xml-stylesheet')", "//self::node()", "descendant::text()[1]",
    "//text()[normalize-space()]", "string(//text()[normalize-space()][1])", "//*[contains(., 'a')]",
    "count(//*[contains(., 'a')])", "//comment()/following-sibling::node()", "//*[@*][2]",
    "escape-uri('a b/c', true())", "foo()", "$x", "//*[", "1 +", "'unterminated", "@", "//x:y",
    "ancestor::*", "following::*", "preceding::*[1]", "//*[.//*]", "//*[ancestor::*[2]]",
    "//*/descendant::*[1]", "//*/descendant-or-self::*[2]", "(/descendant::node())[last()]",
    "//*[following-sibling::*]", "//*[preceding::*][1]", "//*[1]/following::*[1]", "//*[last()]/preceding::*[1]",
    "count(//*[not(@*)])", "sum(//*[1])", "string(number(//*[1]))", "boolean(//@*[. = ''])",
    "//*[string(.) = '']", "count(//text()[. = ' '])", "//*[count(ancestor::*) = 2]",
};

static void add_extra_file(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) { fprintf(stderr, "Cannot read %s\n", file); exit(2); }
    char line[4096];
    while (fgets(line, sizeof(line), f))
    {
        size_t l = strlen(line);
        while (l && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (!l || line[0] == '#') continue;
        extra_exprs_ = (char**)realloc(extra_exprs_, (num_extra_ + 1) * sizeof(char*));
        extra_exprs_[num_extra_++] = strdup(line);
    }
    fclose(f);
}

static char *read_file(const char *file, size_t *len)
{
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = (char*)malloc(n + 1);
    size_t r = fread(b, 1, n, f);
    fclose(f);
    b[r] = 0;
    *len = r;
    return b;
}

// Node signatures: the path of child indexes from the document node.

typedef struct { char *b; size_t len, cap; } Buf;

static void buf_add(Buf *b, const char *s)
{
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap)
    {
        b->cap = (b->len + n + 1) * 2;
        b->b = (char*)realloc(b->b, b->cap);
    }
    memcpy(b->b + b->len, s, n + 1);
    b->len += n;
}

static void lx_sig(xmlNodePtr n, Buf *b)
{
    char tmp[64];
    if (n->type == XML_NAMESPACE_DECL)
    {
        xmlNsPtr ns = (xmlNsPtr)n;
        lx_sig((xmlNodePtr)ns->next, b);
        buf_add(b, "/ns:");
        buf_add(b, ns->prefix ? (const char*)ns->prefix : "");
        return;
    }
    if (n->type == XML_DOCUMENT_NODE || n->type == XML_HTML_DOCUMENT_NODE)
    {
        buf_add(b, "#doc");
        return;
    }
    if (n->type == XML_ATTRIBUTE_NODE)
    {
        lx_sig(n->parent, b);
        int i = 0;
        for (xmlAttrPtr a = n->parent->properties; a && (xmlNodePtr)a != n; a = a->next) i++;
        snprintf(tmp, sizeof(tmp), "/@%d", i);
        buf_add(b, tmp);
        return;
    }
    if (n->parent) lx_sig(n->parent, b);
    else buf_add(b, "#orphan");
    int i = 0;
    for (xmlNodePtr p = n->prev; p; p = p->prev) i++;
    snprintf(tmp, sizeof(tmp), "/%d", i);
    buf_add(b, tmp);
}

static void nx_sig(XMQNode *n, Buf *b)
{
    char tmp[64];
    XmlNodeKind k = xml_node_kind(n);
    if (k == XML_DOM_NAMESPACE)
    {
        nx_sig(xml_node_parent(n), b);
        buf_add(b, "/ns:");
        buf_add(b, xml_node_name(n) ? xml_node_name(n) : "");
        return;
    }
    if (k == XML_DOM_DOCUMENT)
    {
        buf_add(b, "#doc");
        return;
    }
    if (k == XML_DOM_ATTRIBUTE)
    {
        XMQNode *owner = xml_node_parent(n);
        nx_sig(owner, b);
        int i = 0;
        for (XMQAttr *a = xml_node_first_attr(owner); a && xml_attr_node(a) != n; a = xml_attr_next(a)) i++;
        snprintf(tmp, sizeof(tmp), "/@%d", i);
        buf_add(b, tmp);
        return;
    }
    if (xml_node_parent(n)) nx_sig(xml_node_parent(n), b);
    else buf_add(b, "#orphan");
    int i = 0;
    for (XMQNode *p = xml_node_prev(n); p; p = xml_node_prev(p)) i++;
    snprintf(tmp, sizeof(tmp), "/%d", i);
    buf_add(b, tmp);
}

static char *fmt_number(double d)
{
    return xml_xpath_format_number(d);
}

/** Describe the libxml2 result as a string. */
static char *lx_describe(xmlXPathObjectPtr o)
{
    Buf b = { NULL, 0, 0 };
    char tmp[64];
    switch (o->type)
    {
    case XPATH_NODESET:
        buf_add(&b, "nodeset[");
        if (o->nodesetval)
        {
            snprintf(tmp, sizeof(tmp), "%d]", o->nodesetval->nodeNr);
            buf_add(&b, tmp);
            for (int i = 0; i < o->nodesetval->nodeNr; ++i)
            {
                buf_add(&b, " ");
                lx_sig(o->nodesetval->nodeTab[i], &b);
            }
        }
        else buf_add(&b, "0]");
        break;
    case XPATH_BOOLEAN:
        buf_add(&b, o->boolval ? "boolean true" : "boolean false");
        break;
    case XPATH_NUMBER:
    {
        buf_add(&b, "number ");
        char *s = fmt_number(o->floatval);
        buf_add(&b, s);
        free(s);
        if (o->floatval == 0 && signbit(o->floatval)) buf_add(&b, " (-0)");
        break;
    }
    case XPATH_STRING:
        buf_add(&b, "string \"");
        buf_add(&b, (const char*)o->stringval);
        buf_add(&b, "\"");
        break;
    default:
        buf_add(&b, "other");
    }
    return b.b;
}

static char *nx_describe(XmlXPathResult *r)
{
    Buf b = { NULL, 0, 0 };
    char tmp[64];
    switch (xml_xpath_result_type(r))
    {
    case XML_XPATH_NODESET:
        snprintf(tmp, sizeof(tmp), "nodeset[%zu]", xml_xpath_result_size(r));
        buf_add(&b, tmp);
        for (size_t i = 0; i < xml_xpath_result_size(r); ++i)
        {
            buf_add(&b, " ");
            nx_sig(xml_xpath_result_node(r, i), &b);
        }
        break;
    case XML_XPATH_BOOLEAN:
        buf_add(&b, xml_xpath_result_boolean(r) ? "boolean true" : "boolean false");
        break;
    case XML_XPATH_NUMBER:
    {
        buf_add(&b, "number ");
        double d = xml_xpath_result_number(r);
        char *s = fmt_number(d);
        buf_add(&b, s);
        free(s);
        if (d == 0 && signbit(d)) buf_add(&b, " (-0)");
        break;
    }
    case XML_XPATH_STRING:
        buf_add(&b, "string \"");
        buf_add(&b, xml_xpath_result_string(r));
        buf_add(&b, "\"");
        break;
    }
    return b.b;
}

static void ignore_error(void *user, const xmlError *e)
{
    (void)user; (void)e;
}

static void show(const char *what, const char *file, const char *expr, const char *ctx, const char *a, const char *b)
{
    if (diff_ + native_fail_ + libxml_fail_ > max_shown_) return;
    printf("%s %s [%s] %s\n", what, file, ctx, expr);
    if (a) printf("  libxml2: %.400s\n", a);
    if (b) printf("  native:  %.400s\n", b);
}

static void test_expr(const char *file, xmlDocPtr doc, xmlNodePtr lctx, XmlDom *dom, XMQNode *nctx,
                      const char *ctxname, const char *expr)
{
    xmlXPathContextPtr xc = xmlXPathNewContext(doc);
    xmlXPathSetErrorHandler(xc, ignore_error, NULL);
    xc->node = lctx;
    double t0 = now();
    if (has_entities_)
    {
        // libxml2 parses the entity content into nodes that its axes walk into, and
        // loops forever on some documents (//*/following::* on ent_738805.xml). The
        // native dom never expands entities: only check the native side (sanitizers).
        XmlXPathError nerr;
        XmlXPathResult *nr = xml_xpath_eval(expr, nctx, NULL, &nerr);
        if (nr)
        {
            char *b = nx_describe(nr);
            free(b);
            xml_xpath_result_free(nr);
        }
        entity_only_native_++;
        xmlXPathFreeContext(xc);
        return;
    }
    xmlXPathObjectPtr o = xmlXPathEval((const xmlChar*)expr, xc);
    double t1 = now();
    XmlXPathError err;
    XmlXPathResult *r = xml_xpath_eval(expr, nctx, NULL, &err);
    double t2 = now();
    check_compiled(file, ctxname, expr, nctx, r);
    lx_time_ += t1 - t0;
    nx_time_ += t2 - t1;
    if (verbose_ > 1 && (t2 - t1 > 0.5 || t1 - t0 > 0.5)) printf("SLOW %s %s libxml2 %.2fs native %.2fs\n", file, expr, t1 - t0, t2 - t1);
    (void)dom;
    if (!o && !r)
    {
        both_fail_++;
    }
    else if (!o)
    {
        libxml_fail_++;
        char *b = nx_describe(r);
        show("NATIVE ACCEPTS", file, expr, ctxname, NULL, b);
        free(b);
    }
    else if (!r)
    {
        native_fail_++;
        char *a = lx_describe(o);
        show("NATIVE REJECTS", file, expr, ctxname, a, err.message);
        free(a);
    }
    else
    {
        char *a = lx_describe(o);
        char *b = nx_describe(r);
        if (!strcmp(a, b))
        {
            same_++;
            if (verbose_) printf("OK %s [%s] %s: %.200s\n", file, ctxname, expr, a);
        }
        else
        {
            diff_++;
            show("DIFF", file, expr, ctxname, a, b);
        }
        free(a);
        free(b);
    }
    if (o) xmlXPathFreeObject(o);
    if (r) xml_xpath_result_free(r);
    xmlXPathFreeContext(xc);
}

#define MAX_NAMES 4

static void add_name(char **names, int *num, const char *n)
{
    if (!n || strchr(n, '\'')) return;
    for (int i = 0; i < *num; ++i) if (!strcmp(names[i], n)) return;
    if (*num < MAX_NAMES) names[(*num)++] = strdup(n);
}

/** Collect a few unprefixed element and attribute names, and some text values. */
static void collect_names(xmlNodePtr n, char **en, int *ne, char **an, int *na, char **tv, int *nt)
{
    for (; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            if (!n->ns) add_name(en, ne, (const char*)n->name);
            for (xmlAttrPtr a = n->properties; a; a = a->next)
            {
                if (!a->ns) add_name(an, na, (const char*)a->name);
                if (a->children && a->children->content && strlen((const char*)a->children->content) < 20)
                {
                    add_name(tv, nt, (const char*)a->children->content);
                }
            }
            collect_names(n->children, en, ne, an, na, tv, nt);
        }
    }
}

static void test_doc_exprs(const char *file, xmlDocPtr doc, XmlDom *dom, xmlNodePtr lctx, XMQNode *nctx, const char *ctxname)
{
    for (size_t i = 0; i < sizeof(generic_) / sizeof(generic_[0]); ++i)
    {
        test_expr(file, doc, lctx, dom, nctx, ctxname, generic_[i]);
    }
    for (size_t i = 0; i < num_extra_; ++i)
    {
        test_expr(file, doc, lctx, dom, nctx, ctxname, extra_exprs_[i]);
    }
    char *en[MAX_NAMES], *an[MAX_NAMES], *tv[MAX_NAMES];
    int ne = 0, na = 0, nt = 0;
    collect_names(doc->children, en, &ne, an, &na, tv, &nt);
    static const char *elem_templates[] = {
        "//%s", "/%s", "%s", "//%s[1]", "//%s[last()]", "count(//%s)", "//%s/*", "//%s/..", "//%s//text()",
        "//%s/following::*[1]", "//%s/preceding-sibling::*", "string(//%s)", "//*[%s]", "//%s|//%s/@*",
        "//%s/ancestor::*", "//%s[2]/following-sibling::node()", "sum(//%s)", "//%s[. = //%s]", "%s/*",
        "//%s/descendant::*[2]", "(//%s)[2]", "//%s[position() > 1]", "/*/%s", "//*/%s", ".//%s",
    };
    static const char *attr_templates[] = {
        "//@%s", "//*[@%s]", "count(//@%s)", "//@%s/..", "string(//@%s)", "//*[@%s = 'x']", "sum(//@%s)",
        "//*[@%s][1]", "//@%s/following::*[1]", "//*[not(@%s)]",
    };
    static const char *value_templates[] = {
        "//*[@* = '%s']", "id('%s')", "//*[. = '%s']", "contains(/, '%s')", "translate('%s', 'abcdef', 'ABC')",
        "number('%s')", "substring-before('%s', 'e')",
    };
    char expr[1024];
    for (int i = 0; i < ne; ++i)
    {
        for (size_t t = 0; t < sizeof(elem_templates) / sizeof(elem_templates[0]); ++t)
        {
            snprintf(expr, sizeof(expr), elem_templates[t], en[i], en[i]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
        }
        if (i + 1 < ne)
        {
            snprintf(expr, sizeof(expr), "//%s | //%s", en[i], en[i+1]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
            snprintf(expr, sizeof(expr), "//%s//%s", en[i], en[i+1]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
            snprintf(expr, sizeof(expr), "/%s/%s", en[i], en[i+1]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
        }
    }
    for (int i = 0; i < na; ++i)
    {
        for (size_t t = 0; t < sizeof(attr_templates) / sizeof(attr_templates[0]); ++t)
        {
            snprintf(expr, sizeof(expr), attr_templates[t], an[i]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
        }
    }
    for (int i = 0; i < nt; ++i)
    {
        for (size_t t = 0; t < sizeof(value_templates) / sizeof(value_templates[0]); ++t)
        {
            snprintf(expr, sizeof(expr), value_templates[t], tv[i]);
            test_expr(file, doc, lctx, dom, nctx, ctxname, expr);
        }
    }
    for (int i = 0; i < ne; ++i) free(en[i]);
    for (int i = 0; i < na; ++i) free(an[i]);
    for (int i = 0; i < nt; ++i) free(tv[i]);
}

static void test_both(const char *file, xmlDocPtr doc, XmlDom *dom)
{
    pctx_ = xml_xpath_context_new();
    test_doc_exprs(file, doc, dom, (xmlNodePtr)doc, xml_dom_document(dom), "doc");
    xmlNodePtr lroot = xmlDocGetRootElement(doc);
    XMQNode *nroot = xml_dom_root_element(dom);
    if (lroot && nroot) test_doc_exprs(file, doc, dom, lroot, nroot, "root");
    xml_xpath_context_free(pctx_);
    pctx_ = NULL;
}

/** libxml2 resolves namespaces inside entity content without the namespaces in scope at the
    reference (a namespace error), native resolves them where the reference is, as the
    namespaces spec says, so these are not compared expanded. */
static const char *ns_in_entity_files_[] = { "test/errors/ns-ent.xml", "entities/elements.xml", "entities/ns.xml", NULL };

/** Parse with entities expanded on both sides and compare if the trees are identical. */
static void test_expanded(const char *file, const char *buf, size_t len)
{
    for (const char **f = ns_in_entity_files_; *f; ++f)
    {
        size_t fl = strlen(file), el = strlen(*f);
        if (fl >= el && !strcmp(file + fl - el, *f)) return;
    }
    xmlDocPtr doc = xmlReadMemory(buf, (int)len, file, NULL, XML_PARSE_NONET | XML_PARSE_NOENT | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc && doc->intSubset)
    {
        // libxml2 keeps the parsed content of each entity as children of its declaration
        // in the dtd, where its preceding axis and string(/) find it. Drop it.
        for (xmlNodePtr n = doc->intSubset->children; n; n = n->next)
        {
            if (n->type != XML_ENTITY_DECL) continue;
            xmlEntityPtr ent = (xmlEntityPtr)n;
            if (ent->children) xmlFreeNodeList(ent->children);
            ent->children = ent->last = NULL;
        }
    }
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = file;
    XmlParseError err;
    XmlDom *dom = xml_parse_buffer(buf, buf + len, &settings, &err);
    if (dom)
    {
        xml_parse_error_free(&err);
        if (!xml_expand_entities(dom, NULL, NULL, len, &settings, &err))
        {
            xml_dom_free(dom);
            dom = NULL;
        }
    }
    if (doc && dom)
    {
        xmlChar *e = NULL;
        int el = 0;
        xmlDocDumpMemoryEnc(doc, &e, &el, "utf-8");
        size_t gl = 0;
        char *g = xml_save_doc(dom, "utf-8", false, &gl);
        bool same = (size_t)el == gl && !memcmp(e, g, gl);
        xmlFree(e);
        free(g);
        if (same)
        {
            entity_expanded_files_++;
            has_entities_ = 0;
            test_both(file, doc, dom);
        }
    }
    if (doc) xmlFreeDoc(doc);
    if (dom) xml_dom_free(dom);
    xml_parse_error_free(&err);
}

static void test_file(const char *file, const char *buf, size_t len)
{
    xmlDocPtr doc = xmlReadMemory(buf, (int)len, file, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = file;
    XmlParseError err;
    XmlDom *dom = xml_parse_buffer(buf, buf + len, &settings, &err);
    bool expand = false;
    if (doc && dom)
    {
        files_++;
        has_entities_ = doc->intSubset && doc->intSubset->entities && xmlHashSize((xmlHashTablePtr)doc->intSubset->entities) > 0;
        test_both(file, doc, dom);
        expand = has_entities_;
    }
    else
    {
        skipped_++;
    }
    if (doc) xmlFreeDoc(doc);
    if (dom) xml_dom_free(dom);
    xml_parse_error_free(&err);
    if (expand) test_expanded(file, buf, len);
}

int main(int argc, char **argv)
{
    // Line buffered, so that the progress can be followed in a log file.
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-v")) { verbose_++; continue; }
        if (!strcmp(argv[i], "-e") && i + 1 < argc) { add_extra_file(argv[++i]); continue; }
        if (!strcmp(argv[i], "-n") && i + 1 < argc) { max_shown_ = atoi(argv[++i]); continue; }
        if (!strcmp(argv[i], "-s") && i + 1 < argc) { max_size_ = atol(argv[++i]); continue; }
        size_t len = 0;
        char *buf = read_file(argv[i], &len);
        if (!buf) continue;
        if ((long)len > max_size_)
        {
            // The expressions are quadratic on purpose (//*/following::*), keep the run time sane.
            skipped_++;
            free(buf);
            continue;
        }
        long before = same_;
        test_file(argv[i], buf, len);
        printf("xpath [%d/%d] %s: %ld same\n", i, argc-1, argv[i], same_ - before);
        free(buf);
    }
    for (size_t i = 0; i < num_extra_; ++i) free(extra_exprs_[i]);
    free(extra_exprs_);
    xmlCleanupParser();
    printf("xml_xpath_diff: %ld files (%ld skipped), %ld same, %ld differ, %ld both fail, "
           "%ld only native fails, %ld only libxml2 fails, %ld only native (entity documents, of which %ld also compared expanded) (libxml2 %.1fs native %.1fs)\n",
           files_, skipped_, same_, diff_, both_fail_, native_fail_, libxml_fail_, entity_only_native_, entity_expanded_files_, lx_time_, nx_time_);
    printf("xml_xpath_diff: compiled in a kept context: %ld same, %ld differ\n", comp_same_, comp_diff_);
    return (diff_ || native_fail_ || libxml_fail_ || comp_diff_) ? 1 : 0;
}
