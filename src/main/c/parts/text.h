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

#ifndef TEXT_H
#define TEXT_H

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

bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len);
size_t encode_utf8(int uc, UTF8Char *utf8);
const char *has_ending_nl_space(const char *start, const char *stop, size_t *only_newlines);
const char *has_leading_space_nl(const char *start, const char *stop, size_t *only_newlines);
bool has_leading_ending_quote(const char *start, const char *stop);
bool has_newlines(const char *start, const char *stop);
bool has_must_escape_chars(const char *start, const char *stop);
bool has_all_quotes(const char *start, const char *stop);
bool has_all_whitespace(const char *start, const char *stop, bool *all_space, bool *only_newlines);
bool is_lowercase_hex(char c);
bool is_xmq_token_whitespace(char c);
bool is_xml_whitespace(char c);
bool is_all_xml_whitespace(const char *s);
bool is_xmq_element_name(const char *start, const char *stop);
bool is_xmq_element_start(char c);
bool is_xmq_text_name(char c);
size_t num_utf8_bytes(char c);
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc);
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);
char to_hex(int c);
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf);
char *xmq_quote_as_c(const char *start, const char *stop);
char *xmq_unquote_as_c(const char *start, const char *stop);
char *potentially_add_leading_ending_space(const char *start, const char *stop);

#define TEXT_MODULE

#endif // TEXT_H
