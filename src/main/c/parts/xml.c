
#ifndef BUILDING_XMQ

#include"xml.h"
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

xmlNs *xml_first_namespace_def(xmlNode *node)
{
    return node->nsDef;
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

bool is_doctype_node(const xmlNode *node)
{
    return node->type == XML_DTD_NODE;
}

bool is_element_node(const xmlNode *node)
{
    return node->type == XML_ELEMENT_NODE;
}

bool is_key_value_node(xmlNodePtr node)
{
    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    return from && from == to && (is_content_node((xmlNode*)from) || is_entity_node((xmlNode*)from));
}

bool is_leaf_node(xmlNode *node)
{
    return xml_first_child(node) == NULL;
}

bool has_attributes(xmlNodePtr node)
{
    return NULL == xml_first_attribute(node);
}

#endif // XML_MODULE
