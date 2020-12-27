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

#include "string.h"

namespace xmq
{
    // The render type is how the output from the xmq command is presented.
    // Plain is the exact xmq/xml/html input/output.
    // Terminal means that it will inject ansi color sequences.
    // Html means that it will inject html color sequences and formatting.
    // Tex means that it will inject tex color sequences and formatting.
    enum class RenderType { plain, terminal, html, tex };

    // When converting, detect the from/to type or set it accordingly.
    enum class TreeType { auto_detect, xml, html };

    enum class TokenType
    {
        none,          // eof
        equals,        // =
        brace_open,    // {
        brace_close,   // }
        paren_open,    // (
        paren_close,   // )
        quote,         // '....'
        comment,       // / starts either // or /*
        text           // Not quoted text, can be name or content.
    };

    struct str
    {
        const char *s; // Start of string.
        size_t l; // Length of string.

        str(const char *st, size_t le) : s(st), l(le) {}
        bool equals(std::string &st)
        {
            if (l != st.size()) return false;
            return !strncmp(st.c_str(), s, l);
        }
        bool equals(str &st)
        {
            if (l != st.l) return false;
            return !strncmp(st.s, s, l);
        }
        bool equals(const char *st)
        {
            return !strncmp(st, s, l);
        }

        str() : s(""), l(0) {}
    };

    struct Token
    {
        Token(TokenType t, const char *v) : type(t), value(v) { }

        TokenType type;
        const char *value; // Zero terminated string allocated by rapid_xml allocate_string.
    };

    struct RenderActions
    {
        virtual void *root() = 0;
        virtual void *firstNode(void *node) = 0;
        virtual void *nextSibling(void *node) = 0;
        virtual bool hasAttributes(void *node) = 0;
        virtual void *firstAttribute(void *node) = 0;
        virtual void *nextAttribute(void *attr) = 0;
        virtual void *parent(void *node) = 0;
        virtual bool isNodeData(void *node) = 0;
        virtual bool isNodeComment(void *node) = 0;
        virtual bool isNodeCData(void *node) = 0;
        virtual bool isNodeDocType(void *node) = 0;
        virtual bool isNodeDeclaration(void *node) = 0;
        virtual void loadName(void *node, xmq::str *name) = 0;
        virtual void loadValue(void *node, xmq::str *data) = 0;
    };

    struct ParseActions
    {
        virtual void *root() = 0;
        virtual char *allocateCopy(const char *content, size_t len) = 0;
        virtual void *appendElement(void *parent, Token t) = 0;
        virtual void appendComment(void *parent, Token t) = 0;
        virtual void appendData(void *parent, Token t) = 0;
        virtual void appendAttribute(void *parent, Token key, Token value) = 0;
    };

    void renderXMQ(RenderActions *actions, RenderType rt, bool use_color, std::vector<char> *out);
    void parseXMQ(ParseActions *actions, const char *filename, const char *xmq);
}

#endif
