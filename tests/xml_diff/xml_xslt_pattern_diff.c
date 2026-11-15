/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native xslt patterns (parts/xml_xslt.c) against libxslt.

   For every xml file given on the command line, parse it with libxml2 and with
   xml_parse_buffer, build a stylesheet element declaring the namespaces of the
   document, then compile a set of patterns (generic ones plus patterns built from
   the element and attribute names of the document) with libxslt xsltCompilePattern
   and with xml_xslt_pattern_compile. Both must agree on whether the pattern compiles,
   on its default priority and, walking every node of the document in document order
   (like a transformation, so that the position caches of the predicates are used),
   on whether it matches (xsltTestCompMatchList).

   This tool links libxml2 and libxslt and is only used while libxmq moves away from them.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xpath.h"
#include"parts/xml_xslt.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/xpath.h>
#include<libxml/xmlerror.h>
#include<libxslt/xsltInternals.h>
#include<libxslt/pattern.h>
#include<libxslt/transform.h>
#include<libxslt/xsltutils.h>

#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

static int verbose_ = 0;
static long files_ = 0, skipped_ = 0, patterns_ = 0, same_ = 0, diff_ = 0;
static long both_fail_ = 0, native_fail_ = 0, libxslt_fail_ = 0, prio_diff_ = 0;
static int max_shown_ = 30;
static int shown_ = 0;

static long msg_same_ = 0, msg_diff_ = 0;

// The messages of the current pattern compilation.
static char lx_msgs_[8192];
static char nx_msgs_[8192];

static void append_msg(char *buf, const char *m)
{
    size_t l = strlen(buf);
    snprintf(buf + l, sizeof(lx_msgs_) - l, "%s", m);
}

static void capture_lx(void *ctx, const char *msg, ...)
{
    (void)ctx;
    char b[4096];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(b, sizeof(b), msg, ap);
    va_end(ap);
    append_msg(lx_msgs_, b);
}

/** libxml2 errors formatted like its default handler does (domain, message, xpath context). */
static void capture_structured(void *ud, const xmlError *err)
{
    (void)ud;
    xmlFormatError(err, capture_lx, NULL);
}

static void capture_native(void *ud, const char *msg)
{
    (void)ud;
    append_msg(nx_msgs_, msg);
}

static char *read_file(const char *file, size_t *len)
{
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = (char*)malloc(n + 1);
    size_t r = fread(b, 1, n, f);
    fclose(f);
    b[r] = 0;
    *len = r;
    return b;
}

// Node pairs in document order: node, attributes, children.
typedef struct { xmlNodePtr *l; XMQNode **n; size_t len, cap; } Pairs;

static void add_pair(Pairs *p, xmlNodePtr l, XMQNode *n)
{
    if (p->len == p->cap)
    {
        p->cap = p->cap ? p->cap * 2 : 256;
        p->l = (xmlNodePtr*)realloc(p->l, p->cap * sizeof(xmlNodePtr));
        p->n = (XMQNode**)realloc(p->n, p->cap * sizeof(XMQNode*));
    }
    p->l[p->len] = l;
    p->n[p->len] = n;
    p->len++;
}

/** Walk both trees, they come from the same bytes. Returns false if they differ in shape. */
static bool collect(Pairs *p, xmlNodePtr l, XMQNode *n)
{
    // Entity references, dtd nodes and xinclude markers are not matched by patterns.
    if (l->type == XML_DTD_NODE) return true;
    add_pair(p, l, n);
    if (l->type == XML_ELEMENT_NODE)
    {
        xmlAttrPtr la = l->properties;
        XMQAttr *na = xml_node_first_attr(n);
        while (la && na)
        {
            add_pair(p, (xmlNodePtr)la, xml_attr_node(na));
            la = la->next;
            na = xml_attr_next(na);
        }
        if (la || na) return false;
    }
    if (l->type == XML_ENTITY_REF_NODE) return true;
    xmlNodePtr lc = l->children;
    XMQNode *nc = xml_node_first_child(n);
    while (lc && nc)
    {
        if (lc->type == XML_DTD_NODE) { lc = lc->next; if (xml_node_kind(nc) == XML_DOM_DTD) nc = xml_node_next(nc); continue; }
        if (!collect(p, lc, nc)) return false;
        lc = lc->next;
        nc = xml_node_next(nc);
    }
    return !lc && !nc;
}

typedef struct { char **items; size_t len, cap; } Strings;

static void add_str(Strings *s, const char *v)
{
    for (size_t i = 0; i < s->len; ++i) if (!strcmp(s->items[i], v)) return;
    if (s->len == s->cap)
    {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->items = (char**)realloc(s->items, s->cap * sizeof(char*));
    }
    s->items[s->len++] = strdup(v);
}

static void free_strs(Strings *s)
{
    for (size_t i = 0; i < s->len; ++i) free(s->items[i]);
    free(s->items);
}

static const char *generic_patterns_[] = {
    "*", "node()", "text()", "comment()", "processing-instruction()", "@*", "/", "/*", "//*", "*/*",
    "*[1]", "*[last()]", "*[position()=2]", "node()[2]", "text()[1]", "text()[last()]", "@*[1]",
    "*[@*]", "*[not(*)]", "*[2][1]", "*[*][1]", "/*/*", "//node()", "id('x')", "id('a1')",
    "processing-instruction('x')", "*|@*", "comment()|text()", "child::*", "attribute::*",
    "child::node()", "@*[.='1']", "*[.='']", "*[position() mod 2 = 0]", "*[last()-1]",
    "node()[last()]", "/descendant::*", "..", ".", "a/../b", "*[", "@", "*//*", "*//node()",
    "/*//text()", "*/@*", "*/text()[2]", "*[3]/*[1]", "node()[1]/node()[1]", "@*/..", "*[position()]",
    "*[$x]", "*[1][last()]", "text()[normalize-space()]", "processing-instruction()[1]",
    "//comment()", "/node()", "/comment()", "child::text()", "attribute::node()", "@node()",
    "child::comment()", "*[@*][2]", "*[count(*)>1]", "*[name()]", "* | text()[1] | @*[last()]",
    "key('k','v')", "*[string-length(.) > 5]", "/*[1]", "//*[1]", "//*[last()]",
    // Predicates that do not compile, the xpath error positions must be like libxml2.
    "*[1 2]", "*[@@]", "*[1 +]", "*[foo(]", "*[$]", "*[(1]", "*[1 2 3]", "*[@*[1]", "*[)]",
    "*[1 = ]", "*[a/]", "*[//]", "*[..x]", "*[@1]", "*[-]", "*[1 and]", "*[concat('a',]",
    "*[child::]", "*[nope::x]", "*[1][2 2]", "*[string(]", "*[.!]", "*[\"x]", "*[1.2.3]",
    NULL
};

static void collect_names(xmlNodePtr n, Strings *elems, Strings *attrs, Strings *prefixes)
{
    for (; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE) continue;
        char b[512];
        if (n->ns && n->ns->prefix) snprintf(b, sizeof(b), "%s:%s", n->ns->prefix, n->name);
        else snprintf(b, sizeof(b), "%s", n->name);
        if (!(n->ns && !n->ns->prefix)) add_str(elems, b); // Unprefixed names in a default namespace never match.
        if (n->ns && n->ns->prefix) add_str(prefixes, (const char*)n->ns->prefix);
        for (xmlAttrPtr a = n->properties; a; a = a->next)
        {
            if (a->ns && a->ns->prefix) snprintf(b, sizeof(b), "%s:%s", a->ns->prefix, a->name);
            else snprintf(b, sizeof(b), "%s", a->name);
            add_str(attrs, b);
        }
        collect_names(n->children, elems, attrs, prefixes);
        if (elems->len > 25) return;
    }
}

static void build_patterns(xmlDocPtr doc, Strings *pats)
{
    for (const char **g = generic_patterns_; *g; ++g) add_str(pats, *g);
    Strings elems = {0}, attrs = {0}, prefixes = {0};
    collect_names(doc->children, &elems, &attrs, &prefixes);
    char b[1200];
    for (size_t i = 0; i < elems.len && i < 25; ++i)
    {
        const char *n = elems.items[i];
        const char *t[] = { "%s", "%s[1]", "%s[2]", "%s[last()]", "*/%s", "%s/*", "//%s", "/%s", "%s//*",
                            "%s/text()", "%s/@*", "%s[@*]", "%s[*][2]", "%s/node()[1]", "%s[.='x']", "/*/%s" };
        for (size_t k = 0; k < sizeof(t) / sizeof(t[0]); ++k)
        {
            snprintf(b, sizeof(b), t[k], n);
            add_str(pats, b);
        }
        if (i + 1 < elems.len)
        {
            snprintf(b, sizeof(b), "%s/%s", n, elems.items[i + 1]);
            add_str(pats, b);
            snprintf(b, sizeof(b), "%s//%s", n, elems.items[i + 1]);
            add_str(pats, b);
            snprintf(b, sizeof(b), "%s | %s[1]", n, elems.items[i + 1]);
            add_str(pats, b);
        }
    }
    for (size_t i = 0; i < attrs.len && i < 15; ++i)
    {
        const char *a = attrs.items[i];
        const char *t[] = { "@%s", "*[@%s]", "@%s[.!='']", "*/@%s", "@%s[1]" };
        for (size_t k = 0; k < sizeof(t) / sizeof(t[0]); ++k)
        {
            snprintf(b, sizeof(b), t[k], a);
            add_str(pats, b);
        }
    }
    for (size_t i = 0; i < prefixes.len; ++i)
    {
        snprintf(b, sizeof(b), "%s:*", prefixes.items[i]);
        add_str(pats, b);
        snprintf(b, sizeof(b), "@%s:*", prefixes.items[i]);
        add_str(pats, b);
    }
    free_strs(&elems);
    free_strs(&attrs);
    free_strs(&prefixes);
}

/** The stylesheet element declaring the in scope namespaces of the document (prefixes only). */
static char *stylesheet_source(xmlDocPtr doc)
{
    Strings decls = {0};
    xmlNodePtr stack[512];
    int sp = 0;
    if (doc->children) stack[sp++] = doc->children;
    while (sp > 0)
    {
        xmlNodePtr n = stack[--sp];
        for (; n; n = n->next)
        {
            if (n->type != XML_ELEMENT_NODE) continue;
            for (xmlNsPtr ns = n->nsDef; ns; ns = ns->next)
            {
                if (!ns->prefix || !ns->href) continue;
                if (!strcmp((const char*)ns->prefix, "xsl")) continue;
                char b[2048];
                // The first declaration of a prefix wins.
                bool dup = false;
                size_t pl = strlen((const char*)ns->prefix);
                for (size_t i = 0; i < decls.len; ++i)
                {
                    if (!strncmp(decls.items[i] + 7, (const char*)ns->prefix, pl) && decls.items[i][7 + pl] == '=') dup = true;
                }
                if (dup) continue;
                // Skip uris that need escaping.
                if (strchr((const char*)ns->href, '"') || strchr((const char*)ns->href, '&') || strchr((const char*)ns->href, '<')) continue;
                snprintf(b, sizeof(b), " xmlns:%s=\"%s\"", ns->prefix, ns->href);
                add_str(&decls, b);
            }
            if (n->children && sp < 512) stack[sp++] = n->children;
        }
    }
    size_t total = 200;
    for (size_t i = 0; i < decls.len; ++i) total += strlen(decls.items[i]);
    char *s = (char*)malloc(total);
    strcpy(s, "<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\"");
    for (size_t i = 0; i < decls.len; ++i) strcat(s, decls.items[i]);
    strcat(s, "/>");
    free_strs(&decls);
    return s;
}

static const char *node_desc(xmlNodePtr n, char *buf, size_t size)
{
    snprintf(buf, size, "%s(%s) line %d", n->type == XML_ELEMENT_NODE ? "element" : n->type == XML_ATTRIBUTE_NODE ? "attribute" :
             n->type == XML_TEXT_NODE ? "text" : n->type == XML_COMMENT_NODE ? "comment" : n->type == XML_PI_NODE ? "pi" :
             n->type == XML_DOCUMENT_NODE ? "document" : "other", n->name ? (const char*)n->name : "", xmlGetLineNo(n));
    return buf;
}

static void test_file(const char *file, const char *buf, size_t len)
{
    xmlDocPtr doc = xmlReadMemory(buf, (int)len, file, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = file;
    XmlParseError err;
    XmlDom *dom = xml_parse_buffer(buf, buf + len, &settings, &err);
    xml_parse_error_free(&err);
    bool has_entities = doc && doc->intSubset && doc->intSubset->entities;
    Pairs pairs = {0};
    if (!doc || !dom || has_entities || !collect(&pairs, (xmlNodePtr)doc, xml_dom_document(dom)))
    {
        skipped_++;
        if (doc) xmlFreeDoc(doc);
        if (dom) xml_dom_free(dom);
        free(pairs.l);
        free(pairs.n);
        return;
    }
    files_++;

    char *ssrc = stylesheet_source(doc);
    xmlDocPtr lsdoc = xmlReadMemory(ssrc, (int)strlen(ssrc), "style.xsl", NULL, 0);
    XmlDom *nsdom = xml_parse_buffer(ssrc, ssrc + strlen(ssrc), NULL, &err);
    xml_parse_error_free(&err);
    if (nsdom) xml_dom_set_url(nsdom, "style.xsl");
    free(ssrc);
    if (!lsdoc || !nsdom)
    {
        skipped_++;
        files_--;
        if (lsdoc) xmlFreeDoc(lsdoc);
        if (nsdom) xml_dom_free(nsdom);
        xmlFreeDoc(doc);
        xml_dom_free(dom);
        free(pairs.l);
        free(pairs.n);
        return;
    }

    Strings pats = {0};
    build_patterns(doc, &pats);
    XmlXsltSettings xs;
    xml_xslt_settings_default(&xs);
    xs.message_callback = capture_native;

    for (size_t pi = 0; pi < pats.len; ++pi)
    {
        const char *pat = pats.items[pi];
        patterns_++;
        xsltStylesheetPtr lstyle = xsltNewStylesheet();
        XmlXsltStylesheet *nstyle = xml_xslt_stylesheet_new_empty(nsdom, &xs);
        lx_msgs_[0] = 0;
        nx_msgs_[0] = 0;
        xsltCompMatchPtr lc = xsltCompilePattern((const xmlChar*)pat, lsdoc, xmlDocGetRootElement(lsdoc), lstyle, NULL);
        XmlXsltPattern *nc = xml_xslt_pattern_compile(nstyle, pat, xml_dom_root_element(nsdom), false);
        if (!strcmp(lx_msgs_, nx_msgs_))
        {
            msg_same_++;
        }
        else
        {
            msg_diff_++;
            if (shown_++ < max_shown_) printf("MESSAGES %s pattern %s\n  libxslt: %s\n  native:  %s\n", file, pat, lx_msgs_, nx_msgs_);
        }
        if (!lc && !nc)
        {
            both_fail_++;
        }
        else if (!lc || !nc)
        {
            if (!lc) libxslt_fail_++;
            else native_fail_++;
            if (shown_++ < max_shown_) printf("%s %s pattern %s\n", !lc ? "ONLY NATIVE COMPILES" : "ONLY LIBXSLT COMPILES", file, pat);
        }
        else
        {
            // xsltCompMatch is private, the priority of the first alternative is its second member.
            float lprio = *(float*)((char*)lc + sizeof(void*));
            if ((double)lprio != xml_xslt_pattern_priority(nc))
            {
                prio_diff_++;
                if (shown_++ < max_shown_) printf("PRIORITY %s pattern %s libxslt %g native %g\n", file, pat, (double)lprio, xml_xslt_pattern_priority(nc));
            }
            xsltTransformContextPtr lctxt = xsltNewTransformContext(lstyle, doc);
            XmlXsltTransform *nt = xml_xslt_transform_new(nstyle, dom);
            long d = 0;
            for (size_t k = 0; k < pairs.len; ++k)
            {
                int a = xsltTestCompMatchList(lctxt, pairs.l[k], lc);
                int b = xml_xslt_pattern_match(nt, pairs.n[k], nc);
                if (a != b)
                {
                    d++;
                    char nb[300];
                    if (shown_++ < max_shown_) printf("DIFF %s pattern %s node %s libxslt %d native %d\n", file, pat, node_desc(pairs.l[k], nb, sizeof(nb)), a, b);
                }
            }
            if (d) diff_++;
            else same_++;
            xml_xslt_transform_free(nt);
            xsltFreeTransformContext(lctxt);
        }
        if (lc) xsltFreeCompMatchList(lc);
        xml_xslt_pattern_free(nc);
        xsltFreeStylesheet(lstyle);
        xml_xslt_stylesheet_free(nstyle);
    }
    free_strs(&pats);
    free(pairs.l);
    free(pairs.n);
    xmlFreeDoc(lsdoc);
    xml_dom_free(nsdom);
    xmlFreeDoc(doc);
    xml_dom_free(dom);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    xmlSetGenericErrorFunc(NULL, capture_lx);
    xmlSetStructuredErrorFunc(NULL, capture_structured);
    xsltSetGenericErrorFunc(NULL, capture_lx);
    long max_size = 200000;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-v")) { verbose_++; continue; }
        if (!strcmp(argv[i], "-n") && i + 1 < argc) { max_shown_ = atoi(argv[++i]); continue; }
        size_t len = 0;
        char *buf = read_file(argv[i], &len);
        if (!buf) continue;
        if ((long)len > max_size)
        {
            skipped_++;
            free(buf);
            continue;
        }
        long before = same_;
        test_file(argv[i], buf, len);
        if (verbose_) printf("pattern [%d/%d] %s: %ld same\n", i, argc - 1, argv[i], same_ - before);
        free(buf);
    }
    xsltCleanupGlobals();
    xmlCleanupParser();
    printf("xml_xslt_pattern_diff: %ld files (%ld skipped), %ld patterns, %ld same, %ld differ, %ld both fail to compile, "
           "%ld only native fails, %ld only libxslt fails, %ld priorities differ\n",
           files_, skipped_, patterns_, same_, diff_, both_fail_, native_fail_, libxslt_fail_, prio_diff_);
    printf("xml_xslt_pattern_diff: compile messages %ld same, %ld differ\n", msg_same_, msg_diff_);
    return (diff_ || native_fail_ || libxslt_fail_ || prio_diff_ || msg_diff_) ? 1 : 0;
}
