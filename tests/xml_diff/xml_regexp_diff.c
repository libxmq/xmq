/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native regular expressions and automata (parts/xml_regexp.c)
   against libxml2 xmlregexp.c.

   1. The libxml2 test suite: every file in test/regexp (driven like libxml2 runtest.c
      regexpTest) and test/automata (like automataTest) is run through both libraries.
      The outputs must be identical, and the native output must also match the expected
      libxml2 result files (result/regexp/NAME, result/regexp/NAME.err, result/automata/NAME).
      Compile errors (message, expression and index), determinism and match results
      are compared for every expression.
   2. Every Unicode category and block (\p{..} and [^\p{..}]) against the code points
      around all the range bounds of libxml2 codegen/unicode.inc (and all below 0x300).
   3. A generated corpus of regular expressions (structured ones built from classes, escapes,
      categories, blocks, quantifiers, groups and alternations, and random token soups that
      mostly exercise the error paths), each matched against generated strings.
   4. Automata built programmatically through the automata api (plain, negated, counted,
      once, all, epsilon and counter transitions) and driven with the push api. The push
      return values, the callbacks and the xmlRegExecNextValues / xmlRegExecErrInfo results
      are compared after every push. Where libxml2 itself crashes or loops forever (run first
      in a child process), the native port, which fixes these bugs, is only checked to
      finish with sane results.

   Usage: xml_regexp_diff [-v] [-n count:seed] [-a count:seed] libxml2_dir
     -n  number of generated regular expressions and seed (default 20000:1)
     -a  number of generated automata and seed (default 3000:1)

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

// The automata and exec api of libxml2 is deprecated (internal), it is what xmlschemas.c uses.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include"parts/always.h"
#include"parts/xml_regexp.h"

#include<libxml/xmlregexp.h>
#include<libxml/xmlautomata.h>
#include<libxml/xmlerror.h>
#include<libxml/xmlmemory.h>
#include<libxml/parser.h>

// From libxml2 private/regexp.h.
void xmlAutomataSetFlags(xmlAutomataPtr am, int flags);
void xmlRegExecClearErrors(xmlRegExecCtxtPtr exec);

#include<dirent.h>
#include<fcntl.h>
#include<signal.h>
#include<sys/wait.h>
#include<unistd.h>
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

static int verbose_ = 0;
static long diff_ = 0, max_shown_ = 40;
static long n_compiled_ = 0, n_compile_fail_ = 0, n_matches_ = 0, n_determ_ = 0, n_errors_ = 0;
static long n_files_ = 0, n_automata_ = 0, n_automata_compiled_ = 0, n_pushes_ = 0, n_callbacks_ = 0, n_values_ = 0;
static long n_unicode_ = 0;
static long n_suite_lines_ = 0, n_lx_crash_native_ok_ = 0, n_lx_hang_native_ok_ = 0;

/* Growable string. */
typedef struct
{
    char *s;
    size_t len, cap;
} Buf;

static void buf_add(Buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (b->len + n + 1 > b->cap)
    {
        b->cap = (b->len + n + 1) * 2;
        b->s = (char*)realloc(b->s, b->cap);
    }
    vsnprintf(b->s + b->len, n + 1, fmt, ap2);
    va_end(ap2);
    b->len += n;
}

static void buf_clear(Buf *b)
{
    b->len = 0;
    if (b->s) b->s[0] = 0;
}

static const char *buf_str(Buf *b)
{
    return b->s ? b->s : "";
}

static void report(const char *what, const char *where, const char *lx, const char *nx)
{
    diff_++;
    if (diff_ <= max_shown_)
    {
        printf("DIFF %s %s\n  libxml2: %s\n  native:  %s\n", what, where, lx, nx);
    }
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = {0};
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0)
    {
        if (b.len + n + 1 > b.cap) { b.cap = (b.len + n + 1) * 2; b.s = (char*)realloc(b.s, b.cap); }
        memcpy(b.s + b.len, tmp, n);
        b.len += n;
        b.s[b.len] = 0;
    }
    fclose(f);
    if (!b.s) b.s = strdup("");
    return b.s;
}

/* Errors ////////////////////////////////////////////////////////////////////// */

static Buf lx_errors_;   // libxml2 structured errors: message|str2|int1
static Buf nx_errors_;   // native errors
static Buf lx_err_text_; // libxml2 errors formatted like runtest.c testStructuredErrorHandler for regexps

static void lx_structured_error(void *data, const xmlError *err)
{
    (void)data;
    buf_add(&lx_errors_, "[%d %d %s|%s|%d]", err->domain, err->code, err->message ? err->message : "(null)",
            err->str2 ? err->str2 : "(null)", err->int1);
    buf_add(&lx_err_text_, "regexp error : %s", err->message ? err->message : "");
}

static void nx_error(void *data, const char *message, const char *regexp, int index)
{
    (void)data;
    buf_add(&nx_errors_, "[%d %d %s|%s|%d]", XML_FROM_REGEXP, XML_REGEXP_COMPILE_ERROR, message,
            regexp ? regexp : "(null)", index);
}

/* Regular expressions ///////////////////////////////////////////////////////// */

typedef struct
{
    xmlRegexpPtr lx;
    XmlRegexp *nx;
} RePair;

/* Compile with both, compare the errors. Returns 1 if both compiled. */
static int compile_pair(RePair *p, const char *pattern, const char *where)
{
    buf_clear(&lx_errors_);
    buf_clear(&nx_errors_);
    p->lx = xmlRegexpCompile((const xmlChar*)pattern);
    p->nx = xml_regexp_compile_with_errors(pattern, nx_error, NULL);
    if ((p->lx != NULL) != (p->nx != NULL))
    {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "compiled=%d", p->nx != NULL);
        report("compile", where, p->lx ? "compiled=1" : "compiled=0", tmp);
    }
    if (strcmp(buf_str(&lx_errors_), buf_str(&nx_errors_)))
    {
        report("compile errors", where, buf_str(&lx_errors_), buf_str(&nx_errors_));
    }
    else if (lx_errors_.len)
    {
        n_errors_++;
    }
    if (p->lx && p->nx)
    {
        n_compiled_++;
        return 1;
    }
    n_compile_fail_++;
    return 0;
}

static void free_pair(RePair *p)
{
    if (p->lx) xmlRegFreeRegexp(p->lx);
    if (p->nx) xml_regexp_free(p->nx);
    p->lx = NULL;
    p->nx = NULL;
}

static void compare_determinism(RePair *p, const char *where)
{
    int a = xmlRegexpIsDeterminist(p->lx);
    int b = xml_regexp_is_determinist(p->nx);
    n_determ_++;
    if (a != b)
    {
        char sa[32], sb[32];
        snprintf(sa, sizeof(sa), "%d", a);
        snprintf(sb, sizeof(sb), "%d", b);
        report("determinism", where, sa, sb);
    }
}

static int compare_exec(RePair *p, const char *value, const char *where)
{
    int a = xmlRegexpExec(p->lx, (const xmlChar*)value);
    int b = xml_regexp_exec(p->nx, value);
    n_matches_++;
    if (a != b)
    {
        char sa[32], sb[32], w[600];
        snprintf(sa, sizeof(sa), "%d", a);
        snprintf(sb, sizeof(sb), "%d", b);
        snprintf(w, sizeof(w), "%s value \"%.400s\"", where, value);
        report("exec", w, sa, sb);
    }
    return b;
}

/* The libxml2 test suite ////////////////////////////////////////////////////// */

static void out_exec(Buf *out, int ret, const char *value)
{
    if (ret == 1) buf_add(out, "%s: Ok\n", value);
    else if (ret == 0) buf_add(out, "%s: Fail\n", value);
    else buf_add(out, "%s: Error: %d\n", value, ret);
}

/* Like runtest.c regexpTest, but both libraries at once. */
static void regexp_suite_file(const char *path, const char *result, const char *result_err)
{
    FILE *input = fopen(path, "rb");
    if (!input) return;
    n_files_++;
    Buf out = {0};
    buf_clear(&lx_err_text_);
    RePair p = {0};
    int have = 0;
    char expression[5000];
    char where[6000];
    while (fgets(expression, 4500, input) != NULL)
    {
        int len = strlen(expression);
        len--;
        while ((len >= 0) &&
               ((expression[len] == '\n') || (expression[len] == '\t') ||
                (expression[len] == '\r') || (expression[len] == ' '))) len--;
        expression[len + 1] = 0;
        if (len < 0) continue;
        if (expression[0] == '#') continue;
        n_suite_lines_++;
        if ((expression[0] == '=') && (expression[1] == '>'))
        {
            char *pattern = &expression[2];
            free_pair(&p);
            have = 0;
            buf_add(&out, "Regexp: %s\n", pattern);
            snprintf(where, sizeof(where), "%s regexp \"%.1000s\"", path, pattern);
            have = compile_pair(&p, pattern, where);
            if (!p.nx)
            {
                buf_add(&out, "   failed to compile\n");
                break;
            }
            if (have) compare_determinism(&p, where);
        }
        else if (!p.nx)
        {
            buf_add(&out, "Regexp: %s\n", expression);
            snprintf(where, sizeof(where), "%s regexp \"%.1000s\"", path, expression);
            have = compile_pair(&p, expression, where);
            if (!p.nx)
            {
                buf_add(&out, "   failed to compile\n");
                break;
            }
            if (have) compare_determinism(&p, where);
        }
        else
        {
            int ret = have ? compare_exec(&p, expression, where) : xml_regexp_exec(p.nx, expression);
            out_exec(&out, ret, expression);
        }
    }
    fclose(input);
    free_pair(&p);

    char *expected = read_file(result);
    if (expected && strcmp(expected, buf_str(&out)))
    {
        report("suite result", path, expected, buf_str(&out));
    }
    free(expected);
    // The expected errors, libxml2 formats them as "regexp error : message".
    char *expected_err = read_file(result_err);
    const char *got = buf_str(&lx_err_text_);
    if (strcmp(expected_err ? expected_err : "", got))
    {
        report("suite errors (libxml2 itself)", path, expected_err ? expected_err : "", got);
    }
    free(expected_err);
    free(out.s);
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

/* Like runtest.c automataTest, run with one of the libraries. */
static void automata_suite_run(const char *path, int native, Buf *out)
{
    FILE *input = fopen(path, "rb");
    if (!input) return;
    char expr[5000];
    int len, ret = 0;
    xmlAutomataPtr am = NULL;
    XmlAutomata *nam = NULL;
    void *states[1000];
    xmlRegexpPtr regexp = NULL;
    XmlRegexp *nregexp = NULL;
    xmlRegExecCtxtPtr exec = NULL;
    XmlRegExecCtxt *nexec = NULL;
    int have_am, have_regexp;

    memset(states, 0, sizeof(states));
    if (native)
    {
        nam = xml_automata_new();
        states[0] = xml_automata_get_init_state(nam);
    }
    else
    {
        am = xmlNewAutomata();
        states[0] = xmlAutomataGetInitState(am);
    }
#define NEWSTATE() (native ? (void*)xml_automata_new_state(nam) : (void*)xmlAutomataNewState(am))
    while (fgets(expr, 4500, input) != NULL)
    {
        if (expr[0] == '#') continue;
        len = strlen(expr);
        len--;
        while ((len >= 0) &&
               ((expr[len] == '\n') || (expr[len] == '\t') ||
                (expr[len] == '\r') || (expr[len] == ' '))) len--;
        expr[len + 1] = 0;
        if (len < 0) continue;
        have_am = native ? nam != NULL : am != NULL;
        have_regexp = native ? nregexp != NULL : regexp != NULL;
        if (have_am && (expr[0] == 't') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') break;
            if (!states[from]) states[from] = NEWSTATE();
            ptr++;
            int to = scan_number(&ptr);
            if (*ptr != ' ') break;
            if (!states[to]) states[to] = NEWSTATE();
            ptr++;
            if (native) xml_automata_new_transition(nam, (XmlAutomataState*)states[from], (XmlAutomataState*)states[to], ptr, NULL);
            else xmlAutomataNewTransition(am, (xmlAutomataStatePtr)states[from], (xmlAutomataStatePtr)states[to], BAD_CAST ptr, NULL);
        }
        else if (have_am && (expr[0] == 'e') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') break;
            if (!states[from]) states[from] = NEWSTATE();
            ptr++;
            int to = scan_number(&ptr);
            if (!states[to]) states[to] = NEWSTATE();
            if (native) xml_automata_new_epsilon(nam, (XmlAutomataState*)states[from], (XmlAutomataState*)states[to]);
            else xmlAutomataNewEpsilon(am, (xmlAutomataStatePtr)states[from], (xmlAutomataStatePtr)states[to]);
        }
        else if (have_am && (expr[0] == 'f') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int state = scan_number(&ptr);
            if (!states[state]) break;
            if (native) xml_automata_set_final_state(nam, (XmlAutomataState*)states[state]);
            else xmlAutomataSetFinalState(am, (xmlAutomataStatePtr)states[state]);
        }
        else if (have_am && (expr[0] == 'c') && (expr[1] == ' '))
        {
            char *ptr = &expr[2];
            int from = scan_number(&ptr);
            if (*ptr != ' ') break;
            if (!states[from]) states[from] = NEWSTATE();
            ptr++;
            int to = scan_number(&ptr);
            if (*ptr != ' ') break;
            if (!states[to]) states[to] = NEWSTATE();
            ptr++;
            int min = scan_number(&ptr);
            if (*ptr != ' ') break;
            ptr++;
            int max = scan_number(&ptr);
            if (*ptr != ' ') break;
            ptr++;
            if (native) xml_automata_new_count_trans(nam, (XmlAutomataState*)states[from], (XmlAutomataState*)states[to], ptr, min, max, NULL);
            else xmlAutomataNewCountTrans(am, (xmlAutomataStatePtr)states[from], (xmlAutomataStatePtr)states[to], BAD_CAST ptr, min, max, NULL);
        }
        else if (have_am && (expr[0] == '-') && (expr[1] == '-'))
        {
            if (native)
            {
                nregexp = xml_automata_compile(nam);
                xml_automata_free(nam);
                nam = NULL;
                if (!nregexp) { buf_add(out, "Failed to compile the automata"); break; }
                buf_add(out, "[determinist %d]\n", xml_regexp_is_determinist(nregexp));
            }
            else
            {
                regexp = xmlAutomataCompile(am);
                xmlFreeAutomata(am);
                am = NULL;
                if (!regexp) { buf_add(out, "Failed to compile the automata"); break; }
                buf_add(out, "[determinist %d]\n", xmlRegexpIsDeterminist(regexp));
            }
        }
        else if ((expr[0] == '=') && (expr[1] == '>'))
        {
            if (!have_regexp)
            {
                buf_add(out, "=> failed not compiled\n");
            }
            else
            {
                if (native)
                {
                    if (!nexec) nexec = xml_reg_exec_new(nregexp, NULL, NULL);
                    if (ret == 0) ret = xml_reg_exec_push_string(nexec, NULL, NULL);
                }
                else
                {
                    if (!exec) exec = xmlRegNewExecCtxt(regexp, NULL, NULL);
                    if (ret == 0) ret = xmlRegExecPushString(exec, NULL, NULL);
                }
                if (ret == 1) buf_add(out, "=> Passed\n");
                else if ((ret == 0) || (ret == -1)) buf_add(out, "=> Failed\n");
                else if (ret < 0) buf_add(out, "=> Error\n");
                if (native) { xml_reg_exec_free(nexec); nexec = NULL; }
                else { xmlRegFreeExecCtxt(exec); exec = NULL; }
            }
            ret = 0;
        }
        else if (have_regexp)
        {
            if (native)
            {
                if (!nexec) nexec = xml_reg_exec_new(nregexp, NULL, NULL);
                ret = xml_reg_exec_push_string(nexec, expr, NULL);
            }
            else
            {
                if (!exec) exec = xmlRegNewExecCtxt(regexp, NULL, NULL);
                ret = xmlRegExecPushString(exec, BAD_CAST expr, NULL);
            }
            buf_add(out, "[push %s %d]\n", expr, ret);
        }
    }
#undef NEWSTATE
    fclose(input);
    if (regexp) xmlRegFreeRegexp(regexp);
    if (exec) xmlRegFreeExecCtxt(exec);
    if (am) xmlFreeAutomata(am);
    if (nregexp) xml_regexp_free(nregexp);
    if (nexec) xml_reg_exec_free(nexec);
    if (nam) xml_automata_free(nam);
}

/* The output of automata_suite_run without the extra [..] lines, as runtest.c prints it. */
static char *strip_extra(const char *s)
{
    Buf b = {0};
    while (*s)
    {
        const char *e = strchr(s, '\n');
        size_t n = e ? (size_t)(e - s + 1) : strlen(s);
        if (*s != '[') buf_add(&b, "%.*s", (int)n, s);
        s += n;
    }
    return b.s ? b.s : strdup("");
}

static void automata_suite_file(const char *path, const char *result)
{
    n_files_++;
    Buf a = {0}, b = {0};
    automata_suite_run(path, 0, &a);
    automata_suite_run(path, 1, &b);
    if (strcmp(buf_str(&a), buf_str(&b))) report("automata suite", path, buf_str(&a), buf_str(&b));
    char *expected = read_file(result);
    char *got = strip_extra(buf_str(&b));
    if (expected && strcmp(expected, got)) report("automata suite result", path, expected, got);
    free(expected);
    free(got);
    free(a.s);
    free(b.s);
}

static void run_suite(const char *libxml2)
{
    const char *dirs[] = { "regexp", "automata" };
    for (int d = 0; d < 2; d++)
    {
        char dir[2048];
        snprintf(dir, sizeof(dir), "%s/test/%s", libxml2, dirs[d]);
        DIR *dp = opendir(dir);
        if (!dp) { printf("Missing %s\n", dir); diff_++; continue; }
        struct dirent *de;
        char *names[512];
        int n = 0;
        while ((de = readdir(dp)) != NULL && n < 512)
        {
            if (de->d_name[0] == '.') continue;
            names[n++] = strdup(de->d_name);
        }
        closedir(dp);
        for (int i = 0; i < n; i++)
        {
            char path[4096], result[4096], err[4096];
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
            snprintf(result, sizeof(result), "%s/result/%s/%s", libxml2, dirs[d], names[i]);
            snprintf(err, sizeof(err), "%s/result/%s/%s.err", libxml2, dirs[d], names[i]);
            if (d == 0) regexp_suite_file(path, result, err);
            else automata_suite_file(path, result);
            free(names[i]);
        }
    }
}

static void add_cp(Buf *b, int c);

/* The Unicode tables: every category and block of libxml2 codegen/unicode.inc matched
   with \p{..} against all the code points around the range bounds found in that file. */
static void run_unicode(const char *libxml2)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/codegen/unicode.inc", libxml2);
    char *inc = read_file(path);
    if (!inc) { printf("Missing %s\n", path); diff_++; return; }
    int *cps = (int*)malloc(sizeof(int) * 200000);
    int ncps = 0;
    for (const char *c = inc; (c = strstr(c, "0x")) != NULL; c += 2)
    {
        long v = strtol(c, NULL, 16);
        for (long d = -1; d <= 1 && ncps < 199990; d++) if (v + d > 0 && v + d <= 0x10ffff) cps[ncps++] = (int)(v + d);
    }
    for (int i = 1; i < 0x300; i++) cps[ncps++] = i;
    char *names[400];
    int nnames = 0;
    const char *cats[] = { "C", "Cc", "Cf", "Co", "Cn", "L", "Ll", "Lm", "Lo", "Lt", "Lu", "M", "Mc", "Me", "Mn", "N",
                           "Nd", "Nl", "No", "P", "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps", "S", "Sc", "Sk", "Sm",
                           "So", "Z", "Zl", "Zp", "Zs" };
    for (size_t i = 0; i < sizeof(cats) / sizeof(cats[0]); i++) names[nnames++] = strdup(cats[i]);
    for (const char *c = inc; (c = strstr(c, "  {\"")) != NULL && nnames < 390; c += 4)
    {
        const char *e = strchr(c + 4, '"');
        char tmp[200];
        snprintf(tmp, sizeof(tmp), "Is%.*s", (int)(e - c - 4), c + 4);
        names[nnames++] = strdup(tmp);
    }
    free(inc);
    Buf pat = {0}, in = {0};
    long checked = 0;
    for (int n = 0; n < nnames; n++)
    {
        for (int neg = 0; neg < 2; neg++)
        {
            buf_clear(&pat);
            buf_add(&pat, neg ? "[^\\p{%s}]" : "\\p{%s}", names[n]);
            RePair p = {0};
            if (compile_pair(&p, buf_str(&pat), buf_str(&pat)))
            {
                for (int i = 0; i < ncps; i++)
                {
                    buf_clear(&in);
                    add_cp(&in, cps[i]);
                    int a = xmlRegexpExec(p.lx, (const xmlChar*)buf_str(&in));
                    int b = xml_regexp_exec(p.nx, buf_str(&in));
                    checked++;
                    if (a != b)
                    {
                        char w[300];
                        snprintf(w, sizeof(w), "%s code point 0x%x", buf_str(&pat), cps[i]);
                        report("unicode", w, a ? "match" : "no match", b ? "match" : "no match");
                    }
                }
            }
            free_pair(&p);
        }
        free(names[n]);
    }
    n_unicode_ = checked;
    free(cps);
    free(pat.s);
    free(in.s);
}

/* Random generation //////////////////////////////////////////////////////////// */

static uint64_t rng_ = 1;

static uint32_t rnd(void)
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 7;
    rng_ ^= rng_ << 17;
    return (uint32_t)(rng_ >> 11);
}

static int rn(int n)
{
    return n <= 0 ? 0 : (int)(rnd() % (uint32_t)n);
}

static const char *pick(const char **a, int n)
{
    return a[rn(n)];
}
#define PICK(a) pick(a, sizeof(a) / sizeof(a[0]))

static void add_cp(Buf *b, int c)
{
    char s[5];
    if (c < 0x80) { s[0] = c; s[1] = 0; }
    else if (c < 0x800) { s[0] = 0xc0 | (c >> 6); s[1] = 0x80 | (c & 0x3f); s[2] = 0; }
    else if (c < 0x10000) { s[0] = 0xe0 | (c >> 12); s[1] = 0x80 | ((c >> 6) & 0x3f); s[2] = 0x80 | (c & 0x3f); s[3] = 0; }
    else { s[0] = 0xf0 | (c >> 18); s[1] = 0x80 | ((c >> 12) & 0x3f); s[2] = 0x80 | ((c >> 6) & 0x3f); s[3] = 0x80 | (c & 0x3f); s[4] = 0; }
    buf_add(b, "%s", s);
}

/* Characters for the inputs, some of them special for regexps or of different categories. */
static const int input_chars_[] = {
    'a', 'b', 'c', 'x', 'z', 'A', 'B', 'Z', '0', '1', '5', '9', ' ', '\t', '\n', '\r', '-', '_', ':', '.',
    '(', ')', '[', ']', '{', '}', '|', '*', '+', '?', '\\', '^', ',', '!', '"', '#', '$', '%', '/', ';', '=',
    '>', '@', '`', '~', '&', '<', 0x7f, 0x85, 0xa0, 0xab, 0xb7, 0xbb, 0xe9, 0xdf, 0xf8, 0x1c5, 0x2b0, 0x300,
    0x3a9, 0x488, 0x5d0, 0x660, 0x6dd, 0x903, 0xe01, 0x16ee, 0x2028, 0x2029, 0x203f, 0x2160, 0x20ac,
    0x2190, 0x3000, 0x3007, 0x4e2d, 0xe000, 0xfeff, 0xff10, 0xfffd, 0x10000, 0x10400, 0x1d400, 0x1d7ce,
    0x20000, 0xf0000, 0x10fffd,
};

static int random_cp(void)
{
    return input_chars_[rn(sizeof(input_chars_) / sizeof(input_chars_[0]))];
}

/* A regexp syntax tree, for generating the expression and strings that might match it. */
typedef struct Node Node;
struct Node
{
    int kind;          // 0 char, 1 class, 2 escape, 3 dot, 4 group
    char text[256];    // the regexp text of the atom (without quantifier)
    int cp;            // for chars
    int minq, maxq;    // quantifier, maxq -1 unbounded
    char quant[32];
    Node *alts[4];     // group alternatives, each a sequence (linked by next)
    int nalts;
    Node *next;
    int class_cps[8];  // some codepoints of a class
    int nclass;
};

static const char *escapes_[] = {
    "\\d", "\\D", "\\w", "\\W", "\\s", "\\S", "\\i", "\\I", "\\c", "\\C", "\\n", "\\r", "\\t", "\\\\", "\\|",
    "\\.", "\\?", "\\*", "\\+", "\\(", "\\)", "\\{", "\\}", "\\-", "\\[", "\\]", "\\^", "\\!", "\\#", "\\u0041",
    "\\u00e9", "\\uD800\\uDC00", "\\p{L}", "\\p{Lu}", "\\p{Ll}", "\\p{Lt}", "\\p{Lm}", "\\p{Lo}", "\\p{M}",
    "\\p{Mn}", "\\p{Mc}", "\\p{Me}", "\\p{N}", "\\p{Nd}", "\\p{Nl}", "\\p{No}", "\\p{P}", "\\p{Pc}", "\\p{Pd}",
    "\\p{Ps}", "\\p{Pe}", "\\p{Pi}", "\\p{Pf}", "\\p{Po}", "\\p{Z}", "\\p{Zs}", "\\p{Zl}", "\\p{Zp}",
    "\\p{S}", "\\p{Sm}", "\\p{Sc}", "\\p{Sk}", "\\p{So}", "\\p{C}", "\\p{Cc}", "\\p{Cf}", "\\p{Co}", "\\p{Cn}",
    "\\P{L}", "\\P{Nd}", "\\P{Z}", "\\p{IsBasicLatin}", "\\P{IsBasicLatin}", "\\p{IsLatin-1Supplement}",
    "\\p{IsGreek}", "\\p{IsGreekandCoptic}", "\\p{IsCJKUnifiedIdeographs}", "\\p{IsPrivateUse}",
    "\\p{IsSupplementaryPrivateUseArea-A}", "\\p{IsMathematicalAlphanumericSymbols}", "\\p{IsArabic}",
    "\\p{IsHebrew}", "\\p{IsNoSuchBlock}", "\\p{IsCombiningMarksforSymbols}", "\\p{IsSpecials}",
};

static const char *class_items_[] = {
    "a", "b", "z", "0-9", "a-z", "A-Z", "a-c", "x-z", "\\d", "\\s", "\\w", "\\i", "\\c", "\\p{L}", "\\P{L}",
    "\\p{Nd}", "\\p{IsBasicLatin}", "\\-", "\\[", "\\]", "\\^", "\\\\", "\\n", "\\t", "\\u0041-\\u005A",
    "é", "中", "𐀀", "à-ÿ", "Α-Ω", "-", ".", "_", ":", "\\.-\\/", "\\u4e00-\\u9fff", "\\p{IsArabic}",
    "\\p{M}", "\\p{Mn}", "\\p{Mc}", "\\p{N}", "\\p{Nl}", "\\p{No}", "\\p{P}", "\\p{Pd}", "\\p{Ps}", "\\p{Z}",
    "\\p{Zs}", "\\p{S}", "\\p{Sm}", "\\p{Sc}", "\\p{C}", "\\p{Cc}", "\\p{Cf}", "\\p{Lu}", "\\p{Ll}", "\\p{Lo}",
    "\\p{IsGreek}", "\\S", "\\I", "\\C", "\\D", "\\W", "\\P{Lu}", "\\r", "\\u00e9", ".",
};

static Node *gen_seq(int depth);

static Node *gen_atom(int depth)
{
    Node *n = (Node*)calloc(1, sizeof(Node));
    int r = rn(100);
    if (r < 40 || depth > 3)
    {
        n->kind = 0;
        int c;
        do { c = random_cp(); } while (c == '.' || c == '\\' || c == '?' || c == '*' || c == '+' || c == '(' ||
                                         c == ')' || c == '|' || c == '[' || c == ']' || c == '\n' || c == '\r' ||
                                         (c == '{' && rn(4)) || (c == '}' && rn(4)) || c == '\t' || c == ' ');
        n->cp = c;
        Buf b = {0};
        add_cp(&b, c);
        snprintf(n->text, sizeof(n->text), "%s", b.s);
        free(b.s);
    }
    else if (r < 60)
    {
        n->kind = 1;
        Buf b = {0};
        buf_add(&b, "[");
        if (rn(4) == 0) buf_add(&b, "^");
        int items = 1 + rn(3);
        for (int i = 0; i < items; i++)
        {
            const char *it = PICK(class_items_);
            buf_add(&b, "%s", it);
            if (n->nclass < 8)
            {
                int len = 4;
                if (it[0] != '\\') n->class_cps[n->nclass++] = (unsigned char)it[0] < 0x80 ? it[0] : 'a';
                else n->class_cps[n->nclass++] = it[1];
                (void)len;
            }
        }
        if (rn(6) == 0) buf_add(&b, "-[%s]", PICK(class_items_));
        buf_add(&b, "]");
        snprintf(n->text, sizeof(n->text), "%s", b.s);
        free(b.s);
    }
    else if (r < 80)
    {
        n->kind = 2;
        snprintf(n->text, sizeof(n->text), "%s", PICK(escapes_));
    }
    else if (r < 85)
    {
        n->kind = 3;
        snprintf(n->text, sizeof(n->text), ".");
    }
    else
    {
        n->kind = 4;
        n->nalts = 1 + rn(3);
        for (int i = 0; i < n->nalts; i++) n->alts[i] = gen_seq(depth + 1);
    }
    // quantifier
    r = rn(100);
    n->minq = 1; n->maxq = 1;
    if (r < 55) {}
    else if (r < 63) { strcpy(n->quant, "?"); n->minq = 0; n->maxq = 1; }
    else if (r < 71) { strcpy(n->quant, "*"); n->minq = 0; n->maxq = -1; }
    else if (r < 79) { strcpy(n->quant, "+"); n->minq = 1; n->maxq = -1; }
    else
    {
        int a = rn(4), b = a + rn(4);
        int k = rn(4);
        if (k == 0) { snprintf(n->quant, sizeof(n->quant), "{%d}", a); n->minq = a; n->maxq = a; }
        else if (k == 1) { snprintf(n->quant, sizeof(n->quant), "{%d,}", a); n->minq = a; n->maxq = -1; }
        else { snprintf(n->quant, sizeof(n->quant), "{%d,%d}", a, b); n->minq = a; n->maxq = b; }
    }
    return n;
}

static Node *gen_seq(int depth)
{
    int len = rn(4) + (depth == 0 ? 1 : 0);
    Node *head = NULL, **tail = &head;
    for (int i = 0; i < len; i++)
    {
        *tail = gen_atom(depth);
        tail = &(*tail)->next;
    }
    return head;
}

static void free_seq(Node *n)
{
    while (n)
    {
        Node *next = n->next;
        for (int i = 0; i < n->nalts; i++) free_seq(n->alts[i]);
        free(n);
        n = next;
    }
}

static void print_seq(Buf *b, Node *n)
{
    for (; n; n = n->next)
    {
        if (n->kind == 4)
        {
            buf_add(b, "(");
            for (int i = 0; i < n->nalts; i++)
            {
                if (i) buf_add(b, "|");
                print_seq(b, n->alts[i]);
            }
            buf_add(b, ")");
        }
        else buf_add(b, "%s", n->text);
        buf_add(b, "%s", n->quant);
    }
}

/* A string that likely matches the sequence. */
static void sample_seq(Buf *b, Node *n, int budget)
{
    for (; n; n = n->next)
    {
        int count = n->minq + (n->maxq < 0 ? rn(3) : rn(n->maxq - n->minq + 1));
        if (rn(10) == 0) count += rn(3) - 1;
        if (count < 0) count = 0;
        if (count > 4) count = 4;
        for (int i = 0; i < count && (int)b->len < budget; i++)
        {
            switch (n->kind)
            {
            case 0: add_cp(b, n->cp); break;
            case 1: add_cp(b, n->nclass && rn(3) ? n->class_cps[rn(n->nclass)] : random_cp()); break;
            case 2:
                if (n->text[1] == 'd' || (n->text[1] == 'p' && n->text[3] == 'N')) add_cp(b, '0' + rn(10));
                else if (n->text[1] == 'u' && n->text[2] == '0') add_cp(b, n->text[5] == '4' ? 'A' : 0xe9);
                else if (n->text[1] == 'w' || n->text[1] == 'i' || n->text[1] == 'c' || n->text[1] == 'p') add_cp(b, rn(2) ? 'a' + rn(26) : random_cp());
                else if (n->text[1] == 's') add_cp(b, " \t\n\r"[rn(4)]);
                else if (n->text[1] == 'n') add_cp(b, '\n');
                else if (n->text[1] == 't') add_cp(b, '\t');
                else if (n->text[1] == 'r') add_cp(b, '\r');
                else if (strchr("\\|.?*+(){}-[]^!#", n->text[1])) add_cp(b, n->text[1]);
                else add_cp(b, random_cp());
                break;
            case 3: add_cp(b, random_cp()); break;
            case 4: sample_seq(b, n->alts[rn(n->nalts)], budget); break;
            }
        }
    }
}

static const char *soup_[] = {
    "a", "b", "c", "x", "(", ")", "|", "*", "+", "?", "{2}", "{1,3}", "{,2}", "{2,}", "{0}", "{0,0}", "{3,1}",
    "{99999999999}", "{1", "{a}", "[", "]", "^", "-", "[a-z]", "[^abc]", "[a-z-[aeiou]]", "[-a]", "[a-]",
    "[]", "[^]", "[a-\\]]", "[\\]-a]", "[z-a]", "\\", "\\d", "\\p{L}", "\\p{", "\\p{Lx}", "\\p{IsBasicLatin}",
    "\\p{Is}", "\\pL", "\\P{IsBasicLatin}", "\\u0041", "\\uD800", "\\uD800\\uDC00", "\\u00G1", "\\uDC00",
    "\\x", "\\y", "\\q", "\\!", "\\&", ".", "é", "中", "𐀀", "\xff", "\xc3", "\xe4\xb8", "\\i", "\\c", "\\w",
    "\\s", "\\S", "\\n", "\\-", "[\\d-z]", "[a-\\d]", "[\\u0041-\\u0039]", "((", "))", "(a|)", "(|b)", "a**",
    "a?+", "[[a]]", "[a[b]]", "[a-[b]c]", "[\\p{L}-[\\p{Lu}]]", "\\p{I", "\\p{Ix}", "\\P{L", "\\P", "\\pX",
    "[\\r]", "\\r", "[a-\\r]", "[\\r-\\t]", "[\\q]", "[\\q-a]", "[a-\\q]", "{1,a}", "{99999999999,2}", "[\xff]",
    "[a-\xff]", "[\\u0041]", "[\\uD800\\uDC00-\\uDBFF\\uDFFF]", "[\\uZZZZ]", "\\uD800a", "[\\.-\\/]", "[^\\d]",
    "[\\p{M}\\p{N}]", "(((((((((((((((((((((((((((((((((((((((((((((((((((a)))))))))))))))))))))))))))))))))))))))))))))))))))",
    "((((((((((((((((((((((((((((((((((((((((((((((((((a))))))))))))))))))))))))))))))))))))))))))))))))))",
};

static void gen_soup(Buf *b)
{
    int n = 1 + rn(6);
    for (int i = 0; i < n; i++) buf_add(b, "%s", PICK(soup_));
}

static void random_input(Buf *b)
{
    int len = rn(7);
    for (int i = 0; i < len; i++) add_cp(b, random_cp());
    if (rn(40) == 0) buf_add(b, "%s", rn(2) ? "\xff" : "\xc3");
}

static void run_random_regexps(int count, unsigned seed)
{
    rng_ = seed * 2654435761u + 0x9e3779b97f4a7c15ull;
    Buf pat = {0}, in = {0};
    char where[1400];
    for (int i = 0; i < count; i++)
    {
        buf_clear(&pat);
        Node *seq = NULL;
        int alts = 1;
        Node *altseq[3] = {0};
        if (rn(4) == 0)
        {
            gen_soup(&pat);
        }
        else
        {
            alts = rn(5) == 0 ? 2 + rn(2) : 1;
            for (int a = 0; a < alts; a++)
            {
                altseq[a] = gen_seq(0);
                if (a) buf_add(&pat, "|");
                print_seq(&pat, altseq[a]);
            }
            seq = altseq[0];
        }
        snprintf(where, sizeof(where), "random regexp %d \"%.1000s\"", i, buf_str(&pat));
        RePair p = {0};
        if (compile_pair(&p, buf_str(&pat), where))
        {
            if (rn(2)) compare_determinism(&p, where);
            for (int k = 0; k < 12; k++)
            {
                buf_clear(&in);
                if (seq && k < 8) sample_seq(&in, altseq[rn(alts)], 24);
                else random_input(&in);
                compare_exec(&p, buf_str(&in), where);
            }
            compare_exec(&p, "", where);
            if (rn(3) == 0) compare_determinism(&p, where);
        }
        free_pair(&p);
        for (int a = 0; a < alts; a++) free_seq(altseq[a]);
    }
    free(pat.s);
    free(in.s);
}

/* Random automata ////////////////////////////////////////////////////////////// */

static Buf lx_log_, nx_log_;

static void lx_callback(xmlRegExecCtxtPtr exec, const xmlChar *token, void *transdata, void *inputdata)
{
    buf_add(&lx_log_, "{cb %ld %s %ld %ld}", (long)(intptr_t)exec, token ? (const char*)token : "(null)",
            (long)(intptr_t)transdata, (long)(intptr_t)inputdata);
}

static void nx_callback(void *exec_data, const char *token, void *transdata, void *inputdata)
{
    buf_add(&nx_log_, "{cb %ld %s %ld %ld}", (long)(intptr_t)exec_data, token ? token : "(null)",
            (long)(intptr_t)transdata, (long)(intptr_t)inputdata);
}

static const char *long_token_ = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char *tokens_[] = { "a", "b", "c", "d", "a|b", "a|*", "*", "x|ns", "b|ns", "c|other" };
static const char *tokens2_[] = { NULL, "", "ns", "other", "*" };

static void log_values(Buf *log, int ret, int nbval, int nbneg, char **values, int terminal)
{
    buf_add(log, "(ret %d nbval %d nbneg %d terminal %d:", ret, nbval, nbneg, terminal);
    if (ret == 0)
    {
        for (int i = 0; i < nbval + nbneg && i < 30; i++) buf_add(log, " %s", values[i] ? values[i] : "(null)");
    }
    buf_add(log, ")");
}

static void exec_values(xmlRegExecCtxtPtr lx, XmlRegExecCtxt *nx, int err, int maxval)
{
    xmlChar *lvalues[30];
    char *nvalues[30];
    int lnbval = maxval, lnbneg = -9, lterminal = -9;
    int nnbval = maxval, nnbneg = -9, nterminal = -9;
    const xmlChar *lstring = NULL;
    const char *nstring = NULL;
    int lr, nr;
    memset(lvalues, 0, sizeof(lvalues));
    memset(nvalues, 0, sizeof(nvalues));
    if (err)
    {
        lr = xmlRegExecErrInfo(lx, &lstring, &lnbval, &lnbneg, lvalues, &lterminal);
        nr = xml_reg_exec_err_info(nx, &nstring, &nnbval, &nnbneg, nvalues, &nterminal);
        buf_add(&lx_log_, "<err %s>", lstring ? (const char*)lstring : "(null)");
        buf_add(&nx_log_, "<err %s>", nstring ? nstring : "(null)");
    }
    else
    {
        lr = xmlRegExecNextValues(lx, &lnbval, &lnbneg, lvalues, &lterminal);
        nr = xml_reg_exec_next_values(nx, &nnbval, &nnbneg, nvalues, &nterminal);
    }
    n_values_++;
    log_values(&lx_log_, lr, lnbval, lnbneg, (char**)lvalues, lterminal);
    log_values(&nx_log_, nr, nnbval, nnbneg, nvalues, nterminal);
}

/* One exec run with random pushes. mode 0 compares both libraries, 1 runs only libxml2,
   2 only native, 3 none (just consumes the random numbers). */
static void exec_run(xmlRegexpPtr lre, XmlRegexp *nre, int mode, int run, const char *wops)
{
    int ul = mode == 0 || mode == 1, un = mode == 0 || mode == 2;
    buf_clear(&lx_log_);
    buf_clear(&nx_log_);
    int with_cb = rn(3) != 0;
    void *edata = (void*)(intptr_t)(1000 + run);
    xmlRegExecCtxtPtr lx = ul ? xmlRegNewExecCtxt(lre, with_cb ? lx_callback : NULL, edata) : NULL;
    XmlRegExecCtxt *nx = un ? xml_reg_exec_new(nre, with_cb ? nx_callback : NULL, edata) : NULL;
    if (mode == 0 && (lx != NULL) != (nx != NULL)) report("exec new", wops, lx ? "ctxt" : "NULL", nx ? "ctxt" : "NULL");
    int npush = rn(9);
    for (int k = 0; k <= npush; k++)
    {
        int last = k == npush;
        const char *tok = last ? NULL : PICK(tokens_);
        void *idata = rn(4) ? (void*)(intptr_t)(100 + k) : NULL;
        int lr2 = 0, nr2 = 0;
        if (!last && rn(3) == 0)
        {
            const char *t2 = rn(3) ? "ns" : (rn(2) ? "other" : NULL);
            const char *t1 = rn(2) ? "a" : (rn(2) ? "x" : (rn(4) ? "c" : long_token_));
            if (lx) lr2 = xmlRegExecPushString2(lx, BAD_CAST t1, BAD_CAST t2, idata);
            if (nx) nr2 = xml_reg_exec_push_string2(nx, t1, t2, idata);
            buf_add(&lx_log_, "[push2 %s %s %d]", t1, t2 ? t2 : "NULL", lr2);
            buf_add(&nx_log_, "[push2 %s %s %d]", t1, t2 ? t2 : "NULL", nr2);
        }
        else
        {
            if (lx) lr2 = xmlRegExecPushString(lx, BAD_CAST tok, idata);
            if (nx) nr2 = xml_reg_exec_push_string(nx, tok, idata);
            buf_add(&lx_log_, "[push %s %d]", tok ? tok : "NULL", lr2);
            buf_add(&nx_log_, "[push %s %d]", tok ? tok : "NULL", nr2);
        }
        if (mode == 0) n_pushes_++;
        // Only run in a child (libxml2 crashed or hung): the native results must be sane.
        if (mode == 2 && !(nr2 == 0 || nr2 == 1 || nr2 == -1 || nr2 == -4 || nr2 == -5 || nr2 == -6 || nr2 == -7))
            _exit(4);
        int m = 1 + rn(8);
        if (lx && nx) exec_values(lx, nx, 0, m);
        int r = mode == 2 ? nr2 : lr2;
        int e = rn(4) == 0;
        m = 1 + rn(8);
        if ((r < 0 || e) && lx && nx) exec_values(lx, nx, 1, m);
        if (r < 0 && rn(3) == 0)
        {
            // Recover like xmlschemas.c does for some errors.
            if (lx) xmlRegExecClearErrors(lx);
            if (nx) xml_reg_exec_clear_errors(nx);
            buf_add(&lx_log_, "[clear]");
            buf_add(&nx_log_, "[clear]");
        }
    }
    if (lx && nx)
    {
        exec_values(lx, nx, 1, 20);
        if (with_cb) n_callbacks_++;
        if (strcmp(buf_str(&lx_log_), buf_str(&nx_log_))) report("automata exec", wops, buf_str(&lx_log_), buf_str(&nx_log_));
    }
    if (lx) xmlRegFreeExecCtxt(lx);
    if (nx) xml_reg_exec_free(nx);
}

static void child_crash(int sig)
{
    (void)sig;
    _exit(3);
}

/* Run the exec runs of one side in a child process, returns 0 if ok, 1 if it crashed
   and 2 if it did not finish in time. */
static int exec_runs_fail(xmlRegexpPtr lre, XmlRegexp *nre, int mode, const char *wops, int seconds)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) { dup2(fd, 2); dup2(fd, 1); }
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = child_crash;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        alarm(seconds);
        for (int run = 0; run < 6; run++) exec_run(lre, nre, mode, run, wops);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) return 2;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 4) return 3;
    return 1;
}

static void run_random_automata(int count, unsigned seed)
{
    rng_ = seed * 2654435761u + 0x7f4a7c159e3779b9ull;
    char where[200];
    for (int i = 0; i < count; i++)
    {
        snprintf(where, sizeof(where), "random automata %d (seed %u)", i, seed);
        n_automata_++;
        xmlAutomataPtr lam = xmlNewAutomata();
        XmlAutomata *nam = xml_automata_new();
        xml_automata_set_error_func(nam, nx_error, NULL);
        buf_clear(&lx_errors_);
        buf_clear(&nx_errors_);
        xmlAutomataStatePtr ls[64];
        XmlAutomataState *ns[64];
        int nstates = 1;
        ls[0] = xmlAutomataGetInitState(lam);
        ns[0] = xml_automata_get_init_state(nam);
        int lcounters[16], ncounters[16], ncount = 0;
        Buf ops = {0};
        if (rn(8) == 0)
        {
            xmlAutomataSetFlags(lam, 1);
            xml_automata_set_flags(nam, XML_AUTOMATA_RNG);
            buf_add(&ops, "rng ");
        }
        int nops = 1 + rn(14);
        int simple = rn(3) == 0; // only plain transitions, compact representation likely
        for (int o = 0; o < nops; o++)
        {
            int kind = simple ? (rn(5) == 0 ? 1 : 0) : rn(14);
            int from = rn(nstates);
            int to = rn(nstates + 2); // >= nstates means NULL (new state)
            xmlAutomataStatePtr lto = to < nstates ? ls[to] : NULL;
            XmlAutomataState *nto = to < nstates ? ns[to] : NULL;
            const char *tok = PICK(tokens_);
            const char *tok2 = PICK(tokens2_);
            int min = rn(4) - (rn(10) == 0), max = min + rn(4) - (rn(10) == 0);
            void *data = (void*)(intptr_t)(o + 1);
            xmlAutomataStatePtr lr = NULL;
            XmlAutomataState *nr = NULL;
            int made = 1;
            switch (kind)
            {
            case 0:
                lr = xmlAutomataNewTransition(lam, ls[from], lto, BAD_CAST tok, data);
                nr = xml_automata_new_transition(nam, ns[from], nto, tok, data);
                buf_add(&ops, "t(%d,%d,%s) ", from, to, tok);
                break;
            case 1:
                lr = xmlAutomataNewEpsilon(lam, ls[from], lto);
                nr = xml_automata_new_epsilon(nam, ns[from], nto);
                buf_add(&ops, "e(%d,%d) ", from, to);
                break;
            case 2:
                lr = xmlAutomataNewTransition2(lam, ls[from], lto, BAD_CAST tok, BAD_CAST tok2, data);
                nr = xml_automata_new_transition2(nam, ns[from], nto, tok, tok2, data);
                buf_add(&ops, "t2(%d,%d,%s,%s) ", from, to, tok, tok2 ? tok2 : "NULL");
                break;
            case 3:
                lr = xmlAutomataNewNegTrans(lam, ls[from], lto, BAD_CAST tok, BAD_CAST tok2, data);
                nr = xml_automata_new_neg_trans(nam, ns[from], nto, tok, tok2, data);
                buf_add(&ops, "neg(%d,%d,%s,%s) ", from, to, tok, tok2 ? tok2 : "NULL");
                break;
            case 4:
                lr = xmlAutomataNewCountTrans(lam, ls[from], lto, BAD_CAST tok, min, max, data);
                nr = xml_automata_new_count_trans(nam, ns[from], nto, tok, min, max, data);
                buf_add(&ops, "ct(%d,%d,%s,%d,%d) ", from, to, tok, min, max);
                break;
            case 5:
                lr = xmlAutomataNewCountTrans2(lam, ls[from], lto, BAD_CAST tok, BAD_CAST tok2, min, max, data);
                nr = xml_automata_new_count_trans2(nam, ns[from], nto, tok, tok2, min, max, data);
                buf_add(&ops, "ct2(%d,%d,%s,%s,%d,%d) ", from, to, tok, tok2 ? tok2 : "NULL", min, max);
                break;
            case 6:
                lr = xmlAutomataNewOnceTrans(lam, ls[from], lto, BAD_CAST tok, min, max, data);
                nr = xml_automata_new_once_trans(nam, ns[from], nto, tok, min, max, data);
                buf_add(&ops, "once(%d,%d,%s,%d,%d) ", from, to, tok, min, max);
                break;
            case 7:
                lr = xmlAutomataNewOnceTrans2(lam, ls[from], lto, BAD_CAST tok, BAD_CAST tok2, min, max, data);
                nr = xml_automata_new_once_trans2(nam, ns[from], nto, tok, tok2, min, max, data);
                buf_add(&ops, "once2(%d,%d,%s,%s,%d,%d) ", from, to, tok, tok2 ? tok2 : "NULL", min, max);
                break;
            case 8:
            {
                int lax = rn(2);
                lr = xmlAutomataNewAllTrans(lam, ls[from], lto, lax);
                nr = xml_automata_new_all_trans(nam, ns[from], nto, lax);
                buf_add(&ops, "all(%d,%d,%d) ", from, to, lax);
                break;
            }
            case 9:
            {
                made = 0;
                if (ncount < 16)
                {
                    lcounters[ncount] = xmlAutomataNewCounter(lam, min, max);
                    ncounters[ncount] = xml_automata_new_counter(nam, min, max);
                    if (lcounters[ncount] != ncounters[ncount]) report("automata new counter", where, "", "");
                    buf_add(&ops, "counter(%d,%d) ", min, max);
                    ncount++;
                }
                break;
            }
            case 10:
            case 11:
            {
                int c = ncount ? rn(ncount + 1) - 1 : -1;
                int lc = c >= 0 ? lcounters[c] : -1, nc = c >= 0 ? ncounters[c] : -1;
                if (kind == 10)
                {
                    lr = xmlAutomataNewCountedTrans(lam, ls[from], lto, lc);
                    nr = xml_automata_new_counted_trans(nam, ns[from], nto, nc);
                    buf_add(&ops, "counted(%d,%d,%d) ", from, to, c);
                }
                else
                {
                    lr = xmlAutomataNewCounterTrans(lam, ls[from], lto, lc);
                    nr = xml_automata_new_counter_trans(nam, ns[from], nto, nc);
                    buf_add(&ops, "countert(%d,%d,%d) ", from, to, c);
                }
                break;
            }
            case 12:
                lr = xmlAutomataNewState(lam);
                nr = xml_automata_new_state(nam);
                buf_add(&ops, "state ");
                to = nstates;
                break;
            case 13:
            {
                made = 0;
                int s = rn(nstates);
                int a = xmlAutomataSetFinalState(lam, ls[s]);
                int b = xml_automata_set_final_state(nam, ns[s]);
                if (a != b) report("automata set final", where, "", "");
                buf_add(&ops, "final(%d) ", s);
                break;
            }
            }
            if (made)
            {
                if ((lr != NULL) != (nr != NULL))
                {
                    report("automata op result", where, lr ? "state" : "NULL", nr ? "state" : "NULL");
                }
                if (lr && nr && to >= nstates && nstates < 64)
                {
                    ls[nstates] = lr;
                    ns[nstates] = nr;
                    nstates++;
                }
            }
        }
        // Make some state final, mostly.
        if (rn(5))
        {
            int s = rn(nstates);
            xmlAutomataSetFinalState(lam, ls[s]);
            xml_automata_set_final_state(nam, ns[s]);
            buf_add(&ops, "final(%d) ", s);
        }
        char wops[2400];
        snprintf(wops, sizeof(wops), "%s: %.2000s", where, buf_str(&ops));
        if (rn(4) == 0)
        {
            int a = xmlAutomataIsDeterminist(lam);
            int b = xml_automata_is_determinist(nam);
            if (a != b)
            {
                char sa[16], sb[16];
                snprintf(sa, sizeof(sa), "%d", a);
                snprintf(sb, sizeof(sb), "%d", b);
                report("automata determinism", wops, sa, sb);
            }
        }
        xmlRegexpPtr lre = xmlAutomataCompile(lam);
        XmlRegexp *nre = xml_automata_compile(nam);
        xmlFreeAutomata(lam);
        xml_automata_free(nam);
        if (strcmp(buf_str(&lx_errors_), buf_str(&nx_errors_))) report("automata errors", wops, buf_str(&lx_errors_), buf_str(&nx_errors_));
        if ((lre != NULL) != (nre != NULL))
        {
            report("automata compile", wops, lre ? "compiled" : "NULL", nre ? "compiled" : "NULL");
        }
        if (lre && nre)
        {
            n_automata_compiled_++;
            RePair p = { lre, nre };
            compare_determinism(&p, wops);
            // libxml2 dereferences a NULL input stack for some counted transitions
            // (xmlRegExecPushStringInternal, the multiple input sequence) and loops forever
            // on cycles of all/counter transitions. The native port fixes both, so run
            // libxml2 in a child first, and when it crashes or hangs only check that the
            // native side (in a child too) neither crashes nor hangs and returns sane values.
            uint64_t saved = rng_;
            int lc = exec_runs_fail(lre, nre, 1, wops, 3);
            rng_ = saved;
            if (lc == 2)
            {
                // Give libxml2 more time before calling it a hang.
                lc = exec_runs_fail(lre, nre, 1, wops, 120);
                rng_ = saved;
            }
            if (lc)
            {
                static const char *outcome[] = { "ok", "crashed", "hangs", "insane result" };
                int nc = exec_runs_fail(lre, nre, 2, wops, 120);
                rng_ = saved;
                if (nc) report("native exec where libxml2 fails", wops, outcome[lc], outcome[nc]);
                else if (lc == 1) n_lx_crash_native_ok_++;
                else n_lx_hang_native_ok_++;
                for (int run = 0; run < 6; run++) exec_run(lre, nre, 3, run, wops); // consume the same random numbers
            }
            else
            {
                // The native side is not run in a child when libxml2 succeeds, a crash is
                // reported by the sanitizers and a hang ends the test here.
                alarm(600);
                for (int run = 0; run < 6; run++) exec_run(lre, nre, 0, run, wops);
                alarm(0);
            }
        }
        if (lre) xmlRegFreeRegexp(lre);
        if (nre) xml_regexp_free(nre);
        free(ops.s);
    }
}

int main(int argc, char **argv)
{
    int count = 20000, acount = 3000;
    unsigned seed = 1, aseed = 1;
    int i = 1;
    for (; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v")) verbose_ = 1;
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) sscanf(argv[++i], "%d:%u", &count, &seed);
        else if (!strcmp(argv[i], "-a") && i + 1 < argc) sscanf(argv[++i], "%d:%u", &acount, &aseed);
        else break;
    }
    if (i >= argc)
    {
        fprintf(stderr, "Usage: xml_regexp_diff [-v] [-n count:seed] [-a count:seed] libxml2_dir\n");
        return 2;
    }
    if (verbose_) max_shown_ = 1000000;
    xmlInitParser();
    xmlSetStructuredErrorFunc(NULL, lx_structured_error);

    run_suite(argv[i]);
    long suite_diff = diff_;
    run_unicode(argv[i]);
    run_random_regexps(count, seed);
    run_random_automata(acount, aseed);

    xmlSetStructuredErrorFunc(NULL, NULL);
    printf("xml_regexp_diff: suite files %ld (lines %ld, %ld differences), regexps compiled %ld, not compiled %ld, "
           "compile error lists %ld, unicode code point checks %ld, matches %ld, determinism %ld, automata %ld (compiled %ld), pushes %ld, "
           "exec runs with callbacks %ld, values/errinfo %ld, automata where libxml2 crashes %ld or hangs %ld (native ok), differences %ld\n",
           n_files_, n_suite_lines_, suite_diff, n_compiled_, n_compile_fail_, n_errors_, n_unicode_, n_matches_, n_determ_,
           n_automata_, n_automata_compiled_, n_pushes_, n_callbacks_, n_values_, n_lx_crash_native_ok_, n_lx_hang_native_ok_, diff_);
    free(lx_errors_.s);
    free(nx_errors_.s);
    free(lx_err_text_.s);
    free(lx_log_.s);
    free(nx_log_.s);
    xmlCleanupParser();
    return diff_ ? 1 : 0;
}
