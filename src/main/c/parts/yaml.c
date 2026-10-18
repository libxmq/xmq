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

YAML is not a good configuratio nor data storage language at all.
In fact is is just horribly complicated, ambiguous and difficult to
read for a human (yes, that is ironic, since it was designed to be
human readable). https://ruuda.nl/2023/the-yaml-document-from-hell
A language with a bug so severe that it has its own name
"the Norway problem".

And it is so context-sensitive that it is impossible to create an ixml
grammar for it even for a predeclared subset of indentations!
https://lists.w3.org/Archives/Public/public-ixml/2026May/0015.html

Normally context-sensitive indents like python can be handled up to a
predeclared level of indentation in ixml. XMQ itself is
context-sensitive in regards to the quotes (the same number of quotes
start and end a quote).  Still the ixml grammar for xmq can handle up
to 5 quotes which covers most normal use cases.

Not so for yaml since the number of predclared rules for all possible
indentation levels inside a YAML file such as:
  - -   -       - alfa
        - beta
    - gamma
  - delta

The number of grammar rules expands with at least the square of all
possible indentations and levels, perhaps even worse, cubed.

Alas, there is still a lot of YAML in the world which I unfortunately
has to read, so if I can convert it to XMQ then my life will be better.
So I have added this C implementation of a YAML parser, that handles .
subset of the full YAML, enough to parse most normal configuration
files.

This is the reason for this C code.! I need to read YAML and my life
is better when I read it as pretty printed xmq in my browser.  I do
not recommend any use of YAML whatsever!

The implementation in this file gets around the Norway problem
simply but treating all content as text. The yaml file:
```
country: no
```

will turn into
```
_ {
    country = no
}
```

I.e. no will NOT be interpreted as a boolean, which happens in some
yaml implementation where Norway no turns into false.

Xmq does the same for numbers etc. i.e. all content is treated as text.
Xmq tracks the limited number of json types when going back and forth
to json. This is however impossible with yaml.

It is a basic block-style YAML parser that builds the xml DOM by
invoking the same parse callbacks (DO_CALLBACK_SIM) that the JSON
parser, parts/json.c, uses.

Supported:
   - nested mappings   key: value
   - nested sequences  - item
   - plain, single and double quoted scalars (double quotes support
     escapes including \uXXXX)
   - comments (# ...) and blank lines
   - flow collections [...] sequences and {...} mappings
     (single line, arbitrarily nested)
Not supported (reported as errors):
   - tab indentation, document marks ('---' and '...'),
     '%' directives, block scalars (| and >)
   - continuation entries of a map inside a sequence must be
     indented exactly at the column of the first entry.
*/

#include"yaml.h"

#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"stack.h"
#include"text.h"
#include"xmq_internals.h"
#include"xml.h"

#include<assert.h>
#include<stdbool.h>
#include<stddef.h>
#include<stdlib.h>
#include<string.h>

#endif

#ifdef YAML_MODULE

static const char y_underline[]  = "_";
static const char y_array[]      = "A";
static const char y_id[]         = "id";
static const char y_ref[]        = "ref";
static const char y_leftpar[]    = "(";
static const char y_rightpar[]   = ")";
static const char y_leftbrace[]  = "{";
static const char y_rightbrace[] = "}";

typedef struct y_parser
{
    char *buf;    // Private mutable copy of the input, lines NUL-terminated.
    char *cursor;
    char *line_start;
    char *text;    // First non-space char of the current line.
    size_t indent; // Distance from line_start to text.
    size_t line;
    bool eof;
    char *bend; // One past the last byte of the buffer (points at its NUL).
} y_parser;

static unsigned yhex(char c)
{
    if (c >= '0' && c <= '9') return (unsigned)(c - '0');
    if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A' + 10);
    return 0;
}

static int yspace(char c)
{
    return c == ' ' || c == '\t';
}

static void yfail(y_parser *p, XMQParseState *state, const char *msg)
{
    state->error_nr   = XMQ_ERROR_INVALID_CHAR;
    state->line       = p->line;
    state->col        = p->indent;
    // Point into the original buffer not the private copy.
    state->i          = state->buffer_start + (p->text - p->buf);
    state->error_info = msg;
}

/*
  Advance to the next significant line (skipping blank and comment
  lines). Returns 0 on success or 1 when an error is set in state.
*/
static int ynext(y_parser *p, XMQParseState *state)
{
    for (;;)
    {
        char *s  = p->cursor;
        char *e  = s;
        while (*e && *e != '\n') ++e;
        bool at_end = (*e == 0 && e == p->bend);

        *e = 0; // NUL-terminate the line (replaces the newline, if any).
        if (e > s && e[-1] == '\r') e[-1] = 0;
        if (!at_end) ++e; // Step over the newline.
        p->cursor = e;

        p->line_start = s;
        p->line++;
        p->text = s;
        while (*p->text == ' ') ++p->text;
        p->indent = (size_t)(p->text - s);

        if (*p->text == '\t')
        {
            yfail(p, state, "YAML: tab indentation is not supported");
            return 1;
        }
        if (*p->text == '%' ||
            (!strncmp(p->text, "---", 3) && (!p->text[3] || yspace(p->text[3]))) ||
            (!strncmp(p->text, "...", 3) && !p->text[3]))
        {
            yfail(p, state, "YAML: document markers and directives are not supported");
            return 1;
        }

        state->line = p->line;
        state->col  = p->indent;
        state->i    = state->buffer_start + (s - p->buf);

        if (*p->text && *p->text != '#')
        {            return 0;
        }
        if (at_end)
        {
            p->eof = true;
            return 0;
        }
        // A blank or comment line, keep looking.
    }
}

/*
  Read the next raw line (blank and comment lines included) into the
  private buffer. *line and *len point to the line's content.
  Returns 0 on success and 1 at end of input.
*/
static int yraw(y_parser *p, XMQParseState *state, const char **line, size_t *len)
{
    if (p->eof) return 1;

    char *s  = p->cursor;
    char *e  = s;
    while (*e && *e != '\n') ++e;
    // Note: committed lines are NUL-terminated in the middle of the
    // buffer, so EOF is only the final NUL at bend.
    bool at_end = (*e == 0 && e == p->bend);

    *e = 0; // NUL-terminate the line (replaces the newline, if any).
    if (e > s && e[-1] == '\r') e[-1] = 0;
    if (!at_end) ++e; // Step over the newline.
    p->cursor = e;

    p->line++;
    state->line = p->line;
    state->col  = 1;

    *line = s;
    *len  = strlen(s);
    if (at_end) p->eof = true; // Nothing more to read.
    return 0;
}

/*
  Return the colon in a "key: value" line (an unquoted colon followed
  by whitespace or end-of-line), or NULL.
*/
static const char *ycolon(const char *s)
{
    int sq = 0, dq = 0;

    while (*s)
    {
        char c = *s;
        switch (c)
        {
        case 0: return NULL;
        case ' ':
        case '\t': ++s; continue;
        case '#': return NULL;
        case '\'':
            if (sq && s[1] == '\'') { s += 2; continue; } // Escaped quote '' inside single quotes.
            sq ^= 1; ++s; continue;
        case '"': dq ^= 1; ++s; continue;
        case '\\':
            s += (s[1] != 0) ? 2 : 1; // The backslash escapes the next character (a lone trailing one just moves on).
            continue;
        case ':':
            if (!sq && !dq && (s[1] == ' ' || s[1] == '\t' || !s[1]))
            {
                return s;
            }
            ++s; // Not a separator, keep scanning.
            continue;
        }
        ++s;
    }
    return NULL;
}

static int ydash(const char *s)
{
    return s[0] == '-' && (s[1] == ' ' || s[1] == '\t');
}

/*
  Parse the scalar at s, returning the unescaped value (*out, *len).
  Rejects flow collections and block scalars. Returns 1 on error.
*/
static int yscalar(y_parser *p, XMQParseState *state,
                   const char *s, const char **out, size_t *len)
{
    // Note: for double quoted scalars, decoding is done in place starting
    // just after the opening quote (o = s + 1).
    char *o = (char*)s + (*s == '"');

    if (*s == '{' || *s == '[')
    {
        yfail(p, state, "YAML: flow style collections [ ] and { } are not supported");
        return 1;
    }
    if (*s == '|' || *s == '>')
    {
        yfail(p, state, "YAML: block scalars (| and >) are not supported");
        return 1;
    }

    switch (*s)
    {
    default:
        {
            const char *e = s;
            // A plain scalar runs to the end of the line or a comment
            // introduced by a '#' preceded by whitespace (or at line start).
            while (*e)
            {
                if (*e == '#' && (e == s || yspace(e[-1]))) break;
                ++e;
            }
            while (e > s && yspace(e[-1])) --e;
            *out = s;
            *len = (size_t)(e - s);
            return 0;
        }

    case '\'':
        {
            // Inside a single quoted scalar, a doubled '' is a literal
            // quote and anything else (up to the closing lone quote)
            // is plain text. No other escapes are allowed.
            char *o = (char*)s+1;
            const char *e = s+1;
            for (;;)
            {
                char c = *e;
                if (c == 0)
                {
                    yfail(p, state, "YAML: single quoted scalar is not closed");
                    return 1;
                }
                if (c == '\'')
                {
                    if (e[1] == '\'')
                    {
                        *o++ = '\'';
                        e += 2;
                        continue;
                    }
                    break;
                }
                *o++ = c;
                e++;
            }
            *out = s+1;
            *len = (size_t)((char*)o - (char*)(s+1));
            return 0;
        }

    case '"':
        {
            const char *e = s+1;
            for (;;)
            {
                char c = *e++;
                if (c == 0)
                {
                    yfail(p, state, "YAML: double quoted scalar is not closed");
                    return 1;
                }
                if (c == '"') break;
                if (c == '\\')
                {
                    char d = *e++;
                    if (d == 0)
                    {
                        yfail(p, state, "YAML: incomplete escape sequence");
                        return 1;
                    }
                    switch (d)
                    {
                    case 'b':  *o++ = 8;    break;
                    case 'f':  *o++ = 12;   break;
                    case 'n':  *o++ = '\n'; break;
                    case 'r':  *o++ = '\r'; break;
                    case 't':  *o++ = '\t'; break;
                    case '/':  *o++ = '/';  break;
                    case '"':  *o++ = '"';  break;
                    case '\\': *o++ = '\\'; break;
                    case 'u':
                        if (e[0] && e[1] && e[2] && e[3])
                        {
                            unsigned v = (yhex(e[0]) << 12) |
                                         (yhex(e[1]) << 8) |
                                         (yhex(e[2]) << 4) |
                                         yhex(e[3]);
                            e += 4;
                            if (v <= 0x7F)
                            {
                                *o++ = (char)v;
                            }
                            else if (v <= 0x7FF)
                            {
                                *o++ = (char)(0xC0 | ((v >> 6) & 31));
                                *o++ = (char)(0x80 | (v & 63));
                            }
                            else
                            {
                                *o++ = (char)(0xE0 | ((v >> 12) & 15));
                                *o++ = (char)(0x80 | ((v >> 6) & 63));
                                *o++ = (char)(0x80 | (v & 63));
                            }
                        }
                        else
                        {
                            yfail(p, state, "YAML: \\uXXXX needs 4 hex digits");
                            return 1;
                        }
                        break;
                    default:
                        *o++ = d; // Unknown escape, pass through.
                        break;
                    }
                    continue;
                }
                *o++ = c;
            }
            *out = s+1;
            *len = (size_t)((char*)o - (char*)(s+1));
            return 0;
        }
    }
}


static void ykey_attr(XMQParseState *state, const char *ak, size_t ak_len,
                      const char *v, size_t v_len)
{
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, ak, ak+ak_len, ak+ak_len);
    DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, v, v+v_len, v+v_len);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
}

static void ykey(XMQParseState *state, const char *k, const char *stop)
{
    const char *colon = NULL;
    const char *us    = NULL;
    const char *us_end = NULL;

    if (!k || k == stop)
    {
        k = y_underline;
        stop = y_underline+1;
    }
    else if (!is_xmq_element_name(k, stop, &colon))
    {
        us    = k;
        us_end = stop;
        k      = y_underline;
        stop   = y_underline+1;
    }

    // A namespaced key like the JSON parser.
    if (!us && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, k, colon, colon);
        k = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, k, stop, stop);
    if (us)
    {
        ykey_attr(state, y_underline, 1, us, (size_t)(us_end - us));
    }
}

/*
  Scan one entry of a flow collection starting at e. *entry and
  *entry_len are set to the extent of the entry and a pointer to the
  terminating ',' or the closing bracket is returned. NULL is
  returned when the collection is not closed on the line.
  Quotes (including '' and \\$ escapes) and nested flow collections
  are accounted for.
*/
static const char *yflow_entry(const char *e, const char **entry, size_t *entry_len)
{
    int depth = 0;
    int quote = 0; // 0 none, 1 single, 2 double.

    *entry = e;

    while (*e)
    {
        char c = *e;

        if (quote)
        {
            if (quote == 1)
            {
                if (c == '\'')
                {
                    if (e[1] == '\'') { e += 2; continue; }
                    quote = 0;
                }
            }
            else
            {
                if (c == '\\' && e[1]) { e += 2; continue; }
                if (c == '"') quote = 0;
            }
            e++;
            continue;
        }

        switch (c)
        {
        case 0:  return NULL;
        case ' ':
        case '\t':
            e++;
            continue;
        case '\'':
            quote = 1;
            e++;
            continue;
        case '"':
            quote = 2;
            e++;
            continue;
        case '[':
        case '{':
            depth++;
            e++;
            continue;
        case ']':
        case '}':
            // A bracket closing an inner collection just reduces the
            // depth, otherwise it ends the entry.
            if (depth)
            {
                depth--;
                e++;
                continue;
            }
            *entry_len = (size_t)(e - *entry);
            return e;
        case ',':
            if (!depth)
            {
                *entry_len = (size_t)(e - *entry);
                return e;
            }
            e++;
            continue;
        case '#':
            // A comment ends the entry (and the rest of the line).
            if (e == *entry || yspace(e[-1]))
            {
                *entry_len = (size_t)(e - *entry);
                return e;
            }
            e++;
            continue;
        }
        e++;
    }
    return NULL;
}

/*
  Parse a flow collection starting at s ('[' or '{') and emit the
  element structure directly. Nested flow collections of either kind
  are allowed but the collection must close on the current line.
  Returns 0 on success and 1 on failure.
*/
static int yflow(y_parser *p, XMQParseState *state, const char *s)
{
    if (*s == '[')
    {
        // A flow sequence: the caller has already emitted the element
        // key, this adds the (A) attribute and the entries.
        const char *e = s+1;
        int rc = 0;

        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

        for (;;)
        {
            const char *entry;
            size_t entry_len;
            const char *nxt;

            while (yspace(*e)) e++;
            if (*e == 0)
            {
                yfail(p, state, "YAML: a flow collection must close on the current line");
                rc = 1;
                break;
            }
            if (*e == ']') break;
            if (*e == ',') { e++; continue; } // Skip stray commas.

            nxt = yflow_entry(e, &entry, &entry_len);
            if (!nxt)
            {
                yfail(p, state, "YAML: a flow collection must close on the current line");
                rc = 1;
                break;
            }
            while (entry_len && yspace(entry[entry_len-1])) entry_len--;

            if (entry_len)
            {
                if (entry[0] == '[' || entry[0] == '{')
                {
                    // A nested collection is an entry of a sequence, so
                    // it needs its own '_' key (yflow adds the optional
                    // (A) for a nested sequence itself).
                    ykey(state, NULL, NULL);
                    if (yflow(p, state, entry)) { rc = 1; break; }
                }
                else if (entry[0] == '\'' || entry[0] == '"')
                {
                    const char *v;
                    size_t len;
                    if (yscalar(p, state, entry, &v, &len)) { rc = 1; break; }
                    ykey(state, NULL, NULL);
                    DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, v, v+len, v+len);
                }
                else
                {
                    ykey(state, NULL, NULL);
                    DO_CALLBACK_SIM(element_value_text, state, state->line, state->col,
                                   entry, entry+entry_len, entry+entry_len);
                }
            }

            if (*nxt != ',') break;
            e = nxt+1;
        }

        if (!rc)
        {
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        }
        return rc;
    }

    // A flow mapping.
    {
        const char *e = s+1;
        int rc = 0;

        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

        for (;;)
        {
            const char *nxt;

            while (yspace(*e)) e++;
            if (*e == 0)
            {
                yfail(p, state, "YAML: a flow collection must close on the current line");
                rc = 1;
                break;
            }
            if (*e == '}') break;
            if (*e == ',') { e++; continue; } // Skip stray commas.

            // A key runs to the first unquoted ':'.
            const char *c = e;
            int quote = 0;
            while (*c)
            {
                char ch = *c;
                if (quote == 1)
                {
                    if (ch == '\'')
                    {
                        if (c[1] == '\'') { c += 2; continue; }
                        quote = 0;
                    }
                }
                else if (quote == 2)
                {
                    if (ch == '\\' && c[1]) { c += 2; continue; }
                    if (ch == '"') quote = 0;
                }
                else if (ch == ':')
                {
                    break;
                }
                else if (ch == '\'')
                {
                    quote = 1;
                }
                else if (ch == '"')
                {
                    quote = 2;
                }
                c++;
            }
            if (!*c)
            {
                yfail(p, state, "YAML: a flow mapping entry needs the form 'key: value'");
                rc = 1;
                break;
            }

            const char *k = e, *ks = c;
            while (ks > k && yspace(ks[-1])) --ks;
            const char *v = c+1;
            const char *entry;
            size_t entry_len;

            while (yspace(*v)) v++;
            if (*v == 0)
            {
                yfail(p, state, "YAML: a flow collection must close on the current line");
                rc = 1;
                break;
            }

            ykey(state, ks > k ? k : NULL, ks > k ? ks : NULL);

            if (*v == '}')
            {
                // An empty value.
                DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
                break;
            }

            nxt = yflow_entry(v, &entry, &entry_len);
            if (!nxt)
            {
                yfail(p, state, "YAML: a flow collection must close on the current line");
                rc = 1;
                break;
            }
            while (entry_len && yspace(entry[entry_len-1])) entry_len--;

            if (entry_len)
            {
                if (entry[0] == '[' || entry[0] == '{')
                {
                    if (yflow(p, state, entry)) { rc = 1; break; }
                }
                else if (entry[0] == '\'' || entry[0] == '"')
                {
                    const char *sv;
                    size_t len;
                    if (yscalar(p, state, entry, &sv, &len)) { rc = 1; break; }
                    DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, sv, sv+len, sv+len);
                }
                else
                {
                    DO_CALLBACK_SIM(element_value_text, state, state->line, state->col,
                                   entry, entry+entry_len, entry+entry_len);
                }
            }
            else
            {
                // An empty value.
                DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
            }

            if (rc) break;
            if (*nxt != ',') break;
            e = nxt+1;
        }

        if (!rc)
        {
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        }
        return rc;
    }
}

/*
  Parse a literal block scalar. The header line ("key: |" or "- |")
  has been consumed and the parser is positioned after it; this reads
  the following raw lines (blank and comment lines included) that are
  blank or indented more than key_indent.
  `chomp` is 0 for clip (one trailing newline), '-' for strip (none)
  and '+' for keep (all). Emits the decoded value as the element
  value. Returns 0 on success and 1 on failure.
*/


/*
  Parse a | or > block scalar. `style` is '|' (literal, line breaks
  kept) or '>' (folded, line breaks become spaces except for blank,
  whitespace and indented lines, which keep their line breaks). `chomp`
  is 0 for clip (one trailing newline), '-' for strip (none) and '+'
  for keep (all). An explicit indent digit makes the block indent
  key_indent + digit; otherwise the block indent defaults to one space
  or the first content line, whichever is greater.
  Returns 0 on success and 1 on failure.
*/
static int yblock(y_parser *p, XMQParseState *state, char style, char chomp,
                  size_t key_indent, size_t digit)
{
    typedef struct { const char *ptr; size_t cnt; } yline;
    yline *lines = NULL;
    size_t nel = 0, cap = 0;
    size_t count = 0;
    size_t block_indent = digit ? key_indent + digit : 1;
    bool content_seen = false;

#define YB_PUSH(pp, cc) \
    do \
    { \
        if (nel == cap) \
        { \
            size_t nc = cap ? cap * 2 : 8; \
            yline *nl = (yline*)realloc(lines, nc * sizeof(*nl)); \
            if (!nl) { free(lines); return 1; } \
            lines = nl; cap = nc; \
        } \
        lines[nel].ptr = (pp); \
        lines[nel].cnt = (cc); \
        ++nel; \
    } while (0)

    while (!p->eof)
    {
        // Peek the next raw line without committing it, so that a line
        // that terminates the block is left for the caller.
        char *s = p->cursor;
        char *e = s;
        while (*e && *e != '\n') ++e;
        bool at_end = (*e == 0 && e == p->bend);
        size_t llen = (size_t)(e - s);
        if (e > s && e[-1] == '\r') --llen;

        size_t ind = 0;
        while (ind < llen && s[ind] == ' ') ++ind;

        bool blank = (e == s);
        bool spc   = (e > s && ind == llen && block_indent && ind >= block_indent);

        if (!blank && !spc && ind <= key_indent) break; // A terminator.

        // Commit the line: NUL-terminate it and advance the cursor.
        *(s + llen) = 0;
        p->cursor   = at_end ? e : e + 1;
        p->line++;
        state->line = p->line;
        state->col  = 1;
        if (at_end) p->eof = true;

        if (blank) { YB_PUSH(s, 0); continue; }
        if (spc)   { YB_PUSH(s, ind - block_indent); continue; }

        if (s[ind] == '\t')
        {
            yfail(p, state, "YAML: a tab is not allowed in block scalar indentation");
            free(lines);
            return 1;
        }
        if (!content_seen)
        {
            // Without an explicit indent digit the block indent is the
            // first content line's indent. With a digit the indent is
            // fixed and any extra indentation is kept in the content.
            if (!digit && ind > block_indent) block_indent = ind;
            content_seen = true;
        }
        if (ind < block_indent)
        {
            yfail(p, state, "YAML: a block scalar line is indented less than the block indent");
            free(lines);
            return 1;
        }
        YB_PUSH(s + block_indent, llen - block_indent);
    }

    count = nel;
    size_t sum = 0;
    for (size_t i = 0; i < count; ++i) sum += lines[i].cnt;
    size_t vlenmax = sum + 2 * count + 2;

    char *v = (char*)malloc(vlenmax);
    if (!v)
    {
        free(lines);
        return 1;
    }
    char *o = v;

    if (style == '|')
    {
        // A literal scalar: a line break before every non-first line.
        for (size_t i = 0; i < count; ++i)
        {
            if (i) *o++ = '\n';
            const char *line = lines[i].ptr;
            size_t cnt = lines[i].cnt;
            memcpy(o, line, cnt);
            o += cnt;
        }
    }
    else
    {
        // A folded scalar. A line break separates content lines, unless
        // the first or the following line is a content line that follows
        // a blank line or is an indented line, or the previous line was
        // a whitespace-only line.
        bool prev_blank = false, prev_ws = false, prev_ind = false;
        for (size_t i = 0; i < count; ++i)
        {
            const char *line = lines[i].ptr;
            size_t cnt = lines[i].cnt;

            bool allspace = true;
            for (size_t j = 0; j < cnt; ++j)
            {
                if (line[j] != ' ')
                {
                    allspace = false;
                    break;
                }
            }
            bool blnk     = (cnt == 0);
            bool ws       = (allspace && !blnk);
            bool indented = (!allspace && line[0] == ' ');

            if (blnk)
            {
                *o++ = '\n';
            }
            else if (ws)
            {
                if (i && !prev_blank) *o++ = '\n';
                *o++ = '\n';
                memcpy(o, line, cnt);
                o += cnt;
            }
            else if (i == 0 || prev_blank)
            {
                memcpy(o, line, cnt);
                o += cnt;
            }
            else if (prev_ws || indented || prev_ind)
            {
                *o++ = '\n';
                memcpy(o, line, cnt);
                o += cnt;
            }
            else
            {
                *o++ = ' ';
                memcpy(o, line, cnt);
                o += cnt;
            }

            prev_blank = blnk;
            prev_ws    = ws;
            prev_ind   = indented;
        }
    }

    size_t vlen = (size_t)(o - v);

    // Chomp: clip removes trailing line breaks and adds one, strip
    // removes them all, keep keeps them all.
    if (chomp != '+')
    {
        while (vlen && v[vlen-1] == '\n') --vlen;
        if (chomp == 0) ++vlen;
    }
    else
    {
        // Keep: a trailing blank line keeps its own line break too, which
        // the line joining above has folded away.
        size_t blanks = 0;
        for (size_t i = count; i > 0; --i)
        {
            if (lines[i-1].cnt) break;
            ++blanks;
        }
        for (size_t i = 0; i < blanks; ++i) *o++ = '\n';
        vlen = (size_t)(o - v);
    }

    free(lines);
    v[vlen] = 0;
    DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, v, v+vlen, v+vlen);
    free(v);
    return 0;
#undef YB_PUSH
}

static int ynode(y_parser *p, XMQParseState *state, const char *v, size_t key_indent);
static int ymap(y_parser *p, XMQParseState *state, size_t indent);
static int yitem(y_parser *p, XMQParseState *state, const char *dash, size_t indent);
static int yseq(y_parser *p, XMQParseState *state, size_t indent);

static int ynode(y_parser *p, XMQParseState *state,
                 const char *v, size_t key_indent)
{
    int rc = 0;

    if (*v && *v != '#')
    {
        if (*v == '|' || *v == '>')
        {
            // A literal or folded block scalar with an optional chomping
            // indicator and an optional indent digit, in any order.
            char chomp = 0, style = *v;
            size_t digit = 0;
            const char *r = v+1;
            if (*r == '-' || *r == '+') { chomp = *r; ++r; }
            if (*r >= '1' && *r <= '9') { digit = (size_t)(*r - '0'); ++r; }
            if (*r == '-' || *r == '+') { chomp = *r; ++r; }
            if (*r != 0 && *r != '#' && !yspace(*r))
            {
                yfail(p, state, "YAML: a block scalar header needs the form 'style' [chomp] [indent]");
                return 1;
            }
            if (yblock(p, state, style, chomp, key_indent, digit)) return 1;
            return ynext(p, state);
        }
        if (*v == '[' || *v == '{')
        {
            if (yflow(p, state, v)) return 1;
            return ynext(p, state);
        }
        const char *s;
        size_t len;
        if (yscalar(p, state, v, &s, &len)) return 1;
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
        return ynext(p, state);
    }

    if (ynext(p, state)) return 1;
    if (p->eof || (p->indent <= key_indent && !(ydash(p->text) && p->indent == key_indent)))
    {
        // Not more indented, so the value is empty. A dash at exactly the
        // same indentation as the key starts an indentless sequence.
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
        return 0;
    }

    if (ydash(p->text))
    {
        // key(A) { ... }
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = yseq(p, state, p->indent);
        if (!rc) DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        return rc;
    }

    // key { ... }
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
    rc = ymap(p, state, p->indent);
    if (!rc) DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    return rc;
}

static int yentry(y_parser *p, XMQParseState *state,
                  const char *k, const char *ks, const char *v,
                  size_t key_indent)
{
    ykey(state, k, ks);

    if (*v == '&' && v[1] != 0 && !yspace(v[1]))
    {
        // An anchor: remember the label as an (id = ..) attribute on
        // the key, then parse the value as usual.
        const char *lp = v + 1;
        size_t ll = 0;
        while (lp[ll] && !yspace(lp[ll]) && lp[ll] != '#') ++ll;
        ykey_attr(state, y_id, 2, lp, ll);
        v += 1 + ll;
        while (yspace(*v)) ++v;
        return ynode(p, state, v, key_indent);
    }

    if (*v == '*' && v[1] != 0 && !yspace(v[1]))
    {
        // An alias: remember the label as a (ref = ..) attribute on
        // the key. The referenced value is not expanded.
        const char *lp = v + 1;
        size_t ll = 0;
        while (lp[ll] && !yspace(lp[ll]) && lp[ll] != '#') ++ll;
        ykey_attr(state, y_ref, 3, lp, ll);
        return ynext(p, state);
    }

    return ynode(p, state, v, key_indent);
}

/*
  Parse a mapping node whose entries have indentation `indent`.
  Returns 0 on success and 1 on failure.
*/
static int ymap(y_parser *p, XMQParseState *state, size_t indent)
{
    while (!p->eof)
    {
        if (ydash(p->text) || p->indent != indent) break;
        const char *c = ycolon(p->text);
        if (!c)
        {
            yfail(p, state, "YAML: a mapping entry needs the form 'key: value'");
            return 1;
        }
        const char *k = p->text, *ks = c;
        while (ks > k && yspace(ks[-1])) --ks;
        const char *v = c+1;
        while (yspace(*v)) ++v;
        if (yentry(p, state, k, ks, v, indent)) return 1;
    }
    return 0;
}

/*
  Parse the sequence item that starts after the dash at `dash` on the
  current line. `indent` is the indentation of the enclosing sequence.
  Returns 0 on success and 1 on failure.
*/
static int yitem(y_parser *p, XMQParseState *state, const char *dash, size_t indent)
{
    const char *item = dash+1;
    while (yspace(*item)) ++item;
    int rc = 0;

    if (*item == 0 || *item == '#')
    {
        // An empty item.
        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
        return ynext(p, state);
    }

    if (*item == '[' || *item == '{')
    {
        // '- [a, b]' or '- {k: v}'
        ykey(state, NULL, NULL);
        if (yflow(p, state, item)) return 1;
        return ynext(p, state);
    }

    if (ydash(item))
    {
        // A nested sequence: '- - x' with continuation items at the
        // inner dash's column.
        const size_t inner_indent = (size_t)(item - p->line_start);

        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

        if (yitem(p, state, item, inner_indent))
        {
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
            return 1;
        }
        rc = yseq(p, state, inner_indent);
        DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        return rc;
    }

    const char *c = ycolon(item);
    if (!c)
    {
        // '- scalar' or '- |'
        if (item[0] == '|' || item[0] == '>')
        {
            char chomp = 0, style = item[0];
            size_t digit = 0;
            const char *r = item+1;
            if (*r == '-' || *r == '+') { chomp = *r; ++r; }
            if (*r >= '1' && *r <= '9') { digit = (size_t)(*r - '0'); ++r; }
            if (*r == '-' || *r == '+') { chomp = *r; ++r; }
            if (*r != 0 && *r != '#' && !yspace(*r))
            {
                yfail(p, state, "YAML: a block scalar header needs the form 'style' [chomp] [indent]");
                return 1;
            }
            ykey(state, NULL, NULL);
            if (yblock(p, state, style, chomp, indent, digit)) return 1;
            return ynext(p, state);
        }
        const char *s;
        size_t len;
        if (yscalar(p, state, item, &s, &len)) return 1;
        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
        return ynext(p, state);
    }

    // A mapping item: _ { entries }
    const size_t item_indent = (size_t)(item - p->line_start);

    ykey(state, NULL, NULL);
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

    const char *k = item, *ks = c;
    while (ks > k && yspace(ks[-1])) --ks;
    const char *v = c+1;
    while (yspace(*v)) ++v;

    rc = yentry(p, state, k, ks, v, indent);
    if (rc)
    {
        DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        return 1;
    }

    // Continuation entries of the same mapping item.
    while (!p->eof && !ydash(p->text) && p->indent == item_indent)
    {
        const char *c2 = ycolon(p->text);
        if (!c2)
        {
            yfail(p, state, "YAML: a mapping entry needs the form 'key: value'");
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
            return 1;
        }
        const char *k2 = p->text, *ks2 = c2;
        while (ks2 > k2 && yspace(ks2[-1])) --ks2;
        const char *v2 = c2+1;
        while (yspace(*v2)) ++v2;
        rc = yentry(p, state, k2, ks2, v2, indent);
        if (rc)
        {
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
            return 1;
        }
    }

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    return 0;
}

/*
  Parse a sequence node whose dashes have indentation `indent`.
  Returns 0 on success and 1 on failure.
*/
static int yseq(y_parser *p, XMQParseState *state, size_t indent)
{
    while (!p->eof && ydash(p->text) && p->indent == indent)
    {
        if (yitem(p, state, p->text, indent)) return 1;
    }
    return 0;
}

/*
  Parse the whole document at top level. Returns true on success and
  false on failure.
*/
static bool yparse_document(y_parser *p, XMQParseState *state)
{
    int rc = 0;

    if (ynext(p, state)) return false;
    if (p->eof)
    {
        // An empty document.
        return true;
    }

    if (ydash(p->text))
    {
        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = yseq(p, state, p->indent);
        if (!rc) DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    }
    else if (ycolon(p->text))
    {
        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = ymap(p, state, p->indent);
        if (!rc) DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    }
    else
    {
        // A scalar document.
        const char *s;
        size_t len;
        if (yscalar(p, state, p->text, &s, &len)) return false;
        ykey(state, NULL, NULL);
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
        rc = ynext(p, state);
    }

    if (rc) return false;
    if (!p->eof)
    {
        yfail(p, state, "YAML: unexpected content at top level indentation");
        return false;
    }
    return true;
}

/*
  Tokenize a YAML buffer by simulating the xmq tokenizer. Mirrors
  xmq_tokenize_buffer_json in parts/json.c.
*/
bool xmq_tokenize_buffer_yaml(XMQParseState *state, const char *start, const char *stop)
{
    if (!stop) stop = start+strlen(start);
    size_t len = (size_t)(stop - start);

    y_parser p;
    memset(&p, 0, sizeof(p));
    p.buf    = (char*)malloc(len+1);
    if (!p.buf)
    {
        state->error_nr = XMQ_ERROR_OOM;
        return false;
    }
    memcpy(p.buf, start, len);
    p.buf[len] = 0;
    p.cursor   = p.buf;
    p.bend     = p.buf + len;

    state->buffer_start = start;
    state->buffer_stop  = stop;
    state->i            = start;
    state->line         = 1;
    state->col          = 1;

    if (state->parse->init) state->parse->init(state);

    bool ok = yparse_document(&p, state);
    if (!ok || state->error_nr != XMQ_OK)
    {
        generate_state_error_message(state, state->error_nr, start, stop);
    }

    if (ok && state->error_nr == XMQ_OK && state->parse->done)
    {
        state->parse->done(state);
    }

    free(p.buf);
    return ok && state->error_nr == XMQ_OK;
}

bool xmq_parse_buffer_yaml(XMQDoc *doq,
                           const char *start,
                           const char *stop,
                           const char *implicit_root)
{
    bool rc = true;

    if (!stop) stop = start+strlen(start);

    XMQOutputSettings *os = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();
    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, os);
    state->magic_cookie   = MAGIC_COOKIE;
    state->no_trim_quotes = true;
    state->doq            = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;
    state->implicit_root = implicit_root;

    stack_push(state->element_stack, doq->docptr_.xml);
    state->element_last = NULL;

    xmq_tokenize_buffer_yaml(state, start, stop);

    if (xmqStateErrno(state))
    {
        rc     = false;
        doq->errno_ = xmqStateErrno(state);
        doq->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(os);

    return rc;
}

#endif // YAML_MODULE
