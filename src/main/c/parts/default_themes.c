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

#ifndef BUILDING_XMQ

#include"always.h"
#include"colors.h"
#include"default_themes.h"
#include"parts/xmq_internals.h"

#endif

#ifdef DEFAULT_THEMES_MODULE

const char *defaultColor(int i, const char *theme_name);

const char *default_darkbg_colors[NUM_XMQ_COLOR_NAMES] = {
    "#2aa1b3_B", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#a9a9a9", // XMQ_COLOR_NS
    "#ff8c00", // XMQ_COLOR_EN
    "#88b4f7", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#88b4f7", // XMQ_COLOR_AK
    "#6196ec", // XMQ_COLOR_AKV
    "#c061cb", // XMQ_COLOR_CP
    "#2aa1b3", // XMQ_COLOR_NSD
    "#880000_U", // XMQ_COLOR_UW
    "#c061cb" // XMQ_COLOR_XSL
};

const char *default_lightbg_colors[NUM_XMQ_COLOR_NAMES] = {
    "#2aa1b3_B", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#696969", // XMQ_COLOR_NS
    "#a86c00", // XMQ_COLOR_EN
    "#0060fd", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#0060fd", // XMQ_COLOR_AK
    "#12488c", // XMQ_COLOR_AKV
    "#c061cb", // XMQ_COLOR_CP
    "#1a91a3", // XMQ_COLOR_NSD
    "#880000_U", // XMQ_COLOR_UW
    "#c061cb" // XMQ_COLOR_XSL
};

// The more limited Windows console.

#define WIN_NOCOLOR      "\033[0m\033[24m"
#define WIN_GRAY         "\033[37m\033[24m"
#define WIN_DARK_GRAY    "\033[90m\033[24m"
#define WIN_GREEN        "\033[92m\033[24m"
// Not really bold, how?
#define WIN_DARK_GREEN_BOLD   "\033[32m\033[24m"
#define WIN_BLUE         "\033[94m\033[24m"
#define WIN_BLUE_UNDERLINE "\033[94m\033[4m"
#define WIN_LIGHT_BLUE   "\033[36m\033[24m"
#define WIN_LIGHT_BLUE_UNDERLINE   "\033[36m\033[4m"
// Not really bold, how?
#define WIN_DARK_BLUE_BOLD    "\033[34m\033[24m"
#define WIN_ORANGE       "\033[93m\033[24m"
#define WIN_ORANGE_UNDERLINE "\033[93m\033[4m"
#define WIN_DARK_ORANGE  "\033[33m\033[24m"
#define WIN_DARK_ORANGE_UNDERLINE  "\033[33m\033[4m"
#define WIN_MAGENTA      "\033[95m\033[24m"
// Not really bold, how?
#define WIN_CYAN_BOLD         "\033[96m\033[24m"
#define WIN_DARK_CYAN    "\033[36m\033[24m"
#define WIN_DARK_RED     "\033[31m\033[24m"
#define WIN_RED          "\033[91m\033[24m"
#define WIN_RED_UNDERLINE  "\033[91m\033[4m"
#define WIN_RED_BACKGROUND "\033[91m\033[4m"
#define WIN_DARK_RED_BOLD "\033[31m\033[24m"
#define WIN_UNDERLINE    "\033[4m"
#define WIN_NO_UNDERLINE "\033[24m"

const char *defaultColor(int i, const char *theme_name)
{
    if (!strcmp(theme_name, "lightbg")) return default_lightbg_colors[i];
    return default_darkbg_colors[i];
}

void installDefaultThemeColors(XMQTheme *theme)
{
    XMQColorDef *colors = theme->colors_darkbg;
    for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
    {
        const char *color = defaultColor(i, "darkbg");
        string_to_color_def(color, &colors[i]);
    }

    colors = theme->colors_lightbg;
    for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
    {
        const char *color = defaultColor(i, "lightbg");
        string_to_color_def(color, &colors[i]);
    }
}

#endif // DEFAULT_THEMES_MODULE
