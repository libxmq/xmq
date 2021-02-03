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

#ifndef CMDLINE_H
#define CMDLINE_H

#include "xmq.h"

class CmdLineOptions
{
public:
    CmdLineOptions(std::vector<char> *i, std::vector<char> *o) : in(i), out(o) {}
    std::vector<char> *in;
    std::vector<char> *out;

    std::string filename;
    xmq::TreeType tree_type {};  // Set input type to: auto_detect, xml or html.
    xmq::RenderType output {};   // Write plain text, text+ansi, text+html or text+tex.
    bool use_color {};      // Set to true to produce colors. Color can never be enabled with the plain output type.
    bool no_declaration {}; // Do not print any xml-declaration <? ?> nor doctype <!DOCTYPE html>.
    bool preserve_ws {};    // When converting from xml to xmq. Preserve whitespace as much as possible.
    bool view {};           // Do not convert, just view the input, potentially adding color and formatting.
    bool compress {};       // Find common prefixes of the tags.
    bool pp {};             // Do pretty print the xml/html.
    bool no_pp {};          // Do not pretty print the xml/html.
    std::set<std::string> excludes; // Exclude these attributes
    std::string root;       // If non-empty, check that the xmq has this root tag, if not then add it.
};

void parseCommandLine(CmdLineOptions *options, int argc, char **argv);

#endif
