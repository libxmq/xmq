/*
MIT License

Copyright (c) 2019-2020 Fredrik Öhrström

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

#include "util.h"

#include <string>
#include <string.h>
#include <memory>

using namespace std;

unique_ptr<char[]> ws(const char *s)
{
    size_t n = strlen(s);
    char *c = new char[n+1];
    memcpy(c, s, n+1);
    return unique_ptr<char[]>(c);
}

void test_add_string()
{
    auto s1 = ws("alfa.beta");
    auto s2 = ws("alfa.gamma");
    StringCount c;
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s1.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);
    add_string(s2.get(), c);

    string s = find_prefix(s1.get(), c);
    if (s != "alfa.") {
        printf("Expected alfa. but got %s\n", s.c_str());
        exit(1);
    }
}

int main(int argc, char **argv)
{
    test_add_string();
    printf("OK\n");
}
