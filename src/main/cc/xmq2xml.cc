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

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <ostream>
#include <fstream>
#include <map>
#include <vector>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "util.h"
#include "xmq.h"
#include "xmq_implementation.h"

using namespace std;
using namespace rapidxml;
using namespace xmq;

#define VERSION "0.1"

struct ParseActionsRapidXML : ParseActions
{
    xml_document<> *doc;

    void *root()
    {
        return doc;
    }

    char *allocateCopy(const char *content, size_t len)
    {
        return doc->allocate_string(content, len);
    }

    void *appendElement(void *parent, Token t)
    {
        xml_node<> *p = (xml_node<>*)parent;
        xml_node<> *n = doc->allocate_node(node_element, t.value);
        p->append_node(n);
        return n;
    }

    void appendComment(void *parent, Token t)
    {
        xml_node<> *p = (xml_node<>*)parent;
        p->append_node(doc->allocate_node(node_comment, NULL, t.value));
    }

    void appendData(void *parent, Token t)
    {
        xml_node<> *p = (xml_node<>*)parent;
        p->append_node(doc->allocate_node(node_data, NULL, t.value));
    }

    void appendAttribute(void *parent, Token key, Token val)
    {
        xml_node<> *p = (xml_node<>*)parent;
        p->append_attribute(doc->allocate_attribute(key.value, val.value));
    }

};

int xmq::main_xmq2xml(const char *filename, Settings *provided_settings)
{
    vector<char> *buffer = provided_settings->in;
    xml_document<> doc;
    bool generate_html {};

    if (xmq_implementation::firstWordIsHtml(*buffer))
    {
        generate_html = true;
    }

    if (!provided_settings->no_declaration)
    {
        if (generate_html)
        {
            xml_node<> *node = doc.allocate_node(node_doctype, "!DOCTYPE", "html");
            doc.append_node(node);
        }
        else
        {
            xml_node<> *node = doc.allocate_node(node_declaration, "?xml");
            doc.append_node(node);
            node->append_attribute(doc.allocate_attribute("version", "1.0"));
            node->append_attribute(doc.allocate_attribute("encoding", "UTF-8"));
        }
    }

    ParseActionsRapidXML actions;
    actions.doc = &doc;

    parse(filename, &(*buffer)[0], &actions);

    if (provided_settings->view)
    {
        xml_node<> *node = &doc;
        node = node->first_node();
        if (node->type() == node_doctype || node->type() == node_declaration)
        {
            node = node->next_sibling();
        }
        renderXMQ(node, provided_settings);
    }
    else
    {
        string s;
        int flags = 0;
        if (provided_settings->preserve_ws)
        {
            flags |= rapidxml::print_no_indenting;
        }
        if (generate_html)
        {
            flags |= rapidxml::print_html;
        }
        print(back_inserter(s), doc, flags);
        std::cout << s;
    }

    return 0;
}
