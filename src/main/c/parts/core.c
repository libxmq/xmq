/* libxmq - Copyright (C) 2024 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include"core.h"
#include<assert.h>
#include<stdint.h>
#include<stdlib.h>

#ifdef CORE_MODULE

bool internal_parser_number(const char *s, int64_t *out, int num_digits);

bool internal_parser_number(const char *s, int64_t *out, int num_digits)
{
    if (s == NULL || *s == 0) return false;

    char *err = NULL;
    int64_t tmp = strtoll(s, &err, 0);

    if (err == NULL || *err != 0) return false;
    *out = tmp;

    return true;
}

bool coreParseI8(const char *s, int8_t *out)
{
    int64_t tmp = 0;
    bool ok = internal_parser_number(s, &tmp, 3);
    if (!ok) return false;
    if (tmp > 127) return false;
    if (tmp < -128) return false;
    *out = (int8_t)tmp;

    return true;
}

bool coreParseI16(const char *s, int16_t *out)
{
    return false;
}

bool coreParseI32(const char *s, int32_t *out)
{
    return false;
}

bool coreParseI64(const char *s, int64_t *out)
{
    return false;
}

/*bool coreParseI128(const char *s, __int128 *out)
{
    return false;
}*/

#endif // CORE_MODULE
