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

#include<string.h>

const char *manual = R"MANUAL(
usage: xmq <input>
)MANUAL";

void parseCommandLine(CmdLineOptions *options, int argc, char **argv)
{
    int i = 1;
    for (;;)
    {
        bool found = false;
        if (argc >= 2 && !strcmp(argv[i], "--color"))
        {
            options->use_color = true;
            if (options->output == xmq::RenderType::plain)
            {
                options->output = xmq::RenderType::terminal;
            }

            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--mono"))
        {
            options->use_color = false;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--output=plain"))
        {
            options->output = xmq::RenderType::plain;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--output=terminal"))
        {
            options->output = xmq::RenderType::terminal;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--output=html"))
        {
            options->output = xmq::RenderType::html;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--output=tex"))
        {
            options->output = xmq::RenderType::tex;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--html"))
        {
            options->tree_type = xmq::TreeType::html;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--nodec"))
        {
            // Do not print <?xml...> nor <!DOCTYPe...>
            options->no_declaration = true;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "-p"))
        {
            options->preserve_ws = true;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--nopp"))
        {
            options->no_pp = true;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--pp"))
        {
            options->pp = true;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "--compress"))
        {
            options->compress = true;
            i++;
            argc--;
            found = true;
        }
        if (argc >= 3 && !strcmp(argv[i], "--exclude"))
        {
            options->excludes.insert(argv[i+1]);
            i+=2;
            argc-=2;
            found = true;
        }
        if (argc >= 2 && !strncmp(argv[i], "--root=", 7))
        {
            options->root = std::string(argv[i]+7, argv[i]+strlen(argv[i]));
            i+=1;
            argc-=1;
            found = true;
        }
        if (argc >= 2 && !strcmp(argv[i], "-v"))
        {
            options->view = true;
            i++;
            argc--;
            found = true;
        }
        if (!found) break;
    }

    const char *file = argv[i];

    if (file == NULL)
    {
        puts(manual);
        exit(0);
    }

    if (!strcmp(file, "-"))
    {
        bool rc = loadStdin(options->in);
        if (!rc)
        {
            exit(1);
        }
    }
    else
    {
        std::string files = std::string(file);
        bool rc = loadFile(files, options->in);
        if (!rc)
        {
            // Error message already printed by loadFile.
            exit(1);
        }
    }
    options->in->push_back('\0');
}
