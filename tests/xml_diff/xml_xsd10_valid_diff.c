/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
   Differential test of the native XML Schema 1.0 validation (parts/xml_xsd10_valid.c,
   together with the parse and build parts) against libxml2 xmlschemas.c.

   For every schema/instance pair of the libxml2 schema test suite (test/schemas/ *_*.xsd
   with the instances *_N.xml, paired like runtest.c schemasTest does, plus the
   xsi assembly test any1_0.xml without a schema) the schema is parsed by libxml2 and by
   the native port (which loads the schema documents with the native parser) and the
   instance is parsed by both parsers (no entity substitution, like runtest) and validated.
   Compared are:
   - the result codes of xmlSchemaParse (NULL or not) and xmlSchemaValidateDoc,
   - every error of the structured callbacks (domain, code, level, file, line, str1-3,
     int1, int2, the node: kind, name and line, the message and the xmlFormatError text),
   - every message of the plain callbacks (byte identical),
   - and libxml2's output must reproduce result/schemas/<prefix>_<N>.err (so the pairing
     of this harness is the one of runtest).
   Then every instance is mutated (attribute values changed/removed/added, elements
   removed/duplicated/renamed, text changed, entity references added): the mutated document is serialized by libxml2
   and parsed by both parsers, and validated again by both.

   Usage: xml_xsd10_valid_diff [-v] [-m mutations:seed] libxml2_dir
   (the tool changes into libxml2_dir, the file names are the ./test/schemas/ ones of runtest).
   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include<libxml/parser.h>
#include<libxml/tree.h>
#include<libxml/entities.h>
#include<libxml/xmlerror.h>
#include<libxml/xmlschemas.h>
#include<libxml/xmlschemastypes.h>

#define XML_XSD10_INTERNALS
#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xsd10_types.h"
#include"parts/xml_xsd10_schemas.h"

#include<glob.h>
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
static long same_ = 0, diff_ = 0, runs_ = 0, mutated_runs_ = 0, result_ok_ = 0, result_bad_ = 0;
static int max_shown_ = 60;
static unsigned long long rng_ = 1;
static int mutations_ = 30;
static long verrors_ = 0;
static unsigned char codes_[4096];

static unsigned rnd(void)
{
    rng_ = rng_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_ >> 33);
}

// A growing text buffer.
typedef struct { char *s; size_t len, max; } Buf;

static void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->max)
    {
        b->max = (b->len + n + 1) * 2;
        b->s = (char *) realloc(b->s, b->max);
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}
static void buf_str(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }
static void buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t) n < sizeof(tmp)) { buf_add(b, tmp, n); return; }
    char *big = (char *) malloc(n + 1);
    va_start(ap, fmt);
    vsnprintf(big, n + 1, fmt, ap);
    va_end(ap);
    buf_add(b, big, n);
    free(big);
}
static void buf_clear(Buf *b) { b->len = 0; if (b->s) b->s[0] = 0; }
static void buf_free(Buf *b) { free(b->s); b->s = NULL; b->len = b->max = 0; }
static const char *buf_get(Buf *b) { return b->s ? b->s : ""; }

// The errors of one run, one string per error (all fields) and the formatted text.
typedef struct
{
    Buf fields;  // one line per error with all fields
    Buf text;    // the xmlFormatError texts (runtest style)
    Buf plain;   // the plain callback messages
    int count;
} Capture;

static Capture lcap_, ncap_;

static void cap_clear(Capture *c)
{
    buf_clear(&c->fields); buf_clear(&c->text); buf_clear(&c->plain);
    c->count = 0;
}

static const char *s_(const char *s) { return s ? s : "(NULL)"; }

static void l_text_channel(void *ctx, const char *msg, ...)
{
    Capture *c = (Capture *) ctx;
    va_list ap;
    char tmp[65536];
    va_start(ap, msg);
    vsnprintf(tmp, sizeof(tmp), msg, ap);
    va_end(ap);
    buf_str(&c->text, tmp);
}

static void l_structured(void *ctx, const xmlError *err)
{
    Capture *c = (Capture *) ctx;
    const xmlNode *node = (const xmlNode *) err->node;
    buf_printf(&c->fields, "domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%d:%s:%d msg=%s",
               err->domain, err->code, (int) err->level, s_(err->file), err->line,
               s_(err->str1), s_(err->str2), s_(err->str3), err->int1, err->int2,
               node ? (int) node->type : -1,
               (node && (node->type == XML_ELEMENT_NODE || node->type == XML_ATTRIBUTE_NODE)) ? s_((const char *) node->name) : "-",
               (node && node->type == XML_ELEMENT_NODE) ? (int) xmlGetLineNo(node) : -1,
               s_(err->message));
    buf_str(&c->fields, "\n");
    if (err->code >= 0 && err->code < 4096) codes_[err->code] = 1;
    xmlFormatError(err, l_text_channel, c);
    c->count++;
}

static void l_plain(void *ctx, const char *msg, ...)
{
    Capture *c = (Capture *) ctx;
    va_list ap;
    char tmp[65536];
    va_start(ap, msg);
    vsnprintf(tmp, sizeof(tmp), msg, ap);
    va_end(ap);
    buf_str(&c->plain, tmp);
    c->count++;
}

static void n_structured(void *ctx, const XmlXsd10Error *err)
{
    Capture *c = (Capture *) ctx;
    XMQNode *node = err->node;
    int kind = node ? (int) xml_node_kind(node) : -1;
    buf_printf(&c->fields, "domain=%d code=%d level=%d file=%s line=%d str1=%s str2=%s str3=%s int1=%d int2=%d node=%d:%s:%d msg=%s",
               err->domain, err->code, (int) err->level, s_(err->file), err->line,
               s_(err->str1), s_(err->str2), s_(err->str3), err->int1, err->int2,
               kind,
               (kind == XML_DOM_ELEMENT || kind == XML_DOM_ATTRIBUTE) ? s_(xml_node_name(node)) : "-",
               kind == XML_DOM_ELEMENT ? (int) xml_xsd10_tree_get_line_no(node) : -1,
               s_(err->message));
    buf_str(&c->fields, "\n");
    char *t = xml_xsd10_format_error_string(err);
    if (t) { buf_str(&c->text, t); free(t); }
    c->count++;
}

static void n_plain(void *ctx, const char *msg, ...)
{
    Capture *c = (Capture *) ctx;
    va_list ap;
    char tmp[65536];
    va_start(ap, msg);
    vsnprintf(tmp, sizeof(tmp), msg, ap);
    va_end(ap);
    buf_str(&c->plain, tmp);
    c->count++;
}

static void l_quiet(void *ctx, const char *msg, ...) { (void) ctx; (void) msg; }

static void report(const char *what, const char *where, const char *l, const char *n)
{
    if (!strcmp(l, n))
    {
        same_++;
        return;
    }
    diff_++;
    if (diff_ <= max_shown_)
    {
        printf("DIFF %s %s\n--- libxml2:\n%s--- native:\n%s---\n", what, where, l, n);
    }
}

static void report_int(const char *what, const char *where, long l, long n)
{
    char a[64], b[64];
    snprintf(a, sizeof(a), "%ld\n", l);
    snprintf(b, sizeof(b), "%ld\n", n);
    report(what, where, a, b);
}

static char *read_file(const char *file, size_t *len)
{
    FILE *f = fopen(file, "rb");
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

static XmlDom *native_parse(const char *buf, size_t len, const char *name)
{
    XmlParseSettings ps;
    xml_parse_settings_default(&ps);
    ps.source_name = name;
    XmlParseError err;
    memset(&err, 0, sizeof(err));
    XmlDom *doc = xml_parse_buffer(buf, buf + len, &ps, &err);
    xml_parse_error_free(&err);
    if (doc) xml_dom_set_url(doc, name);
    return doc;
}

// Schemas parsed by both, with their parse errors.
typedef struct
{
    xmlSchemaPtr lschema;
    XmlXsd10Schema *nschema;
    char *lerrors; // the runtest text of the parse errors
} Schemas;

static void parse_schemas(const char *file, Schemas *s)
{
    cap_clear(&lcap_);
    cap_clear(&ncap_);
    xmlSchemaParserCtxtPtr lp = xmlSchemaNewParserCtxt(file);
    xmlSchemaSetParserStructuredErrors(lp, l_structured, &lcap_);
    s->lschema = xmlSchemaParse(lp);
    xmlSchemaFreeParserCtxt(lp);

    XmlXsd10ParserCtxt *np = xml_xsd10_new_parser_ctxt(file);
    xml_xsd10_set_parser_structured_errors(np, n_structured, &ncap_);
    s->nschema = xml_xsd10_parse(np);
    xml_xsd10_free_parser_ctxt(np);

    report_int("schema parse result", file, s->lschema != NULL, s->nschema != NULL);
    report("schema parse errors", file, buf_get(&lcap_.fields), buf_get(&ncap_.fields));
    report("schema parse error text", file, buf_get(&lcap_.text), buf_get(&ncap_.text));
    s->lerrors = strdup(buf_get(&lcap_.text));
}

static void free_schemas(Schemas *s)
{
    if (s->lschema) xmlSchemaFree(s->lschema);
    if (s->nschema) xml_xsd10_free(s->nschema);
    free(s->lerrors);
}

// Dumps of the trees, to compare the default attributes and values created by XML_SCHEMA_VAL_VC_I_CREATE.
static void l_dump(Buf *b, xmlNodePtr n)
{
    for (; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            buf_printf(b, "<{%s}%s", n->ns ? s_((const char *) n->ns->href) : "", (const char *) n->name);
            for (xmlNsPtr ns = n->nsDef; ns; ns = ns->next)
                buf_printf(b, " xmlns:%s=%s", s_((const char *) ns->prefix), s_((const char *) ns->href));
            for (xmlAttrPtr a = n->properties; a; a = a->next)
            {
                xmlChar *v = xmlNodeGetContent((xmlNodePtr) a);
                buf_printf(b, " {%s}%s=\"%s\"", a->ns ? s_((const char *) a->ns->href) : "", (const char *) a->name, s_((const char *) v));
                xmlFree(v);
            }
            buf_str(b, ">");
            l_dump(b, n->children);
            buf_str(b, "</>");
        }
        else if (n->type == XML_TEXT_NODE || n->type == XML_CDATA_SECTION_NODE)
            buf_printf(b, "%s", s_((const char *) n->content));
    }
}

static void n_dump(Buf *b, XMQNode *n)
{
    for (; n; n = xml_node_next(n))
    {
        XmlNodeKind k = xml_node_kind(n);
        if (k == XML_DOM_ELEMENT)
        {
            buf_printf(b, "<{%s}%s", xml_node_ns(n) ? s_(xml_ns_href(xml_node_ns(n))) : "", xml_node_name(n));
            for (XmlNs *ns = xml_node_first_ns_decl(n); ns; ns = xml_ns_next_decl(ns))
                buf_printf(b, " xmlns:%s=%s", s_(xml_ns_prefix(ns)), s_(xml_ns_href(ns)));
            for (XMQAttr *a = xml_node_first_attr(n); a; a = xml_attr_next(a))
            {
                XMQNode *an = xml_attr_node(a);
                char *v = xml_xsd10_tree_node_get_content(an);
                buf_printf(b, " {%s}%s=\"%s\"", xml_node_ns(an) ? s_(xml_ns_href(xml_node_ns(an))) : "", xml_attr_name(a), s_(v));
                free(v);
            }
            buf_str(b, ">");
            n_dump(b, xml_node_first_child(n));
            buf_str(b, "</>");
        }
        else if (k == XML_DOM_TEXT || k == XML_DOM_CDATA)
            buf_printf(b, "%s", s_(xml_node_content(n)));
    }
}

/* Validate the document text with both, structured and plain callbacks.
   Returns the libxml2 runtest style output (malloced) of the structured run. */
static char *validate_both(Schemas *s, const char *instance, const char *xml, size_t len, const char *what)
{
    char where[1024];
    snprintf(where, sizeof(where), "%s %s", what, instance);
    Buf out = {0};
    /* Rounds: 0 structured callbacks, 1 plain callbacks, 2 plain callbacks and the option
       XML_SCHEMA_VAL_VC_I_CREATE (the trees are compared afterwards), 3 structured callbacks
       and xmlSchemaValidateOneElement on the first child element of the root. */
    for (int round = 0; round < 4; round++)
    {
        cap_clear(&lcap_);
        cap_clear(&ncap_);
        xmlDocPtr ldoc = xmlReadMemory(xml, (int) len, instance, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        XmlDom *ndoc = native_parse(xml, len, instance);
        if (!ldoc || !ndoc)
        {
            if (round == 0) report_int("instance parse", where, ldoc != NULL, ndoc != NULL);
            if (ldoc) xmlFreeDoc(ldoc);
            if (ndoc) xml_dom_free(ndoc);
            buf_free(&out);
            return NULL;
        }
        xmlSchemaValidCtxtPtr lv = xmlSchemaNewValidCtxt(s->lschema);
        XmlXsd10ValidCtxt *nv = xml_xsd10_new_valid_ctxt(s->nschema);
        if (round == 0)
        {
            xmlSchemaSetValidStructuredErrors(lv, l_structured, &lcap_);
            xml_xsd10_set_valid_structured_errors(nv, n_structured, &ncap_);
        }
        else if (round == 3)
        {
            xmlSchemaSetValidStructuredErrors(lv, l_structured, &lcap_);
            xml_xsd10_set_valid_structured_errors(nv, n_structured, &ncap_);
        }
        else
        {
            xmlSchemaSetValidErrors(lv, l_plain, l_plain, &lcap_);
            xml_xsd10_set_valid_errors(nv, n_plain, n_plain, &ncap_);
        }
        if (round == 2)
        {
            report_int("set options", where, xmlSchemaSetValidOptions(lv, XML_SCHEMA_VAL_VC_I_CREATE),
                       xml_xsd10_set_valid_options(nv, XML_XSD10_SCHEMA_VAL_VC_I_CREATE));
        }
        int lr, nr;
        if (round == 3)
        {
            xmlNodePtr le = xmlDocGetRootElement(ldoc);
            XMQNode *ne = xml_dom_root_element(ndoc);
            le = le ? xmlFirstElementChild(le) : NULL;
            for (ne = ne ? xml_node_first_child(ne) : NULL; ne && xml_node_kind(ne) != XML_DOM_ELEMENT; ne = xml_node_next(ne));
            if (!le || !ne)
            {
                report_int("one element", where, le != NULL, ne != NULL);
                xmlSchemaFreeValidCtxt(lv);
                xml_xsd10_free_valid_ctxt(nv);
                xmlFreeDoc(ldoc);
                xml_dom_free(ndoc);
                continue;
            }
            lr = xmlSchemaValidateOneElement(lv, le);
            nr = xml_xsd10_validate_one_element(nv, ne);
        }
        else
        {
            lr = xmlSchemaValidateDoc(lv, ldoc);
            nr = xml_xsd10_validate_doc(nv, ndoc);
        }
        report_int(round ? "validate result (other rounds)" : "validate result", where, lr, nr);
        if (round == 2)
        {
            Buf lt = {0}, nt = {0};
            l_dump(&lt, xmlDocGetRootElement(ldoc));
            n_dump(&nt, xml_dom_root_element(ndoc));
            buf_str(&lt, "\n");
            buf_str(&nt, "\n");
            report("tree after VC_I_CREATE", where, buf_get(&lt), buf_get(&nt));
            buf_free(&lt);
            buf_free(&nt);
        }
        if (round == 3)
        {
            report("validate one element errors", where, buf_get(&lcap_.fields), buf_get(&ncap_.fields));
        }
        else if (round == 0)
        {
            verrors_ += lcap_.count;
            report("validate errors", where, buf_get(&lcap_.fields), buf_get(&ncap_.fields));
            report("validate error text", where, buf_get(&lcap_.text), buf_get(&ncap_.text));
            buf_str(&out, s->lerrors);
            buf_str(&out, buf_get(&lcap_.text));
            if (lr == 0) buf_printf(&out, "%s validates\n", instance);
            else if (lr > 0) buf_printf(&out, "%s fails to validate\n", instance);
            else buf_printf(&out, "%s validation generated an internal error\n", instance);
        }
        else
        {
            report("validate plain messages", where, buf_get(&lcap_.plain), buf_get(&ncap_.plain));
        }
        report_int(round ? "is valid (plain)" : "is valid", where, xmlSchemaIsValid(lv), xml_xsd10_is_valid(nv));
        xmlSchemaFreeValidCtxt(lv);
        xml_xsd10_free_valid_ctxt(nv);
        xmlFreeDoc(ldoc);
        xml_dom_free(ndoc);
    }
    runs_++;
    char *r = strdup(buf_get(&out));
    buf_free(&out);
    return r;
}

static void check_result_file(const char *err_file, const char *got, const char *instance)
{
    size_t len;
    char *expected = read_file(err_file, &len);
    if (!expected)
    {
        // runtest: a missing result file compares like an empty one.
        expected = strdup("");
    }
    if (!strcmp(expected, got)) result_ok_++;
    else
    {
        result_bad_++;
        if (verbose_ || result_bad_ <= 10)
            printf("RESULT MISMATCH (libxml2 vs %s) for %s\n--- expected:\n%s--- libxml2:\n%s---\n",
                   err_file, instance, expected, got);
    }
    free(expected);
}

// Mutations ////////////////////////////////////////////////////////

static xmlNodePtr *elems_ = NULL;
static size_t elems_len_ = 0, elems_max_ = 0;

static void collect_elems(xmlNodePtr n)
{
    for (; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE) continue;
        if (elems_len_ == elems_max_)
        {
            elems_max_ = elems_max_ ? elems_max_ * 2 : 64;
            elems_ = (xmlNodePtr *) realloc(elems_, elems_max_ * sizeof(xmlNodePtr));
        }
        elems_[elems_len_++] = n;
        collect_elems(n->children);
    }
}

static const char *rnd_value(xmlNodePtr any)
{
    static char tmp[64];
    static const char *const vals[] = {
        "", " ", "0", "1", "-1", "2", "10", "100", "999999999999999999999", "1.5", "-0.0", "abc",
        "true", "false", "INF", "NaN", "a b c", "  x  ", "2004-04-04", "2004-13-01", "12:00:00",
        "P1Y", "xs:string", "foo:bar", "ab12", "#x", "%s", "\t\n", "x y", "1 2 3", "id1", "id2"
    };
    if (any && rnd() % 4 == 0)
    {
        // A value seen elsewhere in the document.
        xmlNodePtr e = elems_[rnd() % elems_len_];
        if (e->properties && e->properties->children && e->properties->children->content)
        {
            snprintf(tmp, sizeof(tmp), "%s", (const char *) e->properties->children->content);
            return tmp;
        }
        if (e->children && e->children->type == XML_TEXT_NODE && e->children->content)
        {
            snprintf(tmp, sizeof(tmp), "%s", (const char *) e->children->content);
            return tmp;
        }
    }
    return vals[rnd() % (sizeof(vals) / sizeof(vals[0]))];
}

static void mutate(xmlDocPtr doc)
{
    elems_len_ = 0;
    collect_elems(xmlDocGetRootElement(doc));
    if (elems_len_ == 0) return;
    int n = 1 + rnd() % 2;
    for (int k = 0; k < n; k++)
    {
        xmlNodePtr e = elems_[rnd() % elems_len_];
        if (!e) continue;
        switch (rnd() % 10)
        {
        case 0: // change an attribute value
        {
            int na = 0;
            for (xmlAttrPtr a = e->properties; a; a = a->next) na++;
            if (na == 0) { xmlSetProp(e, BAD_CAST "a", BAD_CAST rnd_value(e)); break; }
            xmlAttrPtr a = e->properties;
            for (int i = rnd() % na; i > 0; i--) a = a->next;
            xmlSetNsProp(e, a->ns, a->name, BAD_CAST rnd_value(e));
            break;
        }
        case 1: // remove an attribute
            if (e->properties) xmlRemoveProp(e->properties);
            break;
        case 2: // remove the element (not the root)
            if (e->parent && e->parent->type == XML_ELEMENT_NODE)
            {
                xmlUnlinkNode(e);
                xmlFreeNode(e);
                elems_len_ = 0;
                collect_elems(xmlDocGetRootElement(doc));
                if (elems_len_ == 0) return;
            }
            break;
        case 3: // duplicate the element
            if (e->parent && e->parent->type == XML_ELEMENT_NODE)
            {
                xmlNodePtr c = xmlCopyNode(e, 1);
                if (c) xmlAddNextSibling(e, c);
            }
            break;
        case 4: // rename the element (to the name of another element or a new one)
        {
            xmlNodePtr o = elems_[rnd() % elems_len_];
            if (rnd() % 3 == 0) xmlNodeSetName(e, BAD_CAST "renamed");
            else
            {
                xmlNodeSetName(e, o->name);
                if (rnd() % 2) xmlSetNs(e, o->ns);
            }
            break;
        }
        case 5: // change the text
            if (e->children == NULL || (e->children->type == XML_TEXT_NODE && e->children->next == NULL))
                xmlNodeSetContent(e, BAD_CAST rnd_value(e));
            else
                xmlAddChild(e, xmlNewDocText(doc, BAD_CAST rnd_value(e)));
            break;
        case 6: // remove the text / all children
        {
            xmlNodePtr c = e->children;
            while (c)
            {
                xmlNodePtr nx = c->next;
                if (c->type == XML_TEXT_NODE || rnd() % 3 == 0)
                {
                    xmlUnlinkNode(c);
                    xmlFreeNode(c);
                }
                c = nx;
            }
            elems_len_ = 0;
            collect_elems(xmlDocGetRootElement(doc));
            if (elems_len_ == 0) return;
            break;
        }
        case 7: // add an xsi attribute or an unknown attribute
        {
            xmlNsPtr ns = xmlSearchNsByHref(doc, e, BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
            if (!ns) ns = xmlNewNs(xmlDocGetRootElement(doc), BAD_CAST "http://www.w3.org/2001/XMLSchema-instance", BAD_CAST "xsi");
            switch (rnd() % 4)
            {
            case 0: xmlSetNsProp(e, ns, BAD_CAST "nil", BAD_CAST (rnd() % 2 ? "true" : "false")); break;
            case 1: xmlSetNsProp(e, ns, BAD_CAST "type", BAD_CAST (rnd() % 2 ? "xs:string" : "unknownType")); break;
            case 2: xmlSetProp(e, BAD_CAST "unknown", BAD_CAST "1"); break;
            default: xmlSetNsProp(e, ns, BAD_CAST "bogus", BAD_CAST "1"); break;
            }
            break;
        }
        case 8: // add a reference to an internal entity (not substituted when parsed)
            if (doc->intSubset == NULL)
                xmlCreateIntSubset(doc, xmlDocGetRootElement(doc)->name, NULL, NULL);
            if (xmlGetDocEntity(doc, BAD_CAST "ent") == NULL)
                xmlAddDocEntity(doc, BAD_CAST "ent", XML_INTERNAL_GENERAL_ENTITY, NULL, NULL, BAD_CAST "text");
            xmlAddChild(e, xmlNewReference(doc, BAD_CAST "&ent;"));
            break;
        default: // add a child element copied from elsewhere
        {
            xmlNodePtr o = elems_[rnd() % elems_len_];
            if (o == e) break;
            xmlNodePtr c = xmlCopyNode(o, rnd() % 2);
            if (c) xmlAddChild(e, c);
            break;
        }
        }
    }
}

static void run_instance(Schemas *s, const char *instance, const char *err_file)
{
    size_t len;
    char *xml = read_file(instance, &len);
    if (!xml)
    {
        printf("cannot read %s\n", instance);
        return;
    }
    char *out = validate_both(s, instance, xml, len, "instance");
    if (out && err_file) check_result_file(err_file, out, instance);
    free(out);

    // Mutations.
    xmlDocPtr base = xmlReadMemory(xml, (int) len, instance, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (base)
    {
        for (int m = 0; m < mutations_; m++)
        {
            xmlDocPtr d = xmlCopyDoc(base, 1);
            mutate(d);
            xmlChar *mem = NULL;
            int mlen = 0;
            xmlDocDumpMemory(d, &mem, &mlen);
            xmlFreeDoc(d);
            if (!mem) continue;
            char what[64];
            snprintf(what, sizeof(what), "mutation %d", m);
            char *r = validate_both(s, instance, (const char *) mem, (size_t) mlen, what);
            if (r) mutated_runs_++;
            if (r && verbose_ > 1) printf("%s %s:\n%s\n%s", what, instance, (const char *) mem, r);
            free(r);
            xmlFree(mem);
        }
        xmlFreeDoc(base);
    }
    free(xml);
}

static const char *base_filename(const char *f)
{
    const char *b = strrchr(f, '/');
    return b ? b + 1 : f;
}

// Port of the pairing of runtest.c schemasTest.
static void schemas_test(const char *filename)
{
    const char *base = base_filename(filename);
    char pattern[600], prefix[500], err[600];
    int len = (int) strlen(base);
    if (len > 499 || len < 5) return;
    Schemas s;
    memset(&s, 0, sizeof(s));
    if (verbose_) printf("schema %s\n", filename);
    parse_schemas(filename, &s);

    len -= 4;
    if (base[len - 2] == '_') len -= 2;
    if (base[len - 2] == '_') len -= 2;
    memcpy(prefix, base, len);
    prefix[len] = 0;
    snprintf(pattern, sizeof(pattern), "./test/schemas/%s_*.xml", prefix);
    if (base[len] == '_')
    {
        len += 2;
        memcpy(prefix, base, len);
        prefix[len] = 0;
    }
    glob_t g;
    memset(&g, 0, sizeof(g));
    glob(pattern, GLOB_DOOFFS, NULL, &g);
    for (size_t i = 0; i < g.gl_pathc; i++)
    {
        const char *instance = g.gl_pathv[i];
        const char *base2 = base_filename(instance);
        int len2 = (int) strlen(base2);
        if (!(len2 > 6 && base2[len2 - 6] == '_'))
        {
            printf("don't know how to process %s\n", instance);
            continue;
        }
        snprintf(err, sizeof(err), "result/schemas/%s_%c.err", prefix, base2[len2 - 5]);
        if (s.lschema == NULL || s.nschema == NULL)
        {
            // Schema load failed: only the parse errors are expected.
            if (s.lschema == NULL) check_result_file(err, s.lerrors, instance);
            continue;
        }
        if (verbose_) printf("  instance %s\n", instance);
        run_instance(&s, instance, err);
    }
    globfree(&g);
    free_schemas(&s);
}

// Port of runtest.c schemasXsiTest: no schema, assembled from xsi:schemaLocation.
static void schemas_xsi_test(const char *filename)
{
    const char *base = base_filename(filename);
    char err[600], prefix[500];
    int len = (int) strlen(base);
    if (len > 499 || len < 5) return;
    memcpy(prefix, base, len - 4);
    prefix[len - 4] = 0;
    snprintf(err, sizeof(err), "result/schemas/%s_0.err", prefix);
    Schemas s;
    memset(&s, 0, sizeof(s));
    s.lerrors = strdup("");
    run_instance(&s, filename, err);
    free(s.lerrors);
}

int main(int argc, char **argv)
{
    int i = 1;
    for (; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-m") && i + 1 < argc)
        {
            unsigned long long seed = 1;
            sscanf(argv[++i], "%d:%llu", &mutations_, &seed);
            rng_ = seed;
        }
        else break;
    }
    if (i >= argc || chdir(argv[i]) != 0)
    {
        printf("Usage: xml_xsd10_valid_diff [-v] [-m mutations:seed] libxml2_dir\n");
        return 1;
    }
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, l_quiet);

    glob_t g;
    memset(&g, 0, sizeof(g));
    glob("./test/schemas/*_*.xsd", 0, NULL, &g);
    for (size_t k = 0; k < g.gl_pathc; k++) schemas_test(g.gl_pathv[k]);
    globfree(&g);
    schemas_xsi_test("./test/schemas/any1_0.xml");

    buf_free(&lcap_.fields); buf_free(&lcap_.text); buf_free(&lcap_.plain);
    buf_free(&ncap_.fields); buf_free(&ncap_.text); buf_free(&ncap_.plain);
    free(elems_);
    xmlCleanupParser();
    int ncodes = 0;
    for (int c = 0; c < 4096; c++) ncodes += codes_[c];
    printf("xml_xsd10_valid_diff: %ld validations (%ld of mutated instances) with %ld validation errors "
           "(%d different error codes, parsing included), %ld same, %ld different, "
           "libxml2 reproduces %ld result files, %ld mismatches\n",
           runs_, mutated_runs_, verrors_, ncodes, same_, diff_, result_ok_, result_bad_);
    fflush(stdout);
    return (diff_ || result_bad_) ? 1 : 0;
}
