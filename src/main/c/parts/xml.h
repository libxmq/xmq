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
#include<libxml/tree.h>

void free_xml(xmlNode * node);
xmlNode *xml_first_child(xmlNode *node);
xmlNode *xml_last_child(xmlNode *node);
xmlNode *xml_next_sibling(xmlNode *node);
xmlNode *xml_prev_sibling(xmlNode *node);
xmlAttr *xml_first_attribute(xmlNode *node);
xmlAttr *xml_next_attribute(xmlAttr *attr);
xmlAttr *xml_get_attribute(xmlNode *node, const char *name);
xmlNs *xml_first_namespace_def(xmlNode *node);
xmlNs *xml_next_namespace_def(xmlNs *ns);
bool xml_non_empty_namespace(xmlNs *ns);
bool xml_has_non_empty_namespace_defs(xmlNode *node);
const char*xml_element_name(xmlNode *node);
const char*xml_element_content(xmlNode *node);
const char *xml_element_ns_prefix(const xmlNode *node);
const char *xml_attr_key(xmlAttr *attr);
const char* xml_namespace_href(xmlNs *ns);
bool is_entity_node(const xmlNode *node);
bool is_content_node(const xmlNode *node);
bool is_comment_node(const xmlNode *node);
bool is_doctype_node(const xmlNode *node);
bool is_element_node(const xmlNode *node);
bool is_key_value_node(xmlNodePtr node);
bool is_leaf_node(xmlNode *node);
bool has_attributes(xmlNodePtr node);
char *xml_collapse_text(xmlNode *node);
int decode_entity_ref(const char *name);

#define XML_MODULE

#endif // XML_H
