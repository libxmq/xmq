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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include"parts/always.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#ifndef HASHMAP_H
#define HASHMAP_H

struct HashMap;
typedef struct HashMap HashMap;

struct HashMapIterator;
typedef struct HashMapIterator HashMapIterator;

HashMap *hashmap_create(size_t max_size);
// Returns NULL if no key is found.
void *hashmap_get(HashMap* map, const char* key);
// Putting a non-NULL value.
void hashmap_put(HashMap* map, const char* key, void *val);
// How many key-vals are there?
size_t hashmap_size(HashMap *map);
// Free the hashmap itself.
void hashmap_free(HashMap* map);
// Free the hasmap and its contents.
void hashmap_free_and_values(HashMap *map, FreeFuncPtr freefunc);

HashMapIterator *hashmap_iterate(HashMap *map);
bool hashmap_next_key_value(HashMapIterator *i, const char **key, void **val);
void hashmap_free_iterator(HashMapIterator *i);

#define HASHMAP_MODULE

#endif // HASHMAP_H
