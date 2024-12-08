
#include"colors.h"
#include"core.h"
#include"hashmap.h"
#include"stack.h"
#include"text.h"
#include"quicksort_strings.h"

#include<assert.h>
#include<stdint.h>

#define TESTS \
    X(test_core) \
    X(test_hashmap) \
    X(test_quicksort) \
    X(test_stack) \
    X(test_binary_search) \
    X(test_colors) \

#define X(name) void name();
TESTS
#undef X

bool all_ok_ = true;

void test_core()
{
    int8_t i8;
    bool ok = coreParseI8("77", &i8);
    if (i8 != 77 || !ok)
    {
        all_ok_ = false;
        printf("ERROR: coreParseI8(77) failed\n");
    }
}

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

void test_binary_search()
{
    int empty[] = {};
    int a[] = { 1 };
    int b[] = { 1, 2 };

    bool found = category_has_code(1, empty, 0);
    if (found) { all_ok_ = false; printf("ERROR: expected not found in [].\n"); }
    found = category_has_code(1, a, 1);
    if (!found) { all_ok_ = false; printf("ERROR: expected found 1 in [1].\n"); }
    found = category_has_code(2, a, 1);
    if (found) { all_ok_ = false; printf("ERROR: expected not 2 in [1].\n"); }
    found = category_has_code(1, b, 2);
    if (!found) { all_ok_ = false; printf("ERROR: expected 1 in [1, 2].\n"); }
    found = category_has_code(2, b, 2);
    if (!found) { all_ok_ = false; printf("ERROR: expected 2 in [1, 2].\n"); }
    found = category_has_code(7, b, 2);
    if (found) { all_ok_ = false; printf("ERROR: expected not 7 in [1, 2].\n"); }
    found = category_has_code(0, b, 2);
    if (found) { all_ok_ = false; printf("ERROR: expected not 0 in [1, 2].\n"); }

    int c[] = { 1, 2, 7, 10, 11, 12, 55, 99 };
    found = category_has_code(2, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 2 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(99, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 99 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(55, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 55 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(1, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 1 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(7, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 7 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(10, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 10 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(11, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 11 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(12, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 12 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
    found = category_has_code(11, c, sizeof(c)/sizeof(int));
    if (!found) { all_ok_ = false; printf("ERROR: expected 11 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }

    found = category_has_code(13, c, sizeof(c)/sizeof(int));
    if (found) { all_ok_ = false; printf("ERROR: expected not 13 in [1, 2, 7, 10, 11, 12, 55, 99].\n"); }
}

void test_stack()
{
    Stack *stack = stack_create();
    stack_push(stack, (void*)42);
    assert(stack->size == 1);
    int64_t v = (int64_t)stack_pop(stack);
    if (v != 42) {
        printf("BAD STACK\n");
        all_ok_ = false;
    }
    stack_free(stack);
}

void test_colors()
{
    XMQColorDef def;
    string_to_color_def("#800711", &def);

    if (def.r != 128 || def.g != 7 || def.b != 17)
    {
        printf("BAD colors!\n");
    }

    char buf[128];

    generate_ansi_color(buf, sizeof(buf), &def);

    printf("ANSI %sTRUECOLOR\x1b[0m\n", buf);

    generate_html_color(buf, sizeof(buf), &def, "GURKA");

    printf("HTML %s\n", buf);

    generate_tex_color(buf, sizeof(buf), &def, "GURKA");

    printf("TEX %s\n", buf);
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
