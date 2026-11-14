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

/* Portions of this file are ported from libxml2 (xmlsave.c, xmlIO.c, xmlstring.c and
   codegen/escape.inc, version 2.16.0 as found in the libxmq fork in 3rdparty/libxml2).
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

#include"xml_save.h"

#include<assert.h>
#include<string.h>

#endif

#ifdef XML_SAVE_MODULE

/*
    Port of the libxml2 serializer xmlsave.c (Copyright (C) 1998-2012 Daniel Veillard, MIT license)
    as found in the libxmq fork in 3rdparty/libxml2.
*/

#define XML_SAVE_INDENT "  "
#define XML_SAVE_INDENT_SIZE 2
#define XML_SAVE_INDENT_NR (60 / XML_SAVE_INDENT_SIZE)

typedef struct
{
    MemBuffer *mb;
    int level;
    int format;
    bool escape_non_ascii; // True when there is no output encoding.
} XmlSaveCtxt;

typedef enum
{
    XML_SAVE_ESCAPE_TEXT,
    XML_SAVE_ESCAPE_ATTR,
} XmlSaveEscapeMode;

static void xml_save_node_internal(XmlSaveCtxt *ctxt, const XMQNode *cur);

static void xml_save_put(XmlSaveCtxt *ctxt, const char *s)
{
    if (s) membuffer_append(ctxt->mb, s);
}

static void xml_save_putc(XmlSaveCtxt *ctxt, char c)
{
    membuffer_append_char(ctxt->mb, c);
}

/** Port of xmlGetUTF8Char. Returns the code point or -1 on invalid utf8. */
static int xml_save_utf8_char(const unsigned char *utf, int *len)
{
    unsigned int c = utf[0];
    if (c < 0x80) { *len = 1; return (int)c; }
    if ((utf[1] & 0xc0) != 0x80) goto error;
    if (c < 0xe0)
    {
        if (c < 0xc2) goto error;
        *len = 2;
        return (int)(((c & 0x1f) << 6) | (utf[1] & 0x3f));
    }
    if ((utf[2] & 0xc0) != 0x80) goto error;
    if (c < 0xf0)
    {
        *len = 3;
        c = ((c & 0xf) << 12) | ((utf[1] & 0x3f) << 6) | (utf[2] & 0x3f);
        if (c < 0x800 || (c >= 0xd800 && c < 0xe000)) goto error;
        return (int)c;
    }
    if ((utf[3] & 0xc0) != 0x80) goto error;
    *len = 4;
    c = ((c & 0x7) << 18) | ((utf[1] & 0x3f) << 12) | ((utf[2] & 0x3f) << 6) | (utf[3] & 0x3f);
    if (c < 0x10000 || c >= 0x110000) goto error;
    return (int)c;
error:
    *len = 0;
    return -1;
}

/** Port of xmlSerializeHexCharRef. */
static void xml_save_hex_char_ref(XmlSaveCtxt *ctxt, int val)
{
    char buf[16];
    char *out = buf;
    int shift = 0;
    int bits = val;

    *out++ = '&';
    *out++ = '#';
    *out++ = 'x';
    if (bits & 0xFF0000) { shift = 16; bits &= 0xFF0000; }
    else if (bits & 0x00FF00) { shift = 8; bits &= 0x00FF00; }
    if (bits & 0xF0F0F0) shift += 4;
    do
    {
        int d = (val >> shift) & 0x0F;
        *out++ = d < 10 ? '0' + d : 'A' + (d - 10);
        shift -= 4;
    } while (shift >= 0);
    *out++ = ';';
    *out = 0;
    xml_save_put(ctxt, buf);
}

/**
    Port of xmlSerializeText with the xmlEscapeTab / xmlEscapeTabAttr tables:
    text: \r & < > are escaped, attr: also \t \n and ".
    Other control characters (below 0x20) are invalid in xml and become &#xFFFD;
*/
static void xml_save_escape(XmlSaveCtxt *ctxt, const char *s, XmlSaveEscapeMode mode)
{
    if (!s) return;
    const unsigned char *cur = (const unsigned char*)s;
    const unsigned char *base = cur;

    while (true)
    {
        unsigned char c = *cur;
        const char *repl = NULL;

        if (c == 0) break;
        if (c < 0x80)
        {
            switch (c)
            {
            case '&': repl = "&amp;"; break;
            case '<': repl = "&lt;"; break;
            case '>': repl = "&gt;"; break;
            case '\r': repl = "&#13;"; break;
            case '\t': if (mode == XML_SAVE_ESCAPE_ATTR) repl = "&#9;"; break;
            case '\n': if (mode == XML_SAVE_ESCAPE_ATTR) repl = "&#10;"; break;
            case '"': if (mode == XML_SAVE_ESCAPE_ATTR) repl = "&quot;"; break;
            default:
                if (c < 0x20 && c != '\t' && c != '\n') repl = "&#xFFFD;";
                break;
            }
            if (!repl) { cur++; continue; }
        }
        else if (!ctxt->escape_non_ascii)
        {
            cur++;
            continue;
        }

        if (cur > base) membuffer_append_region(ctxt->mb, (const char*)base, (const char*)cur);

        if (repl)
        {
            xml_save_put(ctxt, repl);
            cur++;
        }
        else
        {
            int len = 0;
            int val = xml_save_utf8_char(cur, &len);
            if (val < 0)
            {
                val = 0xFFFD;
                cur += 1;
            }
            else
            {
                if (val == 0xFFFE || val == 0xFFFF) val = 0xFFFD;
                cur += len;
            }
            xml_save_hex_char_ref(ctxt, val);
        }
        base = cur;
    }
    if (cur > base) membuffer_append_region(ctxt->mb, (const char*)base, (const char*)cur);
}

void xml_save_write_quoted(MemBuffer *mb, const char *s)
{
    if (!s) s = "";
    if (strchr(s, '"'))
    {
        if (strchr(s, '\''))
        {
            membuffer_append_char(mb, '"');
            for (const char *i = s; *i; ++i)
            {
                if (*i == '"') membuffer_append(mb, "&quot;");
                else membuffer_append_char(mb, *i);
            }
            membuffer_append_char(mb, '"');
        }
        else
        {
            membuffer_append_char(mb, '\'');
            membuffer_append(mb, s);
            membuffer_append_char(mb, '\'');
        }
    }
    else
    {
        membuffer_append_char(mb, '"');
        membuffer_append(mb, s);
        membuffer_append_char(mb, '"');
    }
}

static void xml_save_indent(XmlSaveCtxt *ctxt)
{
    int n = ctxt->level > XML_SAVE_INDENT_NR ? XML_SAVE_INDENT_NR : ctxt->level;
    for (int i = 0; i < n; ++i) xml_save_put(ctxt, XML_SAVE_INDENT);
}

static void xml_save_qname(XmlSaveCtxt *ctxt, const XMQNode *n)
{
    const char *prefix = xml_node_ns_prefix(n);
    if (prefix)
    {
        xml_save_put(ctxt, prefix);
        xml_save_putc(ctxt, ':');
    }
    xml_save_put(ctxt, xml_node_name(n));
}

/** Port of xmlNsDumpOutput. */
static void xml_save_ns(XmlSaveCtxt *ctxt, const XmlNs *ns)
{
    const char *prefix = xml_ns_prefix(ns);
    const char *href = xml_ns_href(ns);
    if (!href) return;
    if (prefix && !strcmp(prefix, "xml")) return;
    if (prefix)
    {
        xml_save_put(ctxt, " xmlns:");
        xml_save_put(ctxt, prefix);
    }
    else
    {
        xml_save_put(ctxt, " xmlns");
    }
    xml_save_put(ctxt, "=\"");
    xml_save_escape(ctxt, href, XML_SAVE_ESCAPE_ATTR);
    xml_save_putc(ctxt, '"');
}

/** Port of xmlAttrDumpOutput/xmlSaveWriteAttrContent. */
static void xml_save_attr(XmlSaveCtxt *ctxt, const XMQNode *a)
{
    xml_save_putc(ctxt, ' ');
    xml_save_qname(ctxt, a);
    xml_save_put(ctxt, "=\"");
    for (const XMQNode *c = xml_node_first_child(a); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_TEXT)
        {
            xml_save_escape(ctxt, xml_node_content(c), XML_SAVE_ESCAPE_ATTR);
        }
        else if (xml_node_kind(c) == XML_DOM_ENTITY_REF)
        {
            xml_save_putc(ctxt, '&');
            xml_save_put(ctxt, xml_node_name(c));
            xml_save_putc(ctxt, ';');
        }
    }
    xml_save_putc(ctxt, '"');
}

/** Port of xmlBufDumpEntityContent. */
static void xml_save_entity_content(XmlSaveCtxt *ctxt, const char *content)
{
    if (!content) return;
    xml_save_putc(ctxt, '"');
    for (const char *i = content; *i; ++i)
    {
        if (*i == '"') xml_save_put(ctxt, "&quot;");
        else if (*i == '%') xml_save_put(ctxt, "&#x25;");
        else xml_save_putc(ctxt, *i);
    }
    xml_save_putc(ctxt, '"');
}

/** Port of xmlBufDumpEntityDecl. */
static void xml_save_entity_decl(XmlSaveCtxt *ctxt, const XMQNode *e)
{
    XmlEntityType etype = xml_entity_decl_type(e);
    bool parameter = etype == XML_ENTITY_INTERNAL_PARAMETER || etype == XML_ENTITY_EXTERNAL_PARAMETER;
    xml_save_put(ctxt, parameter ? "<!ENTITY % " : "<!ENTITY ");
    xml_save_put(ctxt, xml_node_name(e));
    xml_save_putc(ctxt, ' ');

    if (etype == XML_ENTITY_EXTERNAL_GENERAL_PARSED ||
        etype == XML_ENTITY_EXTERNAL_GENERAL_UNPARSED ||
        etype == XML_ENTITY_EXTERNAL_PARAMETER)
    {
        const char *public_id = xml_entity_decl_public_id(e);
        if (public_id)
        {
            xml_save_put(ctxt, "PUBLIC ");
            xml_save_write_quoted(ctxt->mb, public_id);
            xml_save_putc(ctxt, ' ');
        }
        else
        {
            xml_save_put(ctxt, "SYSTEM ");
        }
        xml_save_write_quoted(ctxt->mb, xml_entity_decl_system_id(e));
    }

    const char *notation = xml_entity_decl_notation(e);
    if (etype == XML_ENTITY_EXTERNAL_GENERAL_UNPARSED && notation)
    {
        xml_save_put(ctxt, " NDATA ");
        xml_save_put(ctxt, notation);
    }

    if (etype == XML_ENTITY_INTERNAL_GENERAL || etype == XML_ENTITY_INTERNAL_PARAMETER)
    {
        const char *orig = xml_entity_decl_orig(e);
        if (orig) xml_save_write_quoted(ctxt->mb, orig);
        else xml_save_entity_content(ctxt, xml_node_content(e));
    }
    xml_save_put(ctxt, ">\n");
}

/** Element, attribute and notation declarations are stored preformatted by the parser. */
static void xml_save_dtd_decl(XmlSaveCtxt *ctxt, const XMQNode *d)
{
    xml_save_put(ctxt, xml_node_content(d));
    const char *default_value = xml_decl_default_value(d);
    if (default_value)
    {
        xml_save_put(ctxt, " \"");
        xml_save_escape(ctxt, default_value, XML_SAVE_ESCAPE_ATTR);
        xml_save_putc(ctxt, '"');
    }
    xml_save_put(ctxt, ">\n");
}

/** Port of xmlDtdDumpOutput. */
static void xml_save_dtd(XmlSaveCtxt *ctxt, const XMQNode *dtd)
{
    const char *public_id = xml_dtd_public_id(dtd);
    const char *system_id = xml_dtd_system_id(dtd);
    xml_save_put(ctxt, "<!DOCTYPE ");
    xml_save_put(ctxt, xml_node_name(dtd));
    if (public_id)
    {
        xml_save_put(ctxt, " PUBLIC ");
        xml_save_write_quoted(ctxt->mb, public_id);
        xml_save_putc(ctxt, ' ');
        xml_save_write_quoted(ctxt->mb, system_id);
    }
    else if (system_id)
    {
        xml_save_put(ctxt, " SYSTEM ");
        xml_save_write_quoted(ctxt->mb, system_id);
    }
    if (!xml_dtd_has_declarations(dtd))
    {
        xml_save_putc(ctxt, '>');
        return;
    }
    xml_save_put(ctxt, " [\n");
    for (const XMQNode *n = xml_dtd_first_notation(dtd); n; n = xml_node_next(n))
    {
        xml_save_dtd_decl(ctxt, n);
    }
    int format = ctxt->format;
    int level = ctxt->level;
    ctxt->format = 0;
    ctxt->level = -1;
    for (const XMQNode *c = xml_node_first_child(dtd); c; c = xml_node_next(c))
    {
        xml_save_node_internal(ctxt, c);
    }
    ctxt->format = format;
    ctxt->level = level;
    xml_save_put(ctxt, "]>");
}

static void xml_save_cdata(XmlSaveCtxt *ctxt, const char *content)
{
    if (!content || !*content)
    {
        xml_save_put(ctxt, "<![CDATA[]]>");
        return;
    }
    const char *start = content;
    const char *end = content;
    while (*end)
    {
        if (end[0] == ']' && end[1] == ']' && end[2] == '>')
        {
            end += 2;
            xml_save_put(ctxt, "<![CDATA[");
            membuffer_append_region(ctxt->mb, start, end);
            xml_save_put(ctxt, "]]>");
            start = end;
        }
        end++;
    }
    if (start != end)
    {
        xml_save_put(ctxt, "<![CDATA[");
        xml_save_put(ctxt, start);
        xml_save_put(ctxt, "]]>");
    }
}

static void xml_save_doc_internal(XmlSaveCtxt *ctxt, XmlDom *dom, const char *encoding)
{
    const char *version = xml_dom_version(dom);
    int standalone = xml_dom_standalone(dom);
    xml_save_put(ctxt, "<?xml version=\"");
    xml_save_put(ctxt, version ? version : "1.0");
    xml_save_putc(ctxt, '"');
    if (encoding)
    {
        xml_save_put(ctxt, " encoding=\"");
        xml_save_put(ctxt, encoding);
        xml_save_putc(ctxt, '"');
    }
    if (standalone == 0) xml_save_put(ctxt, " standalone=\"no\"");
    else if (standalone == 1) xml_save_put(ctxt, " standalone=\"yes\"");
    xml_save_put(ctxt, "?>\n");

    for (const XMQNode *c = xml_node_first_child(xml_dom_document(dom)); c; c = xml_node_next(c))
    {
        ctxt->level = 0;
        xml_save_node_internal(ctxt, c);
        xml_save_putc(ctxt, '\n');
    }
}

/** Port of xmlNodeDumpOutputInternal, iterative to handle deep trees. */
static void xml_save_node_internal(XmlSaveCtxt *ctxt, const XMQNode *cur)
{
    int format = ctxt->format;
    const XMQNode *root = cur;
    const XMQNode *unformatted = NULL;

    while (true)
    {
        XmlNodeKind kind = xml_node_kind(cur);
        switch (kind)
        {
        case XML_DOM_DOCUMENT:
        {
            XmlDom *dom = xml_node_dom(cur);
            xml_save_doc_internal(ctxt, dom, xml_dom_encoding(dom));
            break;
        }
        case XML_DOM_DTD:
            xml_save_dtd(ctxt, cur);
            break;
        case XML_DOM_ELEMENT_DECL:
        case XML_DOM_ATTRIBUTE_DECL:
        case XML_DOM_NOTATION_DECL:
            xml_save_dtd_decl(ctxt, cur);
            break;
        case XML_DOM_ENTITY_DECL:
            xml_save_entity_decl(ctxt, cur);
            break;
        case XML_DOM_PE_REF:
            // libxml2 has no such node, it always expands parameter entities.
            xml_save_putc(ctxt, '%');
            xml_save_put(ctxt, xml_node_name(cur));
            xml_save_put(ctxt, ";\n");
            break;
        case XML_DOM_ELEMENT:
        {
            if (cur != root && ctxt->format == 1) xml_save_indent(ctxt);
            xml_save_putc(ctxt, '<');
            xml_save_qname(ctxt, cur);
            for (const XmlNs *ns = xml_node_first_ns_decl(cur); ns; ns = xml_ns_next_decl(ns)) xml_save_ns(ctxt, ns);
            for (XMQAttr *a = xml_node_first_attr(cur); a; a = xml_attr_next(a)) xml_save_attr(ctxt, xml_attr_node(a));

            const XMQNode *first = xml_node_first_child(cur);
            if (!first)
            {
                xml_save_put(ctxt, "/>");
                break;
            }
            if (ctxt->format == 1)
            {
                for (const XMQNode *t = first; t; t = xml_node_next(t))
                {
                    XmlNodeKind tk = xml_node_kind(t);
                    if (tk == XML_DOM_TEXT || tk == XML_DOM_CDATA || tk == XML_DOM_ENTITY_REF)
                    {
                        ctxt->format = 0;
                        unformatted = cur;
                        break;
                    }
                }
            }
            xml_save_putc(ctxt, '>');
            if (ctxt->format == 1) xml_save_putc(ctxt, '\n');
            if (ctxt->level >= 0) ctxt->level++;
            cur = first;
            continue;
        }
        case XML_DOM_TEXT:
            xml_save_escape(ctxt, xml_node_content(cur), XML_SAVE_ESCAPE_TEXT);
            break;
        case XML_DOM_PI:
        {
            const char *data = xml_node_content(cur);
            if (cur != root && ctxt->format == 1) xml_save_indent(ctxt);
            xml_save_put(ctxt, "<?");
            xml_save_put(ctxt, xml_node_name(cur));
            if (data)
            {
                xml_save_putc(ctxt, ' ');
                xml_save_put(ctxt, data);
            }
            xml_save_put(ctxt, "?>");
            break;
        }
        case XML_DOM_COMMENT:
        {
            const char *text = xml_node_content(cur);
            if (cur != root && ctxt->format == 1) xml_save_indent(ctxt);
            if (text)
            {
                xml_save_put(ctxt, "<!--");
                xml_save_put(ctxt, text);
                xml_save_put(ctxt, "-->");
            }
            break;
        }
        case XML_DOM_ENTITY_REF:
            xml_save_putc(ctxt, '&');
            xml_save_put(ctxt, xml_node_name(cur));
            xml_save_putc(ctxt, ';');
            break;
        case XML_DOM_CDATA:
            xml_save_cdata(ctxt, xml_node_content(cur));
            break;
        case XML_DOM_ATTRIBUTE:
            xml_save_attr(ctxt, cur);
            break;
        }

        while (true)
        {
            if (cur == root) return;
            if (ctxt->format == 1) xml_save_putc(ctxt, '\n');
            const XMQNode *next = xml_node_next(cur);
            if (next)
            {
                cur = next;
                break;
            }
            cur = xml_node_parent(cur);
            if (xml_node_kind(cur) == XML_DOM_ELEMENT)
            {
                if (ctxt->level > 0) ctxt->level--;
                if (ctxt->format == 1) xml_save_indent(ctxt);
                xml_save_put(ctxt, "</");
                xml_save_qname(ctxt, cur);
                xml_save_putc(ctxt, '>');
                if (cur == unformatted)
                {
                    ctxt->format = format;
                    unformatted = NULL;
                }
            }
        }
    }
}

void xml_save_doc_to(MemBuffer *mb, XmlDom *dom, const char *encoding, bool format)
{
    if (!encoding) encoding = xml_dom_encoding(dom);
    XmlSaveCtxt ctxt = { mb, 0, format ? 1 : 0, encoding == NULL };
    xml_save_doc_internal(&ctxt, dom, encoding);
}

void xml_save_node_to(MemBuffer *mb, const XMQNode *node, int level, bool format)
{
    if (level < 0) level = 0;
    else if (level > 100) level = 100;
    XmlSaveCtxt ctxt = { mb, level, format ? 1 : 0, false };
    xml_save_node_internal(&ctxt, node);
}

static char *xml_save_finish(MemBuffer *mb, size_t *out_len)
{
    size_t len = mb->used_;
    membuffer_append_null(mb);
    if (out_len) *out_len = len;
    return free_membuffer_but_return_trimmed_content(mb);
}

char *xml_save_doc(XmlDom *dom, const char *encoding, bool format, size_t *out_len)
{
    MemBuffer *mb = new_membuffer();
    xml_save_doc_to(mb, dom, encoding, format);
    return xml_save_finish(mb, out_len);
}

char *xml_save_node(const XMQNode *node, int level, bool format, size_t *out_len)
{
    MemBuffer *mb = new_membuffer();
    xml_save_node_to(mb, node, level, format);
    return xml_save_finish(mb, out_len);
}

#endif // XML_SAVE_MODULE
