/*
 Copyright (c) 2019 Fredrik Öhrström

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
#include <fstream>
#include <map>
#include <vector>
#include "rapidxml/rapidxml.hpp"

#include "util.h"

using namespace rapidxml;
using namespace std;

StringCount string_count_;
int num_prefixes_ {};
map<string,int> prefixes_;

size_t attr_max_width = 80;

bool needsEscaping(const char *s, bool is_attribute)
{
    size_t n = 0;
    if (s == NULL) return false;
    while (*s != 0)
    {
        if (*s == ' ') return true;
        if (*s == '=') return true;
        if (*s == '\\') return true;
        if (*s == '\n') return true;
        if (*s == '\'') return true;
        if (*s == '(') return true;
        if (*s == ')') return true;
        if (*s == '{') return true;
        if (*s == '}') return true;
        if (*s == '/') return true;
        s++;
        n++;
        if (is_attribute && n > attr_max_width) return true;
    }
    return false;
}

void printIndent(int i, bool newline=true)
{
    if (newline) printf("\n");
    while (--i >= 0) printf(" ");
}

size_t trimWhiteSpace(const char **datap)
{
    const char *data = *datap;
    size_t len = strlen(data);
    // Trim away whitespace at the beginning.
    while (len > 0 && *data != 0)
    {
        if (!isWhiteSpace(*data)) break;
        data++;
        len--;
    }

    // Trim away whitespace at the end.
    while (len >= 1)
    {
        if (!isWhiteSpace(data[len-1])) break;
        len--;
    }
    *datap = data;
    return len;
}

void printEscaped(const char *s, bool is_attribute, int indent, bool must_quote)
{
    if (!must_quote && !needsEscaping(s, is_attribute))
    {
        printf("%s", s);
    }
    else
    {
        size_t n = 0;
        printf("'");
        while (*s != 0)
        {
            switch (*s) {
                case '\\' : printf("\\\\"); break;
                case '\'' : printf("\\'"); break;
                case '\n' : printIndent(indent); break;
                default:    printf("%c", *s);
            }
            s++;
            n++;
            if (is_attribute && n > attr_max_width)
            {
                n -= attr_max_width;
                printf("'");
                printIndent(indent);
                printf("'");
            }
        }
        printf("'");
    }
}

bool nodeHasNoChildren(xml_node<> *node)
{
    return node->first_node() == NULL;
}

bool nodeHasSingleDataChild(xml_node<> *node,
                            const char **data)
{
    *data = NULL;
    xml_node<> *i = node->first_node();

    if (i != NULL &&
        i->type() == node_data &&
        i->next_sibling() == NULL)
    {
        *data = node->value();
         return true;
    }

    return false;
}

bool hasAttributes(xml_node<> *node)
{
    return node->first_attribute() != NULL;
}

void printAlign(int i)
{
    while (--i >= 0) printf(" ");
}

void printAlignedAttribute(xml_attribute<> *i,
                           const char *value,
                           int indent,
                           int align,
                           bool do_indent);

void printAttributes(xml_node<> *node,
                     int indent)
{
    if (node->first_attribute() == NULL) return;
    vector<pair<xml_attribute<>*,const char *>> lines;
    size_t align = 0;

    xml_attribute<> *i = node->first_attribute();
    while (i)
    {
        const char *key = i->name();
        const char *value;
        lines.push_back( { i, value });
        size_t len = strlen(key);
        if (len > align) {
            align = len;
        }
        i = i->next_attribute();
    }

    printf("(");
    i = node->first_attribute();
    bool do_indent = false;
    while (i)
    {
        const char *value = i->value();
        printAlignedAttribute(i, value, indent+strlen(node->name())+1, align, do_indent);
        do_indent = true;
        i = i->next_attribute();
    }
    printf(")");
}

void printAligned(xml_node<> *i,
                  const char *value,
                  int indent,
                  int align,
                  bool do_indent)
{
    if (do_indent) printIndent(indent);
    if (i->type() == node_comment)
    {
        size_t l = trimWhiteSpace(&value);
        printf("// %.*s", (int)l, value);
    }
    else
    if (i->type() == node_data)
    {
        printEscaped(value, false, indent, true);
    }
    else
    {
        const char *key = i->name();
        printf("%s", key);
        if (hasAttributes(i))
        {
            printAttributes(i, indent);
        }
        if (value != NULL) {
            size_t len = strlen(key);
            printAlign(align-len+1);
            printf("= ");
            printEscaped(value, false, indent+align+3, false);
        }
    }
}

void printAlignedAttribute(xml_attribute<> *i,
                           const char *value,
                           int indent,
                           int align,
                           bool do_indent)
{
    if (do_indent) printIndent(indent);
    const char *key = i->name();
    printf("%s", key);
    if (value != NULL) {
        size_t len = strlen(key);
        printAlign(align-len+1);
        printf("= ");
        printEscaped(value, true, indent+align+3, false);
    }
}

// Render is only invoked on nodes that have children nodes
// other than a single content node.
void render(xml_node<> *node, int indent, bool newline=true)
{
    assert(node != NULL);
    size_t align = 0;
    vector<pair<xml_node<>*,const char *>> lines;

    printIndent(indent, newline);
    printf("%s", node->name());
    if (hasAttributes(node))
    {
        printAttributes(node, indent);
        printIndent(indent);
        printf("{");
    }
    else
    {
        printf(" {");
    }
    xml_node<> *i = node->first_node();
    while (i)
    {
        const char *key = i->name();
        const char *value;

        if (i->type() == node_data || i->type() == node_comment)
        {
            lines.push_back( { i, i->value() });
        }
        else
        if (nodeHasNoChildren(i))
        {
            lines.push_back( { i, NULL });
        }
        else
        if (nodeHasSingleDataChild(i, &value))
        {
            lines.push_back( { i, value });
            size_t len = strlen(key);
            if (len > align) {
                align = len;
            }
        }
        else
        {
            // Flush any accumulated key:value lines with proper alignment.
            for (auto &p : lines)
            {
                printAligned(p.first, p.second, indent+4, align, true);
            }
            lines.clear();
            align = 0;
            render(i, indent+4);
        }
        i = i->next_sibling();
    }
    // Flush any accumulated key:value lines with proper alignment.
    for (auto &p : lines)
    {
        printAligned(p.first, p.second, indent+4, align, true);
    }
    printIndent(indent);
    printf("}");
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
            shiftLeft(i->name(), p.length()-3);
            i->name()[0] = '[';
            i->name()[1] = 48+pn;
            i->name()[2] = ']';
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
                shiftLeft(a->name(), p.length()-3);
                a->name()[0] = '[';
                a->name()[1] = 48+pn;
                a->name()[2] = ']';
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

#define VERSION "0.1"

int main_xml2xmq(int argc, char **argv)
{
    xml_document<> doc;
    //bool compress = false;
    if (argc < 2) {
        puts(manual);
        exit(0);
    }
    const char *file = argv[1];
    if (file == NULL) {
        fprintf(stderr, "You must supply an input file or a single - to read from stdin.\n");
        exit(1);
    }

    vector<char> buffer;

    if (!strcmp(file, "-"))
    {
        bool rc = loadStdin(&buffer);
        if (!rc)
        {
            exit(1);
        }
    }
    else
    {
        string files = string(file);
        bool rc = loadFile(files, &buffer);
        if (!rc)
        {
            exit(1);
        }
    }
    buffer.push_back('\0');

    doc.parse<parse_comment_nodes>(&buffer[0]);

    xml_node<> *root = doc.first_node();

    /*
    if (compress)
    {
        find_all_strings(root, string_count_);
        find_all_prefixes(root, string_count_);
    }

    for (auto &p : prefixes_)
    {
        printf("\n# [%d]=%s", p.second, p.first.c_str());
    }

    printf("\n");
    */
    render(root, 0, false);
    printf("\n");
    return 0;
}
