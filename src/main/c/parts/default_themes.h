/* libxmq - Copyright (C) 2024 Fredrik Öhrström (spdx: MIT)

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

#ifndef DEFAULT_THEMES_H
#define DEFAULT_THEMES_H

#ifndef BUILDING_XMQ
#include "xmq.h"
#endif

const char *default_darkbg_colors[] = {
    "#2aa1b3", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#a9a9a9", // XMQ_COLOR_ENS
    "#ff8c00", // XMQ_COLOR_EN
    "#88b4f7", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#88b4f7", // XMQ_COLOR_AK
    "#6196ec", // XMQ_COLOR_AKV
    "#a9a9a9", // XMQ_COLOR_ANS
    "#2aa1b3", // XMQ_COLOR_NSD
    "#c061cb", // XMQ_COLOR_CP
    "#c061cb" // XMQ_COLOR_XSL
};

const char *default_lightbg_colors[] = {
    "#2aa1b3", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#696969", // XMQ_COLOR_ENS
    "#a86c00", // XMQ_COLOR_EN
    "#0060fd", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#0060fd", // XMQ_COLOR_AK
    "#12488c", // XMQ_COLOR_AKV
    "#696969", // XMQ_COLOR_ANS
    "#1a91a3", // XMQ_COLOR_NSD
    "#c061cb", // XMQ_COLOR_CP
    "#c061cb" // XMQ_COLOR_XSL
};

#define DEFAULT_THEMES_MODULE

#endif // DEFAULT_THEMES_H
