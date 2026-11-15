/*
   Run the native xslt (parts/xml_libxslt1_*.c) over the libxslt test suite and
   compare with the expected results, like libxslt's tests/runtest.c does.

   For every name.xsl in a test directory that has a name.xml, the stylesheet is
   compiled and applied (from inside the directory, with the parameters test and
   test2) and the serialized result and the messages are compared with name.out
   and name.err. A missing file means empty. Standalone stylesheets (an
   xml-stylesheet processing instruction, .stand.out) are not supported and are
   not run.

   Usage: xml_libxslt1_runtest [-v] [-k known_failures.txt] dir_or_dir/name.xsl...
   Tests listed (as dir/name.xsl) in the known failures file may fail, the exit
   status is non zero if any other test fails or a known failure passes.
*/

#include"parts/always.h"
#include"parts/membuffer.h"
#include"parts/xml_dom.h"
#include"parts/xml_libxslt1_xslt.h"
#include"parts/xml_libxslt1_transform.h"

#include<dirent.h>
#include<stdarg.h>
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

static int verbose_;
static MemBuffer *msgs_;
static char **known_;
static size_t num_known_;
static long passed_, failed_, known_failed_, known_passed_, out_failed_, err_failed_;

static void capture(void *ud, const char *msg)
{
    (void)ud;
    membuffer_append(msgs_, msg);
}

static char *read_all(const char *path, size_t *len)
{
    *len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    MemBuffer *mb = new_membuffer();
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) membuffer_append_region(mb, buf, buf + n);
    fclose(f);
    *len = mb->used_;
    membuffer_append_null(mb);
    return free_membuffer_but_return_trimmed_content(mb);
}

static bool same_as_file(const char *path, const char *mem, size_t len, size_t *at)
{
    size_t flen;
    char *expected = read_all(path, &flen);
    bool same = flen == len && (!len || !memcmp(expected, mem, len));
    *at = 0;
    if (!same)
    {
        while (*at < flen && *at < len && expected[*at] == mem[*at]) (*at)++;
    }
    free(expected);
    return same;
}

static bool is_known(const char *test)
{
    for (size_t i = 0; i < num_known_; i++) if (!strcmp(known_[i], test)) return true;
    return false;
}

static void show(const char *label, const char *s, size_t len)
{
    printf("  ---- %s\n", label);
    fwrite(s, 1, len > 2000 ? 2000 : len, stdout);
    if (len && s[len - 1] != '\n') putchar('\n');
}

static void run_test(const char *dir, const char *name)
{
    size_t nl = strlen(name);
    char *base = (char*)malloc(nl + 3);
    snprintf(base, nl + 3, "./%.*s", (int)(nl - 4), name);
    char path[1024], input[1024], out_file[1024], err_file[1024], test[1024];
    snprintf(path, sizeof(path), "%s.xsl", base);
    snprintf(input, sizeof(input), "%s.xml", base);
    snprintf(out_file, sizeof(out_file), "%s.out", base);
    snprintf(err_file, sizeof(err_file), "%s.err", base);
    snprintf(test, sizeof(test), "%s/%s", dir, name);
    free(base);
    if (access(input, R_OK) != 0) return;

    msgs_ = new_membuffer();
    XmlLibxslt1Settings settings;
    xml_libxslt1_settings_default(&settings);
    settings.message_callback = capture;
    char *out = NULL;
    size_t out_len = 0;
    XmlDom *style_doc = xml_libxslt1_default_loader(&settings, path, XML_LIBXSLT1_LOAD_START);
    XmlLibxslt1Stylesheet *style = style_doc ? xml_libxslt1_compile(style_doc, &settings) : NULL;
    // Like libxslt, the caller keeps the document when the compilation fails.
    if (style_doc && !style) xml_dom_free(style_doc);
    if (style)
    {
        XmlDom *doc = xml_libxslt1_default_loader(&settings, input, XML_LIBXSLT1_LOAD_DOCUMENT);
        const char *params[] = { "test", "'passed_value'", "test2", "'passed_value2'", NULL };
        int type = 0;
        XmlDom *res = doc ? xml_libxslt1_apply_stylesheet(style, doc, params, &type) : NULL;
        if (!res)
        {
            // xsltproc compat, like runtest.
            char line[1100];
            snprintf(line, sizeof(line), "no result for %s\n", input);
            membuffer_append(msgs_, line);
        }
        else
        {
            out = xml_libxslt1_save_result_to_string(res, style, type, &out_len);
            xml_dom_free(res);
        }
        if (doc) xml_dom_free(doc);
        xml_libxslt1_stylesheet_free(style);
    }
    size_t err_len = msgs_->used_;
    membuffer_append_null(msgs_);
    char *err = free_membuffer_but_return_trimmed_content(msgs_);

    size_t out_at, err_at;
    bool out_same = same_as_file(out_file, out ? out : "", out ? out_len : 0, &out_at);
    bool err_same = same_as_file(err_file, err, err_len, &err_at);
    bool known = is_known(test);
    if (out_same && err_same)
    {
        passed_++;
        if (known)
        {
            known_passed_++;
            printf("KNOWN FAILURE PASSES %s\n", test);
        }
        else if (verbose_) printf("OK %s\n", test);
    }
    else
    {
        if (!out_same) out_failed_++;
        if (!err_same) err_failed_++;
        if (known) known_failed_++;
        else failed_++;
        if (!known || verbose_)
        {
            printf("%s %s:%s%s\n", known ? "KNOWN" : "FAIL", test,
                   out_same ? "" : " output differs", err_same ? "" : " messages differ");
            if (!out_same)
            {
                size_t elen;
                char *expected = read_all(out_file, &elen);
                printf("  output differs at byte %zu\n", out_at);
                show("expected output", expected ? expected : "", elen);
                show("native output", out ? out : "", out ? out_len : 0);
                free(expected);
            }
            if (!err_same)
            {
                size_t elen;
                char *expected = read_all(err_file, &elen);
                show("expected messages", expected ? expected : "", elen);
                show("native messages", err, err_len);
                free(expected);
            }
        }
    }
    free(out);
    free(err);
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char*const*)a, *(char*const*)b);
}

static void run_dir(const char *arg)
{
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) return;
    // A single test: dir/name.xsl.
    char dir[4096];
    const char *only = NULL;
    snprintf(dir, sizeof(dir), "%s", arg);
    size_t al = strlen(dir);
    char *slash = strrchr(dir, '/');
    if (al > 4 && !strcmp(dir + al - 4, ".xsl") && slash)
    {
        *slash = 0;
        only = slash + 1 - dir + arg;
    }
    if (chdir(dir) != 0)
    {
        printf("SKIP %s (no such directory)\n", dir);
        return;
    }
    DIR *d = opendir(".");
    char **names = NULL;
    size_t n = 0;
    struct dirent *e;
    while (d && (e = readdir(d)))
    {
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcmp(e->d_name + l - 4, ".xsl") && (!only || !strcmp(only, e->d_name)))
        {
            names = (char**)realloc(names, (n + 1) * sizeof(char*));
            names[n++] = strdup(e->d_name);
        }
    }
    if (d) closedir(d);
    if (n) qsort(names, n, sizeof(char*), cmp_names);
    // The directory name as given, without the path in front, names the tests.
    const char *short_dir = strstr(dir, "/tests/");
    short_dir = short_dir ? short_dir + 7 : dir;
    for (size_t i = 0; i < n; i++)
    {
        run_test(short_dir, names[i]);
        free(names[i]);
    }
    free(names);
    if (chdir(cwd) != 0) {}
}

static void load_known(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot read %s\n", file);
        exit(2);
    }
    char line[1024];
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
        known_[num_known_++] = strdup(s);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    // The summary must be written before the sanitizer's leak check ends the program.
    setvbuf(stdout, NULL, _IOLBF, 0);
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++)
    {
        if (!strcmp(argv[i], "-v")) verbose_++;
        else if (!strcmp(argv[i], "-k") && i + 1 < argc) load_known(argv[++i]);
        else
        {
            fprintf(stderr, "Usage: xml_libxslt1_runtest [-v] [-k known_failures.txt] dir...\n");
            return 2;
        }
    }
    for (; i < argc; i++) run_dir(argv[i]);
    printf("xml_libxslt1_runtest: %ld passed, %ld failed, %ld known failures (%ld output, %ld messages differ), %ld known failures pass\n",
           passed_, failed_, known_failed_, out_failed_, err_failed_, known_passed_);
    for (size_t k = 0; k < num_known_; k++) free(known_[k]);
    free(known_);
    return (failed_ || known_passed_) ? 1 : 0;
}
