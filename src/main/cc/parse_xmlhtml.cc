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

#include "xmq.h"
#include "xmq_implementation.h"
#include "util.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>

using namespace std;
using namespace xmq;

class XMLHTMLParserImplementation
{
public:
    XMLHTMLParserImplementation(xmq::ParseActions *pa) : parse_actions(pa) {}

private:
    ParseActions *parse_actions {};
    bool html {};
    const char *file {};
    const char *buf {};
    const char *root {};
    size_t buf_len {};
    size_t pos {};
    int line {};
    int col {};

    int ws_start {};
    void eatWhiteSpace();

    void error(const char* fmt, ...);
    void errornoline(const char* fmt, ...);

    int findIndent(int p);

    bool isReservedCharacter(char c);

    NodeType peekNodeToken();
    NodeToken eatNodeToken();
    NodeToken eatToEndOfComment();
    NodeToken eatToEndOfLine();
    NodeToken eatMultipleCommentLines();
    NodeToken eatToEndOfText();
    void eatToEndOfQuote(int indent, vector<char> *buffer);
    NodeToken eatToEndOfQuotes(int indent);

    // Syntax
    void parseComment(void *parent);
    void parseNode(void *parent);
    void parseAttributes(void *parent);

    size_t findDepth(size_t p, int *depth);
    bool isEndingWithDepth(size_t p, int depth);
    size_t potentiallySkipLeading_WS_NL_WS(size_t p);
    void potentiallyRemoveEnding_WS_NL_WS(vector<char> *buffer);
    void trimTokenWhiteSpace(NodeToken *t);

    void padWithSingleSpaces(NodeToken *t);

public:
    void setup(ParseActions *a, const char *f, const char *b, const char *r)
    {
        parse_actions = a;
        file = f;
        buf = b;
        buf_len = strlen(buf);
        root = r;
        pos = 0;
        line = 1;
        col = 1;
    }
    void parseXML(void *node);
    void parse();
};

void XMLHTMLParserImplementation::error(const char* fmt, ...)
{
    printf("%s:%d:%d: error: ", file, line, col);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    printf("%.*s\n", col, &buf[pos-col+1]);
    exit(1);
}

void XMLHTMLParserImplementation::errornoline(const char* fmt, ...)
{
    printf("%s:%d:%d: error: ", file, line, col);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    exit(1);
}

void XMLHTMLParserImplementation::eatWhiteSpace()
{
    ws_start = pos;
    while (true)
    {
        char c = buf[pos];
        if (c == 0) break;
        else if (xmq_implementation::isNewLine(c))
        {
            col = 1;
            line++;
        }
        else if (!xmq_implementation::isWhiteSpace(c)) break;
        pos++;
        col++;
    }
}

NodeType XMLHTMLParserImplementation::peekNodeToken()
{
    eatWhiteSpace();

    char c = buf[pos];

    if (c == 0) return NodeType::none;
    if (c == '<')
    {
        char cc = buf[pos+1];
        if (cc == '?')
        {
            return NodeType::pi;
        }
        if (cc == '!')
        {
            char ccc = buf[pos+1];
            if (ccc == '-')
            {
                return NodeType::comment;
            }
            return NodeType::doctype;
        }
        if (cc == '-')
        {
            return NodeType::comment;
        }
    }
    return NodeType::none;
}

void XMLHTMLParserImplementation::parseXML(void *node)
{

}

void XMLHTMLParserImplementation::parse()
{
    parseXML(parse_actions->root());
}

void xmq::parseXML(ParseActions *actions, const char *filename, const char *xml, xmq::Config &config)
{
    XMLHTMLParserImplementation pi(actions);
    pi.setup(actions, filename, xml, config.root);
    pi.parse();
}
