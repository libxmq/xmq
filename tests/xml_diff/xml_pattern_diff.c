/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native streamable patterns (parts/xml_pattern.c) against libxml2
   pattern.c (xmlPatterncompile, xmlPatternMatch, xmlStreamPush...).

   Usage: xml_pattern_diff [-v] [-n max_shown] [-p pattern_dir [-r result_dir]] [-g count:seed] files...

   -p dir  Run the libxml2 pattern tests (runtest.c patternTest): every line of dir/X.pat is
           compiled (flags 0, namespaces of the root element of dir/X.xml), X.xml is walked
           with xmlPatternMatch, xmlStreamPush and xmlStreamPop and the matches are printed.
           The native output must be identical to the libxml2 output, and (with -r) the
           libxml2 output must be identical to result_dir/X (checks the emulation).
   -g n:s  Generate n random patterns per document (seed s), default 40:1.

   The files: every .xsd file is first scanned for xs:selector and xs:field xpaths, compiled with
   XML_PATTERN_XSSEL respectively XML_PATTERN_XSFIELD and the in-scope namespaces (like
   xmlschemas.c does). Then every file is a document: parsed with libxml2 and natively, and the
   IDC xpaths plus the generated patterns (compiled with flags 0, XSSEL, XSFIELD and XPATH) are
   compared:
     - compile result, streamable, max/min depth, from root, wants any node,
     - streaming like xmlschemas.c, starting at a number of elements: xmlStreamPush for the
       element, xmlStreamPushAttr followed by xmlStreamPop for every attribute, the children,
       xmlStreamPop; every return value is compared,
     - streaming from the document: xmlStreamPush(NULL, NULL), then all nodes with
       xmlStreamPushNode (text, cdata, comment and pi nodes too), attributes as above,
     - xmlPatternMatch on every node of the libxml2 tree and the native tree (walked in parallel).

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_pattern.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/pattern.h>

#include<dirent.h>
#include<stdarg.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

// libxml2 xmlPatMatch leaks its rollback states (xmlPatPushState) when it returns -1 (".//." after "//").
const char *__lsan_default_suppressions(void);
const char *__lsan_default_suppressions(void)
{
    return "leak:xmlPatPushState\n";
}

static int verbose_ = 0;
static int max_shown_ = 50;
static long shown_ = 0;
static long files_ = 0, skipped_ = 0;
static long compiled_same_ = 0, compiled_fail_same_ = 0, compile_diff_ = 0;
static long push_same_ = 0, push_diff_ = 0;
static long match_same_ = 0, match_diff_ = 0, match_ones_ = 0, push_ones_ = 0;
static long structure_skipped_ = 0;
static long pat_tests_same_ = 0, pat_tests_diff_ = 0, pat_tests_result_diff_ = 0;
static long idc_xpaths_ = 0;

static unsigned long rnd_state_ = 1;
static unsigned rnd(unsigned n)
{
    rnd_state_ = rnd_state_ * 6364136223846793005UL + 1442695040888963407UL;
    return (unsigned)((rnd_state_ >> 33) % n);
}

static bool show()
{
    shown_++;
    return shown_ <= max_shown_;
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

typedef struct { char *b; size_t len, cap; } Buf;

static void buf_add(Buf *b, const char *s)
{
    size_t l = strlen(s);
    if (b->len + l + 1 > b->cap)
    {
        b->cap = (b->len + l + 1) * 2;
        b->b = (char*)realloc(b->b, b->cap);
    }
    memcpy(b->b + b->len, s, l + 1);
    b->len += l;
}

static void buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf_add(b, tmp);
}

// Patterns //////////////////////////////////////////////////////////////

typedef struct
{
    char *pattern;
    int flags;
    char **namespaces; // href, prefix pairs, NULL terminated (copied).
    int nns;
} Pat;

static Pat *idc_ = NULL;
static size_t num_idc_ = 0;

static char **copy_ns(const char **ns, int *nout)
{
    int n = 0;
    while (ns && ns[2 * n]) n++;
    char **r = (char**)calloc(2 * n + 2, sizeof(char*));
    for (int i = 0; i < n; i++)
    {
        r[2 * i] = strdup(ns[2 * i]);
        r[2 * i + 1] = ns[2 * i + 1] ? strdup(ns[2 * i + 1]) : NULL;
    }
    *nout = n;
    return r;
}

static void free_ns(char **ns, int n)
{
    if (!ns) return;
    for (int i = 0; i < 2 * n; i++) free(ns[i]);
    free(ns);
}

static const char *flags_name(int flags)
{
    switch (flags)
    {
    case 0: return "0";
    case XML_PATTERN_XPATH: return "XPATH";
    case XML_PATTERN_XSSEL: return "XSSEL";
    case XML_PATTERN_XSFIELD: return "XSFIELD";
    }
    return "?";
}

/** The ns array as text, for messages. */
static void describe_ns(Buf *b, char **ns, int n)
{
    buf_add(b, "[");
    for (int i = 0; i < n; i++)
    {
        buf_printf(b, "%s%s=%s", i ? " " : "", ns[2 * i + 1] ? ns[2 * i + 1] : "(null)", ns[2 * i]);
    }
    buf_add(b, "]");
}

// Both compiled forms of one pattern.
typedef struct
{
    const Pat *p;
    xmlPatternPtr lx;
    XmlPattern *nx;
} Both;

static void report(const char *file, const Pat *p, const char *what, const char *fmt, ...)
{
    if (!show()) return;
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    Buf b = {0};
    describe_ns(&b, p->namespaces, p->nns);
    printf("DIFF %s %s pattern \"%s\" flags %s ns %s: %s\n", file ? file : "-", what, p->pattern, flags_name(p->flags), b.b, tmp);
    free(b.b);
}

/** Compile with both, compare the compile results and the properties. Returns false if both failed. */
static bool compile_both(const char *file, const Pat *p, Both *out)
{
    out->p = p;
    out->lx = NULL;
    out->nx = NULL;
    int lrc = xmlPatternCompileSafe((const xmlChar*)p->pattern, NULL, p->flags, (const xmlChar**)p->namespaces, &out->lx);
    int nrc = xml_pattern_compile_safe(p->pattern, p->flags, (const char**)p->namespaces, &out->nx);
    if (lrc != nrc || (out->lx == NULL) != (out->nx == NULL))
    {
        compile_diff_++;
        report(file, p, "compile", "libxml2 %d %s native %d %s", lrc, out->lx ? "ok" : "NULL", nrc, out->nx ? "ok" : "NULL");
        return out->lx && out->nx;
    }
    if (!out->lx)
    {
        compiled_fail_same_++;
        return false;
    }
    int l[4] = { xmlPatternStreamable(out->lx), xmlPatternMaxDepth(out->lx), xmlPatternMinDepth(out->lx), xmlPatternFromRoot(out->lx) };
    int n[4] = { xml_pattern_streamable(out->nx), xml_pattern_max_depth(out->nx), xml_pattern_min_depth(out->nx), xml_pattern_from_root(out->nx) };
    if (memcmp(l, n, sizeof(l)))
    {
        compile_diff_++;
        report(file, p, "properties", "streamable/max/min/fromroot libxml2 %d %d %d %d native %d %d %d %d",
               l[0], l[1], l[2], l[3], n[0], n[1], n[2], n[3]);
        return true;
    }
    xmlStreamCtxtPtr ls = xmlPatternGetStreamCtxt(out->lx);
    XmlStreamCtxt *ns = xml_pattern_get_stream_ctxt(out->nx);
    if ((ls == NULL) != (ns == NULL) || (ls && xmlStreamWantsAnyNode(ls) != xml_stream_wants_any_node(ns)))
    {
        compile_diff_++;
        report(file, p, "stream ctxt", "libxml2 %s %d native %s %d", ls ? "ok" : "NULL", ls ? xmlStreamWantsAnyNode(ls) : -9,
               ns ? "ok" : "NULL", ns ? xml_stream_wants_any_node(ns) : -9);
    }
    else
    {
        compiled_same_++;
    }
    if (ls) xmlFreeStreamCtxt(ls);
    if (ns) xml_stream_ctxt_free(ns);
    return true;
}

static void free_both(Both *b)
{
    if (b->lx) xmlFreePattern(b->lx);
    if (b->nx) xml_pattern_free(b->nx);
}

// Streaming /////////////////////////////////////////////////////////////

typedef struct
{
    const char *file;
    Both *both;
    xmlStreamCtxtPtr ls;
    XmlStreamCtxt *ns;
    long budget; // Pushes left for this walk.
    bool failed; // A difference was found, stop this walk.
    bool any;    // Push all node types (xmlStreamPushNode).
} Stream;

static const char *ns_href(xmlNsPtr ns)
{
    return ns ? (const char*)ns->href : NULL;
}

static void compare_push(Stream *s, const char *what, int l, int n, const xmlChar *name)
{
    if (l == n)
    {
        push_same_++;
        if (l == 1) push_ones_++;
        return;
    }
    push_diff_++;
    s->failed = true;
    report(s->file, s->both->p, s->any ? "stream from document" : "stream", "%s %s: libxml2 %d native %d", what, name ? (const char*)name : "(null)", l, n);
}

static void stream_attrs(Stream *s, xmlNodePtr e)
{
    for (xmlAttrPtr a = e->properties; a && !s->failed; a = a->next)
    {
        int l = xmlStreamPushAttr(s->ls, a->name, (const xmlChar*)ns_href(a->ns));
        int n = xml_stream_push_attr(s->ns, (const char*)a->name, ns_href(a->ns));
        compare_push(s, "push attr", l, n, a->name);
        l = xmlStreamPop(s->ls);
        n = xml_stream_pop(s->ns);
        compare_push(s, "pop attr", l, n, a->name);
        s->budget--;
    }
}

static void stream_node(Stream *s, xmlNodePtr node)
{
    if (s->failed || s->budget <= 0) return;
    s->budget--;
    if (node->type == XML_ELEMENT_NODE)
    {
        int l, n;
        if (s->any)
        {
            l = xmlStreamPushNode(s->ls, node->name, (const xmlChar*)ns_href(node->ns), XML_ELEMENT_NODE);
            n = xml_stream_push_node(s->ns, (const char*)node->name, ns_href(node->ns), XML_DOM_ELEMENT);
        }
        else
        {
            l = xmlStreamPush(s->ls, node->name, (const xmlChar*)ns_href(node->ns));
            n = xml_stream_push(s->ns, (const char*)node->name, ns_href(node->ns));
        }
        compare_push(s, "push", l, n, node->name);
        stream_attrs(s, node);
        for (xmlNodePtr c = node->children; c && !s->failed; c = c->next) stream_node(s, c);
        l = xmlStreamPop(s->ls);
        n = xml_stream_pop(s->ns);
        compare_push(s, "pop", l, n, node->name);
        return;
    }
    if (!s->any) return;
    if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE ||
        node->type == XML_COMMENT_NODE || node->type == XML_PI_NODE)
    {
        // Like xpath.c xmlXPathRunStreamEval: name NULL except for pis, then pushed and popped.
        const xmlChar *name = node->type == XML_PI_NODE ? node->name : NULL;
        int l = xmlStreamPushNode(s->ls, name, NULL, node->type);
        int n = xml_stream_push_node(s->ns, (const char*)name, NULL, (int)node->type);
        compare_push(s, "push node", l, n, name);
        l = xmlStreamPop(s->ls);
        n = xml_stream_pop(s->ns);
        compare_push(s, "pop node", l, n, name);
    }
}

static void stream_both(const char *file, Both *b, xmlDocPtr doc)
{
    if (!b->lx || !b->nx) return;
    Stream s;
    memset(&s, 0, sizeof(s));
    s.file = file;
    s.both = b;
    // From the document, all node types.
    s.ls = xmlPatternGetStreamCtxt(b->lx);
    s.ns = xml_pattern_get_stream_ctxt(b->nx);
    if (!s.ls || !s.ns)
    {
        if (s.ls) xmlFreeStreamCtxt(s.ls);
        if (s.ns) xml_stream_ctxt_free(s.ns);
        return;
    }
    s.any = true;
    s.budget = 20000;
    compare_push(&s, "push document", xmlStreamPush(s.ls, NULL, NULL), xml_stream_push(s.ns, NULL, NULL), NULL);
    for (xmlNodePtr c = doc->children; c && !s.failed; c = c->next) stream_node(&s, c);
    xmlFreeStreamCtxt(s.ls);
    xml_stream_ctxt_free(s.ns);
    // Like xmlschemas.c: a new context at the element of the identity constraint (here the
    // first elements of the document), only elements and attributes.
    int starts = 0;
    xmlNodePtr e = xmlDocGetRootElement(doc);
    while (e && starts < 12)
    {
        if (e->type == XML_ELEMENT_NODE)
        {
            starts++;
            s.ls = xmlPatternGetStreamCtxt(b->lx);
            s.ns = xml_pattern_get_stream_ctxt(b->nx);
            s.any = false;
            s.failed = false;
            s.budget = 4000;
            stream_node(&s, e);
            xmlFreeStreamCtxt(s.ls);
            xml_stream_ctxt_free(s.ns);
        }
        // Next element in document order.
        if (e->children && e->type == XML_ELEMENT_NODE) e = e->children;
        else
        {
            while (e && !e->next) e = e->parent;
            if (e) e = e->next;
            if (e && e->type == XML_DOCUMENT_NODE) e = NULL;
        }
    }
}

// Matching //////////////////////////////////////////////////////////////

static bool same_structure(xmlNodePtr l, XMQNode *n);

static int lx_kind(xmlElementType t)
{
    return (int)t;
}

/** The children (and attributes) of the libxml2 and native nodes correspond. */
static bool same_structure(xmlNodePtr l, XMQNode *n)
{
    if (lx_kind(l->type) != (int)xml_node_kind(n)) return false;
    if (l->type == XML_ELEMENT_NODE)
    {
        if (strcmp((const char*)l->name, xml_node_name(n))) return false;
        XMQAttr *na = xml_node_first_attr(n);
        for (xmlAttrPtr a = l->properties; a; a = a->next, na = xml_attr_next(na))
        {
            if (!na || strcmp((const char*)a->name, xml_attr_name(na))) return false;
        }
        if (na) return false;
    }
    if (l->type == XML_ENTITY_REF_NODE || l->type == XML_DTD_NODE) return true;
    xmlNodePtr lc = l->children;
    XMQNode *nc = xml_node_first_child(n);
    while (lc && lc->type == XML_DTD_NODE) lc = lc->next;
    while (nc && xml_node_kind(nc) == XML_DOM_DTD) nc = xml_node_next(nc);
    while (lc && nc)
    {
        if (!same_structure(lc, nc)) return false;
        lc = lc->next;
        nc = xml_node_next(nc);
        while (lc && lc->type == XML_DTD_NODE) lc = lc->next;
        while (nc && xml_node_kind(nc) == XML_DOM_DTD) nc = xml_node_next(nc);
    }
    return !lc && !nc;
}

static void match_one(const char *file, Both *b, xmlNodePtr l, XMQNode *n, bool *failed)
{
    int lr = xmlPatternMatch(b->lx, l);
    int nr = xml_pattern_match(b->nx, n);
    if (lr == nr)
    {
        match_same_++;
        if (lr == 1) match_ones_++;
        return;
    }
    match_diff_++;
    *failed = true;
    xmlChar *path = xmlGetNodePath(l);
    report(file, b->p, "match", "node %s (type %d): libxml2 %d native %d", path ? (const char*)path : "?", (int)l->type, lr, nr);
    xmlFree(path);
}

static void match_tree(const char *file, Both *b, xmlNodePtr l, XMQNode *n, bool *failed)
{
    if (*failed) return;
    match_one(file, b, l, n, failed);
    if (l->type == XML_ELEMENT_NODE)
    {
        XMQAttr *na = xml_node_first_attr(n);
        for (xmlAttrPtr a = l->properties; a && na && !*failed; a = a->next, na = xml_attr_next(na))
        {
            match_one(file, b, (xmlNodePtr)a, xml_attr_node(na), failed);
        }
    }
    if (l->type == XML_ENTITY_REF_NODE || l->type == XML_DTD_NODE) return;
    xmlNodePtr lc = l->children;
    XMQNode *nc = xml_node_first_child(n);
    while (lc && nc && !*failed)
    {
        match_tree(file, b, lc, nc, failed);
        lc = lc->next;
        nc = xml_node_next(nc);
    }
}

// Documents /////////////////////////////////////////////////////////////

static const char *gen_names_[64];
static int num_gen_names_ = 0;
static const char *gen_attrs_[64];
static int num_gen_attrs_ = 0;
static const char *gen_prefixes_[32];
static int num_gen_prefixes_ = 0;

static void add_unique(const char **arr, int *num, int max, const char *s)
{
    if (!s) return;
    for (int i = 0; i < *num; i++) if (!strcmp(arr[i], s)) return;
    if (*num < max) arr[(*num)++] = s;
}

static void collect_names(xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE) continue;
        add_unique(gen_names_, &num_gen_names_, 64, (const char*)node->name);
        if (node->ns && node->ns->prefix) add_unique(gen_prefixes_, &num_gen_prefixes_, 32, (const char*)node->ns->prefix);
        for (xmlAttrPtr a = node->properties; a; a = a->next)
        {
            add_unique(gen_attrs_, &num_gen_attrs_, 64, (const char*)a->name);
            if (a->ns && a->ns->prefix) add_unique(gen_prefixes_, &num_gen_prefixes_, 32, (const char*)a->ns->prefix);
        }
        for (xmlNsPtr ns = node->nsDef; ns; ns = ns->next)
        {
            if (ns->prefix) add_unique(gen_prefixes_, &num_gen_prefixes_, 32, (const char*)ns->prefix);
        }
        collect_names(node->children);
    }
}

static const char *pick(const char **arr, int num, const char *fallback)
{
    if (num == 0 || rnd(5) == 0) return fallback;
    return arr[rnd(num)];
}

static void gen_name(Buf *b, bool attr)
{
    static const char *generic[] = { "a", "b", "c", "x", "doc", "item", "\xc3\xa9t\xc3\xa9", "_u", "a-b", "a.b" };
    const char *n = attr ? pick(gen_attrs_, num_gen_attrs_, generic[rnd(10)]) : pick(gen_names_, num_gen_names_, generic[rnd(10)]);
    buf_add(b, n);
}

static void gen_prefix(Buf *b)
{
    static const char *generic[] = { "xml", "g", "zz", "xs" };
    buf_add(b, pick(gen_prefixes_, num_gen_prefixes_, generic[rnd(4)]));
}

static void gen_blank(Buf *b)
{
    if (rnd(12) == 0) buf_add(b, rnd(2) ? " " : "\t");
}

static void gen_step(Buf *b)
{
    gen_blank(b);
    switch (rnd(16))
    {
    case 0: buf_add(b, "."); break;
    case 1: buf_add(b, "*"); break;
    case 2: gen_prefix(b); buf_add(b, ":*"); break;
    case 3: gen_prefix(b); buf_add(b, ":"); gen_name(b, false); break;
    case 4: buf_add(b, "@"); gen_name(b, true); break;
    case 5: buf_add(b, "@"); gen_prefix(b); buf_add(b, ":"); gen_name(b, true); break;
    case 6: buf_add(b, "@*"); break;
    case 7: buf_add(b, "child::"); if (rnd(3) == 0) buf_add(b, "*"); else gen_name(b, false); break;
    case 8: buf_add(b, "child::"); gen_prefix(b); buf_add(b, rnd(3) ? ":" : ":*"); if (b->b[b->len - 1] == ':') gen_name(b, false); break;
    case 9: buf_add(b, "attribute::"); if (rnd(3) == 0) buf_add(b, "*"); else gen_name(b, true); break;
    case 10: buf_add(b, "attribute::"); gen_prefix(b); buf_add(b, ":"); gen_name(b, true); break;
    case 11:
    {
        static const char *odd[] = { "self::a", "1a", "a:", ":a", "a :b", "a: b", "@", "@ a", "child:: a", "child::", "attribute::",
                                     "a::b", "a*", "*a", "..", "@a:*", "@xml:lang", "xml:*", "a[1]", "text()", "-", "" };
        buf_add(b, odd[rnd(sizeof(odd) / sizeof(odd[0]))]);
        break;
    }
    default: gen_name(b, false); break;
    }
    gen_blank(b);
}

static void gen_path(Buf *b)
{
    gen_blank(b);
    switch (rnd(10))
    {
    case 0: buf_add(b, "/"); break;
    case 1: buf_add(b, "//"); break;
    case 2: buf_add(b, ".//"); break;
    case 3: buf_add(b, "./"); break;
    case 4: buf_add(b, rnd(2) ? "./ /" : ". //"); break;
    default: break;
    }
    int steps = 1 + rnd(4);
    for (int i = 0; i < steps; i++)
    {
        if (i) buf_add(b, rnd(4) == 0 ? "//" : (rnd(15) == 0 ? "///" : "/"));
        gen_step(b);
    }
    if (rnd(20) == 0) buf_add(b, rnd(2) ? "/" : "//");
}

static char *gen_pattern()
{
    Buf b = {0};
    buf_add(&b, "");
    int alts = rnd(5) == 0 ? 2 + rnd(2) : 1;
    for (int i = 0; i < alts; i++)
    {
        if (i) buf_add(&b, rnd(10) ? "|" : "||");
        gen_path(&b);
    }
    return b.b;
}

static long gen_count_ = 40;

static void test_pattern_on_doc(const char *file, const Pat *p, xmlDocPtr doc, XmlDom *dom, bool structure)
{
    Both b;
    if (compile_both(file, p, &b))
    {
        stream_both(file, &b, doc);
        if (structure)
        {
            bool failed = false;
            match_tree(file, &b, (xmlNodePtr)doc, xml_dom_document(dom), &failed);
        }
    }
    free_both(&b);
}

static void test_doc(const char *file, xmlDocPtr doc, XmlDom *dom)
{
    bool structure = same_structure((xmlNodePtr)doc, xml_dom_document(dom));
    if (!structure) structure_skipped_++;

    for (size_t i = 0; i < num_idc_; i++)
    {
        test_pattern_on_doc(file, &idc_[i], doc, dom, structure);
    }

    // Generated patterns, with the in-scope namespaces of the root element and a fixed one.
    xmlNodePtr root = xmlDocGetRootElement(doc);
    num_gen_names_ = num_gen_attrs_ = num_gen_prefixes_ = 0;
    collect_names(root);
    const char *nsarr[2 * 40 + 4];
    int k = 0;
    xmlNsPtr *list = root ? xmlGetNsList(doc, root) : NULL;
    for (int i = 0; list && list[i] && k < 2 * 38; i++)
    {
        nsarr[k++] = (const char*)list[i]->href;
        nsarr[k++] = (const char*)list[i]->prefix;
    }
    nsarr[k++] = "urn:gen";
    nsarr[k++] = "g";
    nsarr[k++] = NULL;
    nsarr[k] = NULL;
    static const int flags[] = { 0, XML_PATTERN_XSSEL, XML_PATTERN_XSFIELD, XML_PATTERN_XPATH };
    for (long g = 0; g < gen_count_; g++)
    {
        Pat p;
        p.pattern = gen_pattern();
        p.namespaces = copy_ns(nsarr, &p.nns);
        for (int f = 0; f < 4; f++)
        {
            p.flags = flags[f];
            test_pattern_on_doc(file, &p, doc, dom, structure);
        }
        free(p.pattern);
        free_ns(p.namespaces, p.nns);
    }
    xmlFree(list);
}

static bool parse_both(const char *file, xmlDocPtr *doc, XmlDom **dom)
{
    size_t len = 0;
    char *buf = read_file(file, &len);
    *doc = NULL;
    *dom = NULL;
    if (!buf) return false;
    *doc = xmlReadMemory(buf, (int)len, file, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = file;
    XmlParseError err;
    memset(&err, 0, sizeof(err));
    *dom = xml_parse_buffer(buf, buf + len, &settings, &err);
    xml_parse_error_free(&err);
    free(buf);
    if (*doc && *dom && xmlDocGetRootElement(*doc)) return true;
    if (*doc) xmlFreeDoc(*doc);
    if (*dom) xml_dom_free(*dom);
    *doc = NULL;
    *dom = NULL;
    return false;
}

// Schemas ///////////////////////////////////////////////////////////////

#define XSD_NS "http://www.w3.org/2001/XMLSchema"

static void collect_idc(xmlDocPtr doc, xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE) continue;
        if (node->ns && !strcmp((const char*)node->ns->href, XSD_NS) &&
            (!strcmp((const char*)node->name, "selector") || !strcmp((const char*)node->name, "field")))
        {
            xmlChar *xpath = xmlGetNoNsProp(node, (const xmlChar*)"xpath");
            if (xpath)
            {
                // Like xmlSchemaCheckCSelectorXPath: the in-scope namespaces of the element.
                xmlNsPtr *list = xmlGetNsList(doc, node);
                int count = 0;
                while (list && list[count]) count++;
                const char **arr = (const char**)calloc(2 * count + 2, sizeof(char*));
                for (int i = 0; i < count; i++)
                {
                    arr[2 * i] = (const char*)list[i]->href;
                    arr[2 * i + 1] = (const char*)list[i]->prefix;
                }
                // Each xpath with its own flag and the other flags too.
                int own = !strcmp((const char*)node->name, "field") ? XML_PATTERN_XSFIELD : XML_PATTERN_XSSEL;
                int fl[3] = { own, own == XML_PATTERN_XSSEL ? XML_PATTERN_XSFIELD : XML_PATTERN_XSSEL, 0 };
                for (int f = 0; f < 3; f++)
                {
                    idc_ = (Pat*)realloc(idc_, (num_idc_ + 1) * sizeof(Pat));
                    Pat *p = &idc_[num_idc_++];
                    p->pattern = strdup((const char*)xpath);
                    p->flags = fl[f];
                    p->namespaces = copy_ns(arr, &p->nns);
                }
                idc_xpaths_++;
                free(arr);
                xmlFree(list);
                xmlFree(xpath);
            }
        }
        collect_idc(doc, node->children);
    }
}

// Pattern tests (runtest.c patternTest) //////////////////////////////////

/** Port of the element branch of xmlGetNodePath for the native dom. */
static void nx_node_path(Buf *b, XMQNode *node)
{
    if (!node || xml_node_kind(node) == XML_DOM_DOCUMENT) return;
    nx_node_path(b, xml_node_parent(node));
    int generic = 0;
    buf_add(b, "/");
    XmlNs *ns = xml_node_ns(node);
    if (ns)
    {
        if (xml_ns_prefix(ns))
        {
            buf_add(b, xml_ns_prefix(ns));
            buf_add(b, ":");
            buf_add(b, xml_node_name(node));
        }
        else
        {
            generic = 1;
            buf_add(b, "*");
        }
    }
    else buf_add(b, xml_node_name(node));
    int occur = 0;
#define NX_SAME(tmp) (xml_node_kind(tmp) == XML_DOM_ELEMENT && (generic || \
        (!strcmp(xml_node_name(node), xml_node_name(tmp)) && \
         ((xml_node_ns(tmp) == ns) || (xml_node_ns(tmp) && ns && \
           ((xml_ns_prefix(xml_node_ns(tmp)) == NULL && xml_ns_prefix(ns) == NULL) || \
            (xml_ns_prefix(xml_node_ns(tmp)) && xml_ns_prefix(ns) && !strcmp(xml_ns_prefix(xml_node_ns(tmp)), xml_ns_prefix(ns)))))))))
    for (XMQNode *tmp = xml_node_prev(node); tmp; tmp = xml_node_prev(tmp)) if (NX_SAME(tmp)) occur++;
    if (occur == 0)
    {
        for (XMQNode *tmp = xml_node_next(node); tmp && occur == 0; tmp = xml_node_next(tmp)) if (NX_SAME(tmp)) occur++;
        if (occur != 0) occur = 1;
    }
    else occur++;
#undef NX_SAME
    if (occur) buf_printf(b, "[%d]", occur);
}

static void lx_pattern_walk(Buf *out, const char *pattern, xmlPatternPtr pc, xmlStreamCtxtPtr *ps, xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE) continue;
        int match = xmlPatternMatch(pc, node);
        xmlChar *path = NULL;
        if (match)
        {
            path = xmlGetNodePath(node);
            buf_printf(out, "Node %s matches pattern %s\n", path, pattern);
        }
        if (*ps)
        {
            int ret = xmlStreamPush(*ps, node->name, node->ns ? node->ns->href : NULL);
            if (ret < 0)
            {
                buf_add(out, "xmlStreamPush() failure\n");
                xmlFreeStreamCtxt(*ps);
                *ps = NULL;
            }
            else if (ret != match)
            {
                if (!path) path = xmlGetNodePath(node);
                buf_add(out, "xmlPatternMatch and xmlStreamPush disagree\n");
                buf_printf(out, "  pattern %s node %s\n", pattern, path);
            }
        }
        xmlFree(path);
        lx_pattern_walk(out, pattern, pc, ps, node->children);
        if (*ps && xmlStreamPop(*ps) < 0)
        {
            buf_add(out, "xmlStreamPop() failure\n");
            xmlFreeStreamCtxt(*ps);
            *ps = NULL;
        }
    }
}

static void nx_pattern_walk(Buf *out, const char *pattern, XmlPattern *pc, XmlStreamCtxt **ps, XMQNode *node)
{
    for (; node; node = xml_node_next(node))
    {
        if (xml_node_kind(node) != XML_DOM_ELEMENT) continue;
        int match = xml_pattern_match(pc, node);
        Buf path = {0};
        if (match)
        {
            nx_node_path(&path, node);
            buf_printf(out, "Node %s matches pattern %s\n", path.b, pattern);
        }
        if (*ps)
        {
            int ret = xml_stream_push(*ps, xml_node_name(node), xml_node_ns_uri(node));
            if (ret < 0)
            {
                buf_add(out, "xmlStreamPush() failure\n");
                xml_stream_ctxt_free(*ps);
                *ps = NULL;
            }
            else if (ret != match)
            {
                if (!path.b) nx_node_path(&path, node);
                buf_add(out, "xmlPatternMatch and xmlStreamPush disagree\n");
                buf_printf(out, "  pattern %s node %s\n", pattern, path.b);
            }
        }
        free(path.b);
        nx_pattern_walk(out, pattern, pc, ps, xml_node_first_child(node));
        if (*ps && xml_stream_pop(*ps) < 0)
        {
            buf_add(out, "xmlStreamPop() failure\n");
            xml_stream_ctxt_free(*ps);
            *ps = NULL;
        }
    }
}

static void pattern_test(const char *patfile, const char *result_dir)
{
    char xml[1024];
    snprintf(xml, sizeof(xml), "%.*s.xml", (int)(strlen(patfile) - 4), patfile);
    FILE *f = fopen(patfile, "rb");
    if (!f) return;
    xmlDocPtr doc;
    XmlDom *dom;
    if (!parse_both(xml, &doc, &dom))
    {
        printf("FAILED to parse %s\n", xml);
        pat_tests_diff_++;
        fclose(f);
        return;
    }
    Buf lo = {0}, no = {0};
    buf_add(&lo, "");
    buf_add(&no, "");
    char str[1024];
    while (fgets(str, sizeof(str) - 1, f))
    {
        int i = (int)strlen(str);
        while (i > 0 && (str[i - 1] == '\n' || str[i - 1] == '\r' || str[i - 1] == ' ' || str[i - 1] == '\t')) str[--i] = 0;

        // libxml2
        {
            xmlNodePtr root = xmlDocGetRootElement(doc);
            const xmlChar *namespaces[22];
            int j = 0;
            for (xmlNsPtr ns = root->nsDef; ns && j < 20; ns = ns->next)
            {
                namespaces[j++] = ns->href;
                namespaces[j++] = ns->prefix;
            }
            namespaces[j++] = NULL;
            namespaces[j] = NULL;
            xmlPatternPtr pc = xmlPatterncompile((const xmlChar*)str, doc->dict, 0, namespaces);
            if (!pc)
            {
                buf_printf(&lo, "Pattern %s failed to compile\n", str);
            }
            else
            {
                xmlStreamCtxtPtr ps = xmlPatternGetStreamCtxt(pc);
                if (ps && xmlStreamPush(ps, NULL, NULL) < 0)
                {
                    buf_add(&lo, "xmlStreamPush() failure\n");
                    xmlFreeStreamCtxt(ps);
                    ps = NULL;
                }
                lx_pattern_walk(&lo, str, pc, &ps, doc->children);
                if (ps) xmlFreeStreamCtxt(ps);
                xmlFreePattern(pc);
            }
        }
        // native
        {
            XMQNode *root = xml_dom_root_element(dom);
            const char *namespaces[22];
            int j = 0;
            for (XmlNs *ns = xml_node_first_ns_decl(root); ns && j < 20; ns = xml_ns_next_decl(ns))
            {
                namespaces[j++] = xml_ns_href(ns);
                namespaces[j++] = xml_ns_prefix(ns);
            }
            namespaces[j++] = NULL;
            namespaces[j] = NULL;
            XmlPattern *pc = xml_pattern_compile(str, 0, namespaces);
            if (!pc)
            {
                buf_printf(&no, "Pattern %s failed to compile\n", str);
            }
            else
            {
                XmlStreamCtxt *ps = xml_pattern_get_stream_ctxt(pc);
                if (ps && xml_stream_push(ps, NULL, NULL) < 0)
                {
                    buf_add(&no, "xmlStreamPush() failure\n");
                    xml_stream_ctxt_free(ps);
                    ps = NULL;
                }
                nx_pattern_walk(&no, str, pc, &ps, xml_node_first_child(xml_dom_document(dom)));
                if (ps) xml_stream_ctxt_free(ps);
                xml_pattern_free(pc);
            }
        }
    }
    fclose(f);
    if (!strcmp(lo.b, no.b))
    {
        pat_tests_same_++;
        if (verbose_) printf("pattern test %s: same\n", patfile);
    }
    else
    {
        pat_tests_diff_++;
        printf("DIFF pattern test %s\n--- libxml2\n%s--- native\n%s", patfile, lo.b, no.b);
    }
    if (result_dir)
    {
        const char *base = strrchr(xml, '/');
        base = base ? base + 1 : xml;
        char res[1200];
        snprintf(res, sizeof(res), "%s/%.*s", result_dir, (int)(strlen(base) - 4), base);
        size_t len;
        char *expected = read_file(res, &len);
        // runtest prints the compile failures to stderr, they are not in the result files.
        Buf filtered = {0};
        buf_add(&filtered, "");
        char *line = lo.b;
        while (line && *line)
        {
            char *nl = strchr(line, '\n');
            size_t ll = nl ? (size_t)(nl - line + 1) : strlen(line);
            if (strncmp(line, "Pattern ", 8))
            {
                char tmp[2048];
                snprintf(tmp, sizeof(tmp), "%.*s", (int)ll, line);
                buf_add(&filtered, tmp);
            }
            line += ll;
        }
        if (!expected || strcmp(expected, filtered.b))
        {
            pat_tests_result_diff_++;
            printf("DIFF pattern test %s: libxml2 output differs from %s\n%s", patfile, res, filtered.b);
        }
        free(expected);
        free(filtered.b);
    }
    free(lo.b);
    free(no.b);
    xmlFreeDoc(doc);
    xml_dom_free(dom);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static void pattern_tests(const char *dir, const char *result_dir)
{
    DIR *d = opendir(dir);
    if (!d) { printf("Cannot open %s\n", dir); pat_tests_diff_++; return; }
    char *names[256];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 256)
    {
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcmp(e->d_name + l - 4, ".pat")) names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char*), cmp_str);
    for (int i = 0; i < n; i++)
    {
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        pattern_test(path, result_dir);
        free(names[i]);
    }
}

int main(int argc, char **argv)
{
    // Line buffered, so that the progress can be followed in a log file.
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *pattern_dir = NULL, *result_dir = NULL;
    int first_file = argc;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-v")) { verbose_++; continue; }
        if (!strcmp(argv[i], "-n") && i + 1 < argc) { max_shown_ = atoi(argv[++i]); continue; }
        if (!strcmp(argv[i], "-p") && i + 1 < argc) { pattern_dir = argv[++i]; continue; }
        if (!strcmp(argv[i], "-r") && i + 1 < argc) { result_dir = argv[++i]; continue; }
        if (!strcmp(argv[i], "-g") && i + 1 < argc)
        {
            unsigned long seed = 1;
            sscanf(argv[++i], "%ld:%lu", &gen_count_, &seed);
            rnd_state_ = seed;
            continue;
        }
        first_file = i;
        break;
    }
    if (pattern_dir) pattern_tests(pattern_dir, result_dir);

    // First the identity constraint xpaths of all schemas.
    for (int i = first_file; i < argc; ++i)
    {
        size_t l = strlen(argv[i]);
        if (l < 4 || strcmp(argv[i] + l - 4, ".xsd")) continue;
        xmlDocPtr doc = xmlReadFile(argv[i], NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        if (!doc) continue;
        collect_idc(doc, doc->children);
        xmlFreeDoc(doc);
    }
    if (verbose_) printf("%zu identity constraint patterns\n", num_idc_);

    for (int i = first_file; i < argc; ++i)
    {
        xmlDocPtr doc;
        XmlDom *dom;
        if (!parse_both(argv[i], &doc, &dom))
        {
            skipped_++;
            continue;
        }
        files_++;
        long before = push_same_ + match_same_;
        test_doc(argv[i], doc, dom);
        if (verbose_) printf("pattern [%d/%d] %s: %ld same\n", i - first_file + 1, argc - first_file, argv[i], push_same_ + match_same_ - before);
        xmlFreeDoc(doc);
        xml_dom_free(dom);
    }
    for (size_t i = 0; i < num_idc_; i++)
    {
        free(idc_[i].pattern);
        free_ns(idc_[i].namespaces, idc_[i].nns);
    }
    free(idc_);
    xmlCleanupParser();
    printf("xml_pattern_diff: pattern tests %ld same %ld differ (%ld differ from the libxml2 results)\n",
           pat_tests_same_, pat_tests_diff_, pat_tests_result_diff_);
    printf("xml_pattern_diff: %ld files (%ld skipped, %ld without corresponding trees for match), %ld idc xpaths, "
           "compiled %ld same (%ld both fail) %ld differ, pushes %ld same (%ld matches) %ld differ, "
           "matches %ld same (%ld matches) %ld differ\n",
           files_, skipped_, structure_skipped_, idc_xpaths_, compiled_same_, compiled_fail_same_, compile_diff_,
           push_same_, push_ones_, push_diff_, match_same_, match_ones_, match_diff_);
    return (pat_tests_diff_ || pat_tests_result_diff_ || compile_diff_ || push_diff_ || match_diff_) ? 1 : 0;
}
