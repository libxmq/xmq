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
#include "util.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>

using namespace std;
using namespace xmq;

bool isWhiteSpace(char c);

struct ParserImplementation
{
    ActionsXMQ *actions;
    const char *file;

    char *buf;
    size_t buf_len;
    size_t pos;
    int line;
    int col;
    bool generate_html {};

    void error(const char* fmt, ...);

    int findIndent(int p);

    bool isReservedCharacter(char c);
    void eatWhiteSpace();

    TokenType peekToken();
    Token eatToken();
    Token eatToEndOfComment();
    Token eatToEndOfLine();
    Token eatMultipleCommentLines();
    Token eatToEndOfText();
    Token eatToEndOfQuote(int indent);

    // Syntax
    void parseXMQ(void *parent);
    void parseComment(void *parent);
    void parseNode(void *parent);
    void parseAttributes(void *parent);

    size_t findDepth(size_t p, int *depth);
    bool isEndingWithDepth(size_t p, int depth);
    size_t potentiallySkipLeading_WS_NL_WS(size_t p);
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

    printf("%.*s\n", col, &buf[pos-col+1]);
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
                // We found a shorter sequence of spaces followed by non-whitespace,
                // use this number as the future commonly shared sequence of spaces.
/*                if (found_non_ws)
                  {*/
                    common = curr;
//                }
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
    t->value = actions->allocateCopy(buf, len+3);
}

int ParserImplementation::findIndent(int p)
{
    int count = 0;
    while (p >= 0 && buf[p] != '\n')
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
        char c = buf[pos];
        if (c == 0) break;
        else if (isNewLine(c))
        {
            col = 1;
            line++;
        }
        else if (!isWhiteSpace(c)) break;
        pos++;
        col++;
    }
}

TokenType ParserImplementation::peekToken()
{
    eatWhiteSpace();

    char c = buf[pos];

    switch (c)
    {
    case 0: return TokenType::none;
    case '\'': return TokenType::quote;
    case '=': return TokenType::equals;
    case '{': return TokenType::brace_open;
    case '}': return TokenType::brace_close;
    case '(': return TokenType::paren_open;
    case ')': return TokenType::paren_close;
    }
    if (c == '/' && (buf[pos+1] == '/' || buf[pos+1] == '*')) return TokenType::comment;
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
        pos++;
        return Token(tt, ""); // Do not bother to store the string of the char itself.
    }
    assert(0);
    return Token(TokenType::none, "");
}

Token ParserImplementation::eatToEndOfText()
{
    size_t start = pos;
    size_t i = pos;
    while (true)
    {
        char c = buf[i];
        if (c == 0)
        {
            pos = i;
            break;
        }
        if (c == '\n')
        {
            pos = i+1;
            line++;
            col = 1;
            break;
        }
        if (isReservedCharacter(c))
        {
            pos = i;
            break;
        }
        i++;
        col++;
    }
    size_t len = i-start;
    char *value = actions->allocateCopy(buf+start, len+1);

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

size_t ParserImplementation::findDepth(size_t p, int *depth)
{
    int count = 0;
    while (buf[p] == '\'')
    {
        p++;
        count++;
    }
    *depth = count;
    return p;
}

bool ParserImplementation::isEndingWithDepth(size_t p, int depth)
{
    while (buf[p] == '\'')
    {
        p++;
        depth--;
        if (depth < 0)
        {
            error("too many quotes");
        }
    }
    if (depth == 0) return true;
    return false;
}

size_t ParserImplementation::potentiallySkipLeading_WS_NL_WS(size_t p)
{
    size_t org_p = p;
    bool nl_found = false;
    for (;;)
    {
        if (buf[p] == 0)
        {
            p = org_p;
            break;
        }
        if (buf[p] == ' ')
        {
            p++;
            continue;
        }
        if (buf[p] == '\n')
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
    if (buf[pos] == '\'' && buf[pos+1] == '\'' && buf[pos+2] != '\'')
    {
        // This is the empty string! ''
        pos += 2;
        char *value = actions->allocateCopy("", 1);
        return Token(TokenType::text, value);
    }

    // How many ' single quotes are there?
    int depth = 0;
    size_t start = findDepth(pos, &depth);

    // p now points the first character after the quotes.
    size_t p = start;
    // If there is ws nl ws, then skip it.
    p = potentiallySkipLeading_WS_NL_WS(p);

    // Remember the first lines offset into the line.
    int first_indent = findIndent(p);

    vector<char> buffer;
    while (true)
    {
        char c = buf[p];
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
            pos  = p + depth;
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
    char *value = actions->allocateCopy(&(buffer[0]), buffer.size()+1);

    return Token(TokenType::text, value);
}

Token ParserImplementation::eatToEndOfComment()
{
    assert(buf[pos] == '/');
    pos++;
    bool single_line = buf[pos] == '/';
    pos++;
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
    size_t start = pos;
    size_t p = pos;
    while (true)
    {
        char c = buf[p];
        if (c == 0)
        {
            pos = p;
            break;
        }
        if (c == '\n')
        {
            pos = p + 1;
            line++;
            col = 1;
            break;
        }
        p++;
        col++;
    }
    size_t len = p-start;
    char *value = actions->allocateCopy(buf+start, len+1);

    return Token(TokenType::text, value);
}

Token ParserImplementation::eatMultipleCommentLines()
{
    size_t p = pos;

    int first_indent = findIndent(p);
    vector<char> buffer;

    while (true)
    {
        char c = buf[p];
        if (c == 0)
        {
            error("unexpected eof in comment");
        }
        if (c == '\n')
        {
            line++;
            col = 1;
        }
        if (buf[p] == '*' && buf[p+1] == '/')
        {
            pos = p + 2;
            break;
        }
        buffer.push_back(c);
        p++;
        col++;
    }

    removeIncidentalWhiteSpace(&buffer, first_indent);
    char *value = actions->allocateCopy(&(buffer[0]), buffer.size()+1);

    return Token(TokenType::text, value);
}

void ParserImplementation::parseComment(void *parent)
{
    Token val = eatToken();

    actions->appendComment(parent, val);
}

void ParserImplementation::parseXMQ(void *parent)
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
            actions->appendData(parent, val);
        }
        else
        {
            break;
        }
    }
}

void ParserImplementation::parseAttributes(void *parent)
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
            actions->appendAttribute(parent, t, t);
            continue;
        }

        if (peekToken() != TokenType::equals) error("expected =");
        eatToken();

        Token val = eatToken();

        if (val.type == TokenType::text ||
            val.type == TokenType::quote)
        {
            actions->appendAttribute(parent, t, val);
        }
        else
        {
            error("expected text or quoted text");
        }
    }

}

void ParserImplementation::parseNode(void *parent)
{
    Token t = eatToken();
    if (t.type != TokenType::text) error("expected tag");

    void *node = actions->appendElement(parent, t);

    TokenType tt = peekToken();

    if (tt == TokenType::paren_open)
    {
        parseAttributes(node);
        tt = peekToken();
    }

    if (tt == TokenType::brace_open)
    {
        eatToken();
        parseXMQ(node);
        tt = peekToken();
        if (tt == TokenType::brace_close)
        {
            eatToken();
        }
        else
        {
            error("expecte brace close");
        }
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
            actions->appendData(node, val);
        }
    }
}

void xmq::parse(const char *filename, char *xmq, ActionsXMQ *actions, bool generate_html)
{
    ParserImplementation parser;

    parser.actions = actions;
    parser.buf = xmq;
    parser.buf_len = strlen(xmq);
    parser.pos = 0;
    parser.line = 1;
    parser.col = 1;
    parser.file = filename;
    parser.generate_html = generate_html;

    parser.parseXMQ(actions->root());
}
