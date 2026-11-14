/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native html parser and serializer (parts/xml_html_parser.c,
   parts/xml_html_save.c) against libxml2.

   For every file given on the command line, with and without HTML_PARSE_NOBLANKS, parse
   the bytes with libxml2 htmlReadMemory (like libxmq: UTF-8, NOERROR|NOWARNING|NONET) and
   with xml_html_parse_buffer, then compare:
   - the html output (htmlDocContentDumpOutput, which libxmq uses, vs xml_html_save_doc),
   - the xml output of the same tree (xmlDocDumpMemoryEnc vs xml_save_doc), since libxmq
     can write html input as xml.

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_html_parser.h"
#include"parts/xml_html_save.h"
#include"parts/xml_save.h"

#include<libxml/HTMLparser.h>
#include<libxml/HTMLtree.h>
#include<libxml/tree.h>
#include<libxml/xmlIO.h>

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
static int same_ = 0, diff_ = 0, expected_ = 0;

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

static void compare(const char *what, const char *file, const char *expected, size_t elen, const char *got, size_t glen)
{
    if (elen == glen && !memcmp(expected, got, glen))
    {
        same_++;
        if (verbose_) printf("OK %s %s\n", what, file);
    }
    else if (is_expected(file))
    {
        expected_++;
    }
    else
    {
        diff_++;
        show_diff(what, file, expected, got);
    }
}

static void test_file(const char *file, const char *buf, size_t len, int noblanks)
{
    int options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    if (noblanks) options |= HTML_PARSE_NOBLANKS;
    htmlDocPtr doc = htmlReadMemory(buf, (int)len, NULL, "UTF-8", options);

    XmlHtmlParseSettings settings;
    xml_html_parse_settings_default(&settings);
    settings.no_blanks = noblanks;
    XmlDom *dom = xml_html_parse_buffer(buf, buf + len, &settings);

    if (!doc)
    {
        printf("LIBXML2 FAILED %s\n", file);
        diff_++;
        xml_dom_free(dom);
        return;
    }

    // Html output, like libxmq xmq_print_html.
    xmlOutputBufferPtr out = xmlAllocOutputBuffer(NULL);
    htmlDocContentDumpOutput(out, doc, "utf8");
    const char *expected = (const char*)xmlBufferContent((xmlBuffer*)out->buffer);
    size_t got_len = 0;
    char *got = xml_html_save_doc(dom, true, &got_len);
    compare(noblanks ? "html-noblanks" : "html", file, expected, strlen(expected), got, got_len);
    free(got);
    xmlOutputBufferClose(out);

    // Xml output of the html tree, like libxmq xmq_print_xml.
    xmlChar *x = NULL;
    int xl = 0;
    xmlDocDumpMemoryEnc(doc, &x, &xl, "utf-8");
    got = xml_save_doc(dom, "utf-8", false, &got_len);
    compare(noblanks ? "xml-noblanks" : "xml", file, (const char*)x, (size_t)xl, got, got_len);
    free(got);
    xmlFree(x);

    xmlFreeDoc(doc);
    xml_dom_free(dom);
}

/**
    Run the html5lib tokenizer tests in libxml2 test/html-tokenizer/ (generated by
    codegen/genHtml5LibTests.py). Each record is "<id> <last start tag> <state> <length>\n"
    followed by length bytes of input. Each input is parsed as a document; for the raw text
    states the input is put inside an element that starts that state.
*/
static int run_tokenizer_tests(const char *file)
{
    size_t len = 0;
    char *buf = read_file(file, &len);
    if (!buf) return 0;
    int n = 0;
    size_t i = 0;
    while (i < len)
    {
        char *nl = memchr(buf + i, '\n', len - i);
        if (!nl) break;
        char tag[64] = "";
        int id = 0, state = 0;
        size_t dlen = 0;
        *nl = 0;
        if (sscanf(buf + i, "%d %63s %d %zu", &id, tag, &state, &dlen) != 4) break;
        i = (nl - buf) + 1;
        if (i + dlen > len) break;
        const char *start_tag = strcmp(tag, "-") ? tag : NULL;
        if (!start_tag)
        {
            if (state == 1) start_tag = "textarea";
            else if (state == 2) start_tag = "xmp";
            else if (state == 3) start_tag = "plaintext";
            else if (state == 4) start_tag = "script";
        }
        size_t prefix = start_tag ? strlen(start_tag) + 2 : 0;
        char *doc = (char*)malloc(prefix + dlen + 1);
        if (start_tag) sprintf(doc, "<%s>", start_tag);
        memcpy(doc + prefix, buf + i, dlen);
        doc[prefix + dlen] = 0;
        char name[1100];
        snprintf(name, sizeof(name), "%s#%d", file, id);
        test_file(name, doc, prefix + dlen, 0);
        test_file(name, doc, prefix + dlen, 1);
        free(doc);
        i += dlen + 1;
        n++;
    }
    free(buf);
    return n;
}

// Differential fuzzing: random malformed html from a seeded generator (deterministic).
static unsigned long long fuzz_state_;
static unsigned fuzz_rand(unsigned n)
{
    fuzz_state_ = fuzz_state_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)((fuzz_state_ >> 33) % n);
}

static void fuzz_add(char **b, size_t *len, size_t *cap, const char *s)
{
    size_t n = strlen(s);
    if (*len + n + 1 > *cap)
    {
        *cap = (*len + n + 1) * 2;
        *b = (char*)realloc(*b, *cap);
    }
    memcpy(*b + *len, s, n + 1);
    *len += n;
}

static const char *fuzz_tags_[] = {
    "html", "head", "body", "title", "p", "div", "span", "b", "i", "a", "br", "hr", "img", "table", "tr", "td",
    "th", "tbody", "thead", "ul", "ol", "li", "dl", "dt", "dd", "script", "style", "textarea", "xmp", "pre",
    "form", "input", "select", "option", "meta", "link", "frameset", "frame", "noscript", "font", "center",
    "h1", "plaintext", "iframe", "noembed", "noframes", "object", "param", "button", "label", "foo", "x:y", "P", "DiV"
};
static const char *fuzz_text_[] = {
    "text", " ", "  ", "\n", "\r\n", "\r", "\t", "&amp;", "&lt;", "&nbsp;", "&copy", "&copy;", "&notin;", "&noti",
    "&#65;", "&#x41;", "&#0;", "&#128;", "&#x110000;", "&#xD800;", "&#65", "&bogus;", "&", "<", ">", "]]>", "--",
    "\xc3\xa5", "\xe2\x82\xac", "\xff", "\xc3", "\xed\xa0\x80", "<!-- c -->", "<!-->", "<!--->", "<!---->",
    "<!-- a --!>", "<!-- unclosed", "<?pi x?>", "<!bogus>", "</>", "</ >", "</3>", "<!DOCTYPE html>",
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">",
    "<![CDATA[x]]>", "<!-- <script> -->", "</script>", "<!--<script>", "\xef\xbb\xbf"
};
static const char *fuzz_attrs_[] = {
    " a=1", " a='x y'", " a=\"q\"", " checked", " selected=\"\"", " A=B", " a=1 a=2", " href=\"a b&amp;\xc3\xa5\"",
    " src=x", " name=n", " x:y=z", " =x", " a=&copy;=", " a=&copyx", " a=&lt", " /", " a", " b=\"&#x41;\"", " c='\r\n'"
};

static void fuzz_doc(char **b, size_t *len, size_t *cap, int depth)
{
    unsigned n = 1 + fuzz_rand(8);
    for (unsigned k = 0; k < n; ++k)
    {
        unsigned what = fuzz_rand(10);
        if (what < 4)
        {
            fuzz_add(b, len, cap, fuzz_text_[fuzz_rand(sizeof(fuzz_text_)/sizeof(fuzz_text_[0]))]);
        }
        else if (what < 9)
        {
            const char *tag = fuzz_tags_[fuzz_rand(sizeof(fuzz_tags_)/sizeof(fuzz_tags_[0]))];
            fuzz_add(b, len, cap, "<");
            fuzz_add(b, len, cap, tag);
            unsigned na = fuzz_rand(3);
            for (unsigned a = 0; a < na; ++a) fuzz_add(b, len, cap, fuzz_attrs_[fuzz_rand(sizeof(fuzz_attrs_)/sizeof(fuzz_attrs_[0]))]);
            fuzz_add(b, len, cap, fuzz_rand(6) ? ">" : "/>");
            if (depth < 6) fuzz_doc(b, len, cap, depth + 1);
            if (fuzz_rand(4))
            {
                fuzz_add(b, len, cap, "</");
                fuzz_add(b, len, cap, fuzz_rand(5) ? tag : fuzz_tags_[fuzz_rand(sizeof(fuzz_tags_)/sizeof(fuzz_tags_[0]))]);
                fuzz_add(b, len, cap, fuzz_rand(10) ? ">" : " x=1>");
            }
        }
        else
        {
            fuzz_add(b, len, cap, fuzz_rand(2) ? "\x01" : "<"); // \x01 marks a NUL byte, see run_fuzz.
        }
    }
}

static int run_fuzz(const char *spec)
{
    unsigned count = 0;
    unsigned long long seed = 1;
    sscanf(spec, "%u:%llu", &count, &seed);
    for (unsigned t = 0; t < count; ++t)
    {
        fuzz_state_ = seed * 1000003ULL + t;
        char *b = NULL;
        size_t len = 0, cap = 0;
        fuzz_add(&b, &len, &cap, "");
        fuzz_doc(&b, &len, &cap, 0);
        // The generator cannot add NUL bytes through C strings, it adds \x01 markers instead.
        for (size_t k = 0; k < len; ++k) if (b[k] == 0x01) b[k] = 0;
        char name[64];
        snprintf(name, sizeof(name), "fuzz#%u:%llu", t, seed);
        test_file(name, b, len, 0);
        test_file(name, b, len, 1);
        if (diff_ && verbose_ == 0)
        {
            printf("FUZZ INPUT %s (%zu bytes): ", name, len);
            fwrite(b, 1, len, stdout);
            printf("\n");
            free(b);
            return (int)t + 1;
        }
        free(b);
    }
    return (int)count;
}

int main(int argc, char **argv)
{
    int files = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-v")) { verbose_++; continue; }
        if (!strcmp(argv[i], "-x") && i + 1 < argc) { load_expected(argv[++i]); continue; }
        if (!strcmp(argv[i], "-t") && i + 1 < argc) { files += run_tokenizer_tests(argv[++i]); continue; }
        if (!strcmp(argv[i], "-f") && i + 1 < argc) { files += run_fuzz(argv[++i]); if (diff_) break; continue; }
        size_t len = 0;
        char *buf = read_file(argv[i], &len);
        if (!buf) continue;
        test_file(argv[i], buf, len, 0);
        test_file(argv[i], buf, len, 1);
        free(buf);
        files++;
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
    printf("xml_html_diff: %d files, %d same, %d differ, %d expected differences\n", files, same_, diff_, expected_);
    return (diff_ || stale) ? 1 : 0;
}
