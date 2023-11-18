#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<stdbool.h>

#ifndef HASH_MAP_H
#define HASH_MAP_H

typedef struct HashMap_ HashMap;

HashMap *hashmap_create(size_t max_size);
// Returns NULL if no key is found.
void *hashmap_get(HashMap* map, const char* key);
// Putting a non-NULL value.
void hashmap_put(HashMap* map, const char* key, void *val);
// Remove a key-val.
void hashmap_remove(HashMap* map, const char* key);
void hashmap_free(HashMap* map);

#define HASHMAP_MODULE

#endif // HASH_H
