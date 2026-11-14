
#include"colors.h"
#include"core.h"
#include"default_themes.h"
#include"hashmap.h"
#include"quicksort_strings.h"
#include"stack.h"
#include"text.h"
#include"xml_dom.h"
#include"yaep_objstack.h"

#include<assert.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define TESTS \
    X(test_core) \
    X(test_hashmap) \
    X(test_quicksort) \
    X(test_stack) \
    X(test_binary_search) \
    X(test_colors) \
    X(test_objstack) \
    X(test_theme) \
    X(test_xml_dom) \


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
    int empty[] = { 0 };
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

    generate_ansi_color(buf, sizeof(buf), &def, true);

    printf("ANSI %sTRUECOLOR\x1b[0m\n", buf);

    generate_ansi_color(buf, sizeof(buf), &def, false);

    printf("ANSI %s256COLOR\x1b[0m\n", buf);

    generate_html_color(buf, sizeof(buf), &def, "GURKA");

    printf("HTML %s\n", buf);

    generate_tex_color(buf, sizeof(buf), &def, "GURKA");

    printf("TEX %s\n", buf);
}

void test_objstack()
{
    os_t test_os;
    YaepAllocator *allocator = yaep_alloc_new (NULL, NULL, NULL, NULL);

    const char *org = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    OS_CREATE(test_os, allocator, 0);
    OS_TOP_ADD_STRING(test_os, org);
    const char *msg = (char*)OS_TOP_BEGIN(test_os);
    OS_TOP_FINISH(test_os);
    if (strcmp(msg, org))
    {
        printf("BAD YAEP OBJSTACK\n");
        all_ok_ = false;
    }

    char *s = humanReadableTwoDecimals(objstack_memusage(&test_os));
    if (strcmp(s, "536.00 B"))
    {
        printf("BAD YAEP OBJSTACK SIZE\n");
        all_ok_ = false;
    }
    free(s);
    OS_DELETE(test_os);
    yaep_alloc_del(allocator);
}

void test_theme()
{
    XMQTheme t;
    installTheme(&t, "C=112233");
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

#define DOM_CHECK(cond) do { if (!(cond)) { all_ok_ = false; printf("ERROR: test_xml_dom %s:%d %s\n", __FILE__, __LINE__, #cond); } } while(0)

static bool dom_str_is(char *s, const char *expected)
{
    bool ok = s && !strcmp(s, expected);
    if (!ok) printf("ERROR: got \"%s\" expected \"%s\"\n", s ? s : "(null)", expected);
    free(s);
    return ok;
}

void test_xml_dom()
{
    XmlDom *dom = xml_dom_new();
    XMQNode *root = xml_node_new_element(dom, "root", NULL);
    xml_dom_set_root_element(dom, root);
    DOM_CHECK(xml_dom_root_element(dom) == root);
    DOM_CHECK(xml_node_parent(root) == xml_dom_document(dom));

    // Text merging in add_child, but not in append_child.
    XMQNode *t1 = xml_node_add_child(root, xml_node_new_text(dom, "Hello"));
    XMQNode *t2 = xml_node_add_child(root, xml_node_new_text(dom, " World"));
    DOM_CHECK(t1 == t2);
    DOM_CHECK(xml_node_num_children(root) == 1);
    DOM_CHECK(!strcmp(xml_node_content(t1), "Hello World"));
    xml_node_append_child(root, xml_node_new_text(dom, "!"));
    DOM_CHECK(xml_node_num_children(root) == 2);

    XMQNode *a = xml_node_add_child(root, xml_node_new_element(dom, "a", NULL));
    xml_node_add_child(a, xml_node_new_text(dom, "A"));
    XMQNode *dtd = xml_dom_set_dtd(dom, "root", NULL, NULL);
    DOM_CHECK(xml_node_first_child(xml_dom_document(dom)) == dtd && xml_node_next(dtd) == root);
    DOM_CHECK(!xml_dtd_has_declarations(dtd));
    DOM_CHECK(xml_dtd_add_entity(dtd, "foo", XML_ENTITY_INTERNAL_GENERAL, NULL, NULL, NULL, "&", "&#38;") != NULL);
    DOM_CHECK(xml_dtd_add_entity(dtd, "foo", XML_ENTITY_INTERNAL_GENERAL, NULL, NULL, NULL, "other", NULL) == NULL);
    DOM_CHECK(xml_dtd_add_entity(dtd, "foo", XML_ENTITY_INTERNAL_PARAMETER, NULL, NULL, NULL, "param", NULL) != NULL);
    DOM_CHECK(xml_dtd_add_notation(dtd, "gif", "<!NOTATION gif SYSTEM \"gif\" ") != NULL);
    DOM_CHECK(xml_dtd_add_notation(dtd, "gif", "dup") == NULL);
    DOM_CHECK(xml_dtd_has_declarations(dtd));
    DOM_CHECK(!strcmp(xml_node_content(xml_dom_get_parameter_entity(dom, "foo")), "param"));
    xml_node_add_child(a, xml_node_new_entity_ref(dom, "foo"));
    xml_node_add_child(root, xml_node_new_comment(dom, "comment"));
    XMQNode *b = xml_node_add_child(root, xml_node_new_element(dom, "b", NULL));
    xml_node_add_child(b, xml_node_new_cdata(dom, "B", 1));
    DOM_CHECK(dom_str_is(xml_node_string_value(root), "Hello World!A&B"));

    // Sibling insertion and replace.
    XMQNode *c = xml_node_new_element(dom, "c", NULL);
    xml_node_add_prev_sibling(b, c);
    DOM_CHECK(xml_node_next(c) == b && xml_node_prev(b) == c && xml_node_parent(c) == root);
    XMQNode *d = xml_node_new_element(dom, "d", NULL);
    xml_node_add_next_sibling(b, d);
    DOM_CHECK(xml_node_last_child(root) == d);
    XMQNode *e = xml_node_new_element(dom, "e", NULL);
    DOM_CHECK(xml_node_replace(c, e) == c);
    DOM_CHECK(xml_node_parent(c) == NULL && xml_node_next(e) == b);
    xml_node_free(c);

    // Document order.
    DOM_CHECK(xml_node_compare_order(a, b) < 0);
    DOM_CHECK(xml_node_compare_order(b, a) > 0);
    DOM_CHECK(xml_node_compare_order(root, xml_node_first_child(a)) < 0);
    DOM_CHECK(xml_node_compare_order(xml_node_first_child(a), e) < 0);

    // Attributes.
    XMQAttr *x = xml_node_set_attr(a, "x", NULL, "1");
    XMQAttr *y = xml_node_set_attr(a, "y", NULL, "2");
    DOM_CHECK(xml_node_set_attr(a, "x", NULL, "3") == x);
    DOM_CHECK(!strcmp(xml_attr_simple_value(x), "3"));
    // Attribute values with entity references are stored as children.
    xml_node_append_child(xml_attr_node(y), xml_node_new_entity_ref(dom, "foo"));
    DOM_CHECK(xml_attr_simple_value(y) == NULL);
    DOM_CHECK(dom_str_is(xml_node_string_value(xml_attr_node(y)), "2&"));
    DOM_CHECK(xml_node_get_attr(a, "y", NULL) == y);
    DOM_CHECK(xml_attr_owner(y) == a);
    DOM_CHECK(xml_node_compare_order(xml_attr_node(x), xml_attr_node(y)) < 0);
    DOM_CHECK(xml_node_compare_order(a, xml_attr_node(x)) < 0);
    DOM_CHECK(xml_node_compare_order(xml_attr_node(y), xml_node_first_child(a)) < 0);
    DOM_CHECK(xml_node_remove_attr(a, "x", NULL));
    DOM_CHECK(xml_node_first_attr(a) == y && xml_node_prev(xml_attr_node(y)) == NULL);

    // XMQNS handling, same semantics as xmqAddElement/xmqSetAttribute.
    XMQNode *n1 = xml_node_append_child(root, xml_node_new_element(dom, "n1", NULL));
    DOM_CHECK(xml_node_apply_xmqns(n1, NS_HERE("{p}urn:p")) == XMQ_OK);
    DOM_CHECK(!strcmp(xml_node_ns_uri(n1), "urn:p") && !strcmp(xml_node_ns_prefix(n1), "p"));
    XMQNode *n2 = xml_node_append_child(n1, xml_node_new_element(dom, "n2", NULL));
    DOM_CHECK(xml_node_apply_xmqns(n2, NS_PARENT) == XMQ_OK);
    DOM_CHECK(xml_node_ns(n2) == xml_node_ns(n1));
    XMQNode *n3 = xml_node_append_child(n2, xml_node_new_element(dom, "n3", NULL));
    DOM_CHECK(xml_node_apply_xmqns(n3, NS_ANCESTOR("urn:p")) == XMQ_OK);
    DOM_CHECK(xml_node_ns(n3) == xml_node_ns(n1));
    XMQNode *n4 = xml_node_append_child(n2, xml_node_new_element(dom, "n4", NULL));
    DOM_CHECK(xml_node_apply_xmqns(n4, NS_ANCESTOR("urn:q")) == XMQ_OK);
    DOM_CHECK(!strcmp(xml_node_ns_prefix(n4), "ns1") && xml_node_first_ns_decl(root) && xml_ns_prefix(xml_node_first_ns_decl(root)) && !strcmp(xml_ns_prefix(xml_node_first_ns_decl(root)), "ns1"));
    XMQNode *n5 = xml_node_append_child(n2, xml_node_new_element(dom, "n5", NULL));
    DOM_CHECK(xml_node_apply_xmqns(n5, NS_NONE) == XMQ_OK);
    DOM_CHECK(xml_node_ns_uri(n5) == NULL);
    DOM_CHECK(xml_node_apply_xmqns(n5, NS_HERE("{broken")) == XMQ_ERROR_BAD_VALUE);
    XMQReturnAttr ra = xml_node_set_attr_xmqns(n2, "k", "v", NS_ANCESTOR("{p}urn:p"));
    DOM_CHECK(ra.status == XMQ_OK && xml_node_ns(xml_attr_node(ra.attr)) == xml_node_ns(n1));
    DOM_CHECK(xml_node_set_attr_xmqns(n2, "k", "v", NS_HERE("urn:x")).status == XMQ_ERROR_BAD_VALUE);
    DOM_CHECK(!strcmp(xml_ns_href(xml_ns_lookup_prefix(n3, "xml")), XML_DOM_XML_NAMESPACE));
    DOM_CHECK(xml_ns_lookup_href(n3, "urn:p") == xml_node_ns(n1));
    // A closer redeclaration of p shadows the outer one.
    xml_ns_declare(n2, "p", "urn:other");
    DOM_CHECK(xml_ns_lookup_href(n3, "urn:p") == NULL);
    DOM_CHECK(!strcmp(xml_ns_href(xml_ns_lookup_prefix(n3, "p")), "urn:other"));

    // Deep copy into another dom: namespaces declared inside the subtree are remapped,
    // those declared outside are shared and then declared by reconcile.
    XmlDom *dom2 = xml_dom_new();
    XMQNode *copy = xml_node_copy(dom2, n2, true);
    xml_dom_set_root_element(dom2, copy);
    DOM_CHECK(xml_node_dom(copy) == dom2 && xml_node_dom(xml_node_first_child(copy)) == dom2);
    DOM_CHECK(xml_node_ns(copy) == xml_node_ns(n1)); // Shared before reconcile.
    xml_node_reconcile_ns(copy);
    DOM_CHECK(xml_node_ns(copy) != xml_node_ns(n1) && !strcmp(xml_ns_href(xml_node_ns(copy)), "urn:p"));
    XMQNode *copy_n3 = xml_node_first_child(copy);
    DOM_CHECK(!strcmp(xml_node_name(copy_n3), "n3"));
    DOM_CHECK(xml_ns_lookup_prefix(copy_n3, xml_node_ns_prefix(copy_n3)) == xml_node_ns(copy_n3));
    DOM_CHECK(!strcmp(xml_node_ns_uri(copy_n3), "urn:p"));
    // The inner p=urn:other on n2 forces the copied ns to use a fresh prefix.
    DOM_CHECK(strcmp(xml_node_ns_prefix(copy), "p") != 0);
    DOM_CHECK(!strcmp(xml_ns_href(xml_ns_lookup_prefix(copy, "p")), "urn:other"));

    // Freeing the original subtree must not affect the copy (shared ns refs).
    xml_node_free(n1);
    DOM_CHECK(!strcmp(xml_node_ns_uri(copy_n3), "urn:p"));
    DOM_CHECK(!strcmp(xml_node_ns_uri(copy), "urn:p"));

    // Copying the dtd and an attribute with an entity reference into another dom.
    XmlDom *dom3 = xml_dom_new();
    XMQNode *dtd3 = xml_node_copy(dom3, xml_dom_dtd(dom), false);
    xml_node_append_child(xml_dom_document(dom3), dtd3);
    XMQNode *a3 = xml_node_copy(dom3, a, true);
    xml_dom_set_root_element(dom3, a3);
    DOM_CHECK(xml_dom_dtd(dom3) && xml_dtd_first_notation(xml_dom_dtd(dom3)) && xml_dtd_has_declarations(xml_dom_dtd(dom3)));
    DOM_CHECK(dom_str_is(xml_node_string_value(a3), "A&"));
    DOM_CHECK(dom_str_is(xml_node_string_value(xml_attr_node(xml_node_get_attr(a3, "y", NULL))), "2&"));
    xml_dom_free(dom3);
    xml_dom_free(dom);

    // Dtd, set_content on element.
    XMQNode *dt = xml_dom_set_dtd(dom2, "html", NULL, NULL);
    DOM_CHECK(xml_node_first_child(xml_dom_document(dom2)) == dt && xml_dom_dtd(dom2) == dt);
    xml_node_set_content(copy, "replaced");
    DOM_CHECK(xml_node_num_children(copy) == 1);
    DOM_CHECK(dom_str_is(xml_node_string_value(copy), "replaced"));
    xml_dom_free(dom2);
}

int main(int argc, char **argv)
{
#define X(name) name();
    TESTS
#undef X

    if (all_ok_) printf("OK: parts testinternals\n");
    else printf("ERROR: parts testinternals\n");
    return all_ok_ ? 0 : 1;
}
