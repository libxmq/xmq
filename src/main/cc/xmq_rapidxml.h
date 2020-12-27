/*
 Copyright (c) 2019-2020 Fredrik Öhrström

 MIT License

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#ifndef XMQ_RAPIDXML_H
#define XMQ_RAPIDXML_H

#include "xmq.h"

#include "rapidxml/rapidxml.hpp"

struct ParseActionsRapidXML : xmq::ParseActions
{
private:
    rapidxml::xml_document<> *doc;

public:
    void setDocument(rapidxml::xml_document<> *d)
    {
        doc = d;
    }

    void *root()
    {
        return doc;
    }

    char *allocateCopy(const char *content, size_t len)
    {
        return doc->allocate_string(content, len);
    }

    void *appendElement(void *parent, xmq::Token t)
    {
        rapidxml::xml_node<> *p = (rapidxml::xml_node<>*)parent;
        rapidxml::xml_node<> *n = doc->allocate_node(rapidxml::node_element, t.value);
        p->append_node(n);
        return n;
    }

    void appendComment(void *parent, xmq::Token t)
    {
        rapidxml::xml_node<> *p = (rapidxml::xml_node<>*)parent;
        p->append_node(doc->allocate_node(rapidxml::node_comment, NULL, t.value));
    }

    void appendData(void *parent, xmq::Token t)
    {
        rapidxml::xml_node<> *p = (rapidxml::xml_node<>*)parent;
        p->append_node(doc->allocate_node(rapidxml::node_data, NULL, t.value));
    }

    void appendAttribute(void *parent, xmq::Token key, xmq::Token val)
    {
        rapidxml::xml_node<> *p = (rapidxml::xml_node<>*)parent;
        p->append_attribute(doc->allocate_attribute(key.value, val.value));
    }

};

struct RenderActionsRapidXML : xmq::RenderActions
{
private:
    rapidxml::xml_node<> *root_;

public:

    void setRoot(rapidxml::xml_node<>* r)
    {
        root_ = r;
    }

    void *root()
    {
        return root_;
    }

    void *firstNode(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->first_node();
    }

    void *nextSibling(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->next_sibling();
    }

    void *firstAttribute(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->first_attribute();
    }

    void *nextAttribute(void *attr)
    {
        rapidxml::xml_attribute<> *n = (rapidxml::xml_attribute<>*)attr;
        return n->next_attribute();
    }

    void *parent(void *node)
    {
        rapidxml::xml_attribute<> *n = (rapidxml::xml_attribute<>*)node;
        return n->parent();
    }

    bool isNodeData(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->type() == rapidxml::node_data;
    }

    bool isNodeComment(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->type() == rapidxml::node_comment;
    }

    bool isNodeCData(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->type() == rapidxml::node_cdata;
    }

    bool isNodeDocType(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->type() == rapidxml::node_doctype;
    }

    bool isNodeDeclaration(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->type() == rapidxml::node_declaration;
    }

    void loadName(void *node, xmq::str *name)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        name->s = n->name();
        name->l = n->name_size();
    }

    void loadValue(void *node, xmq::str *data)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        data->s = n->value();
        data->l = n->value_size();
    }

    bool hasAttributes(void *node)
    {
        rapidxml::xml_node<> *n = (rapidxml::xml_node<>*)node;
        return n->first_attribute() != NULL;
    }

};

#endif
