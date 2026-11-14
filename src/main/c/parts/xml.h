/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

#ifndef XML_H
#define XML_H

#include<stdbool.h>
#ifndef BUILDING_DIST_XMQ
#include"xml_dom.h"
#endif

int decode_entity_ref(const char *name);
void free_xml(XMQNode * node);
bool has_attributes(XMQNode* node);
bool is_attribute_node(const XMQNode *node);
bool is_comment_node(const XMQNode *node);
bool is_content_node(const XMQNode *node);
bool is_doctype_node(const XMQNode *node);
bool is_element_node(const XMQNode *node);
bool is_entity_node(const XMQNode *node);
bool is_key_value_node(XMQNode* node);
bool is_leaf_node(XMQNode *node);
bool is_pi_node(const XMQNode *node);
bool is_single_empty_text_node(XMQNode* node);
bool is_text_node(const XMQNode *node);
void xml_add_root_child(XmlDom *doc, XMQNode *node);
const char *xml_attr_key(XMQAttr *attr);
char *xml_collapse_text(XMQNode *node);
const char *xml_element_content(XMQNode *node);
const char *xml_element_name(XMQNode *node);
/** The name libxml2 gives the node, eg "text" for text nodes and "comment" for comments. */
const char *xml_node_libxml_name(XMQNode *node);
const char *xml_element_ns_prefix(const XMQNode *node);
XMQAttr *xml_first_attribute(XMQNode *node);
XMQNode *xml_first_child(XMQNode *node);
XmlNs *xml_first_namespace_def(XMQNode *node);
XMQAttr *xml_get_attribute(XMQNode *node, const char *name);
bool xml_has_non_empty_namespace_defs(XMQNode *node);
XMQNode *xml_last_child(XMQNode *node);
const char *xml_namespace_href(XmlNs *ns);
XMQAttr *xml_next_attribute(XMQAttr *attr);
XmlNs *xml_next_namespace_def(XmlNs *ns);
XMQNode *xml_next_sibling(XMQNode *node);
bool xml_non_empty_namespace(XmlNs *ns);
XMQNode *xml_prev_sibling(XMQNode *node);

#define XML_MODULE

#endif // XML_H
