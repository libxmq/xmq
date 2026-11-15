/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native stylesheet compilation (parts/xml_xslt.c) against libxslt.

   Every stylesheet given on the command line is loaded and compiled with libxslt
   (xmlReadFile with XSLT_PARSE_OPTIONS, xsltParseStylesheetDoc) and with the native
   code (xml_xslt_default_loader, xml_xslt_compile). The compiled stylesheets are
   printed in the format of xml_xslt_stylesheet_describe (the libxslt one from its
   structures) and must be identical, as must the messages printed while compiling.
   With -m count:seed every stylesheet is also compiled after random mutations
   (attribute values, added and removed attributes, renamed, removed and moved
   elements) applied identically to both trees, to exercise the error handling.

   This tool links libxml2 and libxslt and is only used while libxmq moves away from them.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xpath.h"
#include"parts/xml_xslt.h"
#include"parts/membuffer.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/hash.h>
#include<libxml/xmlerror.h>
#include<libxslt/xsltInternals.h>
#include<libxslt/xsltutils.h>
#include<libxslt/xslt.h>
#include<libxslt/extensions.h>
#include<libxslt/preproc.h>

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

// Private structures of libxslt (pattern.c, attributes.c, attrvt.c, extensions.c).
typedef struct
{
    int op;
    xmlChar *value;
    xmlChar *value2;
    xmlChar *value3;
    void *comp;
    int previousExtra;
    int indexExtra;
    int lenExtra;
} LxStepOp;

typedef struct LxCompMatch
{
    struct LxCompMatch *next;
    float priority;
    const xmlChar *pattern;
    const xmlChar *mode;
    const xmlChar *modeURI;
    xsltTemplatePtr templ;
    xmlNodePtr node;
    int direct;
    int nbStep;
    int maxStep;
    xmlNsPtr *nsList;
    int nsNr;
    LxStepOp *steps;
} LxCompMatch;

typedef struct LxAttrElem
{
    struct LxAttrElem *next;
    xmlNodePtr attr;
} LxAttrElem;

typedef struct LxUseAttrSet
{
    struct LxUseAttrSet *next;
    const xmlChar *ncname;
    const xmlChar *ns;
} LxUseAttrSet;

typedef struct
{
    int state;
    LxAttrElem *attrs;
    LxUseAttrSet *useAttrSets;
} LxAttrSet;

typedef struct
{
    void *next;
    int nb_seg;
    int max_seg;
    int strstart;
    xmlNsPtr *nsList;
    int nsNr;
    void *segments[];
} LxAttrVT;

typedef struct LxExtDef
{
    struct LxExtDef *next;
    xmlChar *prefix;
    xmlChar *URI;
    void *data;
} LxExtDef;

static int verbose_ = 0;
static long files_ = 0, skipped_ = 0, same_ = 0, diff_ = 0, both_fail_ = 0, msg_diff_ = 0, with_msgs_ = 0;
static int max_shown_ = 10;
static int shown_ = 0;

static MemBuffer *lx_msgs_;
static MemBuffer *nx_msgs_;

static void capture_lx(void *ctx, const char *msg, ...)
{
    (void)ctx;
    char b[8192];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(b, sizeof(b), msg, ap);
    va_end(ap);
    membuffer_append(lx_msgs_, b);
}

static void capture_structured(void *ud, const xmlError *err)
{
    (void)ud;
    xmlFormatError(err, capture_lx, NULL);
}

static void capture_native(void *ud, const char *msg)
{
    (void)ud;
    membuffer_append(nx_msgs_, msg);
}

static char *take(MemBuffer *mb)
{
    membuffer_append_null(mb);
    return free_membuffer_but_return_trimmed_content(mb);
}

// The same printing as xml_xslt_stylesheet_describe.

static void d_str(MemBuffer *mb, const char *label, const xmlChar *xs)
{
    const char *s = (const char*)xs;
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

static void d_indent(MemBuffer *mb, int depth)
{
    for (int i = 0; i < depth; ++i) membuffer_append(mb, "  ");
}

static const char *type_name(int type)
{
    static const char *names[] = { "?", "copy", "sort", "text", "element", "attribute", "comment", "pi", "copy-of",
        "value-of", "number", "apply-imports", "call-template", "apply-templates", "choose", "if", "for-each",
        "document", "with-param", "param", "variable", "when", "extension" };
    if (type < 0 || type > XSLT_FUNC_EXTENSION) return "?";
    return names[type];
}

static int d_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

typedef struct
{
    char **lines;
    size_t n, max;
} Lines;

static void lines_add(Lines *l, char *s)
{
    if (l->n >= l->max)
    {
        l->max = l->max ? 2 * l->max : 16;
        l->lines = realloc(l->lines, l->max * sizeof(char*));
    }
    l->lines[l->n++] = s;
}

static void lines_print(MemBuffer *mb, Lines *l)
{
    if (l->n) qsort(l->lines, l->n, sizeof(char*), d_cmp);
    for (size_t i = 0; i < l->n; ++i)
    {
        membuffer_append(mb, l->lines[i]);
        free(l->lines[i]);
    }
    free(l->lines);
}

/** The hash key printed like the native one: name|name2|name3 with (null) for missing parts. */
static char *key3(const xmlChar *a, const xmlChar *b, const xmlChar *c, int parts)
{
    MemBuffer *mb = new_membuffer();
    const xmlChar *p[3] = { a, b, c };
    for (int i = 0; i < parts; ++i)
    {
        if (i) membuffer_append_char(mb, '|');
        membuffer_append(mb, p[i] ? (const char*)p[i] : "(null)");
    }
    return take(mb);
}

static void d_precomp(MemBuffer *mb, void *psvi)
{
    if (psvi == (void*)xsltExtMarker)
    {
        membuffer_append(mb, " precomp=ext-marker");
        return;
    }
    xsltStylePreCompPtr c = (xsltStylePreCompPtr)psvi;
    membuffer_printf(mb, " precomp=%s", type_name(c->type));
    if (c->type == XSLT_FUNC_EXTENSION || c->type == XSLT_FUNC_DOCUMENT)
    {
        if (c->type == XSLT_FUNC_DOCUMENT)
        {
            membuffer_printf(mb, " ver11=%d", c->ver11);
            d_str(mb, "filename", c->filename);
        }
        return;
    }
    if (c->stype) d_str(mb, "stype", c->stype);
    if (c->has_stype) membuffer_printf(mb, " has_stype=%d", c->has_stype);
    if (c->number) membuffer_printf(mb, " number=%d", c->number);
    if (c->order) d_str(mb, "order", c->order);
    if (c->has_order) membuffer_printf(mb, " has_order=%d", c->has_order);
    if (c->descending) membuffer_printf(mb, " descending=%d", c->descending);
    if (c->lang) d_str(mb, "lang", c->lang);
    if (c->has_lang) membuffer_printf(mb, " has_lang=%d", c->has_lang);
    if (c->case_order) d_str(mb, "case_order", c->case_order);
    if (c->lower_first) membuffer_printf(mb, " lower_first=%d", c->lower_first);
    if (c->use) d_str(mb, "use", c->use);
    if (c->has_use) membuffer_printf(mb, " has_use=%d", c->has_use);
    if (c->noescape) membuffer_printf(mb, " noescape=%d", c->noescape);
    if (c->name) d_str(mb, "name", c->name);
    if (c->has_name) membuffer_printf(mb, " has_name=%d", c->has_name);
    if (c->ns) d_str(mb, "ns", c->ns);
    if (c->has_ns) membuffer_printf(mb, " has_ns=%d", c->has_ns);
    if (c->mode) d_str(mb, "mode", c->mode);
    if (c->modeURI) d_str(mb, "mode_uri", c->modeURI);
    if (c->test) d_str(mb, "test", c->test);
    if (c->select) d_str(mb, "select", c->select);
    if (c->comp) membuffer_append(mb, " comp");
    if (c->nsNr) membuffer_printf(mb, " ns_nr=%d", c->nsNr);
    if (c->type == XSLT_FUNC_NUMBER)
    {
        xsltNumberData *n = &c->numdata;
        d_str(mb, "level", n->level);
        d_str(mb, "count", n->count);
        d_str(mb, "from", n->from);
        d_str(mb, "value", n->value);
        d_str(mb, "format", n->format);
        membuffer_printf(mb, " has_format=%d digits_per_group=%d grouping_character=%d count_pat=%d from_pat=%d",
                         n->has_format, n->digitsPerGroup, n->groupingCharacter, n->countPat != NULL, n->fromPat != NULL);
    }
}

static void d_avt(MemBuffer *mb, LxAttrVT *avt)
{
    membuffer_printf(mb, " avt=[strstart=%d", avt->strstart);
    int str = avt->strstart;
    for (int i = 0; i < avt->nb_seg; ++i)
    {
        if (str && avt->segments[i]) d_str(mb, "s", (const xmlChar*)avt->segments[i]);
        else if (str) membuffer_append(mb, " s=(null)");
        else membuffer_append(mb, avt->segments[i] ? " x" : " x=(null)");
        str = !str;
    }
    membuffer_append(mb, "]");
}

static void d_tree(MemBuffer *mb, xmlNodePtr node, int depth)
{
    for (; node; node = node->next)
    {
        if (node->type == XML_DTD_NODE) continue;
        d_indent(mb, depth);
        switch (node->type)
        {
        case XML_ELEMENT_NODE:
            membuffer_append(mb, "element");
            d_str(mb, "name", node->name);
            if (node->ns) d_str(mb, "ns", node->ns->href);
            if (node->psvi) d_precomp(mb, node->psvi);
            for (xmlNsPtr ns = node->nsDef; ns; ns = ns->next)
            {
                membuffer_append(mb, " xmlns");
                d_str(mb, "p", ns->prefix);
                d_str(mb, "h", ns->href);
            }
            membuffer_append_char(mb, '\n');
            for (xmlAttrPtr a = node->properties; a; a = a->next)
            {
                d_indent(mb, depth + 1);
                membuffer_append(mb, "attr");
                d_str(mb, "name", a->name);
                if (a->ns) d_str(mb, "ns", a->ns->href);
                if (a->psvi) d_avt(mb, (LxAttrVT*)a->psvi);
                membuffer_append_char(mb, '\n');
            }
            d_tree(mb, node->children, depth + 1);
            continue;
        case XML_TEXT_NODE:
            membuffer_append(mb, "text");
            if (node->name && !strcmp((const char*)node->name, "textnoenc")) membuffer_append(mb, " noenc");
            d_str(mb, "content", node->content);
            break;
        case XML_CDATA_SECTION_NODE:
            membuffer_append(mb, "cdata");
            d_str(mb, "content", node->content);
            break;
        case XML_COMMENT_NODE:
            membuffer_append(mb, "comment");
            break;
        case XML_PI_NODE:
            membuffer_append(mb, "pi");
            break;
        default:
            membuffer_printf(mb, "node libxml2-type=%d", (int)node->type);
            break;
        }
        membuffer_append_char(mb, '\n');
    }
}

static void d_patterns(MemBuffer *mb, const char *label, LxCompMatch *p, int depth)
{
    if (!p) return;
    d_indent(mb, depth);
    membuffer_append(mb, label);
    for (; p; p = p->next)
    {
        membuffer_append(mb, " [");
        d_str(mb, "p", p->pattern);
        membuffer_printf(mb, " prio=%g", (double)p->priority);
        if (p->mode) d_str(mb, "mode", p->mode);
        if (p->modeURI) d_str(mb, "mode_uri", p->modeURI);
        if (p->templ)
        {
            int ix = 0;
            xsltTemplatePtr t = p->templ->style ? p->templ->style->templates : NULL;
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

typedef struct
{
    Lines *lines;
    int depth;
    const char *label;
    int parts;
} ScanData;

static void scan_value(void *payload, void *data, const xmlChar *name, const xmlChar *name2, const xmlChar *name3)
{
    ScanData *sd = (ScanData*)data;
    MemBuffer *l = new_membuffer();
    d_indent(l, sd->depth);
    membuffer_append(l, sd->label);
    char *k = key3(name, name2, name3, sd->parts);
    d_str(l, "key", (const xmlChar*)k);
    free(k);
    // libxslt UNDEFINED_DEFAULT_NS is (xmlChar*)-1.
    if (payload == (void*)-1L) payload = (void*)"#UNDEFINED_DEFAULT_NS";
    d_str(l, "value", (const xmlChar*)payload);
    membuffer_append_char(l, '\n');
    lines_add(sd->lines, take(l));
}

static void scan_attr_set(void *payload, void *data, const xmlChar *name, const xmlChar *ns, const xmlChar *unused)
{
    (void)unused;
    ScanData *sd = (ScanData*)data;
    LxAttrSet *set = (LxAttrSet*)payload;
    MemBuffer *l = new_membuffer();
    d_indent(l, sd->depth);
    membuffer_append(l, "attribute-set");
    d_str(l, "name", name);
    d_str(l, "ns", ns);
    membuffer_printf(l, " state=%d", set->state);
    for (LxAttrElem *a = set->attrs; a; a = a->next)
    {
        xsltStylePreCompPtr c = (xsltStylePreCompPtr)a->attr->psvi;
        membuffer_append(l, " [");
        d_str(l, "name", c ? c->name : NULL);
        d_str(l, "ns", c ? c->ns : NULL);
        membuffer_printf(l, " line=%ld ]", xmlGetLineNo(a->attr));
    }
    for (LxUseAttrSet *u = set->useAttrSets; u; u = u->next)
    {
        membuffer_append(l, " use");
        d_str(l, "ncname", u->ncname);
        d_str(l, "ns", u->ns);
    }
    membuffer_append_char(l, '\n');
    lines_add(sd->lines, take(l));
}

static void scan_templates(void *payload, void *data, const xmlChar *name, const xmlChar *name2, const xmlChar *name3)
{
    ScanData *sd = (ScanData*)data;
    if (!payload) return;
    MemBuffer *l = new_membuffer();
    MemBuffer *label = new_membuffer();
    membuffer_append(label, "hash");
    char *k = key3(name, name2, name3, 3);
    d_str(label, "key", (const xmlChar*)k);
    free(k);
    char *lab = take(label);
    d_patterns(l, lab, (LxCompMatch*)payload, sd->depth);
    free(lab);
    lines_add(sd->lines, take(l));
}

static void d_hash(MemBuffer *mb, xmlHashTablePtr h, xmlHashScannerFull f, const char *label, int parts, int depth)
{
    if (!h) return;
    Lines lines = { 0 };
    ScanData sd = { &lines, depth, label, parts };
    xmlHashScanFull(h, f, &sd);
    lines_print(mb, &lines);
}

static void d_style(MemBuffer *mb, xsltStylesheetPtr style, int depth)
{
    d_indent(mb, depth);
    membuffer_printf(mb, "stylesheet errors=%d warnings=%d literal_result=%d forwards_compatible=%d extras_nr=%d strip_all=%d\n",
                     style->errors, style->warnings, style->literal_result, style->forwards_compatible, style->extrasNr, style->stripAll);
    d_indent(mb, depth);
    membuffer_append(mb, "output");
    d_str(mb, "method", style->method);
    d_str(mb, "method_uri", style->methodURI);
    d_str(mb, "version", style->version);
    d_str(mb, "encoding", style->encoding);
    membuffer_printf(mb, " omit=%d standalone=%d indent=%d", style->omitXmlDeclaration, style->standalone, style->indent);
    d_str(mb, "doctype_public", style->doctypePublic);
    d_str(mb, "doctype_system", style->doctypeSystem);
    d_str(mb, "media_type", style->mediaType);
    d_str(mb, "default_alias", style->defaultAlias);
    membuffer_append_char(mb, '\n');
    d_hash(mb, style->stripSpaces, scan_value, "space", 3, depth);
    d_hash(mb, style->cdataSection, scan_value, "cdata", 3, depth);
    d_hash(mb, style->nsAliases, scan_value, "alias", 1, depth);
    for (int i = 0; i < style->exclPrefixNr; ++i)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "exclude");
        d_str(mb, "uri", style->exclPrefixTab[i]);
        membuffer_append_char(mb, '\n');
    }
    for (LxExtDef *d = (LxExtDef*)style->nsDefs; d; d = d->next)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "extension");
        d_str(mb, "prefix", d->prefix);
        d_str(mb, "uri", d->URI);
        membuffer_append_char(mb, '\n');
    }
    for (xsltDecimalFormatPtr f = style->decimalFormat; f; f = f->next)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "decimal-format");
        d_str(mb, "name", f->name);
        d_str(mb, "ns", f->nsUri);
        d_str(mb, "digit", f->digit);
        d_str(mb, "pattern_separator", f->patternSeparator);
        d_str(mb, "minus_sign", f->minusSign);
        d_str(mb, "infinity", f->infinity);
        d_str(mb, "nan", f->noNumber);
        d_str(mb, "decimal_point", f->decimalPoint);
        d_str(mb, "grouping", f->grouping);
        d_str(mb, "percent", f->percent);
        d_str(mb, "permille", f->permille);
        d_str(mb, "zero_digit", f->zeroDigit);
        membuffer_append_char(mb, '\n');
    }
    for (xsltKeyDefPtr k = (xsltKeyDefPtr)style->keys; k; k = k->next)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "key");
        d_str(mb, "name", k->name);
        d_str(mb, "name_uri", k->nameURI);
        d_str(mb, "match", k->match);
        d_str(mb, "use", k->use);
        membuffer_printf(mb, " comp=%d usecomp=%d\n", k->comp != NULL, k->usecomp != NULL);
    }
    for (xsltStackElemPtr v = style->variables; v; v = v->next)
    {
        d_indent(mb, depth);
        membuffer_printf(mb, "global %s", type_name(v->comp ? (int)v->comp->type : 0));
        d_str(mb, "name", v->name);
        d_str(mb, "name_uri", v->nameURI);
        d_str(mb, "select", v->select);
        membuffer_printf(mb, " tree=%d computed=%d\n", v->tree != NULL, v->computed);
    }
    d_hash(mb, style->attributeSets, scan_attr_set, "attribute-set", 2, depth);
    for (xsltTemplatePtr t = style->templates; t; t = t->next)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "template");
        d_str(mb, "match", t->match);
        d_str(mb, "name", t->name);
        d_str(mb, "name_uri", t->nameURI);
        d_str(mb, "mode", t->mode);
        d_str(mb, "mode_uri", t->modeURI);
        membuffer_printf(mb, " prio=%g position=%d content=%d inherited=%d", (double)t->priority, t->position,
                         t->content != NULL, t->inheritedNsNr);
        for (int i = 0; i < t->inheritedNsNr; ++i) d_str(mb, "ns", t->inheritedNs[i]->prefix);
        membuffer_append_char(mb, '\n');
    }
    d_patterns(mb, "root_match", (LxCompMatch*)style->rootMatch, depth);
    d_patterns(mb, "key_match", (LxCompMatch*)style->keyMatch, depth);
    d_patterns(mb, "elem_match", (LxCompMatch*)style->elemMatch, depth);
    d_patterns(mb, "attr_match", (LxCompMatch*)style->attrMatch, depth);
    d_patterns(mb, "parent_match", (LxCompMatch*)style->parentMatch, depth);
    d_patterns(mb, "text_match", (LxCompMatch*)style->textMatch, depth);
    d_patterns(mb, "pi_match", (LxCompMatch*)style->piMatch, depth);
    d_patterns(mb, "comment_match", (LxCompMatch*)style->commentMatch, depth);
    d_hash(mb, style->templatesHash, scan_templates, "hash", 3, depth);
    if (style->doc)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "document");
        if (style->doc->psvi) d_precomp(mb, style->doc->psvi);
        membuffer_append_char(mb, '\n');
        d_tree(mb, style->doc->children, depth + 1);
    }
    for (xsltDocumentPtr d = style->docList; d; d = d->next)
    {
        d_indent(mb, depth);
        membuffer_printf(mb, "include preproc=%d\n", d->preproc);
        d_tree(mb, d->doc->children, depth + 1);
    }
    for (xsltStylesheetPtr imp = style->imports; imp; imp = imp->next)
    {
        d_indent(mb, depth);
        membuffer_append(mb, "import\n");
        d_style(mb, imp, depth + 1);
    }
}

static char *describe_libxslt(xsltStylesheetPtr style)
{
    MemBuffer *mb = new_membuffer();
    d_style(mb, style, 0);
    return take(mb);
}

/** Print the first differing line with some context. */
static void show_diff(const char *what, const char *a, const char *b)
{
    const char *pa = a, *pb = b;
    int line = 1;
    while (*pa && *pa == *pb)
    {
        if (*pa == '\n') line++;
        pa++;
        pb++;
    }
    // Back up to the start of the line.
    while (pa > a && pa[-1] != '\n')
    {
        pa--;
        pb--;
    }
    const char *ea = strchr(pa, '\n');
    const char *eb = strchr(pb, '\n');
    int la = ea ? (int)(ea - pa) : (int)strlen(pa);
    int lb = eb ? (int)(eb - pb) : (int)strlen(pb);
    printf("  %s line %d\n    libxslt: %.*s\n    native:  %.*s\n", what, line, la, pa, lb, pb);
}

// Mutations, applied identically to both trees so that the error paths are exercised.

static unsigned long rng_ = 1;
static int mutants_ = 0;
static long mutated_ = 0;

static unsigned rnd(unsigned n)
{
    rng_ = rng_ * 6364136223846793005UL + 1442695040888963407UL;
    return n ? (unsigned)((rng_ >> 33) % n) : 0;
}

static const char *values_[] = { "", "1.0", "1.1", "2.0", "a:b", "*", "{", "}", "{@x}", "x{{y}}z", "{1+}", "foo(", "$x", "$undef",
    "yes", "no", "maybe", "#default", "xsl", "text", "html", "xml", "a b", "//x[1]", "1 2", "@*|node()", "key('k',.)",
    "id('a')", "-1", "NaN", "ascending", "descending", "upper-first", "lower-first", "number", "single", "any", "multiple",
    "unknown:prefix", "nope.xsl", "x|", "|x", "x[", "x[1]]", "a/b[@c='1']|d", "child::x", "ancestor::x", "x:*", "1a", ":x",
    "x:", "x::y", "processing-instruction('p')", "text()|comment()", "/", "a//b", "@x", "..", ".", "count(.)", "format-number(1,'#')",
    "A", "a", "i", "I", "1", "01", "&#x10ffff;", "\xc3\xa5\xc3\xa4\xc3\xb6", "\xe2\x80\xb0", "##", "xmlns", "xml:space" };
static const char *attr_names_[] = { "select", "match", "name", "mode", "priority", "test", "use", "href", "version",
    "exclude-result-prefixes", "extension-element-prefixes", "use-attribute-sets", "namespace", "disable-output-escaping",
    "elements", "method", "indent", "cdata-section-elements", "stylesheet-prefix", "result-prefix", "level", "count",
    "from", "value", "format", "order", "data-type", "case-order", "lang", "terminate", "grouping-separator",
    "grouping-size", "decimal-separator", "zero-digit", "digit", "encoding", "standalone", "omit-xml-declaration",
    "doctype-system", "doctype-public", "media-type", "letter-value" };
static const char *elem_names_[] = { "template", "apply-templates", "value-of", "if", "choose", "when", "otherwise",
    "for-each", "sort", "variable", "param", "with-param", "call-template", "attribute", "attribute-set", "element",
    "copy", "copy-of", "number", "text", "comment", "processing-instruction", "key", "output", "decimal-format",
    "namespace-alias", "strip-space", "preserve-space", "include", "import", "message", "fallback", "document",
    "apply-imports", "stylesheet", "transform", "bogus" };
#define NUM(a) (sizeof(a) / sizeof(a[0]))

typedef struct
{
    int op;        // 0 set attribute value, 1 add attribute, 2 remove attribute, 3 rename, 4 delete, 5 move first
    unsigned elem; // index of the element in document order
    unsigned attr; // index of the attribute (op 0 and 2)
    const char *name;
    const char *value;
} Mutation;

static void lx_elements(xmlNodePtr n, xmlNodePtr **v, size_t *num, size_t *max)
{
    for (; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE) continue;
        if (*num >= *max)
        {
            *max = *max ? 2 * *max : 64;
            *v = realloc(*v, *max * sizeof(xmlNodePtr));
        }
        (*v)[(*num)++] = n;
        lx_elements(n->children, v, num, max);
    }
}

static void nx_elements(XMQNode *n, XMQNode ***v, size_t *num, size_t *max)
{
    for (; n; n = xml_node_next(n))
    {
        if (xml_node_kind(n) != XML_DOM_ELEMENT) continue;
        if (*num >= *max)
        {
            *max = *max ? 2 * *max : 64;
            *v = realloc(*v, *max * sizeof(XMQNode*));
        }
        (*v)[(*num)++] = n;
        nx_elements(xml_node_first_child(n), v, num, max);
    }
}

/** Apply a mutation to both trees, returns false if it does not apply. */
static bool mutate(xmlDocPtr doc, XmlDom *dom, Mutation *m)
{
    xmlNodePtr *lv = NULL;
    XMQNode **nv = NULL;
    size_t ln = 0, lmax = 0, nn = 0, nmax = 0;
    lx_elements(doc->children, &lv, &ln, &lmax);
    nx_elements(xml_node_first_child(xml_dom_document(dom)), &nv, &nn, &nmax);
    bool ok = false;
    if (ln != nn || ln == 0) goto done;
    unsigned k = m->elem % (unsigned)ln;
    xmlNodePtr le = lv[k];
    XMQNode *ne = nv[k];
    switch (m->op)
    {
    case 0:
    case 2:
    {
        xmlAttrPtr la = le->properties;
        XMQAttr *na = xml_node_first_attr(ne);
        unsigned na_num = 0;
        for (xmlAttrPtr x = la; x; x = x->next) na_num++;
        if (!na_num) goto done;
        unsigned ai = m->attr % na_num;
        for (unsigned i = 0; i < ai; ++i)
        {
            la = la->next;
            na = na ? xml_attr_next(na) : NULL;
        }
        if (!na || strcmp((const char*)la->name, xml_attr_name(na))) goto done;
        char *name = strdup((const char*)la->name);
        const char *ns_uri = la->ns ? (const char*)la->ns->href : NULL;
        if (m->op == 0)
        {
            xmlSetNsProp(le, la->ns, la->name, (const xmlChar*)m->value);
            xml_node_set_attr(ne, name, xml_node_ns(xml_attr_node(na)), m->value);
        }
        else
        {
            xml_node_remove_attr(ne, name, ns_uri);
            xmlRemoveProp(la);
        }
        free(name);
        ok = true;
        break;
    }
    case 1:
        xmlSetProp(le, (const xmlChar*)m->name, (const xmlChar*)m->value);
        xml_node_set_attr(ne, m->name, NULL, m->value);
        ok = true;
        break;
    case 3:
        xmlNodeSetName(le, (const xmlChar*)m->name);
        xml_node_set_name(ne, m->name);
        ok = true;
        break;
    case 4:
        if (k == 0) goto done;
        xmlUnlinkNode(le);
        xmlFreeNode(le);
        xml_node_unlink(ne);
        xml_node_free(ne);
        ok = true;
        break;
    case 5:
    {
        if (k == 0) goto done;
        xmlNodePtr lfirst = lv[0]->children;
        XMQNode *nfirst = xml_node_first_child(nv[0]);
        if (!lfirst || !nfirst || lfirst == le || nfirst == ne) goto done;
        if (lfirst->type != XML_ELEMENT_NODE || xml_node_kind(nfirst) != XML_DOM_ELEMENT) goto done;
        xmlUnlinkNode(le);
        xmlAddPrevSibling(lfirst, le);
        xml_node_unlink(ne);
        xml_node_add_prev_sibling(nfirst, ne);
        ok = true;
        break;
    }
    }
done:
    free(lv);
    free(nv);
    return ok;
}

static void random_mutation(Mutation *m)
{
    m->op = (int)rnd(6);
    m->elem = rnd(1u << 30);
    m->attr = rnd(16);
    m->name = m->op == 3 ? elem_names_[rnd(NUM(elem_names_))] : attr_names_[rnd(NUM(attr_names_))];
    m->value = values_[rnd(NUM(values_))];
}

static void test_file(const char *path, int mutant)
{
    files_++;
    lx_msgs_ = new_membuffer();
    nx_msgs_ = new_membuffer();

    char *lx_desc = NULL, *nx_desc = NULL;
    bool lx_ok = false, nx_ok = false;

    xmlDocPtr doc = xmlReadFile(path, NULL, XSLT_PARSE_OPTIONS);
    XmlXsltSettings settings;
    xml_xslt_settings_default(&settings);
    settings.message_callback = capture_native;
    XmlDom *dom = xml_xslt_default_loader(&settings, path, XML_XSLT_LOAD_START);
    if (!doc || !dom)
    {
        // The parse error messages are tested by xml_parse_diff.
        if (verbose_) printf("SKIP %s (libxml2 %s native %s)\n", path, doc ? "parsed" : "failed", dom ? "parsed" : "failed");
        skipped_++;
        if (doc) xmlFreeDoc(doc);
        if (dom) xml_dom_free(dom);
        free_membuffer_and_free_content(lx_msgs_);
        free_membuffer_and_free_content(nx_msgs_);
        return;
    }
    char what[64] = "";
    if (mutant)
    {
        int nops = 1 + (int)rnd(3), applied = 0;
        MemBuffer *mb = new_membuffer();
        for (int i = 0; i < nops; ++i)
        {
            Mutation m;
            random_mutation(&m);
            if (mutate(doc, dom, &m))
            {
                applied++;
                membuffer_printf(mb, " op%d/%u/%u", m.op, m.elem, m.attr);
            }
        }
        free_membuffer_and_free_content(mb);
        snprintf(what, sizeof(what), " (mutant %d)", mutant);
        if (applied) mutated_++;
    }
    // Messages printed while parsing are not compared.
    free_membuffer_and_free_content(lx_msgs_);
    free_membuffer_and_free_content(nx_msgs_);
    lx_msgs_ = new_membuffer();
    nx_msgs_ = new_membuffer();

    xsltStylesheetPtr lx = xsltParseStylesheetDoc(doc);
    if (lx)
    {
        lx_ok = true;
        lx_desc = describe_libxslt(lx);
        xsltFreeStylesheet(lx);
    }
    else
    {
        lx_desc = strdup("FAILED\n");
        xmlFreeDoc(doc);
    }
    XmlXsltStylesheet *nx = xml_xslt_compile(dom, &settings);
    if (nx)
    {
        nx_ok = true;
        nx_desc = xml_xslt_stylesheet_describe(nx);
        xml_xslt_stylesheet_free(nx);
    }
    else
    {
        nx_desc = strdup("FAILED\n");
        xml_dom_free(dom);
    }
    char *lm = take(lx_msgs_);
    char *nm = take(nx_msgs_);

    bool same = !strcmp(lx_desc, nx_desc);
    bool msame = !strcmp(lm, nm);
    if (!lx_ok && !nx_ok) both_fail_++;
    if (same) same_++;
    else diff_++;
    if (!msame) msg_diff_++;
    if (*lm) with_msgs_++;
    if ((!same || !msame) && shown_ < max_shown_)
    {
        shown_++;
        printf("DIFF %s%s\n", path, what);
        if (!same) show_diff("stylesheet", lx_desc, nx_desc);
        if (!msame) show_diff("messages", lm, nm);
        if (!same || !msame) printf("  ---- libxslt messages\n%.3000s  ---- native messages\n%.3000s", lm, nm);
        if (verbose_ > 1)
        {
            printf("---- libxslt\n%s%s---- native\n%s%s", lx_desc, lm, nx_desc, nm);
        }
    }
    else if (verbose_)
    {
        printf("OK %s%s\n", path, what);
    }
    free(lx_desc);
    free(nx_desc);
    free(lm);
    free(nm);
}

int main(int argc, char **argv)
{
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; ++i)
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) max_shown_ = atoi(argv[++i]);
        // -m count:seed, the number of mutants of every stylesheet.
        else if (!strcmp(argv[i], "-m") && i + 1 < argc)
        {
            mutants_ = atoi(argv[++i]);
            const char *c = strchr(argv[i], ':');
            if (c) rng_ = strtoul(c + 1, NULL, 10);
        }
    }
    xmlSetStructuredErrorFunc(NULL, capture_structured);
    xsltSetGenericErrorFunc(NULL, capture_lx);
    xmlSetGenericErrorFunc(NULL, capture_lx);
    for (; i < argc; ++i)
    {
        for (int m = 0; m <= mutants_; ++m) test_file(argv[i], m);
    }
    printf("xml_xslt_compile_diff: %ld compilations (%ld mutated), %ld skipped (parse errors), %ld same (%ld both fail), %ld differ, "
           "%ld with messages, %ld message differences\n",
           files_, mutated_, skipped_, same_, both_fail_, diff_, with_msgs_, msg_diff_);
    xsltCleanupGlobals();
    xmlCleanupParser();
    return (diff_ || msg_diff_) ? 1 : 0;
}
