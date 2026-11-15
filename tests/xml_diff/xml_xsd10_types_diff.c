/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)
   Differential test of the native XML Schema 1.0 built in types (parts/xml_xsd10_types.c)
   against libxml2 xmlschemastypes.c.

   For every built in type the type definition is compared (flags, base type, item type,
   allowed facets), then a corpus of lexical values is validated with both: the values found
   in the files given on the command line (text content and attribute values, normally
   3rdparty/libxml2/test/schemas/*), a list of edge cases and random strings built from type
   specific alphabets. The return codes, canonical values (also with every whitespace mode),
   ValueGetAsString/Boolean and copies of the computed values must be the same. Random pairs
   of the computed values (of the same and of different types) are compared with
   CompareValues/CompareValuesWhtsp. Facets (all except pattern, which needs regexps) are
   created with NewFacet + CheckFacet and random values are validated against them. Last
   QName/NOTATION/ENTITY/ENTITIES/ID/IDREF(S) are validated against the nodes of a document
   with namespace declarations, notations and unparsed entities (parsed by both), the
   native ID table must report the same duplicates as the libxml2 document.

   Usage: xml_xsd10_types_diff [-v] [-r count:seed] files...
   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_xsd10_types.h"

#include<libxml/parser.h>
#include<libxml/tree.h>
#include<libxml/xmlschemastypes.h>
#include<libxml/schemasInternals.h>

#include<stdarg.h>
#include<stdio.h>
#if defined(__has_include)
#if __has_include(<sanitizer/lsan_interface.h>)
#include<sanitizer/lsan_interface.h>
#define HAVE_LSAN 1
#endif
#endif
#include<stdlib.h>
#include<string.h>

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

#define XSD_NS "http://www.w3.org/2001/XMLSchema"

// libxml2 leaks the first normalized value of normalizedString values with tab/cr/nl
// (xmlSchemaValAtomicType), and the computed name/uri of prefixed NOTATIONs that are not
// declared. Its allocations in the validation calls are not leak checked.
#ifdef HAVE_LSAN
#define LIBXML2_LEAKS_BEGIN __lsan_disable()
#define LIBXML2_LEAKS_END __lsan_enable()
#else
#define LIBXML2_LEAKS_BEGIN
#define LIBXML2_LEAKS_END
#endif

static void no_errors(void *ctx, const char *msg, ...)
{
    (void)ctx; (void)msg;
}

static int verbose_ = 0;
static long same_ = 0, diff_ = 0;
static int max_shown_ = 60;
static unsigned long long rng_ = 1;

static unsigned rnd(void)
{
    rng_ = rng_ * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_ >> 33);
}

static const char *type_names_[] = {
    "string", "normalizedString", "decimal", "time", "gDay", "gMonth", "gMonthDay", "gYear",
    "gYearMonth", "date", "dateTime", "duration", "float", "double", "boolean", "token",
    "language", "NMTOKEN", "NMTOKENS", "Name", "QName", "NCName", "ID", "IDREF", "IDREFS",
    "ENTITY", "ENTITIES", "NOTATION", "anyURI", "integer", "nonPositiveInteger",
    "negativeInteger", "nonNegativeInteger", "positiveInteger", "int", "unsignedInt", "long",
    "unsignedLong", "short", "unsignedShort", "byte", "unsignedByte", "hexBinary",
    "base64Binary", "anyType", "anySimpleType"
};
#define NUM_TYPES ((int)(sizeof(type_names_)/sizeof(type_names_[0])))

static xmlSchemaTypePtr ltypes_[NUM_TYPES];
static XmlXsd10Type *ntypes_[NUM_TYPES];

// Escape for printing.
static char *esc(const char *s)
{
    if (!s) return strdup("(null)");
    size_t n = strlen(s);
    char *o = (char*)malloc(n * 4 + 3);
    char *p = o;
    *p++ = '"';
    for (const unsigned char *c = (const unsigned char*)s; *c; c++)
    {
        if (*c < 32 || *c >= 127 || *c == '"' || *c == '\\') p += sprintf(p, "\\x%02x", *c);
        else *p++ = (char)*c;
    }
    *p++ = '"';
    *p = 0;
    return o;
}

static void report(const char *what, const char *tname, const char *value, const char *lres, const char *nres)
{
    diff_++;
    if (max_shown_-- > 0)
    {
        char *e = esc(value);
        printf("DIFF %s type=%s value=%s\n  libxml2: %s\n  native:  %s\n", what, tname, e, lres, nres);
        free(e);
    }
}

static void check_int(const char *what, const char *tname, const char *value, long l, long n)
{
    if (l == n) { same_++; return; }
    char a[64], b[64];
    snprintf(a, sizeof(a), "%ld", l);
    snprintf(b, sizeof(b), "%ld", n);
    report(what, tname, value, a, b);
}

static void check_str(const char *what, const char *tname, const char *value, const char *l, const char *n)
{
    if ((l == NULL && n == NULL) || (l && n && !strcmp(l, n))) { same_++; return; }
    char *a = esc(l), *b = esc(n);
    report(what, tname, value, a, b);
    free(a); free(b);
}

// Corpus /////////////////////////////////////////////////////////

static char **corpus_ = NULL;
static size_t corpus_len_ = 0, corpus_max_ = 0;

static void corpus_add_len(const char *s, size_t n)
{
    if (n > 300) return;
    char *v = (char*)malloc(n + 1);
    memcpy(v, s, n);
    v[n] = 0;
    // Cheap dedupe of the last few entries only, the full dedupe is done after sorting.
    if (corpus_len_ == corpus_max_)
    {
        corpus_max_ = corpus_max_ ? corpus_max_ * 2 : 1024;
        corpus_ = (char**)realloc(corpus_, corpus_max_ * sizeof(char*));
    }
    corpus_[corpus_len_++] = v;
}

static void corpus_add(const char *s)
{
    corpus_add_len(s, strlen(s));
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

static void corpus_dedupe(void)
{
    qsort(corpus_, corpus_len_, sizeof(char*), cmp_str);
    size_t j = 0;
    for (size_t i = 0; i < corpus_len_; i++)
    {
        if (j > 0 && !strcmp(corpus_[j-1], corpus_[i])) { free(corpus_[i]); continue; }
        corpus_[j++] = corpus_[i];
    }
    corpus_len_ = j;
}

static void harvest_file(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 10000000) { fclose(f); return; }
    char *buf = (char*)malloc(len + 1);
    if (fread(buf, 1, len, f) != (size_t)len) { fclose(f); free(buf); return; }
    buf[len] = 0;
    fclose(f);
    for (long i = 0; i < len; i++)
    {
        if (buf[i] == '>')
        {
            long j = i + 1;
            while (j < len && buf[j] != '<') j++;
            if (j > i + 1)
            {
                corpus_add_len(buf + i + 1, j - i - 1);
                // Also trimmed.
                long a = i + 1, b = j;
                while (a < b && (buf[a] == ' ' || buf[a] == '\n' || buf[a] == '\t' || buf[a] == '\r')) a++;
                while (b > a && (buf[b-1] == ' ' || buf[b-1] == '\n' || buf[b-1] == '\t' || buf[b-1] == '\r')) b--;
                if (b > a) corpus_add_len(buf + a, b - a);
            }
        }
        else if (buf[i] == '"' || buf[i] == '\'')
        {
            char q = buf[i];
            long j = i + 1;
            while (j < len && buf[j] != q && buf[j] != '\n' && buf[j] != '<' && buf[j] != '>') j++;
            if (j < len && buf[j] == q)
            {
                corpus_add_len(buf + i + 1, j - i - 1);
                i = j;
            }
        }
    }
    free(buf);
}

static const char *edge_cases_[] = {
    "", " ", "  ", "\t", "\n", "\r\n", " a ", "a  b", "a\tb", "a\nb", " a b ", "a b", "ab ", " ab",
    "0", "1", "-0", "+0", "00", "007", "-007", "+", "-", ".", "-.", "+.", "0.", ".0", "-.0", "0.0", "-0.0",
    "+0.0", "1.", "1.0", "1.10", "1.000", "0.001", "-0.001", "000.000", "1.5", "-1.5", "+1.5", "12345678901234567890",
    "123456789012345678901234567890.123456789012345678901234567890", "-123456789012345678901234567890",
    "9223372036854775807", "9223372036854775808", "-9223372036854775808", "-9223372036854775809",
    "18446744073709551615", "18446744073709551616", "4294967295", "4294967296", "-1", "2147483647",
    "2147483648", "-2147483648", "-2147483649", "32767", "32768", "-32768", "-32769", "65535", "65536",
    "127", "128", "-128", "-129", "255", "256", " 42 ", "\t42\n", "4 2", "42a", "0x10", "1e3", "1E3",
    "1e", "1e+", "1e-", "1.5e-3", "-1.5E+30", "1e400", "-1e400", "1e-400", "3.4028235e38", "3.5e38",
    "1E-149", "1E-46", "INF", "-INF", "+INF", "NaN", "-NaN", "inf", "nan", "Infinity", " INF ", "INF ",
    ".5", "5.", "-.5e1", "e5", ".e5", "1.2.3", "0000000000000000000000000000001",
    "true", "false", "1 ", " true", "TRUE", "True", "yes", "t", "f", "truex", " false ",
    "2004-04-12", "2004-04-12Z", "2004-04-12+02:00", "2004-04-12-05:00", "2004-02-29", "2003-02-29",
    "1900-02-29", "2000-02-29", "2004-04-31", "2004-13-01", "2004-00-01", "2004-01-00", "0000-01-01",
    "-0001-01-01", "-2004-04-12", "12004-04-12", "02004-04-12", "999-01-01", "2004-4-12",
    "2004-04-12T13:20:00", "2004-04-12T13:20:00Z", "2004-04-12T13:20:00.5", "2004-04-12T13:20:00.123456789",
    "2004-04-12T13:20:00-05:00", "2004-04-12T13:20:00+14:00", "2004-04-12T13:20:00+14:01",
    "2004-04-12T13:20:00+15:00", "2004-04-12T24:00:00", "2004-04-12T24:00:01", "2004-04-12T24:30:00",
    "2004-04-12T23:59:60", "2004-04-12T23:60:00", "2004-04-12T13:20", "2004-04-12T13:20:00.",
    "2004-12-31T23:59:59-14:00", "2004-12-31T23:59:59+14:00", "0001-01-01T00:00:00Z",
    "-0001-12-31T23:59:59Z", "2004-04-12 T13:20:00", " 2004-04-12T13:20:00Z ",
    "13:20:00", "13:20:00Z", "13:20:00.5-05:00", "24:00:00", "24:00:00Z", "00:00:00", "25:00:00",
    "13:20", "1:20:00", "13:20:00+24:00", "13:20:00+23:59", "13:20:00+14:00", "13:20:00-14:00",
    "---01", "---31", "---32", "---00", "---1", "---01Z", "---01+02:00", "---01-02:00",
    "--01", "--12", "--13", "--00", "--01Z", "--01-05:00", "--01--", "--02-29", "--02-30", "--04-31",
    "--12-31Z", "--01-01-05:00", "--1-01",
    "2004", "2004Z", "-2004", "20040", "0000", "200", "2004-05:00", "2004+02:00",
    "2004-04", "2004-04Z", "2004-13", "-2004-04", "2004-4",
    "P1Y", "P1M", "P1D", "PT1H", "PT1M", "PT1S", "PT1.5S", "P1Y2M3DT10H30M12.3S", "-P1Y", "P",
    "PT", "P1", "P1YT", "P-1Y", "1Y", "P1.5Y", "PT1.S", "PT.5S", "P1Y1Y", "P1M1Y", "P1DT", "PT36H",
    "P0Y", "-P0D", "P400Y", "P30D", "P31D", "P1M30D", "P1M31D", "P365D", "P366D", "PT86400S",
    "P99999999999999999999Y", "PT99999999999999999999S", "P1Y2M", " P1D ", "P 1D", "-PT0.000001S",
    "0FB7", "0fb7", "0FB", "0G", "", "00", "FFFF", " 0F ", "0 F",
    "AAAA", "AA==", "AAA=", "A===", "AAAAAA==", "AB==", "AAB=", "AAC=", "AQ==", "AR==", "YQ==",
    "YWJj", "YWJjZA==", "YWJjZGU=", "Y W J j", "YWJj\nZGVm", "====", "=AAA", "AA=A", "AAAA=",
    "A", "AA", "AAA", "AAAAA", "\xc3\xa9", "YW*Jj",
    "en", "en-US", "en-us-x-123", "i-klingon", "x-private", "abcdefghi", "abcdefgh", "en-", "-en",
    "e1", "en-12345678", "en-123456789", "en--us", "123",
    "a", "abc", "_a", "a.b", "a-b", "a:b", "p:a", "q:b", "xml:lang", ":a", "a:", "a:b:c", "1a", ".a",
    "-a", "\xc3\xa9t\xc3\xa9", "\xe4\xb8\xad\xe6\x96\x87", "a\xcc\x81", "\xcc\x81", "a\x80", "\xff",
    "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x9f\x98\x80", "a\xf0\x9f\x98\x80", "a\xc2\xb7" "b",
    "\xc2\xb7", "a b c", "a  b  c", " a\tb\nc ", "gif", "p:jpg", "pic", "pic pic2", "txt",
    "http://example.com/a?b=c#d", "urn:x", "a b", "%20", "%zz", "http://[::1]/", "http://a:b@c:80/",
    "#frag", "?q", "//host", "mailto:a@b", "\xc3\xa9", "http://e.com/\xc3\xa9", "a<b", "{a}",
    "http://a b", ":", "::", "a:/b", "1:2", "http://a:99999999999/", "http:",
    "13:20:00.12345678901234Z", "13:20:00.123456789012345-05:00", "2004-04-12T13:20:00.12345678901234+01:00",
    "00:00:00.000000000000001Z", "23:59:59.99999999999999", "2004-04-12T23:59:59.999999999999999",
    "P12M", "P13M", "P24M", "P365D", "P366D", "P28D", "P29D", "P30D", "P31D", "P59D", "P60D", "P61D",
    "P62D", "P2M", "P3M", "P89D", "P92D", "P4Y", "P1461D", "P1460D", "P1462D", "PT8760H", "PT8784H",
    "P1YT1S", "-P1Y", "-P365D", "-P366D", "P11M", "P334D", "P337D", "P1Y11M", "P699D", "P700D",
    "PT0.5S", "PT0.25S", "PT86399.999S", "P1DT0.0001S",
};

typedef struct { const char *name; const char *alphabet; } Alphabet;
static Alphabet alphabets_[] = {
    { "decimal", "0123456789+-. 0.0" },
    { "float", "0123456789+-.eE INFaN" },
    { "date", "0123456789-:TZ+.012" },
    { "duration", "PYMDTHS0123456789.-" },
    { "boolean", "truefals01 \t" },
    { "hex", "0123456789abcdefABCDEFG " },
    { "base64", "ABCabc0189+/= \n" },
    { "name", "abc:_-.1 \xc3\xa9\xcc\x81" },
    { "language", "abcDEF-12 " },
    { "uri", "http:/#?%ab <>{}:[]@" },
    { "string", "ab \t\n\r\xc3\xa9" },
};
#define NUM_ALPHABETS ((int)(sizeof(alphabets_)/sizeof(alphabets_[0])))

static void add_random(int count)
{
    char buf[64];
    for (int a = 0; a < NUM_ALPHABETS; a++)
    {
        const char *al = alphabets_[a].alphabet;
        size_t an = strlen(al);
        for (int i = 0; i < count; i++)
        {
            int len = rnd() % 20;
            for (int j = 0; j < len; j++) buf[j] = al[rnd() % an];
            buf[len] = 0;
            corpus_add(buf);
        }
    }
}

// Values /////////////////////////////////////////////////////////

typedef struct
{
    int type; // index in type_names_
    xmlSchemaValPtr lval;
    XmlXsd10Val *nval;
    const char *value;
} ValPair;

static ValPair *pool_ = NULL;
static size_t pool_len_ = 0, pool_max_ = 0;

static void pool_add(int t, xmlSchemaValPtr l, XmlXsd10Val *n, const char *value)
{
    if (pool_len_ == pool_max_)
    {
        pool_max_ = pool_max_ ? pool_max_ * 2 : 1024;
        pool_ = (ValPair*)realloc(pool_, pool_max_ * sizeof(ValPair));
    }
    pool_[pool_len_].type = t;
    pool_[pool_len_].lval = l;
    pool_[pool_len_].nval = n;
    pool_[pool_len_].value = value;
    pool_len_++;
}

/** Compare the computed values: type, canonical values, string/boolean, copy. */
static void compare_vals(const char *tname, const char *value, xmlSchemaValPtr l, XmlXsd10Val *n)
{
    if ((l == NULL) != (n == NULL))
    {
        report("val-presence", tname, value, l ? "value" : "NULL", n ? "value" : "NULL");
        return;
    }
    if (l == NULL) return;
    check_int("val-type", tname, value, xmlSchemaGetValType(l), xml_xsd10_get_val_type(n));
    const xmlChar *lc = NULL;
    const char *nc = NULL;
    int lr = xmlSchemaGetCanonValue(l, &lc);
    int nr = xml_xsd10_get_canon_value(n, &nc);
    check_int("canon-ret", tname, value, lr, nr);
    check_str("canon", tname, value, (const char*)lc, nc);
    if (verbose_ > 1) printf("%s %s -> %s\n", tname, value, nc ? nc : "(null)");
    xmlFree((void*)lc);
    free((void*)nc);
    for (int ws = 0; ws <= 4; ws++)
    {
        lc = NULL; nc = NULL;
        lr = xmlSchemaGetCanonValueWhtsp(l, &lc, (xmlSchemaWhitespaceValueType)ws);
        nr = xml_xsd10_get_canon_value_whtsp(n, &nc, (XmlXsd10WhitespaceValueType)ws);
        check_int("canon-ws-ret", tname, value, lr, nr);
        check_str("canon-ws", tname, value, (const char*)lc, nc);
        xmlFree((void*)lc);
        free((void*)nc);
    }
    check_str("as-string", tname, value, (const char*)xmlSchemaValueGetAsString(l), xml_xsd10_value_get_as_string(n));
    check_int("as-boolean", tname, value, xmlSchemaValueGetAsBoolean(l), xml_xsd10_value_get_as_boolean(n));
    check_int("next", tname, value, xmlSchemaValueGetNext(l) != NULL, xml_xsd10_value_get_next(n) != NULL);
    xmlSchemaValPtr lcp = xmlSchemaCopyValue(l);
    XmlXsd10Val *ncp = xml_xsd10_copy_value(n);
    check_int("copy", tname, value, lcp != NULL, ncp != NULL);
    if (lcp && ncp)
    {
        check_int("copy-compare", tname, value, xmlSchemaCompareValues(lcp, l), xml_xsd10_compare_values(ncp, n));
        lc = NULL; nc = NULL;
        xmlSchemaGetCanonValue(lcp, &lc);
        xml_xsd10_get_canon_value(ncp, &nc);
        check_str("copy-canon", tname, value, (const char*)lc, nc);
        xmlFree((void*)lc);
        free((void*)nc);
    }
    xmlSchemaFreeValue(lcp);
    xml_xsd10_free_value(ncp);
}

/** Validate value against type t with the three entry points (node NULL). The values computed
    by xmlSchemaValPredefTypeNodeNoNorm are kept in the pool when keep is set. */
static void check_value(int t, const char *value, int keep)
{
    const char *tname = type_names_[t];
    xmlSchemaValPtr l = NULL;
    XmlXsd10Val *n = NULL;

    LIBXML2_LEAKS_BEGIN;
    int lr = xmlSchemaValidatePredefinedType(ltypes_[t], (const xmlChar*)value, &l);
    LIBXML2_LEAKS_END;
    int nr = xml_xsd10_validate_predefined_type(ntypes_[t], value, &n);
    check_int("validate", tname, value, lr, nr);
    compare_vals(tname, value, l, n);
    xmlSchemaFreeValue(l);
    xml_xsd10_free_value(n);

    LIBXML2_LEAKS_BEGIN;
    lr = xmlSchemaValidatePredefinedType(ltypes_[t], (const xmlChar*)value, NULL);
    LIBXML2_LEAKS_END;
    nr = xml_xsd10_validate_predefined_type(ntypes_[t], value, NULL);
    check_int("validate-noval", tname, value, lr, nr);

    l = NULL; n = NULL;
    lr = xmlSchemaValPredefTypeNodeNoNorm(ltypes_[t], (const xmlChar*)value, &l, NULL);
    nr = xml_xsd10_val_predef_type_node_no_norm(ntypes_[t], value, &n, NULL);
    check_int("validate-nonorm", tname, value, lr, nr);
    compare_vals(tname, value, l, n);
    if (keep && l && n && lr == 0 && nr == 0)
    {
        pool_add(t, l, n, value);
    }
    else
    {
        xmlSchemaFreeValue(l);
        xml_xsd10_free_value(n);
    }
}

static const char *ws_name(int ws)
{
    static const char *names[] = { "unknown", "preserve", "replace", "collapse", "bad" };
    return names[ws < 0 || ws > 4 ? 4 : ws];
}

static void check_compares(long count)
{
    if (pool_len_ < 2) return;
    // Index the pool by type to also pick pairs of the same type.
    for (long i = 0; i < count; i++)
    {
        size_t a = rnd() % pool_len_, b = rnd() % pool_len_;
        if (i % 2)
        {
            // Same type: search forward for one.
            for (size_t k = 0; k < pool_len_; k++)
            {
                size_t c = (b + k) % pool_len_;
                if (pool_[c].type == pool_[a].type && c != a) { b = c; break; }
            }
        }
        char what[512];
        snprintf(what, sizeof(what), "%s/%s %s <=> ", type_names_[pool_[a].type], type_names_[pool_[b].type], pool_[a].value);
        size_t wl = strlen(what);
        snprintf(what + wl, sizeof(what) - wl, "%s", pool_[b].value);
        int lr = xmlSchemaCompareValues(pool_[a].lval, pool_[b].lval);
        int nr = xml_xsd10_compare_values(pool_[a].nval, pool_[b].nval);
        check_int("compare", "pair", what, lr, nr);
        int xws = rnd() % 5, yws = rnd() % 5;
        lr = xmlSchemaCompareValuesWhtsp(pool_[a].lval, (xmlSchemaWhitespaceValueType)xws, pool_[b].lval, (xmlSchemaWhitespaceValueType)yws);
        nr = xml_xsd10_compare_values_whtsp(pool_[a].nval, (XmlXsd10WhitespaceValueType)xws, pool_[b].nval, (XmlXsd10WhitespaceValueType)yws);
        char what2[600];
        snprintf(what2, sizeof(what2), "%s ws %s/%s", what, ws_name(xws), ws_name(yws));
        check_int("compare-ws", "pair", what2, lr, nr);
    }
}

static void compare_pair(size_t a, size_t b)
{
    char what[512];
    snprintf(what, sizeof(what), "%s/%s %s <=> ", type_names_[pool_[a].type], type_names_[pool_[b].type], pool_[a].value);
    size_t wl = strlen(what);
    snprintf(what + wl, sizeof(what) - wl, "%s", pool_[b].value);
    int lr = xmlSchemaCompareValues(pool_[a].lval, pool_[b].lval);
    int nr = xml_xsd10_compare_values(pool_[a].nval, pool_[b].nval);
    check_int("compare", "pair", what, lr, nr);
}

/** All pairs of (up to per_type) values of each type, and all pairs of a smaller sample of
    every type (the different types of the same value space are compared too). */
static void check_compares_systematic(size_t per_type, size_t across)
{
    size_t start = 0;
    size_t *sample = (size_t*)malloc(NUM_TYPES * across * sizeof(size_t));
    size_t nsample = 0;
    while (start < pool_len_)
    {
        size_t end = start;
        while (end < pool_len_ && pool_[end].type == pool_[start].type) end++;
        size_t n = end - start;
        size_t m = n < per_type ? n : per_type;
        for (size_t i = 0; i < m; i++)
            for (size_t j = 0; j < m; j++)
                compare_pair(start + i * n / m, start + j * n / m);
        size_t k = n < across ? n : across;
        for (size_t i = 0; i < k && nsample < NUM_TYPES * across; i++) sample[nsample++] = start + i * n / k;
        start = end;
    }
    for (size_t i = 0; i < nsample; i++)
        for (size_t j = 0; j < nsample; j++)
            compare_pair(sample[i], sample[j]);
    free(sample);
}

// Facets /////////////////////////////////////////////////////////

static const int facet_types_[] = {
    XML_SCHEMA_FACET_MININCLUSIVE, XML_SCHEMA_FACET_MINEXCLUSIVE, XML_SCHEMA_FACET_MAXINCLUSIVE,
    XML_SCHEMA_FACET_MAXEXCLUSIVE, XML_SCHEMA_FACET_TOTALDIGITS, XML_SCHEMA_FACET_FRACTIONDIGITS,
    XML_SCHEMA_FACET_ENUMERATION, XML_SCHEMA_FACET_WHITESPACE, XML_SCHEMA_FACET_LENGTH,
    XML_SCHEMA_FACET_MAXLENGTH, XML_SCHEMA_FACET_MINLENGTH
};
#define NUM_FACET_TYPES ((int)(sizeof(facet_types_)/sizeof(facet_types_[0])))

static const char *facet_name(int ft)
{
    switch (ft)
    {
    case XML_SCHEMA_FACET_MININCLUSIVE: return "minInclusive";
    case XML_SCHEMA_FACET_MINEXCLUSIVE: return "minExclusive";
    case XML_SCHEMA_FACET_MAXINCLUSIVE: return "maxInclusive";
    case XML_SCHEMA_FACET_MAXEXCLUSIVE: return "maxExclusive";
    case XML_SCHEMA_FACET_TOTALDIGITS: return "totalDigits";
    case XML_SCHEMA_FACET_FRACTIONDIGITS: return "fractionDigits";
    case XML_SCHEMA_FACET_ENUMERATION: return "enumeration";
    case XML_SCHEMA_FACET_WHITESPACE: return "whiteSpace";
    case XML_SCHEMA_FACET_LENGTH: return "length";
    case XML_SCHEMA_FACET_MAXLENGTH: return "maxLength";
    case XML_SCHEMA_FACET_MINLENGTH: return "minLength";
    case XML_SCHEMA_FACET_PATTERN: return "pattern";
    }
    return "?";
}

static int is_length_facet(int ft)
{
    return ft == XML_SCHEMA_FACET_LENGTH || ft == XML_SCHEMA_FACET_MAXLENGTH || ft == XML_SCHEMA_FACET_MINLENGTH;
}

static int is_decimal_valued_facet(int ft)
{
    return is_length_facet(ft) || ft == XML_SCHEMA_FACET_TOTALDIGITS || ft == XML_SCHEMA_FACET_FRACTIONDIGITS;
}

/** A value from the corpus, mostly ones valid for type t (from the pool). */
static const char *pick_value(int t, ValPair **pair)
{
    *pair = NULL;
    if (pool_len_ > 0 && rnd() % 4 != 0)
    {
        for (int tries = 0; tries < 200; tries++)
        {
            size_t k = rnd() % pool_len_;
            if (pool_[k].type == t || rnd() % 8 == 0)
            {
                *pair = &pool_[k];
                return pool_[k].value;
            }
        }
    }
    static char buf[32];
    switch (rnd() % 3)
    {
    case 0: snprintf(buf, sizeof(buf), "%d", (int)(rnd() % 40)); return buf;
    case 1: snprintf(buf, sizeof(buf), "%d.%d", (int)(rnd() % 100) - 50, (int)(rnd() % 1000)); return buf;
    }
    return corpus_[rnd() % corpus_len_];
}

static void check_facets(int per_facet, int per_value)
{
    for (int t = 0; t < NUM_TYPES; t++)
    {
        for (int f = 0; f < NUM_FACET_TYPES; f++)
        {
            int ft = facet_types_[f];
            for (int i = 0; i < per_facet; i++)
            {
                ValPair *fp;
                const char *fv = pick_value(t, &fp);
                if (ft == XML_SCHEMA_FACET_WHITESPACE && i < 4)
                {
                    static const char *wsv[] = { "preserve", "replace", "collapse", " collapse" };
                    fv = wsv[i];
                }
                char fdesc[512];
                snprintf(fdesc, sizeof(fdesc), "%s facet %s=", type_names_[t], facet_name(ft));
                size_t fl = strlen(fdesc);
                snprintf(fdesc + fl, sizeof(fdesc) - fl, "%s", fv);

                xmlSchemaFacetPtr lf = xmlSchemaNewFacet();
                XmlXsd10Facet *nf = xml_xsd10_new_facet();
                lf->type = (xmlSchemaTypeType)ft;
                nf->type = (XmlXsd10TypeType)ft;
                char *lvalue = strdup(fv), *nvalue = strdup(fv);
                lf->value = (const xmlChar*)lvalue;
                nf->value = nvalue;
                int lr = xmlSchemaCheckFacet(lf, ltypes_[t], NULL, NULL);
                int nr = xml_xsd10_check_facet(nf, ntypes_[t], NULL);
                check_int("check-facet", "facet", fdesc, lr, nr);
                check_int("facet-whitespace", "facet", fdesc, lf->whitespace, nf->whitespace);
                compare_vals("facet-val", fdesc, lf->val, nf->val);
                if (lr == nr && (lf->val != NULL) == (nf->val != NULL))
                {
                    if (lf->val && is_decimal_valued_facet(ft) &&
                        (xmlSchemaGetValType(lf->val) == XML_SCHEMAS_NNINTEGER || xmlSchemaGetValType(lf->val) == XML_SCHEMAS_PINTEGER))
                    {
                        check_int("facet-ulong", "facet", fdesc, (long)xmlSchemaGetFacetValueAsULong(lf),
                                  (long)xml_xsd10_get_facet_value_as_ulong(nf));
                    }
                    for (int j = 0; j < per_value; j++)
                    {
                        ValPair *vp;
                        const char *v = pick_value(t, &vp);
                        char vdesc[1100];
                        snprintf(vdesc, sizeof(vdesc), "%s value ", fdesc);
                        size_t vl = strlen(vdesc);
                        snprintf(vdesc + vl, sizeof(vdesc) - vl, "%s", v);
                        xmlSchemaValPtr lv = vp ? vp->lval : NULL;
                        XmlXsd10Val *nv = vp ? vp->nval : NULL;
                        int base = rnd() % 3; // 0 type, 1 NULL base, 2 random other type
                        int bt = base == 2 ? (int)(rnd() % NUM_TYPES) : t;
                        int withval = rnd() % 4 != 0;
                        lr = xmlSchemaValidateFacet(base == 1 ? NULL : ltypes_[bt], lf, (const xmlChar*)v, withval ? lv : NULL);
                        nr = xml_xsd10_validate_facet(base == 1 ? NULL : ntypes_[bt], nf, v, withval ? nv : NULL);
                        check_int("validate-facet", "facet", vdesc, lr, nr);

                        int fws = rnd() % 4, ws = rnd() % 4;
                        int vt = vp ? vp->type : t;
                        xmlSchemaValType valType = (xmlSchemaValType)(vp && withval ? xmlSchemaGetValType(lv) : ltypes_[vt]->builtInType);
                        if (!(ft == XML_SCHEMA_FACET_ENUMERATION && ws != 0 && lf->val == NULL))
                        {
                            lr = xmlSchemaValidateFacetWhtsp(lf, (xmlSchemaWhitespaceValueType)fws, valType, (const xmlChar*)v,
                                                             withval ? lv : NULL, (xmlSchemaWhitespaceValueType)ws);
                            nr = xml_xsd10_validate_facet_whtsp(nf, (XmlXsd10WhitespaceValueType)fws, (XmlXsd10ValType)valType, v,
                                                                withval ? nv : NULL, (XmlXsd10WhitespaceValueType)ws);
                            char wdesc[1200];
                            snprintf(wdesc, sizeof(wdesc), "%s fws %s ws %s", vdesc, ws_name(fws), ws_name(ws));
                            check_int("validate-facet-ws", "facet", wdesc, lr, nr);
                        }

                        unsigned long llen = 7, nlen = 7;
                        lr = xmlSchemaValidateLengthFacet(ltypes_[bt], lf, (const xmlChar*)v, withval ? lv : NULL, &llen);
                        nr = xml_xsd10_validate_length_facet(ntypes_[bt], nf, v, withval ? nv : NULL, &nlen);
                        check_int("length-facet", "facet", vdesc, lr, nr);
                        check_int("length-facet-len", "facet", vdesc, (long)llen, (long)nlen);
                        llen = 7; nlen = 7;
                        lr = xmlSchemaValidateLengthFacetWhtsp(lf, valType, (const xmlChar*)v, withval ? lv : NULL, &llen, (xmlSchemaWhitespaceValueType)ws);
                        nr = xml_xsd10_validate_length_facet_whtsp(nf, (XmlXsd10ValType)valType, v, withval ? nv : NULL, &nlen, (XmlXsd10WhitespaceValueType)ws);
                        check_int("length-facet-ws", "facet", vdesc, lr, nr);
                        check_int("length-facet-ws-len", "facet", vdesc, (long)llen, (long)nlen);

                        if (is_length_facet(ft) || ft == XML_SCHEMA_FACET_ENUMERATION)
                        {
                            unsigned long actual = rnd() % 40, lexp = 99, nexp = 99;
                            lr = xmlSchemaValidateListSimpleTypeFacet(lf, (const xmlChar*)v, actual, &lexp);
                            nr = xml_xsd10_validate_list_simple_type_facet(nf, v, actual, &nexp);
                            check_int("list-facet", "facet", vdesc, lr, nr);
                            check_int("list-facet-expected", "facet", vdesc, (long)lexp, (long)nexp);
                        }
                    }
                }
                xmlSchemaFreeFacet(lf);
                xml_xsd10_free_facet(nf);
                free(lvalue);
                free(nvalue);
            }
        }
    }
}

// Types //////////////////////////////////////////////////////////

static void check_types(void)
{
    for (int t = 0; t < NUM_TYPES; t++)
    {
        const char *tn = type_names_[t];
        ltypes_[t] = xmlSchemaGetPredefinedType((const xmlChar*)tn, (const xmlChar*)XSD_NS);
        ntypes_[t] = xml_xsd10_get_predefined_type(tn, XSD_NS);
        if (!ltypes_[t] || !ntypes_[t])
        {
            report("type-missing", tn, "", ltypes_[t] ? "found" : "NULL", ntypes_[t] ? "found" : "NULL");
            exit(1);
        }
        xmlSchemaTypePtr l = ltypes_[t];
        XmlXsd10Type *n = ntypes_[t];
        check_str("type-name", tn, "", (const char*)l->name, n->name);
        check_str("type-ns", tn, "", (const char*)l->targetNamespace, n->targetNamespace);
        check_int("type-type", tn, "", l->type, n->type);
        check_int("type-flags", tn, "", l->flags, n->flags);
        check_int("type-builtin", tn, "", l->builtInType, n->builtInType);
        check_int("type-content", tn, "", l->contentType, n->contentType);
        check_str("type-base", tn, "", l->baseType ? (const char*)l->baseType->name : NULL, n->baseType ? n->baseType->name : NULL);
        check_int("type-has-subtypes", tn, "", l->subtypes != NULL, n->subtypes != NULL);
        if (l->subtypes && l->builtInType != XML_SCHEMAS_ANYTYPE)
            check_str("type-subtypes", tn, "", (const char*)l->subtypes->name, n->subtypes ? n->subtypes->name : NULL);
        check_int("type-wildcard", tn, "", l->attributeWildcard != NULL, n->attributeWildcard != NULL);
        if (l->attributeWildcard && n->attributeWildcard)
        {
            check_int("type-wildcard-any", tn, "", l->attributeWildcard->any, n->attributeWildcard->any);
            check_int("type-wildcard-pc", tn, "", l->attributeWildcard->processContents, n->attributeWildcard->processContents);
        }
        if (l->builtInType == XML_SCHEMAS_ANYTYPE)
        {
            // particle -> sequence -> particle -> wildcard (libxml2 has the same private structs)
            XmlXsd10Particle *lp = (XmlXsd10Particle*)l->subtypes;
            XmlXsd10Particle *np = (XmlXsd10Particle*)n->subtypes;
            check_int("any-p1", tn, "", lp->type * 1000 + lp->minOccurs * 10 + lp->maxOccurs, np->type * 1000 + np->minOccurs * 10 + np->maxOccurs);
            check_int("any-seq", tn, "", lp->children->type, np->children->type);
            XmlXsd10Particle *lp2 = (XmlXsd10Particle*)lp->children->children;
            XmlXsd10Particle *np2 = (XmlXsd10Particle*)np->children->children;
            check_int("any-p2", tn, "", lp2->minOccurs * 10000000000L + lp2->maxOccurs, np2->minOccurs * 10000000000L + np2->maxOccurs);
            xmlSchemaWildcardPtr lw = (xmlSchemaWildcardPtr)lp2->children;
            XmlXsd10Wildcard *nw = (XmlXsd10Wildcard*)np2->children;
            check_int("any-wild", tn, "", lw->type * 100 + lw->any * 10 + lw->processContents, nw->type * 100 + nw->any * 10 + nw->processContents);
        }
        check_int("type-has-facets", tn, "", l->facets != NULL, n->facets != NULL);
        if (l->facets && n->facets)
        {
            check_int("type-facet-type", tn, "", l->facets->type, n->facets->type);
            check_int("type-facet-ulong", tn, "", (long)xmlSchemaGetFacetValueAsULong(l->facets), (long)xml_xsd10_get_facet_value_as_ulong(n->facets));
            compare_vals(tn, "(facet)", l->facets->val, n->facets->val);
        }
        xmlSchemaTypePtr li = xmlSchemaGetBuiltInListSimpleTypeItemType(l);
        XmlXsd10Type *ni = xml_xsd10_get_built_in_list_simple_type_item_type(n);
        check_str("item-type", tn, "", li ? (const char*)li->name : NULL, ni ? ni->name : NULL);
        for (int ft = XML_SCHEMA_FACET_MININCLUSIVE; ft <= XML_SCHEMA_FACET_MINLENGTH; ft++)
            check_int("is-builtin-facet", tn, facet_name(ft), xmlSchemaIsBuiltInTypeFacet(l, ft), xml_xsd10_is_built_in_type_facet(n, ft));
        xmlSchemaTypePtr lb = xmlSchemaGetBuiltInType((xmlSchemaValType)l->builtInType);
        XmlXsd10Type *nb = xml_xsd10_get_built_in_type((XmlXsd10ValType)n->builtInType);
        check_str("builtin-by-id", tn, "", lb ? (const char*)lb->name : NULL, nb ? nb->name : NULL);
    }
    for (int i = -1; i < 50; i++)
    {
        xmlSchemaTypePtr lb = xmlSchemaGetBuiltInType((xmlSchemaValType)i);
        XmlXsd10Type *nb = xml_xsd10_get_built_in_type((XmlXsd10ValType)i);
        char id[16];
        snprintf(id, sizeof(id), "%d", i);
        check_str("builtin-by-id", "id", id, lb ? (const char*)lb->name : NULL, nb ? nb->name : NULL);
    }
    check_int("predefined-wrong-ns", "string", "", xmlSchemaGetPredefinedType((const xmlChar*)"string", (const xmlChar*)"urn:x") != NULL,
              xml_xsd10_get_predefined_type("string", "urn:x") != NULL);
    check_int("predefined-null-ns", "string", "", xmlSchemaGetPredefinedType((const xmlChar*)"string", NULL) != NULL,
              xml_xsd10_get_predefined_type("string", NULL) != NULL);
    check_int("predefined-unknown", "foo", "", xmlSchemaGetPredefinedType((const xmlChar*)"foo", (const xmlChar*)XSD_NS) != NULL,
              xml_xsd10_get_predefined_type("foo", XSD_NS) != NULL);
}

static void check_whitespace_functions(void)
{
    for (size_t i = 0; i < corpus_len_; i++)
    {
        xmlChar *l = xmlSchemaWhiteSpaceReplace((const xmlChar*)corpus_[i]);
        char *n = xml_xsd10_white_space_replace(corpus_[i]);
        check_str("ws-replace", "-", corpus_[i], (const char*)l, n);
        xmlFree(l); free(n);
        l = xmlSchemaCollapseString((const xmlChar*)corpus_[i]);
        n = xml_xsd10_collapse_string(corpus_[i]);
        check_str("ws-collapse", "-", corpus_[i], (const char*)l, n);
        xmlFree(l); free(n);
    }
}

static void check_misc(void)
{
    // xmlSchemaNewStringValue/NOTATIONValue/QNameValue, ValueAppend/GetNext.
    xmlSchemaValPtr l1 = xmlSchemaNewStringValue(XML_SCHEMAS_STRING, xmlStrdup((const xmlChar*)"a b"));
    XmlXsd10Val *n1 = xml_xsd10_new_string_value(XML_XSD10_SCHEMAS_STRING, strdup("a b"));
    compare_vals("newstring", "a b", l1, n1);
    check_int("newstring-token", "-", "", xmlSchemaNewStringValue(XML_SCHEMAS_TOKEN, NULL) != NULL,
              xml_xsd10_new_string_value(XML_XSD10_SCHEMAS_TOKEN, NULL) != NULL);
    xmlSchemaValPtr l2 = xmlSchemaNewNOTATIONValue(xmlStrdup((const xmlChar*)"n"), xmlStrdup((const xmlChar*)"urn:n"));
    XmlXsd10Val *n2 = xml_xsd10_new_notation_value(strdup("n"), strdup("urn:n"));
    compare_vals("newnotation", "n", l2, n2);
    xmlSchemaValPtr l3 = xmlSchemaNewQNameValue(xmlStrdup((const xmlChar*)"urn:n"), xmlStrdup((const xmlChar*)"n"));
    XmlXsd10Val *n3 = xml_xsd10_new_qname_value(strdup("urn:n"), strdup("n"));
    compare_vals("newqname", "n", l3, n3);
    check_int("qname-notation", "-", "", xmlSchemaCompareValues(l2, l3), xml_xsd10_compare_values(n2, n3));
    xmlSchemaValPtr l4 = xmlSchemaNewQNameValue(NULL, xmlStrdup((const xmlChar*)"n"));
    XmlXsd10Val *n4 = xml_xsd10_new_qname_value(NULL, strdup("n"));
    compare_vals("newqname-nons", "n", l4, n4);
    check_int("qname-qname", "-", "", xmlSchemaCompareValues(l4, l3), xml_xsd10_compare_values(n4, n3));
    check_int("append", "-", "", xmlSchemaValueAppend(l1, l4), xml_xsd10_value_append(n1, n4));
    check_int("append-null", "-", "", xmlSchemaValueAppend(NULL, l4), xml_xsd10_value_append(NULL, n4));
    check_int("get-next", "-", "", xmlSchemaValueGetNext(l1) == l4, xml_xsd10_value_get_next(n1) == n4);
    xmlSchemaValPtr lc = xmlSchemaCopyValue(l1);
    XmlXsd10Val *nc = xml_xsd10_copy_value(n1);
    check_int("copy-list", "-", "", lc && xmlSchemaValueGetNext(lc) != NULL, nc && xml_xsd10_value_get_next(nc) != NULL);
    xmlSchemaFreeValue(lc);
    xml_xsd10_free_value(nc);
    xmlSchemaFreeValue(l1); // frees l4 too
    xml_xsd10_free_value(n1);
    xmlSchemaFreeValue(l2);
    xml_xsd10_free_value(n2);
    xmlSchemaFreeValue(l3);
    xml_xsd10_free_value(n3);
    check_int("compare-null", "-", "", xmlSchemaCompareValues(NULL, NULL), xml_xsd10_compare_values(NULL, NULL));
    check_int("valtype-null", "-", "", xmlSchemaGetValType(NULL), xml_xsd10_get_val_type(NULL));
    const xmlChar *lcv = NULL;
    const char *ncv = NULL;
    check_int("canon-null", "-", "", xmlSchemaGetCanonValue(NULL, &lcv), xml_xsd10_get_canon_value(NULL, &ncv));
    check_int("validate-null-type", "-", "", xmlSchemaValidatePredefinedType(NULL, (const xmlChar*)"a", NULL),
              xml_xsd10_validate_predefined_type(NULL, "a", NULL));
    check_int("check-facet-null", "-", "", xmlSchemaCheckFacet(NULL, NULL, NULL, NULL), xml_xsd10_check_facet(NULL, NULL, NULL));
    check_int("validate-facet-null", "-", "", xmlSchemaValidateFacet(NULL, NULL, NULL, NULL), xml_xsd10_validate_facet(NULL, NULL, NULL, NULL));
}

// Nodes //////////////////////////////////////////////////////////

static const char *node_doc_ =
    "<?xml version='1.0'?>\n"
    "<!DOCTYPE r [\n"
    "<!NOTATION gif SYSTEM 'image/gif'>\n"
    "<!NOTATION p:jpg SYSTEM 'image/jpeg'>\n"
    "<!NOTATION png PUBLIC 'png'>\n"
    "<!ENTITY pic SYSTEM 'pic.gif' NDATA gif>\n"
    "<!ENTITY pic2 SYSTEM 'pic2.gif' NDATA gif>\n"
    "<!ENTITY ext SYSTEM 'ext.xml'>\n"
    "<!ENTITY txt 'text'>\n"
    "]>\n"
    "<r xmlns='urn:default' xmlns:p='urn:p' xmlns:q='urn:q' a='1' b='2' p:c='3'>"
    "<c xmlns='' xmlns:p='urn:p2' x='1' y='2' z='3'><d w='1' v='2'/></c>"
    "<e xmlns:xs='http://www.w3.org/2001/XMLSchema' u='1'/>"
    "</r>";

static const char *qname_values_[] = {
    "a", "p:a", "q:b", "x:y", "xml:lang", "xs:string", " p:a ", ":a", "a:", "a:b:c", "1a", "",
    "gif", "p:jpg", "q:jpg", "png", " gif ", "p:gif", "pic", "pic2", "pic pic2", " pic ", "ext", "txt",
    "pic ext", "pic pic", "id1", "id2", " id1 ", "id1 id2", "a b c", "_x", "\xc3\xa9", "xmlns:a", "p: a",
};
#define NUM_QNAME_VALUES ((int)(sizeof(qname_values_)/sizeof(qname_values_[0])))

static const char *node_types_[] = { "QName", "NOTATION", "ENTITY", "ENTITIES", "NCName", "IDREF", "IDREFS", "NMTOKENS", "string" };
#define NUM_NODE_TYPES ((int)(sizeof(node_types_)/sizeof(node_types_[0])))

static int type_index(const char *name)
{
    for (int t = 0; t < NUM_TYPES; t++) if (!strcmp(type_names_[t], name)) return t;
    return -1;
}

static int has_colon(const char *s)
{
    return strchr(s, ':') != NULL;
}

/** Does value (blank trimmed) name a notation of node_doc_ (libxml2 leaks the computed name
    of a prefixed undeclared notation, so those are only validated without a value)? */
static int is_declared_notation(const char *s)
{
    while (*s == ' ') s++;
    size_t n = strlen(s);
    while (n > 0 && s[n-1] == ' ') n--;
    return (n == 5 && !strncmp(s, "p:jpg", 5));
}

static void check_node_values(xmlNodePtr lnode, XMQNode *nnode, const char *where, XmlXsd10Ids *ids)
{
    (void)ids;
    for (int k = 0; k < NUM_NODE_TYPES; k++)
    {
        int t = type_index(node_types_[k]);
        for (int i = 0; i < NUM_QNAME_VALUES; i++)
        {
            const char *v = qname_values_[i];
            char desc[256];
            snprintf(desc, sizeof(desc), "%s at %s", v, where);
            int wantval = !(t == type_index("NOTATION") && has_colon(v) && !is_declared_notation(v));
            for (int mode = 0; mode < 2; mode++)
            {
                xmlSchemaValPtr l = NULL;
                XmlXsd10Val *n = NULL;
                int lr, nr;
                if (mode == 0)
                {
                    lr = xmlSchemaValPredefTypeNode(ltypes_[t], (const xmlChar*)v, wantval ? &l : NULL, lnode);
                    nr = xml_xsd10_val_predef_type_node(ntypes_[t], v, wantval ? &n : NULL, nnode);
                }
                else
                {
                    lr = xmlSchemaValPredefTypeNodeNoNorm(ltypes_[t], (const xmlChar*)v, wantval ? &l : NULL, lnode);
                    nr = xml_xsd10_val_predef_type_node_no_norm(ntypes_[t], v, wantval ? &n : NULL, nnode);
                }
                check_int(mode ? "node-nonorm" : "node", type_names_[t], desc, lr, nr);
                compare_vals(type_names_[t], desc, l, n);
                if (l && n && (t == type_index("QName") || t == type_index("NOTATION")))
                {
                    // The uri is only visible through comparisons with a known QName.
                    xmlSchemaValPtr lq = xmlSchemaNewQNameValue(xmlStrdup((const xmlChar*)"urn:p"), xmlStrdup((const xmlChar*)"a"));
                    XmlXsd10Val *nq = xml_xsd10_new_qname_value(strdup("urn:p"), strdup("a"));
                    check_int("node-qname-compare", type_names_[t], desc, xmlSchemaCompareValues(l, lq), xml_xsd10_compare_values(n, nq));
                    xmlSchemaFreeValue(lq);
                    xml_xsd10_free_value(nq);
                }
                xmlSchemaFreeValue(l);
                xml_xsd10_free_value(n);
            }
        }
    }
}

/** ID values on attributes: the libxml2 document and the native ID table must agree on duplicates. */
static void check_ids(xmlNodePtr lroot, XMQNode *nroot, XmlXsd10Ids *ids)
{
    static const char *idvals[] = { "id1", "id2", " id1 ", "id3", "1bad", "id2", "id4", "", "id5 ", "id1" };
    int t = type_index("ID");
    int k = 0;
    for (xmlNodePtr le = lroot; le; le = le->children ? le->children : NULL)
    {
        (void)le;
        break;
    }
    // Walk the attributes of r, c and d in document order.
    xmlNodePtr lelems[3];
    XMQNode *nelems[3];
    lelems[0] = lroot;
    nelems[0] = nroot;
    lelems[1] = lroot->children;
    nelems[1] = xml_node_first_child(nroot);
    lelems[2] = lelems[1]->children;
    nelems[2] = xml_node_first_child(nelems[1]);
    for (int round = 0; round < 2; round++)
    {
        for (int e = 0; e < 3; e++)
        {
            xmlAttrPtr la = lelems[e]->properties;
            XMQAttr *na = xml_node_first_attr(nelems[e]);
            for (; la && na; la = la->next, na = xml_attr_next(na))
            {
                const char *v = idvals[k++ % (int)(sizeof(idvals)/sizeof(idvals[0]))];
                char desc[256];
                snprintf(desc, sizeof(desc), "%s on %s/@%s round %d", v, (const char*)lelems[e]->name, (const char*)la->name, round);
                xmlSchemaValPtr l = NULL;
                XmlXsd10Val *n = NULL;
                int lr = xmlSchemaValPredefTypeNode(ltypes_[t], (const xmlChar*)v, &l, (xmlNodePtr)la);
                int nr = xml_xsd10_val_predef_type_node_ids(ntypes_[t], v, &n, xml_attr_node(na), ids);
                check_int("id", "ID", desc, lr, nr);
                compare_vals("ID", desc, l, n);
                xmlSchemaFreeValue(l);
                xml_xsd10_free_value(n);
                check_int("id-atype", "ID", desc, la->atype == XML_ATTRIBUTE_ID,
                          xml_xsd10_ids_attr_type(ids, xml_attr_node(na)) == XML_XSD10_ATTRIBUTE_ID);
                // IDREF on the same attributes (after their ID round).
                if (round == 1)
                {
                    lr = xmlSchemaValPredefTypeNode(ltypes_[type_index("IDREFS")], (const xmlChar*)"id1 id2", NULL, (xmlNodePtr)la);
                    nr = xml_xsd10_val_predef_type_node_ids(ntypes_[type_index("IDREFS")], "id1 id2", NULL, xml_attr_node(na), ids);
                    check_int("idrefs", "IDREFS", desc, lr, nr);
                    check_int("idrefs-atype", "IDREFS", desc, la->atype,
                              xml_xsd10_ids_attr_type(ids, xml_attr_node(na)));
                }
            }
            check_int("id-attr-count", "ID", (const char*)lelems[e]->name, la != NULL, na != NULL);
        }
    }
    // The ids found by libxml2 are in the native table.
    static const char *lookups[] = { "id1", "id2", "id3", "id4", "id5", "1bad", "" };
    for (size_t i = 0; i < sizeof(lookups)/sizeof(lookups[0]); i++)
    {
        xmlAttrPtr la = xmlGetID(lroot->doc, (const xmlChar*)lookups[i]);
        XMQNode *na = xml_xsd10_ids_lookup(ids, lookups[i]);
        check_str("id-lookup", "ID", lookups[i], la ? (const char*)la->name : NULL, na ? xml_node_name(na) : NULL);
    }
}

static void check_nodes(void)
{
    xmlDocPtr doc = xmlReadMemory(node_doc_, (int)strlen(node_doc_), "node.xml", NULL, XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    XmlParseError err;
    memset(&err, 0, sizeof(err));
    XmlDom *dom = xml_parse_buffer(node_doc_, node_doc_ + strlen(node_doc_), &settings, &err);
    if (!doc || !dom)
    {
        printf("Could not parse the node test document (libxml2 %s, native %s)\n", doc ? "ok" : "failed", dom ? "ok" : err.message);
        diff_++;
        xml_parse_error_free(&err);
        if (doc) xmlFreeDoc(doc);
        if (dom) xml_dom_free(dom);
        return;
    }
    XmlXsd10Ids *ids = xml_xsd10_ids_new();
    xmlNodePtr lroot = xmlDocGetRootElement(doc);
    XMQNode *nroot = xml_dom_root_element(dom);
    // Elements and their attributes, walked in parallel.
    xmlNodePtr lstack[16];
    XMQNode *nstack[16];
    int sp = 0;
    lstack[sp] = lroot; nstack[sp] = nroot; sp++;
    while (sp > 0)
    {
        sp--;
        xmlNodePtr le = lstack[sp];
        XMQNode *ne = nstack[sp];
        char where[128];
        snprintf(where, sizeof(where), "<%s>", (const char*)le->name);
        check_str("node-name", "-", where, (const char*)le->name, xml_node_name(ne));
        check_node_values(le, ne, where, ids);
        xmlAttrPtr la = le->properties;
        XMQAttr *na = xml_node_first_attr(ne);
        for (; la && na; la = la->next, na = xml_attr_next(na))
        {
            snprintf(where, sizeof(where), "<%s>/@%s", (const char*)le->name, (const char*)la->name);
            check_node_values((xmlNodePtr)la, xml_attr_node(na), where, ids);
        }
        xmlNodePtr lc = le->children;
        XMQNode *nc = xml_node_first_child(ne);
        for (; lc && nc; lc = lc->next, nc = xml_node_next(nc))
        {
            if (lc->type == XML_ELEMENT_NODE && sp < 16)
            {
                lstack[sp] = lc; nstack[sp] = nc; sp++;
            }
        }
    }
    check_ids(lroot, nroot, ids);
    xml_xsd10_ids_free(ids);
    xmlFreeDoc(doc);
    xml_dom_free(dom);
}

int main(int argc, char **argv)
{
    int random_count = 400;
    int i = 1;
    for (; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v")) { verbose_++; max_shown_ = 1000000; }
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
        {
            unsigned long long seed = 1;
            sscanf(argv[++i], "%d:%llu", &random_count, &seed);
            rng_ = seed;
        }
        else break;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    xmlSetGenericErrorFunc(NULL, no_errors);
    xmlSchemaInitTypes();
    xml_xsd10_init_types();

    for (size_t k = 0; k < sizeof(edge_cases_)/sizeof(edge_cases_[0]); k++) corpus_add(edge_cases_[k]);
    int files = 0;
    for (; i < argc; i++) { harvest_file(argv[i]); files++; }
    add_random(random_count);
    corpus_dedupe();

    check_types();
    long before = same_ + diff_;
    check_whitespace_functions();
    check_misc();
    long validations = 0;
    for (int t = 0; t < NUM_TYPES; t++)
    {
        for (size_t k = 0; k < corpus_len_; k++)
        {
            check_value(t, corpus_[k], 1);
            validations++;
        }
    }
    printf("xml_xsd10_types_diff: %d files, %zu corpus values, %ld type/value validations, %zu computed values\n",
           files, corpus_len_, validations, pool_len_);
    check_compares_systematic(120, 25);
    check_compares(200000);
    check_facets(30, 40);
    check_nodes();
    (void)before;

    for (size_t k = 0; k < pool_len_; k++)
    {
        xmlSchemaFreeValue(pool_[k].lval);
        xml_xsd10_free_value(pool_[k].nval);
    }
    free(pool_);
    for (size_t k = 0; k < corpus_len_; k++) free(corpus_[k]);
    free(corpus_);
    xmlCleanupParser();

    printf("xml_xsd10_types_diff: %ld same, %ld different\n", same_, diff_);
    return diff_ ? 1 : 0;
}
