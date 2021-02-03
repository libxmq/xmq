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

#include "xmq.h"
#include "xmq_implementation.h"
#include "util.h"
#include <string.h>
#include <stdarg.h>
#include <assert.h>

using namespace std;
using namespace xmq;

xmq::Document::Document()
{

}

void *xmq::Document::root()
{
    return root_;
}

char *xmq::Document::allocateCopy(const char *content, size_t len)
{
    return NULL;
}

void *xmq::Document::appendElement(void *parent, Token t)
{
    return NULL;
}

void xmq::Document::appendComment(void *parent, Token t)
{
}

void xmq::Document::appendData(void *parent, Token t)
{
}

void xmq::Document::appendAttribute(void *parent, Token key, Token value)
{
}
