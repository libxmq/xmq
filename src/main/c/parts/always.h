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

#ifndef ALWAYS_H
#define ALWAYS_H

#include<stdbool.h>
#include<stdlib.h>

extern bool xmq_debug_enabled_;
extern bool xmq_verbose_enabled_;

void verbose__(const char* fmt, ...);
void debug__(const char* fmt, ...);
void check_malloc(void *a);

#define verbose(...) if (xmq_verbose_enabled_) { verbose__(__VA_ARGS__); }
#define debug(...) if (xmq_debug_enabled_) {debug__(__VA_ARGS__); }

#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)

#ifdef PLATFORM_WINAPI
char *strndup(const char *s, size_t l);
#endif

#define LIST_OF_XMQ_TOKENS  \
    X(whitespace)           \
    X(equals)               \
    X(brace_left)           \
    X(brace_right)          \
    X(apar_left)            \
    X(apar_right)           \
    X(cpar_left)            \
    X(cpar_right)           \
    X(quote)                \
    X(entity)               \
    X(comment)              \
    X(comment_continuation) \
    X(element_ns)           \
    X(element_name)         \
    X(element_key)          \
    X(element_value_text)   \
    X(element_value_quote)  \
    X(element_value_entity) \
    X(element_value_compound_quote)  \
    X(element_value_compound_entity) \
    X(attr_ns)             \
    X(attr_key)            \
    X(attr_value_text)     \
    X(attr_value_quote)    \
    X(attr_value_entity)   \
    X(attr_value_compound_quote)    \
    X(attr_value_compound_entity)   \
    X(ns_declaration) \
    X(ns_colon)  \

#define MAGIC_COOKIE 7287528

#define ALWAYS_MODULE

#endif // ALWAYS_H
