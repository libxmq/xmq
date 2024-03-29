/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

#ifndef COLORS_H
#define COLORS_H

#ifndef BUILDING_XMQ
#include "xmq.h"
#endif

struct XMQOutputSettings;
typedef struct XMQOutputSettings XMQOutputSettings;

/**
    XMQColor:

    Map token type into color index.
*/
typedef enum XMQColor {
    COLOR_none,
    COLOR_whitespace,
    COLOR_unicode_whitespace,
    COLOR_indentation_whitespace,
    COLOR_equals,
    COLOR_brace_left,
    COLOR_brace_right,
    COLOR_apar_left,
    COLOR_apar_right,
    COLOR_cpar_left,
    COLOR_cpar_right,
    COLOR_quote,
    COLOR_entity,
    COLOR_comment,
    COLOR_comment_continuation,
    COLOR_ns_colon,
    COLOR_element_ns,
    COLOR_element_name,
    COLOR_element_key,
    COLOR_element_value_text,
    COLOR_element_value_quote,
    COLOR_element_value_entity,
    COLOR_element_value_compound_quote,
    COLOR_element_value_compound_entity,
    COLOR_attr_ns,
    COLOR_attr_key,
    COLOR_attr_value_text,
    COLOR_attr_value_quote,
    COLOR_attr_value_entity,
    COLOR_attr_value_compound_quote,
    COLOR_attr_value_compound_entity,
    COLOR_ns_declaration
} XMQColor;

extern const char *color_names[13];

/**
    XMQColorStrings:
    @pre: string to inserted before the token
    @post: string to inserted after the token

    A color string object is stored for each type of token.
    It can store the ANSI color prefix, the html span etc.
    If post is NULL then when the token ends, the pre of the containing color will be reprinted.
    This is used for ansi codes where there is no stack memory (pop impossible) to the previous colors.
    I.e. pre = "\033[0;1;32m" which means reset;bold;green but post = NULL.
    For html/tex coloring we use the stack memory (pop possible) of tags.
    I.e. pre = "<span class="red">" post = "</span>"
    I.e. pre = "{\color{red}" post = "}"
*/
struct XMQColorStrings
{
    const char *pre;
    const char *post;
};
typedef struct XMQColorStrings XMQColorStrings;

/**
    XMQColoring:

    The coloring struct is used to prefix/postfix ANSI/HTML/TEX strings for
    XMQ tokens to colorize the printed xmq output.
*/
struct XMQColoring
{
    XMQColorStrings document; // <html></html>  \documentclass{...}... etc
    XMQColorStrings header; // <head>..</head>
    XMQColorStrings style;  // Stylesheet content inside header (html) or color(tex) definitions.
    XMQColorStrings body; // <body></body> \begin{document}\end{document}
    XMQColorStrings content; // Wrapper around rendered code. Like <pre></pre>, \textt{...}

    XMQColorStrings whitespace; // The normal whitespaces: space=32. Normally not colored.
    XMQColorStrings tab_whitespace; // The tab, colored with red background.
    XMQColorStrings unicode_whitespace; // The remaining unicode whitespaces, like: nbsp=160 color as red underline.
    XMQColorStrings indentation_whitespace; // The xmq generated indentation spaces. Normally not colored.
    XMQColorStrings equals; // The key = value equal sign.
    XMQColorStrings brace_left; // Left brace starting a list of childs.
    XMQColorStrings brace_right; // Right brace ending a list of childs.
    XMQColorStrings apar_left; // Left parentheses surrounding attributes. foo(x=1)
    XMQColorStrings apar_right; // Right parentheses surrounding attributes.
    XMQColorStrings cpar_left; // Left parentheses surrounding a compound value. foo = (&#10;' x '&#10;)
    XMQColorStrings cpar_right; // Right parentheses surrounding a compound value.
    XMQColorStrings quote; // A quote 'x y z' can be single or multiline.
    XMQColorStrings entity; // A entity &#10;
    XMQColorStrings comment; // A comment // foo or /* foo */
    XMQColorStrings comment_continuation; // A comment containing newlines /* Hello */* there */
    XMQColorStrings ns_colon; // The color of the colon separating a namespace from a name.
    XMQColorStrings element_ns; // The namespace part of an element tag, i.e. the text before colon in foo:alfa.
    XMQColorStrings element_name; // When an element tag has multiple children or attributes it is rendered using this color.
    XMQColorStrings element_key; // When an element tag is suitable to be presented as a key value, this color is used.
    XMQColorStrings element_value_text; // When an element is presented as a key and the value is presented as text, use this color.
    XMQColorStrings element_value_quote; // When the value is a single quote, use this color.
    XMQColorStrings element_value_entity; // When the value is a single entity, use this color.
    XMQColorStrings element_value_compound_quote; // When the value is compounded and this is a quote in the compound.
    XMQColorStrings element_value_compound_entity; // When the value is compounded and this is an entity in the compound.
    XMQColorStrings attr_ns; // The namespace part of an attribute name, i.e. the text before colon in bar:speed.
    XMQColorStrings attr_key; // The color of the attribute name, i.e. the key.
    XMQColorStrings attr_value_text; // When the attribute value is text, use this color.
    XMQColorStrings attr_value_quote; // When the attribute value is a quote, use this color.
    XMQColorStrings attr_value_entity; // When the attribute value is an entity, use this color.
    XMQColorStrings attr_value_compound_quote; // When the attribute value is a compound and this is a quote in the compound.
    XMQColorStrings attr_value_compound_entity; // When the attribute value is a compound and this is an entity in the compound.
    XMQColorStrings ns_declaration; // The xmlns part of an attribute namespace declaration.
};
typedef struct XMQColoring XMQColoring;

void getColor(XMQOutputSettings *os, XMQColor c, const char **pre, const char **post);

#define COLORS_MODULE

#endif // COLORS_H
