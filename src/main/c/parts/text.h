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

    Add an extra byte that we can set to zero if we need.
*/
#define MAX_NUM_UTF8_BYTES (4+1)
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
bool is_xmq_element_name(const char *start, const char *stop, const char **colon);
bool is_xmq_element_start(char c);
bool is_xmq_text_name(char c);
size_t num_utf8_bytes(char c);
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc);
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);
char to_hex(int c);
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf);
char *xmq_quote_as_c(const char *start, const char *stop, bool add_quotes);
char *xmq_unquote_as_c(const char *start, const char *stop, bool remove_quotes);
char *potentially_add_leading_ending_space(const char *start, const char *stop);
bool find_line_col(const char *start, const char *stop, size_t at, int *line, int *col);

// Return an array of unicode code points for a given category name.
// Ie. Ll=Letters lowercase, Lu=Letters uppercase etc.
bool unicode_category(const char *name, int **out_cat, size_t *out_cat_len);
bool category_find(int code, int *cat, size_t cat_len);

#define NUM_UNICODE_CATEOGIRES 38
#define UNICODE_CATEGORIES \
    X(Lu,Uppercase_Letter,an uppercase letter) \
    X(Ll,Lowercase_Letter, a lowercase letter) \
    X(Lt,Titlecase_Letter,a digraph encoded as a single character, with first part uppercase) \
    X(LC,Cased_Letter,Lu | Ll | Lt) \
    X(Lm,Modifier_Letter,a modifier letter) \
    X(Lo,Other_Letter,other letters, including syllables and ideographs) \
    X(L,Letter,Lu | Ll | Lt | Lm | Lo) \
    X(Mn,Nonspacing_Mark,a nonspacing combining mark (zero advance width)) \
    X(Mc,Spacing_Mark,a spacing combining mark (positive advance width)) \
    X(Me,Enclosing_Mark,an enclosing combining mark) \
    X(M,Mark,Mn | Mc | Me) \
    X(Nd,Decimal_Number,a decimal digit) \
    X(Nl,Letter_Number,a letterlike numeric character) \
    X(No,Other_Number,a numeric character of other type) \
    X(N,Number,Nd | Nl | No) \
    X(Pc,Connector_Punctuation,a connecting punctuation mark, like a tie) \
    X(Pd,Dash_Punctuation,a dash or hyphen punctuation mark) \
    X(Ps,Open_Punctuation,an opening punctuation mark (of a pair)) \
    X(Pe,Close_Punctuation,a closing punctuation mark (of a pair)) \
    X(Pi,Initial_Punctuation,an initial quotation mark) \
    X(Pf,Final_Punctuation,a final quotation mark) \
    X(Po,Other_Punctuation,a punctuation mark of other type) \
    X(P,Punctuation,Pc | Pd | Ps | Pe | Pi | Pf | Po) \
    X(Sm,Math_Symbol,a symbol of mathematical use) \
    X(Sc,Currency_Symbol,a currency sign) \
    X(Sk,Modifier_Symbol,a non-letterlike modifier symbol) \
    X(So,Other_Symbol,a symbol of other type) \
    X(S,Symbol,Sm | Sc | Sk | So) \
    X(Zs,Space_Separator,a space character (of various non-zero widths)) \
    X(Zl,Line_Separator,U+2028 LINE SEPARATOR only) \
    X(Zp,Paragraph_Separator,U+2029 PARAGRAPH SEPARATOR only) \
    X(Z,Separator,Zs | Zl | Zp) \
    X(Cc,Control,a C0 or C1 control code) \
    X(Cf,Format,a format control character) \
    X(Cs,Surrogate,a surrogate code point) \
    X(Co,Private_Use,a private-use character) \
    X(Cn,Unassigned,a reserved unassigned code point or a noncharacter) \
    X(C,Other,Cc | Cf | Cs | Co | Cn) \


#define TEXT_MODULE

#endif // TEXT_H
