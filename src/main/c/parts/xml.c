
#ifndef BUILDING_DIST_XMQ

#include"xml.h"
#include"text.h"
#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef XML_MODULE

XMQNode *xml_first_child(XMQNode *node)
{
    return xml_node_first_child(node);
}

XMQNode *xml_last_child(XMQNode *node)
{
    return xml_node_last_child(node);
}

XMQNode *xml_next_sibling(XMQNode *node)
{
    return xml_node_next(node);
}

XMQNode *xml_prev_sibling(XMQNode *node)
{
    return xml_node_prev(node);
}

XMQAttr *xml_first_attribute(XMQNode *node)
{
    if (xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    return xml_node_first_attr(node);
}

XMQAttr *xml_next_attribute(XMQAttr *attr)
{
    return xml_attr_next(attr);
}

XMQAttr *xml_get_attribute(XMQNode *node, const char *name)
{
    // Like libxml2 xmlHasProp: the name matches regardless of namespace.
    for (XMQAttr *a = xml_first_attribute(node); a; a = xml_attr_next(a))
    {
        if (!strcmp(xml_attr_name(a), name)) return a;
    }
    return NULL;
}

XmlNs *xml_first_namespace_def(XMQNode *node)
{
    if (xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    return xml_node_first_ns_decl(node);
}

bool xml_non_empty_namespace(XmlNs *ns)
{
    const char *prefix = xml_ns_prefix(ns);
    const char *href = xml_ns_href(ns);

    // These three are non_empty.
    //   xmlns = "something"
    //   xmlns:something = ""
    //   xmlns:something = "something"
    return (href && href[0]) || (prefix && prefix[0]);
}

bool xml_has_non_empty_namespace_defs(XMQNode *node)
{
    return xml_first_namespace_def(node) != NULL;
}

XmlNs *xml_next_namespace_def(XmlNs *ns)
{
    return xml_ns_next_decl(ns);
}

const char *xml_node_libxml_name(XMQNode *node)
{
    switch (xml_node_kind(node))
    {
    case XML_DOM_TEXT: return "text";
    case XML_DOM_COMMENT: return "comment";
    default: return xml_node_name(node);
    }
}

const char*xml_element_name(XMQNode *node)
{
    return xml_node_libxml_name(node);
}

const char*xml_element_content(XMQNode *node)
{
    return xml_node_content(node);
}

const char *xml_element_ns_prefix(const XMQNode *node)
{
    XmlNs *ns = xml_node_ns(node);
    if (!ns) return NULL;
    return xml_ns_prefix(ns);
}

const char *xml_attr_key(XMQAttr *attr)
{
    return xml_attr_name(attr);
}

const char* xml_namespace_href(XmlNs *ns)
{
    return xml_ns_href(ns);
}

bool is_entity_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_ENTITY_REF;
}

bool is_content_node(const XMQNode *node)
{
    XmlNodeKind k = xml_node_kind(node);
    return k == XML_DOM_TEXT || k == XML_DOM_CDATA;
}

bool is_comment_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_COMMENT;
}

bool is_pi_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_PI;
}

bool is_doctype_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_DTD;
}

bool is_element_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_ELEMENT;
}

bool is_attribute_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_ATTRIBUTE;
}

bool is_text_node(const XMQNode *node)
{
    return xml_node_kind(node) == XML_DOM_TEXT;
}

bool is_key_value_node(XMQNode *node)
{
    XMQNode *from = xml_first_child(node);
    XMQNode *to = xml_last_child(node);

    // Single content or entity node.
    bool yes = from && from == to && (is_content_node(from) || is_entity_node(from));
    if (yes) return true;

    if (!from) return false;

    // Multiple text or entity nodes.
    for (XMQNode *i = from; i; i = xml_node_next(i))
    {
        XmlNodeKind k = xml_node_kind(i);
        if (k != XML_DOM_TEXT && k != XML_DOM_ENTITY_REF)
        {
            // Found something other than text or character entities.
            return false;
        }
    }
    return true;
}

bool is_single_empty_text_node(XMQNode *node)
{
    if (xml_node_kind(node) != XML_DOM_TEXT) return false;
    const char *c = xml_node_content(node);
    if (c != NULL && *c != 0) return false;
    return true;
}

bool is_leaf_node(XMQNode *node)
{
    return xml_first_child(node) == NULL;
}

bool has_attributes(XMQNode *node)
{
    return NULL != xml_first_attribute(node);
}

void free_xml(XMQNode * node)
{
    while(node)
    {
        XMQNode *next = xml_node_next(node);
        xml_node_free(node);
        node = next;
    }
}

char *xml_collapse_text(XMQNode *node)
{
    size_t len = 0;
    size_t num_text = 0;
    size_t num_entities = 0;

    for (XMQNode *i = xml_first_child(node); i; i = xml_node_next(i))
    {
        XmlNodeKind k = xml_node_kind(i);
        if (k != XML_DOM_TEXT && k != XML_DOM_ENTITY_REF)
        {
            // Found something other than text or character entities.
            // Cannot collapse.
            return NULL;
        }
        if (k == XML_DOM_TEXT)
        {
            len += strlen(xml_node_content(i));
            num_text++;
        }
        else
        {
            // An unknown entity (or an invalid character reference) cannot be
            // replaced by its character, keep the reference.
            int uc = decode_entity_ref(xml_node_name(i));
            if (uc <= 0 || uc > 0x10ffff) return NULL;
            // &apos;
            len += 2 + strlen(xml_node_name(i));
            num_entities++;
        }
    }

    // It is already collapsed.
    if (num_text <= 1 && num_entities == 0) return NULL;

    char *buf = (char*)malloc(len+1);
    char *out = buf;
    for (XMQNode *i = xml_first_child(node); i; i = xml_node_next(i))
    {
        if (xml_node_kind(i) == XML_DOM_TEXT)
        {
            const char *c = xml_node_content(i);
            size_t l = strlen(c);
            memcpy(out, c, l);
            out += l;
        }
        else
        {
            int uc = decode_entity_ref(xml_node_name(i));
            UTF8Char utf8;
            size_t n = encode_utf8(uc, &utf8);
            for (size_t j = 0; j < n; ++j)
            {
                *out++ = utf8.bytes[j];
            }
        }
    }
    *out = 0;
    out++;
    buf = (char*)realloc(buf, out-buf);
    return buf;
}

int decode_entity_ref(const char *name)
{
    if (!strcmp(name, "amp")) return '&';
    if (!strcmp(name, "apos")) return '\'';
    if (!strcmp(name, "gt")) return '>';
    if (!strcmp(name, "lt")) return '<';
    if (!strcmp(name, "quot")) return '"';
    if (!strcmp(name, "nbsp")) return 160;
    if (name[0] != '#') return 0;
    if (name[1] == 'x') {
        long v = strtol((const char*)name+2, NULL, 16);
        return (int)v;
    }
    return atoi(name+1);
}

void xml_add_root_child(XmlDom *doc, XMQNode *node)
{
    xml_node_append_child(xml_dom_document(doc), node);
}

#endif // XML_MODULE
