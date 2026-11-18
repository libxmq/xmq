/* This file is ported from libxml2 (HTMLtree.c, xmlIO.c and xmlsave.c,
   version 2.16.0 as found in the libxmq fork in 3rdparty/libxml2).
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

#include"xml_html_save.h"
#include"xml_html.h"
#include"xml_save.h"

#include<string.h>

#endif

#ifdef XML_HTML_SAVE_MODULE

static void xml_html_put(MemBuffer *mb, const char *s)
{
    if (s) membuffer_append(mb, s);
}

static int xml_html_casecmp(const char *a, const char *b)
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

/** xmlSerializeText with the htmlEscapeTab / htmlEscapeTabAttr tables: & < > (and " in attributes). */
static void xml_html_escape(MemBuffer *mb, const char *s, bool attr)
{
    if (!s) return;
    const char *base = s;
    for (const char *i = s; *i; ++i)
    {
        const char *repl = NULL;
        switch (*i)
        {
        case '&': repl = "&amp;"; break;
        case '<': repl = "&lt;"; break;
        case '>': repl = "&gt;"; break;
        case '"': if (attr) repl = "&quot;"; break;
        default: break;
        }
        if (!repl) continue;
        if (i > base) membuffer_append_region(mb, base, i);
        membuffer_append(mb, repl);
        base = i + 1;
    }
    membuffer_append(mb, base);
}

/** Port of htmlSerializeUri: %-escape spaces, controls and non ascii, escape " and &. */
static void xml_html_serialize_uri(MemBuffer *mb, const char *content)
{
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *tmp = (const unsigned char*)content;
    const unsigned char *start = tmp;
    // Skip over initial whitespace.
    while (*tmp == 0x09 || *tmp == 0x0A || *tmp == 0x0C || *tmp == 0x0D || *tmp == 0x20) tmp++;
    if (tmp > start) membuffer_append_region(mb, (const char*)start, (const char*)tmp);
    start = tmp;
    while (true)
    {
        int c = *tmp;
        while (c > 0x20 && c < 0x7F && c != '"' && c != '&')
        {
            tmp++;
            c = *tmp;
        }
        if (tmp > start) membuffer_append_region(mb, (const char*)start, (const char*)tmp);
        if (c <= 0x20 || c >= 0x7F)
        {
            if (c == 0) break;
            char esc[4] = { '%', hex[(c >> 4) & 0x0F], hex[c & 0x0F], 0 };
            membuffer_append(mb, esc);
        }
        else if (c == '"')
        {
            membuffer_append(mb, "&quot;");
        }
        else
        {
            membuffer_append(mb, "&amp;");
        }
        tmp++;
        start = tmp;
    }
}

static void xml_html_qname(MemBuffer *mb, const XMQNode *n)
{
    const char *prefix = xml_node_ns_prefix(n);
    if (prefix)
    {
        xml_html_put(mb, prefix);
        membuffer_append_char(mb, ':');
    }
    xml_html_put(mb, xml_node_name(n));
}

/** xmlNsDumpOutput without a save context: attribute escaping plus non ascii as char refs. */
static void xml_html_ns(MemBuffer *mb, const XmlNs *ns)
{
    const char *prefix = xml_ns_prefix(ns);
    const char *href = xml_ns_href(ns);
    if (!href) return;
    if (prefix && !strcmp(prefix, "xml")) return;
    membuffer_append(mb, prefix ? " xmlns:" : " xmlns");
    if (prefix) membuffer_append(mb, prefix);
    membuffer_append(mb, "=\"");
    // Like xmlSerializeText(XML_ESCAPE_ATTR | XML_ESCAPE_NON_ASCII), there is no save context.
    xml_save_escape_attr(mb, href, true);
    membuffer_append_char(mb, '"');
}

/** Port of htmlAttrDumpOutput. */
static void xml_html_attr(MemBuffer *mb, const XMQNode *attr)
{
    membuffer_append_char(mb, ' ');
    xml_html_qname(mb, attr);
    const XMQNode *first = xml_node_first_child(attr);
    if (!first || xml_html_is_boolean_attr(xml_node_name(attr))) return;

    membuffer_append(mb, "=\"");
    const XMQNode *owner = xml_node_parent(attr);
    const char *name = xml_node_name(attr);
    bool is_uri = !xml_node_ns(attr) && owner && !xml_node_ns(owner) &&
                  (!xml_html_casecmp(name, "href") || !xml_html_casecmp(name, "action") ||
                   !xml_html_casecmp(name, "src") ||
                   (!xml_html_casecmp(name, "name") && !xml_html_casecmp(xml_node_name(owner), "a")));
    for (const XMQNode *c = first; c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_TEXT)
        {
            const char *content = xml_node_content(c);
            if (!content) continue;
            if (is_uri) xml_html_serialize_uri(mb, content);
            else xml_html_escape(mb, content, true);
        }
        else if (xml_node_kind(c) == XML_DOM_ENTITY_REF)
        {
            membuffer_append_char(mb, '&');
            xml_html_put(mb, xml_node_name(c));
            membuffer_append_char(mb, ';');
        }
    }
    membuffer_append_char(mb, '"');
}

/** Port of htmlDtdDumpOutput. */
static void xml_html_dtd(MemBuffer *mb, const XMQNode *dtd)
{
    const char *pub = xml_dtd_public_id(dtd);
    const char *sys = xml_dtd_system_id(dtd);
    membuffer_append(mb, "<!DOCTYPE ");
    xml_html_put(mb, xml_node_name(dtd));
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
    else if (sys && strcmp(sys, "about:legacy-compat"))
    {
        membuffer_append(mb, " SYSTEM ");
        xml_save_write_quoted(mb, sys);
    }
    membuffer_append(mb, ">\n");
}

static const XmlHtmlElementDesc *xml_html_info(const XMQNode *n)
{
    if (xml_node_kind(n) != XML_DOM_ELEMENT || xml_node_ns(n)) return NULL;
    return xml_html_element(xml_node_name(n));
}

static bool xml_html_is_text_or_ref(const XMQNode *n)
{
    XmlNodeKind k = xml_node_kind(n);
    return k == XML_DOM_TEXT || k == XML_DOM_ENTITY_REF;
}

static bool xml_html_name_starts_with_p(const XMQNode *n)
{
    const char *name = n ? xml_node_name(n) : NULL;
    return name && name[0] == 'p';
}

/** Port of htmlNodeDumpInternal (encoding NULL, so no meta charset handling). */
static void xml_html_node(MemBuffer *mb, const XMQNode *cur, bool format)
{
    const XMQNode *root = cur;
    bool is_raw = false;

    while (true)
    {
        switch (xml_node_kind(cur))
        {
        case XML_DOM_DOCUMENT:
        {
            XMQNode *dtd = xml_dom_dtd(xml_node_dom(cur));
            if (dtd) xml_html_dtd(mb, dtd);
            if (xml_node_first_child(cur))
            {
                cur = xml_node_first_child(cur);
                continue;
            }
            membuffer_append_char(mb, '\n');
            break;
        }
        case XML_DOM_ELEMENT:
        {
            const XmlHtmlElementDesc *info = xml_html_info(cur);
            membuffer_append_char(mb, '<');
            xml_html_qname(mb, cur);
            for (const XmlNs *ns = xml_node_first_ns_decl(cur); ns; ns = xml_ns_next_decl(ns)) xml_html_ns(mb, ns);
            for (XMQAttr *a = xml_node_first_attr(cur); a; a = xml_attr_next(a)) xml_html_attr(mb, xml_attr_node(a));

            const XMQNode *first = xml_node_first_child(cur);
            if (info && info->empty)
            {
                membuffer_append_char(mb, '>');
            }
            else if (!first)
            {
                membuffer_append(mb, "></");
                xml_html_qname(mb, cur);
                membuffer_append_char(mb, '>');
            }
            else
            {
                membuffer_append_char(mb, '>');
                if (format && info && !info->isinline &&
                    !xml_html_is_text_or_ref(first) &&
                    first != xml_node_last_child(cur) &&
                    !xml_html_name_starts_with_p(cur)) // p, pre, param
                {
                    membuffer_append_char(mb, '\n');
                }
                if (info && info->data_mode >= XML_HTML_DATA_RAWTEXT) is_raw = true;
                cur = first;
                continue;
            }

            const XMQNode *next = xml_node_next(cur);
            const XMQNode *parent = xml_node_parent(cur);
            if (format && next && info && !info->isinline)
            {
                if (!xml_html_is_text_or_ref(next) && parent && xml_node_kind(parent) == XML_DOM_ELEMENT &&
                    !xml_html_name_starts_with_p(parent))
                {
                    membuffer_append_char(mb, '\n');
                }
            }
            break;
        }
        case XML_DOM_ATTRIBUTE:
            xml_html_attr(mb, cur);
            break;
        case XML_DOM_TEXT:
        {
            const char *content = xml_node_content(cur);
            if (!content) break;
            // Text marked "textnoenc" (xslt disable-output-escaping) is written as it is.
            if (is_raw || (xml_node_name(cur) && !strcmp(xml_node_name(cur), "textnoenc"))) membuffer_append(mb, content);
            else xml_html_escape(mb, content, false);
            break;
        }
        case XML_DOM_COMMENT:
        {
            const char *content = xml_node_content(cur);
            if (content)
            {
                membuffer_append(mb, "<!--");
                membuffer_append(mb, content);
                membuffer_append(mb, "-->");
            }
            break;
        }
        case XML_DOM_PI:
            if (xml_node_name(cur))
            {
                membuffer_append(mb, "<?");
                membuffer_append(mb, xml_node_name(cur));
                if (xml_node_content(cur))
                {
                    membuffer_append_char(mb, ' ');
                    membuffer_append(mb, xml_node_content(cur));
                }
                membuffer_append_char(mb, '>');
            }
            break;
        case XML_DOM_ENTITY_REF:
            membuffer_append_char(mb, '&');
            xml_html_put(mb, xml_node_name(cur));
            membuffer_append_char(mb, ';');
            break;
        case XML_DOM_CDATA:
            // HTML_PRESERVE_NODE: raw text such as script content.
            if (xml_node_content(cur)) membuffer_append(mb, xml_node_content(cur));
            break;
        default:
            // The dtd is written first by the document, declarations are not html.
            break;
        }

        while (true)
        {
            if (cur == root) return;
            const XMQNode *next = xml_node_next(cur);
            if (next)
            {
                cur = next;
                break;
            }
            is_raw = false;
            cur = xml_node_parent(cur);
            if (xml_node_kind(cur) == XML_DOM_DOCUMENT)
            {
                membuffer_append_char(mb, '\n');
            }
            else
            {
                const XmlHtmlElementDesc *info = format ? xml_html_info(cur) : NULL;
                const XMQNode *last = xml_node_last_child(cur);
                if (format && info && !info->isinline &&
                    !xml_html_is_text_or_ref(last) &&
                    xml_node_first_child(cur) != last &&
                    !xml_html_name_starts_with_p(cur))
                {
                    membuffer_append_char(mb, '\n');
                }
                membuffer_append(mb, "</");
                xml_html_qname(mb, cur);
                membuffer_append_char(mb, '>');

                const XMQNode *cn = xml_node_next(cur);
                const XMQNode *parent = xml_node_parent(cur);
                if (format && info && !info->isinline && cn)
                {
                    if (!xml_html_is_text_or_ref(cn) && parent && xml_node_kind(parent) == XML_DOM_ELEMENT &&
                        !xml_html_name_starts_with_p(parent))
                    {
                        membuffer_append_char(mb, '\n');
                    }
                }
            }
        }
    }
}

void xml_html_save_doc_to(MemBuffer *mb, XmlDom *dom, bool format)
{
    xml_html_node(mb, xml_dom_document(dom), format);
}

void xml_html_save_node_to(MemBuffer *mb, const XMQNode *node, bool format)
{
    xml_html_node(mb, node, format);
}

static char *xml_html_finish(MemBuffer *mb, size_t *out_len)
{
    size_t len = mb->used_;
    membuffer_append_null(mb);
    if (out_len) *out_len = len;
    return free_membuffer_but_return_trimmed_content(mb);
}

char *xml_html_save_doc(XmlDom *dom, bool format, size_t *out_len)
{
    MemBuffer *mb = new_membuffer();
    xml_html_save_doc_to(mb, dom, format);
    return xml_html_finish(mb, out_len);
}

char *xml_html_save_node(const XMQNode *node, bool format, size_t *out_len)
{
    MemBuffer *mb = new_membuffer();
    xml_html_save_node_to(mb, node, format);
    return xml_html_finish(mb, out_len);
}

// Meta tags with the encoding, port of htmlSetMetaEncoding (HTMLtree.c).

static bool xml_html_save_is_ws(int c)
{
    return c == 0x20 || (c >= 0x09 && c <= 0x0D && c != 0x0B);
}

static XMQNode *xml_html_find_first_child(XMQNode *parent, const char *name)
{
    for (XMQNode *c = xml_node_first_child(parent); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_ELEMENT && !strcasecmp(xml_node_name(c), name)) return c;
    }
    return NULL;
}

static XMQNode *xml_html_find_head(XmlDom *dom)
{
    XMQNode *html = xml_html_find_first_child(xml_dom_document(dom), "html");
    if (!html) return NULL;
    return xml_html_find_first_child(html, "head");
}

typedef struct
{
    size_t start;
    size_t end;
    size_t size;
} XmlHtmlMetaOffsets;

/** Port of htmlParseContentType: the charset in a content-type value. */
static bool xml_html_parse_content_type(const char *val, XmlHtmlMetaOffsets *off)
{
    const char *p = val;
    while (true)
    {
        size_t start, end;
        while (*p != 'c' && *p != 'C')
        {
            if (*p == 0) return false;
            p += 1;
        }
        p += 1;
        if (strncasecmp(p, "harset", 6) != 0) continue;
        p += 6;
        while (xml_html_save_is_ws((unsigned char)*p)) p += 1;
        if (*p != '=') continue;
        p += 1;
        while (xml_html_save_is_ws((unsigned char)*p)) p += 1;
        if (*p == 0) return false;
        if (*p == '"' || *p == '\'')
        {
            int quote = *p;
            p += 1;
            while (xml_html_save_is_ws((unsigned char)*p)) p += 1;
            start = (size_t)(p - val);
            end = start;
            while (*p != quote)
            {
                if (*p == 0) return false;
                if (!xml_html_save_is_ws((unsigned char)*p)) end = (size_t)(p + 1 - val);
                p += 1;
            }
        }
        else
        {
            start = (size_t)(p - val);
            while (*p != 0 && *p != ';' && !xml_html_save_is_ws((unsigned char)*p)) p += 1;
            end = (size_t)(p - val);
        }
        off->start = start;
        off->end = end;
        off->size = (size_t)(p - val) + strlen(p);
        return true;
    }
}

static const char *xml_html_simple_attr_value(XMQNode *attr)
{
    XMQNode *c = xml_node_first_child(attr);
    if (c && xml_node_kind(c) == XML_DOM_TEXT && !xml_node_next(c) && xml_node_content(c)) return xml_node_content(c);
    return NULL;
}

/** Port of htmlFindMetaEncodingAttr. */
static XMQNode *xml_html_find_meta_encoding_attr(XMQNode *elem, bool *is_content_type)
{
    if (strcasecmp(xml_node_name(elem), "meta")) return NULL;
    XMQNode *content_attr = NULL;
    bool ct = false;
    for (XMQAttr *at = xml_node_first_attr(elem); at; at = xml_attr_next(at))
    {
        XMQNode *a = xml_attr_node(at);
        if (xml_node_ns(a)) continue;
        if (!strcasecmp(xml_node_name(a), "charset"))
        {
            *is_content_type = false;
            return a;
        }
        if (!strcasecmp(xml_node_name(a), "content")) content_attr = a;
        const char *v = xml_html_simple_attr_value(a);
        if (!strcasecmp(xml_node_name(a), "http-equiv") && v && !strcasecmp(v, "Content-Type")) ct = true;
    }
    if (ct && content_attr)
    {
        *is_content_type = true;
        return content_attr;
    }
    return NULL;
}

/** Port of htmlParseMetaEncoding: the attribute and the offsets of the encoding in its value. */
static XMQNode *xml_html_parse_meta_encoding(XMQNode *elem, const char **value, XmlHtmlMetaOffsets *off)
{
    if (xml_node_kind(elem) != XML_DOM_ELEMENT || strcasecmp(xml_node_name(elem), "meta")) return NULL;
    bool is_content_type = false;
    XMQNode *attr = xml_html_find_meta_encoding_attr(elem, &is_content_type);
    if (!attr) return NULL;
    const char *val = xml_html_simple_attr_value(attr);
    if (!val) val = "";
    if (!is_content_type)
    {
        size_t size = strlen(val), start = 0, end = size;
        while (start < size && xml_html_save_is_ws((unsigned char)val[start])) start += 1;
        while (end > 0 && xml_html_save_is_ws((unsigned char)val[end - 1])) end -= 1;
        off->start = start;
        off->end = end;
        off->size = size;
        *value = val;
        return attr;
    }
    if (xml_html_parse_content_type(val, off))
    {
        *value = val;
        return attr;
    }
    return NULL;
}

int xml_html_set_meta_encoding(XmlDom *dom, const char *encoding)
{
    if (!encoding) return 1;
    XMQNode *head = xml_html_find_head(dom);
    if (!head) return 1;
    bool found = false;
    for (XMQNode *meta = xml_node_first_child(head); meta; meta = xml_node_next(meta))
    {
        const char *val = NULL;
        XmlHtmlMetaOffsets off;
        XMQNode *attr = xml_html_parse_meta_encoding(meta, &val, &off);
        if (!attr) continue;
        found = true;
        // The pseudo "HTML" encoding only produces ASCII.
        const char *enc = !strcasecmp(encoding, "HTML") ? "ASCII" : encoding;
        size_t old_size = off.end - off.start, new_size = strlen(enc);
        size_t size = off.size - old_size + new_size;
        char *nv = (char*)malloc(size + 1);
        memcpy(nv, val, off.start);
        memcpy(nv + off.start, enc, new_size);
        memcpy(nv + off.start + new_size, val + off.end, off.size - off.end);
        nv[size] = 0;
        xml_node_set_content(attr, nv);
        free(nv);
    }
    if (found) return 0;
    XMQNode *meta = xml_node_new_element(xml_node_dom(head), "meta", NULL);
    xml_node_append_attr(meta, "charset", NULL, encoding);
    XMQNode *first = xml_node_first_child(head);
    if (!first) xml_node_append_child(head, meta);
    else xml_node_add_prev_sibling(first, meta);
    return 0;
}

#endif // XML_HTML_SAVE_MODULE
