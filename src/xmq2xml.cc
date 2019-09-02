/*
 Copyright (c) 2019 Fredrik Öhrström

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

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <ostream>
#include <fstream>
#include <map>
#include <vector>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "util.h"
#include "parse.h"

using namespace std;
using namespace rapidxml;

#define VERSION "0.1"

int main_xmq2xml(int argc, char **argv)
{
    xml_document<> doc;
    if (argc < 2) {
        puts(manual);
        exit(0);
    }
    const char *file = argv[1];

    if (file == NULL) {
        fprintf(stderr, "You must supply an input file or - for reading from stdin.\n");
        exit(1);
    }

    vector<char> buffer;

    if (!strcmp(file, "-"))
    {
        bool rc = loadStdin(&buffer);
        if (!rc)
        {
            exit(1);
        }
    }
    else
    {
        string files = string(file);
        bool rc = loadFile(files, &buffer);
        if (!rc)
        {
            exit(1);
        }
    }
    buffer.push_back('\0');

    xml_node<> *node = doc.allocate_node(node_declaration, "?xml");
    doc.append_node(node);
    node->append_attribute(doc.allocate_attribute("version", "1.0"));
    node->append_attribute(doc.allocate_attribute("encoding", "UTF-8"));

    parse(file, &buffer[0], &doc);

    std::cout << doc;

    return 0;
}
