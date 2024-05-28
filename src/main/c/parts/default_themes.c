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
    "#2aa1b3", // XMQ_COLOR_C
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

const char *win_darkbg_ansi[NUM_XMQ_COLOR_NAMES] = {
    "\033[96m\033[24m", // XMQ_COLOR_C --- CYAN
    "\033[92m\033[24m", // XMQ_COLOR_Q --- GREEN
    "\033[95m\033[24m", // XMQ_COLOR_E --- MAGENTA
    "\033[37m\033[24m", // XMQ_COLOR_NS --- GRAY
    "\033[93m\033[24m", // XMQ_COLOR_EN --- ORANGE
    "\033[36m\033[24m", // XMQ_COLOR_EK --- LIGHT BLUE
    "\033[92m\033[24m", // XMQ_COLOR_EKV --- GREEN
    "\033[36m\033[24m", // XMQ_COLOR_AK --- LIGHT BLUE
    "\033[94m\033[24m", // XMQ_COLOR_AKV --- BLUE
    "\033[95m\033[24m", // XMQ_COLOR_CP --- MAGENTA
    "\033[36m\033[24m", // XMQ_COLOR_NSD --- LIGHT BLUE
    "\033[91m\033[4m", // XMQ_COLOR_UW --- RED UNDERLINE
    "\033[95m\033[24m", // XMQ_COLOR_XSL -- MAGENTA
};

const char *default_lightbg_colors[NUM_XMQ_COLOR_NAMES] = {
    "#2aa1b3", // XMQ_COLOR_C
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

const char *ansiWin(int i)
{
    return win_darkbg_ansi[i];
}

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
