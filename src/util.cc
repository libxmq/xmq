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

#include "util.h"

#include <assert.h>
#include <string>
#include <map>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

void add_string(char *s, StringCount &c)
{
    assert(s != NULL);
    assert(s[0] != 0);

    //printf("add %s\n", s);
    char *p = s+1;

    int x = 0;
    while (*p != 0)
    {
        char tmp = *p;
        *p = 0;
        string tmps(s);
        *p = tmp;
        if (c.count(tmps) == 0)
        {
            c[tmps] = 1+x;
            //printf("Set \"%s\" = 1\n", tmps.c_str());
            break;
        }
        else
        {
            c[tmps]++;
            //printf("Inc \"%s\" = %d\n", tmps.c_str(), c[tmps]);
        }
        p++;
        x++;
    }
}

string find_prefix(char *s, StringCount &c)
{
    assert(s != NULL);
    assert(s[0] != 0);

    char *p = s+1;
    string prev;
    size_t prev_count = 0;

    while (*p != 0)
    {
        char tmp = *p;
        *p = 0;
        string tmps(s);
        *p = tmp;
        size_t count = 0;
        if (c.count(tmps) > 0) {
            count = c[tmps];
        }
        //printf("find \"%s\" %zu\n", tmps.c_str(), count);
        if (count < prev_count)
        {
            //printf("find done %s\n", prev.c_str());
            return prev;
        }
        prev = tmps;
        p++;
        prev_count = count;
    }
    return "";
}

bool loadFile(string file, vector<char> *buf)
{
    int blocksize = 1024;
    char block[blocksize];

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Could not open file %s errno=%d\n", file.c_str(), errno);
        return false;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}

bool loadStdin(vector<char> *buf)
{
    int blocksize = 1024;
    char block[blocksize];

    int fd = 0;
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}


const char *manual = R"MANUAL(
Usage: xml2xmq {options} <input> {<output>}
       xmq2xml {options} <input> {<output>}

As <options> you can use:
  --compress

)MANUAL";

bool isWhiteSpace(char c)
{
    return
        c == ' ' ||
        c == '\t' ||
        c == '\n';
}

bool isNewLine(char c)
{
    return
        c == '\n';
}
