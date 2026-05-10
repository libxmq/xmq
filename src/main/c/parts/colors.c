/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

bool generate_ansi_256color(char *buf, size_t buf_size, XMQColorDef *def);
bool generate_ansi_truecolor(char *buf, size_t buf_size, XMQColorDef *def);
bool hex_to_number(char c, char cc, int *v);

typedef struct {
    uint8_t r, g, b;
} RGB;

static int rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b);
static RGB ansi256_to_rgb(int idx);

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

    // "" the empty string translates into def with -1 -1 -1 as rgb values.
    // used to identify

    def->r = -1;
    def->g = -1;
    def->b = -1;
    def->bold = false;
    def->underline = false;

    if (s[0] == 0) return true;

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

bool generate_ansi_color(char *buf, size_t buf_size, XMQColorDef *def, bool truecolor)
{
    if (buf_size < 32) return false;

    // Start with the bold and underline.
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

    // Generate best approximation from standard 256 color palette.
    if (!truecolor) return generate_ansi_256color(i, buf_size-(i-buf), def);

    // Generate true color ansi.
    return generate_ansi_truecolor(i, buf_size-(i-buf), def);
}

bool generate_ansi_256color(char *buf, size_t buf_size, XMQColorDef *def)
{
    // Example: \x1b[38;5;12m256COLOR\x1b[0m
    char *i = buf;
    *i++ = '3';
    *i++ = '8';
    *i++ = ';';
    *i++ = '5';
    *i++ = ';';

    int color = rgb_to_ansi256(def->r, def->g, def->b);

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", color);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = 'm';
    *i++ = 0;

    return true;
}

bool generate_ansi_truecolor(char *buf, size_t buf_size, XMQColorDef *def)
{
    // Example: \x1b[38;2;40;177;249mTRUECOLOR\x1b[0m
    char *i = buf;
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

    if (def->r < 0)
    {
        snprintf(buf, buf_size, "\\definecolor{%s}{RGB}{%d,%d,%d}", name, 0, 0, 0);
    }
    else
    {
        snprintf(buf, buf_size, "\\definecolor{%s}{RGB}{%d,%d,%d}", name, def->r, def->g, def->b);
    }
    return true;
}

const char *color_names[15] = {
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
    "xmqFG", // Foreground color
    "xmqBG", // Background color
};

const char* colorName(int i)
{
    return color_names[i];
}

int colorShortNameToIndex(const char *name)
{
    if (!strcmp(name, "C")) return 0; // Comment
    if (!strcmp(name, "Q")) return 1; // Quote
    if (!strcmp(name, "E")) return 2; // Entity
    if (!strcmp(name, "NS")) return 3; // Name Space (both for element and attribute)
    if (!strcmp(name, "EN")) return 4; // Element Name
    if (!strcmp(name, "EK")) return 5; // Element Key
    if (!strcmp(name, "EKV")) return 6; // Element Key Value
    if (!strcmp(name, "AK")) return 7; // Attribute Key
    if (!strcmp(name, "AKV")) return 8; // Attribute Key Value
    if (!strcmp(name, "CP")) return 9; // Compound Parentheses
    if (!strcmp(name, "NSD")) return 10; // Name Space declaration xmlns
    if (!strcmp(name, "UW")) return 11; // Unicode whitespace
    if (!strcmp(name, "XSL")) return 12; // Element color for xsl transform elements.
    if (!strcmp(name, "FG")) return 13; // Foreground color
    if (!strcmp(name, "BG")) return 14; // Background color
    return -1;
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

/*
 * Convert 24-bit RGB ANSI color to the nearest 8-bit ANSI terminal color.
 *
 * Supports:
 *   - ANSI 16 system colors
 *   - ANSI 6x6x6 color cube (16-231)
 *   - ANSI grayscale ramp (232-255)
 */


/* Standard ANSI 16-color palette */
static const RGB ansi16[16] = {
    {0,   0,   0},       // 0 black
    {128, 0,   0},       // 1 red
    {0,   128, 0},       // 2 green
    {128, 128, 0},       // 3 yellow
    {0,   0,   128},     // 4 blue
    {128, 0,   128},     // 5 magenta
    {0,   128, 128},     // 6 cyan
    {192, 192, 192},     // 7 white

    {128, 128, 128},     // 8 bright black
    {255, 0,   0},       // 9 bright red
    {0,   255, 0},       // 10 bright green
    {255, 255, 0},       // 11 bright yellow
    {0,   0,   255},     // 12 bright blue
    {255, 0,   255},     // 13 bright magenta
    {0,   255, 255},     // 14 bright cyan
    {255, 255, 255}      // 15 bright white
};

/* Squared Euclidean distance */
static inline int color_distance(RGB a, RGB b)
{
    int dr = (int)a.r - (int)b.r;
    int dg = (int)a.g - (int)b.g;
    int db = (int)a.b - (int)b.b;

    return dr * dr + dg * dg + db * db;
}

/* Generate RGB value for ANSI 256-color index */
static RGB ansi256_to_rgb(int idx)
{
    RGB c;

    /* ANSI 16 colors */
    if (idx < 16)
    {
        return ansi16[idx];
    }

    /* 6x6x6 color cube */
    if (idx >= 16 && idx <= 231)
    {
        int n = idx - 16;

        int r = n / 36;
        int g = (n / 6) % 6;
        int b = n % 6;

        static const int levels[6] = {
            0, 95, 135, 175, 215, 255
        };

        c.r = levels[r];
        c.g = levels[g];
        c.b = levels[b];

        return c;
    }

    /* Grayscale ramp */
    int gray = 8 + (idx - 232) * 10;

    c.r = gray;
    c.g = gray;
    c.b = gray;

    return c;
}

/* Convert 24-bit RGB to nearest ANSI 256-color index */
int rgb_to_ansi256(uint8_t r, uint8_t g, uint8_t b)
{
    RGB input = { r, g, b };

    int best_idx = 0;
    int best_dist = INT32_MAX;

    for (int i = 0; i < 256; ++i)
    {
        RGB candidate = ansi256_to_rgb(i);

        int dist = color_distance(input, candidate);

        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    return best_idx;
}



#endif // COLORS_MODULE
