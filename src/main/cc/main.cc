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
using namespace xmq;

static bool is_white_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int main(int argc, char **argv)
{
    vector<char> in;
    vector<char> out;
    set<string> excludes;

    // Display using color of the output is a terminal.
    Settings settings;
    settings.use_color = isatty(1);

    if (isatty(1))
    {
        settings.output = RenderType::terminal;
    }
    else
    {
        settings.output = RenderType::plain;
    }

    int i = parseCommandLine(&settings, argc, argv);

    if (settings.use_color)
    {
        // printf("\033]11;?\033\\\n");
        // Detect background color of terminal
        // To change the color theme....
        // Now it defaults to colors suitable for a light background.
    }

    const char *file = argv[i];

    if (file == NULL)
    {
        puts(manual);
        exit(0);
    }
    if (!strcmp(file, "-"))
    {
        bool rc = loadStdin(&in);
        if (!rc)
        {
            exit(1);
        }
    }
    else
    {
        string files = string(file);
        bool rc = loadFile(files, &in);
        if (!rc)
        {
            exit(1);
        }
    }
    in.push_back('\0');

    bool input_is_xml = false;

    for (char c : in)
    {
        if (!is_white_space(c))
        {
            // Look at the first non-whitespace character in the file.
            // If it is a <, then it must be an xml file, since
            // it cannot be an xmq file.
            input_is_xml = c == '<';
            break;
        }
    }

    settings.filename = file;
    if (input_is_xml)
    {
        settings.in = &in;
        settings.out = &out;
        if (settings.tree_type == TreeType::auto_detect)
        {
            if (xmq_implementation::isHtml(in))
            {
                settings.tree_type = TreeType::html;
            }
            else
            {
                settings.tree_type = TreeType::xml;
            }
        }
        int rc = main_xml2xmq(&settings);
        if (rc == 0)
        {
            out.push_back('\0');
            printf("%s", &out[0]);
        }
        return rc;
    }
    else
    {
        settings.in = &in;
        settings.out = &out;
        if (settings.tree_type == TreeType::auto_detect)
        {
            if (xmq_implementation::isHtml(in))
            {
                settings.tree_type = TreeType::html;
            }
            else
            {
                settings.tree_type = TreeType::xml;
            }
        }
        int rc = main_xmq2xml(file, &settings);
        if (rc == 0)
        {
            out.push_back('\0');
            printf("%s", &out[0]);
        }
        return rc;
    }
}
