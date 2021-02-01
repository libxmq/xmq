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

#include <assert.h>
#include <stdarg.h>

#include "xmq.h"
#include "xmq_implementation.h"

using namespace std;

struct RenderImplementation
{
    RenderImplementation(xmq::RenderActions *ra,
                         xmq::RenderType rt,
                         bool use_color,
                         std::vector<char> *out) : out_buffer(out), render_type_(rt), use_color_(use_color), actions(ra) {}
    void render();

    std::vector<char> *out_buffer;
    xmq::RenderType render_type_ {};
    bool use_color_ {};
    const char *element_name_color; // blue
    const char *element_name_sugar_color; // green
    const char *attribute_name_sugar_color; // green
    const char *comment_color; // yellow
    const char *data_color; // red
    const char *reset_color;

    xmq::RenderActions *actions {};

    int output(const char* fmt, ...);
    int outputNoEscape(const char* fmt, ...);
    void useAnsiColors();
    void useHtmlColors();
    bool escapeHtml(char c, string *escape);
    void renderElementName(xmq::str name);
    void renderElementNameSugar(xmq::str tag);
    void renderElementNameSugarPI(xmq::str tag);
    void renderElementNameSugarDT();
    void printAttributeKey(xmq::str key);
    bool containsNewlines(xmq::str value);
    void printIndent(int i, bool newline=true);
    size_t trimWhiteSpace(xmq::str *v);
    void printComment(xmq::str comment, int indent);
    void printEscaped(xmq::str value, bool is_attribute, int indent, bool must_quote);
    bool nodeHasNoChildren(void *node);
    bool nodeHasSingleDataChild(void *node, xmq::str *data);
    void printAlign(int i);
    void printAttributes(void *node, int indent);
    void printAlignedAttribute(void *i,
                               xmq::str value,
                               int indent,
                               int align,
                               bool do_indent);
    void printAligned(void *i,
                      xmq::str value,
                      int indent,
                      int align,
                      bool do_indent);
    void renderNode(void *i, int indent, bool newline, vector<pair<void*,xmq::str>> *lines, size_t *align);
    void render(void *node, int indent, bool newline = true);
};


void RenderImplementation::useAnsiColors()
{
    element_name_color = "\033[0;34m";
    element_name_sugar_color = "\033[0;32m";
    attribute_name_sugar_color = "\033[0;32m";
    comment_color = "\033[0;33m";
    data_color = "\033[0;31m";
    reset_color = "\033[0m";
}

void RenderImplementation::useHtmlColors()
{
    element_name_color = "<span style=\"color:#000088\">";
    element_name_sugar_color = "<span style=\"color:#00aa00\">";
    attribute_name_sugar_color = "<span style=\"color:#00aa00\">";
    comment_color = "<span style=\"color:#888800\">";
    data_color = "<span style=\"color:#aa0000\">";
    reset_color = "</span>";
}

bool RenderImplementation::escapeHtml(char c, string *escape)
{
    if (c  == '&')
    {
        *escape = "&amp;";
        return true;
    }
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

void RenderImplementation::renderElementName(xmq::str name)
{
    if (use_color_) outputNoEscape(element_name_color);
    output("%.*s", name.l, name.s);
    if (use_color_) outputNoEscape(reset_color);
}

void RenderImplementation::renderElementNameSugar(xmq::str tag)
{
    if (use_color_) outputNoEscape(element_name_sugar_color);
    output("%.*s", tag.l, tag.s);
    if (use_color_) outputNoEscape(reset_color);
}

void RenderImplementation::renderElementNameSugarPI(xmq::str tag)
{
    if (use_color_) outputNoEscape(element_name_sugar_color);
    output("?%.*s?", tag.l, tag.s);
    if (use_color_) outputNoEscape(reset_color);
}

void RenderImplementation::renderElementNameSugarDT()
{
    if (use_color_) outputNoEscape(element_name_sugar_color);
    output("!DOCTYPE");
    if (use_color_) outputNoEscape(reset_color);
}

void RenderImplementation::printAttributeKey(xmq::str key)
{
    if (use_color_) outputNoEscape(attribute_name_sugar_color);
    output("%.*s", key.l, key.s);
    if (use_color_) outputNoEscape(reset_color);
}

bool RenderImplementation::containsNewlines(xmq::str value)
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

void RenderImplementation::printIndent(int i, bool newline)
{
    if (newline) output("\n");
    while (--i >= 0) output(" ");
}

size_t RenderImplementation::trimWhiteSpace(xmq::str *v)
{
    const char *data = v->s;
    size_t len = v->l;
    if (len == 0) len = strlen(v->s)+1;

    // Trim away whitespace at the beginning.
    while (len > 0 && *data != 0)
    {
        if (!xmq_implementation::isWhiteSpace(*data)) break;
        data++;
        len--;
    }

    // Trim away whitespace at the end.
    while (len >= 1)
    {
        if (!xmq_implementation::isWhiteSpace(data[len-1])) break;
        len--;
    }
    v->s = data;
    v->l = len;
    return len;
}

void RenderImplementation::printComment(xmq::str comment, int indent)
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
        if (use_color_) outputNoEscape(comment_color);
        output("// %.*s", len, c);
        if (use_color_) outputNoEscape(reset_color);
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
                if (use_color_) outputNoEscape(comment_color);
                output("/* %.*s", n, p);
            }
            else if (i == len-1)
            {
                printIndent(indent);
                xmq::str pp(p, n);
                int nn = trimWhiteSpace(&pp);
                if (use_color_) outputNoEscape(comment_color);
                output("   %.*s */", (int)nn, pp);
            }
            else
            {
                printIndent(indent);
                xmq::str pp(p, n);
                size_t nn = trimWhiteSpace(&pp);
                if (use_color_) outputNoEscape(comment_color);
                output("   %.*s", (int)nn, pp);
            }
            if (use_color_) outputNoEscape(reset_color);
            p = c+i+1;
            prev_i = i+1;
        }
    }

}

void RenderImplementation::printEscaped(xmq::str value, bool is_attribute, int indent, bool must_quote)
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
    int escape_depth = xmq_implementation::escapingDepth(value, &add_start_newline, &add_end_newline, is_attribute);

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
        if (use_color_) outputNoEscape(data_color);
        output("%.*s", value.l, value.s);
        if (use_color_) outputNoEscape(reset_color);
    }
    else
    {
        size_t n = 0;
        if (use_color_) outputNoEscape(data_color);
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
                    if (use_color_) outputNoEscape(data_color);
                    break;
                default:    output("%c", *s);
            }
            s++;
            n++;
            if (is_attribute && n > 80)
            {
                n = 0;
                output("'");
                printIndent(indent);
                if (use_color_) outputNoEscape(data_color);
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
        if (use_color_) outputNoEscape(reset_color);
    }
}

/*
    Test if the node has no children.
*/
bool RenderImplementation::nodeHasNoChildren(void *node)
{
    return actions->firstNode(node) == NULL;
}

/*
    Test if the node has a single data child.
    Such nodes should be rendered as node = data
*/
bool RenderImplementation::nodeHasSingleDataChild(void *node,
                                                  xmq::str *data)
{
    data->s = "";
    data->l = 0;
    void *i = actions->firstNode(node);

    if (i != NULL &&
        actions->isNodeData(i) &&
        actions->nextSibling(i) == NULL)
    {
        actions->loadValue(i, data);
        return true;
    }

    return false;
}

void RenderImplementation::printAlign(int i)
{
    while (--i >= 0) output(" ");
}

void RenderImplementation::printAttributes(void *node,
                                           int indent)
{
    if (!actions->hasAttributes(node)) return;
    size_t align = 0;

    xmq::str node_name;
    actions->loadName(node, &node_name);

    void *i = actions->firstAttribute(node);

    while (i)
    {
        /*
        string key = string(i->name(), i->name_size());
        string checka = string("@")+key;
        string checkb = string(node->name())+"@"+key;
        if (settings_->excludes.count(checka) == 0 &&
            settings_->excludes.count(checkb) == 0)
            {*/
        xmq::str name;
        actions->loadName(i, &name);
        if (name.l > align)
        {
            align = name.l;
        }
        i = actions->nextAttribute(i);
    }

    output("(");
    bool do_indent = false;

    i = actions->firstAttribute(node);
    while (i)
    {
        xmq::str key;
        actions->loadName(i, &key);
        xmq::str value;
        actions->loadValue(i, &value);

        string checka = string("@")+key.to_str();
        string checkb = node_name.to_str()+"@"+key.to_str();
        if (settings_->excludes.count(checka) == 0 &&
            settings_->excludes.count(checkb) == 0)
        {
            printAlignedAttribute(i, value, indent+node_name.l+1, align, do_indent);
            do_indent = true;
        }
        i = actions->nextAttribute(i);
    }
    output(")");
}

void RenderImplementation::printAligned(void *i,
                                        xmq::str value,
                                        int indent,
                                        int align,
                                        bool do_indent)
{
    if (do_indent) printIndent(indent);
    if (actions->isNodeComment(i))
    {
        trimWhiteSpace(&value);
        printComment(value, indent);
    }
    else
    if (actions->isNodeData(i))
    {
        printEscaped(value, false, indent, true);
    }
    else
    if (actions->isNodeCData(i))
    {
        // CData becomes just quoted content. The cdata node is not preserved.
        xmq::str cdata;
        actions->loadValue(i, &cdata);
        printEscaped(cdata, false, indent, true);
    }
    else
    if (actions->isNodeDocType(i))
    {
        renderElementNameSugarDT();
        xmq::str pi_data;
        actions->loadValue(i, &pi_data);
        output(" = ");
        printEscaped(pi_data, false, indent, false);
    }
    else
    if (actions->isNodePI(i))
    {
        xmq::str key;
        actions->loadName(i, &key);
        renderElementNameSugarPI(key);
        xmq::str pi_data;
        actions->loadValue(i, &pi_data);
        if (pi_data.l > 0)
        {
            output(" = ");
            printEscaped(pi_data, false, indent, false);
        }
    }
    else
    {
        xmq::str key;
        actions->loadName(i, &key);
        renderElementNameSugar(key);
        if (actions->hasAttributes(i))
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

void RenderImplementation::printAlignedAttribute(void *i,
                                                 xmq::str value,
                                                 int indent,
                                                 int align,
                                                 bool do_indent)
{
    if (do_indent) printIndent(indent);
    xmq::str key;
    actions->loadName(i, &key);
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

void RenderImplementation::renderNode(void *i, int indent, bool newline, vector<pair<void*,xmq::str>> *lines, size_t *align)
{
    xmq::str key;
    actions->loadName(i, &key);
    xmq::str value;
    actions->loadValue(i, &value);
    if (actions->isNodeData(i) || actions->isNodeComment(i) || actions->isNodePI(i) || actions->isNodeDocType(i))
    {
        lines->push_back( { i, value });
    }
    else
    if (nodeHasNoChildren(i))
    {
        lines->push_back( { i, xmq::str("",0) });
    }
    else
    if (nodeHasSingleDataChild(i, &value))
    {
        lines->push_back( { i, value });
        if (key.l > *align)
        {
            *align = key.l;
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
void RenderImplementation::render(void *node, int indent, bool newline)
{
    assert(node != NULL);
    size_t align = 0;
    vector<pair<void*,xmq::str>> lines;

    if (actions->isNodeComment(node))
    {
        xmq::str value;
        actions->loadValue(node, &value);
        printAligned(node, value, indent, 0, newline);
        return;
    }

    printIndent(indent, newline);

    xmq::str name;
    actions->loadName(node, &name);
    renderElementName(name);

    if (actions->hasAttributes(node))
    {
        printAttributes(node, indent);
        printIndent(indent);
        output("{");
    }
    else
    {
        output(" {");
    }
    void *i = actions->firstNode(node);
    while (i)
    {
        renderNode(i, indent, newline, &lines, &align);
        i = actions->nextSibling(i);
    }
    // Flush any accumulated key:value lines with proper alignment.
    for (auto &p : lines)
    {
        printAligned(p.first, p.second, indent+4, align, true);
    }
    printIndent(indent);
    output("}");
}

int RenderImplementation::output(const char* fmt, ...)
{
    if (render_type_ == xmq::RenderType::html)
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

    va_list args;
    char buffer[65536];
    buffer[65535] = 0;
    va_start(args, fmt);
    vsnprintf(buffer, 65356, fmt, args);
    va_end(args);
    out_buffer->insert(out_buffer->end(), buffer, buffer+strlen(buffer));
    return 0;
}

int RenderImplementation::outputNoEscape(const char* fmt, ...)
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

void RenderImplementation::render()
{
    void *root = actions->root();

    if (use_color_)
    {
        if (render_type_ == xmq::RenderType::terminal)
        {
            useAnsiColors();
        }
        if (render_type_ == xmq::RenderType::html)
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
        xmq::str tmp;
        // Handle the special cases, single empty node and single node with data content.
        if (nodeHasSingleDataChild(root, &tmp) || nodeHasNoChildren(root))
        {
            vector<pair<void*,xmq::str>> lines;
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
        if (actions->parent(root))
        {
            root = actions->nextSibling(root);
        }
        else
        {
            break;
        }
    }

    output("\n");
}

void xmq::renderXMQ(xmq::RenderActions *actions, xmq::RenderType rt, bool use_color, vector<char> *out)
{
    RenderImplementation ri(actions, rt, use_color, out);
    ri.render();
}
