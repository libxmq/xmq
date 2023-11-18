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

#ifndef MEMBUFFER_H
#define MEMBUFFER_H

#include<stdlib.h>

/**
    MemBuffer:
    @max_: Current size of malloced buffer.
    @used_: Number of bytes actually used of buffer.
    @buffer_: Start of buffer data.
*/
typedef struct
{
    size_t max_; // Current size of malloc for buffer.
    size_t used_; // How much is actually used.
    char *buffer_; // The malloced data.
} MemBuffer;

// Output buffer functions ////////////////////////////////////////////////////////

MemBuffer *new_membuffer();
char *free_membuffer_but_return_trimmed_content(MemBuffer *mb);
void free_membuffer_and_free_content(MemBuffer *mb);
size_t pick_buffer_new_size(size_t max, size_t used, size_t add);
void membuffer_append_region(MemBuffer *mb, const char *start, const char *stop);
void membuffer_append(MemBuffer *mb, const char *start);
void membuffer_append_char(MemBuffer *mb, char c);
void membuffer_append_null(MemBuffer *mb);
void membuffer_append_entity(MemBuffer *mb, char c);

#define MEMBUFFER_MODULE

#endif // MEMBUFFER_H
