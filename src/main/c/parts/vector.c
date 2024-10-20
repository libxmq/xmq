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

#ifndef BUILDING_XMQ

#include"vector.h"
#include<assert.h>
#include<memory.h>

#endif

#ifdef VECTOR_MODULE

Vector *new_vector()
{
    Vector *s = (Vector*)malloc(sizeof(Vector));
    memset(s, 0, sizeof(Vector));
    s->elements_size = 16;
    s->elements = (void**)calloc(16, sizeof(void*));
    return s;
}

void free_vector(Vector *vector)
{
    if (!vector) return;

    vector->size = 0;
    vector->elements_size = 0;
    if (vector->elements) free(vector->elements);
    vector->elements = NULL;
    free(vector);
}

void free_vector_elements(Vector *v)
{
    for (size_t i = 0; i < v->size; ++i)
    {
        void *e = v->elements[i];
        if (e)
        {
            free(e);
            v->elements[i] = 0;
        }
    }
}

void push_back_vector(Vector *vector, void *data)
{
    assert(vector);

    vector->size++;
    if (vector->size > vector->elements_size)
    {
        if (vector->elements_size >= 1024)
        {
            vector->elements_size += 1024;
        }
        else
        {
            vector->elements_size *= 2;
        }
        vector->elements = (void**)realloc(vector->elements, vector->elements_size);
    }
    vector->elements[vector->size-1] = data;
}

void *element_at_vector(Vector *v, size_t i)
{
    assert(i < v->size);

    return v->elements[i];
}

#endif // VECTOR_MODULE
