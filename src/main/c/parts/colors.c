/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"colors.h"
#include"xmq_internals.h"

#endif

#ifdef COLORS_MODULE

bool hex_to_number(char c, char cc, int *v);

/**
   get_color: Lookup the color strings
   coloring: The table of colors.
   c: The color to use from the table.
   pre: Store a pointer to the start color string here.
   post: Store a pointer to the end color string here.
*/
void getThemeStrings(XMQOutputSettings *os, XMQColor color, const char **pre, const char **post)
{
    XMQTheme *theme = os->theme;

    if (!theme)
    {
        *pre = NULL;
        *post = NULL;
        return;
    }

    switch(color)
    {

#define X(TYPE) case COLOR_##TYPE: *pre = theme->TYPE.pre; *post = theme->TYPE.post; return;
LIST_OF_XMQ_TOKENS
#undef X

    case COLOR_unicode_whitespace: *pre = theme->unicode_whitespace.pre; *post = theme->unicode_whitespace.post; return;
    case COLOR_indentation_whitespace: *pre = theme->indentation_whitespace.pre; *post = theme->indentation_whitespace.post; return;
    case COLOR_ns_override_xsl: *pre = theme->ns_override_xsl.pre; *post = theme->ns_override_xsl.post; return;
    default:
        *pre = NULL;
        *post = NULL;
        return;
    }
    assert(false);
    *pre = NULL;
    *post = NULL;
}

// Set background color: echo -ne "\033]11;#53186f\007"

//#define DEFAULT_COLOR "\033]11;#aa186f\007"
//#echo -ne '\e]10;#123456\e\\'  # set default foreground to #123456
//#echo -ne '\e]11;#abcdef\e\\'  # set default background to #abcdef
//printf "\x1b[38;2;40;177;249mTRUECOLOR\x1b[0m\n"

bool string_to_color_def(const char *s, XMQColorDef *def)
{
    // #aabbcc
    // #aabbcc_B
    // #aabbcc_U
    // #aabbcc_B_U

    def->r = -1;
    def->g = -1;
    def->b = -1;
    def->bold = false;
    def->underline = false;

    int r, g, b;
    bool bold, underline;

    r = g = b = 0;
    bold = false;
    underline = false;

    if (strlen(s) < 7) return false;
    if (*s != '#') return false;
    s++;

    bool ok = hex_to_number(*(s+0), *(s+1), &r);
    ok &= hex_to_number(*(s+2), *(s+3), &g);
    ok &= hex_to_number(*(s+4), *(s+5), &b);

    if (!ok) return false;

    s+=6;
    if (*s == '_')
    {
        if (*(s+1) == 'B') bold = true;
        if (*(s+1) == 'U') underline = true;
        s += 2;
    }
    if (*s == '_')
    {
        if (*(s+1) == 'B') bold = true;
        if (*(s+1) == 'U') underline = true;
        s += 2;
    }
    if (*s != 0) return false;

    def->r = r;
    def->g = g;
    def->b = b;
    def->bold = bold;
    def->underline = underline;

    return true;
}

bool hex_to_number(char c, char cc, int *v)
{
    int hi = 0;
    if (c >= '0' && c <= '9') hi = (int)c-'0';
    else if (c >= 'a' && c <= 'f') hi = 10+(int)c-'a';
    else if (c >= 'A' && c <= 'F') hi = 10+(int)c-'A';
    else return false;

    int lo = 0;
    if (cc >= '0' && cc <= '9') lo = (int)cc-'0';
    else if (cc >= 'a' && cc <= 'f') lo = 10+(int)cc-'a';
    else if (cc >= 'A' && cc <= 'F') lo = 10+(int)cc-'A';
    else return false;

    *v = hi*16+lo;
    return true;
}

bool generate_ansi_color(char *buf, size_t buf_size, XMQColorDef *def)
{
    // Example: \x1b[38;2;40;177;249mTRUECOLOR\x1b[0m
    if (buf_size < 32) return false;

    char *i = buf;

    *i++ = 27;
    *i++ = '[';
    *i++ = '0';
    *i++ = ';';
    if (def->bold)
    {
        *i++ = '1';
        *i++ = ';';
    }
    if (def->underline) {
        *i++ = '4';
        *i++ = ';';
    }
    *i++ = '3';
    *i++ = '8';
    *i++ = ';';
    *i++ = '2';
    *i++ = ';';

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", def->r);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = ';';

    snprintf(tmp, sizeof(tmp), "%d", def->g);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = ';';

    snprintf(tmp, sizeof(tmp), "%d", def->b);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = 'm';
    *i++ = 0;

    return true;
}

bool generate_html_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name)
{
    // xmqQ{color:#26a269;font-weight:600;}

    if (buf_size < 64) return false;

    char *i = buf;
    i += snprintf(i, buf_size, "%s{color:#%02x%02x%02x", name, def->r, def->g, def->b);
    *i++ = ';';

    if (def->bold)
    {
        const char *tmp = "font-weight:600;";
        strcpy(i, tmp);
        i += strlen(tmp);
    }

    if (def->underline)
    {
        const char *tmp = "text-decoration:underline;";
        strcpy(i, tmp);
        i += strlen(tmp);
    }

    *i++ = '}';
    *i++ = 0;
    return false;
}

bool generate_tex_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name)
{
    // \definecolor{mypink2}{RGB}{219, 48, 122}

    if (buf_size < 128) return false;

    snprintf(buf, buf_size, "\\definecolor{%s}{RGB}{%d,%d,%d}", name, def->r, def->g, def->b);
    return true;
}

const char *color_names[13] = {
    "xmqC", // Comment
    "xmqQ", // Quote
    "xmqE", // Entity
    "xmqNS", // Name Space (both for element and attribute)
    "xmqEN", // Element Name
    "xmqEK", // Element Key
    "xmqEKV", // Element Key Value
    "xmqAK", // Attribute Key
    "xmqAKV", // Attribute Key Value
    "xmqCP", // Compound Parentheses
    "xmqNSD", // Name Space declaration xmlns
    "xmqUW", // Unicode whitespace
    "xmqXSL", // Element color for xsl transform elements.
};

const char* colorName(int i)
{
    return color_names[i];
}

void setColorDef(XMQColorDef *cd, int r, int g, int b, bool bold, bool underline);

void setColorDef(XMQColorDef *cd, int r, int g, int b, bool bold, bool underline)
{
    cd->r = r;
    cd->g = g;
    cd->b = b;
    cd->bold = bold;
    cd->underline = underline;
}

#endif // COLORS_MODULE
