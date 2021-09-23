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

class ParserImplementation
{
public:
    ParserImplementation(xmq::ParseActions *pa) : parse_actions(pa) {}

private:
    ParseActions *parse_actions {};
    const char *file {};
    const char *buf {};
    const char *root {};
    size_t buf_len {};
    size_t pos {};
    int line {};
    int col {};

    void eatWhiteSpace();

    void error(const char* fmt, ...);
    void errornoline(const char* fmt, ...);

    int findIndent(int p);

    bool isReservedCharacter(char c);

    TokenType peekToken();
    Token eatToken();
    Token eatToEndOfComment();
    Token eatToEndOfLine();
    Token eatMultipleCommentLines();
    Token eatToEndOfText();
    void eatToEndOfQuote(int indent, vector<char> *buffer);
    Token eatToEndOfQuotes(int indent);

    // Syntax
    void parseComment(void *parent);
    void parseNode(void *parent);
    void parseAttributes(void *parent);

    size_t findDepth(size_t p, int *depth);
    bool isEndingWithDepth(size_t p, int depth);
    size_t potentiallySkipLeading_WS_NL_WS(size_t p);
    void potentiallyRemoveEnding_WS_NL_WS(vector<char> *buffer);
    void trimTokenWhiteSpace(Token *t);

    void padWithSingleSpaces(Token *t);

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
    void parseXMQ(void *node);
    void parse();
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

void ParserImplementation::errornoline(const char* fmt, ...)
{
    printf("%s:%d:%d: error: ", file, line, col);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    exit(1);
}

void ParserImplementation::trimTokenWhiteSpace(Token *t)
{
    size_t len = strlen(t->value);
    // Trim away whitespace at the beginning.
    while (len > 0 && *t->value != 0)
    {
        if (!xmq_implementation::isWhiteSpace(*t->value)) break;
        t->value++;
        len--;
    }

    // Trim away whitespace at the end.
    while (len >= 1)
    {
        if (!xmq_implementation::isWhiteSpace(t->value[len-1])) break;
        len--;
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
    t->value = parse_actions->allocateCopy(buf, len+3);
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
    case TokenType::quote: return eatToEndOfQuotes(col);
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
    char *value = parse_actions->allocateCopy(buf+start, len+1);

    return Token(TokenType::text, value);
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

Token ParserImplementation::eatToEndOfQuotes(int indent)
{
    assert(buf[pos] == '\'');

    vector<char> buffer;
    for (;;)
    {
        assert(buf[pos] == '\'');
        eatToEndOfQuote(indent, &buffer);

        // Now check if a \ is suffixed!
        if (buf[pos] != '\\')
        {
            // No quote new line adding, lest stop.
            break;
        }

        assert(buf[pos] == '\\');
        if (buf[pos+1] != 'n' && buf[pos+1] != '\n')
        {
            error("expected n after quote suffixed with \\.");
        }
        if (buf[pos+1] == 'n' && buf[pos+2] != '\n')
        {
            error("expected newline after quote suffixed with \\n.");
        }
        assert(buf[pos+1] == '\n' || (buf[pos+1] == 'n' && buf[pos+2] == '\n'));

        pos++; // Skip
        if (buf[pos] == 'n')
        {
            pos++; // Skip n
            buffer.push_back('\n');
        }

        assert(buf[pos] == '\n');
        // Detected '.....'\ followed by newline
        // or       '.....'\n followed by newline
        // Now skip whitespace.
        eatWhiteSpace();
        // Now we must have reached another quote.
        if (buf[pos] != '\'')
        {
            error("expected quote after quote suffixed with \\ or \\n.");
        }
    }

    char *value;
    if (buffer.size() == 0)
    {
        value = parse_actions->allocateCopy("", 1);
    }
    else
    {
        value = parse_actions->allocateCopy(&buffer[0], buffer.size()+1);
    }
    return Token(TokenType::text, value);

}

void ParserImplementation::eatToEndOfQuote(int indent, vector<char> *buffer)
{
    if (buf[pos] == '\'' && buf[pos+1] == '\'' && buf[pos+2] != '\'')
    {
        // This is the empty string! ''
        pos += 2;
        // Nothing needs to be added to the buffer.
        return;
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

    vector<char> quote;
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
            quote.push_back('\n');
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
        quote.push_back(c);
        col++;
        p++;
    }

    potentiallyRemoveEnding_WS_NL_WS(&quote);
    xmq_implementation::removeIncidentalWhiteSpace(&quote, first_indent);

    buffer->insert(buffer->end(), quote.begin(), quote.end());
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
    char *value = parse_actions->allocateCopy(buf+start, len+1);

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

    xmq_implementation::removeIncidentalWhiteSpace(&buffer, first_indent);
    char *value = parse_actions->allocateCopy(&(buffer[0]), buffer.size()+1);

    return Token(TokenType::text, value);
}

void ParserImplementation::parseComment(void *parent)
{
    Token val = eatToken();

    parse_actions->appendComment(parent, val);
}

void ParserImplementation::parse()
{
    void *root_node = parse_actions->root();
    if (root != NULL && *root != 0)
    {
        if (!xmq_implementation::firstWordIs(buf, buf_len, root))
        {
            // We expect a specific root node, it does not seem to exist!
            // Lets add it!
            Token t(TokenType::text, root);
            root_node = parse_actions->appendElement(parse_actions->root(), t);
        }
    }

    parseXMQ(root_node);
}

void ParserImplementation::parseXMQ(void *parent)
{
    bool is_root = (parent == parse_actions->root());
    int  num_contents = 0;

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
            if (is_root && num_contents >= 1) goto err;
            parseNode(parent);
            num_contents++;
        }
        else
        if (t == TokenType::quote)
        {
            if (is_root && num_contents >= 1) goto err;
            parse_actions->appendData(parent, eatToken());
            num_contents++;
        }
        else
        {
            break;
        }
    }

    return;

err:
    errornoline("multiple root nodes are not allowed unless for example: --root=config is added.");
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
            parse_actions->appendAttribute(parent, t, t);
            continue;
        }

        if (peekToken() != TokenType::equals) error("expected =");
        eatToken();

        Token val = eatToken();

        if (val.type == TokenType::text ||
            val.type == TokenType::quote)
        {
            parse_actions->appendAttribute(parent, t, val);
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

    void *node = parse_actions->appendElement(parent, t);

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
            error("expected closing brace");
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
            parse_actions->appendData(node, val);
        }
    }
}

void xmq::parseXMQ(ParseActions *actions, const char *filename, const char *xmq, xmq::Config &config)
{
    ParserImplementation pi(actions);
    pi.setup(actions, filename, xmq, config.root);
    pi.parse();
}
