/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
   Differential test of the shared part of the native XML Schema 1.0 structures
   (parts/xml_xsd10_schemas.c) against libxml2 xmlschemas.c.

   libxml2's static functions are reached by including 3rdparty/libxml2/xmlschemas.c
   into this file (its public functions then replace the ones of libxml2.a).

   Compared with random inputs:
   - the dict (interning, qlookup) and the hash tables (add/update/lookup/remove/size
     results against libxml2 xmlHash, the scan visits the live entries in insertion order),
   - the tree helpers (xmlValidateNCName/QName, xmlSplitQName2/3, xmlSearchNs,
     xmlSearchNsByHref, xmlGetNsList, xmlGetNoNsProp, xmlHasProp, xmlNodeGetContent,
     xmlNodeGetBase, xmlNodeGetSpacePreserve, xmlGetLineNo),
   - the message formatting: xmlSchemaFormatQName, xmlSchemaFormatItemForReport,
     xmlSchemaGetComponentDesignation, xmlSchemaFormatNodeForError,
     xmlSchemaGetCanonValueWhtspExt/Hash,
   - every error function (xmlSchemaPErr*, xmlSchemaErr*, xmlSchemaCustomErr*,
     xmlSchemaSimpleTypeErr, xmlSchemaComplexTypeErr, xmlSchemaFacetErr,
     xmlSchemaIllegalAttrErr, xmlSchemaKeyrefErr, xmlSchemaPMissingAttrErr, ...) on
     parser and validation contexts, with the nodes of the documents given on the command
     line (normally 3rdparty/libxml2/test/schemas/ *.xsd, parsed by both): the plain
     callbacks must get the same text, the structured callbacks the same fields and the
     same xmlFormatError text, and the contexts the same error counts.

   Usage: xml_xsd10_schemas_diff [-v] [-r count:seed] files...
   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"xmlschemas.c"

#define XML_XSD10_INTERNALS
#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
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

/* libxml2 xmlSchemaFormatItemForReport leaks the designation of a notation when it also
   gets a node (it is overwritten by the "Element '...'" text) and the first normalized
   value of normalizedString values with tab/cr/nl (xmlSchemaValAtomicType). */
const char *__lsan_default_suppressions(void);
const char *__lsan_default_suppressions(void)
{
    return "leak:xmlSchemaFormatItemForReport\nleak:xmlSchemaValAtomicType\n";
}

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

// String pool, freed at the end.
static char **pool_ = NULL;
static size_t pool_len_ = 0, pool_max_ = 0;
static char *keep(char *s)
{
    if (pool_len_ == pool_max_)
    {
        pool_max_ = pool_max_ ? pool_max_ * 2 : 256;
        pool_ = (char **) realloc(pool_, pool_max_ * sizeof(char *));
    }
    pool_[pool_len_++] = s;
    return s;
}
static void free_pool(void)
{
    for (size_t i = 0; i < pool_len_; i++) free(pool_[i]);
    free(pool_);
    pool_ = NULL;
    pool_len_ = pool_max_ = 0;
}

// A random string, NULL sometimes. percent allows '%' characters.
static const char *rnd_str(int null_chance, int percent)
{
    static const char alpha[] = "abcxyzXYZ_-.:0 ";
    if (null_chance && rnd() % null_chance == 0) return NULL;
    int len = rnd() % 9;
    char *s = (char *) malloc(len + 1);
    for (int i = 0; i < len; i++)
    {
        if (percent && rnd() % 6 == 0) s[i] = '%';
        else s[i] = alpha[rnd() % (sizeof(alpha) - 1)];
    }
    s[len] = 0;
    return keep(s);
}

static const char *rnd_ns(void)
{
    if (rnd() % 3 == 0) return NULL;
    char buf[64];
    snprintf(buf, sizeof(buf), "http://example.com/%u", rnd() % 5);
    return keep(strdup(buf));
}

// Capturing //////////////////////////////////////////////////////

typedef struct { char *s; } Cap;
static Cap lcap_, ncap_;

static void cap_add(Cap *c, const char *text)
{
    size_t a = c->s ? strlen(c->s) : 0, b = strlen(text);
    c->s = (char *) realloc(c->s, a + b + 1);
    memcpy(c->s + a, text, b + 1);
}

static void cap_vadd(Cap *c, const char *prefix, const char *fmt, va_list ap)
{
    char buf[70000];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    cap_add(c, prefix);
    cap_add(c, buf);
}

static void l_err(void *ctx, const char *msg, ...) { va_list ap; va_start(ap, msg); cap_vadd(&lcap_, ctx ? (const char *) ctx : "", msg, ap); va_end(ap); }
static void n_err(void *ctx, const char *msg, ...) { va_list ap; va_start(ap, msg); cap_vadd(&ncap_, ctx ? (const char *) ctx : "", msg, ap); va_end(ap); }
static void l_quiet(void *ctx, const char *msg, ...) { (void) ctx; (void) msg; }
static void l_fmt(void *ctx, const char *msg, ...) { va_list ap; va_start(ap, msg); cap_vadd((Cap *) ctx, "", msg, ap); va_end(ap); }

// The nodes of the documents, libxml2 and native in parallel.
static xmlNodePtr *lnodes_ = NULL;
static XMQNode **nnodes_ = NULL;
static size_t nodes_len_ = 0, nodes_max_ = 0;

static long l_node_index(const void *p)
{
    if (!p) return -1;
    for (size_t i = 0; i < nodes_len_; i++) if ((const void *) lnodes_[i] == p) return (long) i;
    return -2;
}

static long n_node_index(const void *p)
{
    if (!p) return -1;
    for (size_t i = 0; i < nodes_len_; i++) if ((const void *) nnodes_[i] == p) return (long) i;
    return -2;
}

static void l_serr(void *ctx, const xmlError *e)
{
    char buf[1000];
    Cap f = { NULL };
    xmlFormatError(e, l_fmt, &f);
    snprintf(buf, sizeof(buf), "%s[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%ld] ",
             ctx ? (const char *) ctx : "", e->domain, e->code, (int) e->level, e->file ? e->file : "(NULL)", e->line,
             e->str1 ? e->str1 : "(NULL)", e->str2 ? e->str2 : "(NULL)", e->str3 ? e->str3 : "(NULL)",
             e->int1, e->int2, l_node_index(e->node));
    cap_add(&lcap_, buf);
    cap_add(&lcap_, e->message ? e->message : "(NULL)");
    cap_add(&lcap_, "|FORMATTED|");
    cap_add(&lcap_, f.s ? f.s : "");
    free(f.s);
}

static void n_serr(void *ctx, const XmlXsd10Error *e)
{
    char buf[1000];
    char *f = xml_xsd10_format_error_string(e);
    snprintf(buf, sizeof(buf), "%s[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%ld] ",
             ctx ? (const char *) ctx : "", e->domain, e->code, (int) e->level, e->file ? e->file : "(NULL)", e->line,
             e->str1 ? e->str1 : "(NULL)", e->str2 ? e->str2 : "(NULL)", e->str3 ? e->str3 : "(NULL)",
             e->int1, e->int2, n_node_index(e->node));
    cap_add(&ncap_, buf);
    cap_add(&ncap_, e->message ? e->message : "(NULL)");
    cap_add(&ncap_, "|FORMATTED|");
    cap_add(&ncap_, f);
    free(f);
}

static void compare_caps(const char *what)
{
    static int shown = 0;
    if (verbose_ > 1 && lcap_.s && shown++ < 100000) printf("%s: %s\n", what, lcap_.s);
    check_str(what, lcap_.s ? lcap_.s : "", ncap_.s ? ncap_.s : "");
    free(lcap_.s);
    free(ncap_.s);
    lcap_.s = ncap_.s = NULL;
}

// Documents //////////////////////////////////////////////////////

static xmlDocPtr *ldocs_ = NULL;
static XmlDom **ndocs_ = NULL;
static size_t docs_len_ = 0;

static void add_node(xmlNodePtr l, XMQNode *n)
{
    if (nodes_len_ == nodes_max_)
    {
        nodes_max_ = nodes_max_ ? nodes_max_ * 2 : 1024;
        lnodes_ = (xmlNodePtr *) realloc(lnodes_, nodes_max_ * sizeof(xmlNodePtr));
        nnodes_ = (XMQNode **) realloc(nnodes_, nodes_max_ * sizeof(XMQNode *));
    }
    lnodes_[nodes_len_] = l;
    nnodes_[nodes_len_] = n;
    nodes_len_++;
}

static int collect(xmlNodePtr l, XMQNode *n)
{
    if (strcmp((const char *) l->name, xml_node_name(n))) return 0;
    add_node(l, n);
    xmlAttrPtr la = l->properties;
    XMQAttr *na = xml_node_first_attr(n);
    while (la && na)
    {
        if (strcmp((const char *) la->name, xml_attr_name(na))) return 0;
        add_node((xmlNodePtr) la, xml_attr_node(na));
        la = la->next;
        na = xml_attr_next(na);
    }
    if (la || na) return 0;
    xmlNodePtr lc = l->children;
    XMQNode *nc = xml_node_first_child(n);
    while (1)
    {
        while (lc && lc->type != XML_ELEMENT_NODE) lc = lc->next;
        while (nc && xml_node_kind(nc) != XML_DOM_ELEMENT) nc = xml_node_next(nc);
        if (!lc || !nc) break;
        if (!collect(lc, nc)) return 0;
        lc = lc->next;
        nc = xml_node_next(nc);
    }
    return !lc && !nc;
}

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

static void load_doc(const char *file)
{
    size_t len;
    char *buf = read_file(file, &len);
    if (!buf) return;
    xmlDocPtr ldoc = xmlReadMemory(buf, (int) len, file, NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlParseSettings ps;
    xml_parse_settings_default(&ps);
    ps.source_name = file;
    XmlParseError err;
    memset(&err, 0, sizeof(err));
    XmlDom *ndoc = xml_parse_buffer(buf, buf + len, &ps, &err);
    xml_parse_error_free(&err);
    free(buf);
    if (!ldoc || !ndoc)
    {
        if (ldoc) xmlFreeDoc(ldoc);
        if (ndoc) xml_dom_free(ndoc);
        return;
    }
    xml_dom_set_url(ndoc, file);
    size_t mark = nodes_len_;
    xmlNodePtr lroot = xmlDocGetRootElement(ldoc);
    XMQNode *nroot = xml_dom_root_element(ndoc);
    if (!lroot || !nroot || !collect(lroot, nroot))
    {
        if (verbose_) printf("Skipping %s, the trees differ\n", file);
        nodes_len_ = mark;
        xmlFreeDoc(ldoc);
        xml_dom_free(ndoc);
        return;
    }
    ldocs_ = (xmlDocPtr *) realloc(ldocs_, (docs_len_ + 1) * sizeof(xmlDocPtr));
    ndocs_ = (XmlDom **) realloc(ndocs_, (docs_len_ + 1) * sizeof(XmlDom *));
    ldocs_[docs_len_] = ldoc;
    ndocs_[docs_len_] = ndoc;
    docs_len_++;
}

// A random node (element or attribute) or NULL.
static size_t rnd_node(int null_chance)
{
    if (nodes_len_ == 0 || (null_chance && rnd() % null_chance == 0)) return (size_t) -1;
    return rnd() % nodes_len_;
}
#define LNODE(i) ((i) == (size_t) -1 ? NULL : lnodes_[i])
#define NNODE(i) ((i) == (size_t) -1 ? NULL : nnodes_[i])

// Dict and hash //////////////////////////////////////////////////

static void check_dict(int count)
{
    XmlXsd10Dict *dict = xml_xsd10_dict_create();
    const char **seen = (const char **) calloc(count, sizeof(char *));
    char **copies = (char **) calloc(count, sizeof(char *));
    int distinct = 0;
    for (int i = 0; i < count; i++)
    {
        const char *s = rnd_str(0, 1);
        int len = rnd() % 3 == 0 ? (int)(rnd() % (strlen(s) + 1)) : -1;
        const char *r = xml_xsd10_dict_lookup(dict, s, len);
        char *expect = len < 0 ? strdup(s) : strndup(s, len);
        check_str("dict lookup", expect, r);
        int found = 0;
        for (int j = 0; j < distinct; j++)
        {
            if (!strcmp(copies[j], expect))
            {
                found = 1;
                check_int("dict same pointer", 1, seen[j] == r);
            }
        }
        if (!found)
        {
            seen[distinct] = r;
            copies[distinct] = expect;
            distinct++;
        }
        else free(expect);
        check_int("dict owns", 1, xml_xsd10_dict_owns(dict, r));
    }
    check_int("dict size", distinct, xml_xsd10_dict_size(dict));
    const char *q = xml_xsd10_dict_qlookup(dict, "pre", "local");
    check_str("dict qlookup", "pre:local", q);
    check_int("dict qlookup same", 1, q == xml_xsd10_dict_lookup(dict, "pre:local", -1));
    check_str("dict qlookup no prefix", "local", xml_xsd10_dict_qlookup(dict, NULL, "local"));
    xml_xsd10_dict_reference(dict);
    xml_xsd10_dict_free(dict);
    check_str("dict alive after ref free", "pre:local", q);
    xml_xsd10_dict_free(dict);
    for (int j = 0; j < distinct; j++) free(copies[j]);
    free(copies);
    free(seen);
}

static int dealloc_l_ = 0, dealloc_n_ = 0;
static void l_dealloc(void *payload, const xmlChar *name) { (void) payload; (void) name; dealloc_l_++; }
static void n_dealloc(void *payload, const char *name) { (void) payload; (void) name; dealloc_n_++; }

typedef struct { const char *k1, *k2, *k3; intptr_t v; } ModelEntry;
static ModelEntry *model_ = NULL;
static int model_len_ = 0;
static int scan_pos_ = 0, scan_bad_ = 0;

static int keq(const char *a, const char *b) { return (a == NULL && b == NULL) || (a && b && !strcmp(a, b)); }

static void n_scan_check(void *payload, void *data, const char *k1, const char *k2, const char *k3)
{
    (void) data;
    while (scan_pos_ < model_len_ && model_[scan_pos_].v == 0) scan_pos_++;
    if (scan_pos_ >= model_len_ || model_[scan_pos_].v != (intptr_t) payload ||
        !keq(model_[scan_pos_].k1, k1) || !keq(model_[scan_pos_].k2, k2) || !keq(model_[scan_pos_].k3, k3))
        scan_bad_++;
    scan_pos_++;
}

static long l_scan_sum_, n_scan_sum_;
static void l_scan(void *payload, void *data, const xmlChar *name) { (void) data; (void) name; l_scan_sum_ += (intptr_t) payload * 7919 % 1000003; }
static void n_scan(void *payload, void *data, const char *name) { (void) data; (void) name; n_scan_sum_ += (intptr_t) payload * 7919 % 1000003; }

static void check_hash(int count)
{
    static const char *k1s[] = { "a", "b", "", "ns", "http://x", "##", "c", "d" };
    static const char *k2s[] = { NULL, NULL, "", "x", "y" };
    static const char *k3s[] = { NULL, NULL, NULL, "z", "" };
    xmlHashTablePtr lh = xmlHashCreate(rnd() % 20);
    XmlXsd10Hash *nh = xml_xsd10_hash_create(rnd() % 20);
    model_ = (ModelEntry *) calloc(count + 1, sizeof(ModelEntry));
    model_len_ = 0;
    for (int i = 1; i <= count; i++)
    {
        const char *k1 = k1s[rnd() % 8], *k2 = k2s[rnd() % 5], *k3 = k3s[rnd() % 5];
        intptr_t v = i;
        int op = rnd() % 10;
        if (op < 4)
        {
            int lr = xmlHashAddEntry3(lh, BAD_CAST k1, BAD_CAST k2, BAD_CAST k3, (void *) v);
            int nr = xml_xsd10_hash_add_entry3(nh, k1, k2, k3, (void *) v);
            check_int("hash add", lr, nr);
            if (nr == 0)
            {
                model_[model_len_].k1 = k1; model_[model_len_].k2 = k2; model_[model_len_].k3 = k3; model_[model_len_].v = v;
                model_len_++;
            }
        }
        else if (op < 7)
        {
            check_int("hash lookup", (long)(intptr_t) xmlHashLookup3(lh, BAD_CAST k1, BAD_CAST k2, BAD_CAST k3),
                      (long)(intptr_t) xml_xsd10_hash_lookup3(nh, k1, k2, k3));
        }
        else if (op < 9)
        {
            int lr = xmlHashRemoveEntry3(lh, BAD_CAST k1, BAD_CAST k2, BAD_CAST k3, l_dealloc);
            int nr = xml_xsd10_hash_remove_entry3(nh, k1, k2, k3, n_dealloc);
            check_int("hash remove", lr, nr);
            for (int j = 0; j < model_len_; j++)
                if (model_[j].v && keq(model_[j].k1, k1) && keq(model_[j].k2, k2) && keq(model_[j].k3, k3)) model_[j].v = 0;
        }
        else if (k2 == NULL && k3 == NULL)
        {
            int lr = xmlHashUpdateEntry(lh, BAD_CAST k1, (void *) v, l_dealloc);
            int nr = xml_xsd10_hash_update_entry(nh, k1, (void *) v, n_dealloc);
            check_int("hash update", lr, nr);
            int found = 0;
            for (int j = 0; j < model_len_; j++)
                if (model_[j].v && keq(model_[j].k1, k1) && !model_[j].k2 && !model_[j].k3) { model_[j].v = v; found = 1; }
            if (!found)
            {
                model_[model_len_].k1 = k1; model_[model_len_].k2 = NULL; model_[model_len_].k3 = NULL; model_[model_len_].v = v;
                model_len_++;
            }
        }
        check_int("hash size", xmlHashSize(lh), xml_xsd10_hash_size(nh));
        check_int("hash deallocs", dealloc_l_, dealloc_n_);
    }
    l_scan_sum_ = n_scan_sum_ = 0;
    xmlHashScan(lh, l_scan, NULL);
    xml_xsd10_hash_scan(nh, n_scan, NULL);
    check_int("hash scan payloads", l_scan_sum_, n_scan_sum_);
    scan_pos_ = scan_bad_ = 0;
    xml_xsd10_hash_scan_full(nh, n_scan_check, NULL);
    while (scan_pos_ < model_len_ && model_[scan_pos_].v == 0) scan_pos_++;
    check_int("hash scan in insertion order", 0, scan_bad_ + (scan_pos_ != model_len_));
    xmlHashFree(lh, l_dealloc);
    xml_xsd10_hash_free(nh, n_dealloc);
    check_int("hash free deallocs", dealloc_l_, dealloc_n_);
    free(model_);
    model_ = NULL;
}

// Tree helpers ///////////////////////////////////////////////////

static void check_tree(int count)
{
    static const char *names[] = { "a", "a:b", ":a", "a:", "a:b:c", " a ", "1a", "a b", "_x.y-z", "\xc3\xa9t\xc3\xa9", "a:1", "", "x:\xc3\xa9", "\t q:n \n", "\xff" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        for (int space = 0; space < 2; space++)
        {
            check_int("validate ncname", xmlValidateNCName(BAD_CAST names[i], space), xml_xsd10_tree_validate_ncname(names[i], space));
            check_int("validate qname", xmlValidateQName(BAD_CAST names[i], space), xml_xsd10_tree_validate_qname(names[i], space));
        }
        xmlChar *lp = NULL;
        char *np = NULL;
        xmlChar *ll = xmlSplitQName2(BAD_CAST names[i], &lp);
        char *nl = xml_xsd10_tree_split_qname2(names[i], &np);
        check_str("split qname2 local", (const char *) ll, nl);
        check_str("split qname2 prefix", (const char *) lp, np);
        xmlFree(ll); xmlFree(lp); free(nl); free(np);
        int llen = -1, nlen = -1;
        const xmlChar *l3 = xmlSplitQName3(BAD_CAST names[i], &llen);
        const char *n3 = xml_xsd10_tree_split_qname3(names[i], &nlen);
        check_str("split qname3", (const char *) l3, n3);
        check_int("split qname3 len", llen, nlen);
    }
    static const char *prefixes[] = { NULL, "xs", "xsd", "xml", "tns", "foo", "" };
    for (int i = 0; i < count && nodes_len_ > 0; i++)
    {
        size_t k = rnd_node(0);
        xmlNodePtr l = lnodes_[k];
        XMQNode *n = nnodes_[k];
        const char *prefix = prefixes[rnd() % 7];
        xmlNsPtr lns = xmlSearchNs(l->doc, l, BAD_CAST prefix);
        XmlNs *nns = xml_xsd10_tree_search_ns(n, prefix);
        check_str("search ns href", lns ? (const char *) lns->href : NULL, nns ? xml_ns_href(nns) : NULL);
        const char *href = lns ? (const char *) lns->href : (rnd() % 2 ? "http://www.w3.org/2001/XMLSchema" : "urn:none");
        lns = xmlSearchNsByHref(l->doc, l, BAD_CAST href);
        nns = xml_xsd10_tree_search_ns_by_href(n, href);
        check_str("search ns by href prefix", lns ? (lns->prefix ? (const char *) lns->prefix : "(default)") : NULL,
                  nns ? (xml_ns_prefix(nns) ? xml_ns_prefix(nns) : "(default)") : NULL);
        xmlNsPtr *ll = xmlGetNsList(l->doc, l);
        XmlNs **nl = xml_xsd10_tree_get_ns_list(n);
        Cap a = { NULL }, b = { NULL };
        for (int j = 0; ll && ll[j]; j++) { cap_add(&a, ll[j]->prefix ? (const char *) ll[j]->prefix : "-"); cap_add(&a, "="); cap_add(&a, (const char *) ll[j]->href); cap_add(&a, " "); }
        for (int j = 0; nl && nl[j]; j++) { cap_add(&b, xml_ns_prefix(nl[j]) ? xml_ns_prefix(nl[j]) : "-"); cap_add(&b, "="); cap_add(&b, xml_ns_href(nl[j])); cap_add(&b, " "); }
        check_str("get ns list", a.s, b.s);
        free(a.s); free(b.s); xmlFree(ll); free(nl);
        if (l->type == XML_ELEMENT_NODE)
        {
            static const char *anames[] = { "name", "type", "ref", "minOccurs", "base", "value", "id", "nothere" };
            const char *an = anames[rnd() % 8];
            xmlChar *lv = xmlGetNoNsProp(l, BAD_CAST an);
            char *nv = xml_xsd10_tree_get_no_ns_prop(n, an);
            check_str("get no ns prop", (const char *) lv, nv);
            xmlFree(lv); free(nv);
            xmlAttrPtr lp = xmlHasProp(l, BAD_CAST an);
            XMQNode *np = xml_xsd10_tree_has_prop(n, an);
            check_int("has prop", l_node_index(lp), n_node_index(np));
            check_int("space preserve", xmlNodeGetSpacePreserve(l), xml_xsd10_tree_node_get_space_preserve(n));
        }
        xmlChar *lc = xmlNodeGetContent(l);
        char *nc = xml_xsd10_tree_node_get_content(n);
        check_str("node get content", (const char *) lc, nc);
        xmlFree(lc); free(nc);
        xmlChar *lb = xmlNodeGetBase(l->doc, l);
        char *nb = xml_xsd10_tree_node_get_base(xml_node_dom(n), n);
        check_str("node get base", (const char *) lb, nb);
        xmlFree(lb); free(nb);
        check_int("get line no", xmlGetLineNo(l), xml_xsd10_tree_get_line_no(n));
    }
}

// Components /////////////////////////////////////////////////////

static const char *builtin_names_[] = { "string", "anyType", "anySimpleType", "decimal", "integer", "NMTOKENS", "IDREFS", "QName", "boolean", "token" };

typedef struct
{
    xmlSchemaBasicItemPtr l;
    XmlXsd10BasicItem *n;
} ItemPair;

static void **allocs_ = NULL;
static size_t allocs_len_ = 0;
static void *alloc_keep(size_t size)
{
    void *p = calloc(1, size);
    allocs_ = (void **) realloc(allocs_, (allocs_len_ + 1) * sizeof(void *));
    allocs_[allocs_len_++] = p;
    return p;
}

static const int item_kinds_[] = {
    XML_SCHEMA_TYPE_BASIC, XML_SCHEMA_TYPE_SIMPLE, XML_SCHEMA_TYPE_COMPLEX, XML_SCHEMA_TYPE_ATTRIBUTE_USE,
    XML_SCHEMA_TYPE_ATTRIBUTE, XML_SCHEMA_TYPE_ATTRIBUTEGROUP, XML_SCHEMA_TYPE_ELEMENT, XML_SCHEMA_TYPE_IDC_UNIQUE,
    XML_SCHEMA_TYPE_IDC_KEY, XML_SCHEMA_TYPE_IDC_KEYREF, XML_SCHEMA_TYPE_ANY, XML_SCHEMA_TYPE_ANY_ATTRIBUTE,
    XML_SCHEMA_FACET_MININCLUSIVE, XML_SCHEMA_FACET_PATTERN, XML_SCHEMA_FACET_LENGTH, XML_SCHEMA_FACET_WHITESPACE,
    XML_SCHEMA_TYPE_GROUP, XML_SCHEMA_TYPE_SEQUENCE, XML_SCHEMA_TYPE_CHOICE, XML_SCHEMA_TYPE_ALL,
    XML_SCHEMA_TYPE_PARTICLE, XML_SCHEMA_TYPE_NOTATION, XML_SCHEMA_EXTRA_QNAMEREF, XML_SCHEMA_EXTRA_ATTR_USE_PROHIB,
    XML_SCHEMA_TYPE_UNION
};
#define NUM_ITEM_KINDS ((int) (sizeof(item_kinds_) / sizeof(item_kinds_[0])))

static const int type_flag_bits_[] = { XML_SCHEMAS_TYPE_GLOBAL, XML_SCHEMAS_TYPE_VARIETY_ATOMIC, XML_SCHEMAS_TYPE_VARIETY_LIST, XML_SCHEMAS_TYPE_VARIETY_UNION };

static int rnd_type_flags(void)
{
    int flags = 0;
    for (int i = 0; i < 4; i++) if (rnd() % 2) flags |= type_flag_bits_[i];
    return flags;
}

/* A random component as libxml2 and native structs. Names may have % if percent. */
static ItemPair make_item(int kind, int percent)
{
    ItemPair p = { NULL, NULL };
    const char *name = rnd_str(5, percent);
    const char *tns = rnd_ns();
    switch (kind)
    {
    case XML_SCHEMA_TYPE_BASIC:
    {
        const char *bn = builtin_names_[rnd() % 10];
        p.l = (xmlSchemaBasicItemPtr) xmlSchemaGetPredefinedType(BAD_CAST bn, BAD_CAST "http://www.w3.org/2001/XMLSchema");
        p.n = (XmlXsd10BasicItem *) xml_xsd10_get_predefined_type(bn, "http://www.w3.org/2001/XMLSchema");
        break;
    }
    case XML_SCHEMA_TYPE_SIMPLE:
    case XML_SCHEMA_TYPE_COMPLEX:
    {
        xmlSchemaTypePtr l = (xmlSchemaTypePtr) alloc_keep(sizeof(xmlSchemaType));
        XmlXsd10Type *n = (XmlXsd10Type *) alloc_keep(sizeof(XmlXsd10Type));
        l->type = (xmlSchemaTypeType) kind; n->type = (XmlXsd10TypeType) kind;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        l->flags = n->flags = rnd_type_flags();
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_ATTRIBUTE_USE:
    {
        xmlSchemaAttributeUsePtr l = (xmlSchemaAttributeUsePtr) alloc_keep(sizeof(xmlSchemaAttributeUse));
        XmlXsd10AttributeUse *n = (XmlXsd10AttributeUse *) alloc_keep(sizeof(XmlXsd10AttributeUse));
        l->type = XML_SCHEMA_TYPE_ATTRIBUTE_USE; n->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE_USE;
        if (rnd() % 4)
        {
            xmlSchemaAttributePtr la = (xmlSchemaAttributePtr) alloc_keep(sizeof(xmlSchemaAttribute));
            XmlXsd10Attribute *na = (XmlXsd10Attribute *) alloc_keep(sizeof(XmlXsd10Attribute));
            la->type = XML_SCHEMA_TYPE_ATTRIBUTE; na->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE;
            la->name = BAD_CAST name; na->name = name;
            la->targetNamespace = BAD_CAST tns; na->targetNamespace = tns;
            l->attrDecl = la; n->attrDecl = na;
        }
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_ATTRIBUTE:
    {
        xmlSchemaAttributePtr l = (xmlSchemaAttributePtr) alloc_keep(sizeof(xmlSchemaAttribute));
        XmlXsd10Attribute *n = (XmlXsd10Attribute *) alloc_keep(sizeof(XmlXsd10Attribute));
        l->type = XML_SCHEMA_TYPE_ATTRIBUTE; n->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTE;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        l->flags = n->flags = rnd() % 2 ? XML_SCHEMAS_ATTR_GLOBAL : 0;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_ATTRIBUTEGROUP:
    {
        xmlSchemaAttributeGroupPtr l = (xmlSchemaAttributeGroupPtr) alloc_keep(sizeof(xmlSchemaAttributeGroup));
        XmlXsd10AttributeGroup *n = (XmlXsd10AttributeGroup *) alloc_keep(sizeof(XmlXsd10AttributeGroup));
        l->type = XML_SCHEMA_TYPE_ATTRIBUTEGROUP; n->type = XML_XSD10_SCHEMA_TYPE_ATTRIBUTEGROUP;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_ELEMENT:
    {
        xmlSchemaElementPtr l = (xmlSchemaElementPtr) alloc_keep(sizeof(xmlSchemaElement));
        XmlXsd10Element *n = (XmlXsd10Element *) alloc_keep(sizeof(XmlXsd10Element));
        l->type = XML_SCHEMA_TYPE_ELEMENT; n->type = XML_XSD10_SCHEMA_TYPE_ELEMENT;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        l->flags = n->flags = rnd() % 2 ? XML_SCHEMAS_ELEM_GLOBAL : 0;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_IDC_UNIQUE:
    case XML_SCHEMA_TYPE_IDC_KEY:
    case XML_SCHEMA_TYPE_IDC_KEYREF:
    {
        xmlSchemaIDCPtr l = (xmlSchemaIDCPtr) alloc_keep(sizeof(xmlSchemaIDC));
        XmlXsd10IDC *n = (XmlXsd10IDC *) alloc_keep(sizeof(XmlXsd10IDC));
        l->type = (xmlSchemaTypeType) kind; n->type = (XmlXsd10TypeType) kind;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_ANY:
    case XML_SCHEMA_TYPE_ANY_ATTRIBUTE:
    {
        xmlSchemaWildcardPtr l = (xmlSchemaWildcardPtr) alloc_keep(sizeof(xmlSchemaWildcard));
        XmlXsd10Wildcard *n = (XmlXsd10Wildcard *) alloc_keep(sizeof(XmlXsd10Wildcard));
        l->type = (xmlSchemaTypeType) kind; n->type = (XmlXsd10TypeType) kind;
        l->processContents = n->processContents = rnd() % 5;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_GROUP:
    {
        xmlSchemaModelGroupDefPtr l = (xmlSchemaModelGroupDefPtr) alloc_keep(sizeof(xmlSchemaModelGroupDef));
        XmlXsd10ModelGroupDef *n = (XmlXsd10ModelGroupDef *) alloc_keep(sizeof(XmlXsd10ModelGroupDef));
        l->type = XML_SCHEMA_TYPE_GROUP; n->type = XML_XSD10_SCHEMA_TYPE_GROUP;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_TYPE_NOTATION:
    {
        xmlSchemaNotationPtr l = (xmlSchemaNotationPtr) alloc_keep(sizeof(xmlSchemaNotation));
        XmlXsd10Notation *n = (XmlXsd10Notation *) alloc_keep(sizeof(XmlXsd10Notation));
        l->type = XML_SCHEMA_TYPE_NOTATION; n->type = XML_XSD10_SCHEMA_TYPE_NOTATION;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    case XML_SCHEMA_EXTRA_QNAMEREF:
    {
        xmlSchemaQNameRefPtr l = (xmlSchemaQNameRefPtr) alloc_keep(sizeof(xmlSchemaQNameRef));
        XmlXsd10QNameRef *n = (XmlXsd10QNameRef *) alloc_keep(sizeof(XmlXsd10QNameRef));
        l->type = XML_SCHEMA_EXTRA_QNAMEREF; n->type = XML_XSD10_SCHEMA_EXTRA_QNAMEREF;
        l->name = BAD_CAST name; n->name = name;
        l->targetNamespace = BAD_CAST tns; n->targetNamespace = tns;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    default:
    {
        /* Facets, model groups, particles and others: only the type matters (and a node). */
        xmlSchemaParticlePtr l = (xmlSchemaParticlePtr) alloc_keep(sizeof(xmlSchemaParticle) + sizeof(xmlSchemaFacet) + sizeof(xmlSchemaType));
        XmlXsd10Particle *n = (XmlXsd10Particle *) alloc_keep(sizeof(XmlXsd10Particle) + sizeof(XmlXsd10Facet) + sizeof(XmlXsd10Type));
        l->type = (xmlSchemaTypeType) kind; n->type = (XmlXsd10TypeType) kind;
        p.l = (xmlSchemaBasicItemPtr) l; p.n = (XmlXsd10BasicItem *) n;
        break;
    }
    }
    return p;
}

/* Give the item a node (where the struct has one), only for types with a node field. */
static void set_item_node(ItemPair p, size_t k)
{
    xmlNodePtr l = LNODE(k);
    XMQNode *n = NNODE(k);
    switch ((int) p.n->type)
    {
    case XML_SCHEMA_TYPE_SIMPLE: case XML_SCHEMA_TYPE_COMPLEX:
        ((xmlSchemaTypePtr) p.l)->node = l; ((XmlXsd10Type *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_ELEMENT:
        ((xmlSchemaElementPtr) p.l)->node = l; ((XmlXsd10Element *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_ATTRIBUTE:
        ((xmlSchemaAttributePtr) p.l)->node = l; ((XmlXsd10Attribute *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_ATTRIBUTE_USE:
        ((xmlSchemaAttributeUsePtr) p.l)->node = l; ((XmlXsd10AttributeUse *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_GROUP:
        ((xmlSchemaModelGroupDefPtr) p.l)->node = l; ((XmlXsd10ModelGroupDef *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_PARTICLE:
        ((xmlSchemaParticlePtr) p.l)->node = l; ((XmlXsd10Particle *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_ANY: case XML_SCHEMA_TYPE_ANY_ATTRIBUTE:
        ((xmlSchemaWildcardPtr) p.l)->node = l; ((XmlXsd10Wildcard *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_IDC_UNIQUE: case XML_SCHEMA_TYPE_IDC_KEY: case XML_SCHEMA_TYPE_IDC_KEYREF:
        ((xmlSchemaIDCPtr) p.l)->node = l; ((XmlXsd10IDC *) p.n)->node = n; break;
    case XML_SCHEMA_TYPE_ATTRIBUTEGROUP:
        ((xmlSchemaAttributeGroupPtr) p.l)->node = l; ((XmlXsd10AttributeGroup *) p.n)->node = n; break;
    case XML_SCHEMA_EXTRA_QNAMEREF:
        ((xmlSchemaQNameRefPtr) p.l)->node = l; ((XmlXsd10QNameRef *) p.n)->node = n; break;
    default: break;
    }
}

static void free_allocs(void)
{
    for (size_t i = 0; i < allocs_len_; i++) free(allocs_[i]);
    free(allocs_);
    allocs_ = NULL;
    allocs_len_ = 0;
}

static void check_format(int count)
{
    for (int i = 0; i < count; i++)
    {
        const char *ns = rnd_str(3, 1), *local = rnd_str(4, 1);
        xmlChar *lb = rnd() % 2 ? xmlStrdup(BAD_CAST "old") : NULL;
        char *nb = lb ? strdup("old") : NULL;
        const xmlChar *lr = xmlSchemaFormatQName(&lb, BAD_CAST ns, BAD_CAST local);
        const char *nr = xml_xsd10_format_q_name(&nb, ns, local);
        check_str("format qname", (const char *) lr, nr);
        xmlFree(lb); free(nb);

        int kind = item_kinds_[rnd() % NUM_ITEM_KINDS];
        ItemPair p = make_item(kind, 1);
        size_t k = rnd_node(3);
        set_item_node(p, rnd_node(2));
        const char *des = rnd() % 8 == 0 ? rnd_str(0, 1) : NULL;
        lb = NULL; nb = NULL;
        lb = NULL; nb = NULL;
        int with_item = rnd() % 10 != 0;
        xmlChar *lf = xmlSchemaFormatItemForReport(&lb, BAD_CAST des, with_item ? p.l : NULL, LNODE(k));
        char *nf = xml_xsd10_format_item_for_report(&nb, des, with_item ? p.n : NULL, NNODE(k));
        check_str("format item for report", (const char *) lf, nf);
        xmlFree(lb); free(nb);

        if (kind != XML_SCHEMA_TYPE_BASIC || p.l != NULL)
        {
            lb = NULL; nb = NULL;
            xmlSchemaGetComponentDesignation(&lb, p.l);
            xml_xsd10_get_component_designation(&nb, p.n);
            check_str("component designation", (const char *) lb, nb);
            xmlFree(lb); free(nb);
            check_str("component type str", (const char *) xmlSchemaGetComponentTypeStr(p.l), xml_xsd10_get_component_type_str(p.n));
            check_int("component node", l_node_index(xmlSchemaGetComponentNode(p.l)), n_node_index(xml_xsd10_get_component_node(p.n)));
        }
        check_str("item type to str", (const char *) xmlSchemaItemTypeToStr((xmlSchemaTypeType) kind), xml_xsd10_item_type_to_str((XmlXsd10TypeType) kind));
        check_str("facet type to string", (const char *) xmlSchemaFacetTypeToString((xmlSchemaTypeType) kind), xml_xsd10_facet_type_to_string((XmlXsd10TypeType) kind));
        int pc = rnd() % 5;
        check_str("wildcard pc", (const char *) xmlSchemaWildcardPCToString(pc), xml_xsd10_wildcard_pc_to_string(pc));
    }
}

// Values /////////////////////////////////////////////////////////

static const char *val_types_[] = { "string", "normalizedString", "token", "decimal", "integer", "double", "NMTOKENS", "boolean", "date", "anySimpleType", "IDREFS", "int" };
static const char *val_values_[] = { "  a\tb \n c ", "1", "1.0", "-0.50", "007", "1e3", "x y  z", "true", "2001-10-26", "", " 12 ", "INF", "a" };

static void check_values(int count)
{
    for (int i = 0; i < count; i++)
    {
        const char *tn = val_types_[rnd() % 12];
        const char *v = val_values_[rnd() % 13];
        xmlSchemaTypePtr lt = xmlSchemaGetPredefinedType(BAD_CAST tn, BAD_CAST "http://www.w3.org/2001/XMLSchema");
        XmlXsd10Type *nt = xml_xsd10_get_predefined_type(tn, "http://www.w3.org/2001/XMLSchema");
        xmlSchemaValPtr lv = NULL;
        XmlXsd10Val *nv = NULL;
        int lr = xmlSchemaValidatePredefinedType(lt, BAD_CAST v, &lv);
        int nr = xml_xsd10_validate_predefined_type(nt, v, &nv);
        check_int("validate predefined", lr, nr);
        if (lv && nv)
        {
            int ws = rnd() % 4;
            xmlChar *lc = NULL;
            char *nc = NULL;
            lr = xmlSchemaGetCanonValueWhtspExt(lv, (xmlSchemaWhitespaceValueType) ws, &lc);
            nr = xml_xsd10_get_canon_value_whtsp_ext(nv, (XmlXsd10WhitespaceValueType) ws, &nc);
            check_int("canon whtsp ext", lr, nr);
            check_str("canon whtsp ext value", (const char *) lc, nc);
            xmlFree(lc); free(nc);
            lc = NULL; nc = NULL;
            lr = xmlSchemaGetCanonValueHash(lv, &lc);
            nr = xml_xsd10_get_canon_value_hash(nv, &nc);
            check_int("canon hash", lr, nr);
            check_str("canon hash value", (const char *) lc, nc);
            xmlFree(lc); free(nc);
            check_int("white space facet value", xmlSchemaGetWhiteSpaceFacetValue(lt), xml_xsd10_get_white_space_facet_value(nt));
        }
        if (lv) xmlSchemaFreeValue(lv);
        if (nv) xml_xsd10_free_value(nv);
    }
}

// Error functions ////////////////////////////////////////////////

typedef struct
{
    xmlSchemaParserCtxtPtr lp;
    XmlXsd10ParserCtxt *np;
    xmlSchemaValidCtxtPtr lv;
    XmlXsd10ValidCtxt *nv;
    int mode; /* 0 plain callbacks, 1 structured callbacks */
} Ctxts;

static XmlXsd10ValidCtxt *native_valid_ctxt(void)
{
    /* The validation context creation belongs to xml_xsd10_valid, build one by hand. */
    XmlXsd10ValidCtxt *v = (XmlXsd10ValidCtxt *) calloc(1, sizeof(XmlXsd10ValidCtxt));
    v->type = XML_XSD10_SCHEMA_CTXT_VALIDATOR;
    v->dict = xml_xsd10_dict_create();
    v->nodeQNames = xml_xsd10_item_list_create();
    return v;
}

static void free_native_valid_ctxt(XmlXsd10ValidCtxt *v)
{
    xml_xsd10_item_list_free(v->nodeQNames);
    v->nodeQNames = NULL;
    /* xml_xsd10_free_valid_ctxt frees the filename. */
    xml_xsd10_free_valid_ctxt(v);
}

static void make_ctxts(Ctxts *c, int mode)
{
    c->mode = mode;
    c->lp = xmlSchemaNewParserCtxt("test.xsd");
    c->np = xml_xsd10_new_parser_ctxt("test.xsd");
    c->lv = xmlSchemaNewValidCtxt(NULL);
    c->nv = native_valid_ctxt();
    if (mode == 0)
    {
        xmlSchemaSetParserErrors(c->lp, l_err, l_err, (void *) "P:");
        xml_xsd10_set_parser_errors(c->np, n_err, n_err, (void *) "P:");
        xmlSchemaSetValidErrors(c->lv, l_err, l_err, (void *) "V:");
        xml_xsd10_set_valid_errors(c->nv, n_err, n_err, (void *) "V:");
    }
    else if (mode == 1)
    {
        xmlSchemaSetParserStructuredErrors(c->lp, l_serr, (void *) "P:");
        xml_xsd10_set_parser_structured_errors(c->np, n_serr, (void *) "P:");
        xmlSchemaSetValidStructuredErrors(c->lv, l_serr, (void *) "V:");
        xml_xsd10_set_valid_structured_errors(c->nv, n_serr, (void *) "V:");
    }
    else
    {
        /* Errors only, warnings fall back to the (stderr) default: only use errors then. */
        xmlSchemaSetParserErrors(c->lp, l_err, NULL, (void *) "P:");
        xml_xsd10_set_parser_errors(c->np, n_err, NULL, (void *) "P:");
        xmlSchemaSetValidErrors(c->lv, l_err, NULL, (void *) "V:");
        xml_xsd10_set_valid_errors(c->nv, n_err, NULL, (void *) "V:");
    }
}

static void free_ctxts(Ctxts *c)
{
    c->lv->elemInfos = NULL; c->lv->inode = NULL;
    c->nv->elemInfos = NULL; c->nv->inode = NULL;
    xmlSchemaFreeParserCtxt(c->lp);
    xml_xsd10_free_parser_ctxt(c->np);
    xmlSchemaFreeValidCtxt(c->lv);
    free_native_valid_ctxt(c->nv);
}

static const char *messages_[] = {
    "The value '%s' is not valid",
    "Expected %s but got '%s'",
    "Skipping import of schema located at '%s' for the namespace '%s'",
    "No params here",
    "Four: %s %s %s %s",
};

/* An element info for the validation context (libxml2 and native), the current inode. */
static xmlSchemaNodeInfo linfo_[2];
static XmlXsd10NodeInfo ninfo_[2];
static xmlSchemaNodeInfoPtr lelem_infos_[1];
static XmlXsd10NodeInfo *nelem_infos_[1];

static void set_inode(Ctxts *c, int with)
{
    if (!with)
    {
        c->lv->inode = NULL; c->nv->inode = NULL;
        c->lv->depth = c->nv->depth = 0;
        return;
    }
    const char *ln = rnd_str(0, 0), *ns = rnd_ns();
    const char *aln = rnd_str(0, 0), *ans = rnd_ns();
    memset(linfo_, 0, sizeof(linfo_));
    memset(ninfo_, 0, sizeof(ninfo_));
    linfo_[0].nodeType = XML_ELEMENT_NODE; ninfo_[0].nodeType = XML_DOM_ELEMENT;
    linfo_[0].localName = BAD_CAST ln; ninfo_[0].localName = ln;
    linfo_[0].nsName = BAD_CAST ns; ninfo_[0].nsName = ns;
    linfo_[1].nodeType = XML_ATTRIBUTE_NODE; ninfo_[1].nodeType = XML_DOM_ATTRIBUTE;
    linfo_[1].localName = BAD_CAST aln; ninfo_[1].localName = aln;
    linfo_[1].nsName = BAD_CAST ans; ninfo_[1].nsName = ans;
    lelem_infos_[0] = &linfo_[0]; nelem_infos_[0] = &ninfo_[0];
    c->lv->elemInfos = lelem_infos_; c->nv->elemInfos = nelem_infos_;
    c->lv->depth = c->nv->depth = 0;
    int attr = rnd() % 2;
    c->lv->inode = &linfo_[attr]; c->nv->inode = &ninfo_[attr];
}

static void check_errors_once(Ctxts *c)
{
    int on_validator = rnd() % 2;
    xmlSchemaAbstractCtxtPtr la = on_validator ? (xmlSchemaAbstractCtxtPtr) c->lv : (xmlSchemaAbstractCtxtPtr) c->lp;
    XmlXsd10AbstractCtxt *na = on_validator ? (XmlXsd10AbstractCtxt *) c->nv : (XmlXsd10AbstractCtxt *) c->np;
    set_inode(c, on_validator && rnd() % 2);
    size_t k = rnd_node(4), k2 = rnd_node(3);
    xmlNodePtr ln = LNODE(k);
    XMQNode *nn = NNODE(k);
    const char *s1 = rnd_str(4, 0), *s2 = rnd_str(4, 0), *s3 = rnd_str(4, 0), *s4 = rnd_str(4, 0);
    const char *msg = messages_[rnd() % 4];
    int code = 1700 + rnd() % 200;
    int op = rnd() % 30;
    char what[64];
    snprintf(what, sizeof(what), "error op %d mode %d %s", op, c->mode, on_validator ? "validator" : "parser");
    int warn_ok = c->mode != 2;
    /* libxml2 xmlSchemaFormatNodeForError needs a node or a current node info on a validation context. */
    int fmt_ok = ln || !on_validator || c->lv->inode;

    /* A random type for the type errors. */
    ItemPair tp = make_item(rnd() % 3 ? XML_SCHEMA_TYPE_SIMPLE : XML_SCHEMA_TYPE_BASIC, 0);
    if (!tp.l) tp = make_item(XML_SCHEMA_TYPE_SIMPLE, 0);
    ItemPair ip = make_item(item_kinds_[rnd() % NUM_ITEM_KINDS], 0);
    if (!ip.l) ip = make_item(XML_SCHEMA_TYPE_ELEMENT, 0);
    set_item_node(ip, rnd_node(3));

    switch (op)
    {
    case 0:
        xmlSchemaPErr(c->lp, ln, code, msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_p_err(c->np, nn, code, msg, s1, s2);
        break;
    case 1:
        xmlSchemaPErr2(c->lp, ln, LNODE(k2), code, msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_p_err2(c->np, nn, NNODE(k2), code, msg, s1, s2);
        break;
    case 2:
        xmlSchemaPErrExt(c->lp, ln, code, BAD_CAST s1, BAD_CAST s2, BAD_CAST s3, messages_[4], BAD_CAST s4, BAD_CAST s3, BAD_CAST s2, BAD_CAST s1, NULL);
        xml_xsd10_p_err_ext(c->np, nn, code, s1, s2, s3, messages_[4], s4, s3, s2, s1, NULL);
        break;
    case 3:
        xmlSchemaErr4(la, code, ln, messages_[4], BAD_CAST s1, BAD_CAST s2, BAD_CAST s3, BAD_CAST s4);
        xml_xsd10_err4(na, code, nn, messages_[4], s1, s2, s3, s4);
        break;
    case 4:
    {
        int line = rnd() % 3 ? 0 : (int) (rnd() % 100);
        xmlErrorLevel level = (warn_ok && rnd() % 2) ? XML_ERR_WARNING : XML_ERR_ERROR;
        xmlSchemaErr4Line(la, level, code, ln, line, messages_[4], BAD_CAST s1, BAD_CAST s2, BAD_CAST s3, BAD_CAST s4);
        xml_xsd10_err4_line(na, (XmlXsd10ErrorLevel) level, code, nn, line, messages_[4], s1, s2, s3, s4);
        break;
    }
    case 5:
        xmlSchemaInternalErr2(la, "someFunc", msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_internal_err2(na, "someFunc", msg, s1, s2);
        break;
    case 6:
    {
        if (!fmt_ok) break;
        int with_item = rnd() % 2;
        int with_node = rnd() % 2;
        if ((!with_node || !ln) && on_validator && !c->lv->inode) break;
        xmlSchemaCustomErr4(la, (xmlParserErrors) code, with_node ? ln : NULL, with_item ? ip.l : NULL, messages_[4], BAD_CAST s1, BAD_CAST s2, BAD_CAST s3, BAD_CAST s4);
        xml_xsd10_custom_err4(na, code, with_node ? nn : NULL, with_item ? ip.n : NULL, messages_[4], s1, s2, s3, s4);
        break;
    }
    case 7:
        if (!warn_ok || !fmt_ok) break;
        xmlSchemaCustomWarning(la, (xmlParserErrors) code, ln, NULL, msg, BAD_CAST s1, BAD_CAST s2, BAD_CAST s3);
        xml_xsd10_custom_warning(na, code, nn, NULL, msg, s1, s2, s3);
        break;
    case 8:
    {
        int dv = rnd() % 2;
        if (!ln && !(on_validator && c->lv->inode)) break;
        xmlSchemaSimpleTypeErr(la, (xmlParserErrors) code, ln, BAD_CAST s1, (xmlSchemaTypePtr) tp.l, dv);
        xml_xsd10_simple_type_err(na, code, nn, s1, (XmlXsd10Type *) tp.n, dv);
        break;
    }
    case 9:
        if (!ln) break;
        xmlSchemaIllegalAttrErr(la, (xmlParserErrors) code, NULL, ln);
        xml_xsd10_illegal_attr_err(na, code, NULL, nn);
        break;
    case 10:
    {
        static const char *vals[] = { "a", "not *", "b|http://ns", "*|*", "not *|*", "*", "x|*", "not y|z", NULL, "c" };
        const char *lvals[6];
        int nbval = rnd() % 4, nbneg = rnd() % 3;
        for (int i = 0; i < nbval + nbneg; i++) lvals[i] = vals[rnd() % 10];
        if (!ln && !(on_validator && c->lv->inode)) break;
        xmlSchemaComplexTypeErr(la, (xmlParserErrors) code, ln, NULL, "This element is not expected", nbval, nbneg, (xmlChar **) lvals);
        xml_xsd10_complex_type_err(na, code, nn, NULL, "This element is not expected", nbval, nbneg, (char **) lvals);
        break;
    }
    case 11:
    case 12:
    {
        /* Facets on a simple type derived from a built in type. */
        static const int fts[] = { XML_SCHEMA_FACET_MININCLUSIVE, XML_SCHEMA_FACET_MAXEXCLUSIVE, XML_SCHEMA_FACET_TOTALDIGITS,
                                   XML_SCHEMA_FACET_FRACTIONDIGITS, XML_SCHEMA_FACET_PATTERN, XML_SCHEMA_FACET_ENUMERATION,
                                   XML_SCHEMA_FACET_LENGTH, XML_SCHEMA_FACET_MINLENGTH, XML_SCHEMA_FACET_MAXLENGTH, XML_SCHEMA_FACET_WHITESPACE,
                                   XML_SCHEMA_FACET_MINEXCLUSIVE, XML_SCHEMA_FACET_MAXINCLUSIVE };
        int ft = fts[rnd() % 12];
        if (!ln && !(on_validator && c->lv->inode)) break;
        const char *base = ft == XML_SCHEMA_FACET_ENUMERATION ? (rnd() % 2 ? "string" : "decimal") :
                           (ft >= XML_SCHEMA_FACET_LENGTH && ft <= XML_SCHEMA_FACET_MINLENGTH) ? "string" : "decimal";
        const char *fvalue = ft == XML_SCHEMA_FACET_PATTERN ? "[a-z]+" : ft == XML_SCHEMA_FACET_WHITESPACE ? "collapse" : "5";
        xmlSchemaTypePtr lbase = xmlSchemaGetPredefinedType(BAD_CAST base, BAD_CAST "http://www.w3.org/2001/XMLSchema");
        XmlXsd10Type *nbase = xml_xsd10_get_predefined_type(base, "http://www.w3.org/2001/XMLSchema");
        xmlSchemaTypePtr lt = (xmlSchemaTypePtr) alloc_keep(sizeof(xmlSchemaType));
        XmlXsd10Type *nt = (XmlXsd10Type *) alloc_keep(sizeof(XmlXsd10Type));
        lt->type = XML_SCHEMA_TYPE_SIMPLE; nt->type = XML_XSD10_SCHEMA_TYPE_SIMPLE;
        lt->flags = nt->flags = XML_SCHEMAS_TYPE_VARIETY_ATOMIC | XML_SCHEMAS_TYPE_GLOBAL;
        lt->name = BAD_CAST "myType"; nt->name = "myType";
        lt->baseType = lbase; nt->baseType = nbase;
        int nf = ft == XML_SCHEMA_FACET_ENUMERATION ? 1 + rnd() % 3 : 1;
        static const char *enums[] = { " a ", "b", "1.50", "007", "x\ty" };
        xmlSchemaFacetPtr lf = NULL, lfirst = NULL;
        XmlXsd10Facet *nfa = NULL, *nfirst = NULL;
        int ok = 1;
        for (int i = 0; i < nf; i++)
        {
            const char *v = ft == XML_SCHEMA_FACET_ENUMERATION ? enums[rnd() % 5] : fvalue;
            xmlSchemaFacetPtr l = xmlSchemaNewFacet();
            XmlXsd10Facet *n = xml_xsd10_new_facet();
            l->type = (xmlSchemaTypeType) ft; n->type = (XmlXsd10TypeType) ft;
            l->value = xmlStrdup(BAD_CAST v); n->value = keep(strdup(v));
            if (ft != XML_SCHEMA_FACET_PATTERN)
            {
                int r1 = xmlSchemaCheckFacet(l, lt, NULL, NULL);
                int r2 = xml_xsd10_check_facet(n, nt, NULL);
                if (r1 != 0 || r2 != 0) ok = 0;
            }
            if (lf) lf->next = l; else lfirst = l;
            if (nfa) nfa->next = n; else nfirst = n;
            lf = l; nfa = n;
        }
        lt->facets = lfirst; nt->facets = nfirst;
        if (ok)
        {
            int use_msg = rnd() % 5 == 0;
            unsigned long length = rnd() % 10;
            int err = ft == XML_SCHEMA_FACET_ENUMERATION ? XML_SCHEMAV_CVC_ENUMERATION_VALID : code;
            xmlSchemaFacetErr(la, (xmlParserErrors) err, ln, BAD_CAST s1, length, lt, lfirst, use_msg ? msg : NULL, BAD_CAST s2, BAD_CAST s3);
            xml_xsd10_facet_err(na, err, nn, s1, length, nt, nfirst, use_msg ? msg : NULL, s2, s3);
        }
        for (xmlSchemaFacetPtr f = lfirst, nx; f; f = nx) { nx = f->next; xmlFree((xmlChar *) f->value); f->value = NULL; xmlSchemaFreeFacet(f); }
        for (XmlXsd10Facet *f = nfirst, *nx; f; f = nx) { nx = f->next; xml_xsd10_free_facet(f); }
        break;
    }
    case 13:
    {
        int m = rnd() % 2;
        xmlSchemaPMissingAttrErr(c->lp, (xmlParserErrors) code, ip.l, ln, s1 ? s1 : "x", m ? msg : NULL);
        xml_xsd10_p_missing_attr_err(c->np, code, ip.n, nn, s1 ? s1 : "x", m ? msg : NULL);
        break;
    }
    case 14:
    {
        int with_str = rnd() % 2;
        int rt = item_kinds_[rnd() % NUM_ITEM_KINDS];
        xmlSchemaPResCompAttrErr(c->lp, (xmlParserErrors) code, ip.l, ln, "type", BAD_CAST s1, BAD_CAST s2, (xmlSchemaTypeType) rt, with_str ? "simple type" : NULL);
        xml_xsd10_p_res_comp_attr_err(c->np, code, ip.n, nn, "type", s1, s2, (XmlXsd10TypeType) rt, with_str ? "simple type" : NULL);
        break;
    }
    case 15:
    {
        if (!ln || ln->type != XML_ATTRIBUTE_NODE) break;
        int how = rnd() % 3;
        xmlChar *ldes = how == 2 ? xmlStrdup(BAD_CAST "preset des") : NULL;
        char *ndes = how == 2 ? strdup("preset des") : NULL;
        xmlSchemaPCustomAttrErr(c->lp, (xmlParserErrors) code, how ? &ldes : NULL, ip.l, (xmlAttrPtr) ln, msg);
        xml_xsd10_p_custom_attr_err(c->np, code, how ? &ndes : NULL, ip.n, nn, msg);
        check_str("custom attr err des", (const char *) ldes, ndes);
        xmlFree(ldes); free(ndes);
        break;
    }
    case 16:
        if (!ln || ln->type != XML_ATTRIBUTE_NODE) break;
        xmlSchemaPIllegalAttrErr(c->lp, (xmlParserErrors) code, NULL, (xmlAttrPtr) ln);
        xml_xsd10_p_illegal_attr_err(c->np, code, NULL, nn);
        break;
    case 17:
        xmlSchemaPCustomErrExt(c->lp, (xmlParserErrors) code, ip.l, ln, messages_[4], BAD_CAST s1, BAD_CAST s2, BAD_CAST s3);
        xml_xsd10_p_custom_err_ext(c->np, code, ip.n, nn, messages_[4], s1, s2, s3);
        break;
    case 18:
        xmlSchemaPCustomErr(c->lp, (xmlParserErrors) code, ip.l, ln, msg, BAD_CAST s1);
        xml_xsd10_p_custom_err(c->np, code, ip.n, nn, msg, s1);
        break;
    case 19:
    {
        ItemPair au = make_item(XML_SCHEMA_TYPE_ATTRIBUTE_USE, 0);
        xmlSchemaPAttrUseErr4(c->lp, (xmlParserErrors) code, ln, ip.l, (xmlSchemaAttributeUsePtr) au.l, messages_[4], BAD_CAST s1, BAD_CAST s2, BAD_CAST s3, BAD_CAST s4);
        xml_xsd10_p_attr_use_err4(c->np, code, nn, ip.n, (XmlXsd10AttributeUse *) au.n, messages_[4], s1, s2, s3, s4);
        break;
    }
    case 20:
    {
        ItemPair bt = make_item(XML_SCHEMA_TYPE_BASIC, 0);
        if (!bt.l) break;
        xmlSchemaFacet lf; XmlXsd10Facet nf;
        memset(&lf, 0, sizeof(lf)); memset(&nf, 0, sizeof(nf));
        lf.type = nf.type = (XmlXsd10TypeType) (XML_SCHEMA_FACET_MININCLUSIVE + rnd() % 12);
        set_item_node(tp, k);
        xmlSchemaPIllegalFacetAtomicErr(c->lp, (xmlParserErrors) code, (xmlSchemaTypePtr) tp.l, (xmlSchemaTypePtr) bt.l, &lf);
        xml_xsd10_p_illegal_facet_atomic_err(c->np, code, (XmlXsd10Type *) tp.n, (XmlXsd10Type *) bt.n, &nf);
        xmlSchemaPIllegalFacetListUnionErr(c->lp, (xmlParserErrors) code, (xmlSchemaTypePtr) tp.l, &lf);
        xml_xsd10_p_illegal_facet_list_union_err(c->np, code, (XmlXsd10Type *) tp.n, &nf);
        break;
    }
    case 21:
        if (!ln || ln->type != XML_ATTRIBUTE_NODE) break;
        xmlSchemaPMutualExclAttrErr(c->lp, (xmlParserErrors) code, ip.l, (xmlAttrPtr) ln, "ref", "name");
        xml_xsd10_p_mutual_excl_attr_err(c->np, code, ip.n, nn, "ref", "name");
        break;
    case 22:
    {
        if (!ln) break;
        int with_type = rnd() % 3, with_msg = rnd() % 4 == 0, with_exp = rnd() % 2;
        xmlSchemaPSimpleTypeErr(c->lp, (xmlParserErrors) code, NULL, ln, with_type ? (xmlSchemaTypePtr) tp.l : NULL,
                                with_exp ? "(a | b%)" : NULL, BAD_CAST s1, with_msg ? msg : NULL, BAD_CAST s2, BAD_CAST s3);
        xml_xsd10_p_simple_type_err(c->np, code, NULL, nn, with_type ? (XmlXsd10Type *) tp.n : NULL,
                                    with_exp ? "(a | b%)" : NULL, s1, with_msg ? msg : NULL, s2, s3);
        break;
    }
    case 23:
    {
        int with_msg = rnd() % 3 == 0, with_content = rnd() % 2;
        xmlSchemaPContentErr(c->lp, (xmlParserErrors) code, ip.l, ln, LNODE(k2), with_msg ? msg : NULL, with_content ? "(annotation?)" : NULL);
        xml_xsd10_p_content_err(c->np, code, ip.n, nn, NNODE(k2), with_msg ? msg : NULL, with_content ? "(annotation?)" : NULL);
        break;
    }
    case 24:
    {
        /* Keyref errors: the node qnames list and a line. */
        const char *qn = rnd_str(0, 0), *qns = rnd_ns();
        xmlSchemaItemListAdd(c->lv->nodeQNames, (void *) qn);
        xmlSchemaItemListAdd(c->lv->nodeQNames, (void *) qns);
        xml_xsd10_item_list_add(c->nv->nodeQNames, (void *) qn);
        xml_xsd10_item_list_add(c->nv->nodeQNames, (void *) qns);
        xmlSchemaPSVIIDCNode lnode; XmlXsd10PSVIIDCNode nnode;
        memset(&lnode, 0, sizeof(lnode)); memset(&nnode, 0, sizeof(nnode));
        lnode.nodeLine = nnode.nodeLine = rnd() % 50;
        lnode.nodeQNameID = nnode.nodeQNameID = c->nv->nodeQNames->nbItems - 2;
        if (rnd() % 2) { c->lv->doc = ldocs_[0]; c->nv->doc = ndocs_[0]; }
        if (rnd() % 2) { xmlSchemaValidateSetFilename(c->lv, "file.xml"); free(c->nv->filename); c->nv->filename = strdup("file.xml"); }
        xmlSchemaKeyrefErr(c->lv, (xmlParserErrors) code, &lnode, NULL, msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_keyref_err(c->nv, code, &nnode, NULL, msg, s1, s2);
        c->lv->doc = NULL; c->nv->doc = NULL;
        break;
    }
    case 25:
        if (!(on_validator && c->lv->inode) && !ln) break;
        {
            xmlChar *lm = NULL; char *nm = NULL;
            xmlSchemaFormatNodeForError(&lm, la, ln);
            xml_xsd10_format_node_for_error(&nm, na, nn);
            check_str("format node for error", (const char *) lm, nm);
            xmlFree(lm); free(nm);
            check_int("eval error node type", xmlSchemaEvalErrorNodeType(la, ln), xml_xsd10_eval_error_node_type(na, nn));
        }
        break;
    case 26:
        xmlSchemaErr3(la, code, ln, msg, BAD_CAST s1, BAD_CAST s2, BAD_CAST s3);
        xml_xsd10_err3(na, code, nn, msg, s1, s2, s3);
        break;
    case 27:
        xmlSchemaErr(la, code, ln, msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_err(na, code, nn, msg, s1, s2);
        break;
    case 28:
    {
        xmlChar *lb = NULL; char *nb = NULL;
        ItemPair tp2 = make_item(XML_SCHEMA_TYPE_SIMPLE, 0);
        check_int("is global item", xmlSchemaIsGlobalItem((xmlSchemaTypePtr) ip.l), xml_xsd10_is_global_item((XmlXsd10Type *) ip.n));
        check_str("format error node qname", (const char *) xmlSchemaFormatErrorNodeQName(&lb, NULL, ln), xml_xsd10_format_error_node_q_name(&nb, NULL, nn));
        xmlFree(lb); free(nb);
        (void) tp2;
        break;
    }
    default:
        if (!fmt_ok) break;
        xmlSchemaCustomErr(la, (xmlParserErrors) code, ln, NULL, msg, BAD_CAST s1, BAD_CAST s2);
        xml_xsd10_custom_err(na, code, nn, NULL, msg, s1, s2);
        break;
    }
    compare_caps(what);
    check_int("parser nberrors", c->lp->nberrors, c->np->nberrors);
    check_int("parser err", c->lp->err, c->np->err);
    check_int("validator nberrors", c->lv->nberrors, c->nv->nberrors);
    check_int("validator err", c->lv->err, c->nv->err);
}

static void check_memory_errors(void)
{
    for (int mode = 0; mode < 2; mode++)
    {
        Ctxts c;
        make_ctxts(&c, mode);
        xmlSchemaPErrMemory(c.lp);
        xml_xsd10_p_err_memory(c.np);
        xmlSchemaVErrMemory(c.lv);
        xml_xsd10_v_err_memory(c.nv);
        /* Memory errors block further errors. */
        xmlSchemaPErr(c.lp, NULL, 1701, "blocked %s\n", NULL, NULL);
        xml_xsd10_p_err(c.np, NULL, 1701, "blocked %s\n", NULL, NULL);
        compare_caps(mode ? "memory error structured" : "memory error plain");
        check_int("memory nberrors", c.lp->nberrors, c.np->nberrors);
        check_int("memory err", c.lp->err, c.np->err);
        free_ctxts(&c);
    }
}

static void check_errors(int count)
{
    for (int mode = 0; mode < 3; mode++)
    {
        Ctxts c;
        make_ctxts(&c, mode);
        for (int i = 0; i < count; i++) check_errors_once(&c);
        check_int("is valid", xmlSchemaIsValid(c.lv), xml_xsd10_is_valid(c.nv));
        free_ctxts(&c);
    }
    check_memory_errors();
    /* The setters and options. */
    Ctxts c;
    make_ctxts(&c, 0);
    /* (libxml2 xmlSchemaSetValidOptions shifts 1 << 31, undefined behaviour, so only the native one here.) */
    for (int o = 0; o < 6; o++)
    {
        check_int("set valid options", o <= 1 ? 0 : -1, xml_xsd10_set_valid_options(c.nv, o));
        check_int("get valid options", o <= 1 ? o : 1, xml_xsd10_valid_ctxt_get_options(c.nv));
    }
    free_ctxts(&c);
}

int main(int argc, char **argv)
{
    long count = 2000;
    int i = 1;
    for (; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
        {
            unsigned long long seed = 1;
            sscanf(argv[++i], "%ld:%llu", &count, &seed);
            rng_ = seed;
        }
        else break;
    }
    xmlInitParser();
    /* libxml2 reports some facet problems (xmlSchemaCheckFacet without a context) on the generic channel. */
    xmlSetGenericErrorFunc(NULL, l_quiet);
    for (; i < argc; i++) load_doc(argv[i]);
    if (verbose_) printf("%zu documents, %zu nodes\n", docs_len_, nodes_len_);
    if (docs_len_ == 0)
    {
        printf("xml_xsd10_schemas_diff: no documents given\n");
        return 1;
    }

    check_dict((int) count);
    for (int h = 0; h < 20; h++) check_hash((int) count / 4 + 1);
    check_tree((int) count);
    check_format((int) count * 2);
    check_values((int) count);
    check_errors((int) count * 2);

    for (size_t d = 0; d < docs_len_; d++)
    {
        xmlFreeDoc(ldocs_[d]);
        xml_dom_free(ndocs_[d]);
    }
    free(ldocs_); free(ndocs_); free(lnodes_); free(nnodes_);
    free_allocs();
    free_pool();
    xmlCleanupParser();
    printf("xml_xsd10_schemas_diff: %ld same, %ld different\n", same_, diff_);
    fflush(stdout);
    return diff_ ? 1 : 0;
}
