/*
 Copyright (c) 2019-2021 Fredrik Öhrström

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
#include "rapidxml/rapidxml_print.hpp"

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <set>

using namespace std;

bool detectTreeType(CmdLineOptions *options);
int xml2xmq(CmdLineOptions *options);
int xmq2xml(CmdLineOptions *options);

int main(int argc, char **argv)
{
    vector<char> in;
    vector<char> out;

    CmdLineOptions options(&in, &out);

    if (isatty(1))
    {
        options.use_color = true;
        options.output = xmq::RenderType::terminal;
    }

    parseCommandLine(&options, argc, argv);

    bool is_xmq = detectTreeType(&options);
    int rc = 0;

    if (is_xmq)
    {
        rc = xmq2xml(&options);
    }
    else
    {
        rc = xml2xmq(&options);
    }

    if (rc == 0)
    {
        out.push_back('\0');
        printf("%s", &out[0]);
    }

    return rc;
}

bool detectTreeType(CmdLineOptions *options)
{
    bool is_xmq = false == xmq_implementation::startsWithLessThan(*options->in);

    if (is_xmq)
    {
        if (options->tree_type == xmq::TreeType::auto_detect)
        {
            options->tree_type = xmq::TreeType::xml;
            if (xmq_implementation::firstWordIsHtml(*options->in))
            {
                options->tree_type = xmq::TreeType::html;
            }
        }
    }
    else
    {
        if (options->tree_type == xmq::TreeType::auto_detect)
        {
            options->tree_type = xmq::TreeType::xml;
            if (xmq_implementation::isHtml(*options->in))
            {
                options->tree_type = xmq::TreeType::html;
            }
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

void find_all_strings(rapidxml::xml_node<> *i, StringCount &c)
{
    if (i->type() == rapidxml::node_element)
    {
        add_string(i->name(), c);
        rapidxml::xml_attribute<> *a = i->first_attribute();
        while (a != NULL)
        {
            add_string(a->name(), c);
            a = a->next_attribute();
        }
        rapidxml::xml_node<> *n = i->first_node();
        while (n != NULL)
        {
            find_all_strings(n, c);
            n = n->next_sibling();
        }
    }
}

void find_all_prefixes(rapidxml::xml_node<> *i, StringCount &c)
{
    if (i->type() == rapidxml::node_element)
    {
        fprintf(stderr, "A1\n");
        string p = find_prefix(i->name(), c);
        if (p.length() > 5)
        {
            fprintf(stderr, "A2\n");
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
        rapidxml::xml_attribute<> *a = i->first_attribute();
        while (a != NULL)
        {
            fprintf(stderr, "x1\n");
            string p = find_prefix(a->name(), c);
            fprintf(stderr, "x2\n");
            if (p.length() > 5)
            {
                fprintf(stderr, "x3\n");
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
            fprintf(stderr, "x4\n");
        }
        rapidxml::xml_node<> *n = i->first_node();
        while (n != NULL)
        {
            fprintf(stderr, "Na\n");
            find_all_prefixes(n, c);
            fprintf(stderr, "Nb\n");
            n = n->next_sibling();
            fprintf(stderr, "Nc\n");
        }
    }
}

int xml2xmq(CmdLineOptions *options)
{
    vector<char> *buffer = options->in;

    xmq::Document ddoc;
    xmq::Config s;
    parseXML(&ddoc, "", &(*buffer)[0], s);

    rapidxml::xml_document<> doc;
    try
    {
        int flags =
            rapidxml::parse_doctype_node |
            rapidxml::parse_pi_nodes |
            rapidxml::parse_comment_nodes |
            rapidxml::parse_no_string_terminators;

        if (!options->preserve_ws)
        {
            flags |= rapidxml::parse_trim_whitespace;
        }
        if (options->tree_type == xmq::TreeType::html)
        {
            flags |= rapidxml::parse_void_elements;
        }
        doc.parse(&(*buffer)[0], flags);

    }
    catch (rapidxml::parse_error &pe)
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
                options->filename.c_str(), line, col, pe.what(), (int)count, from);
        for (int i=2; i<col; ++i) fprintf(stderr, " ");
        fprintf(stderr, "^\n");
    }
    rapidxml::xml_node<> *root = doc.first_node();

    if (options->compress)
    {
        // This will find common prefixes.
        fprintf(stderr, "UGKRA1\n");
        find_all_strings(root, string_count_);
        fprintf(stderr, "UGKRA2\n");
        find_all_prefixes(root, string_count_);
        fprintf(stderr, "UGKRA3\n");

        for (auto &p : prefixes_)
        {
            printf("# %d=%s\n", p.second, p.first.c_str());
        }
    }

    RenderActionsRapidXML ractions(root);
    xmq::Config config;
    config.render_type = options->output;
    config.use_color = options->use_color;
    config.sort_attributes = options->sort_attributes;
    xmq::renderXMQ(&ractions, options->out, config);
    return 0;
}

int xmq2xml(CmdLineOptions *options)
{
    vector<char> *buffer = options->in;
    rapidxml::xml_document<> doc;

    // Check its valid utf8.
//    int line, col;
    /*
    if (!isValidUtf8(buffer, &line, &col))
    {
        fprintf(stderr, "%s:%d:%d Invalid UTF8!\n",
                options->filename.c_str(), line, col);
        return 1;
        }*/

    // Change any \r\n to \n.
    removeCrs(buffer);

    if (!options->no_declaration)
    {
        if (options->tree_type == xmq::TreeType::html)
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

    ParseActionsRapidXML pactions(&doc);

    xmq::Config config;
    config.root = options->root.c_str();
    config.render_type = options->output;
    config.use_color = options->use_color;
    parseXMQ(&pactions, options->filename.c_str(), &(*buffer)[0], config);

    if (options->view)
    {
        RenderActionsRapidXML ractions(doc.first_node());
        renderXMQ(&ractions, options->out, config);
    }
    else
    {
        string s;
        int flags = 0;
        if (options->tree_type == xmq::TreeType::html)
        {
            flags |= rapidxml::print_html;
            // Html generation defaults to no pretty printing.
            if (!options->pp)
            {
                // Force pretty printing.
                flags |= rapidxml::print_no_indenting;
            }
        }
        else
        {
            // Xml generation defaults to pretty printing.
            if (options->no_pp)
            {
                // Force disable of pretty printing.
                flags |= rapidxml::print_no_indenting;
            }
        }
        print(back_inserter(s), doc, flags);
        options->out->insert(options->out->end(), s.begin(), s.end());
    }

    return 0;
}
