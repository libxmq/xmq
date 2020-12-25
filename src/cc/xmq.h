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

#ifndef XMQ_H
#define XMQ_H

#include <string>
#include <vector>
#include <set>

namespace xmq
{
// The render type is how the output from the xmq command is presented.
// Plain is the exact xmq/xml/html input/output.
// Terminal means that it will inject ansi color sequences.
// Html means that it will inject html color sequences and formatting.
// Tex means that it will inject tex color sequences and formatting.
enum class RenderType { plain, terminal, html, tex };

// Xmq can be converted between the selected or auto tree type.
enum class TreeType { auto_detect, xml, html };

struct Settings
{
    // You can specify a filename for xml2xmq and it will be loaded
    // automatically if in is NULL.
    // For xmq2xml a filename is necessary when there are multiple root nodes
    // in the xmq file/buffer. Since xml only allows for a single root node,
    // for such xmq files, an implicit root node with the name of the file will be created.
    std::string filename;
    std::vector<char> *in;
    std::vector<char> *out;

    // When converting, auto_detect or use xml or html.
    TreeType tree_type {};
    // You can render plain text, terminal output potentially with ansi colors,
    // html output potentially with html colors and tex output potentially with tex colors.
    RenderType output {};
    // Set to true to produce colors. Color can never be enabled with the plain output type.
    bool use_color {};
    // Set to true to allow parsing and generation of void elements (br,img,input etc).
    bool html {};
    // Do not print any xml-declaration <? ?> nor doctype <!DOCTYPE html>.
    bool no_declaration {};
    // When converting from xml to xmq. Preserve whitespace as much as possible.
    bool preserve_ws {};
    // Do not convert, just view the input, potentially adding color and formatting.
    bool view {};
    // Find common prefixes of the tags.
    bool compress {};

    std::set<std::string> excludes;
};

enum class TokenType
{
    none,
    equals,        // =
    brace_open,    // {
    brace_close,   // }
    paren_open,    // (
    paren_close,   // )
    quote,         // '....'
    comment,       // / starts either // or /*
    text           // Not quoted text, can be tag or content.
};

struct Token
{
    Token(TokenType t, const char *v) : type(t), value(v) { }

    TokenType type;
    const char *value; // Zero terminated string allocated by rapid_xml allocate_string.
};

struct ActionsXMQ
{
    virtual void *root() = 0;
    virtual char *allocateCopy(const char *content, size_t len) = 0;
    virtual void *appendElement(void *parent, Token t) = 0;
    virtual void appendComment(void *parent, Token t) = 0;
    virtual void appendData(void *parent, Token t) = 0;
    virtual void appendAttribute(void *parent, Token key, Token value) = 0;
};

int main_xml2xmq(Settings *settings);
int main_xmq2xml(const char *filename, Settings *settings);
void renderDoc(void *node, Settings *provided_settings);

void parse(const char *filename, char *xmq, ActionsXMQ *actions, bool generate_html);

}

#endif
