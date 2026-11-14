/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Differential test of the native serializer (parts/xml_save.c) against libxml2.

   For every file given on the command line: parse it with libxml2 using the same
   options as xmq (XML_PARSE_NOCDATA|XML_PARSE_NONET, with and without NOBLANKS),
   copy the libxml2 tree into the native XmlDom, then serialize the document with
   both xmlDocDumpMemoryEnc and xml_save_doc and compare the bytes. Every top level
   node is also compared with xmlNodeDump vs xml_save_node.

   This tool links libxml2 and is only used while libxmq moves away from libxml2.
*/

#include"parts/xml_dom.h"
#include"parts/xml_save.h"

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/entities.h>
#include<libxml/hash.h>

#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

static int verbose_ = 0;

// The parts' logging (always.c) calls into xmq.c, which is not linked here.
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    (void)lc; (void)ap;
    return strdup(element_name ? element_name : "");
}

static void copy_children(XmlDom *dom, xmlNodePtr from, XMQNode *to);

static XmlNs *map_ns(XMQNode *n, xmlNsPtr ns)
{
    if (!ns) return NULL;
    XmlNs *found = xml_ns_lookup_prefix(n, (const char*)ns->prefix);
    if (found && !strcmp(xml_ns_href(found), ns->href ? (const char*)ns->href : "")) return found;
    // Not declared in scope (can happen with libxml2 trees), declare it here.
    XmlNs *d = xml_ns_declare(xml_node_kind(n) == XML_DOM_ATTRIBUTE ? xml_node_parent(n) : n, (const char*)ns->prefix, (const char*)ns->href);
    return d ? d : found;
}

static char *dump_node(xmlDocPtr doc, xmlNodePtr n)
{
    xmlBufferPtr b = xmlBufferCreate();
    xmlNodeDump(b, doc, n, 0, 0);
    char *r = strdup((const char*)xmlBufferContent(b));
    xmlBufferFree(b);
    return r;
}

static char *strip_decl_end(char *s)
{
    size_t len = strlen(s);
    if (len >= 2 && s[len-2] == '>' && s[len-1] == '\n') s[len-2] = 0;
    return s;
}

typedef struct { XMQNode *dtd; } NotationScan;

// Recreates libxml2's xmlBufDumpNotationDecl (xmlsave.c) without the final ">\n", since the
// native dom stores declarations preformatted.
// libxml2: Copyright (C) 1998-2012 Daniel Veillard, Copyright (C) The Libxml2 Contributors, MIT license.
static void notation_scanner(void *payload, void *data, const xmlChar *name)
{
    xmlNotationPtr nota = (xmlNotationPtr)payload;
    NotationScan *ns = (NotationScan*)data;
    MemBuffer *mb = new_membuffer();
    membuffer_append(mb, "<!NOTATION ");
    membuffer_append(mb, (const char*)nota->name);
    if (nota->PublicID)
    {
        membuffer_append(mb, " PUBLIC ");
        xml_save_write_quoted(mb, (const char*)nota->PublicID);
        if (nota->SystemID)
        {
            membuffer_append(mb, " ");
            xml_save_write_quoted(mb, (const char*)nota->SystemID);
        }
    }
    else
    {
        membuffer_append(mb, " SYSTEM ");
        xml_save_write_quoted(mb, (const char*)nota->SystemID);
    }
    membuffer_append(mb, " ");
    membuffer_append_null(mb);
    char *text = free_membuffer_but_return_trimmed_content(mb);
    xml_dtd_add_notation(ns->dtd, (const char*)name, text);
    free(text);
}

static void copy_dtd(XmlDom *dom, xmlDocPtr doc, xmlDtdPtr d)
{
    XMQNode *dtd = xml_dom_set_dtd(dom, (const char*)d->name, (const char*)d->ExternalID, (const char*)d->SystemID);
    if (d->notations)
    {
        NotationScan ns = { dtd };
        xmlHashScan((xmlHashTablePtr)d->notations, notation_scanner, &ns);
    }
    for (xmlNodePtr c = d->children; c; c = c->next)
    {
        switch (c->type)
        {
        case XML_ENTITY_DECL:
        {
            xmlEntityPtr e = (xmlEntityPtr)c;
            const char *notation = NULL;
            if (e->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY)
            {
                notation = e->orig ? (const char*)e->orig : (const char*)e->content;
            }
            xml_dtd_add_entity(dtd, (const char*)e->name, (XmlEntityType)e->etype,
                               (const char*)e->ExternalID, (const char*)e->SystemID,
                               notation,
                               e->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY ? NULL : (const char*)e->content,
                               e->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY ? NULL : (const char*)e->orig);
            break;
        }
        case XML_ELEMENT_DECL:
        case XML_ATTRIBUTE_DECL:
        {
            char *text = strip_decl_end(dump_node(doc, c));
            xml_dtd_add_decl(dtd, c->type == XML_ELEMENT_DECL ? XML_DOM_ELEMENT_DECL : XML_DOM_ATTRIBUTE_DECL,
                             (const char*)c->name, text, NULL);
            free(text);
            break;
        }
        case XML_COMMENT_NODE:
            xml_node_append_child(dtd, xml_node_new_comment(dom, (const char*)c->content));
            break;
        case XML_PI_NODE:
            xml_node_append_child(dtd, xml_node_new_pi(dom, (const char*)c->name, (const char*)c->content));
            break;
        default:
            break;
        }
    }
}

static XMQNode *copy_node(XmlDom *dom, xmlNodePtr from, XMQNode *parent)
{
    XMQNode *n = NULL;
    switch (from->type)
    {
    case XML_ELEMENT_NODE:
    {
        n = xml_node_append_child(parent, xml_node_new_element(dom, (const char*)from->name, NULL));
        for (xmlNsPtr ns = from->nsDef; ns; ns = ns->next)
        {
            xml_ns_declare(n, (const char*)ns->prefix, (const char*)ns->href);
        }
        xml_node_set_ns(n, map_ns(n, from->ns));
        for (xmlAttrPtr a = from->properties; a; a = a->next)
        {
            XMQAttr *na = xml_node_set_attr(n, (const char*)a->name, map_ns(n, a->ns), NULL);
            copy_children(dom, a->children, xml_attr_node(na));
        }
        copy_children(dom, from->children, n);
        break;
    }
    case XML_TEXT_NODE:
        n = xml_node_append_child(parent, xml_node_new_text(dom, (const char*)from->content));
        break;
    case XML_CDATA_SECTION_NODE:
        n = xml_node_append_child(parent, xml_node_new_cdata(dom, (const char*)from->content, from->content ? strlen((const char*)from->content) : 0));
        break;
    case XML_ENTITY_REF_NODE:
        n = xml_node_append_child(parent, xml_node_new_entity_ref(dom, (const char*)from->name));
        break;
    case XML_PI_NODE:
        n = xml_node_append_child(parent, xml_node_new_pi(dom, (const char*)from->name, (const char*)from->content));
        break;
    case XML_COMMENT_NODE:
        n = xml_node_append_child(parent, xml_node_new_comment(dom, (const char*)from->content));
        if (!from->content) xml_node_set_content(n, NULL);
        break;
    case XML_DTD_NODE:
        copy_dtd(dom, from->doc, (xmlDtdPtr)from);
        n = xml_dom_dtd(dom);
        break;
    default:
        break;
    }
    return n;
}

static void copy_children(XmlDom *dom, xmlNodePtr from, XMQNode *to)
{
    for (xmlNodePtr c = from; c; c = c->next) copy_node(dom, c, to);
}

static XmlDom *copy_doc(xmlDocPtr doc)
{
    XmlDom *dom = xml_dom_new();
    xml_dom_set_version(dom, (const char*)doc->version);
    xml_dom_set_encoding(dom, (const char*)doc->encoding);
    xml_dom_set_standalone(dom, doc->standalone);
    copy_children(dom, doc->children, xml_dom_document(dom));
    return dom;
}

static void show_diff(const char *what, const char *file, const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] && a[i] == b[i]) i++;
    size_t from = i > 40 ? i-40 : 0;
    printf("DIFF %s %s at byte %zu\n  libxml2: \"%.*s\"\n  native:  \"%.*s\"\n",
           what, file, i, 80, a+from, 80, b+from);
}

static int test_file(const char *file, int options)
{
    xmlResetLastError();
    xmlDocPtr doc = xmlReadFile(file, NULL, options | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return -1; // Not well formed, skip.
    const xmlError *err = xmlGetLastError();
    if (err && err->level >= XML_ERR_ERROR)
    {
        // Recovered errors (eg namespace errors) give trees the native dom does not allow. Skip.
        xmlFreeDoc(doc);
        return -1;
    }

    int fails = 0;
    XmlDom *dom = copy_doc(doc);

    xmlChar *expected = NULL;
    int expected_len = 0;
    xmlDocDumpMemoryEnc(doc, &expected, &expected_len, "utf-8");
    size_t got_len = 0;
    char *got = xml_save_doc(dom, "utf-8", false, &got_len);
    if ((size_t)expected_len != got_len || memcmp(expected, got, got_len))
    {
        show_diff("doc", file, (const char*)expected, got);
        fails++;
    }
    xmlFree(expected);
    free(got);

    // Also the formatted dump.
    xmlDocDumpFormatMemoryEnc(doc, &expected, &expected_len, "utf-8", 1);
    got = xml_save_doc(dom, "utf-8", true, &got_len);
    if ((size_t)expected_len != got_len || memcmp(expected, got, got_len))
    {
        show_diff("formatted doc", file, (const char*)expected, got);
        fails++;
    }
    xmlFree(expected);
    free(got);

    // And every top level node with xmlNodeDump.
    XMQNode *nn = xml_node_first_child(xml_dom_document(dom));
    for (xmlNodePtr n = doc->children; n && nn; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE && n->type != XML_DTD_NODE && n->type != XML_PI_NODE && n->type != XML_COMMENT_NODE) continue;
        char *e = dump_node(doc, n);
        char *g = xml_save_node(nn, 0, false, NULL);
        if (strcmp(e, g))
        {
            show_diff("node", file, e, g);
            fails++;
        }
        free(e);
        free(g);
        nn = xml_node_next(nn);
    }

    xml_dom_free(dom);
    xmlFreeDoc(doc);
    if (verbose_ && !fails) printf("OK %s\n", file);
    return fails;
}

int main(int argc, char **argv)
{
    int files = 0, skipped = 0, failed = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-v")) { verbose_ = 1; continue; }
        int f1 = test_file(argv[i], XML_PARSE_NOCDATA | XML_PARSE_NONET);
        if (f1 < 0) { skipped++; continue; }
        int f2 = test_file(argv[i], XML_PARSE_NOCDATA | XML_PARSE_NONET | XML_PARSE_NOBLANKS);
        int f3 = test_file(argv[i], XML_PARSE_NONET); // Keep cdata sections too.
        files++;
        if (f1 || f2 || f3) failed++;
    }
    xmlCleanupParser();
    printf("xml_save_diff: %d files compared, %d failed, %d not well-formed (skipped)\n", files, failed, skipped);
    return failed ? 1 : 0;
}
