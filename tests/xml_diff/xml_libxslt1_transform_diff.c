/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native xslt transformation (parts/xml_libxslt1_transform.c) against libxslt.

   For every stylesheet given on the command line that has an input document (the
   same name with .xml, like libxslt's runtest), apply it with libxslt and with the
   native code (with the parameters test and test2 like runtest) and compare the
   result trees (built from the dom, not serialized), the output type and the
   messages printed. With -m count:seed every stylesheet is also applied after random
   identical mutations of both stylesheet trees (see xml_libxslt1_mutate.h).

   This tool links libxml2 and libxslt and is only used while libxmq moves away from them.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xpath.h"
#include"parts/xml_libxslt1_xslt.h"
#include"parts/xml_libxslt1_transform.h"
#include"parts/membuffer.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/hash.h>
#include<libxml/xmlerror.h>
#include<libxslt/xsltInternals.h>
#include<libxslt/xsltutils.h>
#include<libxslt/transform.h>
#include<libxslt/xslt.h>
#include<libxslt/extensions.h>
#include<libxslt/preproc.h>

#include<dirent.h>
#include<sys/stat.h>
#include<stdarg.h>
#include<unistd.h>
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
static long out_same_ = 0, out_diff_ = 0;
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

static void d_str(MemBuffer *mb, const char *label, const char *s)
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

static void d_indent(MemBuffer *mb, int depth)
{
    for (int i = 0; i < depth; ++i) membuffer_append(mb, "  ");
}

// The libxml2 result.

static void lx_tree(MemBuffer *mb, xmlNodePtr node, int depth)
{
    for (; node; node = node->next)
    {
        if (node->type == XML_DTD_NODE) continue;
        d_indent(mb, depth);
        switch (node->type)
        {
        case XML_ELEMENT_NODE:
            membuffer_append(mb, "element");
            d_str(mb, "name", (const char*)node->name);
            if (node->ns)
            {
                d_str(mb, "prefix", (const char*)node->ns->prefix);
                d_str(mb, "ns", (const char*)node->ns->href);
            }
            for (xmlNsPtr ns = node->nsDef; ns; ns = ns->next)
            {
                membuffer_append(mb, " xmlns");
                d_str(mb, "p", (const char*)ns->prefix);
                d_str(mb, "h", (const char*)ns->href);
            }
            membuffer_append_char(mb, '\n');
            for (xmlAttrPtr a = node->properties; a; a = a->next)
            {
                d_indent(mb, depth + 1);
                membuffer_append(mb, "attr");
                d_str(mb, "name", (const char*)a->name);
                if (a->ns)
                {
                    d_str(mb, "prefix", (const char*)a->ns->prefix);
                    d_str(mb, "ns", (const char*)a->ns->href);
                }
                xmlChar *v = xmlNodeListGetString(a->doc, a->children, 1);
                d_str(mb, "value", v ? (const char*)v : "");
                xmlFree(v);
                membuffer_append_char(mb, '\n');
            }
            lx_tree(mb, node->children, depth + 1);
            continue;
        case XML_TEXT_NODE:
            membuffer_append(mb, "text");
            if (node->name && !strcmp((const char*)node->name, "textnoenc")) membuffer_append(mb, " noenc");
            d_str(mb, "content", (const char*)node->content);
            break;
        case XML_CDATA_SECTION_NODE:
            membuffer_append(mb, "cdata");
            d_str(mb, "content", (const char*)node->content);
            break;
        case XML_COMMENT_NODE:
            membuffer_append(mb, "comment");
            d_str(mb, "content", (const char*)node->content);
            break;
        case XML_PI_NODE:
            membuffer_append(mb, "pi");
            d_str(mb, "name", (const char*)node->name);
            d_str(mb, "content", (const char*)node->content);
            break;
        case XML_ENTITY_REF_NODE:
            membuffer_append(mb, "entity-ref");
            d_str(mb, "name", (const char*)node->name);
            break;
        default:
            membuffer_printf(mb, "node libxml2-type=%d", (int)node->type);
            break;
        }
        membuffer_append_char(mb, '\n');
    }
}

static char *lx_describe(xmlDocPtr doc)
{
    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "result html=%d", doc->type == XML_HTML_DOCUMENT_NODE);
    d_str(mb, "version", (const char*)doc->version);
    d_str(mb, "encoding", (const char*)doc->encoding);
    membuffer_append_char(mb, '\n');
    if (doc->intSubset)
    {
        membuffer_append(mb, "dtd");
        d_str(mb, "name", (const char*)doc->intSubset->name);
        d_str(mb, "public", (const char*)doc->intSubset->ExternalID);
        d_str(mb, "system", (const char*)doc->intSubset->SystemID);
        membuffer_append_char(mb, '\n');
    }
    lx_tree(mb, doc->children, 0);
    return take(mb);
}

// The native result.

static void nx_tree(MemBuffer *mb, XMQNode *node, int depth)
{
    for (; node; node = xml_node_next(node))
    {
        if (xml_node_kind(node) == XML_DOM_DTD) continue;
        d_indent(mb, depth);
        switch (xml_node_kind(node))
        {
        case XML_DOM_ELEMENT:
            membuffer_append(mb, "element");
            d_str(mb, "name", xml_node_name(node));
            if (xml_node_ns(node))
            {
                d_str(mb, "prefix", xml_ns_prefix(xml_node_ns(node)));
                d_str(mb, "ns", xml_ns_href(xml_node_ns(node)));
            }
            for (XmlNs *ns = xml_node_first_ns_decl(node); ns; ns = xml_ns_next_decl(ns))
            {
                membuffer_append(mb, " xmlns");
                d_str(mb, "p", xml_ns_prefix(ns));
                d_str(mb, "h", xml_ns_href(ns));
            }
            membuffer_append_char(mb, '\n');
            for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
            {
                XMQNode *an = xml_attr_node(a);
                d_indent(mb, depth + 1);
                membuffer_append(mb, "attr");
                d_str(mb, "name", xml_attr_name(a));
                if (xml_node_ns(an))
                {
                    d_str(mb, "prefix", xml_ns_prefix(xml_node_ns(an)));
                    d_str(mb, "ns", xml_ns_href(xml_node_ns(an)));
                }
                char *v = xml_node_string_value(an);
                d_str(mb, "value", v);
                free(v);
                membuffer_append_char(mb, '\n');
            }
            nx_tree(mb, xml_node_first_child(node), depth + 1);
            continue;
        case XML_DOM_TEXT:
            membuffer_append(mb, "text");
            if (xml_node_name(node) && !strcmp(xml_node_name(node), "textnoenc")) membuffer_append(mb, " noenc");
            d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_CDATA:
            membuffer_append(mb, "cdata");
            d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_COMMENT:
            membuffer_append(mb, "comment");
            d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_PI:
            membuffer_append(mb, "pi");
            d_str(mb, "name", xml_node_name(node));
            d_str(mb, "content", xml_node_content(node));
            break;
        case XML_DOM_ENTITY_REF:
            membuffer_append(mb, "entity-ref");
            d_str(mb, "name", xml_node_name(node));
            break;
        default:
            membuffer_printf(mb, "node kind=%d", (int)xml_node_kind(node));
            break;
        }
        membuffer_append_char(mb, '\n');
    }
}

static char *nx_describe(XmlDom *dom, int type)
{
    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "result html=%d", type == XML_LIBXSLT1_OUTPUT_HTML);
    d_str(mb, "version", xml_dom_version(dom));
    d_str(mb, "encoding", xml_dom_encoding(dom));
    membuffer_append_char(mb, '\n');
    XMQNode *dtd = xml_dom_dtd(dom);
    if (dtd)
    {
        membuffer_append(mb, "dtd");
        d_str(mb, "name", xml_node_name(dtd));
        d_str(mb, "public", xml_dtd_public_id(dtd));
        d_str(mb, "system", xml_dtd_system_id(dtd));
        membuffer_append_char(mb, '\n');
    }
    nx_tree(mb, xml_node_first_child(xml_dom_document(dom)), 0);
    return take(mb);
}

/** Print the first differing line. */
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

#include"xml_libxslt1_mutate.h"

static long files_written_same_ = 0, files_written_diff_ = 0;

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 4096, n = 0;
    char *b = malloc(cap);
    size_t r;
    while ((r = fread(b + n, 1, cap - n, f)) > 0)
    {
        n += r;
        if (n == cap) b = realloc(b, cap *= 2);
    }
    fclose(f);
    *len = n;
    return b;
}

/** Compare the files xsl:document wrote in lx/ and nx/, then remove them. Returns true if equal. */
static bool compare_written_files(const char *what)
{
    bool same = true;
    const char *dirs[2] = { "lx", "nx" };
    for (int d = 0; d < 2; d++)
    {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir)))
        {
            if (e->d_name[0] == '.') continue;
            char a[1024], b[1024];
            snprintf(a, sizeof(a), "lx/%s", e->d_name);
            snprintf(b, sizeof(b), "nx/%s", e->d_name);
            size_t la = 0, lb = 0;
            char *ca = read_all(a, &la), *cb = read_all(b, &lb);
            bool eq = ca && cb && la == lb && !memcmp(ca, cb, la);
            if (d == 0 || !ca)
            {
                if (eq) files_written_same_++;
                else
                {
                    files_written_diff_++;
                    if (shown_ < max_shown_) printf("DIFF %s: written file %s differs\n", what, e->d_name);
                    same = false;
                }
            }
            free(ca);
            free(cb);
        }
        closedir(dir);
    }
    // Remove them.
    for (int d = 0; d < 2; d++)
    {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir)))
        {
            if (e->d_name[0] == '.') continue;
            char a[1024];
            snprintf(a, sizeof(a), "%s/%s", dirs[d], e->d_name);
            unlink(a);
        }
        closedir(dir);
    }
    return same;
}

static const char *params_[] = { "test", "'passed_value'", "test2", "'passed_value2'", NULL };

static void test_file(const char *path, int mutant)
{
    size_t pl = strlen(path);
    if (pl < 4 || strcmp(path + pl - 4, ".xsl")) return;
    char *input = strdup(path);
    strcpy(input + pl - 4, ".xml");
    FILE *f = fopen(input, "rb");
    if (!f)
    {
        free(input);
        return;
    }
    fclose(f);
    files_++;
    lx_msgs_ = new_membuffer();
    nx_msgs_ = new_membuffer();

    XmlLibxslt1Settings settings;
    xml_libxslt1_settings_default(&settings);
    settings.message_callback = capture_native;
    xmlDocPtr lx_style_doc = xmlReadFile(path, NULL, XSLT_PARSE_OPTIONS);
    xmlDocPtr lx_doc = xmlReadFile(input, NULL, XSLT_PARSE_OPTIONS);
    XmlDom *nx_style_doc = xml_libxslt1_default_loader(&settings, path, XML_LIBXSLT1_LOAD_START);
    XmlDom *nx_doc = xml_libxslt1_default_loader(&settings, input, XML_LIBXSLT1_LOAD_DOCUMENT);
    if (!lx_style_doc || !lx_doc || !nx_style_doc || !nx_doc)
    {
        if (verbose_) printf("SKIP %s (parse errors)\n", path);
        skipped_++;
        if (lx_style_doc) xmlFreeDoc(lx_style_doc);
        if (lx_doc) xmlFreeDoc(lx_doc);
        if (nx_style_doc) xml_dom_free(nx_style_doc);
        if (nx_doc) xml_dom_free(nx_doc);
        free_membuffer_and_free_content(lx_msgs_);
        free_membuffer_and_free_content(nx_msgs_);
        free(input);
        return;
    }
    if (mutant && mutate_both(lx_style_doc, nx_style_doc)) mutated_++;
    // Messages printed while parsing are not compared.
    free_membuffer_and_free_content(lx_msgs_);
    free_membuffer_and_free_content(nx_msgs_);
    lx_msgs_ = new_membuffer();
    nx_msgs_ = new_membuffer();

    char *lx_desc = NULL, *nx_desc = NULL;
    xmlChar *lx_out = NULL;
    int lx_out_len = 0;
    char *nx_out = NULL;
    size_t nx_out_len = 0;
    xsltStylesheetPtr lx = xsltParseStylesheetDoc(lx_style_doc);
    if (!lx)
    {
        xmlFreeDoc(lx_style_doc);
        lx_desc = strdup("NO STYLESHEET\n");
    }
    else
    {
        // Files written by xsl:document go below lx/ and nx/.
        if (chdir("lx") != 0) {}
        xmlDocPtr res = xsltApplyStylesheet(lx, lx_doc, params_);
        if (chdir("..") != 0) {}
        if (res)
        {
            lx_desc = lx_describe(res);
            xsltSaveResultToString(&lx_out, &lx_out_len, res, lx);
            xmlFreeDoc(res);
        }
        else
        {
            lx_desc = strdup("NO RESULT\n");
        }
        xsltFreeStylesheet(lx);
    }
    xmlFreeDoc(lx_doc);

    XmlLibxslt1Stylesheet *nx = xml_libxslt1_compile(nx_style_doc, &settings);
    if (!nx)
    {
        xml_dom_free(nx_style_doc);
        nx_desc = strdup("NO STYLESHEET\n");
    }
    else
    {
        int type = 0;
        if (chdir("nx") != 0) {}
        XmlDom *res = xml_libxslt1_apply_stylesheet(nx, nx_doc, params_, &type);
        if (chdir("..") != 0) {}
        if (res)
        {
            nx_desc = nx_describe(res, type);
            nx_out = xml_libxslt1_save_result_to_string(res, nx, type, &nx_out_len);
            xml_dom_free(res);
        }
        else
        {
            nx_desc = strdup("NO RESULT\n");
        }
        xml_libxslt1_stylesheet_free(nx);
    }
    xml_dom_free(nx_doc);

    char *lm = take(lx_msgs_);
    char *nm = take(nx_msgs_);
    bool same = !strcmp(lx_desc, nx_desc);
    bool msame = !strcmp(lm, nm);
    if (!compare_written_files(path)) diff_++;
    // The serialized results (compared when both have one).
    bool osame = true;
    if (lx_out || nx_out)
    {
        size_t ll = lx_out ? (size_t)lx_out_len : 0;
        osame = (ll == nx_out_len) && (!ll || !memcmp(lx_out, nx_out, ll));
        if (osame) out_same_++;
        else out_diff_++;
    }
    if (!strncmp(lx_desc, "NO ", 3) && !strncmp(nx_desc, "NO ", 3)) both_fail_++;
    if (same && osame) same_++;
    else diff_++;
    if (!msame) msg_diff_++;
    if (*lm) with_msgs_++;
    if ((!same || !msame || !osame) && shown_ < max_shown_)
    {
        shown_++;
        if (mutant) printf("DIFF %s (mutant %d)\n", path, mutant);
        else printf("DIFF %s\n", path);
        if (!same) show_diff("result", lx_desc, nx_desc);
        if (!msame) show_diff("messages", lm, nm);
        if (!osame)
        {
            size_t ll = lx_out ? (size_t)lx_out_len : 0, k = 0;
            while (k < ll && k < nx_out_len && lx_out[k] == (unsigned char)nx_out[k]) k++;
            size_t from = k > 40 ? k - 40 : 0;
            printf("  output differs at byte %zu (libxslt %zu bytes, native %zu bytes)\n    libxslt: ", k, ll, nx_out_len);
            for (size_t j = from; j < ll && j < k + 40; j++) putchar(lx_out[j] == '\n' ? '|' : lx_out[j]);
            printf("\n    native:  ");
            for (size_t j = from; j < nx_out_len && j < k + 40; j++) putchar(nx_out[j] == '\n' ? '|' : nx_out[j]);
            printf("\n");
        }
        if (verbose_ > 1) printf("---- libxslt\n%s%s---- native\n%s%s", lx_desc, lm, nx_desc, nm);
        else printf("  ---- libxslt messages\n%.2000s  ---- native messages\n%.2000s", lm, nm);
    }
    else if (verbose_)
    {
        printf("OK %s\n", path);
    }
    free(lx_desc);
    free(nx_desc);
    xmlFree(lx_out);
    free(nx_out);
    free(lm);
    free(nm);
    free(input);
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
    mkdir("lx", 0777);
    mkdir("nx", 0777);
    for (; i < argc; ++i)
    {
        for (int m = 0; m <= mutants_; ++m) test_file(argv[i], m);
    }
    printf("xml_libxslt1_transform_diff: %ld transformations (%ld mutated), %ld skipped (parse errors), %ld same (%ld both fail), %ld differ, "
           "%ld with messages, %ld message differences, outputs %ld same %ld differ, written files %ld same %ld differ\n",
           files_, mutated_, skipped_, same_, both_fail_, diff_, with_msgs_, msg_diff_, out_same_, out_diff_,
           files_written_same_, files_written_diff_);
    xsltCleanupGlobals();
    xmlCleanupParser();
    return (diff_ || msg_diff_) ? 1 : 0;
}
