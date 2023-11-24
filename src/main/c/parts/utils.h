/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef UTILS_H
#define UTILS_H

#include<stdbool.h>
#include<stdlib.h>

/**
    UTF8Char: storage for 1 to 4 utf8 bytes

    An utf8 char is at most 4 bytes since the max unicode nr is capped at U+10FFFF:
*/
#define MAX_NUM_UTF8_BYTES 4
typedef struct
{
    char bytes[MAX_NUM_UTF8_BYTES];
} UTF8Char;

void check_malloc(void *a);

#define PRINT_STDOUT(...) printf(__VA_ARGS__)
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)

bool is_lowercase_hex(char c);

// UTF8 functions /////////////////

bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len);
size_t num_utf8_bytes(char c);
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc);
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf);

#define UTILS_MODULE

#endif // UTILS_H
