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

#ifndef XMQ_IMPLEMENTATION_H
#define XMQ_IMPLEMENTATION_H

#include "xmq.h"

#include<vector>
#include<string.h>

namespace xmq_implementation
{
    bool startsWithLessThan(std::vector<char> &buffer);
    bool isWhiteSpace(char c);
    bool isNewLine(char c);
    bool isHtml(std::vector<char> &buffer);
    bool firstWordIsHtml(std::vector<char> &buffer);
    bool firstWordIs(const char *b, size_t len, const char *word);
    void removeIncidentalWhiteSpace(std::vector<char> *buffer, int first_indent);
    int  escapingDepth(xmq::str value, bool *add_start_newline, bool *add_end_newline, bool is_attribute);
    const char *findStartingNewline(const char *where, const char *start);
    const char *findEndingNewline(const char *where);
    void findLineAndColumn(const char *from, const char *where, int *line, int *col);
    bool strCompare(const xmq::str &a, const xmq::str &b);
}

#endif
