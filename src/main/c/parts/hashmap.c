#include"hashmap.h"
#include<assert.h>

#ifdef HASHMAP_MODULE

typedef struct HashMapNode_
{
    const char *key_;
    void *val_;
    struct HashMapNode_ *next_;
} HashMapNode;

typedef struct HashMap_
{
    int size_;
    int max_size_;
    HashMapNode** nodes_;
} HashMap;

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
    for (size_t i=0; i < table->max_size_; i++)
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
