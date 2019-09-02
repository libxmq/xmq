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

#ifndef PARSE_H
#define PARSE_H

#include <map>
#include <vector>
#include "rapidxml/rapidxml.hpp"

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
    Token(TokenType t, const char *d, size_t l) : type(t), data(d), len(l) { }

    TokenType type;
    const char *data;
    size_t len;

    void print(const char *pre, const char *post)
    {
        int l = (int)len;
        printf("%s%.*s%s", pre, l, data, post);
    }
};

void parse(const char *filename, char *text, rapidxml::xml_document<> *doc);

#endif
