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

#define VERSION "0.1"

int main_xml2xmq(int argc, char **argv);
int main_xmq2xml(int argc, char **argv);

int main(int argc, char **argv)
{
    char *name = strrchr(argv[0], '/');
    if (name == NULL) name = argv[0];
    else name++;
    if (!strcmp("xml2xmq", name))
    {
        return main_xml2xmq(argc, argv);
    }
    if (!strcmp("xmq2xml", name))
    {
        return main_xmq2xml(argc, argv);
    }
    fprintf(stderr, "Binary must be named xml2xmq or xmq2xml!\n");
    return 1;
}
