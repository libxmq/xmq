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

#include "parse.h"
#include "util.h"
#include <string.h>
#include <stdarg.h>
#include "rapidxml/rapidxml.hpp"

using namespace rapidxml;
using namespace std;

bool isWhiteSpace(char c);

struct ParserImplementation
{
    const char *file;

    char *beginning;
    char *s;
    int line;
    int col;
    xml_document<> *doc;
    xml_node<> *root;
    bool generate_html {};

    void error(const char* fmt, ...);

    int findIndent(const char *p);

    bool isReservedCharacter(char c);
    void eatWhiteSpace();

    TokenType peekToken();
    Token eatToken();
    Token eatToEndOfComment();
    Token eatToEndOfLine();
    Token eatMultipleCommentLines();
    Token eatToEndOfText();
    Token eatToEndOfQuote(int indent);

    void parseComment(xml_node<> *parent);
    void parseNode(xml_node<> *parent);
    void parseNodeContent(xml_node<> *parent);
    void parseAttributes(xml_node<> *parent);

    char *findDepth(char *p, int *depth);
    bool isEndingWithDepth(char *p, int depth);
    char *potentiallySkipLeading_WS_NL_WS(char *p);
    void potentiallyRemoveEnding_WS_NL_WS(vector<char> *buffer);
    void padWithSingleSpaces(Token *t);
};

void ParserImplementation::error(const char* fmt, ...)
{
    printf("%s:%d:%d: error: ", file, line, col);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    printf("%.*s\n", col, s-col+1);
    exit(1);
}

void trimTokenWhiteSpace(Token *t)
{
    size_t len = strlen(t->value);
    // Trim away whitespace at the beginning.
    while (len > 0 && *t->value != 0)
    {
        if (!isWhiteSpace(*t->value)) break;
        t->value++;
        len--;
    }

    // Trim away whitespace at the end.
    while (len >= 1)
    {
        if (!isWhiteSpace(t->value[len-1])) break;
        len--;
    }
}

void removeIncidentalWhiteSpace(vector<char> *buffer, int first_indent)
{
    // Check that there are newlines in here!
    bool found_nl = false;
    for (size_t i=0; i<buffer->size(); ++i)
    {
        if ((*buffer)[i] == '\n') { found_nl = true; break; }
    }
    if (!found_nl) return;

    // There is at least one newline!
    int common = -1;
    int curr = first_indent;
    bool looking = true;
    vector<char> copy;
    // Simulate the indentation in the copy by pushing spaces first.
    for (int i = 0; i < first_indent-1; ++i)
    {
        copy.push_back(' ');
    }
    for (size_t i = 0; i < buffer->size(); ++i)
    {
        char c = (*buffer)[i];
//        fprintf(stderr, "%c(%d)\n", c, c);
        copy.push_back(c);
        if (c == '\n')
        {
            // We reached end of line.
            if (curr < common || common == -1)
            {
                // We found a shorter sequence of spaces, use this number
                // as the future commonly shared sequence of spaces.
                common = curr;
//                fprintf(stderr, "common %d\n", common);
            }
            curr = 0;
            looking = true;
        }
        else
        {
            if (looking)
            {
                if (c == ' ')
                {
                    curr++;
//                    fprintf(stderr, "curr++ %d\n", curr);
                }
                else
                {
                    looking = false;
                }
            }
        }
    }
    buffer->clear();

    curr = common+1;
    for (size_t i = 0; i < copy.size(); ++i)
    {
        if (copy[i] == '\n')
        {
            curr = common+1;
            buffer->push_back('\n');
            continue;
        }
        if (copy[i] != ' ')
        {
            curr = 0;
        }
        else
        {
            if (curr > 0) curr--;
        }
        if (curr == 0)
        {
            buffer->push_back(copy[i]);
        }
    }
}

void ParserImplementation::padWithSingleSpaces(Token *t)
{
    size_t len = strlen(t->value);
    char buf[len+3]; // Two spaces and zero terminator.
    buf[0] = ' ';
    memcpy(buf+1, t->value, len);
    buf[1+len] = ' ';
    buf[2+len] = 0;
    t->value = doc->allocate_string(buf, len+3);
}

int ParserImplementation::findIndent(const char *p)
{
    int count = 0;
    while (p > beginning && *p != '\n')
    {
        p--;
        count++;
    }
    return count;
}

bool ParserImplementation::isReservedCharacter(char c)
{
    return
        c == 0 ||
        c == '\'' ||
        c == '=' ||
        c == '{' ||
        c == '}' ||
        c == '(' ||
        c == ')' ||
        c == ' ' ||
        c == '\t' ||
        c == '\r' ||
        c == '\n';
}

void ParserImplementation::eatWhiteSpace()
{
    while (true)
    {
        if (*s == 0) break;
        else if (isNewLine(*s))
        {
            col = 1;
            line++;
        }
        else if (!isWhiteSpace(*s)) break;
        s++;
        col++;
    }
}

TokenType ParserImplementation::peekToken()
{
    eatWhiteSpace();

    switch (*s)
    {
    case 0: return TokenType::none;
    case '\'': return TokenType::quote;
    case '=': return TokenType::equals;
    case '{': return TokenType::brace_open;
    case '}': return TokenType::brace_close;
    case '(': return TokenType::paren_open;
    case ')': return TokenType::paren_close;
    }
    if (*s == '/' && (*(s+1) == '/' || *(s+1) == '*')) return TokenType::comment;
    return TokenType::text;
}

Token ParserImplementation::eatToken()
{
    TokenType tt = peekToken();
    switch (tt)
    {
    case TokenType::none: return Token(TokenType::none, "");
    case TokenType::text: return eatToEndOfText();
    case TokenType::quote: return eatToEndOfQuote(col);
    case TokenType::comment: return eatToEndOfComment();
    case TokenType::equals:
    case TokenType::brace_open:
    case TokenType::brace_close:
    case TokenType::paren_open:
    case TokenType::paren_close:
        s++;
        return Token(tt, ""); // Do not bother to store the string of the char itself.
    }
    assert(0);
    return Token(TokenType::none, "");
}

Token ParserImplementation::eatToEndOfText()
{
    char *start = s;
    char *p = start;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            s = p;
            break;
        }
        if (c == '\n')
        {
            s = p+1;
            line++;
            col = 1;
            break;
        }
        if (isReservedCharacter(c))
        {
            s = p;
            break;
        }
        p++;
        col++;
    }
    size_t len = p-start;
    char *value = doc->allocate_string(start, len+1);

    return Token(TokenType::text, value);
}

void addNewline(vector<char> *buffer)
{
    buffer->push_back('&');
    buffer->push_back('#');
    buffer->push_back('1');
    buffer->push_back('0');
    buffer->push_back(';');
}

char *ParserImplementation::findDepth(char *p, int *depth)
{
    while (*p == '\'')
    {
        p++;
        (*depth)++;
    }
    return p;
}

bool ParserImplementation::isEndingWithDepth(char *p, int depth)
{
    while (*p == '\'')
    {
        p++;
        depth--;
    }
    if (depth == 0) return true;
    if (depth < 0)
    {
        error("too many quotes");
    }
    return false;
}

char *ParserImplementation::potentiallySkipLeading_WS_NL_WS(char *p)
{
    char *org_p = p;
    bool nl_found = false;
    for (;;)
    {
        if (*p == 0)
        {
            p = org_p;
            break;
        }
        if (*p == ' ')
        {
            p++;
            continue;
        }
        if (*p == '\n')
        {
            if (nl_found) break;
            nl_found = true;
            p++;
            continue;
        }
        break;
    }
    // Only trim if there was a new line!
    if (nl_found) return p;
    // Else leave the whitespace as is.
    return org_p;
}

void ParserImplementation::potentiallyRemoveEnding_WS_NL_WS(vector<char> *buffer)
{
    char *start = &(*buffer)[0];
    char *p = &(*buffer)[buffer->size()-1];
    bool nl_found = false;
    for (;;)
    {
        if (p <= start)
        {
            p = start;
            break;
        }
        if (*p == ' ')
        {
            p--;
            continue;
        }
        if (*p == '\n')
        {
            if (nl_found) break;
            nl_found = true;
            p--;
            continue;
        }
        break;
    }
    if (nl_found)
    {
        // Only trim if there was a new line!
        size_t len = 1+p-start;
        buffer->resize(len);
    }
}

Token ParserImplementation::eatToEndOfQuote(int indent)
{
    if (*s == '\'' && *(s+1) == '\'' && *(s+2) != '\'')
    {
        // This is the empty string! ''
        s += 2;
        char *value = doc->allocate_string("", 1);
        return Token(TokenType::text, value);
    }

    // How many ' single quotes are there?
    int depth = 0;
    char *start = findDepth(s, &depth);

    // p now points the first character after the quotes.
    char *p = start;
    // If there is ws nl ws, then skip it.
    p = potentiallySkipLeading_WS_NL_WS(p);

    // Remember the first lines offset into the line.
    int first_indent = findIndent(p);

    vector<char> buffer;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            error("unexpected eof in quoted text");
        }
        else
        if (c == '\n')
        {
            buffer.push_back('\n');
            line++;
            col = 1;
            p++;
            continue;
        }
        else
        if (isEndingWithDepth(p, depth))
        {
            // We found the ending quote!
            s = p+depth;
            break;
        }
        buffer.push_back(c);
        col++;
        p++;
    }

    potentiallyRemoveEnding_WS_NL_WS(&buffer);
    removeIncidentalWhiteSpace(&buffer, first_indent);

    if (buffer.size() == 0)
    {
        error("empty string must always be two single quotes ''.");
    }
    char *value = doc->allocate_string(&(buffer[0]), buffer.size()+1);

    return Token(TokenType::text, value);
}

Token ParserImplementation::eatToEndOfComment()
{
    assert(*s == '/');
    s++;
    bool single_line = *s == '/';
    s++;
    if (single_line)
    {
        Token t = eatToEndOfLine();
        trimTokenWhiteSpace(&t);
        padWithSingleSpaces(&t);
        return t;
    }
    Token t = eatMultipleCommentLines();
    return t;
}

Token ParserImplementation::eatToEndOfLine()
{
    char *start = s;
    char *p = start;
    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            s = p;
            break;
        }
        if (c == '\n')
        {
            s = p+1;
            line++;
            col = 1;
            break;
        }
        p++;
        col++;
    }
    size_t len = p-start;
    char *value = doc->allocate_string(start, len+1);

    return Token(TokenType::text, value);
}

Token ParserImplementation::eatMultipleCommentLines()
{
    char *start = s + 1;
    char *p = start;
    int first_indent = findIndent(p);
    vector<char> buffer;

    while (true)
    {
        char c = *p;
        if (c == 0)
        {
            error("unexpected eof in comment");
        }
        if (c == '\n')
        {
            line++;
            col = 1;
        }
        if (c == '*' && *(p+1) == '/')
        {
            s = p+2;
            break;
        }
        buffer.push_back(c);
        p++;
        col++;
    }

    removeIncidentalWhiteSpace(&buffer, first_indent);
    char *value = doc->allocate_string(&(buffer[0]), buffer.size()+1);

    return Token(TokenType::text, value);
}

void ParserImplementation::parseComment(xml_node<> *parent)
{
    Token val = eatToken();

    parent->append_node(doc->allocate_node(node_comment, NULL, val.value));
}

void ParserImplementation::parseNodeContent(xml_node<> *parent)
{
    while (true)
    {
        TokenType t = peekToken();

        if (t == TokenType::comment)
        {
            parseComment(parent);
        }
        else
        if (t == TokenType::text)
        {
            parseNode(parent);
        }
        else
        if (t == TokenType::quote)
        {
            Token val = eatToken();
            parent->append_node(doc->allocate_node(node_data, NULL, val.value));
        }
        else
        if (t == TokenType::brace_close ||
            t == TokenType::none)
        {
            eatToken();
            break;
        }
        else
        {
            error("unexpected token");
        }
    }
}

void ParserImplementation::parseAttributes(xml_node<> *parent)
{
    Token po = eatToken();
    assert(po.type == TokenType::paren_open);

    while (true)
    {
        Token t = eatToken();
        if (t.type == TokenType::paren_close) break;
        if (t.type != TokenType::text)
        {
            error("expected attribute");
        }
        TokenType nt = peekToken();
        if (nt == TokenType::text ||
            nt == TokenType::paren_close)
        {
            // This attribute is completed, it has no data.
            parent->append_attribute(doc->allocate_attribute(t.value, t.value));
            continue;
        }

        if (peekToken() != TokenType::equals) error("expected =");
        eatToken();

        Token val = eatToken();

        if (val.type == TokenType::text ||
            val.type == TokenType::quote)
        {
            parent->append_attribute(doc->allocate_attribute(t.value, val.value));
        }
        else
        {
            error("expected text or quoted text");
        }
    }

}

void ParserImplementation::parseNode(xml_node<> *parent)
{
    Token t = eatToken();
    if (t.type != TokenType::text) error("expected tag");

    xml_node<> *node = doc->allocate_node(node_element, t.value);
    parent->append_node(node);

    TokenType tt = peekToken();

    if (tt == TokenType::paren_open)
    {
        parseAttributes(node);
        tt = peekToken();
    }

    if (tt == TokenType::brace_open)
    {
        eatToken();
        parseNodeContent(node);
    }
    else if (tt == TokenType::equals)
    {
        eatToken();
        Token val = eatToken();
        if (val.type != TokenType::text &&
            val.type != TokenType::quote)
        {
            error("expected text or quote");
        }
        if (val.value[0] != 0)
        {
            node->append_node(doc->allocate_node(node_data, NULL, val.value));
        }
    }
}

void parse(const char *filename, char *xmq, xml_document<> *doc, bool generate_html)
{
    ParserImplementation parser;

    parser.doc = doc;
    parser.s = xmq;
    parser.beginning = parser.s;
    parser.line = 1;
    parser.col = 1;
    parser.file = filename;
    parser.generate_html = generate_html;

    // Handle early comments.
    while (TokenType::comment == parser.peekToken())
    {
        parser.parseComment(doc);
    }

    if (TokenType::none != parser.peekToken())
    {
        parser.parseNode(doc);
    }

    // Handle trailing comments.
    while (TokenType::comment == parser.peekToken())
    {
        parser.parseComment(doc);
    }

    if (TokenType::none != parser.peekToken())
    {
        printf("Syntax error, no more data is allowed after last closing brace.\n");
        //exit(1);
    }
}
