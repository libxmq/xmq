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
#include <fstream>
#include <map>
#include <set>
#include <stdarg.h>
#include <vector>
#include "rapidxml/rapidxml.hpp"

#include "util.h"
#include "xmq.h"

using namespace rapidxml;
using namespace std;

StringCount string_count_;
int num_prefixes_ {};
map<string,int> prefixes_;

size_t attr_max_width = 80;

Settings *settings_;

const char *green;
const char *yellow;
const char *light_blue;
const char *dark_blue;
const char *magenta;
const char *red;
const char *reset_color;

void render(xml_node<> *node, int indent, bool newline = true);

void useAnsiColors()
{
    green = "\033[0;32m";
    yellow = "\033[0;33m";
    light_blue = "\033[1;34m";
    dark_blue = "\033[0;34m";
    magenta = "\033[0;35m";
    red = "\033[0;31m";
    reset_color = "\033[0m";
}

void useHtmlColors()
{
    green = "<span style=\"color:#00aa00\">";
    yellow = "<span style=\"color:#888800\">";
    light_blue = "<span style=\"color:#aaaaff\">";
    dark_blue = "<span style=\"color:#000088\">";
    magenta = "<span style=\"color:#00aaaa\">";
    red = "<span style=\"color:#aa0000\">";
    reset_color = "</span>";
}


thread_local int (*output)(const char* fmt, ...);
thread_local int (*outputNoEscape)(const char* fmt, ...);
thread_local vector<char> *out_buffer;

int outputTextTerminal(const char *fmt, ...)
{
    va_list args;
    char buffer[65536];
    buffer[65535] = 0;
    va_start(args, fmt);
    vsnprintf(buffer, 65356, fmt, args);
    va_end(args);
    out_buffer->insert(out_buffer->end(), buffer, buffer+strlen(buffer));
    return 0;
}

bool escapeHtml(char c, string *escape)
{
    if (c  == '<')
    {
        *escape = "&lt;";
        return true;
    }
    if (c  == '>')
    {
        *escape = "&gt;";
        return true;
    }
    return false;
}

int outputTextHtml(const char *fmt, ...)
{
    va_list args;
    char buffer[65536];
    buffer[65535] = 0;
    va_start(args, fmt);
    vsnprintf(buffer, 65536, fmt, args);
    va_end(args);
    size_t len = strlen(buffer);
    for (size_t i=0; i<len; ++i)
    {
        string escape;
        if (!escapeHtml(buffer[i], &escape))
        {
            out_buffer->insert(out_buffer->end(), buffer[i]);
        }
        else
        {
            out_buffer->insert(out_buffer->end(), escape.begin(), escape.end());
        }
    }
    return 0;
}

void printTag(str tag)
{
    if (settings_->use_color) outputNoEscape(dark_blue);
    output("%.*s", tag.l, tag.s);
    if (settings_->use_color) outputNoEscape(reset_color);
}

void printKeyTag(str tag)
{
    if (settings_->use_color) outputNoEscape(green);
    output("%.*s", tag.l, tag.s);
    if (settings_->use_color) outputNoEscape(reset_color);
}

void printAttributeKey(str key)
{
    if (settings_->use_color) outputNoEscape(green);
    output("%.*s", key.l, key.s);
    if (settings_->use_color) outputNoEscape(reset_color);
}

// Returns the number of single quotes necessary
// to quote this string.
int escapingDepth(str value, bool *add_start_newline, bool *add_end_newline, bool is_attribute)
{
    size_t n = 0;
    if (value.l == 0) return 0; // No escaping neseccary.
    const char *s = value.s;
    const char *end = s+value.l;
    bool escape = false;
    int depth = 0;
    if (*s == '/' && *(s+1) == '/') escape = true;
    if (*s == '/' && *(s+1) == '*') escape = true;
    const char *q = NULL; // Start of most recently found single quote sequence.
    if (*s == '\'') *add_start_newline = true;
    while (s < end)
    {
        if (*s == '=' ||
            *s == '(' ||
            *s == ')' ||
            *s == '{' ||
            *s == '}' ||
            *s == ' ' ||
            *s == '\n' ||
            *s == '\r' ||
            *s == '\t')
        {
            escape = true;
        }
        else
        if (*s == '\'')
        {
            escape = true;
            if (q == NULL)
            {
                q = s;
                depth = 1;
            }
            else
            {
                int d = s-q+1;
                if (d > depth) depth = d;
            }
        }
        else
        {
            q = NULL;
        }
        s++;
        n++;
        if (is_attribute && n > attr_max_width)
        {
            escape = true;
        }
    }
    if (*(s-1) == '\'') *add_end_newline = true;

    if (escape)
    {
        if (depth == 0) depth = 1;
        if (depth == 2) depth = 3;
    }
    return depth;
}

bool containsNewlines(str value)
{
    if (value.l == 0) return false;
    const char *s = value.s;
    const char *end = s+value.l;
    while (s < end)
    {
        if (*s == '\n') return true;
        if (*s == '\r') return true;
        s++;
    }
    return false;
}

void printIndent(int i, bool newline=true)
{
    if (newline) output("\n");
    while (--i >= 0) output(" ");
}

size_t trimWhiteSpace(str *v)
{
    const char *data = v->s;
    size_t len = v->l;
    if (len == 0) len = strlen(v->s)+1;

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
    v->s = data;
    v->l = len;
    return len;
}

void printComment(str comment, int indent)
{
    const char *c = comment.s;
    size_t len = comment.l;
    bool single_line = true;

    for (size_t i=0; i<len; ++i)
    {
        if (c[i] == '\n') single_line = false;
    }
    if (single_line)
    {
        if (settings_->use_color) outputNoEscape(yellow);
        output("// %.*s", len, c);
        if (settings_->use_color) outputNoEscape(reset_color);
        return;
    }
    const char *p = c;
    int prev_i = 0;
    for (size_t i=0; i<len; ++i)
    {
        if (c[i] == '\n' || i == len-1)
        {
            int n = i - prev_i;
            if (p == c)
            {
                if (settings_->use_color) outputNoEscape(yellow);
                output("/* %.*s", n, p);
            }
            else if (i == len-1)
            {
                printIndent(indent);
                str pp(p, n);
                int nn = trimWhiteSpace(&pp);
                if (settings_->use_color) outputNoEscape(yellow);
                output("   %.*s */", (int)nn, pp);
            }
            else
            {
                printIndent(indent);
                str pp(p, n);
                size_t nn = trimWhiteSpace(&pp);
                if (settings_->use_color) outputNoEscape(yellow);
                output("   %.*s", (int)nn, pp);
            }
            if (settings_->use_color) outputNoEscape(reset_color);
            p = c+i+1;
            prev_i = i+1;
        }
    }

}

void printEscaped(str value, bool is_attribute, int indent, bool must_quote)
{
    const char *s = value.s;
    const char *end = s+value.l;

    if (s[0] == 0)
    {
        // Generate the empty '' string.
        must_quote = true;
    }
    bool add_start_newline = false;
    bool add_end_newline = false;

    // Check how many (if any) single quotes are needed to protect the content properly.
    int escape_depth = escapingDepth(value, &add_start_newline, &add_end_newline, is_attribute);

    if (escape_depth > 0)
    {
        must_quote = true;
    }

    if (must_quote && escape_depth == 0)
    {
        escape_depth = 1;
    }

    if (!must_quote)
    {
        // There are no single quotes inside the content s.
        // We can safely print it.
        if (settings_->use_color) outputNoEscape(red);
        output("%.*s", value.l, value.s);
        if (settings_->use_color) outputNoEscape(reset_color);
    }
    else
    {
        size_t n = 0;
        if (settings_->use_color) outputNoEscape(red);
        for (int i=0; i<escape_depth; ++i)
        {
            output("'");
        }
        if (add_start_newline)
        {
            printIndent(indent+escape_depth);
        }
        while (s < end)
        {
            switch (*s) {
                case '\n' :
                    printIndent(indent+escape_depth);
                    n = 0;
                    if (settings_->use_color) outputNoEscape(red);
                    break;
                default:    output("%c", *s);
            }
            s++;
            n++;
            if (is_attribute && n > attr_max_width)
            {
                n = 0;
                output("'");
                printIndent(indent);
                if (settings_->use_color) outputNoEscape(red);
                output("'");
            }
        }
        if (add_end_newline)
        {
            printIndent(indent+escape_depth);
        }
        for (int i=0; i<escape_depth; ++i)
        {
            output("'");
        }
        if (settings_->use_color) outputNoEscape(reset_color);
    }
}

/*
    Test if the node has no children.
*/
bool nodeHasNoChildren(xml_node<> *node)
{
    return node->first_node() == NULL;
}

/*
    Test if the node has a single data child.
    Such nodes should be rendered as node = data
*/
bool nodeHasSingleDataChild(xml_node<> *node,
                            str *data)
{
    data->s = "";
    data->l = 0;
    xml_node<> *i = node->first_node();

    if (i != NULL &&
        i->type() == node_data &&
        i->next_sibling() == NULL)
    {
        data->s = i->value();
        data->l = i->value_size();
         return true;
    }

    return false;
}

/*
    Test if the node has attributes.
    Such nodes should be rendered as node(...)
*/
bool hasAttributes(xml_node<> *node)
{
    return node->first_attribute() != NULL;
}

void printAlign(int i)
{
    while (--i >= 0) output(" ");
}

void printAlignedAttribute(xml_attribute<> *i,
                           str value,
                           int indent,
                           int align,
                           bool do_indent);

void printAttributes(xml_node<> *node,
                     int indent)
{
    if (node->first_attribute() == NULL) return;
    vector<pair<xml_attribute<>*,str>> lines;
    size_t align = 0;

    xml_attribute<> *i = node->first_attribute();

    i = node->first_attribute();
    while (i)
    {
        string key = string(i->name(), i->name_size());
        string checka = string("@")+key;
        string checkb = string(node->name())+"@"+key;
        if (settings_->excludes.count(checka) == 0 &&
            settings_->excludes.count(checkb) == 0)
        {
            lines.push_back( { i, str("",0) });
            size_t len = key.size();
            if (len > align) {
                align = len;
            }
        }
        i = i->next_attribute();
    }

    output("(");
    bool do_indent = false;

    i = node->first_attribute();
    while (i)
    {
        string key = string(i->name(), i->name_size());
        str value = str(i->value(), i->value_size());

        string checka = string("@")+key;
        string checkb = string(node->name())+"@"+key;
        if (settings_->excludes.count(checka) == 0 &&
            settings_->excludes.count(checkb) == 0)
        {
            printAlignedAttribute(i, value, indent+node->name_size()+1, align, do_indent);
            do_indent = true;
        }
        i = i->next_attribute();
    }
    output(")");
}

void printAligned(xml_node<> *i,
                  str value,
                  int indent,
                  int align,
                  bool do_indent)
{
    if (do_indent) printIndent(indent);
    if (i->type() == node_comment)
    {
        trimWhiteSpace(&value);
        printComment(value, indent);
    }
    else
    if (i->type() == node_data)
    {
        printEscaped(value, false, indent, true);
    }
    else
    if (i->type() == node_cdata)
    {
        // CData becomes just quoted content. The cdata node is not preserved.
        str cdata(i->value(), i->value_size());
        printEscaped(cdata, false, indent, true);
    }
    else
    {
        str key(i->name(), i->name_size());
        printKeyTag(key);
        if (hasAttributes(i))
        {
            printAttributes(i, indent);
        }
        if (value.l != 0)
        {
            size_t len = key.l;
            printAlign(align-len+1);
            int ind = indent+align+3;
            if (containsNewlines(value))
            {
                output("=");
                ind = indent;
                printIndent(indent);
            }
            else
            {
                output("= ");
            }
            printEscaped(value, false, ind, false);
        }
    }
}

void printAlignedAttribute(xml_attribute<> *i,
                           str value,
                           int indent,
                           int align,
                           bool do_indent)
{
    if (do_indent) printIndent(indent);
    str key(i->name(), i->name_size());
    printAttributeKey(key);

    // Print the value if it exists, and is different
    // from the key. I.e. boolean xml values must be stored as:
    // hidden="hidden" this will translate into just hidden in xmq.
    if (value.l > 0 && !value.equals(key))
    {
        size_t len = key.l;
        printAlign(align-len+1);
        int ind = indent+align+3;
        if (containsNewlines(value))
        {
            output("=");
            ind = indent+4;
            printIndent(ind);
        }
        else
        {
            output("= ");
        }
        printEscaped(value, false, ind, false);
    }
}

void renderNode(xml_node<> *i, int indent, bool newline, vector<pair<xml_node<>*,str>> *lines, size_t *align)
{
    string key = string(i->name(), i->name_size());
    str value = str(i->value(), i->value_size());
    if (i->type() == node_data || i->type() == node_comment)
    {
        lines->push_back( { i, value });
    }
    else
    if (nodeHasNoChildren(i))
    {
        lines->push_back( { i, str("",0) });
    }
    else
    if (nodeHasSingleDataChild(i, &value))
    {
        lines->push_back( { i, value });
        size_t len = key.size();
        if (len > *align)
        {
            *align = len;
        }
    }
    else
    {
        // Flush any accumulated key:value lines with proper alignment.
        for (auto &p : *lines)
        {
            printAligned(p.first, p.second, indent+4, *align, true);
        }
        lines->clear();
        *align = 0;
        render(i, indent+4);
    }
}

/*
    Render is only invoked on nodes that have children nodes
    other than a single content node.
*/
void render(xml_node<> *node, int indent, bool newline)
{
    assert(node != NULL);
    size_t align = 0;
    vector<pair<xml_node<>*,str>> lines;

    if (node->type() == node_comment)
    {
        str value(node->value(), node->value_size());
        printAligned(node, value, indent, 0, newline);
        return;
    }
    printIndent(indent, newline);
    str name(node->name(), node->name_size());
    printTag(name);
    if (hasAttributes(node))
    {
        printAttributes(node, indent);
        printIndent(indent);
        output("{");
    }
    else
    {
        output(" {");
    }
    xml_node<> *i = node->first_node();
    while (i)
    {
        renderNode(i, indent, newline, &lines, &align);
        i = i->next_sibling();
    }
    // Flush any accumulated key:value lines with proper alignment.
    for (auto &p : lines)
    {
        printAligned(p.first, p.second, indent+4, align, true);
    }
    printIndent(indent);
    output("}");
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

const char *findStartingNewline(const char *where, const char *start)
{
    while (where > start && *(where-1) != '\n') where--;
    return where;
}


const char *findEndingNewline(const char *where)
{
    while (*where && *where != '\n') where++;
    return where;
}

void findLineAndColumn(const char *from, const char *where, int *line, int *col)
{
    *line = 1;
    *col = 1;
    for (const char *i = from; *i != 0; ++i)
    {
        (*col)++;
        if (*i == '\n')
        {
            (*line)++;
            (*col)=1;
        }
        if (i == where) break;
    }
}

void renderDoc(xml_node<> *root, Settings *provided_settings)
{
    settings_ = provided_settings;
    if (provided_settings->out == NULL)
    {
        output = printf;
        assert(0);
    }
    else
    {
        out_buffer = provided_settings->out;
        if (provided_settings->output == OutputType::html)
        {
            output = outputTextHtml;
        }
        else
        {
            output = outputTextTerminal;
        }
    }

    outputNoEscape = outputTextTerminal;

    if (provided_settings->use_color)
    {
        if (provided_settings->output == OutputType::terminal)
        {
            useAnsiColors();
        }
        if (provided_settings->output == OutputType::html)
        {
            useHtmlColors();
        }
    }
    // Xml usually only have a single root data node,
    // but xml with comments can have multiple root
    // nodes where some are comment nodes.
    bool newline = false;
    while (root != NULL)
    {
        if (root->type() == node_doctype)
        {
            // Do not print the doctype.
            // This is assumed to be <!DOCTYPE html>
            if (strncmp(root->value(), "html", 4))
            {
                fprintf(stderr, "Warning! Unexpected doctype %s\n", root->value());
            }
            root = root->next_sibling();
            continue;
        }
        str tmp;
        // Handle the special cases, single empty node and single node with data content.
        if (nodeHasSingleDataChild(root, &tmp) || nodeHasNoChildren(root))
        {
            vector<pair<xml_node<>*,str>> lines;
            size_t align = 0;
            renderNode(root, 0, false, &lines, &align);
            // Flush any accumulated key:value lines with proper alignment.
            for (auto &p : lines)
            {
                printAligned(p.first, p.second, 0, align, false);
            }
        }
        else
        {
            render(root, 0, newline);
        }
        newline = true;
        if (root->parent())
        {
            root = root->next_sibling();
        }
        else
        {
            break;
        }
    }

    output("\n");
}

#define VERSION "0.1"

int main_xml2xmq(Settings *provided_settings)
{
    // Parsing is destructive, store a copy for error messages here.
    vector<char> original = *provided_settings->in;

    vector<char> *buffer = provided_settings->in;
    settings_ = provided_settings;
    xml_document<> doc;
    try
    {
        // This looks kind of silly, doesnt it? Is there another way to configure the rapidxml parser?
        if (provided_settings->preserve_ws)
        {
            if (provided_settings->html)
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
            if (provided_settings->html)
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
        const char *from = findStartingNewline(where, &(*buffer)[0]);
        const char *to = findEndingNewline(where);
        int line, col;
        findLineAndColumn(&(*buffer)[0], where, &line, &col);

        size_t count = to-from;

        // bad.xml:2:16 Parse error expected =
        //     <block clean>
        //                 ^

        fprintf(stderr, "%s:%d:%d Parse error %s\n%.*s\n",
                provided_settings->filename.c_str(), line, col, pe.what(), (int)count, from);
        for (int i=2; i<col; ++i) fprintf(stderr, " ");
        fprintf(stderr, "^\n");
    }
    xml_node<> *root = doc.first_node();

    if (settings_->compress)
    {
        // This will find common prefixes.
        find_all_strings(root, string_count_);
        find_all_prefixes(root, string_count_);

        for (auto &p : prefixes_)
        {
            output("# %d=%s\n", p.second, p.first.c_str());
        }
    }

    renderDoc(root, provided_settings);
    return 0;
}
