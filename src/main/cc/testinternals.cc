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
#include "xmq.h"
#include "xmq_implementation.h"

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


bool checkEquals(vector<char> *alfa, const char *beta)
{
    size_t len = alfa->size();
    if (strlen(beta) != len) goto error;

    for (size_t i=0; i<len; ++i)
    {
        if (((*alfa)[i] != beta[i])) goto error;
    }

    return true;

error:
    printf("GOT >%.*s<\nBut expected >%s<\n", (int)len, &((*alfa)[0]), beta);
    exit(1);
}

void test_incidental()
{
    vector<char> buffer;
    const char *ex1 = "alfa\nbeta\ngamma\n";
    buffer.insert(buffer.end(), ex1, ex1+strlen(ex1));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 0);
    checkEquals(&buffer, ex1);
    buffer.clear();
    const char *ex2 = " alfa\n beta\n gamma";
    buffer.insert(buffer.end(), ex2, ex2+strlen(ex2));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 0);
    checkEquals(&buffer, "alfa\nbeta\ngamma");
    buffer.clear();
    const char *ex3 = "  alfa\n  beta\n  gamma";
    buffer.insert(buffer.end(), ex3, ex3+strlen(ex3));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 0);
    checkEquals(&buffer, "alfa\nbeta\ngamma");
    buffer.clear();
    const char *ex4 = "alfa\n   beta\n   gamma\n";
    buffer.insert(buffer.end(), ex4, ex4+strlen(ex4));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 3);
    checkEquals(&buffer, "alfa\nbeta\ngamma\n");
    buffer.clear();

    const char *ex5 = "alfa\n    beta\n   gamma\n  delta\n";
    buffer.insert(buffer.end(), ex5, ex5+strlen(ex5));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 6);
    checkEquals(&buffer, "   alfa\n  beta\n gamma\ndelta\n");
    buffer.clear();

    const char *ex6 = "      alfa\n     beta\n    gamma\n   delta\n";
    buffer.insert(buffer.end(), ex6, ex6+strlen(ex6));
    xmq_implementation::removeIncidentalWhiteSpace(&buffer, 0);
    checkEquals(&buffer, "   alfa\n  beta\n gamma\ndelta\n");
    buffer.clear();

    /*
    const char *ex7 = "     alfa\n\n     beta\n     gamma\n     delta\n";
    buffer.insert(buffer.end(), ex7, ex7+strlen(ex7));
    removeIncidentalWhiteSpace(&buffer, 0);
    checkEquals(&buffer, "alfa\n\nbeta\ngamma\ndelta\n");
    buffer.clear();*/
}

void test_utf8_check()
{
}

void print_buf(vector<char> &b)
{
    for (char c : b) printf("%d ", c);
    printf("\n");
}

void test_a_cr_removal(const char *from, const char *to)
{
    vector<char> f(from, from+strlen(from));
    vector<char> t(to, to+strlen(to));
    removeCrs(&f);

    if (f.size() != t.size())
    {
        goto err;
    }

    for (size_t i = 0; i < f.size(); ++i)
    {
        if (f[i] != t[i]) goto err;
    }

    return;

err:
    printf("ERROR! Expected:\n");
    print_buf(t);
    printf("but got:\n");
    print_buf(f);
}

void test_cr_removal()
{
    // Test proper removal \r\n becomes \n
    test_a_cr_removal("aa\r\nbbb\r\n", "aa\nbbb\n");
    // Test that final \r remains.
    test_a_cr_removal("aa\r\nbbb\r",   "aa\nbbb\r");
    // Test that only \r remains.
    test_a_cr_removal("aa\rbbb\r",     "aa\rbbb\r");
    // Test no change for \n\r.
    test_a_cr_removal("aa\n\rbbb\n\r", "aa\n\rbbb\n\r");
    // Odd sequence.
    test_a_cr_removal("\n\r\n\r\n\r", "\n\n\n\r");
}

int main(int argc, char **argv)
{
    test_add_string();
    test_incidental();
    test_utf8_check();
    test_cr_removal();
    printf("OK\n");
}
