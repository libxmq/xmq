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

extern bool xmq_trace_enabled_;
extern bool xmq_debug_enabled_;
extern bool xmq_verbose_enabled_;
extern bool xmq_log_xmq_enabled_;

void verbose__(const char* fmt, ...);
void debug__(const char* fmt, ...);
void trace__(const char* fmt, ...);
void check_malloc(void *a);

#define verbose(...) if (xmq_verbose_enabled_) { verbose__(__VA_ARGS__); }
#define debug(...) if (xmq_debug_enabled_) {debug__(__VA_ARGS__); }
#define trace(...) if (xmq_trace_enabled_) {trace__(__VA_ARGS__); }

#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define PRINT_WARNING(...) fprintf(stderr, __VA_ARGS__)

#ifdef PLATFORM_WINAPI
char *strndup(const char *s, size_t l);
#endif

// A common free function ptr to be used when freeing collections.
typedef void(*FreeFuncPtr)(void*);

#define ALWAYS_MODULE

#endif // ALWAYS_H
