/* This file is ported from libxml2 (parser.c, parserInternals.c, SAX2.c and
   error.c), through the java port src/main/java/org/libxmq/imp/xml/XmlParser.java.
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef BUILDING_DIST_XMQ

#include"xml_parser.h"
#include"xml_save.h"
#include"xml_uri.h"
#include"hashmap.h"
#include"membuffer.h"

#include<assert.h>
#include<setjmp.h>
#include<stdarg.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_PARSER_MODULE

// Growable byte buffer owned by the parser, freed at the end of the parse (also on errors).
typedef struct
{
    char *b;
    size_t len;
    size_t cap;
} XmlPBuf;

typedef struct
{
    const char *s;
    size_t len;
} XmlSlice;

typedef struct
{
    XmlSlice prefix; // s == NULL if no prefix.
    XmlSlice local;
} XmlQName;

typedef struct
{
    XMQNode *element;
    char *qname;      // Malloced "prefix:local" as written.
    size_t line;
    int space;        // xml:space state: -1 default, 1 preserve, 0 default declared, -2 mixed content seen.
    bool prefixed;    // The name has a namespace prefix.
    size_t ns_open;   // 1 + the index of the nearest open element, this or an enclosing one, that declares namespaces, 0 if none.
} XmlOpen;

typedef struct
{
    bool entity;      // An entity reference &name; otherwise text.
    size_t off;       // Into XmlParser.attval.
    size_t len;
} XmlAttPart;

typedef struct
{
    XmlQName name;
    size_t first_part;
    size_t num_parts;
    size_t pos;       // Start of the attribute name.
    size_t end;       // After the attribute value, where libxml2 reports problems with the value.
    bool ns_skip;     // A namespace declaration that is not kept (an error was reported).
} XmlRawAttr;

typedef struct
{
    char *element;    // Element qname.
    char *attr;       // Attribute qname, without dtd_attr only xmlns and xmlns:x are kept, libxml2 adds only those.
    char *value;
} XmlAttDefault;

typedef struct
{
    const XmlParseSettings *settings;
    const char *buf;  // UTF-8, \r normalized, valid xml chars.
    size_t len;
    size_t i;
    char *owned;      // Buffer owned by the parser (if conversion/normalization was needed).
    unsigned char *crmark; // If the input had \r: 1 where a \n came from \r\n, 2 where it came from a lone \r.
    size_t input_len; // Original input length, for the amplification limit.

    XmlDom *dom;
    XMQNode *node;    // Current parent: the document or an element.
    XMQNode *dtd;
    bool in_subset;
    bool has_external_subset;
    bool has_pe_refs;

    XmlOpen *stack;
    size_t depth;
    size_t stack_cap;
    size_t ns_base_depth; // The open elements from this depth are the ones of the chunk being parsed, see xml_lookup_ns.

    XmlPBuf text;     // Pending character data for the current parent.
    bool has_text;
    size_t text_line;

    XmlPBuf attval;   // Attribute value parts of the current start tag.
    XmlAttPart *parts;
    size_t num_parts;
    size_t value_first_part; // First part of the attribute value being parsed, parts never merge across values.
    size_t parts_cap;
    XmlRawAttr *attrs;
    size_t num_attrs;
    size_t attrs_cap;

    char *cm_msg;       // A message being reported by xml_cm_error.
    // The first character of the input that is not an xml char. Like libxml2 the parser reports
    // it where it finds it, in the words of the construct it is in.
    bool has_bad;
    size_t bad_pos;
    int bad_cp;
    bool io_error_at_end; // The input ends with an incomplete UTF-16 code unit.
    bool qname_quiet;     // Do not report malformed QNames (end tags report them themselves).
    bool qname_malformed; // The last QName parsed was malformed.
    bool in_int_subset; // Parsing the markup declarations of the internal subset.
    XmlPBuf tmp[6];   // Scratch buffers for zero terminated copies.

    HashMap *entities;      // name -> XMQNode* entity declaration (general).
    HashMap *pentities;     // name -> XMQNode* entity declaration (parameter).
    HashMap *element_decls; // name -> XMQNode* element declaration.
    HashMap *attr_types;    // "element\1attr" -> (void*)1 for CDATA, (void*)2 for other types (first declaration wins).
    XmlAttDefault *defaults;
    size_t num_defaults;
    size_t defaults_cap;

    size_t line_pos;  // Cache for line numbers.
    size_t line_no;

    XmlParseError *error;
    jmp_buf on_error;
    const char *fatal_kind; // The kind of the next fatal message, NULL for "parser error".

    // Used by xml_expand_entities.
    bool expanding;
    XMQNode **active;      // The entity declarations being expanded, for loop detection.
    size_t num_active;
    size_t active_cap;
    size_t expanded_size;  // Total size of the replacement texts copied so far.
    XMQNode *container;    // The outermost temporary container linked into the tree.
    XmlPBuf attr_out;      // The expanded attribute value being built.
    const char *only;      // Expand only references to this entity (but everything inside it).
} XmlParser;

// xml_parser_fatal never returns, it longjmps out of the parse (and the compilers then know
// that the variables after it are set).
#if defined(__GNUC__) || defined(__clang__)
#define XML_PARSER_NORETURN __attribute__((noreturn))
#else
#define XML_PARSER_NORETURN
#endif
static void xml_parser_fatal(XmlParser *p, size_t pos, const char *fmt, ...) XML_PARSER_NORETURN;
static int xml_utf8_decode(const unsigned char *s, size_t avail, size_t *len);

// Buffers ////////////////////////////////////////////////////////

static void xml_pbuf_reserve(XmlPBuf *b, size_t add)
{
    if (b->len + add + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap * 2 : 64;
    while (nc < b->len + add + 1) nc *= 2;
    b->b = (char*)realloc(b->b, nc);
    b->cap = nc;
}

static void xml_pbuf_add(XmlPBuf *b, const char *s, size_t len)
{
    xml_pbuf_reserve(b, len);
    memcpy(b->b + b->len, s, len);
    b->len += len;
    b->b[b->len] = 0;
}

static void xml_pbuf_addc(XmlPBuf *b, char c)
{
    xml_pbuf_add(b, &c, 1);
}

static void xml_pbuf_add_cp(XmlPBuf *b, int cp)
{
    char u[4];
    size_t n;
    if (cp < 0x80) { u[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { u[0] = (char)(0xC0 | (cp >> 6)); u[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000) { u[0] = (char)(0xE0 | (cp >> 12)); u[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else { u[0] = (char)(0xF0 | (cp >> 18)); u[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    xml_pbuf_add(b, u, n);
}

static void xml_pbuf_clear(XmlPBuf *b)
{
    b->len = 0;
    if (b->b) b->b[0] = 0;
}

static void xml_pbuf_free(XmlPBuf *b)
{
    free(b->b);
    b->b = NULL;
    b->len = b->cap = 0;
}

/** A zero terminated copy of s in scratch slot n, valid until the slot is reused. */
static const char *xml_tmp(XmlParser *p, int n, const char *s, size_t len)
{
    xml_pbuf_clear(&p->tmp[n]);
    xml_pbuf_add(&p->tmp[n], s, len);
    return p->tmp[n].b;
}

static const char *xml_tmp_slice(XmlParser *p, int n, XmlSlice sl)
{
    return xml_tmp(p, n, sl.s ? sl.s : "", sl.len);
}

/** "prefix:local" in scratch slot n. */
static const char *xml_tmp_qname(XmlParser *p, int n, XmlQName q)
{
    xml_pbuf_clear(&p->tmp[n]);
    if (q.prefix.s)
    {
        xml_pbuf_add(&p->tmp[n], q.prefix.s, q.prefix.len);
        xml_pbuf_addc(&p->tmp[n], ':');
    }
    xml_pbuf_add(&p->tmp[n], q.local.s, q.local.len);
    return p->tmp[n].b;
}

static char *xml_parser_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char*)malloc(n+1);
    memcpy(r, s, n+1);
    return r;
}

static bool xml_slice_eq(XmlSlice a, const char *s)
{
    size_t n = strlen(s);
    return a.s && a.len == n && !memcmp(a.s, s, n);
}

static bool xml_slice_eq_slice(XmlSlice a, XmlSlice b)
{
    if (!a.s || !b.s) return a.s == b.s;
    return a.len == b.len && !memcmp(a.s, b.s, a.len);
}

// Lines and messages /////////////////////////////////////////////

static size_t xml_parser_line_at(XmlParser *p, size_t pos)
{
    if (pos > p->len) pos = p->len;
    if (pos < p->line_pos)
    {
        p->line_pos = 0;
        p->line_no = 1;
    }
    for (size_t k = p->line_pos; k < pos; ++k)
    {
        if (p->buf[k] == '\n') p->line_no++;
    }
    p->line_pos = pos;
    return p->line_no;
}

char *xml_parser_format_message(const char *source, size_t line, const char *kind, const char *msg,
                                const char *s, size_t len, size_t pos, size_t *out_col)
{
    MemBuffer *mb = new_membuffer();
    membuffer_printf(mb, "%s:%zu: %s : %s", source, line, kind, msg);
    size_t ml = strlen(msg);
    if (ml == 0 || msg[ml-1] != '\n') membuffer_append_char(mb, '\n');

    if (pos > len) pos = len;
    // Port of libxml2 2.16 xmlParserInputGetWindow and xmlParserPrintFileContextInternal:
    // at most 80 bytes of the line around pos, never splitting a UTF-8 character.
    const unsigned char *u = (const unsigned char*)s;
    const size_t size = 80;
#define XML_AT(k) ((k) < len ? u[(k)] : 0)
    size_t cur = pos;
    // Skip backwards over any end of lines.
    while (cur > 0 && (XML_AT(cur) == '\n' || XML_AT(cur) == '\r')) cur--;
    size_t n = 0;
    // Search backwards for the beginning of the line (to max size).
    while (n < size && cur > 0 && XML_AT(cur) != '\n' && XML_AT(cur) != '\r')
    {
        cur--;
        n++;
    }
    if (n > 0 && (XML_AT(cur) == '\n' || XML_AT(cur) == '\r'))
    {
        cur++;
    }
    else
    {
        // Skip over continuation bytes.
        while (cur < pos && (XML_AT(cur) & 0xC0) == 0x80) cur++;
    }
    size_t col = pos - cur;
    // Search forward for the end of the line (to max size), whole characters only.
    size_t start = cur;
    n = 0;
    while (XML_AT(cur) != 0 && XML_AT(cur) != '\n' && XML_AT(cur) != '\r')
    {
        size_t l = 0;
        int c = xml_utf8_decode(u + cur, len - cur, &l);
        if (c < 0 || n + l > size) break;
        cur += l;
        n += l;
    }
#undef XML_AT
    // The column can only point to the end of the buffer if there is space for the marker.
    if (col >= n) col = n < size ? n : size - 1;
    membuffer_append_region(mb, s + start, s + start + n);
    membuffer_append_char(mb, '\n');
    for (size_t k = 0; k < col; ++k)
    {
        membuffer_append_char(mb, s[start + k] == '\t' ? '\t' : ' ');
    }
    membuffer_append(mb, "^\n");
    membuffer_append_null(mb);

    if (out_col) *out_col = col + 1;
    return free_membuffer_but_return_trimmed_content(mb);
}

/** Format a message with source context like libxml2 xmlParserPrintFileContextInternal. */
static char *xml_parser_format(XmlParser *p, const char *kind, const char *msg, size_t pos, size_t *out_line, size_t *out_col)
{
    size_t line = xml_parser_line_at(p, pos);
    if (out_line) *out_line = line;
    const char *source = p->settings->source_name ? p->settings->source_name : "noname.xml";
    return xml_parser_format_message(source, line, kind, msg, p->buf, p->len, pos, out_col);
}

/** The maximum size of a formatted message, like libxml2 MAX_ERR_MSG_SIZE. */
#define XML_MAX_ERR_MSG_SIZE 64000

/**
    Port of libxml2 xmlStrVASPrintf: format into a malloced string of at most max_size bytes
    (including the terminating zero), a truncated result does not end with a partial UTF-8 sequence.
*/
static char *xml_parser_vasprintf(int max_size, const char *fmt, va_list ap)
{
    va_list copy;
    va_copy(copy, ap);
    int res = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (res < 0) res = 0;
    int size = res < max_size ? res + 1 : max_size;
    unsigned char *buf = (unsigned char*)malloc(size);
    vsnprintf((char*)buf, size, fmt, ap);
    if (res >= max_size)
    {
        int i = size - 1;
        while (i > 0)
        {
            // Break after ASCII.
            if (buf[i-1] < 0x80) break;
            i -= 1;
            // Break before non-ASCII.
            if (buf[i] >= 0xc0) break;
        }
        buf[i] = 0;
    }
    return (char*)buf;
}

static void xml_parser_message(XmlParser *p, const char *kind, size_t pos, const char *fmt, ...)
{
    if (!p->settings->message_callback) return;
    va_list ap;
    va_start(ap, fmt);
    char *msg = xml_parser_vasprintf(XML_MAX_ERR_MSG_SIZE, fmt, ap);
    va_end(ap);
    char *m = xml_parser_format(p, kind, msg, pos, NULL, NULL);
    free(msg);
    p->settings->message_callback(p->settings->message_user_data, m);
    free(m);
}

static void xml_parser_fatal(XmlParser *p, size_t pos, const char *fmt, ...)
{
    char *msg;
    if (p->has_bad && pos > p->bad_pos)
    {
        // The invalid character was passed by a construct that does not check for it.
        p->has_bad = false;
        p->fatal_kind = NULL;
        pos = p->bad_pos;
        char m[64];
        snprintf(m, sizeof(m), "Char 0x%X out of allowed range", p->bad_cp);
        msg = xml_parser_strdup(m);
    }
    else
    {
        va_list ap;
        va_start(ap, fmt);
        msg = xml_parser_vasprintf(XML_MAX_ERR_MSG_SIZE, fmt, ap);
        va_end(ap);
    }
    if (p->error)
    {
        p->error->message = xml_parser_format(p, p->fatal_kind ? p->fatal_kind : "parser error", msg, pos,
                                              &p->error->line, &p->error->col);
        p->error->short_message = msg;
    }
    else
    {
        free(msg);
    }
    longjmp(p->on_error, 1);
}

// Character classes //////////////////////////////////////////////

static bool xml_is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool xml_is_name_start_char(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':'
        || (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF)
        || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D)
        || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF)
        || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

static bool xml_is_name_char(int c)
{
    return xml_is_name_start_char(c) || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == 0xB7
        || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
}

static bool xml_is_xml_char(int c)
{
    return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF)
        || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
}

bool xml_is_valid_char(int c)
{
    return xml_is_xml_char(c);
}

bool xml_is_valid_name(const char *name, bool allow_colon)
{
    const unsigned char *s = (const unsigned char*)name;
    size_t avail = strlen(name);
    if (avail == 0) return false;
    bool first = true;
    while (avail > 0)
    {
        size_t len = 0;
        int c = xml_utf8_decode(s, avail, &len);
        if (c < 0) return false;
        if (c == ':' && !allow_colon) return false;
        if (first ? !xml_is_name_start_char(c) : !xml_is_name_char(c)) return false;
        first = false;
        s += len;
        avail -= len;
    }
    return true;
}

/** Decode one utf8 code point. Returns -1 on invalid utf8. */
static int xml_utf8_decode(const unsigned char *s, size_t avail, size_t *len)
{
    unsigned int c = s[0];
    if (c < 0x80) { *len = 1; return (int)c; }
    if (c < 0xC2) return -1;
    if (c < 0xE0)
    {
        if (avail < 2 || (s[1] & 0xC0) != 0x80) return -1;
        *len = 2;
        return (int)(((c & 0x1F) << 6) | (s[1] & 0x3F));
    }
    if (c < 0xF0)
    {
        if (avail < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return -1;
        unsigned int v = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (v < 0x800 || (v >= 0xD800 && v < 0xE000)) return -1;
        *len = 3;
        return (int)v;
    }
    if (c < 0xF5)
    {
        if (avail < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return -1;
        unsigned int v = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (v < 0x10000 || v > 0x10FFFF) return -1;
        *len = 4;
        return (int)v;
    }
    return -1;
}

/** Code point at position (the input is already validated). */
static int xml_cp_at(XmlParser *p, size_t pos, size_t *len)
{
    if (pos >= p->len) { *len = 0; return 0; }
    int c = xml_utf8_decode((const unsigned char*)p->buf + pos, p->len - pos, len);
    assert(c >= 0);
    return c;
}

// Input preparation //////////////////////////////////////////////

static bool xml_ascii_ieq(const char *a, size_t alen, const char *b)
{
    size_t n = strlen(b);
    if (alen != n) return false;
    for (size_t k = 0; k < n; ++k)
    {
        char x = a[k], y = b[k];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return false;
    }
    return true;
}

/** Find the encoding="..." of a leading xml declaration in an ascii compatible buffer. */
static bool xml_find_declared_encoding(const char *s, size_t len, const char **enc, size_t *enc_len)
{
    if (len < 6 || memcmp(s, "<?xml", 5) || !xml_is_blank(s[5])) return false;
    size_t end = len < 1024 ? len : 1024;
    for (size_t k = 5; k + 1 < end; ++k)
    {
        if (s[k] == '?' && s[k+1] == '>') return false;
        if (k + 8 < end && !memcmp(s + k, "encoding", 8))
        {
            size_t j = k + 8;
            while (j < end && xml_is_blank(s[j])) j++;
            if (j >= end || s[j] != '=') return false;
            j++;
            while (j < end && xml_is_blank(s[j])) j++;
            if (j >= end || (s[j] != '"' && s[j] != '\'')) return false;
            char q = s[j++];
            size_t start = j;
            while (j < end && s[j] != q) j++;
            if (j >= end) return false;
            *enc = s + start;
            *enc_len = j - start;
            return true;
        }
    }
    return false;
}

static void xml_convert_utf16(XmlPBuf *out, const unsigned char *s, size_t len, bool big_endian, XmlParser *p)
{
    // Like libxml2 a trailing odd byte is an I/O error found after the decoded input is parsed.
    if (len % 2) p->io_error_at_end = true;
    for (size_t k = 0; k + 1 < len; k += 2)
    {
        unsigned int u = big_endian ? (unsigned)(s[k] << 8 | s[k+1]) : (unsigned)(s[k+1] << 8 | s[k]);
        if (u >= 0xD800 && u < 0xDC00)
        {
            if (k + 3 >= len) xml_parser_fatal(p, 0, "Input is not proper UTF-16, indicate encoding !");
            unsigned int l = big_endian ? (unsigned)(s[k+2] << 8 | s[k+3]) : (unsigned)(s[k+3] << 8 | s[k+2]);
            if (l < 0xDC00 || l >= 0xE000) xml_parser_fatal(p, 0, "Input is not proper UTF-16, indicate encoding !");
            u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00);
            k += 2;
        }
        else if (u >= 0xDC00 && u < 0xE000)
        {
            xml_parser_fatal(p, 0, "Input is not proper UTF-16, indicate encoding !");
        }
        xml_pbuf_add_cp(out, (int)u);
    }
}

/** End of line handling of p->buf: \r\n and \r become \n, p->crmark records where they were. */
static void xml_normalize_eol(XmlParser *p)
{
    free(p->crmark);
    p->crmark = NULL;
    if (memchr(p->buf, '\r', p->len))
    {
        char *n = (char*)malloc(p->len + 1);
        p->crmark = (unsigned char*)calloc(p->len + 1, 1);
        size_t o = 0;
        for (size_t k = 0; k < p->len; ++k)
        {
            char c = p->buf[k];
            if (c == '\r')
            {
                bool crlf = k + 1 < p->len && p->buf[k+1] == '\n';
                p->crmark[o] = crlf ? 1 : 2;
                n[o++] = '\n';
                if (crlf) k++;
            }
            else
            {
                n[o++] = c;
            }
        }
        n[o] = 0;
        free(p->owned);
        p->owned = n;
        p->buf = n;
        p->len = o;
    }
}

/**
    Convert the input to UTF-8 (UTF-16 by bom or by "<?" pattern, ISO-8859-1 by declaration),
    skip a UTF-8 bom, normalize \r\n and \r to \n, then check that the result is valid
    utf8 and contains only allowed xml characters.
*/
static void xml_parser_prepare_input(XmlParser *p, const char *start, const char *stop)
{
    const unsigned char *s = (const unsigned char*)start;
    size_t len = stop - start;
    // Convert into scratch slot 0, which is freed by the cleanup also if the conversion fails.
    XmlPBuf *conv = &p->tmp[0];
    xml_pbuf_clear(conv);
    bool converted = false;

    if (len >= 2 && ((s[0] == 0xFE && s[1] == 0xFF) || (s[0] == 0xFF && s[1] == 0xFE)))
    {
        xml_convert_utf16(conv, s + 2, len - 2, s[0] == 0xFE, p);
        converted = true;
    }
    else if (len >= 4 && s[0] == 0 && s[1] == '<' && s[2] == 0 && s[3] == '?')
    {
        xml_convert_utf16(conv, s, len, true, p);
        converted = true;
    }
    else if (len >= 4 && s[0] == '<' && s[1] == 0 && s[2] == '?' && s[3] == 0)
    {
        xml_convert_utf16(conv, s, len, false, p);
        converted = true;
    }
    else
    {
        if (len >= 3 && s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF)
        {
            s += 3;
            len -= 3;
        }
        const char *enc = NULL;
        size_t enc_len = 0;
        if (xml_find_declared_encoding((const char*)s, len, &enc, &enc_len))
        {
            if (xml_ascii_ieq(enc, enc_len, "ISO-8859-1") || xml_ascii_ieq(enc, enc_len, "latin1") ||
                xml_ascii_ieq(enc, enc_len, "ISO_8859-1") || xml_ascii_ieq(enc, enc_len, "ISO-LATIN-1") ||
                xml_ascii_ieq(enc, enc_len, "L1"))
            {
                for (size_t k = 0; k < len; ++k) xml_pbuf_add_cp(conv, s[k]);
                converted = true;
            }
            else if (!(xml_ascii_ieq(enc, enc_len, "UTF-8") || xml_ascii_ieq(enc, enc_len, "UTF8") ||
                       xml_ascii_ieq(enc, enc_len, "US-ASCII") || xml_ascii_ieq(enc, enc_len, "ASCII") ||
                       xml_ascii_ieq(enc, enc_len, "UTF-16") || xml_ascii_ieq(enc, enc_len, "UTF16")))
            {
                p->buf = (const char*)s;
                p->len = len;
                xml_parser_fatal(p, (const char*)enc - (const char*)s + enc_len + 1, "Unsupported encoding: %.*s", (int)enc_len, enc);
            }
        }
    }

    if (converted)
    {
        p->owned = conv->b ? conv->b : xml_parser_strdup("");
        p->buf = p->owned;
        p->len = conv->len;
        conv->b = NULL;
        conv->len = conv->cap = 0;
    }
    else
    {
        p->buf = (const char*)s;
        p->len = len;
    }

    xml_normalize_eol(p);

    // Validate utf8 and xml chars.
    const unsigned char *b = (const unsigned char*)p->buf;
    for (size_t k = 0; k < p->len; )
    {
        if (b[k] >= 0x20 && b[k] < 0x80) { k++; continue; }
        size_t l = 0;
        int c = xml_utf8_decode(b + k, p->len - k, &l);
        if (c < 0)
        {
            // Like libxml2 2.16, the decoded input (and so the context line) ends at the bad bytes.
            size_t full = p->len;
            p->len = k;
            size_t need = b[k] >= 0xF0 ? 4 : b[k] >= 0xE0 ? 3 : 2;
            bool incomplete = b[k] >= 0xC2 && b[k] < 0xF5 && full - k < need;
            for (size_t j = k + 1; incomplete && j < full; ++j) if ((b[j] & 0xC0) != 0x80) incomplete = false;
            if (incomplete)
            {
                xml_parser_fatal(p, k, "Incomplete UTF-8 sequence starting with %02X", b[k]);
            }
            p->fatal_kind = "I/O error";
            xml_parser_fatal(p, k, "Invalid bytes in character encoding\nBytes: 0x%02X 0x%02X 0x%02X 0x%02X",
                             b[k], k+1 < full ? b[k+1] : 0, k+2 < full ? b[k+2] : 0, k+3 < full ? b[k+3] : 0);
        }
        if (!xml_is_xml_char(c) && !p->has_bad)
        {
            p->has_bad = true;
            p->bad_pos = k;
            p->bad_cp = c;
        }
        k += l;
    }
}

// Scanning helpers ///////////////////////////////////////////////

static bool xml_eof(XmlParser *p)
{
    return p->i >= p->len;
}

static char xml_cur(XmlParser *p)
{
    return p->i < p->len ? p->buf[p->i] : 0;
}

static char xml_nxt(XmlParser *p, size_t n)
{
    return p->i + n < p->len ? p->buf[p->i + n] : 0;
}

static bool xml_at(XmlParser *p, const char *t)
{
    size_t n = strlen(t);
    return p->i + n <= p->len && !memcmp(p->buf + p->i, t, n);
}

static bool xml_skip_blanks(XmlParser *p)
{
    size_t start = p->i;
    while (p->i < p->len && xml_is_blank(p->buf[p->i])) p->i++;
    return p->i > start;
}

/** Find t at or after from, returns the position or SIZE_MAX. */
static size_t xml_find(XmlParser *p, size_t from, const char *t)
{
    size_t n = strlen(t);
    if (n == 0 || from + n > p->len) return SIZE_MAX;
    const char *hay = p->buf + from;
    size_t hl = p->len - from;
    while (hl >= n)
    {
        const char *f = (const char*)memchr(hay, t[0], hl - n + 1);
        if (!f) return SIZE_MAX;
        if (!memcmp(f, t, n)) return f - p->buf;
        hl -= (f - hay) + 1;
        hay = f + 1;
    }
    return SIZE_MAX;
}

static void xml_check_name_length(XmlParser *p, size_t start)
{
    if (p->i - start > p->settings->max_name_length)
    {
        xml_parser_fatal(p, start, "Name too long");
    }
}

/** Parse a Name, returns a slice with s == NULL if no name starts here. */
static XmlSlice xml_parse_name(XmlParser *p, bool allow_colon)
{
    XmlSlice r = { NULL, 0 };
    if (xml_eof(p)) return r;
    size_t l;
    int c = xml_cp_at(p, p->i, &l);
    if (!xml_is_name_start_char(c) || (!allow_colon && c == ':')) return r;
    size_t start = p->i;
    p->i += l;
    while (p->i < p->len)
    {
        c = xml_cp_at(p, p->i, &l);
        if (!xml_is_name_char(c) || (!allow_colon && c == ':')) break;
        p->i += l;
    }
    xml_check_name_length(p, start);
    r.s = p->buf + start;
    r.len = p->i - start;
    return r;
}

static XmlSlice xml_parse_nmtoken(XmlParser *p)
{
    XmlSlice r = { NULL, 0 };
    size_t start = p->i;
    while (p->i < p->len)
    {
        size_t l;
        int c = xml_cp_at(p, p->i, &l);
        if (!xml_is_name_char(c)) break;
        p->i += l;
    }
    if (p->i == start) return r;
    xml_check_name_length(p, start);
    r.s = p->buf + start;
    r.len = p->i - start;
    return r;
}

static void xml_qname_malformed(XmlParser *p, XmlSlice name)
{
    // Like libxml2 xmlParseQNameHashed, reported where the name ends.
    p->qname_malformed = true;
    if (!p->qname_quiet)
    {
        xml_parser_message(p, "namespace error", p->i, "Failed to parse QName '%.*s'", (int)name.len, name.s);
    }
}

/** Parse a QName. Returns false if no name starts here. */
static bool xml_parse_qname(XmlParser *p, XmlQName *q)
{
    size_t start = p->i;
    q->prefix.s = NULL;
    q->prefix.len = 0;
    p->qname_malformed = false;
    XmlSlice l = xml_parse_name(p, false);
    if (!l.s)
    {
        // Names starting with a colon are accepted by libxml2 as plain names.
        XmlSlice n = xml_parse_name(p, true);
        if (!n.s) return false;
        q->local = n;
        xml_qname_malformed(p, n);
        return true;
    }
    if (xml_cur(p) == ':')
    {
        p->i++;
        XmlSlice l2 = xml_parse_name(p, false);
        if (!l2.s || xml_cur(p) == ':')
        {
            // Not a valid QName, treat the whole thing as a name.
            p->i = start;
            q->local = xml_parse_name(p, true);
            xml_qname_malformed(p, q->local);
            return true;
        }
        q->prefix = l;
        q->local = l2;
        return true;
    }
    q->local = l;
    return true;
}

// Text ///////////////////////////////////////////////////////////

static void xml_check_text_length(XmlParser *p, size_t len, size_t pos)
{
    if (len > p->settings->max_text_length)
    {
        xml_parser_fatal(p, pos, "text node too long, try XML_PARSE_HUGE");
    }
}

static void xml_characters(XmlParser *p, const char *s, size_t len)
{
    if (!p->has_text)
    {
        p->has_text = true;
        p->text_line = xml_parser_line_at(p, p->i);
        xml_pbuf_clear(&p->text);
    }
    xml_pbuf_add(&p->text, s, len);
    xml_check_text_length(p, p->text.len, p->i);
}

static void xml_flush_text(XmlParser *p)
{
    if (!p->has_text) return;
    XMQNode *t = xml_node_new_text_len(p->dom, p->text.b ? p->text.b : "", p->text.len);
    xml_node_set_line(t, (int)p->text_line);
    xml_node_append_child(p->node, t);
    p->has_text = false;
    xml_pbuf_clear(&p->text);
}

static void xml_append_node(XmlParser *p, XMQNode *n)
{
    if (p->in_subset)
    {
        xml_node_append_child(p->dtd, n);
        return;
    }
    xml_flush_text(p);
    xml_node_append_child(p->node, n);
}

// References /////////////////////////////////////////////////////

static const char *xml_predefined(XmlSlice name)
{
    if (xml_slice_eq(name, "lt")) return "<";
    if (xml_slice_eq(name, "gt")) return ">";
    if (xml_slice_eq(name, "amp")) return "&";
    if (xml_slice_eq(name, "apos")) return "'";
    if (xml_slice_eq(name, "quot")) return "\"";
    return NULL;
}

/** Parse &#123; or &#x1F; at the current position and return the code point. */
static int xml_parse_char_ref(XmlParser *p)
{
    p->i += 2;
    int val = 0;
    bool ok = false;
    if (xml_cur(p) == 'x')
    {
        p->i++;
        while (!xml_eof(p) && xml_cur(p) != ';')
        {
            char c = xml_cur(p);
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else xml_parser_fatal(p, p->i, "xmlParseCharRef: invalid hexadecimal value");
            val = val * 16 + d;
            if (val > 0x110000) val = 0x110000;
            ok = true;
            p->i++;
        }
    }
    else
    {
        while (!xml_eof(p) && xml_cur(p) != ';')
        {
            char c = xml_cur(p);
            if (c < '0' || c > '9') xml_parser_fatal(p, p->i, "xmlParseCharRef: invalid decimal value");
            val = val * 10 + (c - '0');
            if (val > 0x110000) val = 0x110000;
            ok = true;
            p->i++;
        }
    }
    if (xml_cur(p) != ';' || !ok)
    {
        xml_parser_fatal(p, p->i, "xmlParseCharRef: invalid decimal value");
    }
    p->i++;
    // Like libxml2 these are reported after the reference.
    if (val >= 0x110000)
    {
        xml_parser_fatal(p, p->i, "xmlParseCharRef: character reference out of bounds");
    }
    if (!xml_is_xml_char(val))
    {
        xml_parser_fatal(p, p->i, "xmlParseCharRef: invalid xmlChar value %d", val);
    }
    return val;
}

static XMQNode *xml_get_entity(XmlParser *p, XmlSlice name)
{
    if (p->expanding) return xml_dom_get_entity(p->dom, xml_tmp_slice(p, 3, name));
    if (!p->entities) return NULL;
    return (XMQNode*)hashmap_get(p->entities, xml_tmp_slice(p, 3, name));
}

/** Report an undeclared entity, like libxml2 at the current position, after the reference. */
static void xml_undeclared_entity(XmlParser *p, XmlSlice name)
{
    if (!p->dtd || xml_dom_standalone(p->dom) == 1 || (!p->has_external_subset && !p->has_pe_refs))
    {
        xml_parser_fatal(p, p->i, "Entity '%.*s' not defined", (int)name.len, name.s);
    }
    // Recoverable when the entity could be declared in an external subset, a warning in libxml2.
    xml_parser_message(p, "parser warning", p->i, "Entity '%.*s' not defined", (int)name.len, name.s);
}

/** Parse &name; (not a char ref) at the current position. Returns the name. */
static XmlSlice xml_parse_entity_ref_name(XmlParser *p)
{
    p->i++;
    XmlSlice name = xml_parse_name(p, true);
    if (!name.s) xml_parser_fatal(p, p->i, "xmlParseEntityRef: no name");
    if (xml_cur(p) != ';') xml_parser_fatal(p, p->i, "EntityRef: expecting ';'");
    p->i++;
    return name;
}

static void xml_parse_reference(XmlParser *p)
{
    if (xml_nxt(p, 1) == '#')
    {
        int cp = xml_parse_char_ref(p);
        XmlPBuf *tmp = &p->tmp[0];
        xml_pbuf_clear(tmp);
        xml_pbuf_add_cp(tmp, cp);
        xml_characters(p, tmp->b, tmp->len);
        return;
    }
    XmlSlice name = xml_parse_entity_ref_name(p);
    const char *pre = xml_predefined(name);
    if (pre)
    {
        xml_characters(p, pre, 1);
        return;
    }
    XMQNode *ent = xml_get_entity(p, name);
    if (!ent)
    {
        xml_undeclared_entity(p, name);
    }
    else if (xml_entity_decl_type(ent) == XML_ENTITY_EXTERNAL_GENERAL_UNPARSED)
    {
        xml_parser_fatal(p, p->i, "Entity reference to unparsed entity %.*s", (int)name.len, name.s);
    }
    xml_append_node(p, xml_node_new_entity_ref(p->dom, xml_tmp_slice(p, 1, name)));
}

// Attribute values ///////////////////////////////////////////////

static void xml_att_part(XmlParser *p, bool entity, const char *s, size_t len)
{
    if (!entity && p->num_parts > p->value_first_part)
    {
        XmlAttPart *last = &p->parts[p->num_parts-1];
        if (!last->entity && last->off + last->len == p->attval.len)
        {
            // Extend the previous text part.
            xml_pbuf_add(&p->attval, s, len);
            last->len += len;
            return;
        }
    }
    if (p->num_parts == p->parts_cap)
    {
        p->parts_cap = p->parts_cap ? p->parts_cap * 2 : 16;
        p->parts = (XmlAttPart*)realloc(p->parts, p->parts_cap * sizeof(XmlAttPart));
    }
    XmlAttPart *part = &p->parts[p->num_parts++];
    part->entity = entity;
    part->off = p->attval.len;
    part->len = len;
    xml_pbuf_add(&p->attval, s, len);
}

/** Parse a quoted attribute value into text and entity reference parts. Returns the number of parts. */
static size_t xml_parse_att_value(XmlParser *p)
{
    size_t first = p->num_parts;
    p->value_first_part = first;
    size_t total = 0;
    char q = xml_cur(p);
    if (q != '"' && q != '\'') xml_parser_fatal(p, p->i, "AttValue: \" or ' expected");
    p->i++;
    while (true)
    {
        if (xml_eof(p)) xml_parser_fatal(p, p->i, "AttValue: ' expected");
        if (p->has_bad && p->i == p->bad_pos)
        {
            p->has_bad = false;
            xml_parser_fatal(p, p->i, "invalid character in attribute value");
        }
        char c = xml_cur(p);
        if (c == q)
        {
            p->i++;
            break;
        }
        if (c == '<') xml_parser_fatal(p, p->i, "Unescaped '<' not allowed in attributes values");
        if (c == '&')
        {
            if (xml_nxt(p, 1) == '#')
            {
                int cp = xml_parse_char_ref(p);
                XmlPBuf *tmp = &p->tmp[0];
                xml_pbuf_clear(tmp);
                xml_pbuf_add_cp(tmp, cp);
                xml_att_part(p, false, tmp->b, tmp->len);
                total += tmp->len;
                continue;
            }
            XmlSlice name = xml_parse_entity_ref_name(p);
            const char *pre = xml_predefined(name);
            if (pre)
            {
                xml_att_part(p, false, pre, 1);
                total++;
                continue;
            }
            XMQNode *ent = xml_get_entity(p, name);
            if (!ent)
            {
                xml_undeclared_entity(p, name);
            }
            else if (xml_entity_decl_type(ent) == XML_ENTITY_EXTERNAL_GENERAL_PARSED ||
                     xml_entity_decl_type(ent) == XML_ENTITY_EXTERNAL_GENERAL_UNPARSED)
            {
                xml_parser_fatal(p, p->i, "Attribute references external entity '%.*s'", (int)name.len, name.s);
            }
            else if (xml_node_content(ent) && strchr(xml_node_content(ent), '<'))
            {
                xml_parser_fatal(p, p->i, "'<' in entity '%.*s' is not allowed in attributes values", (int)name.len, name.s);
            }
            xml_att_part(p, true, name.s, name.len);
            total += name.len + 2;
            continue;
        }
        // Copy a run of ordinary characters, attribute value normalization turns \t \n into spaces.
        size_t start = p->i;
        while (p->i < p->len)
        {
            char d = p->buf[p->i];
            if (d == q || d == '<' || d == '&' || d == '\t' || d == '\n') break;
            if (p->has_bad && p->i == p->bad_pos) break;
            p->i++;
        }
        if (p->i > start)
        {
            xml_att_part(p, false, p->buf + start, p->i - start);
            total += p->i - start;
        }
        else
        {
            xml_att_part(p, false, " ", 1);
            total++;
            p->i++;
        }
        xml_check_text_length(p, total, start);
    }
    return p->num_parts - first;
}

/** The value as a string, entity references written as &name;. In scratch slot n. */
static const char *xml_att_value_string(XmlParser *p, int n, size_t first, size_t count)
{
    XmlPBuf *b = &p->tmp[n];
    xml_pbuf_clear(b);
    xml_pbuf_add(b, "", 0);
    for (size_t k = first; k < first + count; ++k)
    {
        XmlAttPart *part = &p->parts[k];
        if (part->entity) xml_pbuf_addc(b, '&');
        xml_pbuf_add(b, p->attval.b + part->off, part->len);
        if (part->entity) xml_pbuf_addc(b, ';');
    }
    return b->b;
}

/** The value of an attribute default as libxml2 stores it: entity references written as
    &name; and a & that came from a reference (&amp; or &#38;) written as &#38;. In scratch slot n. */
static const char *xml_att_default_string(XmlParser *p, int n, size_t first, size_t count)
{
    XmlPBuf *b = &p->tmp[n];
    xml_pbuf_clear(b);
    xml_pbuf_add(b, "", 0);
    for (size_t k = first; k < first + count; ++k)
    {
        XmlAttPart *part = &p->parts[k];
        if (part->entity)
        {
            xml_pbuf_addc(b, '&');
            xml_pbuf_add(b, p->attval.b + part->off, part->len);
            xml_pbuf_addc(b, ';');
            continue;
        }
        for (size_t j = 0; j < part->len; ++j)
        {
            char c = p->attval.b[part->off + j];
            if (c == '&') xml_pbuf_add(b, "&#38;", 5);
            else xml_pbuf_addc(b, c);
        }
    }
    return b->b;
}

/** Is the attribute declared with a type other than CDATA? Then its value is normalized. */
static bool xml_attr_needs_normalization(XmlParser *p, const char *element_qname, XmlQName attr)
{
    if (!p->attr_types) return false;
    XmlPBuf *k = &p->tmp[3];
    xml_pbuf_clear(k);
    xml_pbuf_add(k, element_qname, strlen(element_qname));
    xml_pbuf_addc(k, 1);
    if (attr.prefix.s)
    {
        xml_pbuf_add(k, attr.prefix.s, attr.prefix.len);
        xml_pbuf_addc(k, ':');
    }
    xml_pbuf_add(k, attr.local.s, attr.local.len);
    return hashmap_get(p->attr_types, k->b) == (void*)2;
}

/**
    Port of libxml2 xmlAttrNormalizeSpace2: remove leading and trailing spaces and collapse
    runs of spaces into one. Works on the parts of the value, entity references count as
    non space. The normalized parts are appended, the attribute is updated to use them.
*/
static void xml_normalize_attr_parts(XmlParser *p, XmlRawAttr *ra)
{
    size_t from = ra->first_part;
    size_t to = from + ra->num_parts;
    size_t first = p->num_parts;
    p->value_first_part = first;
    bool started = false;
    bool pending_space = false;
    for (size_t k = from; k < to; ++k)
    {
        XmlAttPart part = p->parts[k];
        if (part.entity)
        {
            if (pending_space) xml_att_part(p, false, " ", 1);
            pending_space = false;
            xml_att_part(p, true, xml_tmp(p, 0, p->attval.b + part.off, part.len), part.len);
            started = true;
            continue;
        }
        for (size_t j = 0; j < part.len; ++j)
        {
            char c = p->attval.b[part.off + j];
            if (c == ' ')
            {
                if (started) pending_space = true;
                continue;
            }
            if (pending_space) xml_att_part(p, false, " ", 1);
            pending_space = false;
            xml_att_part(p, false, &c, 1);
            started = true;
        }
    }
    ra->first_part = first;
    ra->num_parts = p->num_parts - first;
}

/** Normalize a zero terminated default value in place, like xmlAttrNormalizeSpace. */
static void xml_normalize_space(char *v)
{
    char *src = v, *dst = v;
    while (*src == ' ') src++;
    while (*src)
    {
        if (*src == ' ')
        {
            while (*src == ' ') src++;
            if (*src) *dst++ = ' ';
            continue;
        }
        *dst++ = *src++;
    }
    *dst = 0;
}

// Elements ///////////////////////////////////////////////////////

static void xml_push_open(XmlParser *p, XMQNode *e, const char *qname, size_t line, int space)
{
    if (p->depth == p->stack_cap)
    {
        p->stack_cap = p->stack_cap ? p->stack_cap * 2 : 32;
        p->stack = (XmlOpen*)realloc(p->stack, p->stack_cap * sizeof(XmlOpen));
    }
    XmlOpen *o = &p->stack[p->depth++];
    o->ns_open = xml_node_first_ns_decl(e) ? p->depth : (p->depth > 1 ? p->stack[p->depth-2].ns_open : 0);
    o->element = e;
    o->qname = xml_parser_strdup(qname);
    o->line = line;
    o->space = space;
}

static XmlOpen *xml_top(XmlParser *p)
{
    return p->depth ? &p->stack[p->depth-1] : NULL;
}

static XmlNs *xml_ns_declared_on(const XMQNode *e, const char *prefix)
{
    for (XmlNs *d = xml_node_first_ns_decl(e); d; d = xml_ns_next_decl(d))
    {
        const char *dp = xml_ns_prefix(d);
        if (dp == prefix || (dp && prefix && !strcmp(dp, prefix))) return d;
    }
    return NULL;
}

/** The namespace of prefix in scope at e, the element of the start tag being parsed (not yet
    pushed), like xml_ns_lookup_prefix(e, prefix). That walks all the ancestors, which is
    quadratic in the depth of a deep document, here only the open elements that declare
    namespaces are visited (the ns_open chain), then the ancestors below the open elements of
    the chunk being parsed (the document, or the node an entity is expanded into). */
static XmlNs *xml_lookup_ns(XmlParser *p, XMQNode *e, const char *prefix)
{
    if (prefix && !strcmp(prefix, "xml")) return xml_ns_lookup_prefix(e, prefix);
    XmlNs *d = xml_ns_declared_on(e, prefix);
    if (d) return d;
    size_t k = p->depth ? p->stack[p->depth-1].ns_open : 0;
    while (k > p->ns_base_depth)
    {
        d = xml_ns_declared_on(p->stack[k-1].element, prefix);
        if (d) return d;
        k = k > 1 ? p->stack[k-2].ns_open : 0;
    }
    XMQNode *below = p->depth > p->ns_base_depth ? xml_node_parent(p->stack[p->ns_base_depth].element) : xml_node_parent(e);
    return below ? xml_ns_lookup_prefix(below, prefix) : NULL;
}

static bool xml_raw_attr_is_xmlns(const XmlRawAttr *ra, XmlSlice *prefix)
{
    if (!ra->name.prefix.s && xml_slice_eq(ra->name.local, "xmlns"))
    {
        prefix->s = NULL;
        prefix->len = 0;
        return true;
    }
    if (xml_slice_eq(ra->name.prefix, "xmlns"))
    {
        *prefix = ra->name.local;
        return true;
    }
    return false;
}

static bool xml_ns_declared_here(XMQNode *e, const char *prefix)
{
    for (XmlNs *d = xml_node_first_ns_decl(e); d; d = xml_ns_next_decl(d))
    {
        const char *dp = xml_ns_prefix(d);
        if ((!dp && !prefix) || (dp && prefix && !strcmp(dp, prefix))) return true;
    }
    return false;
}

/** The checks of a namespace declaration of libxml2 xmlParseStartTag2, reported right after
    the attribute value. Returns false if the declaration is not kept. */
static bool xml_check_ns_decl(XmlParser *p, XmlRawAttr *ra)
{
    XmlSlice prefix;
    if (!xml_raw_attr_is_xmlns(ra, &prefix)) return true;
    for (size_t j = ra->first_part; j < ra->first_part + ra->num_parts; ++j)
    {
        if (p->parts[j].entity) return true; // Rejected later.
    }
    const char *href = xml_att_value_string(p, 0, ra->first_part, ra->num_parts);
    const char *pfx = prefix.s ? xml_tmp_slice(p, 1, prefix) : NULL;
    static const char *xml_ns_uri = "http://www.w3.org/XML/1998/namespace";
    static const char *xmlns_ns_uri = "http://www.w3.org/2000/xmlns/";
    if (!pfx)
    {
        if (*href)
        {
            XmlURI *uri = NULL;
            xml_uri_parse_safe(href, &uri);
            if (!uri)
            {
                xml_parser_message(p, "namespace error", ra->end, "xmlns: '%s' is not a valid URI", href);
            }
            else
            {
                if (!uri->scheme) xml_parser_message(p, "namespace warning", ra->end, "xmlns: URI %s is not absolute", href);
                xml_uri_free(uri);
            }
            if (!strcmp(href, xml_ns_uri))
            {
                xml_parser_message(p, "namespace error", ra->end, "xml namespace URI cannot be the default namespace");
                return false;
            }
            if (!strcmp(href, xmlns_ns_uri))
            {
                xml_parser_message(p, "namespace error", ra->end, "reuse of the xmlns namespace name is forbidden");
                return false;
            }
        }
    }
    else
    {
        if (!strcmp(pfx, "xml"))
        {
            if (strcmp(href, xml_ns_uri)) xml_parser_message(p, "namespace error", ra->end, "xml namespace prefix mapped to wrong URI");
            return false;
        }
        if (!strcmp(href, xml_ns_uri))
        {
            xml_parser_message(p, "namespace error", ra->end, "xml namespace URI mapped to wrong prefix");
            return false;
        }
        if (!strcmp(pfx, "xmlns"))
        {
            xml_parser_message(p, "namespace error", ra->end, "redefinition of the xmlns prefix is forbidden");
            return false;
        }
        if (!strcmp(href, xmlns_ns_uri))
        {
            xml_parser_message(p, "namespace error", ra->end, "reuse of the xmlns namespace name is forbidden");
            return false;
        }
        if (!*href)
        {
            xml_parser_message(p, "namespace error", ra->end, "xmlns:%s: Empty XML namespace is not allowed", pfx);
            return false;
        }
        XmlURI *uri = NULL;
        xml_uri_parse_safe(href, &uri);
        if (!uri) xml_parser_message(p, "namespace error", ra->end, "xmlns:%s: '%s' is not a valid URI", pfx, href);
        xml_uri_free(uri);
    }
    return true;
}

/** Parse a start tag. Returns true if an element was opened (not an empty element tag). */
static bool xml_parse_start_tag(XmlParser *p)
{
    size_t tag_start = p->i;
    size_t tag_line = xml_parser_line_at(p, p->i);
    p->i++; // Skip <
    XmlQName qn;
    if (!xml_parse_qname(p, &qn)) xml_parser_fatal(p, p->i, "StartTag: invalid element name");

    if (p->depth >= p->settings->max_depth)
    {
        xml_parser_fatal(p, tag_start, "Excessive depth in document: %zu", p->settings->max_depth);
    }

    p->num_attrs = 0;
    p->num_parts = 0;
    xml_pbuf_clear(&p->attval);
    bool empty = false;
    // The end of the start tag (the > or />), where libxml2 2.16 reports duplicate
    // attributes and undefined prefixes, since it checks them after reading all attributes.
    size_t tag_end = p->i;
    while (true)
    {
        bool blank = xml_skip_blanks(p);
        if (xml_eof(p))
        {
            xml_parser_fatal(p, p->i, "Couldn't find end of Start Tag %s line %zu", xml_tmp_qname(p, 0, qn), tag_line);
        }
        if (xml_cur(p) == '>')
        {
            tag_end = p->i;
            p->i++;
            break;
        }
        if (xml_at(p, "/>"))
        {
            tag_end = p->i;
            p->i += 2;
            empty = true;
            break;
        }
        if (!blank) xml_parser_fatal(p, p->i, "attributes construct error");
        size_t apos = p->i;
        XmlQName an;
        if (!xml_parse_qname(p, &an)) xml_parser_fatal(p, p->i, "attributes construct error");
        xml_skip_blanks(p);
        if (xml_cur(p) != '=')
        {
            xml_parser_fatal(p, p->i, "Specification mandates value for attribute %s", xml_tmp_qname(p, 0, an));
        }
        p->i++;
        xml_skip_blanks(p);
        size_t first = p->num_parts;
        size_t count = xml_parse_att_value(p);
        if (p->num_attrs == p->attrs_cap)
        {
            p->attrs_cap = p->attrs_cap ? p->attrs_cap * 2 : 16;
            p->attrs = (XmlRawAttr*)realloc(p->attrs, p->attrs_cap * sizeof(XmlRawAttr));
        }
        XmlRawAttr *ra = &p->attrs[p->num_attrs++];
        ra->name = an;
        ra->first_part = first;
        ra->num_parts = count;
        ra->pos = apos;
        ra->end = p->i;
        ra->ns_skip = !xml_check_ns_decl(p, ra);
    }

    // Values of attributes declared with a non CDATA type are normalized.
    if (p->attr_types)
    {
        const char *eq = xml_tmp_qname(p, 2, qn);
        char *element_qname = xml_parser_strdup(eq);
        for (size_t k = 0; k < p->num_attrs; ++k)
        {
            if (xml_attr_needs_normalization(p, element_qname, p->attrs[k].name))
            {
                xml_normalize_attr_parts(p, &p->attrs[k]);
            }
        }
        free(element_qname);
    }

    // Duplicate attribute names as written, reported after the namespaced duplicates
    // like libxml2 does.
    XmlRawAttr *raw_dup = NULL;
    HashMap *seen = p->num_attrs > 16 ? hashmap_create(p->num_attrs * 2) : NULL;
    for (size_t k = 0; k < p->num_attrs; ++k)
    {
        XmlRawAttr *ra = &p->attrs[k];
        bool dup = false;
        if (seen)
        {
            const char *key = xml_tmp_qname(p, 0, ra->name);
            if (hashmap_get(seen, key)) dup = true;
            else hashmap_put(seen, key, (void*)ra);
        }
        else
        {
            for (size_t j = 0; j < k; ++j)
            {
                if (xml_slice_eq_slice(p->attrs[j].name.local, ra->name.local) &&
                    xml_slice_eq_slice(p->attrs[j].name.prefix, ra->name.prefix)) dup = true;
            }
        }
        if (dup && !raw_dup)
        {
            raw_dup = ra;
            if (seen) break;
        }
    }
    if (seen) hashmap_free(seen);
    XmlSlice dup_prefix;
    if (raw_dup && xml_raw_attr_is_xmlns(raw_dup, &dup_prefix))
    {
        xml_parser_fatal(p, tag_end, "Attribute %s redefined", xml_tmp_qname(p, 0, raw_dup->name));
    }

    const char *qname = xml_tmp_qname(p, 2, qn);
    XMQNode *e = xml_node_new_element(p->dom, xml_tmp_slice(p, 1, qn.local), NULL);
    // Like libxml2 the element gets the line where its start tag ends.
    xml_node_set_line(e, (int)xml_parser_line_at(p, tag_end));
    xml_append_node(p, e);

    // Namespace declarations first.
    for (size_t k = 0; k < p->num_attrs; ++k)
    {
        XmlRawAttr *ra = &p->attrs[k];
        XmlSlice prefix;
        if (!xml_raw_attr_is_xmlns(ra, &prefix)) continue;
        for (size_t j = ra->first_part; j < ra->first_part + ra->num_parts; ++j)
        {
            if (p->parts[j].entity)
            {
                // Entities are never expanded implicitly and a namespace uri is a plain string,
                // so a reference cannot be kept. Reject for now (TODO.md).
                xml_parser_fatal(p, ra->end, "Entity references in namespace uris are not supported (xmlns%s%.*s)",
                                 prefix.s ? ":" : "", (int)prefix.len, prefix.s ? prefix.s : "");
            }
        }
        const char *href = xml_att_value_string(p, 0, ra->first_part, ra->num_parts);
        const char *pfx = prefix.s ? xml_tmp_slice(p, 1, prefix) : NULL;
        if (ra->ns_skip) continue;
        xml_ns_declare(e, pfx, href);
    }

    // Namespace declarations defaulted from the dtd, like libxml2 xmlParseStartTag2.
    for (size_t k = 0; k < p->num_defaults; ++k)
    {
        XmlAttDefault *d = &p->defaults[k];
        if (strcmp(d->element, qname)) continue;
        if (strcmp(d->attr, "xmlns") && strncmp(d->attr, "xmlns:", 6)) continue;
        const char *prefix = !strcmp(d->attr, "xmlns") ? NULL : d->attr + 6;
        if (xml_ns_declared_here(e, prefix)) continue;
        if (prefix && !strcmp(prefix, "xml")) continue;
        xml_ns_declare(e, prefix, d->value);
    }

    // Resolve the element namespace.
    const char *eprefix = qn.prefix.s ? xml_tmp_slice(p, 1, qn.prefix) : NULL;
    XmlNs *ens = xml_lookup_ns(p, e, eprefix);
    bool eprefix_undefined = eprefix && !ens;
    if (eprefix_undefined)
    {
        // Reported after the attributes, like libxml2.
        xml_node_set_name(e, qname);
    }
    else if (ens && !(eprefix == NULL && !*xml_ns_href(ens)))
    {
        xml_node_set_ns(e, ens);
    }

    // Then the attributes.
    XmlOpen *top = xml_top(p);
    int space = top ? top->space : -1;
    if (space == -2) space = -1;
    HashMap *resolved = p->num_attrs > 16 ? hashmap_create(p->num_attrs * 2) : NULL;
    for (size_t k = 0; k < p->num_attrs; ++k)
    {
        XmlRawAttr *ra = &p->attrs[k];
        XmlSlice dummy;
        if (xml_raw_attr_is_xmlns(ra, &dummy)) continue;
        const char *local = xml_tmp_slice(p, 1, ra->name.local);
        XmlNs *ans = NULL;
        const char *name = local;
        if (ra->name.prefix.s)
        {
            const char *apfx = xml_tmp_slice(p, 0, ra->name.prefix);
            ans = xml_lookup_ns(p, e, apfx);
            if (!ans)
            {
                xml_parser_message(p, "namespace error", tag_end, "Namespace prefix %s for %s on %.*s is not defined",
                                   apfx, local, (int)qn.local.len, qn.local.s);
                name = xml_tmp_qname(p, 1, ra->name);
            }
        }
        const char *uri = ans ? xml_ns_href(ans) : NULL;
        // The first earlier attribute with the same expanded name, like the attribute hash of
        // libxml2 xmlParseStartTag2. The same prefix is a duplicate, another prefix
        // bound to the same uri a recoverable namespace error, both attributes are kept.
        bool dup = false;
        bool same_prefix = false;
        if (resolved)
        {
            XmlPBuf *kb = &p->tmp[3];
            xml_pbuf_clear(kb);
            if (uri) xml_pbuf_add(kb, uri, strlen(uri));
            xml_pbuf_addc(kb, '}');
            xml_pbuf_add(kb, name, strlen(name));
            XmlRawAttr *prev = (XmlRawAttr*)hashmap_get(resolved, kb->b);
            dup = prev != NULL;
            if (!dup) hashmap_put(resolved, kb->b, (void*)ra);
            else same_prefix = xml_slice_eq_slice(prev->name.prefix, ra->name.prefix);
        }
        else
        {
            XMQAttr *prev = xml_node_get_attr(e, name, uri);
            dup = prev != NULL;
            if (dup)
            {
                const char *pp = uri ? xml_node_ns_prefix(xml_attr_node(prev)) : NULL;
                same_prefix = !uri || (pp && ra->name.prefix.s && xml_slice_eq(ra->name.prefix, pp));
            }
        }
        if (dup && same_prefix)
        {
            if (resolved) hashmap_free(resolved);
            xml_parser_fatal(p, tag_end, "Attribute %s redefined", xml_tmp_qname(p, 0, ra->name));
        }
        if (dup)
        {
            xml_parser_message(p, "namespace error", tag_end, "Namespaced Attribute %s in '%s' redefined", local, uri);
        }
        XMQAttr *a = dup ? xml_node_append_attr(e, name, ans, NULL) : xml_node_set_attr(e, name, ans, NULL);
        XMQNode *an = xml_attr_node(a);
        for (size_t j = ra->first_part; j < ra->first_part + ra->num_parts; ++j)
        {
            XmlAttPart *part = &p->parts[j];
            const char *v = xml_tmp(p, 0, p->attval.b + part->off, part->len);
            if (part->entity) xml_node_append_child(an, xml_node_new_entity_ref(p->dom, v));
            else xml_node_append_child(an, xml_node_new_text_len(p->dom, v, part->len));
        }
        if (xml_slice_eq(ra->name.prefix, "xml") && xml_slice_eq(ra->name.local, "space"))
        {
            const char *v = xml_att_value_string(p, 0, ra->first_part, ra->num_parts);
            if (!strcmp(v, "preserve")) space = 1;
            else if (!strcmp(v, "default")) space = 0;
        }
    }
    if (resolved) hashmap_free(resolved);
    if (raw_dup) xml_parser_fatal(p, tag_end, "Attribute %s redefined", xml_tmp_qname(p, 0, raw_dup->name));

    // The attributes defaulted from the dtd (libxml2 XML_PARSE_DTDATTR).
    if (p->settings->dtd_attr)
    {
        for (size_t k = 0; k < p->num_defaults; ++k)
        {
            XmlAttDefault *d = &p->defaults[k];
            if (strcmp(d->element, qname)) continue;
            if (!strcmp(d->attr, "xmlns") || !strncmp(d->attr, "xmlns:", 6)) continue;
            const char *colon = strchr(d->attr, ':');
            const char *local = colon ? colon + 1 : d->attr;
            XmlNs *ans = NULL;
            if (colon)
            {
                const char *apfx = xml_tmp(p, 0, d->attr, (size_t)(colon - d->attr));
                ans = xml_lookup_ns(p, e, apfx);
                if (!ans)
                {
                    xml_parser_message(p, "namespace error", tag_end, "Namespace prefix %s for %s on %.*s is not defined",
                                       apfx, local, (int)qn.local.len, qn.local.s);
                }
            }
            const char *uri = ans ? xml_ns_href(ans) : NULL;
            if (xml_node_get_attr(e, local, uri)) continue;
            XMQAttr *a = xml_node_append_attr(e, local, ans, NULL);
            xml_node_append_child(xml_attr_node(a), xml_node_new_text(p->dom, d->value));
        }
    }

    if (eprefix_undefined)
    {
        xml_parser_message(p, "namespace error", tag_end, "Namespace prefix %.*s on %.*s is not defined",
                           (int)qn.prefix.len, qn.prefix.s, (int)qn.local.len, qn.local.s);
    }

    if (empty) return false;
    xml_push_open(p, e, qname, tag_line, space);
    xml_top(p)->prefixed = qn.prefix.s != NULL;
    p->node = e;
    return true;
}

static void xml_parse_end_tag(XmlParser *p)
{
    XmlOpen *top = xml_top(p);
    p->i += 2;
    XmlQName qn;
    const char *name = "";
    // libxml2 xmlParseQNameAndCompare (used if the start tag has a prefix) only parses the
    // QName (and reports it) if it differs, otherwise a plain name is parsed.
    p->qname_quiet = true;
    bool has_name = xml_parse_qname(p, &qn);
    p->qname_quiet = false;
    if (has_name)
    {
        name = xml_tmp_qname(p, 0, qn);
        if (p->qname_malformed && top->prefixed && strcmp(name, top->qname)) xml_qname_malformed(p, qn.local);
    }
    xml_skip_blanks(p);
    if (xml_cur(p) != '>') xml_parser_fatal(p, p->i, "expected '>'");
    p->i++;
    if (strcmp(name, top->qname))
    {
        xml_parser_fatal(p, p->i, "Opening and ending tag mismatch: %s line %zu and %s", top->qname, top->line, name);
    }
}

/** The declared content type of an element declaration: 'E'MPTY, 'A'NY, 'M'IXED or 'C'hildren. */
static char xml_element_decl_type(XMQNode *d)
{
    const char *t = xml_node_content(d);
    // The text is "<!ELEMENT name spec", find the spec after the name.
    const char *sp = strchr(t + 10, ' ');
    if (!sp) return 'C';
    sp++;
    if (!strcmp(sp, "EMPTY")) return 'E';
    if (!strcmp(sp, "ANY")) return 'A';
    if (!strncmp(sp, "(#PCDATA", 8)) return 'M';
    return 'C';
}

/** The raw input byte at pos, as libxml2 sees it before end of line normalization. */
static char xml_raw_at(XmlParser *p, size_t pos)
{
    if (pos >= p->len) return 0;
    if (p->crmark && p->crmark[pos]) return '\r';
    return p->buf[pos];
}

/** The raw byte after the one at pos (for a \r\n the \n). */
static char xml_raw_next(XmlParser *p, size_t pos)
{
    if (p->crmark && pos < p->len && p->crmark[pos] == 1) return '\n';
    return xml_raw_at(p, pos + 1);
}

/** A real \n in the raw input (not one produced from a \r). */
static bool xml_is_real_lf(XmlParser *p, size_t pos)
{
    return pos < p->len && p->buf[pos] == '\n' && !(p->crmark && p->crmark[pos]);
}

/**
    Port of the libxml2 areBlanks heuristic, called for a chunk of character data.
    p->i is the position after the chunk (libxml2's ctxt->input->cur).
*/
static bool xml_are_blanks(XmlParser *p, const char *chunk, size_t len, bool known_blank)
{
    XmlOpen *top = xml_top(p);
    if (!top) return false;
    if (top->space == 1 || top->space == -2) return false;
    if (!known_blank)
    {
        for (size_t k = 0; k < len; ++k) if (!xml_is_blank(chunk[k])) return false;
    }
    // Look if the element is mixed content in the DTD if available.
    if (p->element_decls)
    {
        XMQNode *d = (XMQNode*)hashmap_get(p->element_decls, top->qname);
        if (d)
        {
            char t = xml_element_decl_type(d);
            if (t == 'C') return true;
            if (t == 'A' || t == 'M') return false;
            // EMPTY falls through to the heuristic, like libxml2.
        }
    }
    char raw = xml_raw_at(p, p->i);
    if (raw != '<' && raw != '\r') return false;
    XMQNode *first = xml_node_first_child(p->node);
    XMQNode *last = xml_node_last_child(p->node);
    bool no_children = !first && !p->has_text;
    if (no_children && raw == '<' && xml_raw_next(p, p->i) == '/') return false;
    if (p->has_text) return false; // The last child is text.
    if (last && xml_node_kind(last) == XML_DOM_TEXT) return false;
    if (first && xml_node_kind(first) == XML_DOM_TEXT) return false;
    return true;
}

/** Port of libxml2 xmlCharacters: a chunk of character data, p->i is after the chunk. */
static void xml_chars_chunk(XmlParser *p, size_t from, size_t to, bool known_blank)
{
    if (to <= from) return;
    bool no_blanks = p->settings->no_blanks;
    if (no_blanks && xml_are_blanks(p, p->buf + from, to - from, known_blank)) return;
    size_t save = p->i;
    p->i = from;
    bool started = !p->has_text;
    xml_characters(p, p->buf + from, to - from);
    // libxml2 gives a text node the line where its first chunk was handed over.
    if (started && p->has_text) p->text_line = xml_parser_line_at(p, to);
    p->i = save;
    if (!no_blanks) return;
    // Once character data (blank or not) has been kept the element is mixed content,
    // after this no blanks are ignored in this element.
    XmlOpen *top = xml_top(p);
    if (top && top->space == -1) top->space = -2;
}

static bool xml_test_char_data(unsigned char c)
{
    // libxml2 test_char_data: tab and printable ascii except < & ]
    return c == 0x09 || (c >= 0x20 && c < 0x80 && c != '<' && c != '&' && c != ']');
}

/** Port of libxml2 xmlParseCharDataComplex: 300 byte chunks. */
static void xml_parse_char_data_complex(XmlParser *p)
{
    size_t chunk_start = p->i;
    size_t nbchar = 0;
    while (p->i < p->len)
    {
        char c = p->buf[p->i];
        if (c == '<' || c == '&') break;
        if (c == ']' && xml_at(p, "]]>")) xml_parser_fatal(p, p->i, "Sequence ']]>' not allowed in content");
        if (p->has_bad && p->i == p->bad_pos)
        {
            p->has_bad = false;
            // libxml2 xmlCurrentChar reports a nul byte itself.
            if (p->bad_cp == 0) xml_parser_message(p, "parser error", p->i, "Invalid character: Char 0x0 out of allowed range\n\n");
            xml_parser_fatal(p, p->i, "PCDATA invalid Char value %d", p->bad_cp);
        }
        size_t l;
        xml_cp_at(p, p->i, &l);
        p->i += l;
        nbchar += l;
        if (nbchar >= 300)
        {
            xml_chars_chunk(p, chunk_start, p->i, false);
            chunk_start = p->i;
            nbchar = 0;
        }
    }
    if (nbchar) xml_chars_chunk(p, chunk_start, p->i, false);
}

/** Port of libxml2 xmlParseCharDataInternal: its chunks are used by the no_blanks heuristic and for the line of the text node. */
static void xml_parse_char_data_noblanks(XmlParser *p)
{
    size_t in = p->i;
    do
    {
        // get_more_space
        while (true)
        {
            while (in < p->len && p->buf[in] == ' ') in++;
            if (xml_is_real_lf(p, in))
            {
                while (xml_is_real_lf(p, in)) in++;
                continue;
            }
            break;
        }
        if (in < p->len && p->buf[in] == '<')
        {
            size_t from = p->i;
            p->i = in;
            xml_chars_chunk(p, from, in, true);
            return;
        }
        // get_more
        while (true)
        {
            while (in < p->len && xml_test_char_data((unsigned char)p->buf[in]) && !(p->crmark && p->crmark[in])) in++;
            if (xml_is_real_lf(p, in))
            {
                while (xml_is_real_lf(p, in)) in++;
                continue;
            }
            if (in < p->len && p->buf[in] == ']')
            {
                if (in + 2 < p->len && p->buf[in+1] == ']' && p->buf[in+2] == '>')
                {
                    xml_parser_fatal(p, in, "Sequence ']]>' not allowed in content");
                }
                in++;
                continue;
            }
            break;
        }
        // invoke_callback
        {
            size_t from = p->i;
            p->i = in;
            xml_chars_chunk(p, from, in, false);
        }
        if (p->crmark && in < p->len && p->crmark[in])
        {
            if (p->crmark[in] == 1)
            {
                // \r\n: the \n starts the next chunk.
                in++;
                continue;
            }
            // A lone \r, libxml2 leaves the fast path.
            break;
        }
        if (in >= p->len || p->buf[in] == '<' || p->buf[in] == '&') return;
    } while (in < p->len &&
             (((unsigned char)p->buf[in] >= 0x20 && (unsigned char)p->buf[in] <= 0x7F) ||
              p->buf[in] == 0x09 || xml_is_real_lf(p, in)));
    // Like libxml2 the complex path starts at the current position (ctxt->input->cur),
    // which after a \r\n is the \n, not the scan position.
    xml_parse_char_data_complex(p);
}

static void xml_parse_char_data(XmlParser *p)
{
    // The chunks of libxml2 also decide the line of the text node.
    xml_parse_char_data_noblanks(p);
}

static void xml_parse_comment(XmlParser *p)
{
    size_t start = p->i;
    p->i += 4;
    size_t content_start = p->i;
    while (true)
    {
        size_t dash = xml_find(p, p->i, "--");
        if (p->has_bad && p->bad_pos >= content_start && p->bad_pos < dash)
        {
            p->has_bad = false;
            if (p->bad_cp == 0)
            {
                // libxml2 xmlCurrentChar reports a nul byte, which then ends the comment.
                xml_parser_message(p, "parser error", p->bad_pos, "Invalid character: Char 0x0 out of allowed range\n\n");
                xml_parser_fatal(p, p->bad_pos, "Comment not terminated");
            }
            xml_parser_fatal(p, p->bad_pos, "xmlParseComment: invalid xmlChar value %d", p->bad_cp);
        }
        if (dash == SIZE_MAX)
        {
            size_t n = p->len - content_start;
            if (n > 50) n = 50;
            xml_parser_fatal(p, start, "Comment not terminated \n<!--%.*s", (int)n, p->buf + content_start);
        }
        p->i = dash;
        if (xml_nxt(p, 2) == '>') break;
        size_t n = dash - content_start;
        if (n > 50) n = 50;
        xml_parser_fatal(p, dash, "Double hyphen within comment: <!--%.*s", (int)n, p->buf + content_start);
    }
    xml_check_text_length(p, p->i - content_start, content_start);
    XMQNode *c = xml_node_new_comment(p->dom, xml_tmp(p, 0, p->buf + content_start, p->i - content_start));
    p->i += 3;
    // Like libxml2, the line where the comment ends.
    xml_node_set_line(c, (int)xml_parser_line_at(p, p->i));
    xml_append_node(p, c);
}

static void xml_parse_pi(XmlParser *p)
{
    size_t start = p->i;
    p->i += 2;
    XmlSlice target = xml_parse_name(p, true);
    if (!target.s) xml_parser_fatal(p, p->i, "xmlParsePI : no target name");
    if (target.len == 3 && (target.s[0] | 32) == 'x' && (target.s[1] | 32) == 'm' && (target.s[2] | 32) == 'l')
    {
        if (xml_slice_eq(target, "xml")) xml_parser_fatal(p, p->i, "XML declaration allowed only at the start of the document");
        xml_parser_fatal(p, p->i, "xmlParsePITarget: invalid name prefix 'xml'");
    }
    const char *data = NULL;
    size_t data_len = 0;
    if (xml_at(p, "?>"))
    {
        p->i += 2;
    }
    else
    {
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "ParsePI: PI %.*s space expected", (int)target.len, target.s);
        size_t end = xml_find(p, p->i, "?>");
        if (p->has_bad && p->bad_pos >= p->i && p->bad_pos < end)
        {
            // Like libxml2 xmlParsePI, the pi ends at a character that is not an xml char.
            p->has_bad = false;
            xml_parser_fatal(p, p->bad_pos, "ParsePI: PI %.*s never end ...", (int)target.len, target.s);
        }
        if (end == SIZE_MAX) xml_parser_fatal(p, start, "ParsePI: PI %.*s never end ...", (int)target.len, target.s);
        data = p->buf + p->i;
        data_len = end - p->i;
        xml_check_text_length(p, data_len, p->i);
        p->i = end + 2;
    }
    XMQNode *pi = xml_node_new_pi(p->dom, xml_tmp_slice(p, 1, target), data ? xml_tmp(p, 0, data, data_len) : NULL);
    // Like libxml2, the line where the pi ends.
    xml_node_set_line(pi, (int)xml_parser_line_at(p, p->i));
    xml_append_node(p, pi);
}

static void xml_parse_cdata(XmlParser *p)
{
    size_t start = p->i;
    p->i += 9;
    size_t end = xml_find(p, p->i, "]]>");
    if (end == SIZE_MAX)
    {
        size_t n = p->len - p->i;
        if (n > 50) n = 50;
        xml_parser_fatal(p, start, "CData section not finished\n%.*s", (int)n, p->buf + p->i);
    }
    size_t content_start = p->i;
    size_t len = end - content_start;
    if (p->settings->no_cdata)
    {
        // Like libxml2, even an empty cdata section creates a (empty) text node,
        // with the line where the section ends.
        bool started = !p->has_text;
        xml_characters(p, p->buf + content_start, len);
        if (started && p->has_text) p->text_line = xml_parser_line_at(p, end + 3);
    }
    else
    {
        xml_check_text_length(p, len, content_start);
        XMQNode *c = xml_node_new_cdata(p->dom, p->buf + content_start, len);
        xml_node_set_line(c, (int)xml_parser_line_at(p, end + 3));
        xml_append_node(p, c);
    }
    p->i = end + 3;
}

static void xml_parse_element_tree(XmlParser *p)
{
    if (!xml_parse_start_tag(p)) return;
    while (p->depth > 0)
    {
        XmlOpen *top = xml_top(p);
        if (xml_eof(p))
        {
            xml_parser_fatal(p, p->i, "Premature end of data in tag %s line %zu", top->qname, top->line);
        }
        char c = xml_cur(p);
        if (c == '<')
        {
            char n = xml_nxt(p, 1);
            if (n == '/')
            {
                xml_parse_end_tag(p);
                xml_flush_text(p);
                free(top->qname);
                p->depth--;
                p->node = p->depth ? p->stack[p->depth-1].element : xml_dom_document(p->dom);
            }
            else if (n == '?') xml_parse_pi(p);
            else if (xml_at(p, "<!--")) xml_parse_comment(p);
            else if (xml_at(p, "<![CDATA[")) xml_parse_cdata(p);
            else if (n == '!') xml_parser_fatal(p, p->i, "StartTag: invalid element name");
            else xml_parse_start_tag(p);
        }
        else if (c == '&')
        {
            xml_parse_reference(p);
        }
        else
        {
            xml_parse_char_data(p);
        }
    }
}

// Prolog /////////////////////////////////////////////////////////

/** Parse = "literal" and return the literal in scratch slot n. */
static const char *xml_parse_eq_literal(XmlParser *p, int n)
{
    xml_skip_blanks(p);
    if (xml_cur(p) != '=') xml_parser_fatal(p, p->i, "Specification mandates value for attribute version");
    p->i++;
    xml_skip_blanks(p);
    char q = xml_cur(p);
    if (q != '"' && q != '\'') xml_parser_fatal(p, p->i, "String not started expecting ' or \"");
    const char *e = (const char*)memchr(p->buf + p->i + 1, q, p->len - p->i - 1);
    if (!e) xml_parser_fatal(p, p->i, "String not closed expecting \" or '");
    size_t end = e - p->buf;
    const char *v = xml_tmp(p, n, p->buf + p->i + 1, end - p->i - 1);
    p->i = end + 1;
    return v;
}

static void xml_parse_xml_decl(XmlParser *p)
{
    p->i += 5;
    xml_skip_blanks(p);
    if (!xml_at(p, "version")) xml_parser_fatal(p, p->i, "Malformed declaration expecting version");
    p->i += 7;
    xml_dom_set_version(p->dom, xml_parse_eq_literal(p, 0));
    bool blank = xml_skip_blanks(p);
    if (xml_at(p, "encoding"))
    {
        if (!blank) xml_parser_fatal(p, p->i, "Blank needed here");
        p->i += 8;
        xml_dom_set_encoding(p->dom, xml_parse_eq_literal(p, 0));
        blank = xml_skip_blanks(p);
    }
    if (xml_at(p, "standalone"))
    {
        if (!blank) xml_parser_fatal(p, p->i, "Blank needed here");
        p->i += 10;
        const char *sa = xml_parse_eq_literal(p, 0);
        if (!strcmp(sa, "yes")) xml_dom_set_standalone(p->dom, 1);
        else if (!strcmp(sa, "no")) xml_dom_set_standalone(p->dom, 0);
        else xml_parser_fatal(p, p->i, "standalone accepts only 'yes' or 'no'");
        xml_skip_blanks(p);
    }
    if (xml_at(p, "?>")) p->i += 2;
    else if (xml_cur(p) == '>') xml_parser_fatal(p, p->i, "XML declaration must end-up with '?>'");
    else xml_parser_fatal(p, p->i, "parsing XML declaration: '?>' expected");
}

/** Comments, processing instructions and blanks outside of the root element. */
static void xml_parse_misc(XmlParser *p)
{
    while (!xml_eof(p))
    {
        if (xml_at(p, "<?")) xml_parse_pi(p);
        else if (xml_at(p, "<!--")) xml_parse_comment(p);
        else if (xml_is_blank(xml_cur(p))) xml_skip_blanks(p);
        else return;
    }
}

// DTD ////////////////////////////////////////////////////////////

/** Parse a quoted literal into scratch slot n. */
static const char *xml_parse_quoted(XmlParser *p, int n, const char *what)
{
    char q = xml_cur(p);
    if (q != '"' && q != '\'') xml_parser_fatal(p, p->i, "%s \" or ' expected", what);
    const char *e = (const char*)memchr(p->buf + p->i + 1, q, p->len - p->i - 1);
    if (!e) xml_parser_fatal(p, p->i, "Unfinished %s", what);
    size_t end = e - p->buf;
    const char *v = xml_tmp(p, n, p->buf + p->i + 1, end - p->i - 1);
    p->i = end + 1;
    return v;
}

/** Parse SYSTEM "x" or PUBLIC "p" "x" into scratch slots 1 (public) and 2 (system). */
static void xml_parse_external_id(XmlParser *p, bool allow_public_only, const char **pub, const char **sys)
{
    *pub = NULL;
    *sys = NULL;
    if (xml_at(p, "SYSTEM"))
    {
        p->i += 6;
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after 'SYSTEM'");
        *sys = xml_parse_quoted(p, 2, "SystemLiteral");
    }
    else if (xml_at(p, "PUBLIC"))
    {
        p->i += 6;
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after 'PUBLIC'");
        *pub = xml_parse_quoted(p, 1, "PubidLiteral");
        size_t save = p->i;
        bool blank = xml_skip_blanks(p);
        if (xml_cur(p) == '"' || xml_cur(p) == '\'')
        {
            if (!blank) xml_parser_fatal(p, p->i, "Space required after the Public Identifier");
            *sys = xml_parse_quoted(p, 2, "SystemLiteral");
        }
        else
        {
            p->i = save;
            if (!allow_public_only) xml_parser_fatal(p, p->i, "Space required after the Public Identifier");
        }
    }
}

static void xml_expect_decl_end(XmlParser *p, const char *what)
{
    xml_skip_blanks(p);
    if (xml_cur(p) != '>') xml_parser_fatal(p, p->i, "%s", what);
    p->i++;
}

/** Expand the character references in an entity value. Parameter entity references are kept
    as written: entities are never expanded by the parser, only by an explicit xmqExpandEntities. Result in slot 0. */
static const char *xml_decode_entity_value(XmlParser *p, const char *v, size_t pos)
{
    XmlPBuf *out = &p->tmp[0];
    xml_pbuf_clear(out);
    xml_pbuf_add(out, "", 0);
    size_t n = strlen(v);
    size_t k = 0;
    while (k < n)
    {
        char c = v[k];
        if (c == '&' && k + 1 < n && v[k+1] == '#')
        {
            const char *semi = strchr(v + k, ';');
            if (!semi) xml_parser_fatal(p, pos, "xmlParseCharRef: invalid decimal value");
            bool hex = v[k+2] == 'x';
            const char *d = v + k + 2 + (hex ? 1 : 0);
            if (d == semi) xml_parser_fatal(p, pos, "xmlParseCharRef: invalid decimal value");
            int cp = 0;
            for (; d < semi; ++d)
            {
                int digit;
                if (*d >= '0' && *d <= '9') digit = *d - '0';
                else if (hex && *d >= 'a' && *d <= 'f') digit = *d - 'a' + 10;
                else if (hex && *d >= 'A' && *d <= 'F') digit = *d - 'A' + 10;
                else xml_parser_fatal(p, pos, hex ? "xmlParseCharRef: invalid hexadecimal value" : "xmlParseCharRef: invalid decimal value");
                cp = cp * (hex ? 16 : 10) + digit;
                if (cp > 0x110000) cp = 0x110000;
            }
            if (!xml_is_xml_char(cp)) xml_parser_fatal(p, pos, "xmlParseCharRef: invalid xmlChar value %d", cp);
            xml_pbuf_add_cp(out, cp);
            k = semi - v + 1;
            continue;
        }
        xml_pbuf_addc(out, c);
        k++;
    }
    return out->b;
}

/** A predefined entity may only be redeclared as itself or as a character reference to itself. */
static bool xml_valid_predefined_redecl(char c, XmlEntityType type, const char *content)
{
    if (type != XML_ENTITY_INTERNAL_GENERAL || !content) return false;
    if (content[0] == c && content[1] == 0 && (c == '>' || c == '\'' || c == '"')) return true;
    if (content[0] == '&' && content[1] == '#')
    {
        char ref[8];
        if (content[2] == 'x')
        {
            const char *hex = "0123456789ABCDEF";
            snprintf(ref, sizeof(ref), "%c%c;", hex[c / 16 % 16], hex[c % 16]);
            const char *a = content + 3, *b = ref;
            while (*a && *b)
            {
                char x = *a, y = *b;
                if (x >= 'a' && x <= 'z') x -= 32;
                if (x != y) return false;
                a++; b++;
            }
            return *a == 0 && *b == 0;
        }
        snprintf(ref, sizeof(ref), "%c%c;", '0' + c / 10 % 10, '0' + c % 10);
        return !strcmp(content + 2, ref);
    }
    return false;
}

static void xml_parse_entity_decl(XmlParser *p)
{
    size_t start = p->i;
    p->i += 8;
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after '<!ENTITY'");
    bool pe = false;
    if (xml_cur(p) == '%')
    {
        p->i++;
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after '%%'");
        pe = true;
    }
    XmlSlice name = xml_parse_name(p, true);
    if (!name.s) xml_parser_fatal(p, p->i, "xmlParseEntityDecl: no name");
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after the entity name");

    // Strings: name in slot 1 (after ids are parsed), orig/content kept in malloced copies.
    char *ename = NULL;
    char *orig = NULL;
    char *content = NULL;
    char *pub = NULL;
    char *sys = NULL;
    char *ndata = NULL;
    XmlEntityType type;

    if (xml_cur(p) == '"' || xml_cur(p) == '\'')
    {
        char q = xml_cur(p);
        const char *e = (const char*)memchr(p->buf + p->i + 1, q, p->len - p->i - 1);
        if (!e) xml_parser_fatal(p, p->i, "EntityValue: \" expected");
        size_t end = e - p->buf;
        size_t vpos = p->i + 1;
        if (p->has_bad && p->bad_pos >= vpos && p->bad_pos < end)
        {
            // libxml2 xmlExpandPEsInEntityValue checks the literal once it is read.
            p->has_bad = false;
            xml_parser_fatal(p, end, "invalid character in entity value");
        }
        xml_check_text_length(p, end - vpos, vpos);
        const char *o = xml_tmp(p, 1, p->buf + vpos, end - vpos);
        p->i = end + 1;
        const char *c = xml_decode_entity_value(p, o, vpos);
        xml_expect_decl_end(p, "xmlParseEntityDecl: entity not terminated");
        orig = xml_parser_strdup(o);
        content = xml_parser_strdup(c);
        type = pe ? XML_ENTITY_INTERNAL_PARAMETER : XML_ENTITY_INTERNAL_GENERAL;
    }
    else
    {
        const char *pubid, *sysid;
        xml_parse_external_id(p, false, &pubid, &sysid);
        if (!pubid && !sysid) xml_parser_fatal(p, p->i, "Entity value required");
        if (!pe)
        {
            size_t save = p->i;
            bool blank = xml_skip_blanks(p);
            if (xml_at(p, "NDATA"))
            {
                if (!blank) xml_parser_fatal(p, p->i, "Space required before 'NDATA'");
                p->i += 5;
                xml_skip_blanks(p);
                XmlSlice nd = xml_parse_name(p, true);
                if (nd.s) ndata = xml_parser_strdup(xml_tmp_slice(p, 3, nd));
            }
            else
            {
                p->i = save;
            }
        }
        pub = xml_parser_strdup(pubid);
        sys = xml_parser_strdup(sysid);
        type = pe ? XML_ENTITY_EXTERNAL_PARAMETER
                  : (ndata ? XML_ENTITY_EXTERNAL_GENERAL_UNPARSED : XML_ENTITY_EXTERNAL_GENERAL_PARSED);
        // Make sure nothing leaks if the declaration is not terminated.
        size_t save = p->i;
        xml_skip_blanks(p);
        if (xml_cur(p) != '>')
        {
            free(pub); free(sys); free(ndata);
            p->i = save;
            xml_expect_decl_end(p, "xmlParseEntityDecl: entity not terminated");
        }
        p->i++;
    }
    (void)start;

    ename = xml_parser_strdup(xml_tmp_slice(p, 3, name));
    const char *pre = pe ? NULL : xml_predefined(name);
    if (pre && !xml_valid_predefined_redecl(pre[0], type, content))
    {
        // Port of the libxml2 xmlAddEntity check (4.6 Predefined Entities), reported and ignored.
        // libxml2 adds the entity (and warns) before the closing '>' is consumed.
        xml_parser_message(p, "parser warning", p->i - 1, "Invalid redeclaration of predefined entity '%s'", ename);
    }
    else
    {
        HashMap **map = pe ? &p->pentities : &p->entities;
        if (!*map) *map = hashmap_create(1024);
        if (!hashmap_get(*map, ename))
        {
            XMQNode *decl = xml_dtd_add_entity(p->dtd, ename, type, pub, sys, ndata, content, orig);
            if (decl) hashmap_put(*map, ename, decl);
        }
    }
    // A redeclaration is ignored (first wins). A redeclared predefined entity is kept in
    // the dtd like libxml2 does, but references still use the predefined value.
    free(ename);
    free(orig);
    free(content);
    free(pub);
    free(sys);
    free(ndata);
}

// Element content models. Port of libxml2 xmlParseElementMixedContentDecl,
// xmlParseElementChildrenContentDeclPriv and xmlBufDumpElementContent: the model is
// a binary tree of SEQ/OR nodes, including libxml2's normalizations, printed back
// exactly like libxml2 does.

typedef enum { XML_CM_PCDATA = 1, XML_CM_ELEMENT, XML_CM_SEQ, XML_CM_OR } XmlCmType;
typedef enum { XML_CM_ONCE = 1, XML_CM_OPT, XML_CM_MULT, XML_CM_PLUS } XmlCmOccur;

typedef struct
{
    XmlCmType type;
    XmlCmOccur ocur;
    size_t name_off;  // Into the input buffer, for XML_CM_ELEMENT.
    size_t name_len;
    size_t c1, c2, parent; // Indices, SIZE_MAX for none.
} XmlCmNode;

typedef struct
{
    XmlCmNode *nodes;
    size_t num;
    size_t cap;
} XmlContentModel;

#define XML_CM_NONE SIZE_MAX

static size_t xml_cm_new(XmlContentModel *cm, XmlCmType type)
{
    if (cm->num == cm->cap)
    {
        cm->cap = cm->cap ? cm->cap * 2 : 16;
        cm->nodes = (XmlCmNode*)realloc(cm->nodes, cm->cap * sizeof(XmlCmNode));
    }
    XmlCmNode *n = &cm->nodes[cm->num];
    n->type = type;
    n->ocur = XML_CM_ONCE;
    n->name_off = 0;
    n->name_len = 0;
    n->c1 = n->c2 = n->parent = XML_CM_NONE;
    return cm->num++;
}

static size_t xml_cm_new_element(XmlParser *p, XmlContentModel *cm, XmlSlice name)
{
    size_t n = xml_cm_new(cm, XML_CM_ELEMENT);
    cm->nodes[n].name_off = name.s - p->buf;
    cm->nodes[n].name_len = name.len;
    return n;
}

static void xml_cm_set_c1(XmlContentModel *cm, size_t parent, size_t child)
{
    cm->nodes[parent].c1 = child;
    if (child != XML_CM_NONE) cm->nodes[child].parent = parent;
}

static void xml_cm_set_c2(XmlContentModel *cm, size_t parent, size_t child)
{
    cm->nodes[parent].c2 = child;
    if (child != XML_CM_NONE) cm->nodes[child].parent = parent;
}

static XmlCmOccur xml_cm_parse_occur(XmlParser *p)
{
    char c = xml_cur(p);
    if (c == '?') { p->i++; return XML_CM_OPT; }
    if (c == '*') { p->i++; return XML_CM_MULT; }
    if (c == '+') { p->i++; return XML_CM_PLUS; }
    return XML_CM_ONCE;
}

/**
    An error in an element content declaration. libxml2 reports it and returns from the
    declaration at the error position, its internal subset loop then reports
    "Content error in the internal subset" there unless a declaration, a parameter entity
    reference or the end of the subset follows. The native parser stops at the first error
    it cannot recover from like that.
*/
static void xml_cm_error(XmlParser *p, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *msg = xml_parser_vasprintf(XML_MAX_ERR_MSG_SIZE, fmt, ap);
    va_end(ap);
    size_t pos = p->i;
    if (p->in_int_subset)
    {
        size_t k = pos;
        while (k < p->len && xml_is_blank(p->buf[k])) k++;
        char c = k < p->len ? p->buf[k] : 0;
        char n = k + 1 < p->len ? p->buf[k+1] : 0;
        bool goes_on = k < p->len && ((c == '<' && (n == '!' || n == '?')) || c == '%' || c == ']');
        if (!goes_on)
        {
            xml_parser_message(p, "parser error", pos, "%s", msg);
            free(msg);
            xml_parser_fatal(p, k, "Content error in the internal subset");
        }
    }
    p->cm_msg = msg; // Freed by the cleanup, the fatal call does not return.
    xml_parser_fatal(p, pos, "%s", msg);
}

/** Port of xmlParseElementMixedContentDecl, the '(' and blanks have been skipped. */
static size_t xml_cm_parse_mixed(XmlParser *p, XmlContentModel *cm)
{
    size_t ret = XML_CM_NONE, cur = XML_CM_NONE;
    XmlSlice elem = { NULL, 0 };
    p->i += 7; // #PCDATA
    xml_skip_blanks(p);
    if (xml_cur(p) == ')')
    {
        p->i++;
        ret = xml_cm_new(cm, XML_CM_PCDATA);
        if (xml_cur(p) == '*')
        {
            cm->nodes[ret].ocur = XML_CM_MULT;
            p->i++;
        }
        return ret;
    }
    if (xml_cur(p) == '(' || xml_cur(p) == '|')
    {
        ret = cur = xml_cm_new(cm, XML_CM_PCDATA);
    }
    while (xml_cur(p) == '|')
    {
        p->i++;
        size_t n = xml_cm_new(cm, XML_CM_OR);
        if (!elem.s)
        {
            xml_cm_set_c1(cm, n, cur);
            ret = cur = n;
        }
        else
        {
            xml_cm_set_c2(cm, cur, n);
            xml_cm_set_c1(cm, n, xml_cm_new_element(p, cm, elem));
            cur = n;
        }
        xml_skip_blanks(p);
        elem = xml_parse_name(p, true);
        if (!elem.s) xml_cm_error(p, "xmlParseElementMixedContentDecl : Name expected");
        xml_skip_blanks(p);
    }
    if (xml_cur(p) == ')' && xml_nxt(p, 1) == '*')
    {
        if (elem.s) xml_cm_set_c2(cm, cur, xml_cm_new_element(p, cm, elem));
        if (ret != XML_CM_NONE) cm->nodes[ret].ocur = XML_CM_MULT;
        p->i += 2;
    }
    else
    {
        xml_cm_error(p, "xmlParseElementMixedContentDecl : '|' or ')*' expected");
    }
    return ret;
}

/** Port of xmlParseElementChildrenContentDeclPriv, the '(' has been skipped. */
static size_t xml_cm_parse_children(XmlParser *p, XmlContentModel *cm, size_t depth)
{
    size_t ret = XML_CM_NONE, cur = XML_CM_NONE, last = XML_CM_NONE;
    char type = 0;

    if (depth > p->settings->max_depth)
    {
        xml_parser_fatal(p, p->i, "xmlParseElementChildrenContentDecl : depth %zu too deep, use XML_PARSE_HUGE", depth);
    }
    xml_skip_blanks(p);
    if (xml_cur(p) == '(')
    {
        p->i++;
        cur = ret = xml_cm_parse_children(p, cm, depth + 1);
    }
    else
    {
        XmlSlice elem = xml_parse_name(p, true);
        if (!elem.s) xml_cm_error(p, "ContentDecl : Name or '(' expected");
        cur = ret = xml_cm_new_element(p, cm, elem);
        cm->nodes[cur].ocur = xml_cm_parse_occur(p);
    }
    while (true)
    {
        xml_skip_blanks(p);
        char c = xml_cur(p);
        if (c == ')') break;
        if (c == ',' || c == '|')
        {
            if (type == 0) type = c;
            else if (type != c) xml_cm_error(p, "xmlParseElementChildrenContentDecl : '%c' expected", type);
            p->i++;
            size_t op = xml_cm_new(cm, c == ',' ? XML_CM_SEQ : XML_CM_OR);
            if (last == XML_CM_NONE)
            {
                xml_cm_set_c1(cm, op, ret);
                ret = cur = op;
            }
            else
            {
                xml_cm_set_c2(cm, cur, op);
                xml_cm_set_c1(cm, op, last);
                cur = op;
                last = XML_CM_NONE;
            }
        }
        else
        {
            xml_cm_error(p, "ContentDecl : ',' '|' or ')' expected");
        }
        xml_skip_blanks(p);
        if (xml_cur(p) == '(')
        {
            p->i++;
            last = xml_cm_parse_children(p, cm, depth + 1);
        }
        else
        {
            XmlSlice elem = xml_parse_name(p, true);
            if (!elem.s) xml_cm_error(p, "ContentDecl : Name or '(' expected");
            last = xml_cm_new_element(p, cm, elem);
            cm->nodes[last].ocur = xml_cm_parse_occur(p);
        }
    }
    if (cur != XML_CM_NONE && last != XML_CM_NONE) xml_cm_set_c2(cm, cur, last);
    p->i++; // )

    XmlCmNode *r = &cm->nodes[ret];
    char o = xml_cur(p);
    if (o == '?')
    {
        r->ocur = (r->ocur == XML_CM_PLUS || r->ocur == XML_CM_MULT) ? XML_CM_MULT : XML_CM_OPT;
        p->i++;
    }
    else if (o == '*')
    {
        r->ocur = XML_CM_MULT;
        // Some normalization: (a | b* | c?)* == (a | b | c)*
        size_t k = ret;
        while (k != XML_CM_NONE && cm->nodes[k].type == XML_CM_OR)
        {
            size_t c1 = cm->nodes[k].c1, c2 = cm->nodes[k].c2;
            if (c1 != XML_CM_NONE && (cm->nodes[c1].ocur == XML_CM_OPT || cm->nodes[c1].ocur == XML_CM_MULT)) cm->nodes[c1].ocur = XML_CM_ONCE;
            if (c2 != XML_CM_NONE && (cm->nodes[c2].ocur == XML_CM_OPT || cm->nodes[c2].ocur == XML_CM_MULT)) cm->nodes[c2].ocur = XML_CM_ONCE;
            k = c2;
        }
        p->i++;
    }
    else if (o == '+')
    {
        bool found = false;
        r->ocur = (r->ocur == XML_CM_OPT || r->ocur == XML_CM_MULT) ? XML_CM_MULT : XML_CM_PLUS;
        // Some normalization: (a | b*)+ == (a | b)* and (a | b?)+ == (a | b)*
        size_t k = cur;
        while (k != XML_CM_NONE && cm->nodes[k].type == XML_CM_OR)
        {
            size_t c1 = cm->nodes[k].c1, c2 = cm->nodes[k].c2;
            if (c1 != XML_CM_NONE && (cm->nodes[c1].ocur == XML_CM_OPT || cm->nodes[c1].ocur == XML_CM_MULT)) { cm->nodes[c1].ocur = XML_CM_ONCE; found = true; }
            if (c2 != XML_CM_NONE && (cm->nodes[c2].ocur == XML_CM_OPT || cm->nodes[c2].ocur == XML_CM_MULT)) { cm->nodes[c2].ocur = XML_CM_ONCE; found = true; }
            k = c2;
        }
        if (found) cm->nodes[ret].ocur = XML_CM_MULT;
        p->i++;
    }
    return ret;
}

static void xml_cm_print_occur(XmlPBuf *out, XmlCmNode *n)
{
    switch (n->ocur)
    {
    case XML_CM_OPT: xml_pbuf_addc(out, '?'); break;
    case XML_CM_MULT: xml_pbuf_addc(out, '*'); break;
    case XML_CM_PLUS: xml_pbuf_addc(out, '+'); break;
    default: break;
    }
}

/** Port of xmlBufDumpElementContent. */
static void xml_cm_print(XmlParser *p, XmlContentModel *cm, XmlPBuf *out, size_t content)
{
    xml_pbuf_addc(out, '(');
    size_t cur = content;
    do
    {
        XmlCmNode *n = &cm->nodes[cur];
        switch (n->type)
        {
        case XML_CM_PCDATA:
            xml_pbuf_add(out, "#PCDATA", 7);
            break;
        case XML_CM_ELEMENT:
            xml_pbuf_add(out, p->buf + n->name_off, n->name_len);
            break;
        case XML_CM_SEQ:
        case XML_CM_OR:
            if (cur != content && n->parent != XML_CM_NONE &&
                (n->type != cm->nodes[n->parent].type || n->ocur != XML_CM_ONCE))
            {
                xml_pbuf_addc(out, '(');
            }
            cur = n->c1;
            continue;
        }

        while (cur != content)
        {
            XmlCmNode *c = &cm->nodes[cur];
            size_t parent = c->parent;
            XmlCmNode *pn = &cm->nodes[parent];
            if ((c->type == XML_CM_OR || c->type == XML_CM_SEQ) &&
                (c->type != pn->type || c->ocur != XML_CM_ONCE))
            {
                xml_pbuf_addc(out, ')');
            }
            xml_cm_print_occur(out, c);
            if (cur == pn->c1)
            {
                if (pn->type == XML_CM_SEQ) xml_pbuf_add(out, " , ", 3);
                else if (pn->type == XML_CM_OR) xml_pbuf_add(out, " | ", 3);
                cur = pn->c2;
                break;
            }
            cur = parent;
        }
    } while (cur != content);
    xml_pbuf_addc(out, ')');
    xml_cm_print_occur(out, &cm->nodes[content]);
}

static void xml_parse_element_decl(XmlParser *p)
{
    p->i += 9;
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after 'ELEMENT'");
    XmlSlice name = xml_parse_name(p, true);
    if (!name.s) xml_parser_fatal(p, p->i, "xmlParseElementDecl: no name for Element");
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after the element name");

    XmlPBuf *out = &p->tmp[1];
    xml_pbuf_clear(out);
    xml_pbuf_add(out, "<!ELEMENT ", 10);
    xml_pbuf_add(out, name.s, name.len);
    xml_pbuf_addc(out, ' ');

    if (xml_at(p, "EMPTY"))
    {
        p->i += 5;
        xml_pbuf_add(out, "EMPTY", 5);
    }
    else if (xml_at(p, "ANY"))
    {
        p->i += 3;
        xml_pbuf_add(out, "ANY", 3);
    }
    else if (xml_cur(p) == '(')
    {
        // The content model array is owned here; make sure it is freed if parsing fails.
        XmlContentModel cm = { NULL, 0, 0 };
        jmp_buf save;
        memcpy(&save, &p->on_error, sizeof(jmp_buf));
        if (setjmp(p->on_error))
        {
            free(cm.nodes);
            memcpy(&p->on_error, &save, sizeof(jmp_buf));
            longjmp(p->on_error, 1);
        }
        p->i++;
        xml_skip_blanks(p);
        size_t root;
        if (xml_at(p, "#PCDATA")) root = xml_cm_parse_mixed(p, &cm);
        else root = xml_cm_parse_children(p, &cm, 1);
        xml_cm_print(p, &cm, out, root);
        free(cm.nodes);
        memcpy(&p->on_error, &save, sizeof(jmp_buf));
    }
    else
    {
        xml_parser_fatal(p, p->i, "xmlParseElementDecl: 'EMPTY', 'ANY' or '(' expected");
    }
    xml_expect_decl_end(p, "expected '>' at the end of the element declaration");

    const char *n = xml_tmp_slice(p, 3, name);
    if (!p->element_decls) p->element_decls = hashmap_create(1024);
    if (hashmap_get(p->element_decls, n)) return;
    // Copy the name, since hashmap_put below must not see a reused scratch buffer.
    char *nc = xml_parser_strdup(n);
    XMQNode *d = xml_dtd_add_decl(p->dtd, XML_DOM_ELEMENT_DECL, nc, out->b, NULL);
    hashmap_put(p->element_decls, nc, d);
    free(nc);
}

/** Parse (a|b|c) and append "a | b | c" to out. */
static void xml_parse_enumeration(XmlParser *p, XmlPBuf *out)
{
    if (xml_cur(p) != '(') xml_parser_fatal(p, p->i, "'(' required to start ATTLIST enumeration");
    p->i++;
    bool first = true;
    while (true)
    {
        xml_skip_blanks(p);
        XmlSlice n = xml_parse_nmtoken(p);
        if (!n.s) xml_parser_fatal(p, p->i, "NmToken expected in ATTLIST enumeration");
        if (!first) xml_pbuf_add(out, " | ", 3);
        first = false;
        xml_pbuf_add(out, n.s, n.len);
        xml_skip_blanks(p);
        if (xml_cur(p) == ')')
        {
            p->i++;
            return;
        }
        if (xml_cur(p) != '|') xml_parser_fatal(p, p->i, "')' required to finish ATTLIST enumeration");
        p->i++;
    }
}

static bool xml_attlist_exists(XmlParser *p, const char *prefix)
{
    size_t pl = strlen(prefix);
    for (XMQNode *d = xml_node_first_child(p->dtd); d; d = xml_node_next(d))
    {
        if (xml_node_kind(d) != XML_DOM_ATTRIBUTE_DECL) continue;
        const char *t = xml_node_content(d);
        if (!strncmp(t, prefix, pl) && (t[pl] == ' ' || t[pl] == 0)) return true;
    }
    return false;
}

static void xml_parse_attlist_decl(XmlParser *p)
{
    p->i += 9;
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after '<!ATTLIST'");
    XmlSlice elem = xml_parse_name(p, true);
    if (!elem.s) xml_parser_fatal(p, p->i, "ATTLIST: no name for Element");
    while (true)
    {
        xml_skip_blanks(p);
        if (xml_cur(p) == '>')
        {
            p->i++;
            return;
        }
        XmlSlice attr = xml_parse_name(p, true);
        if (!attr.s) xml_parser_fatal(p, p->i, "ATTLIST: no name for Attribute");
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after the attribute name");
        XmlPBuf *decl = &p->tmp[1];
        xml_pbuf_clear(decl);
        xml_pbuf_add(decl, "<!ATTLIST ", 10);
        xml_pbuf_add(decl, elem.s, elem.len);
        xml_pbuf_addc(decl, ' ');
        xml_pbuf_add(decl, attr.s, attr.len);
        size_t key_len = decl->len;
        bool cdata = false;
        if (xml_cur(p) == '(')
        {
            xml_pbuf_add(decl, " (", 2);
            xml_parse_enumeration(p, decl);
            xml_pbuf_addc(decl, ')');
        }
        else
        {
            XmlSlice type = xml_parse_name(p, true);
            if (!type.s) xml_parser_fatal(p, p->i, "attribute type expected");
            if (xml_slice_eq(type, "NOTATION"))
            {
                xml_skip_blanks(p);
                xml_pbuf_add(decl, " NOTATION (", 11);
                xml_parse_enumeration(p, decl);
                xml_pbuf_addc(decl, ')');
            }
            else
            {
                xml_pbuf_addc(decl, ' ');
                xml_pbuf_add(decl, type.s, type.len);
                cdata = xml_slice_eq(type, "CDATA");
            }
        }
        if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after the attribute type");
        const char *default_value = NULL;
        if (xml_at(p, "#REQUIRED"))
        {
            p->i += 9;
            xml_pbuf_add(decl, " #REQUIRED", 10);
        }
        else if (xml_at(p, "#IMPLIED"))
        {
            p->i += 8;
            xml_pbuf_add(decl, " #IMPLIED", 9);
        }
        else
        {
            if (xml_at(p, "#FIXED"))
            {
                p->i += 6;
                xml_pbuf_add(decl, " #FIXED", 7);
                if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after '#FIXED'");
            }
            p->num_parts = 0;
            xml_pbuf_clear(&p->attval);
            size_t count = xml_parse_att_value(p);
            default_value = xml_att_default_string(p, 2, 0, count);
            if (!cdata) xml_normalize_space(p->tmp[2].b);
        }

        // Remember the attribute type, the first declaration wins (libxml2 attsSpecial).
        {
            XmlPBuf *tk = &p->tmp[3];
            xml_pbuf_clear(tk);
            xml_pbuf_add(tk, elem.s, elem.len);
            xml_pbuf_addc(tk, 1);
            xml_pbuf_add(tk, attr.s, attr.len);
            if (!p->attr_types) p->attr_types = hashmap_create(1024);
            if (!hashmap_get(p->attr_types, tk->b)) hashmap_put(p->attr_types, tk->b, cdata ? (void*)1 : (void*)2);
        }

        char *key = xml_parser_strdup(xml_tmp(p, 0, decl->b, key_len));
        if (!xml_attlist_exists(p, key))
        {
            xml_dtd_add_decl(p->dtd, XML_DOM_ATTRIBUTE_DECL, xml_tmp_slice(p, 3, attr), decl->b, default_value);
            if (default_value &&
                (p->settings->dtd_attr || xml_slice_eq(attr, "xmlns") || (attr.len > 6 && !memcmp(attr.s, "xmlns:", 6))))
            {
                if (p->num_defaults == p->defaults_cap)
                {
                    p->defaults_cap = p->defaults_cap ? p->defaults_cap * 2 : 8;
                    p->defaults = (XmlAttDefault*)realloc(p->defaults, p->defaults_cap * sizeof(XmlAttDefault));
                }
                XmlAttDefault *d = &p->defaults[p->num_defaults++];
                d->element = xml_parser_strdup(xml_tmp_slice(p, 3, elem));
                d->attr = xml_parser_strdup(xml_tmp_slice(p, 3, attr));
                d->value = xml_parser_strdup(default_value);
            }
        }
        free(key);
    }
}

static void xml_parse_notation_decl(XmlParser *p)
{
    p->i += 10;
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after '<!NOTATION'");
    XmlSlice name = xml_parse_name(p, true);
    if (!name.s) xml_parser_fatal(p, p->i, "Name expected in NOTATION declaration");
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after the NOTATION name'");
    const char *pub, *sys;
    xml_parse_external_id(p, true, &pub, &sys);
    xml_expect_decl_end(p, "'>' required to close NOTATION declaration");
    MemBuffer *mb = new_membuffer();
    membuffer_append(mb, "<!NOTATION ");
    membuffer_append_region(mb, name.s, name.s + name.len);
    if (pub)
    {
        membuffer_append(mb, " PUBLIC ");
        xml_save_write_quoted(mb, pub);
        if (sys)
        {
            membuffer_append_char(mb, ' ');
            xml_save_write_quoted(mb, sys);
        }
    }
    else
    {
        membuffer_append(mb, " SYSTEM ");
        xml_save_write_quoted(mb, sys);
    }
    membuffer_append(mb, " ");
    membuffer_append_null(mb);
    char *text = free_membuffer_but_return_trimmed_content(mb);
    xml_dtd_add_notation(p->dtd, xml_tmp_slice(p, 3, name), text);
    free(text);
}

static void xml_parse_internal_subset(XmlParser *p)
{
    p->in_int_subset = true;
    while (true)
    {
        xml_skip_blanks(p);
        if (xml_eof(p)) xml_parser_fatal(p, p->i, "Content error in the internal subset");
        if (xml_cur(p) == ']')
        {
            p->i++;
            p->in_int_subset = false;
            return;
        }
        if (xml_at(p, "<!ENTITY")) xml_parse_entity_decl(p);
        else if (xml_at(p, "<!ELEMENT")) xml_parse_element_decl(p);
        else if (xml_at(p, "<!ATTLIST")) xml_parse_attlist_decl(p);
        else if (xml_at(p, "<!NOTATION")) xml_parse_notation_decl(p);
        else if (xml_at(p, "<!--")) xml_parse_comment(p);
        else if (xml_at(p, "<?")) xml_parse_pi(p);
        else if (xml_cur(p) == '%')
        {
            p->i++;
            XmlSlice n = xml_parse_name(p, true);
            if (!n.s || xml_cur(p) != ';') xml_parser_fatal(p, p->i, "PEReference: no name");
            p->i++;
            p->has_pe_refs = true;
            // Not expanded, kept as a reference: only an explicit xmqExpandEntities expands entities.
            xml_dtd_add_pe_ref(p->dtd, xml_tmp_slice(p, 3, n));
        }
        else
        {
            // Like libxml2 xmlParseMarkupDecl, an unknown declaration starting with
            // <!E <!A <!N or <!- is skipped by two characters before the error.
            char k = xml_nxt(p, 2);
            bool skip = xml_cur(p) == '<' && xml_nxt(p, 1) == '!' && (k == 'E' || k == 'A' || k == 'N' || k == '-');
            xml_parser_fatal(p, p->i + (skip ? 2 : 0), "Content error in the internal subset");
        }
    }
}

static void xml_parse_doctype_decl(XmlParser *p)
{
    p->i += 9;
    if (!xml_skip_blanks(p)) xml_parser_fatal(p, p->i, "Space required after 'DOCTYPE'");
    XmlSlice name = xml_parse_name(p, true);
    if (!name.s) xml_parser_fatal(p, p->i, "xmlParseDocTypeDecl : no DOCTYPE name !");
    xml_skip_blanks(p);
    const char *pub, *sys;
    xml_parse_external_id(p, false, &pub, &sys);
    if (pub || sys) p->has_external_subset = true;
    p->dtd = xml_dom_set_dtd(p->dom, xml_tmp_slice(p, 3, name), pub, sys);
    xml_skip_blanks(p);
    if (xml_cur(p) == '[')
    {
        p->i++;
        p->in_subset = true;
        xml_parse_internal_subset(p);
        p->in_subset = false;
        xml_skip_blanks(p);
    }
    if (xml_cur(p) != '>') xml_parser_fatal(p, p->i, "DOCTYPE improperly terminated");
    p->i++;
}

// Document ///////////////////////////////////////////////////////

static void xml_parse_document(XmlParser *p)
{
    if (xml_eof(p)) xml_parser_fatal(p, p->i, "Document is empty");
    if (xml_at(p, "<?xml") && xml_is_blank(xml_nxt(p, 5))) xml_parse_xml_decl(p);
    xml_parse_misc(p);
    if (xml_at(p, "<!DOCTYPE"))
    {
        xml_parse_doctype_decl(p);
        xml_parse_misc(p);
    }
    if (xml_eof(p) || xml_cur(p) != '<') xml_parser_fatal(p, p->i, "Start tag expected, '<' not found");
    xml_parse_element_tree(p);
    xml_parse_misc(p);
    if (!xml_eof(p)) xml_parser_fatal(p, p->i, "Extra content at the end of the document");
}

void xml_parse_settings_default(XmlParseSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->max_depth = 10000; // Like the xmq, json, yaml and ixml parsers (libxml2 has 256).
    s->max_name_length = 50000;
    s->max_text_length = 10000000;
    s->max_amplification = 5;
}

static void xml_parser_cleanup(XmlParser *p)
{
    free(p->cm_msg);
    for (size_t k = 0; k < p->depth; ++k) free(p->stack[k].qname);
    free(p->stack);
    xml_pbuf_free(&p->text);
    xml_pbuf_free(&p->attval);
    for (int k = 0; k < 6; ++k) xml_pbuf_free(&p->tmp[k]);
    free(p->parts);
    free(p->attrs);
    if (p->entities) hashmap_free(p->entities);
    if (p->pentities) hashmap_free(p->pentities);
    if (p->element_decls) hashmap_free(p->element_decls);
    if (p->attr_types) hashmap_free(p->attr_types);
    for (size_t k = 0; k < p->num_defaults; ++k)
    {
        free(p->defaults[k].element);
        free(p->defaults[k].attr);
        free(p->defaults[k].value);
    }
    free(p->defaults);
    free(p->owned);
    free(p->crmark);
}

XmlDom *xml_parse_buffer(const char *start, const char *stop, const XmlParseSettings *settings, XmlParseError *error)
{
    XmlParseSettings defaults;
    if (!settings)
    {
        xml_parse_settings_default(&defaults);
        settings = &defaults;
    }
    if (error) memset(error, 0, sizeof(*error));
    if (!stop) stop = start + strlen(start);

    XmlParser *p = (XmlParser*)calloc(1, sizeof(XmlParser));
    p->settings = settings;
    p->error = error;
    p->input_len = stop - start;
    p->line_pos = 0;
    p->line_no = 1;
    p->buf = start;
    p->len = stop - start;

    if (setjmp(p->on_error))
    {
        xml_dom_free(p->dom);
        xml_parser_cleanup(p);
        free(p);
        return NULL;
    }

    p->dom = xml_dom_new();
    p->node = xml_dom_document(p->dom);
    xml_parser_prepare_input(p, start, stop);
    xml_parse_document(p);
    xml_flush_text(p);
    if (p->has_bad) xml_parser_fatal(p, p->bad_pos, "Char 0x%X out of allowed range", p->bad_cp);
    if (p->io_error_at_end)
    {
        p->fatal_kind = "I/O error";
        xml_parser_fatal(p, p->len, "Invalid bytes in character encoding");
    }

    XmlDom *dom = p->dom;
    xml_parser_cleanup(p);
    free(p);
    return dom;
}

// Entity expansion /////////////////////////////////////////////////

#define XML_ENTITY_MAX_NESTING 20
#define XML_ENTITY_ALLOWED_EXPANSION 1000000

/** Parse content (text, references, elements, comments, pis, cdata) appended to p->node,
    used for entity replacement text. */
static void xml_parse_content_chunk(XmlParser *p, const char *s, size_t len)
{
    p->buf = s;
    p->len = len;
    p->i = 0;
    p->line_pos = 0;
    p->line_no = 1;
    p->has_text = false;
    // The replacement text is an input of its own, its end of lines are normalized.
    xml_normalize_eol(p);
    XMQNode *base = p->node;
    size_t base_depth = p->depth;
    size_t outer_ns_base_depth = p->ns_base_depth;
    p->ns_base_depth = base_depth;
    while (!xml_eof(p))
    {
        char c = xml_cur(p);
        if (c == '<')
        {
            char n = xml_nxt(p, 1);
            if (n == '/')
            {
                if (p->depth == base_depth) xml_parser_fatal(p, p->i, "chunk is not well balanced");
                XmlOpen *top = xml_top(p);
                xml_parse_end_tag(p);
                xml_flush_text(p);
                free(top->qname);
                p->depth--;
                p->node = p->depth > base_depth ? p->stack[p->depth-1].element : base;
            }
            else if (n == '?') xml_parse_pi(p);
            else if (xml_at(p, "<!--")) xml_parse_comment(p);
            else if (xml_at(p, "<![CDATA[")) xml_parse_cdata(p);
            else if (n == '!') xml_parser_fatal(p, p->i, "StartTag: invalid element name");
            else xml_parse_start_tag(p);
        }
        else if (c == '&')
        {
            xml_parse_reference(p);
        }
        else
        {
            xml_parse_char_data(p);
        }
    }
    if (p->depth > base_depth)
    {
        XmlOpen *top = xml_top(p);
        xml_parser_fatal(p, p->i, "Premature end of data in tag %s line %zu", top->qname, top->line);
    }
    p->ns_base_depth = outer_ns_base_depth;
    xml_flush_text(p);
}

/** Find the internal general entity to expand for a reference, NULL if it is kept. */
static XMQNode *xml_expandable_entity(XmlParser *p, const char *name)
{
    if (!name || name[0] == '#') return NULL; // Character reference node (from xmq).
    if (p->only && p->num_active == 0 && strcmp(name, p->only)) return NULL;
    XMQNode *decl = xml_dom_get_entity(p->dom, name);
    if (!decl) return NULL; // Undeclared, kept.
    if (xml_entity_decl_type(decl) != XML_ENTITY_INTERNAL_GENERAL) return NULL; // External entities are never loaded.
    return decl;
}

static void xml_entity_enter(XmlParser *p, XMQNode *decl, const char *name, size_t value_len)
{
    for (size_t k = 0; k < p->num_active; ++k)
    {
        if (p->active[k] == decl) xml_parser_fatal(p, 0, "Detected an entity reference loop");
    }
    // libxml2 limits the input stack, the document is one of the inputs.
    if (p->num_active + 1 >= XML_ENTITY_MAX_NESTING) xml_parser_fatal(p, 0, "Maximum entity nesting depth exceeded");
    p->expanded_size += value_len;
    if (p->expanded_size > XML_ENTITY_ALLOWED_EXPANSION &&
        p->expanded_size / p->settings->max_amplification > p->input_len)
    {
        xml_parser_fatal(p, 0, "Maximum entity amplification factor exceeded, see xmlCtxtSetMaxAmplification.");
    }
    if (p->num_active == p->active_cap)
    {
        p->active_cap = p->active_cap ? p->active_cap * 2 : 8;
        p->active = (XMQNode**)realloc(p->active, p->active_cap * sizeof(XMQNode*));
    }
    p->active[p->num_active++] = decl;
    (void)name;
}

static void xml_expand_node_children(XmlParser *p, XMQNode *node);

/** Append the expanded replacement text of an entity referenced from an attribute value. */
static void xml_expand_attr_entity(XmlParser *p, XmlPBuf *out, XMQNode *decl, const char *name)
{
    const char *v = xml_node_content(decl);
    if (!v) v = "";
    size_t len = strlen(v);
    if (strchr(v, '<')) xml_parser_fatal(p, 0, "'<' in entity '%s' is not allowed in attributes values", name);
    xml_entity_enter(p, decl, name, len);
    p->buf = v;
    p->len = len;
    p->line_pos = 0;
    p->line_no = 1;
    p->crmark = NULL;
    size_t i = 0;
    while (i < len)
    {
        char c = v[i];
        if (c == '&')
        {
            p->i = i;
            if (v[i+1] == '#')
            {
                int cp = xml_parse_char_ref(p);
                i = p->i;
                xml_pbuf_add_cp(out, cp);
                continue;
            }
            XmlSlice ename = xml_parse_entity_ref_name(p);
            i = p->i;
            const char *pre = xml_predefined(ename);
            if (pre)
            {
                xml_pbuf_add(out, pre, 1);
                continue;
            }
            XMQNode *d = xml_dom_get_entity(p->dom, xml_tmp_slice(p, 1, ename));
            if (!d) xml_parser_fatal(p, p->i, "Entity '%.*s' not defined", (int)ename.len, ename.s);
            if (xml_entity_decl_type(d) != XML_ENTITY_INTERNAL_GENERAL)
            {
                xml_parser_fatal(p, p->i, "Attribute references external entity '%.*s'", (int)ename.len, ename.s);
            }
            xml_expand_attr_entity(p, out, d, xml_node_name(d));
            // Restore the scan of this value.
            p->buf = v;
            p->len = len;
            continue;
        }
        if (c == '\t' || c == '\n' || c == '\r') xml_pbuf_addc(out, ' ');
        else xml_pbuf_addc(out, c);
        i++;
    }
    xml_check_text_length(p, out->len, 0);
    p->num_active--;
}

/** The qualified name of an element or attribute node. */
static const char *xml_expand_qname(XmlParser *p, int slot, XMQNode *n)
{
    XmlPBuf *b = &p->tmp[slot];
    xml_pbuf_clear(b);
    const char *prefix = xml_node_ns_prefix(n);
    if (prefix)
    {
        xml_pbuf_add(b, prefix, strlen(prefix));
        xml_pbuf_addc(b, ':');
    }
    const char *name = xml_node_name(n);
    xml_pbuf_add(b, name, strlen(name));
    return b->b;
}

/** Remember the attribute types declared in the dtd, the first declaration wins (libxml2 attsSpecial). */
static void xml_expand_collect_attr_types(XmlParser *p)
{
    for (XMQNode *d = xml_node_first_child(p->dtd); d; d = xml_node_next(d))
    {
        if (xml_node_kind(d) != XML_DOM_ATTRIBUTE_DECL) continue;
        // The declaration is "<!ATTLIST element attr type ...".
        const char *t = xml_node_content(d);
        if (!t || strncmp(t, "<!ATTLIST ", 10)) continue;
        const char *e = t + 10;
        const char *es = strchr(e, ' ');
        if (!es) continue;
        const char *a = es + 1;
        const char *as = strchr(a, ' ');
        if (!as) continue;
        const char *ty = as + 1;
        bool cdata = !strncmp(ty, "CDATA", 5) && (ty[5] == ' ' || ty[5] == 0);
        XmlPBuf *tk = &p->tmp[3];
        xml_pbuf_clear(tk);
        xml_pbuf_add(tk, e, es - e);
        xml_pbuf_addc(tk, 1);
        xml_pbuf_add(tk, a, as - a);
        if (!p->attr_types) p->attr_types = hashmap_create(1024);
        if (!hashmap_get(p->attr_types, tk->b)) hashmap_put(p->attr_types, tk->b, cdata ? (void*)1 : (void*)2);
    }
}

/**
    Like libxml2 xmlAttrNormalizeSpace2 for the expanded value of an attribute declared with a
    non CDATA type: remove leading and trailing spaces and collapse runs of spaces into one.
    Kept entity references count as non space.
*/
static void xml_expand_normalize_attr(XmlParser *p, XMQNode *an)
{
    bool started = false;
    bool pending_space = false;
    XmlPBuf *b = &p->tmp[4];
    XMQNode *c = xml_node_first_child(an);
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        if (xml_node_kind(c) != XML_DOM_TEXT)
        {
            if (pending_space) xml_node_add_prev_sibling(c, xml_node_new_text_len(p->dom, " ", 1));
            pending_space = false;
            started = true;
            c = next;
            continue;
        }
        const char *t = xml_node_content(c);
        xml_pbuf_clear(b);
        xml_pbuf_add(b, "", 0);
        for (; t && *t; ++t)
        {
            if (*t == ' ')
            {
                if (started) pending_space = true;
                continue;
            }
            if (pending_space) xml_pbuf_addc(b, ' ');
            pending_space = false;
            xml_pbuf_addc(b, *t);
            started = true;
        }
        if (b->len) xml_node_add_prev_sibling(c, xml_node_new_text_len(p->dom, b->b, b->len));
        xml_node_free(c);
        c = next;
    }
}

static void xml_expand_attr(XmlParser *p, XMQNode *element, XMQNode *an)
{
    bool any = false;
    for (XMQNode *c = xml_node_first_child(an); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_ENTITY_REF && xml_expandable_entity(p, xml_node_name(c))) any = true;
    }
    if (!any) return;
    XmlPBuf *out = &p->attr_out;
    xml_pbuf_clear(out);
    XMQNode *c = xml_node_first_child(an);
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        XMQNode *decl = NULL;
        if (xml_node_kind(c) == XML_DOM_TEXT)
        {
            const char *t = xml_node_content(c);
            if (t) xml_pbuf_add(out, t, strlen(t));
            xml_node_free(c);
        }
        else if (xml_node_kind(c) == XML_DOM_ENTITY_REF && (decl = xml_expandable_entity(p, xml_node_name(c))))
        {
            xml_node_free(c);
            xml_expand_attr_entity(p, out, decl, xml_node_name(decl));
        }
        else
        {
            // A kept reference: the text before it becomes a text node.
            if (out->len)
            {
                xml_node_add_prev_sibling(c, xml_node_new_text_len(p->dom, out->b, out->len));
                xml_pbuf_clear(out);
            }
        }
        c = next;
    }
    if (out->len) xml_node_append_child(an, xml_node_new_text_len(p->dom, out->b, out->len));
    xml_pbuf_clear(out);
    if (p->attr_types)
    {
        XmlPBuf *k = &p->tmp[5];
        xml_pbuf_clear(k);
        const char *eq = xml_expand_qname(p, 3, element);
        xml_pbuf_add(k, eq, strlen(eq));
        xml_pbuf_addc(k, 1);
        const char *aq = xml_expand_qname(p, 3, an);
        xml_pbuf_add(k, aq, strlen(aq));
        if (hashmap_get(p->attr_types, k->b) == (void*)2) xml_expand_normalize_attr(p, an);
    }
}

static void xml_expand_ref(XmlParser *p, XMQNode *ref, XMQNode *decl)
{
    const char *v = xml_node_content(decl);
    if (!v) v = "";
    size_t len = strlen(v);
    xml_entity_enter(p, decl, xml_node_name(decl), len);

    // Parse into a temporary container at the position of the reference, so that
    // the namespaces in scope there are used.
    XMQNode *container = xml_node_new_element(p->dom, "entity", NULL);
    xml_node_add_prev_sibling(ref, container);
    if (!p->container) p->container = container;
    XMQNode *save_node = p->node;
    p->node = container;
    xml_parse_content_chunk(p, v, len);
    p->node = save_node;
    xml_expand_node_children(p, container);
    p->num_active--;

    XMQNode *c = xml_node_first_child(container);
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        xml_node_unlink(c);
        xml_node_add_prev_sibling(ref, c);
        c = next;
    }
    if (p->container == container) p->container = NULL;
    xml_node_free(container);
    xml_node_free(ref);
}

static void xml_expand_node_children(XmlParser *p, XMQNode *node)
{
    if (xml_node_kind(node) == XML_DOM_ELEMENT)
    {
        for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
        {
            xml_expand_attr(p, node, xml_attr_node(a));
        }
    }
    XMQNode *c = xml_node_first_child(node);
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        XmlNodeKind k = xml_node_kind(c);
        if (k == XML_DOM_ENTITY_REF)
        {
            XMQNode *decl = xml_expandable_entity(p, xml_node_name(c));
            if (decl) xml_expand_ref(p, c, decl);
        }
        else if (k == XML_DOM_ELEMENT)
        {
            xml_expand_node_children(p, c);
        }
        c = next;
    }
}

/** Merge adjacent text nodes, created where expanded text meets existing text. */
static void xml_merge_text(XMQNode *node)
{
    XMQNode *c = xml_node_first_child(node);
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        if (xml_node_kind(c) == XML_DOM_TEXT)
        {
            while (next && xml_node_kind(next) == XML_DOM_TEXT)
            {
                const char *t = xml_node_content(next);
                xml_node_append_content(c, t ? t : "", t ? strlen(t) : 0);
                XMQNode *nn = xml_node_next(next);
                xml_node_free(next);
                next = nn;
            }
        }
        else if (xml_node_kind(c) == XML_DOM_ELEMENT)
        {
            xml_merge_text(c);
        }
        c = next;
    }
}

bool xml_expand_entities(XmlDom *dom, XMQNode *node, const char *only, size_t input_size,
                         const XmlParseSettings *settings, XmlParseError *error)
{
    XmlParseSettings defaults;
    if (!settings)
    {
        xml_parse_settings_default(&defaults);
        settings = &defaults;
    }
    if (error) memset(error, 0, sizeof(*error));
    if (!node) node = xml_dom_document(dom);
    if (!xml_dom_dtd(dom)) return true; // No declared entities.

    XmlParser *p = (XmlParser*)calloc(1, sizeof(XmlParser));
    p->settings = settings;
    p->error = error;
    p->input_len = input_size;
    p->dom = dom;
    p->dtd = xml_dom_dtd(dom);
    p->expanding = true;
    p->only = only;
    p->has_external_subset = xml_dtd_system_id(p->dtd) || xml_dtd_public_id(p->dtd);
    for (XMQNode *c = xml_node_first_child(p->dtd); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_PE_REF) p->has_pe_refs = true;
    }
    p->buf = "";
    p->line_no = 1;
    xml_expand_collect_attr_types(p);

    if (setjmp(p->on_error))
    {
        // Remove the partially parsed content, the reference being expanded stays.
        if (p->container) xml_node_free(p->container);
        free(p->active);
        xml_pbuf_free(&p->attr_out);
        xml_parser_cleanup(p);
        free(p);
        return false;
    }

    xml_expand_node_children(p, node);
    xml_merge_text(node);

    free(p->active);
    xml_pbuf_free(&p->attr_out);
    xml_parser_cleanup(p);
    free(p);
    return true;
}

void xml_parse_error_free(XmlParseError *error)
{
    if (!error) return;
    free(error->message);
    free(error->short_message);
    error->message = NULL;
    error->short_message = NULL;
}

#undef XML_PARSER_NORETURN

#endif // XML_PARSER_MODULE
