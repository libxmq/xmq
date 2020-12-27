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
#include "xmq_rapidxml.h"

using namespace std;
using namespace xmq;

int xmq::main_xmq2xml(Settings *settings)
{
    vector<char> *buffer = settings->in;
    rapidxml::xml_document<> doc;
    bool generate_html {};

    if (xmq_implementation::firstWordIsHtml(*buffer))
    {
        generate_html = true;
    }

    if (!settings->no_declaration)
    {
        if (generate_html)
        {
            rapidxml::xml_node<> *node = doc.allocate_node(rapidxml::node_doctype, "!DOCTYPE", "html");
            doc.append_node(node);
        }
        else
        {
            rapidxml::xml_node<> *node = doc.allocate_node(rapidxml::node_declaration, "?xml");
            doc.append_node(node);
            node->append_attribute(doc.allocate_attribute("version", "1.0"));
            node->append_attribute(doc.allocate_attribute("encoding", "UTF-8"));
        }
    }

    ParseActionsRapidXML pactions;
    pactions.doc = &doc;

    parseXMQ(&pactions, settings->filename.c_str(), &(*buffer)[0]);

    if (settings->view)
    {
        rapidxml::xml_node<> *node = &doc;
        node = node->first_node();
        if (node->type() == rapidxml::node_doctype || node->type() == rapidxml::node_declaration)
        {
            node = node->next_sibling();
        }
        RenderActionsRapidXML ractions;
        ractions.setRoot(node);
        renderXMQ(&ractions, settings);
    }
    else
    {
        string s;
        int flags = 0;
        if (settings->preserve_ws)
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
