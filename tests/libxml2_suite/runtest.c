// libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
//
// Run the native xml parts (src/main/c/parts) over the libxml2 test suite
// (tests/libxml2/test) and compare the output, and the error output where
// libxml2's runtest.c compares it, byte for byte with the expected results
// in tests/libxml2/result. Only native code: libxml2 is not needed.
//
// The tests are driven like the functions of libxml2's runtest.c (in
// 3rdparty/libxml2/runtest.c when fetched) with the same names:
//
// xml          oldParseTest    ./test/*                  result/<name> (and the round trip)
// noent        noentParseTest  ./test/*                  result/noent/<name>
// namespaces   errParseTest    ./test/namespaces/*       result/namespaces/<name>(.err)
// errors       errParseTest    ./test/errors/*.xml       result/errors/<name>(.err)
// errors-noent errParseTest    ./test/errors/*.xml       result/errors/<name>.ent (XML_PARSE_NOENT)
// html         errParseTest    ./test/HTML/*             result/HTML/<name>(.err)
// xpath-expr   xpathExprTest   ./test/XPath/expr/*       result/XPath/expr/<name>
// xpath-doc    xpathDocTest    ./test/XPath/docs/*       result/XPath/tests/<name>
// uri-parse    uriParseTest    ./test/URI/*.uri          result/URI/<name>
// uri-base     uriBaseTest     ./test/URI/*.data         result/URI/<name>
// schemas      schemasTest     ./test/schemas/*_*.xsd    result/schemas/<prefix>_<n>.err
//              schemasXsiTest  ./test/schemas/any1_0.xml result/schemas/any1_0_0.err
// pattern      patternTest     ./test/pattern/*.pat      result/pattern/<name>
// regexp       regexpTest      ./test/regexp/*           result/regexp/<name>(.err)
// automata     automataTest    ./test/automata/*         result/automata/<name>
//
// The libxml2 printing helpers used by these tests (xmlXPathDebugDumpObject,
// xmlPrintURI, xmlFormatError for the schema and regexp errors, xmlGetNodePath)
// are ported below onto the native api. Modes that libxmq does not have (the
// reader, sax, push and memory variants, DTD validation, xinclude, c14n, relaxng,
// schematron, catalogs, xpointer) are not run.
//
// Usage: runtest [-v] [-k known_failures.txt] [-d libxml2_test_dir] [category...]
// The test names are category:file, eg xml:./test/att1. Tests listed in the known
// failures file may fail, the exit status is non zero if any other test fails or
// a known failure passes. Run from the repository root (the default -d is
// tests/libxml2, the program changes into it like runtest.c runs from libxml2's
// source directory, the file names in the messages are ./test/...).

#include"parts/always.h"
#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_save.h"
#include"parts/xml_html_parser.h"
#include"parts/xml_html_save.h"
#include"parts/xml_xpath.h"
#include"parts/xml_uri.h"
#include"parts/xml_regexp.h"
#include"parts/xml_pattern.h"
#include"parts/xml_xsd10_schemas.h"

#include<glob.h>
#include<math.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<sys/stat.h>
#include<unistd.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

// Buffers ////////////////////////////////////////////////////////

typedef struct
{
    char *s;
    size_t len, max;
} Buf;

static void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->max)
    {
        b->max = (b->len + n + 1) * 2;
        b->s = (char*)realloc(b->s, b->max);
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}

static void buf_str(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }

static void buf_printf(Buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *tmp = (char*)malloc(n + 1);
    va_start(ap, fmt);
    vsnprintf(tmp, n + 1, fmt, ap);
    va_end(ap);
    buf_add(b, tmp, n);
    free(tmp);
}

static void buf_putc(Buf *b, char c) { buf_add(b, &c, 1); }
static const char *buf_get(const Buf *b) { return b->s ? b->s : ""; }
static void buf_clear(Buf *b) { b->len = 0; if (b->s) b->s[0] = 0; }
static void buf_free(Buf *b) { free(b->s); b->s = NULL; b->len = b->max = 0; }

static char *read_all(const char *path, size_t *len)
{
    *len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = {0};
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    if (!b.s) buf_add(&b, "", 0);
    *len = b.len;
    return b.s;
}

static const char *base_filename(const char *filename)
{
    const char *s = strrchr(filename, '/');
    return s ? s + 1 : filename;
}

static int is_regular_file(const char *filename)
{
    struct stat st;
    return stat(filename, &st) == 0 && S_ISREG(st.st_mode);
}

// Test bookkeeping ///////////////////////////////////////////////

static int verbose_;
static char **known_;
static int *known_seen_;
static size_t num_known_;

typedef struct
{
    const char *name;
    long passed, failed, known, known_passed;
} Category;

static Category categories_[] = {
    { "xml" }, { "noent" }, { "namespaces" }, { "errors" }, { "errors-noent" }, { "html" },
    { "xpath-expr" }, { "xpath-doc" }, { "uri-parse" }, { "uri-base" }, { "schemas" },
    { "pattern" }, { "regexp" }, { "automata" },
};
#define NUM_CATEGORIES (sizeof(categories_) / sizeof(categories_[0]))
static Category *cat_;

static bool is_known(const char *test)
{
    for (size_t i = 0; i < num_known_; i++)
    {
        if (!strcmp(known_[i], test))
        {
            known_seen_[i] = 1;
            return true;
        }
    }
    return false;
}

static void show(Buf *why, const char *label, const char *s, size_t len)
{
    buf_printf(why, "  ---- %s (%zu bytes)\n", label, len);
    buf_add(why, s, len > 3000 ? 3000 : len);
    if (len > 3000) buf_str(why, "\n  ...\n");
    else if (len && s[len - 1] != '\n') buf_str(why, "\n");
}

/** Compare with an expected file like runtest.c compareFileMem: a missing file is the same as an empty one. */
static bool check_file(const char *file, const char *got, size_t got_len, Buf *why, const char *what)
{
    size_t elen;
    char *expected = read_all(file, &elen);
    bool same = (!expected && got_len == 0) || (expected && elen == got_len && !memcmp(expected, got, got_len));
    if (!same)
    {
        size_t at = 0;
        while (expected && at < elen && at < got_len && expected[at] == got[at]) at++;
        buf_printf(why, "  %s differs from %s at byte %zu\n", what, file, at);
        if (verbose_)
        {
            char label[1200];
            snprintf(label, sizeof(label), "expected %s", file);
            show(why, label, expected ? expected : "", expected ? elen : 0);
            show(why, "native", got, got_len);
        }
    }
    free(expected);
    return same;
}

/** Record the result of a test. why holds the reasons of a failure. */
static void result(const char *file, bool ok, Buf *why)
{
    char test[2048];
    snprintf(test, sizeof(test), "%s:%s", cat_->name, file);
    bool known = is_known(test);
    if (ok)
    {
        cat_->passed++;
        if (known)
        {
            cat_->known_passed++;
            printf("KNOWN FAILURE PASSES %s\n", test);
        }
        else if (verbose_ > 1) printf("OK %s\n", test);
    }
    else if (known)
    {
        cat_->known++;
        if (verbose_) printf("KNOWN %s\n%s", test, buf_get(why));
    }
    else
    {
        cat_->failed++;
        printf("FAIL %s\n%s", test, buf_get(why));
    }
}

static glob_t glob_files(const char *pattern)
{
    glob_t g;
    memset(&g, 0, sizeof(g));
    glob(pattern, 0, NULL, &g);
    return g;
}

// Parsing ////////////////////////////////////////////////////////

static void collect_message(void *user, const char *msg)
{
    buf_str((Buf*)user, msg);
}

/** Parse an xml file like xmlCtxtReadFile, the messages (recoverable and the fatal one) are
    appended to msgs if not NULL. With noent the entities are substituted (XML_PARSE_NOENT)
    and dtd_attr adds the default attributes (XML_PARSE_DTDATTR). */
static XmlDom *parse_xml_file(const char *filename, Buf *msgs, bool noent, bool dtd_attr)
{
    size_t len;
    char *buf = read_all(filename, &len);
    if (!buf) return NULL;
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = filename;
    settings.dtd_attr = dtd_attr;
    settings.message_callback = msgs ? collect_message : NULL;
    settings.message_user_data = msgs;
    XmlParseError err;
    memset(&err, 0, sizeof(err));
    XmlDom *dom = xml_parse_buffer(buf, buf + len, &settings, &err);
    if (!dom && msgs && err.message) buf_str(msgs, err.message);
    xml_parse_error_free(&err);
    if (dom && noent)
    {
        memset(&err, 0, sizeof(err));
        if (!xml_expand_entities(dom, NULL, NULL, len, &settings, &err))
        {
            // libxml2 gives no document when the expansion fails.
            if (msgs && err.message) buf_str(msgs, err.message);
            xml_dom_free(dom);
            dom = NULL;
        }
        xml_parse_error_free(&err);
    }
    if (dom) xml_dom_set_url(dom, filename);
    free(buf);
    return dom;
}

/** Convert the UTF-8 output to the encoding (no conversion for UTF-8 or an unsupported encoding). */
static char *transcode(char *utf8, size_t *len, const char *encoding)
{
    if (!encoding || !strcasecmp(encoding, "UTF-8") || !strcasecmp(encoding, "UTF8") || !xml_save_encoding_supported(encoding))
    {
        return utf8;
    }
    MemBuffer *out = new_membuffer();
    xml_save_transcode(out, utf8, *len, encoding);
    free(utf8);
    *len = out->used_;
    membuffer_append_null(out);
    return free_membuffer_but_return_trimmed_content(out);
}

/** xmlSaveFile / xmlDocDumpMemory: the document in its own encoding. */
static char *save_xml(XmlDom *dom, size_t *len)
{
    char *utf8 = xml_save_doc(dom, NULL, false, len);
    return transcode(utf8, len, xml_dom_encoding(dom));
}

// XML regression tests (oldParseTest) ////////////////////////////

static void old_parse_test(const char *filename, const char *result_file)
{
    Buf why = {0};
    bool ok = false;
    XmlDom *dom = parse_xml_file(filename, NULL, false, false);
    if (!dom)
    {
        buf_str(&why, "  native rejects the document\n");
    }
    else
    {
        size_t len;
        char *out = save_xml(dom, &len);
        xml_dom_free(dom);
        ok = check_file(result_file, out, len, &why, "output");
        // Parse the saved result to make sure the round trip is okay.
        XmlParseSettings settings;
        xml_parse_settings_default(&settings);
        settings.source_name = filename;
        XmlParseError err;
        memset(&err, 0, sizeof(err));
        XmlDom *dom2 = xml_parse_buffer(out, out + len, &settings, &err);
        xml_parse_error_free(&err);
        if (!dom2)
        {
            buf_str(&why, "  native rejects its own output\n");
            ok = false;
        }
        else
        {
            size_t len2;
            char *out2 = save_xml(dom2, &len2);
            if (ok && !check_file(result_file, out2, len2, &why, "round trip output")) ok = false;
            free(out2);
            xml_dom_free(dom2);
        }
        free(out);
    }
    result(filename, ok, &why);
    buf_free(&why);
}

static void noent_parse_test(const char *filename, const char *result_file)
{
    Buf why = {0};
    bool ok = false;
    XmlDom *dom = parse_xml_file(filename, NULL, true, false);
    if (!dom)
    {
        buf_str(&why, "  native rejects the document\n");
    }
    else
    {
        size_t len;
        char *out = save_xml(dom, &len);
        ok = check_file(result_file, out, len, &why, "output");
        free(out);
        xml_dom_free(dom);
    }
    result(filename, ok, &why);
    buf_free(&why);
}

/** errParseTest: the result (if result_file) and the errors (if err_file). */
static void err_parse_test(const char *filename, const char *result_file, const char *err_file, bool html, bool noent)
{
    Buf why = {0};
    Buf msgs = {0};
    bool ok = true;
    XmlDom *dom;
    if (html)
    {
        size_t len;
        char *buf = read_all(filename, &len);
        XmlHtmlParseSettings settings;
        xml_html_parse_settings_default(&settings);
        settings.source_name = filename;
        settings.message_callback = collect_message;
        settings.message_user_data = &msgs;
        dom = xml_html_parse_buffer(buf, buf + len, &settings);
        free(buf);
    }
    else
    {
        dom = parse_xml_file(filename, &msgs, noent, false);
    }
    if (result_file)
    {
        size_t len = 0;
        char *out = NULL;
        if (dom) out = html ? xml_html_save_doc(dom, true, &len) : save_xml(dom, &len);
        ok = check_file(result_file, out ? out : "", len, &why, "output");
        free(out);
    }
    // Like runtest, the errors are only compared when the result is the same.
    if (ok && err_file) ok = check_file(err_file, buf_get(&msgs), msgs.len, &why, "errors");
    if (dom) xml_dom_free(dom);
    result(filename, ok, &why);
    buf_free(&msgs);
    buf_free(&why);
}

// XPath (xpathExprTest, xpathDocTest) ////////////////////////////

/** xmlDebugDumpString: at most 40 bytes, blanks as spaces, non ascii bytes as #XX. */
static void dump_string(Buf *o, const char *str)
{
    if (!str)
    {
        buf_str(o, "(NULL)");
        return;
    }
    for (int i = 0; i < 40; i++)
    {
        unsigned char c = (unsigned char)str[i];
        if (c == 0) return;
        if (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D) buf_putc(o, ' ');
        else if (c >= 0x80) buf_printf(o, "#%X", c);
        else buf_putc(o, (char)c);
    }
    buf_str(o, "...");
}

/** xmlCtxtDumpSpaces. */
static void dump_spaces(Buf *o, int depth)
{
    if (depth <= 0) return;
    if (depth > 50) depth = 50;
    for (int i = 0; i < 2 * depth; i++) buf_putc(o, ' ');
}

static void dump_node_list(Buf *o, XMQNode *node, int depth);

/** xmlCtxtDumpNamespace. */
static void dump_namespace(Buf *o, const char *prefix, const char *href, int depth)
{
    dump_spaces(o, depth);
    if (!href) return; // libxml2 reports an incomplete namespace as a debug error.
    if (prefix) buf_printf(o, "namespace %s href=", prefix);
    else buf_str(o, "default namespace href=");
    dump_string(o, href);
    buf_str(o, "\n");
}

/** xmlCtxtDumpAttr. */
static void dump_attr(Buf *o, XMQNode *attr, int depth)
{
    dump_spaces(o, depth);
    buf_str(o, "ATTRIBUTE ");
    dump_string(o, xml_node_name(attr));
    buf_str(o, "\n");
    if (xml_node_first_child(attr)) dump_node_list(o, xml_node_first_child(attr), depth + 1);
}

/** xmlCtxtDumpEntity. */
static void dump_entity(Buf *o, XMQNode *ent, int depth)
{
    dump_spaces(o, depth);
    switch (xml_entity_decl_type(ent))
    {
    case XML_ENTITY_INTERNAL_GENERAL: buf_str(o, "INTERNAL_GENERAL_ENTITY "); break;
    case XML_ENTITY_EXTERNAL_GENERAL_PARSED: buf_str(o, "EXTERNAL_GENERAL_PARSED_ENTITY "); break;
    case XML_ENTITY_EXTERNAL_GENERAL_UNPARSED: buf_str(o, "EXTERNAL_GENERAL_UNPARSED_ENTITY "); break;
    case XML_ENTITY_INTERNAL_PARAMETER: buf_str(o, "INTERNAL_PARAMETER_ENTITY "); break;
    case XML_ENTITY_EXTERNAL_PARAMETER: buf_str(o, "EXTERNAL_PARAMETER_ENTITY "); break;
    default: buf_printf(o, "ENTITY_%d ! ", (int)xml_entity_decl_type(ent));
    }
    buf_printf(o, "%s\n", xml_node_name(ent));
    if (xml_entity_decl_public_id(ent))
    {
        dump_spaces(o, depth);
        buf_printf(o, "ExternalID=%s\n", xml_entity_decl_public_id(ent));
    }
    if (xml_entity_decl_system_id(ent))
    {
        dump_spaces(o, depth);
        buf_printf(o, "SystemID=%s\n", xml_entity_decl_system_id(ent));
    }
    if (xml_node_content(ent))
    {
        dump_spaces(o, depth);
        buf_str(o, "content=");
        dump_string(o, xml_node_content(ent));
        buf_str(o, "\n");
    }
}

/** xmlCtxtDumpOneNode (not recursive). */
static void dump_one_node(Buf *o, XMQNode *node, int depth)
{
    XmlNodeKind kind = xml_node_kind(node);
    switch (kind)
    {
    case XML_DOM_ELEMENT:
        dump_spaces(o, depth);
        buf_str(o, "ELEMENT ");
        if (xml_node_ns(node) && xml_ns_prefix(xml_node_ns(node)))
        {
            dump_string(o, xml_ns_prefix(xml_node_ns(node)));
            buf_str(o, ":");
        }
        dump_string(o, xml_node_name(node));
        buf_str(o, "\n");
        break;
    case XML_DOM_ATTRIBUTE:
        dump_spaces(o, depth);
        buf_str(o, "Error, ATTRIBUTE found here\n");
        return;
    case XML_DOM_TEXT:
        dump_spaces(o, depth);
        buf_str(o, "TEXT\n");
        break;
    case XML_DOM_CDATA:
        dump_spaces(o, depth);
        buf_str(o, "CDATA_SECTION\n");
        break;
    case XML_DOM_ENTITY_REF:
        dump_spaces(o, depth);
        buf_printf(o, "ENTITY_REF(%s)\n", xml_node_name(node));
        break;
    case XML_DOM_PI:
        dump_spaces(o, depth);
        buf_printf(o, "PI %s\n", xml_node_name(node));
        break;
    case XML_DOM_COMMENT:
        dump_spaces(o, depth);
        buf_str(o, "COMMENT\n");
        break;
    case XML_DOM_DOCUMENT:
        dump_spaces(o, depth);
        buf_str(o, "Error, DOCUMENT found here\n");
        return;
    case XML_DOM_DTD:
        dump_spaces(o, depth);
        buf_printf(o, "DTD(%s)", xml_node_name(node));
        if (xml_dtd_public_id(node)) buf_printf(o, ", PUBLIC %s", xml_dtd_public_id(node));
        if (xml_dtd_system_id(node)) buf_printf(o, ", SYSTEM %s", xml_dtd_system_id(node));
        buf_str(o, "\n");
        return;
    case XML_DOM_NAMESPACE:
        dump_namespace(o, xml_node_name(node), xml_node_content(node), depth);
        return;
    default:
        dump_spaces(o, depth);
        buf_printf(o, "Unknown node type %d\n", (int)kind);
        return;
    }
    if (kind == XML_DOM_ELEMENT)
    {
        for (XmlNs *ns = xml_node_first_ns_decl(node); ns; ns = xml_ns_next_decl(ns))
        {
            dump_namespace(o, xml_ns_prefix(ns), xml_ns_href(ns), depth + 1);
        }
        for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
        {
            dump_attr(o, xml_attr_node(a), depth + 1);
        }
    }
    if (kind == XML_DOM_ENTITY_REF)
    {
        XMQNode *ent = xml_dom_get_entity(xml_node_dom(node), xml_node_name(node));
        if (ent) dump_entity(o, ent, depth + 1);
    }
    else if (kind != XML_DOM_ELEMENT && xml_node_content(node))
    {
        dump_spaces(o, depth + 1);
        buf_str(o, "content=");
        dump_string(o, xml_node_content(node));
        buf_str(o, "\n");
    }
}

/** xmlCtxtDumpNodeList (recursive). */
static void dump_node_list(Buf *o, XMQNode *node, int depth)
{
    for (; node; node = xml_node_next(node))
    {
        dump_one_node(o, node, depth);
        if (xml_node_kind(node) != XML_DOM_ENTITY_REF && xml_node_first_child(node))
        {
            dump_node_list(o, xml_node_first_child(node), depth + 1);
        }
    }
}

/** xmlXPathDebugDumpNode. */
static void xpath_dump_node(Buf *o, XMQNode *cur, int depth)
{
    if (xml_node_kind(cur) == XML_DOM_DOCUMENT)
    {
        dump_spaces(o, depth > 25 ? 25 : depth);
        buf_str(o, " /\n");
    }
    else if (xml_node_kind(cur) == XML_DOM_ATTRIBUTE) dump_attr(o, cur, depth);
    else dump_one_node(o, cur, depth);
}

/** xmlXPathDebugDumpObject with depth 0. */
static void xpath_dump_object(Buf *o, XmlXPathResult *cur)
{
    if (!cur)
    {
        buf_str(o, "Object is empty (NULL)\n");
        return;
    }
    switch (xml_xpath_result_type(cur))
    {
    case XML_XPATH_NODESET:
    {
        size_t n = xml_xpath_result_size(cur);
        if (xml_xpath_result_is_tree(cur))
        {
            buf_str(o, "Object is an XSLT value tree :\n");
            break;
        }
        buf_str(o, "Object is a Node Set :\n");
        buf_printf(o, "Set contains %zu nodes:\n", n);
        for (size_t i = 0; i < n; i++)
        {
            buf_printf(o, "%zu", i + 1);
            xpath_dump_node(o, xml_xpath_result_node(cur, i), 1);
        }
        break;
    }
    case XML_XPATH_BOOLEAN:
        buf_printf(o, "Object is a Boolean : %s\n", xml_xpath_result_boolean(cur) ? "true" : "false");
        break;
    case XML_XPATH_NUMBER:
    {
        double d = xml_xpath_result_number(cur);
        if (isinf(d)) buf_str(o, d > 0 ? "Object is a number : Infinity\n" : "Object is a number : -Infinity\n");
        else if (isnan(d)) buf_str(o, "Object is a number : NaN\n");
        else if (d == 0) buf_str(o, "Object is a number : 0\n");
        else buf_printf(o, "Object is a number : %0g\n", d);
        break;
    }
    case XML_XPATH_STRING:
        buf_str(o, "Object is a string : ");
        dump_string(o, xml_xpath_result_string(cur));
        buf_str(o, "\n");
        break;
    }
}

/** xpathCommonTest: every line of filename is an expression, evaluated with the root element
    of doc (if any) as context node. */
static void xpath_common_test(const char *filename, const char *result_file, XmlDom *doc)
{
    FILE *input = fopen(filename, "rb");
    if (!input) return;
    Buf out = {0};
    char expression[5000];
    XmlDom *empty = doc ? NULL : xml_dom_new();
    while (fgets(expression, 4500, input) != NULL)
    {
        int len = (int)strlen(expression);
        len--;
        while ((len >= 0) &&
               ((expression[len] == '\n') || (expression[len] == '\t') ||
                (expression[len] == '\r') || (expression[len] == ' '))) len--;
        expression[len + 1] = 0;
        if (len < 0) continue;
        buf_printf(&out, "\n========================\nExpression: %s\n", expression);
        XmlXPathSettings settings;
        xml_xpath_settings_default(&settings);
        XMQNode *context;
        if (doc)
        {
            context = xml_dom_root_element(doc);
            if (!context)
            {
                context = xml_dom_document(doc);
                settings.no_context_node = true;
            }
        }
        else
        {
            // xmlXPathNewContext(NULL): no document and no context node.
            context = xml_dom_document(empty);
            settings.no_context_node = true;
        }
        XmlXPathError err;
        memset(&err, 0, sizeof(err));
        XmlXPathResult *res = xml_xpath_eval(expression, context, &settings, &err);
        xpath_dump_object(&out, res);
        if (res) xml_xpath_result_free(res);
    }
    fclose(input);
    if (empty) xml_dom_free(empty);
    Buf why = {0};
    bool ok = check_file(result_file, buf_get(&out), out.len, &why, "output");
    result(filename, ok, &why);
    buf_free(&why);
    buf_free(&out);
}

static void xpath_doc_test(const char *filename)
{
    XmlDom *doc = parse_xml_file(filename, NULL, true, true);
    if (!doc)
    {
        Buf why = {0};
        buf_str(&why, "  native rejects the document\n");
        result(filename, false, &why);
        buf_free(&why);
        return;
    }
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "./test/XPath/tests/%s*", base_filename(filename));
    glob_t g = glob_files(pattern);
    for (size_t i = 0; i < g.gl_pathc; i++)
    {
        char res[700];
        snprintf(res, sizeof(res), "result/XPath/tests/%s", base_filename(g.gl_pathv[i]));
        xpath_common_test(g.gl_pathv[i], res, doc);
    }
    globfree(&g);
    xml_dom_free(doc);
}

// URI (uriParseTest, uriBaseTest) ////////////////////////////////

static void handle_uri(const char *str, const char *base, Buf *o)
{
    if (!base)
    {
        XmlURI *uri = xml_uri_create();
        int ret = xml_uri_parse_reference(uri, str);
        if (ret != 0) buf_printf(o, "%s : error %d\n", str, ret);
        else
        {
            if (uri->path) xml_uri_normalize_uri_path(uri->path);
            // xmlPrintURI
            char *s = xml_uri_save(uri);
            if (s) buf_str(o, s);
            free(s);
            buf_str(o, "\n");
        }
        xml_uri_free(uri);
    }
    else
    {
        char *res = xml_uri_build(str, base);
        if (res) buf_printf(o, "%s\n", res);
        else buf_str(o, "::ERROR::\n");
        free(res);
    }
}

static void uri_common_test(const char *filename, const char *result_file, const char *base)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return;
    Buf out = {0};
    char str[1024];
    while (fgets(str, sizeof(str) - 1, f) != NULL)
    {
        int i = (int)strlen(str);
        while ((i > 0) && ((str[i - 1] == '\n') || (str[i - 1] == '\r') || (str[i - 1] == ' ') || (str[i - 1] == '\t')))
        {
            i--;
            str[i] = 0;
        }
        handle_uri(str, base, &out);
    }
    fclose(f);
    Buf why = {0};
    bool ok = check_file(result_file, buf_get(&out), out.len, &why, "output");
    result(filename, ok, &why);
    buf_free(&why);
    buf_free(&out);
}

// Schemas (schemasTest, schemasXsiTest) //////////////////////////

static void schema_error(void *ctx, const XmlXsd10Error *err)
{
    char *t = xml_xsd10_format_error_string(err);
    if (t) buf_str((Buf*)ctx, t);
    free(t);
}

/** schemasOneTest (the in memory round, the streaming round of runtest has the same expectations). */
static void schemas_one_test(const char *sch, const char *filename, const char *err_file, XmlXsd10Schema *schema,
                             const char *parse_errors)
{
    char name[1300];
    snprintf(name, sizeof(name), "%s:%s", sch, filename);
    Buf errors = {0};
    Buf why = {0};
    buf_str(&errors, parse_errors);
    bool ok;
    XmlDom *doc = parse_xml_file(filename, NULL, false, false);
    if (!doc)
    {
        buf_printf(&why, "  failed to parse instance %s for %s\n", filename, sch);
        ok = false;
    }
    else
    {
        XmlXsd10ValidCtxt *ctxt = xml_xsd10_new_valid_ctxt(schema);
        xml_xsd10_set_valid_structured_errors(ctxt, schema_error, &errors);
        int ret = xml_xsd10_validate_doc(ctxt, doc);
        if (ret == 0) buf_printf(&errors, "%s validates\n", filename);
        else if (ret > 0) buf_printf(&errors, "%s fails to validate\n", filename);
        else buf_printf(&errors, "%s validation generated an internal error\n", filename);
        xml_xsd10_free_valid_ctxt(ctxt);
        xml_dom_free(doc);
        ok = check_file(err_file, buf_get(&errors), errors.len, &why, "errors");
    }
    result(name, ok, &why);
    buf_free(&why);
    buf_free(&errors);
}

static void schemas_test(const char *filename)
{
    const char *base = base_filename(filename);
    char pattern[600], prefix[500], err[700];
    Buf parse_errors = {0};
    buf_str(&parse_errors, "");
    XmlXsd10ParserCtxt *ctxt = xml_xsd10_new_parser_ctxt(filename);
    xml_xsd10_set_parser_structured_errors(ctxt, schema_error, &parse_errors);
    XmlXsd10Schema *schema = xml_xsd10_parse(ctxt);
    xml_xsd10_free_parser_ctxt(ctxt);

    int len = (int)strlen(base);
    if (len > 499 || len < 5)
    {
        if (schema) xml_xsd10_free(schema);
        buf_free(&parse_errors);
        return;
    }
    len -= 4; // remove trailing .xsd
    if (base[len - 2] == '_') len -= 2; // remove subtest number
    if (base[len - 2] == '_') len -= 2; // remove subtest number
    memcpy(prefix, base, len);
    prefix[len] = 0;
    snprintf(pattern, sizeof(pattern), "./test/schemas/%s_*.xml", prefix);
    if (base[len] == '_')
    {
        len += 2;
        memcpy(prefix, base, len);
        prefix[len] = 0;
    }
    glob_t g = glob_files(pattern);
    for (size_t i = 0; i < g.gl_pathc; i++)
    {
        const char *instance = g.gl_pathv[i];
        const char *base2 = base_filename(instance);
        int len2 = (int)strlen(base2);
        if (!(len2 > 6 && base2[len2 - 6] == '_')) continue; // runtest: don't know how to process
        snprintf(err, sizeof(err), "result/schemas/%s_%c.err", prefix, base2[len2 - 5]);
        if (!schema)
        {
            // Schema load failed, verify the error message.
            char name[1300];
            snprintf(name, sizeof(name), "%s:%s", filename, instance);
            Buf why = {0};
            bool ok = check_file(err, buf_get(&parse_errors), parse_errors.len, &why, "errors");
            result(name, ok, &why);
            buf_free(&why);
        }
        else schemas_one_test(filename, instance, err, schema, buf_get(&parse_errors));
    }
    globfree(&g);
    if (schema) xml_xsd10_free(schema);
    buf_free(&parse_errors);
}

static void schemas_xsi_test(const char *filename)
{
    const char *base = base_filename(filename);
    char err[600];
    int len = (int)strlen(base);
    if (len > 499 || len < 5) return;
    snprintf(err, sizeof(err), "result/schemas/%.*s_0.err", len - 4, base);
    schemas_one_test(filename, filename, err, NULL, "");
}

// Patterns (patternTest) /////////////////////////////////////////

/** The element branch of xmlGetNodePath. */
static void node_path(Buf *b, XMQNode *node)
{
    if (!node || xml_node_kind(node) == XML_DOM_DOCUMENT) return;
    node_path(b, xml_node_parent(node));
    int generic = 0;
    buf_str(b, "/");
    XmlNs *ns = xml_node_ns(node);
    if (ns)
    {
        if (xml_ns_prefix(ns))
        {
            buf_str(b, xml_ns_prefix(ns));
            buf_str(b, ":");
            buf_str(b, xml_node_name(node));
        }
        else
        {
            generic = 1;
            buf_str(b, "*");
        }
    }
    else buf_str(b, xml_node_name(node));
    int occur = 0;
#define SAME_NAME(tmp) (xml_node_kind(tmp) == XML_DOM_ELEMENT && (generic || \
        (!strcmp(xml_node_name(node), xml_node_name(tmp)) && \
         ((xml_node_ns(tmp) == ns) || (xml_node_ns(tmp) && ns && \
           ((xml_ns_prefix(xml_node_ns(tmp)) == NULL && xml_ns_prefix(ns) == NULL) || \
            (xml_ns_prefix(xml_node_ns(tmp)) && xml_ns_prefix(ns) && !strcmp(xml_ns_prefix(xml_node_ns(tmp)), xml_ns_prefix(ns)))))))))
    for (XMQNode *tmp = xml_node_prev(node); tmp; tmp = xml_node_prev(tmp)) if (SAME_NAME(tmp)) occur++;
    if (occur == 0)
    {
        for (XMQNode *tmp = xml_node_next(node); tmp && occur == 0; tmp = xml_node_next(tmp)) if (SAME_NAME(tmp)) occur++;
        if (occur != 0) occur = 1;
    }
    else occur++;
#undef SAME_NAME
    if (occur) buf_printf(b, "[%d]", occur);
}

/** The walk of the xmlReaderWalker loop of patternTest with patternNode for every element. */
static void pattern_walk(Buf *out, const char *pattern, XmlPattern *pc, XmlStreamCtxt **ps, XMQNode *node)
{
    for (; node; node = xml_node_next(node))
    {
        if (xml_node_kind(node) != XML_DOM_ELEMENT) continue;
        int match = xml_pattern_match(pc, node);
        Buf path = {0};
        if (match)
        {
            node_path(&path, node);
            buf_printf(out, "Node %s matches pattern %s\n", buf_get(&path), pattern);
        }
        if (*ps)
        {
            int ret = xml_stream_push(*ps, xml_node_name(node), xml_node_ns_uri(node));
            if (ret < 0)
            {
                buf_str(out, "xmlStreamPush() failure\n");
                xml_stream_ctxt_free(*ps);
                *ps = NULL;
            }
            else if (ret != match)
            {
                if (!path.s) node_path(&path, node);
                buf_str(out, "xmlPatternMatch and xmlStreamPush disagree\n");
                buf_printf(out, "  pattern %s node %s\n", pattern, buf_get(&path));
            }
        }
        buf_free(&path);
        pattern_walk(out, pattern, pc, ps, xml_node_first_child(node));
        if (*ps && xml_stream_pop(*ps) < 0)
        {
            buf_str(out, "xmlStreamPop() failure\n");
            xml_stream_ctxt_free(*ps);
            *ps = NULL;
        }
    }
}

static void pattern_test(const char *filename)
{
    char xml[600], result_file[700];
    int len = (int)strlen(filename) - 4;
    snprintf(xml, sizeof(xml), "%.*s.xml", len, filename);
    snprintf(result_file, sizeof(result_file), "result/pattern/%.*s", (int)strlen(base_filename(xml)) - 4, base_filename(xml));
    Buf why = {0};
    Buf out = {0};
    bool ok = true;
    FILE *f = fopen(filename, "rb");
    if (!f) return;
    char str[1024];
    while (fgets(str, sizeof(str) - 1, f) != NULL)
    {
        int i = (int)strlen(str);
        while ((i > 0) && ((str[i - 1] == '\n') || (str[i - 1] == '\r') || (str[i - 1] == ' ') || (str[i - 1] == '\t')))
        {
            i--;
            str[i] = 0;
        }
        XmlDom *doc = parse_xml_file(xml, NULL, false, false);
        if (!doc)
        {
            buf_printf(&why, "  failed to parse %s\n", xml);
            ok = false;
            continue;
        }
        XMQNode *root = xml_dom_root_element(doc);
        const char *namespaces[22];
        int j = 0;
        for (XmlNs *ns = root ? xml_node_first_ns_decl(root) : NULL; ns && j < 20; ns = xml_ns_next_decl(ns))
        {
            namespaces[j++] = xml_ns_href(ns);
            namespaces[j++] = xml_ns_prefix(ns);
        }
        namespaces[j++] = NULL;
        namespaces[j] = NULL;
        XmlPattern *pc = xml_pattern_compile(str, 0, namespaces);
        if (!pc)
        {
            // runtest reports this on its error channel, which is not compared.
            xml_dom_free(doc);
            continue;
        }
        XmlStreamCtxt *ps = xml_pattern_get_stream_ctxt(pc);
        if (ps && xml_stream_push(ps, NULL, NULL) < 0)
        {
            xml_stream_ctxt_free(ps);
            ps = NULL;
        }
        pattern_walk(&out, str, pc, &ps, xml_node_first_child(xml_dom_document(doc)));
        if (ps) xml_stream_ctxt_free(ps);
        xml_pattern_free(pc);
        xml_dom_free(doc);
    }
    fclose(f);
    if (!check_file(result_file, buf_get(&out), out.len, &why, "output")) ok = false;
    result(filename, ok, &why);
    buf_free(&why);
    buf_free(&out);
}

// Regular expressions and automata (regexpTest, automataTest) ////

static void regexp_error(void *data, const char *message, const char *regexp, int index)
{
    (void)regexp; (void)index;
    // xmlFormatError of a regexp error without file and node.
    buf_printf((Buf*)data, "regexp error : %s", message);
}

static void regexp_test(const char *filename, const char *result_file, const char *err_file)
{
    FILE *input = fopen(filename, "rb");
    if (!input) return;
    Buf out = {0}, errors = {0}, why = {0};
    XmlRegexp *comp = NULL;
    char expression[5000];
    while (fgets(expression, 4500, input) != NULL)
    {
        int len = (int)strlen(expression);
        len--;
        while ((len >= 0) &&
               ((expression[len] == '\n') || (expression[len] == '\t') ||
                (expression[len] == '\r') || (expression[len] == ' '))) len--;
        expression[len + 1] = 0;
        if (len < 0) continue;
        if (expression[0] == '#') continue;
        if ((expression[0] == '=') && (expression[1] == '>'))
        {
            char *pattern = &expression[2];
            if (comp)
            {
                xml_regexp_free(comp);
                comp = NULL;
            }
            buf_printf(&out, "Regexp: %s\n", pattern);
            comp = xml_regexp_compile_with_errors(pattern, regexp_error, &errors);
            if (!comp)
            {
                buf_str(&out, "   failed to compile\n");
                break;
            }
        }
        else if (!comp)
        {
            buf_printf(&out, "Regexp: %s\n", expression);
            comp = xml_regexp_compile_with_errors(expression, regexp_error, &errors);
            if (!comp)
            {
                buf_str(&out, "   failed to compile\n");
                break;
            }
        }
        else
        {
            int ret = xml_regexp_exec(comp, expression);
            if (ret == 1) buf_printf(&out, "%s: Ok\n", expression);
            else if (ret == 0) buf_printf(&out, "%s: Fail\n", expression);
            else buf_printf(&out, "%s: Error: %d\n", expression, ret);
        }
    }
    fclose(input);
    if (comp) xml_regexp_free(comp);
    bool ok = check_file(result_file, buf_get(&out), out.len, &why, "output");
    if (!check_file(err_file, buf_get(&errors), errors.len, &why, "errors")) ok = false;
    result(filename, ok, &why);
    buf_free(&out);
    buf_free(&errors);
    buf_free(&why);
}

static int scan_number(char **ptr)
{
    int ret = 0;
    char *cur = *ptr;
    while ((*cur >= '0') && (*cur <= '9'))
    {
        ret = ret * 10 + (*cur - '0');
        cur++;
    }
    *ptr = cur;
    return ret;
}

static void automata_test(const char *filename, const char *result_file)
{
    FILE *input = fopen(filename, "rb");
    if (!input) return;
    Buf out = {0}, why = {0};
    char expr[5000];
    XmlAutomataState *states[1000];
    XmlRegexp *regexp = NULL;
    XmlRegExecCtxt *exec = NULL;
    bool ok = true;
    memset(states, 0, sizeof(states));
    XmlAutomata *am = xml_automata_new();
    states[0] = xml_automata_get_init_state(am);
    int ret = 0;
    while (fgets(expr, 4500, input) != NULL)
    {
        if (expr[0] == '#') continue;
        int len = (int)strlen(expr);
        len--;
        while ((len >= 0) &&
               ((expr[len] == '\n') || (expr[len] == '\t') ||
                (expr[len] == '\r') || (expr[len] == ' '))) len--;
        expr[len + 1] = 0;
        if (len < 0) continue;
        if (am && (expr[0] == 't') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            if (!states[from]) states[from] = xml_automata_new_state(am);
            ptr++;
            int to = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            if (!states[to]) states[to] = xml_automata_new_state(am);
            ptr++;
            xml_automata_new_transition(am, states[from], states[to], ptr, NULL);
        }
        else if (am && (expr[0] == 'e') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            if (!states[from]) states[from] = xml_automata_new_state(am);
            ptr++;
            int to = scan_number(&ptr);
            if (!states[to]) states[to] = xml_automata_new_state(am);
            xml_automata_new_epsilon(am, states[from], states[to]);
        }
        else if (am && (expr[0] == 'f') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int state = scan_number(&ptr);
            if (!states[state]) { buf_printf(&why, "  Bad state %d : %s\n", state, expr); ok = false; break; }
            xml_automata_set_final_state(am, states[state]);
        }
        else if (am && (expr[0] == 'c') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            if (!states[from]) states[from] = xml_automata_new_state(am);
            ptr++;
            int to = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            if (!states[to]) states[to] = xml_automata_new_state(am);
            ptr++;
            int min = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            ptr++;
            int max = scan_number(&ptr);
            if (*ptr != ' ') { buf_printf(&why, "  Bad line %s\n", expr); ok = false; break; }
            ptr++;
            xml_automata_new_count_trans(am, states[from], states[to], ptr, min, max, NULL);
        }
        else if (am && (expr[0] == '-') && (expr[1] == '-'))
        {
            // End of the automata.
            regexp = xml_automata_compile(am);
            xml_automata_free(am);
            am = NULL;
            if (!regexp) { buf_str(&why, "  Failed to compile the automata\n"); ok = false; break; }
        }
        else if ((expr[0] == '=') && (expr[1] == '>'))
        {
            if (!regexp)
            {
                buf_str(&out, "=> failed not compiled\n");
            }
            else
            {
                if (!exec) exec = xml_reg_exec_new(regexp, NULL, NULL);
                if (ret == 0) ret = xml_reg_exec_push_string(exec, NULL, NULL);
                if (ret == 1) buf_str(&out, "=> Passed\n");
                else if ((ret == 0) || (ret == -1)) buf_str(&out, "=> Failed\n");
                else if (ret < 0) buf_str(&out, "=> Error\n");
                xml_reg_exec_free(exec);
                exec = NULL;
            }
            ret = 0;
        }
        else if (regexp)
        {
            if (!exec) exec = xml_reg_exec_new(regexp, NULL, NULL);
            ret = xml_reg_exec_push_string(exec, expr, NULL);
        }
    }
    fclose(input);
    if (regexp) xml_regexp_free(regexp);
    if (exec) xml_reg_exec_free(exec);
    if (am) xml_automata_free(am);
    if (!check_file(result_file, buf_get(&out), out.len, &why, "output")) ok = false;
    result(filename, ok, &why);
    buf_free(&out);
    buf_free(&why);
}

// Driver /////////////////////////////////////////////////////////

static char *result_filename(const char *filename, const char *out, const char *suffix)
{
    char res[1200];
    snprintf(res, sizeof(res), "%s%s%s", out, base_filename(filename), suffix);
    return strdup(res);
}

static void run_category(const char *name)
{
    cat_ = NULL;
    for (size_t i = 0; i < NUM_CATEGORIES; i++) if (!strcmp(categories_[i].name, name)) cat_ = &categories_[i];
    if (!cat_) return;
    glob_t g;
    if (!strcmp(name, "xml") || !strcmp(name, "noent")) g = glob_files("./test/*");
    else if (!strcmp(name, "namespaces")) g = glob_files("./test/namespaces/*");
    else if (!strcmp(name, "errors") || !strcmp(name, "errors-noent")) g = glob_files("./test/errors/*.xml");
    else if (!strcmp(name, "html")) g = glob_files("./test/HTML/*");
    else if (!strcmp(name, "xpath-expr")) g = glob_files("./test/XPath/expr/*");
    else if (!strcmp(name, "xpath-doc")) g = glob_files("./test/XPath/docs/*");
    else if (!strcmp(name, "uri-parse")) g = glob_files("./test/URI/*.uri");
    else if (!strcmp(name, "uri-base")) g = glob_files("./test/URI/*.data");
    else if (!strcmp(name, "schemas")) g = glob_files("./test/schemas/*_*.xsd");
    else if (!strcmp(name, "pattern")) g = glob_files("./test/pattern/*.pat");
    else if (!strcmp(name, "regexp")) g = glob_files("./test/regexp/*");
    else g = glob_files("./test/automata/*");
    for (size_t i = 0; i < g.gl_pathc; i++)
    {
        const char *f = g.gl_pathv[i];
        if (!is_regular_file(f)) continue;
        char *res = NULL, *err = NULL;
        if (!strcmp(name, "xml"))
        {
            res = result_filename(f, "result/", "");
            old_parse_test(f, res);
        }
        else if (!strcmp(name, "noent"))
        {
            res = result_filename(f, "result/noent/", "");
            noent_parse_test(f, res);
        }
        else if (!strcmp(name, "namespaces"))
        {
            res = result_filename(f, "result/namespaces/", "");
            err = result_filename(f, "result/namespaces/", ".err");
            err_parse_test(f, res, err, false, false);
        }
        else if (!strcmp(name, "errors"))
        {
            res = result_filename(f, "result/errors/", "");
            err = result_filename(f, "result/errors/", ".err");
            err_parse_test(f, res, err, false, false);
        }
        else if (!strcmp(name, "errors-noent"))
        {
            err = result_filename(f, "result/errors/", ".ent");
            err_parse_test(f, NULL, err, false, true);
        }
        else if (!strcmp(name, "html"))
        {
            res = result_filename(f, "result/HTML/", "");
            err = result_filename(f, "result/HTML/", ".err");
            err_parse_test(f, res, err, true, false);
        }
        else if (!strcmp(name, "xpath-expr"))
        {
            res = result_filename(f, "result/XPath/expr/", "");
            xpath_common_test(f, res, NULL);
        }
        else if (!strcmp(name, "xpath-doc")) xpath_doc_test(f);
        else if (!strcmp(name, "uri-parse"))
        {
            res = result_filename(f, "result/URI/", "");
            uri_common_test(f, res, NULL);
        }
        else if (!strcmp(name, "uri-base"))
        {
            res = result_filename(f, "result/URI/", "");
            uri_common_test(f, res, "http://foo.com/path/to/index.html?orig#help");
        }
        else if (!strcmp(name, "schemas")) schemas_test(f);
        else if (!strcmp(name, "pattern")) pattern_test(f);
        else if (!strcmp(name, "regexp"))
        {
            res = result_filename(f, "result/regexp/", "");
            err = result_filename(f, "result/regexp/", ".err");
            regexp_test(f, res, err);
        }
        else
        {
            res = result_filename(f, "result/automata/", "");
            automata_test(f, res);
        }
        free(res);
        free(err);
    }
    globfree(&g);
    if (!strcmp(name, "schemas") && is_regular_file("./test/schemas/any1_0.xml")) schemas_xsi_test("./test/schemas/any1_0.xml");
}

static void load_known(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot read %s\n", file);
        exit(2);
    }
    char line[2048];
    while (fgets(line, sizeof(line), f))
    {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l - 1] == '\n' || s[l - 1] == ' ' || s[l - 1] == '\t' || s[l - 1] == '\r')) s[--l] = 0;
        if (!l) continue;
        known_ = (char**)realloc(known_, (num_known_ + 1) * sizeof(char*));
        known_seen_ = (int*)realloc(known_seen_, (num_known_ + 1) * sizeof(int));
        known_seen_[num_known_] = 0;
        known_[num_known_++] = strdup(s);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    // The summary must be written before the sanitizer's leak check ends the program.
    setvbuf(stdout, NULL, _IOLBF, 0);
    const char *dir = "tests/libxml2";
    const char *known_file = NULL;
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++)
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-k") && i + 1 < argc) known_file = argv[++i];
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) dir = argv[++i];
        else
        {
            fprintf(stderr, "Usage: runtest [-v] [-k known_failures.txt] [-d libxml2_test_dir] [category...]\n");
            return 2;
        }
    }
    if (known_file) load_known(known_file);
    if (chdir(dir) != 0)
    {
        fprintf(stderr, "Cannot change into %s\n", dir);
        return 2;
    }
    int first = i;
    bool all = first >= argc;
    for (size_t c = 0; c < NUM_CATEGORIES; c++)
    {
        bool run = all;
        for (int k = first; k < argc; k++) if (!strcmp(argv[k], categories_[c].name)) run = true;
        if (run) run_category(categories_[c].name);
    }
    long passed = 0, failed = 0, known = 0, known_passed = 0;
    for (size_t c = 0; c < NUM_CATEGORIES; c++)
    {
        Category *k = &categories_[c];
        if (!k->passed && !k->failed && !k->known) continue;
        printf("libxml2 suite %-13s %4ld passed, %3ld failed, %3ld known failures%s\n", k->name,
               k->passed - k->known_passed, k->failed, k->known,
               k->known_passed ? " (some known failures pass, remove them from the list)" : "");
        passed += k->passed - k->known_passed;
        failed += k->failed;
        known += k->known;
        known_passed += k->known_passed;
    }
    // Known failures of the categories that ran that did not run at all (renamed or removed tests).
    long stale = 0;
    for (size_t k = 0; k < num_known_; k++)
    {
        if (known_seen_[k]) continue;
        bool ran = all;
        for (int a = first; a < argc; a++)
        {
            size_t l = strlen(argv[a]);
            if (!strncmp(known_[k], argv[a], l) && known_[k][l] == ':') ran = true;
        }
        if (ran)
        {
            printf("KNOWN FAILURE NOT RUN %s\n", known_[k]);
            stale++;
        }
    }
    printf("libxml2 suite total         %4ld passed, %3ld failed, %3ld known failures, %ld known failures pass, %ld not run\n",
           passed, failed, known, known_passed, stale);
    for (size_t k = 0; k < num_known_; k++) free(known_[k]);
    free(known_);
    free(known_seen_);
    return (failed || known_passed || stale) ? 1 : 0;
}
