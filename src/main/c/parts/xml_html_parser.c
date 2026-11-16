/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/* Portions of this file are ported from libxml2 (HTMLparser.c and SAX2.c, version 2.16.0
   as found in the libxmq fork in 3rdparty/libxml2).
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

#include"xml_html_parser.h"
#include"xml_html.h"
#include"hashmap.h"
#include"xml_parser.h"

#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef XML_HTML_PARSER_MODULE

// Insertion modes, libxml2 ctxt->html.
#define XML_HTML_INSERT_INITIAL 1
#define XML_HTML_INSERT_IN_HEAD 3
#define XML_HTML_INSERT_IN_BODY 10

// Like libxml2 HTML_PARSER_BUFFER_SIZE: longer names are truncated.
#define XML_HTML_MAX_NAME 100

typedef struct
{
    char *b;
    size_t len;
    size_t cap;
} XmlHtmlBuf;

typedef struct
{
    XMQNode *node;
    char *name;
} XmlHtmlOpen;

typedef struct
{
    char *name;
    char *value; // NULL if the attribute has no value.
} XmlHtmlAttr;

typedef struct
{
    const XmlHtmlParseSettings *settings;
    const unsigned char *buf;
    size_t len;
    size_t i;

    XmlDom *dom;
    XmlHtmlOpen *stack;   // The name stack (libxml2 nameTab) and node stack (nodeTab) in one.
    size_t nr;
    size_t cap;
    int insert_mode;      // libxml2 ctxt->html
    int depth;            // Misplaced html/head/body start tags that were discarded.
    int end_check_state;  // The raw text mode of the current element.
    bool in_misc;         // Before the content, a doctype is only accepted here.

    // Pending character data, merged like libxml2 xmlSAX2Text does.
    XmlHtmlBuf text;
    bool has_text;
    XmlNodeKind text_kind;
    XMQNode *text_parent;

    XmlHtmlBuf data;      // Result of xml_html_parse_data.
    XmlHtmlBuf name;      // Result of xml_html_parse_name.

    XmlHtmlAttr *atts;
    size_t natts;
    size_t atts_cap;

    size_t line_pos;
    size_t line_no;
} XmlHtmlParser;

// Buffers ////////////////////////////////////////////////////////

static void xml_hbuf_add(XmlHtmlBuf *b, const void *s, size_t len)
{
    if (b->len + len + 1 > b->cap)
    {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + len + 1) nc *= 2;
        b->b = (char*)realloc(b->b, nc);
        b->cap = nc;
    }
    memcpy(b->b + b->len, s, len);
    b->len += len;
    b->b[b->len] = 0;
}

static void xml_hbuf_clear(XmlHtmlBuf *b)
{
    b->len = 0;
    if (b->b) b->b[0] = 0;
    else xml_hbuf_add(b, "", 0);
}

static char *xml_html_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = (char*)malloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

// Character classes ///////////////////////////////////////////////

static bool xml_html_is_ws(int c)
{
    return c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == 0x20;
}

static bool xml_html_is_letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool xml_html_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static bool xml_html_is_hex(int c)
{
    return xml_html_is_digit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
}

static int xml_html_ascii_casecmp_p(const char *a, const char *b)
{
    while (*a && *b)
    {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int xml_html_upper(int c)
{
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

/** Port of htmlValidateUtf8: the length of a valid sequence, or -1. */
static int xml_html_validate_utf8(const unsigned char *str, size_t len)
{
    unsigned c = str[0];
    if (c < 0xC2) return -1;
    if (c < 0xE0)
    {
        if (len < 2 || (str[1] & 0xC0) != 0x80) return -1;
        return 2;
    }
    if (c < 0xF0)
    {
        if (len < 3) return -1;
        unsigned v = c << 16 | str[1] << 8 | str[2];
        if (((v & 0x00C0C0) != 0x008080) || ((v & 0x0F2000) == 0x000000) || ((v & 0x0F2000) == 0x0D2000)) return -1;
        return 3;
    }
    if (len < 4) return -1;
    unsigned v = c << 24 | str[1] << 16 | str[2] << 8 | str[3];
    if (((v & 0x00C0C0C0) != 0x00808080) || (v < 0xF0900000) || (v >= 0xF4900000)) return -1;
    return 4;
}

static const short xml_html_c1_remap[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

/** Port of htmlCodePointToUtf8. */
static size_t xml_html_cp_to_utf8(int c, unsigned char *out)
{
    size_t i = 0;
    int bits, hi;
    if (c >= 0x80 && c < 0xA0) c = xml_html_c1_remap[c - 0x80];
    else if (c <= 0 || (c >= 0xD800 && c < 0xE000) || c > 0x10FFFF) c = 0xFFFD;
    if (c < 0x80) { bits = 0; hi = 0x00; }
    else if (c < 0x800) { bits = 6; hi = 0xC0; }
    else if (c < 0x10000) { bits = 12; hi = 0xE0; }
    else { bits = 18; hi = 0xF0; }
    out[i++] = (unsigned char)((c >> bits) | hi);
    while (bits > 0)
    {
        bits -= 6;
        out[i++] = (unsigned char)(((c >> bits) & 0x3F) | 0x80);
    }
    return i;
}

// Input helpers ///////////////////////////////////////////////////

static int xml_hcur(XmlHtmlParser *p)
{
    return p->i < p->len ? p->buf[p->i] : 0;
}

static int xml_hnxt(XmlHtmlParser *p, size_t n)
{
    return p->i + n < p->len ? p->buf[p->i + n] : 0;
}

static int xml_hupp(XmlHtmlParser *p, size_t n)
{
    return xml_html_upper(xml_hnxt(p, n));
}

static void xml_html_skip_blanks(XmlHtmlParser *p)
{
    while (p->i < p->len && xml_html_is_ws(p->buf[p->i])) p->i++;
}

static size_t xml_html_line(XmlHtmlParser *p)
{
    size_t pos = p->i < p->len ? p->i : p->len;
    if (pos < p->line_pos)
    {
        p->line_pos = 0;
        p->line_no = 1;
    }
    for (size_t k = p->line_pos; k < pos; ++k) if (p->buf[k] == '\n') p->line_no++;
    p->line_pos = pos;
    return p->line_no;
}

static void xml_html_error(XmlHtmlParser *p, const char *fmt, ...)
{
    if (!p->settings->message_callback) return;
    // Like libxml2 xmlStrVASPrintf with MAX_ERR_MSG_SIZE (64000 bytes), the message is
    // truncated without splitting a UTF-8 sequence.
    const int max_size = 64000;
    va_list ap, copy;
    va_start(ap, fmt);
    va_copy(copy, ap);
    int res = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (res < 0) res = 0;
    int size = res < max_size ? res + 1 : max_size;
    unsigned char *msg = (unsigned char*)malloc(size);
    vsnprintf((char*)msg, size, fmt, ap);
    va_end(ap);
    if (res >= max_size)
    {
        int i = size - 1;
        while (i > 0)
        {
            if (msg[i-1] < 0x80) break;
            i -= 1;
            if (msg[i] >= 0xc0) break;
        }
        msg[i] = 0;
    }
    // The source context line and the ^ at the current position, like libxml2.
    size_t pos = p->i < p->len ? p->i : p->len;
    char *out = xml_parser_format_message(p->settings->source_name ? p->settings->source_name : "noname.html",
                                          xml_html_line(p), "HTML parser error", (const char*)msg,
                                          (const char*)p->buf, p->len, pos, NULL);
    free(msg);
    p->settings->message_callback(p->settings->message_user_data, out);
    free(out);
}

// Tree building (the libxml2 SAX2 callbacks for html) ////////////

static XMQNode *xml_html_current(XmlHtmlParser *p)
{
    return p->nr ? p->stack[p->nr - 1].node : NULL;
}

static const char *xml_html_current_name(XmlHtmlParser *p)
{
    return p->nr ? p->stack[p->nr - 1].name : NULL;
}

static void xml_html_flush_text(XmlHtmlParser *p)
{
    if (!p->has_text) return;
    XMQNode *n;
    if (p->text_kind == XML_DOM_TEXT) n = xml_node_new_text_len(p->dom, p->text.b, p->text.len);
    else n = xml_node_new_cdata(p->dom, p->text.b, p->text.len);
    xml_node_append_child(p->text_parent, n);
    p->has_text = false;
    xml_hbuf_clear(&p->text);
}

/** xmlSAX2Text: append text or cdata to the current node, merging with the previous node of the same kind. */
static void xml_html_text(XmlHtmlParser *p, const char *s, size_t len, XmlNodeKind kind)
{
    XMQNode *parent = xml_html_current(p);
    if (!parent) return; // Text outside of any element is dropped.
    if (p->has_text && (p->text_parent != parent || p->text_kind != kind)) xml_html_flush_text(p);
    if (!p->has_text)
    {
        // libxml2 also merges with an existing last child of the same kind.
        XMQNode *last = xml_node_last_child(parent);
        if (last && xml_node_kind(last) == kind)
        {
            const char *c = xml_node_content(last);
            xml_hbuf_clear(&p->text);
            if (c) xml_hbuf_add(&p->text, c, strlen(c));
            xml_node_free(last);
        }
        else
        {
            xml_hbuf_clear(&p->text);
        }
        p->has_text = true;
        p->text_parent = parent;
        p->text_kind = kind;
    }
    xml_hbuf_add(&p->text, s, len);
}

static void xml_html_characters(XmlHtmlParser *p, const char *s, size_t len)
{
    xml_html_text(p, s, len, XML_DOM_TEXT);
}

static void xml_html_append(XmlHtmlParser *p, XMQNode *n)
{
    xml_html_flush_text(p);
    XMQNode *parent = xml_html_current(p);
    if (!parent) parent = xml_dom_document(p->dom);
    xml_node_append_child(parent, n);
}

static void xml_html_comment(XmlHtmlParser *p, const char *text)
{
    xml_html_append(p, xml_node_new_comment(p->dom, text));
}

/** htmlnamePush + startElement (xmlSAX2StartHtmlElement). */
static void xml_html_start_element(XmlHtmlParser *p, const char *name, XmlHtmlAttr *atts, size_t natts)
{
    if (p->insert_mode < XML_HTML_INSERT_IN_HEAD && !strcmp(name, "head")) p->insert_mode = XML_HTML_INSERT_IN_HEAD;
    if (p->insert_mode < XML_HTML_INSERT_IN_BODY && !strcmp(name, "body")) p->insert_mode = XML_HTML_INSERT_IN_BODY;

    XMQNode *e = xml_node_new_element(p->dom, name, NULL);
    xml_node_set_line(e, (int)xml_html_line(p));
    xml_html_append(p, e);

    for (size_t k = 0; k < natts; ++k)
    {
        const char *value = atts[k].value;
        if (!value && xml_html_is_boolean_attr(atts[k].name)) value = atts[k].name;
        XMQAttr *a = xml_node_set_attr(e, atts[k].name, NULL, NULL);
        if (value) xml_node_append_child(xml_attr_node(a), xml_node_new_text(p->dom, value));
    }

    if (p->nr == p->cap)
    {
        p->cap = p->cap ? p->cap * 2 : 32;
        p->stack = (XmlHtmlOpen*)realloc(p->stack, p->cap * sizeof(XmlHtmlOpen));
    }
    p->stack[p->nr].node = e;
    p->stack[p->nr].name = xml_html_strdup(name);
    p->nr++;
}

/** endElement + htmlnamePop. */
static void xml_html_end_element(XmlHtmlParser *p)
{
    if (!p->nr) return;
    xml_html_flush_text(p);
    p->nr--;
    free(p->stack[p->nr].name);
}

/** xmlSAX2InternalSubset: only before the content, the dtd is put first like in a libxml2 html document. */
static void xml_html_internal_subset(XmlHtmlParser *p, const char *name, const char *pub, const char *sys)
{
    if (!p->in_misc) return;
    XMQNode *dtd = xml_dom_set_dtd(p->dom, name, pub, sys);
    XMQNode *first = xml_node_first_child(xml_dom_document(p->dom));
    if (first != dtd) xml_node_add_prev_sibling(first, dtd);
}

// Implied elements and implicit closing /////////////////////////

/** Port of htmlAutoClose. */
static void xml_html_auto_close(XmlHtmlParser *p, const char *newtag)
{
    while (xml_html_current_name(p) && xml_html_start_closes(newtag, xml_html_current_name(p)))
    {
        xml_html_end_element(p);
    }
}

/** Port of htmlAutoCloseOnClose. */
static void xml_html_auto_close_on_close(XmlHtmlParser *p, const char *newtag)
{
    int priority = xml_html_end_priority(newtag);
    size_t i;
    for (i = p->nr; i > 0; --i)
    {
        if (!strcmp(newtag, p->stack[i-1].name)) break;
        // A misplaced endtag can only close elements with lower or equal priority.
        if (xml_html_end_priority(p->stack[i-1].name) > priority) return;
    }
    if (i == 0) return;
    while (strcmp(newtag, xml_html_current_name(p)))
    {
        const XmlHtmlElementDesc *info = xml_html_element(xml_html_current_name(p));
        if (info && info->end_tag == 3)
        {
            xml_html_error(p, "Opening and ending tag mismatch: %s and %s", newtag, xml_html_current_name(p));
        }
        xml_html_end_element(p);
    }
}

static void xml_html_auto_close_on_end(XmlHtmlParser *p)
{
    while (p->nr) xml_html_end_element(p);
}

/** Port of htmlCheckImplied. */
static void xml_html_check_implied(XmlHtmlParser *p, const char *newtag)
{
    if (!strcmp(newtag, "html")) return;
    if (p->nr == 0) xml_html_start_element(p, "html", NULL, 0);
    if (!strcmp(newtag, "body") || !strcmp(newtag, "head")) return;
    if (p->nr <= 1 &&
        (!strcmp(newtag, "script") || !strcmp(newtag, "style") || !strcmp(newtag, "meta") ||
         !strcmp(newtag, "link") || !strcmp(newtag, "title") || !strcmp(newtag, "base")))
    {
        if (p->insert_mode >= XML_HTML_INSERT_IN_HEAD) return; // We already saw or generated a head.
        xml_html_start_element(p, "head", NULL, 0);
    }
    else if (strcmp(newtag, "noframes") && strcmp(newtag, "frame") && strcmp(newtag, "frameset"))
    {
        if (p->insert_mode >= XML_HTML_INSERT_IN_BODY) return; // We already saw or generated a body.
        for (size_t i = 0; i < p->nr; ++i)
        {
            if (!strcmp(p->stack[i].name, "body")) return;
            if (!strcmp(p->stack[i].name, "head")) return;
        }
        xml_html_start_element(p, "body", NULL, 0);
    }
}

/** Port of htmlStartCharData: prepare for non whitespace character data. */
static void xml_html_start_char_data(XmlHtmlParser *p)
{
    const char *name = xml_html_current_name(p);
    if (name && !strcmp(name, "head")) xml_html_auto_close(p, "p");
    xml_html_check_implied(p, "p");
}

// Character data ////////////////////////////////////////////////

/** The last child of the current node as libxml2 sees it, pending text included, comments skipped. */
static XmlNodeKind xml_html_last_child_kind(XmlHtmlParser *p, const char **name)
{
    XMQNode *node = xml_html_current(p);
    *name = NULL;
    if (p->has_text && p->text_parent == node) return p->text_kind;
    XMQNode *last = node ? xml_node_last_child(node) : NULL;
    while (last && xml_node_kind(last) == XML_DOM_COMMENT) last = xml_node_prev(last);
    if (!last) return (XmlNodeKind)0;
    *name = xml_node_name(last);
    return xml_node_kind(last);
}

/** Port of the html areBlanks: 1 if ignorable, 0 if whitespace to keep, -1 if not whitespace. */
static int xml_html_are_blanks(XmlHtmlParser *p, const char *str, size_t len)
{
    for (size_t j = 0; j < len; ++j) if (!xml_html_is_ws((unsigned char)str[j])) return -1;
    int cur = xml_hcur(p);
    if (cur == 0) return 1;
    if (cur != '<') return 0;
    const char *name = xml_html_current_name(p);
    if (!name) return 1;
    if (!strcmp(name, "html")) return 1;
    if (!strcmp(name, "head")) return 1;

    // Only strip CDATA children of the body tag for strict HTML DTDs.
    if (!strcmp(name, "body"))
    {
        XMQNode *dtd = xml_dom_dtd(p->dom);
        const char *pub = dtd ? xml_dtd_public_id(dtd) : NULL;
        if (pub)
        {
            if (!xml_html_ascii_casecmp_p(pub, "-//W3C//DTD HTML 4.01//EN") || !xml_html_ascii_casecmp_p(pub, "-//W3C//DTD HTML 4//EN")) return 1;
        }
    }

    if (!xml_html_current(p)) return 0;
    const char *last_name = NULL;
    XmlNodeKind last = xml_html_last_child_kind(p, &last_name);
    if (last == 0)
    {
        // Keep ws in constructs like ...<b> </b>... for all tags "b" allowing PCDATA.
        if (xml_html_allows_pcdata(name)) return 0;
    }
    else if (last == XML_DOM_TEXT)
    {
        return 0;
    }
    else
    {
        // Keep ws in constructs like <p><b>xy</b> <i>z</i><p> for all tags "p" allowing PCDATA.
        if (last_name && xml_html_allows_pcdata(last_name)) return 0;
    }
    return 1;
}

/** Port of htmlCharDataSAXCallback. p->i is after the data. */
static void xml_html_char_data_callback(XmlHtmlParser *p, const char *buf, size_t size, int mode)
{
    if (mode == 0 || mode == XML_HTML_DATA_RCDATA)
    {
        const char *name = xml_html_current_name(p);
        if (!name || !strcmp(name, "html") || !strcmp(name, "head"))
        {
            // Add leading whitespace to html or head elements before calling htmlStartCharData.
            size_t i;
            for (i = 0; i < size; ++i) if (!xml_html_is_ws((unsigned char)buf[i])) break;
            if (i > 0)
            {
                if (!p->settings->no_blanks) xml_html_characters(p, buf, i);
                buf += i;
                size -= i;
            }
            if (size == 0) return;
            xml_html_start_char_data(p);
        }
        if (mode == 0 && p->settings->no_blanks && xml_html_are_blanks(p, buf, size) > 0)
        {
            return; // Ignorable whitespace.
        }
        xml_html_characters(p, buf, size);
    }
    else
    {
        // Raw text is inserted as cdata (HTML_PRESERVE_NODE).
        xml_html_text(p, buf, size, XML_DOM_CDATA);
    }
}

/** Port of htmlParseCharData (partial == 0). */
static void xml_html_parse_char_data(XmlHtmlParser *p)
{
    int mode = p->end_check_state;
    bool complete = false;
    bool done = false;

    while (true)
    {
        size_t chunk = p->i;
        size_t in = chunk;
        const char *repl = "";
        size_t repl_size = 0;
        unsigned char utf8[4];
        size_t skip = 0;
        int ncr = 0;
        size_t ncr_size = 0;
        int cp = 0;

        while (true)
        {
            if (in >= p->len)
            {
                done = true;
                break;
            }
            int cur = p->buf[in];
            size_t size = 1;
            size_t avail = p->len - in;

            if (ncr)
            {
                int lc = cur | 0x20;
                int digit;
                if (cur >= '0' && cur <= '9') digit = cur - '0';
                else if (ncr == 16 && lc >= 'a' && lc <= 'f') digit = lc - 'a' + 10;
                else
                {
                    if (cur == ';')
                    {
                        in += 1;
                        ncr_size += 1;
                    }
                    goto next_chunk;
                }
                cp = cp * ncr + digit;
                if (cp >= 0x110000) cp = 0x110000;
                ncr_size += 1;
                in += size;
                continue;
            }

            switch (cur)
            {
            case '<':
                if (mode == 0)
                {
                    done = true;
                    complete = true;
                    goto next_chunk;
                }
                if (mode == XML_HTML_DATA_PLAINTEXT) break;
                {
                    size_t j = 1;
                    if (j < avail)
                    {
                        if (mode == XML_HTML_DATA_SCRIPT && p->buf[in+j] == '!')
                        {
                            // Check for comment start.
                            j += 1;
                            if (j < avail && p->buf[in+j] == '-')
                            {
                                j += 1;
                                if (j < avail && p->buf[in+j] == '-') mode = XML_HTML_DATA_SCRIPT_ESC1;
                            }
                        }
                        else
                        {
                            const char *name = xml_html_current_name(p);
                            size_t ni = 0;
                            bool solidus = false;
                            // Check for tag.
                            if (p->buf[in+j] == '/')
                            {
                                j += 1;
                                solidus = true;
                            }
                            if ((solidus || mode == XML_HTML_DATA_SCRIPT_ESC1) && name)
                            {
                                while (j < avail && name[ni] != 0 && (unsigned char)name[ni] == (p->buf[in+j] | 0x20))
                                {
                                    ni++;
                                    j++;
                                }
                                if (name[ni] == 0 && j < avail)
                                {
                                    int c = p->buf[in+j];
                                    if (c == '>' || c == '/' || xml_html_is_ws(c))
                                    {
                                        if (mode == XML_HTML_DATA_SCRIPT_ESC1 && !solidus)
                                        {
                                            mode = XML_HTML_DATA_SCRIPT_ESC2;
                                        }
                                        else if (mode == XML_HTML_DATA_SCRIPT_ESC2)
                                        {
                                            mode = XML_HTML_DATA_SCRIPT_ESC1;
                                        }
                                        else
                                        {
                                            complete = true;
                                            done = true;
                                            goto next_chunk;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                break;

            case '-':
                if (mode != XML_HTML_DATA_SCRIPT_ESC1 && mode != XML_HTML_DATA_SCRIPT_ESC2) break;
                // Check for comment end.
                if (1 < avail && p->buf[in+1] == '-')
                {
                    if (2 < avail && p->buf[in+2] == '>') mode = XML_HTML_DATA_SCRIPT;
                }
                break;

            case '&':
                if (mode != 0 && mode != XML_HTML_DATA_RCDATA) break;
                {
                    size_t j = 1;
                    if (j < avail && p->buf[in+j] == '#')
                    {
                        j += 1;
                        if (j < avail)
                        {
                            if ((p->buf[in+j] | 0x20) == 'x')
                            {
                                j += 1;
                                if (j < avail && xml_html_is_hex(p->buf[in+j]))
                                {
                                    ncr = 16;
                                    size = 3;
                                    ncr_size = 3;
                                    cp = 0;
                                }
                            }
                            else if (xml_html_is_digit(p->buf[in+j]))
                            {
                                ncr = 10;
                                size = 2;
                                ncr_size = 2;
                                cp = 0;
                            }
                        }
                    }
                    else
                    {
                        size_t nlen = 0, rlen = 0;
                        const unsigned char *r = xml_html_find_entity_prefix(p->buf + in + j, avail - j, false, &nlen, &rlen);
                        if (r)
                        {
                            repl = (const char*)r;
                            repl_size = rlen;
                            skip = nlen + 1;
                            goto next_chunk;
                        }
                    }
                }
                break;

            case '\0':
                skip = 1;
                if (mode == 0)
                {
                    // The tokenizer strips U+0000 in normal data mode.
                    repl = "";
                    repl_size = 0;
                }
                else
                {
                    repl = "\xEF\xBF\xBD";
                    repl_size = 3;
                }
                goto next_chunk;

            case '\r':
                skip = 1;
                if (in + 1 >= p->len || p->buf[in+1] != 0x0A)
                {
                    repl = "\n";
                    repl_size = 1;
                }
                goto next_chunk;

            default:
                if (cur < 0x80) break;
                {
                    int vs = xml_html_validate_utf8(p->buf + in, avail);
                    if (vs <= 0)
                    {
                        skip = 1;
                        repl = "\xEF\xBF\xBD";
                        repl_size = 3;
                        goto next_chunk;
                    }
                    size = (size_t)vs;
                }
                break;
            }
            in += size;
        }

    next_chunk:
        if (ncr_size > 0)
        {
            skip = ncr_size;
            in -= ncr_size;
            repl_size = xml_html_cp_to_utf8(cp, utf8);
            repl = (const char*)utf8;
        }
        if (in > chunk)
        {
            p->i = in;
            xml_html_char_data_callback(p, (const char*)p->buf + chunk, in - chunk, mode);
        }
        p->i = in + skip;
        if (repl_size > 0) xml_html_char_data_callback(p, repl, repl_size, mode);
        if (done) break;
    }

    p->end_check_state = complete ? 0 : mode;
}

// Data: attribute values, comments, doctype literals /////////////

#define XML_HTML_MASK_DQ    1
#define XML_HTML_MASK_SQ    2
#define XML_HTML_MASK_GT    4
#define XML_HTML_MASK_DASH  8
#define XML_HTML_MASK_WS_GT 16
#define XML_HTML_MASK_DQ_GT 32
#define XML_HTML_MASK_SQ_GT 64

static bool xml_html_mask_match(int mask, int c)
{
    switch (mask)
    {
    case XML_HTML_MASK_DQ: return c == '"';
    case XML_HTML_MASK_SQ: return c == '\'';
    case XML_HTML_MASK_GT: return c == '>';
    case XML_HTML_MASK_DASH: return c == '-';
    case XML_HTML_MASK_WS_GT: return c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == ' ' || c == '>';
    case XML_HTML_MASK_DQ_GT: return c == '"' || c == '>';
    case XML_HTML_MASK_SQ_GT: return c == '\'' || c == '>';
    }
    return false;
}

/** Port of htmlParseData. The result is in p->data. Returns false if the value is too long. */
static bool xml_html_parse_data(XmlHtmlParser *p, int mask, bool comment, bool refs, size_t max_length)
{
    xml_hbuf_clear(&p->data);
    int term_skip = -1;

    while (true)
    {
        size_t chunk = p->i;
        size_t in = chunk;
        const char *repl = "";
        size_t repl_size = 0;
        unsigned char utf8[4];
        size_t skip = 0;
        int ncr = 0;
        size_t ncr_size = 0;
        int cp = 0;

        while (true)
        {
            if (in >= p->len)
            {
                term_skip = 0;
                break;
            }
            size_t avail = p->len - in;
            int cur = p->buf[in];
            size_t size = 1;

            if (xml_html_mask_match(mask, cur))
            {
                if (comment)
                {
                    if (avail < 2) term_skip = 1;
                    else if (p->buf[in+1] == '-')
                    {
                        if (avail < 3) term_skip = 2;
                        else if (p->buf[in+2] == '>') term_skip = 3;
                        else if (p->buf[in+2] == '!')
                        {
                            if (avail < 4) term_skip = 3;
                            else if (p->buf[in+3] == '>') term_skip = 4;
                        }
                    }
                    if (term_skip >= 0) break;
                }
                else
                {
                    term_skip = 0;
                    break;
                }
            }

            if (ncr)
            {
                int lc = cur | 0x20;
                int digit;
                if (cur >= '0' && cur <= '9') digit = cur - '0';
                else if (ncr == 16 && lc >= 'a' && lc <= 'f') digit = lc - 'a' + 10;
                else
                {
                    if (cur == ';')
                    {
                        in += 1;
                        ncr_size += 1;
                    }
                    goto next_chunk;
                }
                cp = cp * ncr + digit;
                if (cp >= 0x110000) cp = 0x110000;
                ncr_size += 1;
                in += size;
                continue;
            }

            switch (cur)
            {
            case '&':
                if (!refs) break;
                {
                    size_t j = 1;
                    if (j < avail && p->buf[in+j] == '#')
                    {
                        j += 1;
                        if (j < avail)
                        {
                            if ((p->buf[in+j] | 0x20) == 'x')
                            {
                                j += 1;
                                if (j < avail && xml_html_is_hex(p->buf[in+j]))
                                {
                                    ncr = 16;
                                    size = 3;
                                    ncr_size = 3;
                                    cp = 0;
                                }
                            }
                            else if (xml_html_is_digit(p->buf[in+j]))
                            {
                                ncr = 10;
                                size = 2;
                                ncr_size = 2;
                                cp = 0;
                            }
                        }
                    }
                    else
                    {
                        size_t nlen = 0, rlen = 0;
                        const unsigned char *r = xml_html_find_entity_prefix(p->buf + in + j, avail - j, true, &nlen, &rlen);
                        if (r)
                        {
                            repl = (const char*)r;
                            repl_size = rlen;
                            skip = nlen + 1;
                            goto next_chunk;
                        }
                    }
                }
                break;
            case '\0':
                skip = 1;
                repl = "\xEF\xBF\xBD";
                repl_size = 3;
                goto next_chunk;
            case '\r':
                skip = 1;
                if (in + 1 >= p->len || p->buf[in+1] != 0x0A)
                {
                    repl = "\n";
                    repl_size = 1;
                }
                goto next_chunk;
            default:
                if (cur < 0x80) break;
                {
                    int vs = xml_html_validate_utf8(p->buf + in, avail);
                    if (vs <= 0)
                    {
                        skip = 1;
                        repl = "\xEF\xBF\xBD";
                        repl_size = 3;
                        goto next_chunk;
                    }
                    size = (size_t)vs;
                }
                break;
            }
            in += size;
        }

    next_chunk:
        if (ncr_size > 0)
        {
            skip = ncr_size;
            in -= ncr_size;
            repl_size = xml_html_cp_to_utf8(cp, utf8);
            repl = (const char*)utf8;
        }
        {
            size_t chunk_size = in - chunk;
            if (chunk_size + repl_size > max_length - p->data.len)
            {
                xml_html_error(p, "value too long");
                return false;
            }
            if (chunk_size > 0) xml_hbuf_add(&p->data, p->buf + chunk, chunk_size);
            p->i = in + skip;
            if (repl_size > 0) xml_hbuf_add(&p->data, repl, repl_size);
        }
        if (term_skip >= 0) break;
    }
    if (term_skip > 0) p->i += term_skip;
    return true;
}

/** Port of htmlParseHTMLName: lowercased, truncated to 100 bytes. Result in p->name. */
static const char *xml_html_parse_name(XmlHtmlParser *p, bool attr)
{
    int stop = attr ? '=' : ' ';
    xml_hbuf_clear(&p->name);
    size_t nbchar = 0;
    while (p->i < p->len)
    {
        int c = p->buf[p->i];
        size_t size = 1;
        if (nbchar != 0 && (c == '/' || c == '>' || c == stop || xml_html_is_ws(c))) break;
        if (c == 0)
        {
            if (nbchar + 3 <= XML_HTML_MAX_NAME)
            {
                xml_hbuf_add(&p->name, "\xEF\xBF\xBD", 3);
                nbchar += 3;
            }
        }
        else if (c < 0x80)
        {
            if (nbchar < XML_HTML_MAX_NAME)
            {
                char lc = (char)((c >= 'A' && c <= 'Z') ? c + 0x20 : c);
                xml_hbuf_add(&p->name, &lc, 1);
                nbchar++;
            }
        }
        else
        {
            int vs = xml_html_validate_utf8(p->buf + p->i, p->len - p->i);
            if (vs > 0)
            {
                size = (size_t)vs;
                if (nbchar + size <= XML_HTML_MAX_NAME)
                {
                    xml_hbuf_add(&p->name, p->buf + p->i, size);
                    nbchar += size;
                }
            }
            else
            {
                size = 1;
                if (nbchar + 3 <= XML_HTML_MAX_NAME)
                {
                    xml_hbuf_add(&p->name, "\xEF\xBF\xBD", 3);
                    nbchar += 3;
                }
            }
        }
        p->i += size;
    }
    return p->name.b;
}

/** Port of htmlParseAttValue. Returns a malloced value, or NULL if too long. */
static char *xml_html_parse_att_value(XmlHtmlParser *p)
{
    bool ok;
    if (xml_hcur(p) == '"')
    {
        p->i++;
        ok = xml_html_parse_data(p, XML_HTML_MASK_DQ, false, true, p->settings->max_text_length);
        if (xml_hcur(p) == '"') p->i++;
    }
    else if (xml_hcur(p) == '\'')
    {
        p->i++;
        ok = xml_html_parse_data(p, XML_HTML_MASK_SQ, false, true, p->settings->max_text_length);
        if (xml_hcur(p) == '\'') p->i++;
    }
    else
    {
        ok = xml_html_parse_data(p, XML_HTML_MASK_WS_GT, false, true, p->settings->max_text_length);
    }
    return ok ? xml_html_strdup(p->data.b) : NULL;
}

/** Port of htmlParseComment. */
static void xml_html_parse_comment(XmlHtmlParser *p, bool bogus)
{
    if (bogus)
    {
        bool ok = xml_html_parse_data(p, XML_HTML_MASK_GT, false, false, p->settings->max_text_length);
        if (xml_hcur(p) == '>') p->i++;
        if (ok) xml_html_comment(p, p->data.b);
        return;
    }
    if (xml_hcur(p) == '>')
    {
        p->i++;
        xml_html_comment(p, "");
    }
    else if (xml_hcur(p) == '-' && xml_hnxt(p, 1) == '>')
    {
        p->i += 2;
        xml_html_comment(p, "");
    }
    else
    {
        bool ok = xml_html_parse_data(p, XML_HTML_MASK_DASH, true, false, p->settings->max_text_length);
        if (ok) xml_html_comment(p, p->data.b);
    }
}

/** Port of htmlParseDoctypeLiteral. Returns malloced or NULL. */
static char *xml_html_parse_doctype_literal(XmlHtmlParser *p)
{
    bool ok;
    if (xml_hcur(p) == '"')
    {
        p->i++;
        ok = xml_html_parse_data(p, XML_HTML_MASK_DQ_GT, false, false, 50000);
        if (xml_hcur(p) == '"') p->i++;
    }
    else if (xml_hcur(p) == '\'')
    {
        p->i++;
        ok = xml_html_parse_data(p, XML_HTML_MASK_SQ_GT, false, false, 50000);
        if (xml_hcur(p) == '\'') p->i++;
    }
    else
    {
        return NULL;
    }
    return ok ? xml_html_strdup(p->data.b) : NULL;
}

/** Port of htmlParseDocTypeDecl. */
static void xml_html_parse_doctype(XmlHtmlParser *p)
{
    char *name = NULL, *pub = NULL, *sys = NULL;
    p->i += 9;
    xml_html_skip_blanks(p);
    if (p->i < p->len && xml_hcur(p) != '>')
    {
        if (xml_html_parse_data(p, XML_HTML_MASK_WS_GT, false, false, 50000)) name = xml_html_strdup(p->data.b);
        xml_html_skip_blanks(p);
    }
    if (xml_hupp(p, 0) == 'P' && xml_hupp(p, 1) == 'U' && xml_hupp(p, 2) == 'B' &&
        xml_hupp(p, 3) == 'L' && xml_hupp(p, 4) == 'I' && xml_hupp(p, 5) == 'C')
    {
        p->i += 6;
        xml_html_skip_blanks(p);
        pub = xml_html_parse_doctype_literal(p);
        if (pub)
        {
            xml_html_skip_blanks(p);
            sys = xml_html_parse_doctype_literal(p);
        }
    }
    else if (xml_hupp(p, 0) == 'S' && xml_hupp(p, 1) == 'Y' && xml_hupp(p, 2) == 'S' &&
             xml_hupp(p, 3) == 'T' && xml_hupp(p, 4) == 'E' && xml_hupp(p, 5) == 'M')
    {
        p->i += 6;
        xml_html_skip_blanks(p);
        sys = xml_html_parse_doctype_literal(p);
    }
    // htmlSkipBogusDoctype: skip to and over the next >.
    while (p->i < p->len)
    {
        if (p->buf[p->i++] == '>') break;
    }
    xml_html_internal_subset(p, name, pub, sys);
    free(name);
    free(pub);
    free(sys);
}

// Tags ///////////////////////////////////////////////////////////

static void xml_html_free_atts(XmlHtmlParser *p)
{
    for (size_t k = 0; k < p->natts; ++k)
    {
        free(p->atts[k].name);
        free(p->atts[k].value);
    }
    p->natts = 0;
}

/** Port of htmlParseAttribute: the name in p->name (copied by the caller), value malloced or NULL. */
static void xml_html_parse_attribute(XmlHtmlParser *p, char **name, char **value)
{
    *value = NULL;
    *name = xml_html_strdup(xml_html_parse_name(p, true));
    xml_html_skip_blanks(p);
    if (xml_hcur(p) == '=')
    {
        p->i++;
        xml_html_skip_blanks(p);
        *value = xml_html_parse_att_value(p);
    }
}

/** Port of htmlParseStartTag. */
static void xml_html_parse_start_tag(XmlHtmlParser *p)
{
    bool discard = false;
    p->end_check_state = 0;
    p->i++; // <
    char *name = xml_html_strdup(xml_html_parse_name(p, false));

    // Check for auto-closure of HTML elements.
    xml_html_auto_close(p, name);
    // Check for implied HTML elements.
    xml_html_check_implied(p, name);

    // Avoid html at any level > 0, head at any level != 1 or any attempt to recurse body.
    if (p->nr > 0 && !strcmp(name, "html"))
    {
        xml_html_error(p, "htmlParseStartTag: misplaced <html> tag");
        discard = true;
        p->depth++;
    }
    if (p->nr != 1 && !strcmp(name, "head"))
    {
        xml_html_error(p, "htmlParseStartTag: misplaced <head> tag");
        discard = true;
        p->depth++;
    }
    if (!strcmp(name, "body"))
    {
        for (size_t indx = 0; indx < p->nr; ++indx)
        {
            if (!strcmp(p->stack[indx].name, "body"))
            {
                xml_html_error(p, "htmlParseStartTag: misplaced <body> tag");
                discard = true;
                p->depth++;
            }
        }
    }

    // Parse the attributes.
    xml_html_free_atts(p);
    xml_html_skip_blanks(p);
    while (p->i < p->len && xml_hcur(p) != '>' && !(xml_hcur(p) == '/' && xml_hnxt(p, 1) == '>'))
    {
        // unexpected-solidus-in-tag
        if (xml_hcur(p) == '/')
        {
            p->i++;
            xml_html_skip_blanks(p);
            continue;
        }
        char *attname, *attvalue;
        xml_html_parse_attribute(p, &attname, &attvalue);
        if (p->natts == p->atts_cap)
        {
            p->atts_cap = p->atts_cap ? p->atts_cap * 2 : 16;
            p->atts = (XmlHtmlAttr*)realloc(p->atts, p->atts_cap * sizeof(XmlHtmlAttr));
        }
        p->atts[p->natts].name = attname;
        p->atts[p->natts].value = attvalue;
        p->natts++;
        xml_html_skip_blanks(p);
    }

    if (p->i >= p->len)
    {
        discard = true;
    }
    else if (p->natts > 1)
    {
        // Attribute names must be unique, the first one wins.
        HashMap *seen = hashmap_create(p->natts * 2 + 1);
        size_t j = 0;
        for (size_t k = 0; k < p->natts; ++k)
        {
            if (hashmap_get(seen, p->atts[k].name))
            {
                free(p->atts[k].name);
                free(p->atts[k].value);
                continue;
            }
            hashmap_put(seen, p->atts[k].name, (void*)1);
            p->atts[j++] = p->atts[k];
        }
        p->natts = j;
        hashmap_free(seen);
    }

    if (!discard) xml_html_start_element(p, name, p->atts, p->natts);
    xml_html_free_atts(p);
    free(name);
}

/** Port of htmlParseEndTag. */
static void xml_html_parse_end_tag(XmlHtmlParser *p)
{
    p->end_check_state = 0;
    p->i += 2;
    if (p->i >= p->len)
    {
        xml_html_start_char_data(p);
        xml_html_characters(p, "</", 2);
        return;
    }
    if (xml_hcur(p) == '>')
    {
        p->i++;
        return;
    }
    if (!xml_html_is_letter(xml_hcur(p)))
    {
        xml_html_parse_comment(p, true);
        return;
    }

    char *name = xml_html_strdup(xml_html_parse_name(p, false));

    // Parse and ignore attributes.
    xml_html_skip_blanks(p);
    while (p->i < p->len && xml_hcur(p) != '>' && !(xml_hcur(p) == '/' && xml_hnxt(p, 1) == '>'))
    {
        if (xml_hcur(p) == '/')
        {
            p->i++;
            xml_html_skip_blanks(p);
            continue;
        }
        char *an, *av;
        xml_html_parse_attribute(p, &an, &av);
        free(an);
        free(av);
        xml_html_skip_blanks(p);
    }

    if (xml_hcur(p) == '>' && p->i < p->len)
    {
        p->i++;
    }
    else if (xml_hcur(p) == '/' && xml_hnxt(p, 1) == '>')
    {
        p->i += 2;
    }
    else
    {
        free(name);
        return;
    }

    // If we ignored misplaced tags in htmlParseStartTag don't pop them out now.
    if (p->depth > 0 && (!strcmp(name, "html") || !strcmp(name, "body") || !strcmp(name, "head")))
    {
        p->depth--;
        free(name);
        return;
    }

    // If the name read is not one of the element in the parsing stack then return, it's just an error.
    size_t i;
    for (i = p->nr; i > 0; --i)
    {
        if (!strcmp(name, p->stack[i-1].name)) break;
    }
    if (i == 0)
    {
        xml_html_error(p, "Unexpected end tag : %s", name);
        free(name);
        return;
    }

    // Check for auto-closure of HTML elements.
    xml_html_auto_close_on_close(p, name);

    const char *cur = xml_html_current_name(p);
    if (cur && strcmp(cur, name))
    {
        xml_html_error(p, "Opening and ending tag mismatch: %s and %s", name, cur);
    }
    if (cur && !strcmp(cur, name)) xml_html_end_element(p);
    free(name);
}

/** Port of htmlParseElementInternal. */
static void xml_html_parse_element(XmlHtmlParser *p)
{
    xml_html_parse_start_tag(p);
    // Note: like libxml2 this is the current name, which is the parent if the start tag was discarded.
    const char *name = xml_html_current_name(p);
    if (!name) return;

    // Check for an Empty Element labeled the XML/SGML way.
    if (xml_hcur(p) == '/' && xml_hnxt(p, 1) == '>' && p->i < p->len)
    {
        p->i += 2;
        xml_html_end_element(p);
        return;
    }
    if (xml_hcur(p) != '>' || p->i >= p->len) return;
    p->i++;

    // Check for an Empty Element from DTD definition.
    const XmlHtmlElementDesc *info = xml_html_element(name);
    if (info && info->empty)
    {
        xml_html_end_element(p);
        return;
    }
    if (info) p->end_check_state = info->data_mode;
}

static bool xml_html_at_doctype(XmlHtmlParser *p)
{
    return xml_hupp(p, 2) == 'D' && xml_hupp(p, 3) == 'O' && xml_hupp(p, 4) == 'C' &&
           xml_hupp(p, 5) == 'T' && xml_hupp(p, 6) == 'Y' && xml_hupp(p, 7) == 'P' && xml_hupp(p, 8) == 'E';
}

/** Port of htmlParseContent. */
static void xml_html_parse_content(XmlHtmlParser *p)
{
    while (p->i < p->len)
    {
        int mode = p->end_check_state;
        if (mode == 0 && xml_hcur(p) == '<')
        {
            int n = xml_hnxt(p, 1);
            if (n == '/')
            {
                xml_html_parse_end_tag(p);
            }
            else if (n == '!')
            {
                // Sometimes DOCTYPE arrives in the middle of the document.
                if (xml_html_at_doctype(p))
                {
                    xml_html_parse_doctype(p);
                }
                else if (xml_hnxt(p, 2) == '-' && xml_hnxt(p, 3) == '-')
                {
                    p->i += 4;
                    xml_html_parse_comment(p, false);
                }
                else
                {
                    p->i += 2;
                    xml_html_parse_comment(p, true);
                }
            }
            else if (n == '?')
            {
                p->i += 1;
                xml_html_parse_comment(p, true);
            }
            else if (xml_html_is_letter(n))
            {
                xml_html_parse_element(p);
            }
            else
            {
                xml_html_start_char_data(p);
                xml_html_characters(p, "<", 1);
                p->i += 1;
            }
        }
        else
        {
            xml_html_parse_char_data(p);
        }
    }
    xml_html_auto_close_on_end(p);
}

/** Port of htmlParseDocument. */
static void xml_html_parse_document(XmlHtmlParser *p)
{
    // Like libxml2 xmlDetectEncoding with the encoding already set (libxmq gives UTF-8): a bom
    // is skipped, but only if the input has at least four bytes. Otherwise it is the
    // character U+FEFF in the content.
    if (p->len >= 4 && p->buf[0] == 0xEF && p->buf[1] == 0xBB && p->buf[2] == 0xBF) p->i = 3;

    // Wipe out everything which is before the first '<'.
    xml_html_skip_blanks(p);

    // Parse possible comments, PIs or doctype declarations before any content.
    p->in_misc = true;
    while (xml_hcur(p) == '<' && p->i < p->len)
    {
        if (xml_hnxt(p, 1) == '!')
        {
            if (xml_hnxt(p, 2) == '-' && xml_hnxt(p, 3) == '-')
            {
                p->i += 4;
                xml_html_parse_comment(p, false);
            }
            else if (xml_html_at_doctype(p))
            {
                xml_html_parse_doctype(p);
                p->in_misc = false;
            }
            else
            {
                p->i += 2;
                xml_html_parse_comment(p, true);
            }
        }
        else if (xml_hnxt(p, 1) == '?')
        {
            p->i += 1;
            xml_html_parse_comment(p, true);
        }
        else
        {
            break;
        }
        xml_html_skip_blanks(p);
    }
    p->in_misc = false;

    // Time to start parsing the tree itself.
    xml_html_parse_content(p);
    xml_html_flush_text(p);

    // Like xmlSAX2EndDocument: a default doctype if none was given.
    if (!xml_dom_dtd(p->dom))
    {
        p->in_misc = true;
        xml_html_internal_subset(p, "html", "-//W3C//DTD HTML 4.0 Transitional//EN",
                                 "http://www.w3.org/TR/REC-html40/loose.dtd");
        p->in_misc = false;
    }
}

void xml_html_parse_settings_default(XmlHtmlParseSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->max_text_length = 10000000;
}

XmlDom *xml_html_parse_buffer(const char *start, const char *stop, const XmlHtmlParseSettings *settings)
{
    XmlHtmlParseSettings defaults;
    if (!settings)
    {
        xml_html_parse_settings_default(&defaults);
        settings = &defaults;
    }
    if (!stop) stop = start + strlen(start);

    XmlHtmlParser *p = (XmlHtmlParser*)calloc(1, sizeof(XmlHtmlParser));
    p->settings = settings;
    p->buf = (const unsigned char*)start;
    p->len = stop - start;
    p->dom = xml_dom_new();
    p->insert_mode = XML_HTML_INSERT_INITIAL;
    p->line_no = 1;
    // Like htmlNewDocNoDtD and xmlSAX2EndDocument.
    xml_dom_set_standalone(p->dom, 1);
    xml_dom_set_encoding(p->dom, "UTF-8");

    xml_html_parse_document(p);

    XmlDom *dom = p->dom;
    for (size_t k = 0; k < p->nr; ++k) free(p->stack[k].name);
    free(p->stack);
    xml_html_free_atts(p);
    free(p->atts);
    free(p->text.b);
    free(p->data.b);
    free(p->name.b);
    free(p);
    return dom;
}

#endif // XML_HTML_PARSER_MODULE
