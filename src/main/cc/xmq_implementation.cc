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

#include<string.h>
bool xmq_implementation::isWhiteSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool xmq_implementation::isNewLine(char c)
{
    return c == '\n';
}

const char *doctype = "<!DOCTYPE html>";
const char *html = "<html";

bool xmq_implementation::startsWithLessThan(std::vector<char> &buffer)
{
    for (char c : buffer)
    {
        if (!xmq_implementation::isWhiteSpace(c))
        {
            return c == '<';
        }
    }
    return false;
}

bool xmq_implementation::isHtml(std::vector<char> &buffer)
{
    size_t i=0;
    for (; i<buffer.size(); ++i)
    {
        // Skip any whitespace
        if (isWhiteSpace(buffer[i])) continue;
        // First non-whitespace character.
        if (i+strlen(doctype) < buffer.size() &&
            !strncasecmp(&buffer[i], doctype, strlen(doctype)))
        {
            return true;
        }
        if (i+strlen(html) < buffer.size() &&
            (!strncasecmp(&buffer[i], html, strlen(html))))
        {
            return true;
        }
        break;
    }
    return false;
}

bool xmq_implementation::firstWordIsHtml(std::vector<char> &buffer)
{
    size_t i=0;
    size_t len = strlen("html");

    for (; i<buffer.size(); ++i)
    {
        // Skip any whitespace
        if (isWhiteSpace(buffer[i])) continue;
        // First non-whitespace character.
        if (i+len+1 < buffer.size() && (!strncasecmp(&buffer[i], "html", len)))
        {
            // Check that we have "html " "html=123" or "html{"
            if (buffer[i+len] == ' ' || buffer[i+len] == '=' || buffer[i+len] == '{' || buffer[i+len] == '(')
            {
                return true;
            }
        }
        break;
    }
    return false;
}

void xmq_implementation::removeIncidentalWhiteSpace(std::vector<char> *buffer, int first_indent)
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
    std::vector<char> copy;
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

int xmq_implementation::escapingDepth(xmq::str value, bool *add_start_newline, bool *add_end_newline, bool is_attribute)
{
    size_t n = 0;
    if (value.l == 0) return 0; // No escaping neseccary.
    const char *s = value.s;
    const char *end = s+value.l;
    bool escape = false;
    bool found_quote = false;
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
            found_quote = true;
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
        if (is_attribute)
        {
            escape = true;
        }
    }
    if (*(s-1) == '\'') *add_end_newline = true;

    if (escape)
    {
        if (depth == 0)
        {
            depth = 1;
        }
        if (found_quote)
        {
            if (depth < 3)
            {
                depth = 3;
            }
            else
            {
                depth = depth+1;
            }
        }
    }
    return depth;
}

const char *xmq_implementation::findStartingNewline(const char *where, const char *start)
{
    while (where > start && *(where-1) != '\n') where--;
    return where;
}

const char *xmq_implementation::findEndingNewline(const char *where)
{
    while (*where && *where != '\n') where++;
    return where;
}

void xmq_implementation::findLineAndColumn(const char *from, const char *where, int *line, int *col)
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
