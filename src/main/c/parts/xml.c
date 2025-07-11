
#ifndef BUILDING_DIST_XMQ

#include"xml.h"
#include"text.h"
#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef XML_MODULE

xmlNode *xml_first_child(xmlNode *node)
{
    return node->children;
}

xmlNode *xml_last_child(xmlNode *node)
{
    return node->last;
}

xmlNode *xml_next_sibling(xmlNode *node)
{
    return node->next;
}

xmlNode *xml_prev_sibling(xmlNode *node)
{
    return node->prev;
}

xmlAttr *xml_first_attribute(xmlNode *node)
{
    return node->properties;
}

xmlAttr *xml_next_attribute(xmlAttr *attr)
{
    return attr->next;
}

xmlAttr *xml_get_attribute(xmlNode *node, const char *name)
{
    return xmlHasProp(node, (const xmlChar*)name);
}

xmlNs *xml_first_namespace_def(xmlNode *node)
{
    return node->nsDef;
}

bool xml_non_empty_namespace(xmlNs *ns)
{
    const char *prefix = (const char*)ns->prefix;
    const char *href = (const char*)ns->href;

    // These three are non_empty.
    //   xmlns = "something"
    //   xmlns:something = ""
    //   xmlns:something = "something"
    return (href && href[0]) || (prefix && prefix[0]);
}

bool xml_has_non_empty_namespace_defs(xmlNode *node)
{
    xmlNs *ns = node->nsDef;
    if (ns) return true;
    /*!= NULL)
    while (ns)
    {
        //if (xml_non_empty_namespace(ns)) return true;
        ns = xml_next_namespace_def(ns);
    }
    */
    return false;
}

xmlNs *xml_next_namespace_def(xmlNs *ns)
{
    return ns->next;
}

const char*xml_element_name(xmlNode *node)
{
    return (const char*)node->name;
}

const char*xml_element_content(xmlNode *node)
{
    return (const char*)node->content;
}

const char *xml_element_ns_prefix(const xmlNode *node)
{
    if (!node->ns) return NULL;
    return (const char*)node->ns->prefix;
}

const char *xml_attr_key(xmlAttr *attr)
{
    return (const char*)attr->name;
}

const char* xml_namespace_href(xmlNs *ns)
{
    return (const char*)ns->href;
}

bool is_entity_node(const xmlNode *node)
{
    return node->type == XML_ENTITY_NODE ||
        node->type == XML_ENTITY_REF_NODE;
}

bool is_content_node(const xmlNode *node)
{
    return node->type == XML_TEXT_NODE ||
        node->type == XML_CDATA_SECTION_NODE;
}

bool is_comment_node(const xmlNode *node)
{
    return node->type == XML_COMMENT_NODE;
}

bool is_pi_node(const xmlNode *node)
{
    return node->type == XML_PI_NODE;
}

bool is_doctype_node(const xmlNode *node)
{
    return node->type == XML_DTD_NODE;
}

bool is_element_node(const xmlNode *node)
{
    return node->type == XML_ELEMENT_NODE;
}

bool is_attribute_node(const xmlNode *node)
{
    return node->type == XML_ATTRIBUTE_NODE;
}

bool is_text_node(const xmlNode *node)
{
    return node->type == XML_TEXT_NODE;
}

bool is_key_value_node(xmlNodePtr node)
{
    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    // Single content or entity node.
    bool yes = from && from == to && (is_content_node((xmlNode*)from) || is_entity_node((xmlNode*)from));
    if (yes) return true;

    if (!from) return false;

    // Multiple text or entity nodes.
    xmlNode *i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        if (i->type != XML_TEXT_NODE &&
            i->type != XML_ENTITY_REF_NODE)
        {
            // Found something other than text or character entities.
            return false;
        }
        i = next;
    }
    return true;
}

bool is_single_empty_text_node(xmlNodePtr node)
{
    if (is_entity_node(node)) return false;
    if (node->type != XML_TEXT_NODE) return false;
    const char *c = (const char*)node->content;
    if (c != NULL && *c != 0) return false;
    return true;
}

bool is_leaf_node(xmlNode *node)
{
    return xml_first_child(node) == NULL;
}

bool has_attributes(xmlNodePtr node)
{
    return NULL != xml_first_attribute(node);
}

void free_xml(xmlNode * node)
{
    while(node)
    {
        xmlNode *next = node->next;
        free_xml(node->children);
        xmlFreeNode(node);
        node = next;
    }
}

char *xml_collapse_text(xmlNode *node)
{
    xmlNode *i = node->children;
    size_t len = 0;
    size_t num_text = 0;
    size_t num_entities = 0;

    while (i)
    {
        xmlNode *next = i->next;
        if (i->type != XML_TEXT_NODE &&
            i->type != XML_ENTITY_REF_NODE)
        {
            // Found something other than text or character entities.
            // Cannot collapse.
            return NULL;
        }
        if (i->type == XML_TEXT_NODE)
        {
            len += strlen((const char*)i->content);
            num_text++;
        }
        else
        {
            // &apos;
            len += 2 + strlen((const char*)i->name);
            num_entities++;
        }
        i = next;
    }

    // It is already collapsed.
    if (num_text <= 1 && num_entities == 0) return NULL;

    char *buf = (char*)malloc(len+1);
    char *out = buf;
    i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        if (i->type == XML_TEXT_NODE)
        {
            size_t l = strlen((const char *)i->content);
            memcpy(out, i->content, l);
            out += l;
        }
        else
        {
            int uc = decode_entity_ref((const char *)i->name);
            UTF8Char utf8;
            size_t n = encode_utf8(uc, &utf8);
            for (size_t j = 0; j < n; ++j)
            {
                *out++ = utf8.bytes[j];
            }
        }
        i = next;
    }
    *out = 0;
    out++;
    buf = (char*)realloc(buf, out-buf);
    return buf;
}

int decode_entity_ref(const char *name)
{
    if (!strcmp(name, "apos")) return '\'';
    if (!strcmp(name, "gt")) return '>';
    if (!strcmp(name, "lt")) return '<';
    if (!strcmp(name, "quot")) return '"';
    if (!strcmp(name, "nbsp")) return 160;
    if (name[0] != '#') return 0;
    if (name[1] == 'x') {
        long v = strtol((const char*)name, NULL, 16);
        return (int)v;
    }
    return atoi(name+1);
}

void xml_add_root_child(xmlDoc *doc, xmlNode *node)
{
    if (doc->children == NULL)
    {
        doc->children = node;
        doc->last = node;
    }
    else
    {
        xmlNode *prev = doc->last;
        prev->next = node;
        node->prev = prev;
        doc->last = node;
    }
}

#endif // XML_MODULE
