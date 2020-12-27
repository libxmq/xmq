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

#include "cmdline.h"
#include "util.h"
#include "xmq.h"
#include "xmq_implementation.h"
#include "xmq_rapidxml.h"

#include "rapidxml/rapidxml.hpp"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <set>
#define VERSION "0.1"

using namespace std;
using namespace rapidxml;

bool detectTreeType(xmq::Settings *settings);
int xml2xmq(xmq::Settings *settings);

int main(int argc, char **argv)
{
    vector<char> in;
    vector<char> out;

    xmq::Settings settings(&in, &out);

    if (isatty(1))
    {
        settings.use_color = true;
        settings.output = xmq::RenderType::terminal;
    }

    parseCommandLine(&settings, argc, argv);

    bool is_xmq = detectTreeType(&settings);
    int rc = 0;

    if (is_xmq)
    {
        rc = main_xmq2xml(&settings);
    }
    else
    {
        rc = xml2xmq(&settings);
    }

    if (rc == 0)
    {
        out.push_back('\0');
        printf("%s", &out[0]);
    }

    return rc;
}

bool detectTreeType(xmq::Settings *settings)
{
    bool is_xmq = false == xmq_implementation::startsWithLessThan(*settings->in);

    if (settings->tree_type == xmq::TreeType::auto_detect)
    {
        settings->tree_type = xmq::TreeType::xml;
        if (xmq_implementation::isHtml(*settings->in))
        {
            settings->tree_type = xmq::TreeType::html;
        }
    }

    return is_xmq;
}

StringCount string_count_;
int num_prefixes_ {};
map<string,int> prefixes_;

void shiftLeft(char *s, size_t l)
{
    size_t len = strlen(s);
    assert(l <= len);
    size_t newlen = len - l;
    for (size_t i=0; i<newlen; ++i)
    {
        s[i] = s[i+l];
    }
    s[newlen] = 0;
}

void find_all_strings(xml_node<> *i, StringCount &c)
{
    if (i->type() == node_element)
    {
        add_string(i->name(), c);
        xml_attribute<> *a = i->first_attribute();
        while (a != NULL)
        {
            add_string(a->name(), c);
            a = a->next_attribute();
        }
        xml_node<> *n = i->first_node();
        while (n != NULL)
        {
            find_all_strings(n, c);
            n = n->next_sibling();
        }
    }
}

void find_all_prefixes(xml_node<> *i, StringCount &c)
{
    if (i->type() == node_element)
    {
        string p = find_prefix(i->name(), c);
        if (p.length() > 5)
        {
            int pn = 0;
            if (prefixes_.count(p) > 0)
            {
                pn = prefixes_[p];
            }
            else
            {
                pn = num_prefixes_;
                prefixes_[p] = pn;
                num_prefixes_++;
            }
            shiftLeft(i->name(), p.length()-2);
            i->name()[0] = 48+pn;
            i->name()[1] = ':';
        }
        xml_attribute<> *a = i->first_attribute();
        while (a != NULL)
        {
            string p = find_prefix(a->name(), c);
            if (p.length() > 5)
            {
                int pn = 0;
                if (prefixes_.count(p) > 0)
                {
                    pn = prefixes_[p];
                }
                else
                {
                    pn = num_prefixes_;
                    prefixes_[p] = pn;
                    num_prefixes_++;
                }
                shiftLeft(a->name(), p.length()-2);
                a->name()[0] = 48+pn;
                a->name()[1] = ':';
            }

            a = a->next_attribute();
        }
        xml_node<> *n = i->first_node();
        while (n != NULL)
        {
            find_all_prefixes(n, c);
            n = n->next_sibling();
        }
    }
}

int xml2xmq(xmq::Settings *settings)
{
    vector<char> *buffer = settings->in;
    xml_document<> doc;
    try
    {
        // This looks kind of silly, doesnt it? Is there another way to configure the rapidxml parser?
        if (settings->preserve_ws)
        {
            if (settings->tree_type == xmq::TreeType::html)
            {
                doc.parse<parse_void_elements|parse_doctype_node|parse_comment_nodes|parse_no_string_terminators>(&(*buffer)[0]);
            }
            else
            {
                doc.parse<parse_doctype_node|parse_comment_nodes|parse_no_string_terminators>(&(*buffer)[0]);
            }
        }
        else
        {
            if (settings->tree_type == xmq::TreeType::html)
            {
                doc.parse<parse_void_elements|parse_doctype_node|parse_comment_nodes|parse_trim_whitespace|parse_no_string_terminators>(&(*buffer)[0]);
            }
            else
            {
                doc.parse<parse_doctype_node|parse_comment_nodes|parse_trim_whitespace|parse_no_string_terminators>(&(*buffer)[0]);
            }
        }
    }
    catch (rapidxml::parse_error pe)
    {
        const char *where = pe.where<const char>();
        //size_t offset = where - &(*buffer)[0];
        const char *from = xmq_implementation::findStartingNewline(where, &(*buffer)[0]);
        const char *to = xmq_implementation::findEndingNewline(where);
        int line, col;
        xmq_implementation::findLineAndColumn(&(*buffer)[0], where, &line, &col);

        size_t count = to-from;

        // bad.xml:2:16 Parse error expected =
        //     <block clean>
        //                 ^

        fprintf(stderr, "%s:%d:%d Parse error %s\n%.*s\n",
                settings->filename.c_str(), line, col, pe.what(), (int)count, from);
        for (int i=2; i<col; ++i) fprintf(stderr, " ");
        fprintf(stderr, "^\n");
    }
    xml_node<> *root = doc.first_node();

    if (settings->compress)
    {
        // This will find common prefixes.
        find_all_strings(root, string_count_);
        find_all_prefixes(root, string_count_);

        for (auto &p : prefixes_)
        {
            printf("# %d=%s\n", p.second, p.first.c_str());
        }
    }

    RenderActionsRapidXML ractions;
    ractions.setRoot(root);
    xmq::renderXMQ(&ractions, settings);
    return 0;
}
