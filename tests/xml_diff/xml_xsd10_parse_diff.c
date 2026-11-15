/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
   Differential test of the schema parsing part of the native XML Schema 1.0 structures
   (parts/xml_xsd10_parse.c) against libxml2 xmlschemas.c.

   libxml2's static functions are reached by including 3rdparty/libxml2/xmlschemas.c
   into this file (its public functions then replace the ones of libxml2.a).

   Every schema document given on the command line (normally 3rdparty/libxml2/test/schemas/ *.xsd)
   is parsed by both up to the point where xmlSchemaParse calls xmlSchemaFixupComponents:
   xmlSchemaNewSchema, the construction context, xmlSchemaAddSchemaDoc (which loads the
   imported/included/redefined documents while parsing) and xmlSchemaParseNewDocWithContext.
   Compared are the return values, all messages (the structured errors: every field, the node
   as line:name, and the xmlFormatError text) and a dump of the context, the buckets (with their
   relations) and every global and local component in bucket order (kinds, names, namespaces,
   flags, particles, model groups, wildcards, attribute uses, facets, identity constraints,
   annotations, value constraints, references between the components and the nodes), the
   pending list and the redefinitions.

   Three ways of giving the main schema are used: the file name (xmlSchemaNewParserCtxt), the
   parsed document (xmlSchemaNewDocParserCtxt, the native document must not be modified, libxml2
   removes the blank text nodes, comments and PIs) and random mutations of the text parsed from
   memory (xmlSchemaNewMemParserCtxt, in the directory of the file so that the relative includes
   and imports are found): attribute values replaced, attributes removed or renamed, elements
   renamed, removed or duplicated, comments, PIs, blank and non blank text, CDATA and elements
   inserted. The mutations keep the documents well formed. Some extra schemas (redefinitions,
   chameleon includes, circular imports, notations, ...) are written to a temporary directory
   and tested the same way.

   Usage: xml_xsd10_parse_diff [-v] [-r count:seed] files...
   (count mutated copies of every file.)
   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#ifndef XSD10_PARSE_DIFF_PASS

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
#include<unistd.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

static int verbose_ = 0;
static long same_ = 0, diff_ = 0;
static int max_shown_ = 20;
static unsigned long long rng_ = 1;

static unsigned rnd(void)
{
    rng_ = rng_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_ >> 33);
}

// Capturing //////////////////////////////////////////////////////

typedef struct { char *s; size_t len, cap; } Cap;

static void cap_add(Cap *c, const char *text)
{
    size_t b = strlen(text);
    if (c->len + b + 1 > c->cap)
    {
        c->cap = (c->len + b + 1) * 2;
        c->s = (char *) realloc(c->s, c->cap);
    }
    memcpy(c->s + c->len, text, b + 1);
    c->len += b;
}

static void cap_printf(Cap *c, const char *fmt, ...)
{
    char buf[70000];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cap_add(c, buf);
}

static void cap_free(Cap *c)
{
    free(c->s);
    c->s = NULL;
    c->len = c->cap = 0;
}

static const char *S(const void *s) { return s ? (const char *) s : "(NULL)"; }

static void l_fmt(void *ctx, const char *msg, ...)
{
    char buf[70000];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(buf, sizeof(buf), msg, ap);
    va_end(ap);
    cap_add((Cap *) ctx, buf);
}
static void l_quiet(void *ctx, const char *msg, ...) { (void) ctx; (void) msg; }

static void l_node_desc(Cap *c, xmlNodePtr n)
{
    if (n == NULL) { cap_add(c, "-"); return; }
    if (n->type == XML_ATTRIBUTE_NODE)
    {
        cap_printf(c, "@%s/", (const char *) n->name);
        n = n->parent;
        if (n == NULL) { cap_add(c, "-"); return; }
    }
    cap_printf(c, "%ld:%s", xmlGetLineNo(n), n->name ? (const char *) n->name : "#");
}

static void n_node_desc(Cap *c, XMQNode *n)
{
    if (n == NULL) { cap_add(c, "-"); return; }
    if (xml_node_kind(n) == XML_DOM_ATTRIBUTE)
    {
        cap_printf(c, "@%s/", xml_node_name(n));
        n = xml_node_parent(n);
        if (n == NULL) { cap_add(c, "-"); return; }
    }
    cap_printf(c, "%ld:%s", xml_xsd10_tree_get_line_no(n), xml_node_name(n) ? xml_node_name(n) : "#");
}

static Cap lcap_, ncap_;

/* The messages of the xml parser (namespace warnings of well formed documents) are not part of the
   schema parser, they are compared by the parser harnesses. */
static int skip_domain(int domain) { return (domain == XML_FROM_PARSER) || (domain == XML_FROM_NAMESPACE) || (domain == XML_FROM_TREE); }

static void l_serr(void *ctx, const xmlError *e)
{
    Cap f = { NULL, 0, 0 };
    (void) ctx;
    if (skip_domain(e->domain)) return;
    xmlFormatError(e, l_fmt, &f);
    cap_printf(&lcap_, "[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=",
               e->domain, e->code, (int) e->level, S(e->file), e->line, S(e->str1), S(e->str2), S(e->str3),
               e->int1, e->int2);
    l_node_desc(&lcap_, (xmlNodePtr) e->node);
    cap_printf(&lcap_, "] %s|FORMATTED|%s\n", S(e->message), f.s ? f.s : "");
    cap_free(&f);
}

static void n_serr(void *ctx, const XmlXsd10Error *e)
{
    char *f;
    (void) ctx;
    if (skip_domain(e->domain)) return;
    f = xml_xsd10_format_error_string(e);
    cap_printf(&ncap_, "[domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=",
               e->domain, e->code, (int) e->level, S(e->file), e->line, S(e->str1), S(e->str2), S(e->str3),
               e->int1, e->int2);
    n_node_desc(&ncap_, e->node);
    cap_printf(&ncap_, "] %s|FORMATTED|%s\n", S(e->message), f ? f : "");
    free(f);
}

static void show_diff(const char *what, const char *l, const char *n)
{
    /* Show the first differing line with some context. */
    const char *a = l, *b = n;
    int line = 1;
    while (*a && *a == *b) { if (*a == '\n') line++; a++; b++; }
    while (a > l && a[-1] != '\n') { a--; b--; }
    const char *ae = strchr(a, '\n'), *be = strchr(b, '\n');
    int al = ae ? (int)(ae - a) : (int) strlen(a), bl = be ? (int)(be - b) : (int) strlen(b);
    printf("DIFF %s (line %d)\n  libxml2: %.*s\n  native:  %.*s\n", what, line, al, a, bl, b);
}

static void check_str(const char *what, const char *l, const char *n)
{
    if (!strcmp(l, n)) { same_++; return; }
    diff_++;
    if (diff_ <= max_shown_) show_diff(what, l, n);
    if (verbose_ > 1) printf("FULL libxml2:\n%s\nFULL native:\n%s\n", l, n);
}

// The component dumps, generated twice (libxml2 and native) from the code at the end. //

typedef struct { const void **p; char **label; size_t len, cap; } Labels;

static void labels_add(Labels *t, const void *p, const char *label)
{
    if (t->len == t->cap)
    {
        t->cap = t->cap ? t->cap * 2 : 256;
        t->p = (const void **) realloc((void *) t->p, t->cap * sizeof(void *));
        t->label = (char **) realloc(t->label, t->cap * sizeof(char *));
    }
    t->p[t->len] = p;
    t->label[t->len] = strdup(label);
    t->len++;
}

static const char *labels_get(Labels *t, const void *p)
{
    for (size_t i = 0; i < t->len; i++) if (t->p[i] == p) return t->label[i];
    return NULL;
}

static void labels_free(Labels *t)
{
    for (size_t i = 0; i < t->len; i++) free(t->label[i]);
    free((void *) t->p);
    free(t->label);
    memset(t, 0, sizeof(*t));
}

#define XSD10_PARSE_DIFF_PASS 1

/* libxml2 */
#define D(n) l_##n
#define T(n) xmlSchema##n *
#define K(n) XML_SCHEMA_##n
#define NODE_T xmlNodePtr
#define NODE_DESC l_node_desc
#define PCTXT_T xmlSchemaParserCtxtPtr
#define LIST_T xmlSchemaItemListPtr
#define SCHEMA_T xmlSchemaPtr
#define BUCKET_T xmlSchemaBucketPtr
#define FREE_SCHEMA xmlSchemaFree
#include __FILE__
#undef D
#undef T
#undef K
#undef NODE_T
#undef NODE_DESC
#undef PCTXT_T
#undef LIST_T
#undef SCHEMA_T
#undef BUCKET_T
#undef FREE_SCHEMA

/* native */
#define D(n) n_##n
#define T(n) XmlXsd10##n *
#define K(n) XML_XSD10_SCHEMA_##n
#define NODE_T XMQNode *
#define NODE_DESC n_node_desc
#define PCTXT_T XmlXsd10ParserCtxt *
#define LIST_T XmlXsd10ItemList *
#define SCHEMA_T XmlXsd10Schema *
#define BUCKET_T XmlXsd10Bucket *
#define FREE_SCHEMA xml_xsd10_free
#include __FILE__
#undef D
#undef T
#undef K
#undef NODE_T
#undef NODE_DESC
#undef PCTXT_T
#undef LIST_T
#undef SCHEMA_T
#undef BUCKET_T
#undef FREE_SCHEMA

// The parse stage (xmlSchemaParse before xmlSchemaFixupComponents) //

static void l_stage(xmlSchemaParserCtxtPtr ctxt, Cap *out)
{
    xmlSchemaPtr mainSchema;
    xmlSchemaBucketPtr bucket = NULL;
    int res, res2 = -99;

    xmlSchemaSetParserStructuredErrors(ctxt, l_serr, NULL);
    xmlSchemaInitTypes();
    ctxt->nberrors = 0;
    ctxt->err = 0;
    ctxt->counter = 0;
    mainSchema = xmlSchemaNewSchema(ctxt);
    ctxt->constructor = xmlSchemaConstructionCtxtCreate(ctxt->dict);
    ctxt->ownsConstructor = 1;
    ctxt->constructor->mainSchema = mainSchema;
    res = xmlSchemaAddSchemaDoc(ctxt, XML_SCHEMA_SCHEMA_MAIN, ctxt->URL, ctxt->doc, ctxt->buffer, ctxt->size,
                                NULL, NULL, NULL, &bucket);
    if ((res == 0) && (bucket == NULL))
        xmlSchemaCustomErr(ACTXT_CAST ctxt, XML_SCHEMAP_FAILED_LOAD, NULL, NULL,
                           "Failed to locate the main schema resource at '%s'", ctxt->URL, NULL);
    else if ((res == 0) && (bucket != NULL))
        res2 = xmlSchemaParseNewDocWithContext(ctxt, mainSchema, bucket);
    cap_printf(out, "add=%d parse=%d\n", res, res2);
    l_dump_ctxt(ctxt, mainSchema, out);
    xmlSchemaConstructionCtxtFree(ctxt->constructor);
    ctxt->constructor = NULL;
    ctxt->ownsConstructor = 0;
    xmlSchemaFree(mainSchema);
    ctxt->schema = NULL;
}

static void n_stage(XmlXsd10ParserCtxt *ctxt, Cap *out)
{
    XmlXsd10Schema *mainSchema;
    XmlXsd10Bucket *bucket = NULL;
    int res, res2 = -99;

    xml_xsd10_set_parser_structured_errors(ctxt, n_serr, NULL);
    ctxt->nberrors = 0;
    ctxt->err = 0;
    ctxt->counter = 0;
    mainSchema = xml_xsd10_new_schema(ctxt);
    ctxt->constructor = xml_xsd10_construction_ctxt_create(ctxt->dict);
    ctxt->ownsConstructor = 1;
    ctxt->constructor->mainSchema = mainSchema;
    res = xml_xsd10_add_schema_doc(ctxt, XML_XSD10_SCHEMA_SCHEMA_MAIN, ctxt->URL, ctxt->doc, ctxt->buffer, ctxt->size,
                                   NULL, NULL, NULL, &bucket);
    if ((res == 0) && (bucket == NULL))
        xml_xsd10_custom_err(XSD10_ACTXT_CAST ctxt, XML_XSD10_SCHEMAP_FAILED_LOAD, NULL, NULL,
                             "Failed to locate the main schema resource at '%s'", ctxt->URL, NULL);
    else if ((res == 0) && (bucket != NULL))
        res2 = xml_xsd10_parse_new_doc_with_context(ctxt, mainSchema, bucket);
    cap_printf(out, "add=%d parse=%d\n", res, res2);
    n_dump_ctxt(ctxt, mainSchema, out);
    xml_xsd10_construction_ctxt_free(ctxt->constructor);
    ctxt->constructor = NULL;
    ctxt->ownsConstructor = 0;
    xml_xsd10_free(mainSchema);
    ctxt->schema = NULL;
}

static long runs_ = 0;

static void compare(const char *what, xmlSchemaParserCtxtPtr lp, XmlXsd10ParserCtxt *np)
{
    Cap ld = { NULL, 0, 0 }, nd = { NULL, 0, 0 };
    runs_++;
    if (lp) l_stage(lp, &ld); else cap_add(&ld, "no context\n");
    if (np) n_stage(np, &nd); else cap_add(&nd, "no context\n");
    cap_add(&ld, "MESSAGES\n");
    cap_add(&ld, lcap_.s ? lcap_.s : "");
    cap_add(&nd, "MESSAGES\n");
    cap_add(&nd, ncap_.s ? ncap_.s : "");
    if (verbose_ > 2) printf("=== %s\n%s", what, ld.s);
    check_str(what, ld.s, nd.s);
    cap_free(&ld);
    cap_free(&nd);
    cap_free(&lcap_);
    cap_free(&ncap_);
    if (lp) xmlSchemaFreeParserCtxt(lp);
    if (np) xml_xsd10_free_parser_ctxt(np);
}

// Mutations //////////////////////////////////////////////////////

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

typedef struct { size_t start, end; int close, empty; size_t name, name_len; } Tag;

/* The tags of the text (comments, PIs, CDATA, doctype skipped). */
static Tag *scan_tags(const char *s, size_t *count)
{
    Tag *tags = NULL;
    size_t n = 0, cap = 0, i = 0, len = strlen(s);
    while (i < len)
    {
        if (s[i] != '<') { i++; continue; }
        if (!strncmp(s + i, "<!--", 4)) { const char *e = strstr(s + i, "-->"); i = e ? (size_t)(e - s) + 3 : len; continue; }
        if (!strncmp(s + i, "<![CDATA[", 9)) { const char *e = strstr(s + i, "]]>"); i = e ? (size_t)(e - s) + 3 : len; continue; }
        if (s[i + 1] == '?' || s[i + 1] == '!')
        {
            /* PI or doctype (internal subsets with '>' are not handled, skip the file then). */
            const char *e = strchr(s + i, '>');
            if (s[i + 1] == '!' && memchr(s + i, '[', e ? (size_t)(e - (s + i)) : 0)) { free(tags); *count = 0; return NULL; }
            i = e ? (size_t)(e - s) + 1 : len;
            continue;
        }
        Tag t;
        memset(&t, 0, sizeof(t));
        t.start = i;
        t.close = s[i + 1] == '/';
        t.name = i + 1 + t.close;
        size_t j = t.name;
        while (j < len && !strchr(" \t\r\n/>", s[j])) j++;
        t.name_len = j - t.name;
        char q = 0;
        while (j < len && (q || s[j] != '>'))
        {
            if (q) { if (s[j] == q) q = 0; }
            else if (s[j] == '"' || s[j] == '\'') q = s[j];
            j++;
        }
        t.end = j + 1;
        t.empty = j > 0 && s[j - 1] == '/';
        if (n == cap) { cap = cap ? cap * 2 : 64; tags = (Tag *) realloc(tags, cap * sizeof(Tag)); }
        tags[n++] = t;
        i = t.end;
    }
    *count = n;
    return tags;
}

/* The index of the matching end tag of start tag k (k itself if empty), -1 if none. */
static long match_end(Tag *tags, size_t n, size_t k)
{
    if (tags[k].empty) return (long) k;
    int depth = 0;
    for (size_t i = k; i < n; i++)
    {
        if (tags[i].close) { depth--; if (depth == 0) return (long) i; }
        else if (!tags[i].empty) depth++;
    }
    return -1;
}

static char *splice(const char *s, size_t from, size_t to, const char *ins)
{
    size_t len = strlen(s), il = strlen(ins);
    char *r = (char *) malloc(len - (to - from) + il + 1);
    memcpy(r, s, from);
    memcpy(r + from, ins, il);
    memcpy(r + from + il, s + to, len - to + 1);
    return r;
}

static const char *values_[] = {
    "unbounded", "0", "1", "2", "-1", " 1 ", "x", "", "qualified", "unqualified", "#all", "extension restriction",
    "list union", "substitution", "##any", "##other", "##local ##targetNamespace", "##other ##any", "http://foo",
    "strict", "skip", "lax", "true", "false", "optional", "prohibited", "required", "xs:string", "xsd:string",
    "xs:int", "foo:bar", "xs:anyType", "tns:a", "a", "b", "xmlns", ".//a", "@b", "a/b", "child::a", "..", "|",
    "a b", "nothere.xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/2001/XMLSchema-instance",
    "3.5", "en", "preserve", "collapse", "[a-z]+", "(", "id1"
};
static const char *anames_[] = {
    "name", "ref", "type", "minOccurs", "maxOccurs", "use", "default", "fixed", "form", "base", "itemType",
    "memberTypes", "namespace", "processContents", "abstract", "nillable", "block", "final", "mixed",
    "substitutionGroup", "targetNamespace", "elementFormDefault", "attributeFormDefault", "blockDefault",
    "finalDefault", "schemaLocation", "id", "value", "xpath", "refer", "public", "system", "version", "foo"
};
static const char *enames_[] = {
    "element", "attribute", "complexType", "simpleType", "sequence", "choice", "all", "group", "attributeGroup",
    "any", "anyAttribute", "annotation", "documentation", "appinfo", "restriction", "extension", "simpleContent",
    "complexContent", "list", "union", "unique", "key", "keyref", "selector", "field", "notation", "include",
    "import", "redefine", "enumeration", "pattern", "length", "minInclusive", "whiteSpace", "schema", "foo"
};
static const char *inserts_[] = {
    "<!-- c -->", "<?pi x?>", "  \n  ", "text", "<![CDATA[x]]>", "<![CDATA[ ]]>", "<xs:annotation/>",
    "<xs:annotation><xs:documentation>d</xs:documentation></xs:annotation>", "<xs:element name=\"ins\"/>",
    "<foo/>", "<xs:attribute name=\"ia\"/>", "<xs:sequence/>", "<xs:element ref=\"ins\"/>",
    "<xs:anyAttribute/>", "<xs:any/>", "<xs:simpleType><xs:restriction base=\"xs:string\"/></xs:simpleType>",
    "<xs:complexType/>", "<xs:group ref=\"g\"/>", "<xs:attributeGroup ref=\"ag\"/>", "&#32;"
};
#define NUM(a) (sizeof(a) / sizeof(a[0]))

static char *mutate_once(const char *s)
{
    size_t n;
    Tag *tags = scan_tags(s, &n);
    if (!tags || n < 2) { free(tags); return NULL; }
    /* The root element: the first start tag, the last end tag. */
    size_t root = 0;
    char *r = NULL;
    size_t k = root + rnd() % (n - root);
    Tag *t = &tags[k];
    switch (rnd() % 8)
    {
    case 0: case 1: case 2: /* replace, remove or rename an attribute */
    {
        if (t->close) break;
        /* Find the attributes. */
        size_t j = t->name + t->name_len, attrs[64][2];
        int na = 0;
        while (j < t->end && na < 64)
        {
            while (j < t->end && strchr(" \t\r\n", s[j])) j++;
            if (s[j] == '/' || s[j] == '>') break;
            size_t as = j;
            while (j < t->end && s[j] != '=') j++;
            j++;
            while (j < t->end && strchr(" \t\r\n", s[j])) j++;
            char q = s[j];
            if (q != '"' && q != '\'') break;
            j++;
            while (j < t->end && s[j] != q) j++;
            j++;
            attrs[na][0] = as;
            attrs[na][1] = j;
            na++;
        }
        unsigned what = rnd() % 4;
        if (na == 0 || what == 3)
        {
            char buf[200];
            snprintf(buf, sizeof(buf), " %s=\"%s\"", anames_[rnd() % NUM(anames_)], values_[rnd() % NUM(values_)]);
            r = splice(s, t->name + t->name_len, t->name + t->name_len, buf);
            break;
        }
        int a = (int)(rnd() % (unsigned) na);
        if (what == 0)
        {
            /* new value */
            size_t eq = attrs[a][0];
            while (s[eq] != '=') eq++;
            char buf[200];
            snprintf(buf, sizeof(buf), "=\"%s\"", values_[rnd() % NUM(values_)]);
            r = splice(s, eq, attrs[a][1], buf);
        }
        else if (what == 1)
        {
            /* remove (not the namespace declarations) */
            if (!strncmp(s + attrs[a][0], "xmlns", 5)) break;
            r = splice(s, attrs[a][0], attrs[a][1], "");
        }
        else
        {
            /* rename */
            size_t eq = attrs[a][0];
            while (s[eq] != '=') eq++;
            if (!strncmp(s + attrs[a][0], "xmlns", 5)) break;
            const char *nn = anames_[rnd() % NUM(anames_)];
            /* avoid duplicate attributes */
            char look[64];
            snprintf(look, sizeof(look), " %s=", nn);
            char *tagtext = strndup(s + t->start, t->end - t->start);
            int dup = strstr(tagtext, look) != NULL;
            free(tagtext);
            if (dup) break;
            r = splice(s, attrs[a][0], eq, nn);
        }
        break;
    }
    case 3: /* rename an element (keep its prefix) */
    {
        if (t->close) break;
        long e = match_end(tags, n, k);
        if (e < 0) break;
        const char *nn = enames_[rnd() % NUM(enames_)];
        const char *colon = (const char *) memchr(s + t->name, ':', t->name_len);
        size_t ls = colon ? (size_t)(colon - s) + 1 : t->name;
        size_t le = t->name + t->name_len;
        char *a;
        if ((size_t) e != k)
        {
            Tag *et = &tags[e];
            const char *ec = (const char *) memchr(s + et->name, ':', et->name_len);
            size_t els = ec ? (size_t)(ec - s) + 1 : et->name;
            a = splice(s, els, et->name + et->name_len, nn);
        }
        else a = strdup(s);
        r = splice(a, ls, le, nn);
        free(a);
        break;
    }
    case 4: /* remove an element */
    {
        if (t->close || k == root) break;
        long e = match_end(tags, n, k);
        if (e < 0) break;
        r = splice(s, t->start, tags[e].end, "");
        break;
    }
    case 5: /* duplicate an element */
    {
        if (t->close || k == root) break;
        long e = match_end(tags, n, k);
        if (e < 0) break;
        char *copy = strndup(s + t->start, tags[e].end - t->start);
        r = splice(s, tags[e].end, tags[e].end, copy);
        free(copy);
        break;
    }
    default: /* insert something after a tag inside the root element */
    {
        if (k + 1 >= n) break;
        r = splice(s, t->end, t->end, inserts_[rnd() % NUM(inserts_)]);
        break;
    }
    }
    free(tags);
    return r;
}

static char *mutate(const char *s)
{
    char *cur = strdup(s);
    int times = 1 + (int)(rnd() % 3);
    for (int i = 0; i < times; i++)
    {
        char *m = NULL;
        for (int tries = 0; tries < 10 && !m; tries++) m = mutate_once(cur);
        if (!m) break;
        free(cur);
        cur = m;
    }
    return cur;
}

// Main ///////////////////////////////////////////////////////////

/* All the nodes of a native tree with their kinds, names, contents and attributes. */
static void n_fingerprint(Cap *c, XMQNode *n)
{
    for (; n != NULL; n = xml_node_next(n))
    {
        cap_printf(c, "(%d %s %s", (int) xml_node_kind(n), S(xml_node_name(n)), S(xml_node_content(n)));
        if (xml_node_kind(n) == XML_DOM_ELEMENT)
            for (XMQAttr *a = xml_node_first_attr(n); a != NULL; a = xml_attr_next(a))
                cap_printf(c, " @%s=%s", xml_attr_name(a), S(xml_attr_simple_value(a)));
        n_fingerprint(c, xml_node_first_child(n));
        cap_add(c, ")");
    }
}

static void run_file(const char *file, long count)
{
    char what[1200];
    char cwd[4096];

    /* By file name. */
    snprintf(what, sizeof(what), "%s (file)", file);
    compare(what, xmlSchemaNewParserCtxt(file), xml_xsd10_new_parser_ctxt(file));

    /* By document. */
    xmlDocPtr ldoc = xmlReadFile(file, NULL, XML_PARSE_NOENT | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlDom *ndoc = xml_xsd10_default_loader(NULL, file);
    if (ldoc && ndoc)
    {
        Cap before = { NULL, 0, 0 }, after = { NULL, 0, 0 };
        n_fingerprint(&before, xml_dom_document(ndoc));
        snprintf(what, sizeof(what), "%s (doc)", file);
        compare(what, xmlSchemaNewDocParserCtxt(ldoc), xml_xsd10_new_doc_parser_ctxt(ndoc));
        /* The native parse must not modify the document of the user. */
        n_fingerprint(&after, xml_dom_document(ndoc));
        snprintf(what, sizeof(what), "%s (doc not modified)", file);
        check_str(what, before.s ? before.s : "", after.s ? after.s : "");
        cap_free(&before);
        cap_free(&after);
    }
    if (ldoc) xmlFreeDoc(ldoc);
    if (ndoc) xml_dom_free(ndoc);

    /* Mutated, from memory in the directory of the file. */
    size_t len;
    char *text = read_file(file, &len);
    if (!text) return;
    if (!getcwd(cwd, sizeof(cwd))) { free(text); return; }
    char *dir = strdup(file);
    char *slash = strrchr(dir, '/');
    const char *base = file;
    if (slash) { *slash = 0; base = slash + 1; if (chdir(dir) != 0) { free(dir); free(text); return; } }
    for (long i = 0; i < count; i++)
    {
        unsigned long long seed = rng_;
        char *m = mutate(text);
        /* Skip documents that libxml2 does not consider well formed (the parsers are compared elsewhere). */
        xmlDocPtr check = xmlReadMemory(m, (int) strlen(m), NULL, NULL, XML_PARSE_NOENT | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        if (check)
        {
            xmlFreeDoc(check);
            snprintf(what, sizeof(what), "%s (mutation %ld seed %llu)", base, i, seed);
            compare(what, xmlSchemaNewMemParserCtxt(m, (int) strlen(m)), xml_xsd10_new_mem_parser_ctxt(m, (int) strlen(m)));
            if (verbose_ > 3) printf("MUTATED %s:\n%s\n", what, m);
        }
        free(m);
    }
    if (slash && chdir(cwd) != 0) perror("chdir");
    free(dir);
    free(text);
}


/* Extra schemas (written to a temporary directory) for what the libxml2 test schemas do not
   exercise: redefinitions, chameleon includes, includes/imports of each other, notations. */
static const char *extra_[][2] = {
    { "x_base.xsd",
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' xmlns:r='urn:r' targetNamespace='urn:r'"
      " elementFormDefault='qualified' attributeFormDefault='unqualified' blockDefault='#all' finalDefault='extension'>\n"
      " <xs:simpleType name='st'><xs:restriction base='xs:string'><xs:maxLength value='10'/></xs:restriction></xs:simpleType>\n"
      " <xs:complexType name='ct'><xs:sequence><xs:element name='a' type='xs:string'/></xs:sequence>"
      "<xs:attribute name='x' type='xs:int'/></xs:complexType>\n"
      " <xs:group name='g'><xs:sequence><xs:element name='b' type='r:st'/></xs:sequence></xs:group>\n"
      " <xs:attributeGroup name='ag'><xs:attribute name='y' type='xs:string' default='d'/></xs:attributeGroup>\n"
      " <xs:element name='root' type='r:ct'/>\n"
      "</xs:schema>\n" },
    { "x_cham.xsd",
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>\n"
      " <xs:element name='ce' type='ctype'/>\n"
      " <xs:complexType name='ctype'><xs:choice minOccurs='0' maxOccurs='unbounded'><xs:element ref='ce'/>"
      "<xs:any namespace='urn:q ##local ##targetNamespace' processContents='lax'/></xs:choice>"
      "<xs:anyAttribute namespace='##targetNamespace'/></xs:complexType>\n"
      " <xs:attribute name='ca' type='xs:QName'/>\n"
      "</xs:schema>\n" },
    { "x_redef.xsd",
      "<?xml version='1.0'?>\n<!-- a comment -->\n"
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' xmlns:r='urn:r' targetNamespace='urn:r'>\n"
      " <!-- c --> <?pi x?>\n"
      " <xs:include schemaLocation='x_cham.xsd'/>\n"
      " <xs:redefine schemaLocation='x_base.xsd'>\n"
      "  <xs:annotation><xs:documentation xml:lang='en'>redefined</xs:documentation></xs:annotation>\n"
      "  <xs:simpleType name='st'><xs:restriction base='r:st'><xs:minLength value='1'/></xs:restriction></xs:simpleType>\n"
      "  <xs:complexType name='ct'><xs:complexContent><xs:extension base='r:ct'><xs:sequence>"
      "<xs:element name='c' type='xs:int' minOccurs='0'/></xs:sequence></xs:extension></xs:complexContent></xs:complexType>\n"
      "  <xs:group name='g'><xs:sequence><xs:group ref='r:g'/><xs:element name='d'/></xs:sequence></xs:group>\n"
      "  <xs:attributeGroup name='ag'><xs:attributeGroup ref='r:ag'/><xs:attribute name='z'/></xs:attributeGroup>\n"
      " </xs:redefine>\n"
      " <xs:import namespace='urn:q' schemaLocation='x_q.xsd'/>\n"
      " <xs:import namespace='urn:q' schemaLocation='x_q2.xsd'/>\n"
      " <xs:import/>\n"
      " <xs:element name='top' substitutionGroup='r:root' block='substitution' final='#all' abstract='true' nillable='true'>\n"
      "  <xs:complexType mixed='true'><xs:all minOccurs='0'><xs:element name='e1' fixed='f'/><xs:element name='e2' default='x'/></xs:all>"
      "<xs:attribute ref='r:ca'/><xs:attribute name='p' use='prohibited'/><xs:attributeGroup ref='r:ag'/></xs:complexType>\n"
      "  <xs:unique name='u'><xs:selector xpath='.//r:e1|r:e2'/><xs:field xpath='@p'/><xs:field xpath='.'/></xs:unique>\n"
      "  <xs:key name='k'><xs:selector xpath='r:e1'/><xs:field xpath='@x'/></xs:key>\n"
      "  <xs:keyref name='kr' refer='r:k'><xs:selector xpath='r:e2'/><xs:field xpath='@y'/></xs:keyref>\n"
      " </xs:element>\n"
      " <xs:simpleType name='un'><xs:union memberTypes='xs:int r:st'><xs:simpleType><xs:list itemType='xs:date'/></xs:simpleType>"
      "</xs:union></xs:simpleType>\n"
      " <xs:simpleType name='li' final='list restriction'><xs:list><xs:simpleType><xs:restriction base='xs:decimal'>"
      "<xs:totalDigits value='3'/><xs:fractionDigits value='1' fixed='true'/><xs:enumeration value='1.5'/>"
      "<xs:pattern value='[0-9.]+'/><xs:minInclusive value='0'/><xs:maxExclusive value='99'/><xs:whiteSpace value='collapse'/>"
      "</xs:restriction></xs:simpleType></xs:list></xs:simpleType>\n"
      " <xs:complexType name='sc' block='extension'><xs:simpleContent><xs:extension base='xs:string'>"
      "<xs:attribute name='lang' type='xs:language' use='required'/></xs:extension></xs:simpleContent></xs:complexType>\n"
      " <xs:complexType name='sc2'><xs:simpleContent><xs:restriction base='r:sc'><xs:simpleType><xs:restriction base='xs:string'/>"
      "</xs:simpleType><xs:length value='2'/><xs:attribute name='lang' use='optional'/><xs:anyAttribute/></xs:restriction>"
      "</xs:simpleContent></xs:complexType>\n"
      " <xs:complexType name='cc' abstract='true'><xs:complexContent mixed='false'><xs:restriction base='xs:anyType'>"
      "<xs:sequence><xs:any minOccurs='0' maxOccurs='2'/></xs:sequence></xs:restriction></xs:complexContent></xs:complexType>\n"
      " <xs:notation name='gif' public='image/gif' system='view.exe'/>\n"
      " <xs:notation name='jpg'><xs:annotation/></xs:notation>\n"
      " <xs:attribute name='ga' type='r:un' fixed='1'/>\n"
      " <xs:attributeGroup name='ag2'><xs:attribute ref='r:ga'/><xs:anyAttribute namespace='##any'/></xs:attributeGroup>\n"
      " <xs:group name='g2'><xs:choice><xs:element ref='r:top' maxOccurs='3'/><xs:group ref='r:g'/></xs:choice></xs:group>\n"
      "</xs:schema>\n" },
    { "x_q.xsd",
      "<schema xmlns='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:q' xmlns:q='urn:q'>\n"
      " <import namespace='urn:r' schemaLocation='x_redef.xsd'/>\n"
      " <include schemaLocation='x_cham.xsd'/>\n"
      " <element name='qe' type='q:ctype'/>\n"
      "</schema>\n" },
    { "x_q2.xsd",
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:q'/>\n" },
    { "x_bad.xsd",
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:bad'>\n"
      " <xs:include schemaLocation='x_base.xsd'/>\n"
      " <xs:import namespace='urn:bad' schemaLocation='x_q2.xsd'/>\n"
      " <xs:import schemaLocation='x_cham.xsd'/>\n"
      " <xs:redefine schemaLocation='x_cham.xsd'><xs:element name='no'/></xs:redefine>\n"
      " <xs:include schemaLocation='x_notschema.xsd'/>\n"
      " <xs:include schemaLocation='x_bad.xsd'/>\n"
      " <xs:redefine schemaLocation='x_none.xsd'/>\n"
      " <xs:element name='e' type='q:t'/>\n"
      "</xs:schema>\n" },
    { "x_notschema.xsd", "<root/>\n" },
};

static void run_extra(long count)
{
    char dir[] = "/tmp/xml_xsd10_parse_diff_XXXXXX";
    char path[200];
    if (!mkdtemp(dir)) { perror("mkdtemp"); return; }
    for (size_t i = 0; i < NUM(extra_); i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, extra_[i][0]);
        FILE *f = fopen(path, "wb");
        if (!f) continue;
        fputs(extra_[i][1], f);
        fclose(f);
    }
    for (size_t i = 0; i < NUM(extra_); i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, extra_[i][0]);
        run_file(path, count * 5);
    }
    for (size_t i = 0; i < NUM(extra_); i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, extra_[i][0]);
        unlink(path);
    }
    rmdir(dir);
}

int main(int argc, char **argv)
{
    long count = 20;
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
    if (i >= argc)
    {
        printf("xml_xsd10_parse_diff: no schema documents given\n");
        return 1;
    }
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, l_quiet);
    xml_xsd10_install_regexp_hooks();
    /* The native default loader prints the problems of the documents on stderr (like libxml2
       without a structured handler), keep the output readable. */
    if (!verbose_ && !freopen("/dev/null", "w", stderr)) return 1;
    for (; i < argc; i++) run_file(argv[i], count);
    run_extra(count);
    xmlCleanupParser();
    printf("xml_xsd10_parse_diff: %ld runs, %ld same, %ld different\n", runs_, same_, diff_);
    fflush(stdout);
    return diff_ ? 1 : 0;
}

#else // XSD10_PARSE_DIFF_PASS: the dumps, compiled once for libxml2 and once for the native types.

static void D(ptr)(Labels *t, Cap *c, const void *p)
{
    const char *l;
    if (p == NULL) { cap_add(c, "-"); return; }
    l = labels_get(t, p);
    if (l) { cap_add(c, l); return; }
    /* A built in type. */
    {
        T(Type) ty = (T(Type)) p;
        if (ty->type == K(TYPE_BASIC)) { cap_printf(c, "builtin:%s", S(ty->name)); return; }
    }
    cap_add(c, "?unknown");
}

static void D(annot)(Cap *c, T(Annot) a)
{
    if (a == NULL) return;
    cap_add(c, " annot=");
    for (; a != NULL; a = a->next) { NODE_DESC(c, a->content); cap_add(c, a->next ? "," : ""); }
}

static void D(list)(Labels *t, Cap *c, const char *what, LIST_T list)
{
    if (list == NULL) return;
    cap_printf(c, " %s=[", what);
    for (int i = 0; i < list->nbItems; i++)
    {
        if (i) cap_add(c, ",");
        D(ptr)(t, c, list->items[i]);
    }
    cap_add(c, "]");
}

static void D(wildns)(Cap *c, const char *what, T(WildcardNs) ns)
{
    if (ns == NULL) return;
    cap_printf(c, " %s=[", what);
    for (; ns != NULL; ns = ns->next) cap_printf(c, "%s%s", S(ns->value), ns->next ? "," : "");
    cap_add(c, "]");
}

static void D(idcsel)(Cap *c, const char *what, T(IDCSelect) sel)
{
    if (sel == NULL) return;
    cap_printf(c, " %s=[", what);
    for (; sel != NULL; sel = sel->next)
        cap_printf(c, "{xpath=%s index=%d comp=%d}", S(sel->xpath), sel->index, sel->xpathComp != NULL);
    cap_add(c, "]");
}

static void D(item)(Labels *t, Cap *c, T(BasicItem) item)
{
    cap_printf(c, "kind=%d", (int) item->type);
    switch (item->type)
    {
    case K(TYPE_SIMPLE):
    case K(TYPE_COMPLEX):
    {
        T(Type) ty = (T(Type)) item;
        cap_printf(c, " name=%s tns=%s flags=%x content=%d base=%s baseNs=%s builtin=%d", S(ty->name),
                   S(ty->targetNamespace), (unsigned) ty->flags, (int) ty->contentType, S(ty->base), S(ty->baseNs),
                   ty->builtInType);
        cap_add(c, " subtypes="); D(ptr)(t, c, ty->subtypes);
        cap_add(c, " baseType="); D(ptr)(t, c, ty->baseType);
        cap_add(c, " contentTypeDef="); D(ptr)(t, c, ty->contentTypeDef);
        cap_add(c, " wildcard="); D(ptr)(t, c, ty->attributeWildcard);
        cap_add(c, " next="); D(ptr)(t, c, ty->next);
        for (T(Facet) f = ty->facets; f != NULL; f = f->next)
        {
            cap_printf(c, " facet{type=%d value=%s fixed=%d ws=%d val=%d node=", (int) f->type, S(f->value), f->fixed,
                       f->whitespace, f->val != NULL);
            NODE_DESC(c, f->node);
            D(annot)(c, f->annot);
            cap_add(c, "}");
        }
        if (ty->memberTypes)
        {
            cap_add(c, " members=[");
            for (T(TypeLink) l = ty->memberTypes; l != NULL; l = l->next) { D(ptr)(t, c, l->type); cap_add(c, l->next ? "," : ""); }
            cap_add(c, "]");
        }
        if (ty->facetSet) cap_add(c, " facetSet");
        D(list)(t, c, "attrUses", (LIST_T) ty->attrUses);
        cap_add(c, " node="); NODE_DESC(c, ty->node);
        D(annot)(c, ty->annot);
        break;
    }
    case K(TYPE_ELEMENT):
    {
        T(Element) e = (T(Element)) item;
        cap_printf(c, " name=%s tns=%s flags=%x namedType=%s namedTypeNs=%s substGroup=%s substGroupNs=%s value=%s scope=%s",
                   S(e->name), S(e->targetNamespace), (unsigned) e->flags, S(e->namedType), S(e->namedTypeNs),
                   S(e->substGroup), S(e->substGroupNs), S(e->value), S(e->scope));
        cap_add(c, " subtypes="); D(ptr)(t, c, e->subtypes);
        cap_add(c, " refDecl="); D(ptr)(t, c, e->refDecl);
        cap_add(c, " idcs=[");
        for (T(IDC) idc = (T(IDC)) e->idcs; idc != NULL; idc = idc->next) { D(ptr)(t, c, idc); cap_add(c, idc->next ? "," : ""); }
        cap_add(c, "] node="); NODE_DESC(c, e->node);
        D(annot)(c, e->annot);
        break;
    }
    case K(TYPE_ATTRIBUTE):
    {
        T(Attribute) a = (T(Attribute)) item;
        cap_printf(c, " name=%s tns=%s typeName=%s typeNs=%s flags=%x defValue=%s", S(a->name), S(a->targetNamespace),
                   S(a->typeName), S(a->typeNs), (unsigned) a->flags, S(a->defValue));
        cap_add(c, " subtypes="); D(ptr)(t, c, a->subtypes);
        cap_add(c, " node="); NODE_DESC(c, a->node);
        D(annot)(c, a->annot);
        break;
    }
    case K(TYPE_ATTRIBUTE_USE):
    {
        T(AttributeUse) u = (T(AttributeUse)) item;
        cap_printf(c, " occurs=%d flags=%x defValue=%s", u->occurs, (unsigned) u->flags, S(u->defValue));
        cap_add(c, " decl="); D(ptr)(t, c, u->attrDecl);
        cap_add(c, " next="); D(ptr)(t, c, u->next);
        cap_add(c, " node="); NODE_DESC(c, u->node);
        D(annot)(c, u->annot);
        break;
    }
    case K(EXTRA_ATTR_USE_PROHIB):
    {
        T(AttributeUseProhib) p = (T(AttributeUseProhib)) item;
        cap_printf(c, " name=%s tns=%s isRef=%d node=", S(p->name), S(p->targetNamespace), p->isRef);
        NODE_DESC(c, p->node);
        break;
    }
    case K(TYPE_ATTRIBUTEGROUP):
    {
        T(AttributeGroup) g = (T(AttributeGroup)) item;
        cap_printf(c, " name=%s tns=%s flags=%x", S(g->name), S(g->targetNamespace), (unsigned) g->flags);
        D(list)(t, c, "attrUses", (LIST_T) g->attrUses);
        cap_add(c, " wildcard="); D(ptr)(t, c, g->attributeWildcard);
        cap_add(c, " node="); NODE_DESC(c, g->node);
        D(annot)(c, g->annot);
        break;
    }
    case K(TYPE_PARTICLE):
    {
        T(Particle) p = (T(Particle)) item;
        cap_printf(c, " min=%d max=%d", p->minOccurs, p->maxOccurs);
        cap_add(c, " term="); D(ptr)(t, c, p->children);
        cap_add(c, " next="); D(ptr)(t, c, p->next);
        cap_add(c, " node="); NODE_DESC(c, p->node);
        D(annot)(c, p->annot);
        break;
    }
    case K(TYPE_SEQUENCE):
    case K(TYPE_CHOICE):
    case K(TYPE_ALL):
    {
        T(ModelGroup) g = (T(ModelGroup)) item;
        cap_add(c, " first="); D(ptr)(t, c, g->children);
        cap_add(c, " next="); D(ptr)(t, c, g->next);
        cap_add(c, " node="); NODE_DESC(c, g->node);
        D(annot)(c, g->annot);
        break;
    }
    case K(TYPE_GROUP):
    {
        T(ModelGroupDef) g = (T(ModelGroupDef)) item;
        cap_printf(c, " name=%s tns=%s flags=%x", S(g->name), S(g->targetNamespace), (unsigned) g->flags);
        cap_add(c, " model="); D(ptr)(t, c, g->children);
        cap_add(c, " node="); NODE_DESC(c, g->node);
        D(annot)(c, g->annot);
        break;
    }
    case K(TYPE_ANY):
    case K(TYPE_ANY_ATTRIBUTE):
    {
        T(Wildcard) w = (T(Wildcard)) item;
        cap_printf(c, " pc=%d any=%d flags=%x", w->processContents, w->any, (unsigned) w->flags);
        D(wildns)(c, "nsSet", w->nsSet);
        D(wildns)(c, "negNsSet", w->negNsSet);
        cap_add(c, " node="); NODE_DESC(c, w->node);
        D(annot)(c, w->annot);
        break;
    }
    case K(EXTRA_QNAMEREF):
    {
        T(QNameRef) r = (T(QNameRef)) item;
        cap_printf(c, " itemType=%d name=%s tns=%s", (int) r->itemType, S(r->name), S(r->targetNamespace));
        cap_add(c, " item="); D(ptr)(t, c, r->item);
        cap_add(c, " node="); NODE_DESC(c, r->node);
        break;
    }
    case K(TYPE_IDC_UNIQUE):
    case K(TYPE_IDC_KEY):
    case K(TYPE_IDC_KEYREF):
    {
        T(IDC) idc = (T(IDC)) item;
        cap_printf(c, " name=%s tns=%s nbFields=%d", S(idc->name), S(idc->targetNamespace), idc->nbFields);
        D(idcsel)(c, "selector", idc->selector);
        D(idcsel)(c, "fields", idc->fields);
        cap_add(c, " ref="); D(ptr)(t, c, idc->ref);
        cap_add(c, " node="); NODE_DESC(c, idc->node);
        D(annot)(c, idc->annot);
        break;
    }
    case K(TYPE_NOTATION):
    {
        T(Notation) nt = (T(Notation)) item;
        cap_printf(c, " name=%s tns=%s identifier=%s", S(nt->name), S(nt->targetNamespace), S(nt->identifier));
        D(annot)(c, nt->annot);
        break;
    }
    default:
        break;
    }
    cap_add(c, "\n");
}

static void D(dump_ctxt)(PCTXT_T ctxt, SCHEMA_T schema, Cap *c)
{
    Labels t;
    LIST_T buckets;
    memset(&t, 0, sizeof(t));
    cap_printf(c, "ctxt nberrors=%d err=%d counter=%d stop=%d isS4S=%d isRedefine=%d\n", ctxt->nberrors, ctxt->err,
               ctxt->counter, ctxt->stop, ctxt->isS4S, ctxt->isRedefine);
    cap_printf(c, "schema tns=%s flags=%x includes=%d\n", S(schema->targetNamespace), (unsigned) schema->flags,
               schema->includes ? ((LIST_T) schema->includes)->nbItems : -1);
    if (ctxt->constructor == NULL) { cap_add(c, "no constructor\n"); return; }
    buckets = ctxt->constructor->buckets;
    /* Labels: buckets B<i>, globals B<i>G<j>, locals B<i>L<j>. */
    for (int b = 0; buckets && b < buckets->nbItems; b++)
    {
        BUCKET_T bk = (BUCKET_T) buckets->items[b];
        char buf[64];
        snprintf(buf, sizeof(buf), "B%d", b);
        labels_add(&t, bk, buf);
        for (int j = 0; bk->globals && j < bk->globals->nbItems; j++)
        {
            snprintf(buf, sizeof(buf), "B%dG%d", b, j);
            labels_add(&t, bk->globals->items[j], buf);
        }
        for (int j = 0; bk->locals && j < bk->locals->nbItems; j++)
        {
            snprintf(buf, sizeof(buf), "B%dL%d", b, j);
            labels_add(&t, bk->locals->items[j], buf);
        }
    }
    for (int b = 0; buckets && b < buckets->nbItems; b++)
    {
        BUCKET_T bk = (BUCKET_T) buckets->items[b];
        cap_printf(c, "B%d type=%d flags=%x location=%s origTns=%s tns=%s located=%d parsed=%d imported=%d preserve=%d doc=%d\n",
                   b, bk->type, (unsigned) bk->flags, S(bk->schemaLocation), S(bk->origTargetNamespace),
                   S(bk->targetNamespace), bk->located, bk->parsed, bk->imported, bk->preserveDoc, bk->doc != NULL);
        for (T(SchemaRelation) r = bk->relations; r != NULL; r = r->next)
        {
            cap_printf(c, "  relation type=%d importNs=%s bucket=", r->type, S(r->importNamespace));
            D(ptr)(&t, c, r->bucket);
            cap_add(c, "\n");
        }
        for (int j = 0; bk->globals && j < bk->globals->nbItems; j++)
        {
            cap_printf(c, "  B%dG%d ", b, j);
            D(item)(&t, c, (T(BasicItem)) bk->globals->items[j]);
        }
        for (int j = 0; bk->locals && j < bk->locals->nbItems; j++)
        {
            cap_printf(c, "  B%dL%d ", b, j);
            D(item)(&t, c, (T(BasicItem)) bk->locals->items[j]);
        }
    }
    D(list)(&t, c, "pending", ctxt->constructor->pending);
    cap_add(c, "\n");
    for (T(Redef) r = ctxt->constructor->redefs; r != NULL; r = r->next)
    {
        cap_add(c, "redef item=");
        D(ptr)(&t, c, r->item);
        cap_add(c, " reference=");
        D(ptr)(&t, c, r->reference);
        cap_add(c, " target=");
        D(ptr)(&t, c, r->target);
        cap_printf(c, " refName=%s refTns=%s bucket=", S(r->refName), S(r->refTargetNs));
        D(ptr)(&t, c, r->targetBucket);
        cap_add(c, "\n");
    }
    cap_printf(c, "substGroups=%d\n", ctxt->constructor->substGroups != NULL);
    labels_free(&t);
}

#endif // XSD10_PARSE_DIFF_PASS
