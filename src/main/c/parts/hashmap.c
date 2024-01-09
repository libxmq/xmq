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

#include"hashmap.h"
#include<assert.h>

#ifdef HASHMAP_MODULE

struct HashMapNode;
typedef struct HashMapNode HashMapNode;

struct HashMapNode
{
    const char *key_;
    void *val_;
    struct HashMapNode *next_;
};

struct HashMap
{
    int size_;
    int max_size_;
    HashMapNode** nodes_;
};

// FUNCTION DECLARATIONS //////////////////////////////////////////////////

size_t hash_code(const char *str);
HashMapNode* hashmap_create_node(const char *key);

///////////////////////////////////////////////////////////////////////////

size_t hash_code(const char *str)
{
    size_t hash = 0;
    size_t c;

    while (true)
    {
        c = *str++;
        if (!c) break;
        hash = c + (hash << 6) + (hash << 16) - hash; // sdbm
        // hash = hash * 33 ^ c; // djb2a
    }

    return hash;
}

HashMapNode* hashmap_create_node(const char *key)
{
    HashMapNode* new_node = (HashMapNode*) malloc(sizeof(HashMapNode));
    memset(new_node, 0, sizeof(*new_node));
    new_node->key_ = key;

    return new_node;
}

HashMap* hashmap_create(size_t max_size)
{
    HashMap* new_table = (HashMap*) malloc(sizeof(HashMap));
    memset(new_table, 0, sizeof(*new_table));

    new_table->nodes_ = (HashMapNode**)calloc(max_size, sizeof(HashMapNode*));
    new_table->max_size_ = max_size;
    new_table->size_ = 0;

    return new_table;
}

void hashmap_free_and_values(HashMap *map)
{
    for (int i=0; i < map->max_size_; ++i)
    {
        HashMapNode *node = map->nodes_[i];
        map->nodes_[i] = NULL;
        while (node)
        {
            if (node->val_) free(node->val_);
            node->val_ = NULL;
            HashMapNode *next = node->next_;
            free(node);
            node = next;
        }
    }
    map->size_ = 0;
    map->max_size_ = 0;
    free(map->nodes_);
    free(map);
}


void hashmap_put(HashMap* table, const char* key, void *val)
{
    assert(val != NULL);

    size_t index = hash_code(key) % table->max_size_;
    HashMapNode* slot = table->nodes_[index];
    HashMapNode* prev_slot = NULL;

    if (!slot)
    {
        // No hash in this position, create new node and fill slot.
        HashMapNode* new_node = hashmap_create_node(key);
        new_node->val_ = val;
        table->nodes_[index] = new_node;
        return;
    }

    // Look for a match...
    while (slot)
    {
        if (strcmp(slot->key_, key) == 0)
        {
            slot->val_ = val;
            return;
        }
        prev_slot = slot;
        slot = slot->next_;
    }

    // No matching node found, append to prev_slot
    HashMapNode* new_node = hashmap_create_node(key);
    new_node->val_ = val;
    prev_slot->next_ = new_node;
    return;
}

void *hashmap_get(HashMap* table, const char* key)
{
    int index = hash_code(key) % table->max_size_;
    HashMapNode* slot = table->nodes_[index];

    while (slot)
    {
        if (!strcmp(slot->key_, key))
        {
            return slot->val_;
        }
        slot = slot->next_;
    }

    return NULL;
}

void hashmap_free(HashMap* table)
{
    for (int i=0; i < table->max_size_; i++)
    {
        HashMapNode* slot = table->nodes_[i];
        while (slot != NULL)
        {
            HashMapNode* tmp = slot;
            slot = slot ->next_;
            free(tmp);
        }
    }

    free(table->nodes_);
    free(table);
}

#endif // HASHMAP_MODULE
