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
static const char y_tag[]        = "tag";
static const char y_ltag[]       = "ltag";
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
    struct ycomment *cmts; // Buffered comment lines awaiting placement.
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
  Emit an xmq comment node from a yaml comment body (the text after '#',
  already whitespace-trimmed). xmq renders it as "// <body>".
*/
static void yemit_comment(XMQParseState *state, const char *s, size_t len)
{
    while (len && (s[0] == ' ' || s[0] == '\t')) { s++; len--; }
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;
    if (len == 0) return;
    DO_CALLBACK_SIM(comment, state, state->line, state->col, s, s + len, s + len);
}

/*
  Comments are held in a small list, tagged with the indentation of the line
  they sat on, and emitted at the nearest structurally-valid point (a block's
  top or its close) once its brace is open and the block is on top of the
  element stack. do_comment parents the node onto element_stack->top.
*/
struct ycomment
{
    size_t indent;
    char *text;
    size_t len;
    struct ycomment *next;
};

static char *ycopy(const char *s, size_t len)
{
    char *o = (char *)malloc(len + 1);
    if (o) { memcpy(o, s, len); o[len] = 0; }
    return o;
}

/* Buffer a full-line comment for later placement. */
static void ybuf_comment(y_parser *p, size_t indent, const char *s, size_t len)
{
    if (!len) return;
    struct ycomment *c = (struct ycomment *)malloc(sizeof *c);
    if (!c) return;
    c->indent = indent;
    c->text   = ycopy(s, len);
    if (!c->text) { free(c); return; }
    c->len = len;
    c->next = NULL;
    struct ycomment **tail = &p->cmts;
    while (*tail) tail = &(*tail)->next;
    *tail = c;
}

/*
  Emit (into the current element-stack top) and drop every buffered comment
  whose indentation equals `indent`; the rest stay buffered.
*/
static void yflush_comments(y_parser *p, XMQParseState *state, size_t indent)
{
    struct ycomment **prev = &p->cmts;
    while (*prev)
    {
        struct ycomment *c = *prev;
        if (c->indent == indent)
        {
            yemit_comment(state, c->text, c->len);
            *prev = c->next;
            free(c->text);
            free(c);
        }
        else
        {
            prev = &c->next;
        }
    }
}

/*
  Re-tag every buffered comment to `indent`, so a leading comment is placed at
  the top of a nested block rather than held back at the parent's indent.
*/
static void yretag_comments(y_parser *p, size_t indent)
{
    for (struct ycomment *c = p->cmts; c; c = c->next) c->indent = indent;
}

/* Free any buffered comments (parse failure or end of document). */
static void yfree_comments(y_parser *p)
{
    struct ycomment *c = p->cmts;
    while (c)
    {
        struct ycomment *n = c->next;
        free(c->text);
        free(c);
        c = n;
    }
    p->cmts = NULL;
}

/*
  Scan a yaml line for a trailing ' #' comment: a '#' that is the first
  character, or is preceded by whitespace, and is not inside a quoted
  scalar. Returns a pointer to the '#', or NULL when there is no comment.
*/
static const char *yclfind_comment(const char *line)
{
    char q = 0;
    for (const char *i = line; *i; i++)
    {
        char c = *i;
        if (q == '\'')
        {
            if (c == '\'')
            {
                if (i[1] == '\'') i++; // Escaped '' , a literal quote.
                else q = 0;            // Closing quote.
            }
        }
        else if (q == '"')
        {
            if (c == '\\' && i[1]) i++; // Escaped char.
            else if (c == '"') q = 0;   // Closing quote.
        }
        else
        {
            if (c == '\'' || c == '"') q = c;
            else if (c == '#' && (i == line || i[-1] == ' ' || i[-1] == '\t')) return i;
        }
    }
    return NULL;
}

/*
  If the current line carries a trailing inline comment, emit it as an
  xmq comment node.
*/
static void yinline(y_parser *p, XMQParseState *state)
{
    const char *c = yclfind_comment(p->text);
    if (c) yemit_comment(state, c + 1, strlen(c + 1));
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
        if (p->text[0] == '#')
        {
            // A full-line comment: buffer it (with the line's indentation)
            // for placement at the nearest structurally-valid point.
            // Emitting it here would place it before the element that owns
            // it, so defer until that element is on top of the stack.
            ybuf_comment(p, p->indent, p->text + 1, strlen(p->text + 1));
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
    // A dash followed by a space, or a lone dash at end of line, starts
    // a sequence entry.
    return s[0] == '-' && (s[1] == ' ' || s[1] == '\t' || s[1] == 0);
}

/*
  If v starts with a '!' it is a YAML tag (global after '!!', local
  after a single '!'). It is emitted as a (tag = ..) or (ltag = ..)
  attribute on the current key and the value past the tag is
  returned. (Defined further down.)
*/
static const char *ytag(XMQParseState *state, const char *v);

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
  Return the number of raw source characters used by a closed quoted
  scalar starting at s (both quotes included), or 0 if the scalar is
  not closed on the current line.
*/
static size_t
yquote_raw(const char *s)
{
    const char *e = s+1;

    if (*s == '\'')
    {
        char c;

        for (;;)
        {
            c = *e;
            if (!c) { return 0; }
            if (c == '\'')
            {
                if (e[1] == '\'') { e += 2; continue; }
                return (size_t)(e - s) + 1;
            }
            e++;
        }
    }

    for (;;)
    {
        if (!*e) { return 0; }
        if (*e == '\\')
        {
            if (!e[1]) { return 0; }
            e += 2;
            continue;
        }
        if (*e == '"') { return (size_t)(e - s) + 1; }
        e++;
    }
}

/* A forward declaration so that the two can call each other. */
static int
yflow(y_parser *p, XMQParseState *state, const char *s, const char **e_after);

/* Forward declarations (defined further down). */
static bool ylooks_like_native(const char *s, size_t len);
static void yforce_string(XMQParseState *state);

/*
  Parse one flow entry value starting at e. A leading tag becomes a
  (tag/ltag) attribute. The value is a nested flow collection (which
  may itself span several lines), a quoted scalar closed on the
  current line, or a plain scalar running to the next top-level ',',
  ']' or '}'. On success *e_after is set to the first character after
  the value. Returns 0 on success and 1 on failure.
*/
static int
yflow_value(y_parser *p, XMQParseState *state, const char *e, const char **e_after)
{
    const char *v;
    size_t len;

    e = ytag(state, e);

    if (*e == '[' || *e == '{')
    {
        const char *after;
        if (yflow(p, state, e, &after)) { return 1; }
        *e_after = after;
        return 0;
    }

    if ((*e == '\'') || (*e == '"'))
    {
        const char *after;

        if (yscalar(p, state, e, &v, &len)) { return 1; }

        after = e + yquote_raw(e);
        if (after <= e)
        {
            // yscalar closed the scalar, so this should not happen.
            yfail(p, state, "YAML: a quoted scalar in a flow collection is not closed");
            return 1;
        }

        if (ylooks_like_native(v, len))
        {
            yforce_string(state);
        }
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, v, v+len, v+len);
        *e_after = after;
        return 0;
    }

    // A plain scalar runs to the next top-level ',', ']' or '}'.
    {
        const char *e2 = e;
        int quote = 0;

        while (*e2)
        {
            char c = *e2;
            if (quote == 1)
            {
                if (c == '\'')
                {
                    if (e2[1] == '\'') { e2 = e2+2; continue; }
                    quote = 0;
                }
            }
            else if (quote == 2)
            {
                if ((c == '\\') && e2[1]) { e2 = e2+2; continue; }
                if (c == '"') { quote = 0; }
            }
            else if (c == '\'') { quote = 1; }
            else if (c == '"') { quote = 2; }
            else if (c == ',') { break; }
            else if ((c == ']') || (c == '}')) { break; }
            e2++;
        }

        if (quote)
        {
            yfail(p, state, "YAML: a quoted scalar in a flow collection is not closed");
            return 1;
        }

        while ((e2 > e) && yspace(e2[-1])) e2--;
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, e, e2, e2);
        *e_after = e2;
        return 0;
    }
}

/*
  Emit a flow-mapping key element from the raw key range k..ke (both
  within the current line buffer). A leading global ('!!') or local ('!')
  tag is captured and emitted as a (tag = ..) / (ltag = ..) attribute on the
  key after the element name; a quoted key is decoded. This mirrors how the
  key/value tags are attached: the element is created first, so any tag
  attribute lands on it. A plain key keeps its (trimmed) raw text.
  Returns 0 on success and 1 on failure.
*/
static int
yflow_key(y_parser *p, XMQParseState *state, const char *k, const char *ke)
{
    const char    *tk      = k;
    const char    *tagname = NULL;
    size_t         taglen  = 0;
    bool           global  = false;

    if ((*tk == '!') && tk[1])
    {
        tagname = (tk[1] == '!') ? tk+2 : tk+1;
        global  = (tk[1] == '!');
        const char *end = tagname;
        while (*end && (end < ke) && !yspace(*end) && (*end != '#')
               && (*end != ',') && (*end != '[') && (*end != ']')
               && (*end != '{') && (*end != '}'))
        {
            ++end;
        }
        if (end != tagname)
        {
            taglen = (size_t)(end - tagname);
            while (yspace(*end)) { ++end; }
            tk = end;
        }
        else
        {
            tagname = NULL; // A bare '!' with no tag name: plain text.
        }
    }

    const char  *ktxt;
    size_t       klen;

    if ((*tk == '\'') || (*tk == '"'))
    {
        if (yscalar(p, state, tk, &ktxt, &klen) != 0) { return 1; }
    }
    else
    {
        ktxt = tk;
        klen = (size_t)(ke - tk);
    }

    if (klen) { ykey(state, ktxt, ktxt+klen); }
    else      { ykey(state, NULL, NULL); }

    if (tagname)
    {
        const char *ak     = global ? y_tag : y_ltag;
        const size_t ak_len = global ? 3u    : 4u;
        ykey_attr(state, ak, ak_len, tagname, taglen);
    }
    return 0;
}

/*
  Parse a flow collection starting at s ('[' or '{') and emit the
  element structure directly. Nested flow collections of either kind
  are allowed, and a collection may span several lines: if the current
  line ends before the collection is closed, the scan resumes on the
  next line (blank and comment lines are skipped by ynext()). Quoted
  scalars still have to close on the current line.
  On success *e_after is set to the first character after the matching
  closing bracket and the parser is positioned at the line that holds
  it. Returns 0 on success and 1 on failure.
*/
static int
yflow(y_parser *p, XMQParseState *state, const char *s, const char **e_after)
{
    const bool   is_seq  = (*s == '[');
    const char   closeb  = is_seq ? ']' : '}';
    const char  *e = s+1;
    int rc = 0;

    if (is_seq)
    {
        // A flow sequence gets the (A) attribute; the entries are
        // keyed by this loop.
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
    }
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

    for (;;)    {
        // Skip blanks; if this line ends before the collection is
        // closed, resume the scan on the next one.
        for (;;)
        {
            while (yspace(*e)) e++;
            if (*e) { break; }
            if (ynext(p, state)) { rc = 1; goto out; }
            if (p->eof)
            {
                yfail(p, state, "YAML: a flow collection is not closed");
                rc = 1;
                goto out;
            }
            e = p->text;
        }

        if (*e == closeb)
        {
            *e_after = e+1;
            break;
        }

        if (*e == ',')
        {
            // Skip stray commas.
            e++;
            continue;
        }

        if (is_seq)
        {
            ykey(state, NULL, NULL);
            {
                const char *after;
                if (yflow_value(p, state, e, &after)) { rc = 1; goto out; }
                e = after;
            }
            continue;
        }

        // A flow mapping entry: either a plain 'key: value' pair (the key
        // runs to the first unquoted ':') or an explicit one introduced by
        // '?', whose ': value' part may be on the next line:
        //
        //     { ? key : value }
        //     {
        //       ? key
        //       : value
        //     }
        // A leading tag ('!!'/'!') on the key and a quoted key are both
        // supported; the tag becomes a (tag = ..)/(ltag = ..) attribute
        // on the key and a quoted key is decoded.
        {
            const char *e0 = e;
            bool explicit_key = false;

            if ((*e0 == '?') && (!e0[1] || yspace(e0[1])))
            {
                explicit_key = true;
                e0++;
                while (yspace(*e0)) { e0++; }
            }

            // Locate the ':' separating key and value on this line, if any.
            const char *c = e0;
            int    quote = 0;

            while (*c)
            {
                char ch = *c;
                if (quote == 1)
                {
                    if (ch == '\'')
                    {
                        if (c[1] == '\'') { c = c+2; continue; }
                        quote = 0;
                    }
                }
                else if (quote == 2)
                {
                    if ((ch == '\\') && c[1]) { c = c+2; continue; }
                    if (ch == '"') { quote = 0; }
                }
                else if (ch == ':') { break; }
                else if (ch == '\'') { quote = 1; }
                else if (ch == '"') { quote = 2; }
                c++;
            }
            const bool colon_here = (*c != 0);

            const char *ks = c;
            while ((ks > e0) && yspace(ks[-1])) { ks--; }

            if (colon_here)
            {
                if (explicit_key)
                {
                    if (yflow_key(p, state, e0, ks)) { rc = 1; goto out; }
                }
                else
                {
                    ykey(state, (ks > e0) ? e0 : NULL, (ks > e0) ? ks : NULL);
                }
                e = c+1;
            }
            else
            {
                // No ':' on this line.
                if (!explicit_key)
                {
                    yfail(p, state, "YAML: a flow mapping entry needs the form 'key: value'");
                    rc = 1;
                    goto out;
                }
                // The key is the rest of the line; ':' and the value follow.
                if (yflow_key(p, state, e0, ks)) { rc = 1; goto out; }
                if (ynext(p, state)) { rc = 1; goto out; }
                if (p->eof)
                {
                    yfail(p, state, "YAML: a flow collection is not closed");
                    rc = 1;
                    goto out;
                }
                e = p->text;
                if (e[0] != ':')
                {
                    yfail(p, state, "YAML: an explicit flow key must be followed by a ':'");
                    rc = 1;
                    goto out;
                }
                e++;
            }

            while (yspace(*e)) e++;

            if ((*e == closeb) || (*e == ','))
            {
                // An empty value.
                DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
                if (*e == closeb)
                {
                    *e_after = e+1;
                    break;
                }
                continue;
            }

            if (!*e)
            {
                // An empty value; the collection may continue on the
                // next line.
                DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
                if (ynext(p, state)) { rc = 1; goto out; }
                if (p->eof)
                {
                    yfail(p, state, "YAML: a flow collection is not closed");
                    rc = 1;
                    goto out;
                }
                e = p->text;
                continue;
            }

            {
                const char *after;
                if (yflow_value(p, state, e, &after)) { rc = 1; goto out; }
                e = after;
            }
            continue;
        }
    }

out:
    if (!rc)
    {
        DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    }
    return rc;
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

/*
  If v starts with a '!' it is a YAML tag: a '!!' leads a global tag,
  a single '!' a local one. The tag is emitted as a (tag = ..) or
  (ltag = ..) attribute on the current key and the value with the
  tag skipped (and following spaces trimmed) is returned. A '!' that
  is not followed by a tag name is plain scalar text and is given
  back unchanged.
*/
static const char *ytag(XMQParseState *state, const char *v)
{
    if (*v != '!' || !v[1])
    {
        return v;
    }

    const bool     global = (v[1] == '!');
    const char    *name   = (global) ? v+2 : v+1;
    const char    *end    = name;

    while (*end && !yspace(*end) && *end != '#' && *end != ',' && *end != '[' && *end != ']' && *end != '{' && *end != '}') ++end;

    if (end == name)
    {
        return v; // A bare '!' with no name, plain scalar text.
    }

    const char *ak       = (global) ? y_tag  : y_ltag;
    const size_t ak_len  = (global) ? 3u     : 4u;

    ykey_attr(state, ak, ak_len, name, (size_t)(end - name));
    while (yspace(*end)) ++end;
    return end;
}

/*
  Return true when the whole quoted scalar decodes to text that would
  otherwise be interpreted as a boolean (true/false), a null or a number
  by downstream formatters (the same rule the JSON parser applies to
  strings that look like JSON literals). The string must equal one of
  these exactly; no other YAML types are recognised.
*/
static bool ylooks_like_native(const char *s, size_t len)
{
    if ((len == 4) && !memcmp(s, "true", 4)) { return true; }
    if ((len == 5) && !memcmp(s, "false", 5)) { return true; }
    if ((len == 4) && !memcmp(s, "null", 4)) { return true; }

    // A number: [+-] digits [ . digits ] [ [eE] [+-] digits ]
    size_t i = 0;
    if ((i < len) && ((s[i] == '+') || (s[i] == '-'))) { ++i; }
    bool any_digit = false;
    while ((i < len) && (s[i] >= '0') && (s[i] <= '9'))
    {
        any_digit = true;
        ++i;
    }
    if ((i < len) && (s[i] == '.'))
    {
        ++i;
        while ((i < len) && (s[i] >= '0') && (s[i] <= '9'))
        {
            any_digit = true;
            ++i;
        }
    }
    if (!any_digit) { return false; }
    if ((i < len) && ((s[i] == 'e') || (s[i] == 'E')))
    {
        ++i;
        if ((i < len) && ((s[i] == '+') || (s[i] == '-'))) { ++i; }
        bool exp_digit = false;
        while ((i < len) && (s[i] >= '0') && (s[i] <= '9'))
        {
            exp_digit = true;
            ++i;
        }
        if (!exp_digit) { return false; }
    }
    return (i == len);
}

/*
  Emit the (S) force-string attribute on the pending element so that a
  quoted scalar keeping its exact text (true, false, null or a number)
  survives a roundtrip as a string.
*/
static void yforce_string(XMQParseState *state)
{
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, "S", "S"+1, "S"+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
}

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
            const char *after;
            // `after` is deliberately unused: what follows the close
            // bracket on the line is discarded, as before.
            if (yflow(p, state, v, &after)) { return 1; }
            return ynext(p, state);
        }
        const char *s;
        size_t len;
        if (yscalar(p, state, v, &s, &len)) return 1;
        if ((v[0] == '"') || (v[0] == '\''))
        {
            if (ylooks_like_native(s, len))
            {
                yforce_string(state);
            }
        }
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
        yinline(p, state);
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
        const size_t seq_indent = p->indent;
        yretag_comments(p, seq_indent);
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = yseq(p, state, seq_indent);
        if (!rc)
        {
            yflush_comments(p, state, seq_indent);
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        }
        return rc;
    }

    // key { ... }
    const size_t map_indent = p->indent;
    yretag_comments(p, map_indent);
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
    rc = ymap(p, state, map_indent);
    if (!rc)
    {
        yflush_comments(p, state, map_indent);
        DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
    }
    return rc;
}

static int yentry(y_parser *p, XMQParseState *state,
                  const char *k, const char *ks, const char *v,
                  size_t key_indent)
{
    ykey(state, k, ks);

    v = ytag(state, v);

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
    yflush_comments(p, state, indent);
    while (!p->eof)
    {
        if (ydash(p->text) || p->indent != indent) break;
        // A comment sitting between two keys (read while parsing the
        // previous value) is emitted right before the key it precedes.
        yflush_comments(p, state, indent);
        if (p->text[0] == '?' && (p->text[1] == 0 || yspace(p->text[1])))
        {
            // A complex key (single-pair form):
            //   ? key
            //   : value
            // The key is decoded here; ykey() keeps it as the element
            // name when it is a valid name and only then falls back
            // to the underscore form with a (_ = ..) attribute, the
            // same as the JSON conversion.
            const char *kk = p->text + 1;
            while (yspace(*kk)) ++kk;
            const char *key = NULL;
            size_t key_len = 0;
            if (yscalar(p, state, kk, &key, &key_len)) return 1;

            if (ynext(p, state)) return 1;
            if (p->text[0] != ':' || (p->text[1] != 0 && !yspace(p->text[1])))
            {
                yfail(p, state, "YAML: a complex key must be followed by a ':' line with the value");
                return 1;
            }
            const char *v = p->text + 1;
            while (yspace(*v)) ++v;
            if (yentry(p, state, key, key + key_len, v, indent)) return 1;
            continue;
        }

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
  Emit the body of a mapping item whose first entry is at `first` on
  the current line: the opening brace, the entries, and the closing
  brace. The caller has already emitted the item key (with any
  attributes). `indent` is the indentation of the enclosing sequence.
  Returns 0 on success and 1 on failure.
*/
static int yitem_map(y_parser *p, XMQParseState *state,
                     const char *first, size_t indent)
{
    const char *c = ycolon(first);
    if (!c)
    {
        yfail(p, state, "YAML: a mapping entry needs the form 'key: value'");
        return 1;
    }
    const size_t item_indent = (size_t)(first - p->line_start);
    int rc = 0;

    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);

    const char *k = first, *ks = c;
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
        yflush_comments(p, state, item_indent);
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

    yflush_comments(p, state, item_indent);
    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
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
        // No value on this line: either an empty item, or an item
        // whose node value continues on more deeply indented lines
        // below the bare dash.
        ykey(state, NULL, NULL);
        if (ynext(p, state)) { return 1; }
        if (p->eof || p->indent <= indent)
        {
            // An empty item.
            DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
            return 0;
        }
        if (ydash(p->text))
        {
            // A nested sequence below the bare dash.
            DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
            DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
            DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
            rc = yseq(p, state, p->indent);
            if (!rc)
            {
                DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
            }
            return rc;
        }
        if (ycolon(p->text))
        {
            // A mapping whose first entry is on the next line.
            return yitem_map(p, state, p->text, indent);
        }
        // A plain line with no key: treat the item as empty and let
        // the outer level parse the line.
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, "", "", "");
        return 0;
    }

    if (*item == '&' && item[1] != 0 && !yspace(item[1]))
    {
        // An anchor: '- &label [value]'. The anchor becomes an
        // (id = ..) attribute on the item key; the value may be on
        // this line or on more deeply indented lines below.
        const char *lp = item + 1;
        size_t ll = 0;
        while (lp[ll] && !yspace(lp[ll]) && lp[ll] != '#') ++ll;

        ykey(state, NULL, NULL);
        ykey_attr(state, y_id, 2, lp, ll);

        item += 1 + ll;
        while (yspace(*item)) ++item;

        if (*item && *item != '#')
        {
            // A value on the rest of this line.
            if (*item == '[' || *item == '{')
            {
                const char *after;
                if (yflow(p, state, item, &after)) { return 1; }
                return ynext(p, state);
            }
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
                if (yblock(p, state, style, chomp, indent, digit)) { return 1; }
                return ynext(p, state);
            }
            if (ycolon(item))
            {
                // An inline mapping: '- &x name: 1'
                return yitem_map(p, state, item, indent);
            }
            const char *s;
            size_t len;
            item = ytag(state, item);
            if (yscalar(p, state, item, &s, &len)) { return 1; }
            if (((item[0] == '"') || (item[0] == '\'')) && ylooks_like_native(s, len))
            {
                yforce_string(state);
            }
            DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
            yinline(p, state);
            return ynext(p, state);
        }

        // No value on this line: the item is empty or continues on
        // more deeply indented lines.
        if (ynext(p, state)) { return 1; }
        if (p->eof || p->indent <= indent)
        {
            // An empty item (the attribute is already emitted).
            return 0;
        }
        if (ydash(p->text))
        {
            // A nested sequence below the anchor.
            DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
            DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
            DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
            rc = yseq(p, state, p->indent);
            if (!rc)
            {
                DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
            }
            return rc;
        }
        if (ycolon(p->text))
        {
            // A mapping whose first entry is on the next line.
            return yitem_map(p, state, p->text, indent);
        }
        // A plain line with no key: treat the item as empty and let
        // the outer level parse the line.
        return 0;
    }

    if (*item == '*' && item[1] != 0 && !yspace(item[1]))
    {
        // An alias: '- *label'. The alias becomes a (ref = ..)
        // attribute on the item key; the referenced value is not
        // expanded.
        const char *lp = item + 1;
        size_t ll = 0;
        while (lp[ll] && !yspace(lp[ll]) && lp[ll] != '#') ++ll;

        ykey(state, NULL, NULL);
        ykey_attr(state, y_ref, 3, lp, ll);

        item += 1 + ll;
        while (yspace(*item)) ++item;
        if (*item && *item != '#')
        {
            yfail(p, state, "YAML: an alias must stand alone");
            return 1;
        }
        return ynext(p, state);
    }

    if (*item == '[' || *item == '{')
    {
        // '- [a, b]' or '- {k: v}'
        ykey(state, NULL, NULL);
        {
            const char *after;
            // `after` is deliberately unused: what follows the close
            // bracket on the line is discarded, as before.
            if (yflow(p, state, item, &after)) { return 1; }
        }
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
        ykey(state, NULL, NULL);
        item = ytag(state, item);
        if (yscalar(p, state, item, &s, &len)) return 1;
        if (((item[0] == '"') || (item[0] == '\'')) && ylooks_like_native(s, len))
        {
            yforce_string(state);
        }
        DO_CALLBACK_SIM(element_value_text, state, state->line, state->col, s, s+len, s+len);
        yinline(p, state);
        return ynext(p, state);
    }

    // A mapping item: '- name: 1'
    ykey(state, NULL, NULL);
    return yitem_map(p, state, item, indent);
}

/*
  Parse a sequence node whose dashes have indentation `indent`.
  Returns 0 on success and 1 on failure.
*/
static int yseq(y_parser *p, XMQParseState *state, size_t indent)
{
    yflush_comments(p, state, indent);
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
        const size_t ti = p->indent;
        ykey(state, NULL, NULL);
        yretag_comments(p, ti);
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, y_leftpar, y_leftpar+1, y_leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, y_array, y_array+1, y_array+1);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, y_rightpar, y_rightpar+1, y_rightpar+1);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = yseq(p, state, ti);
        if (!rc)
        {
            yflush_comments(p, state, ti);
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        }
    }
    else if (ycolon(p->text))
    {
        const size_t ti = p->indent;
        ykey(state, NULL, NULL);
        yretag_comments(p, ti);
        DO_CALLBACK_SIM(brace_left, state, state->line, state->col, y_leftbrace, y_leftbrace+1, y_leftbrace+1);
        rc = ymap(p, state, ti);
        if (!rc)
        {
            yflush_comments(p, state, ti);
            DO_CALLBACK_SIM(brace_right, state, state->line, state->col, y_rightbrace, y_rightbrace+1, y_rightbrace+1);
        }
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
    yfree_comments(&p);
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


/* ------------------------------------------------------------------
 * YAML printer: a simple human readable block style output (no flow form).
 *
 * Example:
 *
 *   alfa: 123
 *   beta:
 *     gamma: 123
 *   list:
 *     - 1
 *     - 2
 *   pairs:
 *     - foo: 1
 *       bar: 2
 *
 * Indentation per level is taken from XMQOutputSettings::add_indent.
 * ------------------------------------------------------------------ */

static void yw(XMQPrintState *ps, const char *s)
{
    if (!s) return;
    XMQOutputSettings *output_settings = ps->output_settings;
    output_settings->content.write(output_settings->content.writer_state, s, NULL);
}

static void yws(XMQPrintState *ps, size_t n)
{
    while (n-- > 0) yw(ps, " ");
}

static size_t yaml_indent(XMQPrintState *ps)
{
    size_t indent = ps->output_settings->add_indent;
    if (indent < 1) indent = 2;
    return indent;
}

/*
  From needs_yaml_quote.c: report whether NUL-terminated text, intended to
  remain a YAML string, needs to be quoted to keep its meaning (numbers,
  dates and leading indicators are interpreted by YAML if left plain, as
  are the reserved words null/true/false/y/n/yes/no/on/off, case
  insensitively; a trailing space would be stripped).
*/
static bool nyq_ascii_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool nyq_ascii_iequal(const char *a, const char *b)
{
    while (*a && *b)
    {
        unsigned char x = (unsigned char)*a++;
        unsigned char y = (unsigned char)*b++;

        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}

bool yaml_needs_quotes(const char *s)
{
    if (!s || !*s)
        return true;

    /* Exclude leading YAML indicators. */
    if (strchr("!&*?:{}[],#|>'\"%@`", s[0]) != NULL)
        return true;

    /* Deliberately small whitelist, safe in block and flow values. */
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
    {
        unsigned char c = *p;
        if (!nyq_ascii_alpha(c) && !(c >= '0' && c <= '9') &&
            c != '_' && c != '-' && c != '.' && c != '/' && c != ' ')
            return true;
    }

    return s[strlen(s) - 1] == ' ';
}

// A scalar is written plain (unquoted) exactly when yaml_needs_quotes()
// says the text is safe in a plain scalar position. The S attribute
// (force_string) never removes the need for quotes: it only marks a
// string that looked like a number, boolean or null. Scalars are plain
// text in the x model and are only quoted when necessary in yaml.
static bool yaml_scalar_is_plain(const char *s, bool force_string)
{
    // The S attribute (force_string) always wins: the value is a string
    // that would be misread by a typed reader and is therefore quoted.
    // Unmarked text is plain when yaml says it is safe in a plain scalar
    // position (plain numbers, booleans and nulls are fine: x does not
    // care about those yaml types). The S attribute means the data is a
    // string that would be misread as one of those, so it is always
    // quoted.
    if (force_string) { return false; }
    return !yaml_needs_quotes(s);
}

static void yaml_print_comment(XMQPrintState *ps, const xmlNode *node)
{
    yw(ps, "# ");
    const char *p = node->content ? (const char*)node->content : "";
    for (; *p; p++)
    {
        if (*p == '\n' || *p == '\t' || *p == '\r') yw(ps, " ");
        else
        {
            char buf[2] = { *p, 0 };
            yw(ps, buf);
        }
    }
}

// Write out the scalar value of the node range [from..to] (quote it if necessary).
static void yaml_print_value(XMQPrintState *ps, xmlNode *from, xmlNode *to, bool force_string)
{
    // force_string (node has attribute S): the value is a string even if it
    // *looks* like a number, boolean or null, so it must be quoted in yaml.
    if (from == to)
    {
        const char *content = xml_element_content(from);
        if (content && !is_entity_node(from) && yaml_scalar_is_plain(content, force_string))
        {
            yw(ps, content);
            return;
        }
    }

    yw(ps, "\"");
    for (xmlNode *i = from; i; i = xml_next_sibling(i))
    {
        if (i > to) break;
        if (is_entity_node(i))
        {
            yw(ps, "&"); yw(ps, (const char*)i->name); yw(ps, ";");
            continue;
        }
        const char *content = xml_element_content(i);
        if (content)
        {
            XMQReturnString rs = xmq_quote_as_c(content, content+strlen(content), false);
            if (rs.status != XMQ_OK || !rs.string)
            {
                yw(ps, "\"");
                return;
            }
            yw(ps, rs.string);
            free(rs.string);
        }
        if (i == to) break;
    }
    yw(ps, "\"");
}

static size_t yaml_dup_used(xmlNode *node)
{
    size_t used = 0;
    for (xmlNode *i = xml_prev_sibling(node); i; i = xml_prev_sibling(i))
    {
        if (xml_element_name(i) && strcmp((const char*)i->name, (const char*)node->name) == 0) used++;
    }
    return used;
}

static size_t yaml_dup_total(xmlNode *node)
{
    size_t total = 1;
    for (xmlNode *i = xml_prev_sibling(node); i; i = xml_prev_sibling(i))
    {
        if (xml_element_name(i) && strcmp((const char*)i->name, (const char*)node->name) == 0) total++;
    }
    for (xmlNode *i = xml_next_sibling(node); i; i = xml_next_sibling(i))
    {
        if (xml_element_name(i) && strcmp((const char*)i->name, (const char*)node->name) == 0) total++;
    }
    return total;
}

static void yaml_print_key(XMQPrintState *ps, const char *name, size_t total, size_t used)
{
    if (strlen(name) > 0 && !yaml_needs_quotes(name))
    {
        yw(ps, name);
    }
    else
    {
        XMQReturnString rs = xmq_quote_as_c(name, name+strlen(name), false);
        if (rs.status == XMQ_OK && rs.string) { yw(ps, rs.string); free(rs.string); }
    }
    if (total > 1)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "[%zu]", used);
        yw(ps, buf);
    }
}

static void yaml_children(XMQPrintState *ps, size_t col,
                          xmlNode *first, xmlNode *last,
                          bool dashed, bool inline_first);
static void yaml_child(XMQPrintState *ps, size_t col, xmlNode *node);

// Write the range [first..last] of siblings, one per line starting at col.
// When dashed, each entry is prefixed with "- " and its content starts at col+2.
// When inline_first, the first entry is written at the current cursor position
// (i.e. no newline before it).
static void yaml_children(XMQPrintState *ps, size_t col,
                          xmlNode *first, xmlNode *last,
                          bool dashed, bool inline_first)
{
    bool inline_here = inline_first;
    for (xmlNode *i = first; i; i = xml_next_sibling(i))
    {
        if (i > last) break;
        if (inline_here)
        {
            if (dashed) yw(ps, "- ");
        }
        else
        {
            yw(ps, "\n");
            yws(ps, col);
            if (dashed) yw(ps, "- ");
        }
        inline_here = false;
        yaml_child(ps, col + (dashed ? 2 : 0), i);
    }
}

// Write a single node (mapping entry, sequence item, scalar or comment) at
// the current cursor position. Continuation lines (deeper levels, or more
// entries of an inline item) start at col (or col+2 for a nested sequence).
static void yaml_child(XMQPrintState *ps, size_t col, xmlNode *node)
{
    if (is_comment_node(node))
    {
        yaml_print_comment(ps, node);
        return;
    }

    if (is_entity_node(node))
    {
        yw(ps, "\"&"); yw(ps, (const char*)node->name); yw(ps, ";\"");
        return;
    }

    const char *name = (const char*)node->name;
    bool under = (name[0] == '_' && name[1] == 0);

    if (is_key_value_node(node))
    {
        bool force_string = xml_get_attribute(node, "S") != NULL;
        if (under)
        {
            // Bare scalar item.
            yaml_print_value(ps, xml_first_child(node), xml_last_child(node), force_string);
        }
        else
        {
            yaml_print_key(ps, name, yaml_dup_total(node), yaml_dup_used(node));
            yw(ps, ": ");
            yaml_print_value(ps, xml_first_child(node), xml_last_child(node), force_string);
        }
        return;
    }

    // Container node.
    if (under)
    {
        // Anonymous container = a (possibly nested) sequence item.
        // Its children align with the item content column.
        bool a = xml_get_attribute(node, "A") != NULL;
        xmlNode *first = xml_first_child(node);
        if (!first) { yw(ps, "\"\""); return; }
        yaml_children(ps, col, first, xml_last_child(node), a, true);
        return;
    }

    // Named container: "name:" followed by its children indented one level.
    yaml_print_key(ps, name, yaml_dup_total(node), yaml_dup_used(node));
    yw(ps, ":");
    xmlNode *first = xml_first_child(node);
    if (!first) return; // A lone "name:" line is valid yaml (a null value).
    bool a = xml_get_attribute(node, "A") != NULL;
    yaml_children(ps, col + yaml_indent(ps), first, xml_last_child(node), a, false);
}

void xmq_print_yaml(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    if (!doq || !doq->docptr_.xml) return;

    xmlDoc *doc = doq->docptr_.xml;
    XMQPrintState ps;
    memset(&ps, 0, sizeof(ps));
    ps.output_settings = output_settings;
    ps.doq = doq;

    bool started = false;
    for (xmlNode *r = doc->children; r; r = xml_next_sibling(r))
    {
        if (is_element_node(r) &&
            r->name && r->name[0] == '_' && r->name[1] == 0 &&
            !is_key_value_node(r))
        {
            // Standard root element: write its children as the top level.
            bool a = xml_get_attribute(r, "A") != NULL;
            xmlNode *first = xml_first_child(r);
            if (first)
            {
                yaml_children(&ps, 0, first, xml_last_child(r), a, !started);
                started = true;
            }
        }
        else
        {
            if (started) yw(&ps, "\n");
            yaml_child(&ps, 0, r);
            started = true;
        }
    }

    if (output_settings->final_newline) yw(&ps, "\n");
}

#endif // YAML_MODULE
