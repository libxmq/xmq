
#include"hashmap.h"
#include"stack.h"
#include"quicksort_strings.h"

#include<assert.h>

#define TESTS \
    X(test_hashmap) \
    X(test_quicksort) \
    X(test_stack) \

bool all_ok_ = true;

void test_hashmap()
{
    HashMap *hm = hashmap_create(100);
    hashmap_put(hm, "HOWDY", (void*)42);
    void *v = hashmap_get(hm, "HOWDY");
    if (v != (void*)42)
    {
        all_ok_ = false;
        printf("ERROR: hashmap_get expected 42 but got %zu\n", (size_t)v);
    }
    hashmap_free(hm);
}

void test_quicksort()
{
    const char *a = "car"; // 0
    const char *b = "color"; // 1
    const char *c = "colour"; // 2
    const char *d = "detail"; // 3
    const char *e = "work"; // 4
    const char *f = "zebra"; //

    const char *strings[] = { d,f,c,a,e,b };

    quicksort_strings(strings, 6);

    if (strings[0] != a ||
        strings[1] != b ||
        strings[2] != c ||
        strings[3] != d ||
        strings[4] != e ||
        strings[5] != f)
    {
        printf("ERROR in quick sort!\n");
        for (size_t i=0; i<6; ++i)
        {
            printf("%zu %s\n", i, strings[i]);
        }
        all_ok_ = false;
    }
}

void test_stack()
{
    Stack *stack = new_stack();
    push_stack(stack, (void*)42);
    assert(stack->size == 1);
    int64_t v = (int64_t)pop_stack(stack);
    if (v != 42) {
        printf("BAD STACK\n");
        all_ok_ = false;
    }
    free_stack(stack);
}

/*
bool TEST_MEM_BUFFER()
{
    MemBuffer *mb = new_membuffer();
    membuffer_append(mb, "HEJSAN");
    membuffer_append_null(mb);
    char *mem = free_membuffer_but_return_trimmed_content(mb);
    if (!strcmp(mem, "HESJAN")) return false;
    free(mem);

    mb = new_membuffer();
    size_t n = 0;
    for (int i = 0; i < 32000; ++i)
    {
        membuffer_append(mb, "Foo");
        n += 3;
        assert(mb->used_ == n);
    }
    membuffer_append_null(mb);
    mem = free_membuffer_but_return_trimmed_content(mb);
    n = strlen(mem);
    if (n != 96000) return false;
    free(mem);

    return true;
}
*/

int main(int argc, char **argv)
{
#define X(name) name();
    TESTS
#undef X

    if (all_ok_) printf("OK: parts testinternals\n");
    else printf("ERROR: parts testinternals\n");
    return all_ok_ ? 0 : 1;
}
