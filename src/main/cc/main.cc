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

#include "cmdline.h"
#include "util.h"
#include "xmq.h"
#include "xmq_implementation.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <set>
#define VERSION "0.1"

using namespace std;

bool detectTreeType(xmq::Settings *settings)
{
    bool is_xmq = false == xmq_implementation::startsWithLessThan(*settings->in);

    if (settings->tree_type == xmq::TreeType::auto_detect)
    {
        settings->tree_type = xmq::TreeType::xml;
        if (xmq_implementation::isHtml(*settings->in))
        {
            settings->tree_type = xmq::TreeType::html;
        }
    }

    return is_xmq;
}

int main(int argc, char **argv)
{
    vector<char> in;
    vector<char> out;

    xmq::Settings settings(&in, &out);

    if (isatty(1))
    {
        settings.use_color = true;
        settings.output = xmq::RenderType::terminal;
    }

    parseCommandLine(&settings, argc, argv);

    bool is_xmq = detectTreeType(&settings);
    int rc = 0;

    if (is_xmq)
    {
        rc = main_xmq2xml(&settings);
    }
    else
    {
        rc = main_xml2xmq(&settings);
    }

    if (rc == 0)
    {
        out.push_back('\0');
        printf("%s", &out[0]);
    }

    return rc;
}
