/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef XMQ_CLI_JOIN_H
#define XMQ_CLI_JOIN_H

/*
  Join the command line arguments into a single line that is parsed with the
  xmq command line grammar xmqcli.ixml. The line starts with xmq (not argv[0]),
  then every argument follows separated by a single space.

  An argument that is empty or contains whitespace, a quote, a double quote or
  a backslash is written inside double quotes, where " and \ are escaped with
  a backslash. For an option with a value, --name=value, only the value is
  quoted (an empty value is not quoted).

  The Java version is org.libxmq.imp.CliJoin, the two must be identical.
  The returned string is malloced and must be freed by the caller.
*/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool xmq_cli_join_needs_quotes(const char *s, bool is_option_value)
{
    if (*s == 0) return !is_option_value;
    for (const char *i = s; *i; ++i)
    {
        char c = *i;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\'' || c == '"' || c == '\\') return true;
    }
    return false;
}

static char *xmq_cli_join_append_quoted(char *o, const char *s)
{
    *o++ = '"';
    for (const char *i = s; *i; ++i)
    {
        if (*i == '"' || *i == '\\') *o++ = '\\';
        *o++ = *i;
    }
    *o++ = '"';
    return o;
}

static char *xmq_cli_join_argv(int argc, const char **argv)
{
    // Worst case every char is escaped, plus quotes and a space per argument.
    size_t len = 4;
    for (int i = 1; i < argc; ++i) len += 2*strlen(argv[i]) + 3;
    char *out = (char*)malloc(len);
    char *o = out;
    memcpy(o, "xmq", 3);
    o += 3;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        *o++ = ' ';
        const char *eq = strchr(arg, '=');
        if (arg[0] == '-' && arg[1] == '-' && eq != NULL)
        {
            // An option --name=value, quote only the value.
            size_t n = eq-arg+1;
            memcpy(o, arg, n);
            o += n;
            if (xmq_cli_join_needs_quotes(eq+1, true)) o = xmq_cli_join_append_quoted(o, eq+1);
            else
            {
                size_t m = strlen(eq+1);
                memcpy(o, eq+1, m);
                o += m;
            }
        }
        else if (xmq_cli_join_needs_quotes(arg, false))
        {
            o = xmq_cli_join_append_quoted(o, arg);
        }
        else
        {
            size_t m = strlen(arg);
            memcpy(o, arg, m);
            o += m;
        }
    }
    *o = 0;
    return out;
}

#endif // XMQ_CLI_JOIN_H
