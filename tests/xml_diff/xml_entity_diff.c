/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native entity expansion (xml_expand_entities in
   parts/xml_parser.c) against libxml2.

   For every file given on the command line that declares entities, parse the bytes with
   libxml2 xmlReadMemory using XML_PARSE_NOENT (entities substituted while parsing) and
   with xml_parse_buffer followed by xml_expand_entities, then:
   - both must agree on whether the document is well formed and expandable,
   - the serialized trees (xmlDocDumpMemoryEnc vs xml_save_doc) must be identical.

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_save.h"

#include<libxml/tree.h>
#include<libxml/parser.h>

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
static int same_ = 0, diff_ = 0, both_fail_ = 0, native_fail_ = 0, libxml_fail_ = 0, expected_ = 0;

#define MAX_EXPECTED 256
static char *expected_files_[MAX_EXPECTED];
static int expected_hit_[MAX_EXPECTED];
static int num_expected_ = 0;

static void load_expected(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) { fprintf(stderr, "Cannot read %s\n", file); exit(2); }
    char line[1024];
    while (fgets(line, sizeof(line), f) && num_expected_ < MAX_EXPECTED)
    {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *sp = strpbrk(line, " \t\n");
        if (sp) *sp = 0;
        expected_files_[num_expected_++] = strdup(line);
    }
    fclose(f);
}

/** Is this file listed as an expected difference? */
static int is_expected(const char *file)
{
    size_t fl = strlen(file);
    for (int k = 0; k < num_expected_; ++k)
    {
        size_t el = strlen(expected_files_[k]);
        if (fl >= el && !strcmp(file + fl - el, expected_files_[k]))
        {
            expected_hit_[k] = 1;
            return 1;
        }
    }
    return 0;
}

static void ignore_message(void *user, const char *msg)
{
    (void)user;
    if (verbose_ > 1) fprintf(stderr, "%s", msg);
}

static void show_diff(const char *what, const char *file, const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] && a[i] == b[i]) i++;
    size_t from = i > 40 ? i-40 : 0;
    printf("DIFF %s %s at byte %zu\n  libxml2: \"", what, file, i);
    for (size_t k = from; a[k] && k < i + 40; ++k) putchar(a[k] == '\n' ? '|' : a[k]);
    printf("\"\n  native:  \"");
    for (size_t k = from; b[k] && k < i + 40; ++k) putchar(b[k] == '\n' ? '|' : b[k]);
    printf("\"\n");
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

static void test_file(const char *file, const char *buf, size_t len, int options, const char *mode)
{
    xmlDocPtr doc = xmlReadMemory(buf, (int)len, file, NULL, options | XML_PARSE_NOENT | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = file;
    settings.no_cdata = (options & XML_PARSE_NOCDATA) != 0;
    settings.no_blanks = (options & XML_PARSE_NOBLANKS) != 0;
    settings.message_callback = ignore_message;
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

    int differs = 1;
    char *got = NULL;
    size_t got_len = 0;
    xmlChar *expected = NULL;
    int expected_len = 0;
    if (doc && dom)
    {
        xmlDocDumpMemoryEnc(doc, &expected, &expected_len, "utf-8");
        got = xml_save_doc(dom, "utf-8", false, &got_len);
        differs = !((size_t)expected_len == got_len && !memcmp(expected, got, got_len));
    }

    if (!doc && !dom)
    {
        both_fail_++;
    }
    else if (is_expected(file))
    {
        // Compared anyway, an expected difference that is gone is reported below.
        if (differs) expected_++;
        else same_++;
    }
    else if (!doc)
    {
        libxml_fail_++;
        const xmlError *e = xmlGetLastError();
        printf("NATIVE ACCEPTS %s %s (libxml2: %s)", mode, file, e && e->message ? e->message : "?\n");
    }
    else if (!dom)
    {
        native_fail_++;
        printf("NATIVE REJECTS %s %s: %s\n", mode, file, err.short_message ? err.short_message : "?");
    }
    else if (!differs)
    {
        same_++;
        if (verbose_) printf("OK %s %s\n", mode, file);
    }
    else
    {
        diff_++;
        show_diff(mode, file, (const char*)expected, got);
    }
    if (expected) xmlFree(expected);
    free(got);
    if (doc) xmlFreeDoc(doc);
    if (dom) xml_dom_free(dom);
    xml_parse_error_free(&err);
}

int main(int argc, char **argv)
{
    // Line buffered, so that the progress can be followed in a log file.
    setvbuf(stdout, NULL, _IOLBF, 0);
    int skipped = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (i % 100 == 0) printf("entity [%d/%d] files\n", i, argc-1);
        if (!strcmp(argv[i], "-v")) { verbose_++; continue; }
        if (!strcmp(argv[i], "-x") && i + 1 < argc) { load_expected(argv[++i]); continue; }
        size_t len = 0;
        char *buf = read_file(argv[i], &len);
        if (!buf) continue;
        // Only documents that declare entities are interesting.
        if (!strstr(buf, "<!ENTITY"))
        {
            skipped++;
            free(buf);
            continue;
        }
        test_file(argv[i], buf, len, XML_PARSE_NOCDATA | XML_PARSE_NONET, "noent");
        free(buf);
    }
    xmlCleanupParser();
    int stale = 0;
    for (int k = 0; k < num_expected_; ++k)
    {
        if (!expected_hit_[k])
        {
            printf("EXPECTED DIFFERENCE NOT SEEN (remove from list?): %s\n", expected_files_[k]);
            stale++;
        }
        free(expected_files_[k]);
    }
    printf("xml_entity_diff: %d same, %d differ, %d both reject, %d only native rejects, %d only libxml2 rejects, %d expected differences (%d files without entities)\n",
           same_, diff_, both_fail_, native_fail_, libxml_fail_, expected_, skipped);
    return (diff_ || native_fail_ || libxml_fail_ || stale) ? 1 : 0;
}
