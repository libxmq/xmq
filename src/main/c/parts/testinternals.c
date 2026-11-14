
#include"colors.h"
#include"core.h"
#include"default_themes.h"
#include"hashmap.h"
#include"quicksort_strings.h"
#include"stack.h"
#include"text.h"
#include"xml_dom.h"
#include"xml_parser.h"
#include"xml_save.h"
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
    X(test_xml_save) \
    X(test_xml_parser) \


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

static void check_xml_save_node(XMQNode *n, const char *expected)
{
    char *got = xml_save_node(n, 0, false, NULL);
    if (strcmp(got, expected))
    {
        all_ok_ = false;
        printf("ERROR: test_xml_save got \"%s\" expected \"%s\"\n", got, expected);
    }
    free(got);
}

void test_xml_save()
{
    XmlDom *dom = xml_dom_new();
    XMQNode *root = xml_node_new_element(dom, "r", NULL);
    xml_dom_set_root_element(dom, root);

    // Text escaping: & < > and \r, but not quotes. Control chars are invalid in xml and become U+FFFD.
    xml_node_append_child(root, xml_node_new_text(dom, "a&b<c>d\"e'f\rg\x01h"));
    check_xml_save_node(root, "<r>a&amp;b&lt;c&gt;d\"e'f&#13;g&#xFFFD;h</r>");

    // Attribute escaping: also tab, newline and double quote.
    xml_node_set_content(root, NULL);
    xml_node_set_attr(root, "x", NULL, "1\t2\n3\"4'5&6<7");
    check_xml_save_node(root, "<r x=\"1&#9;2&#10;3&quot;4'5&amp;6&lt;7\"/>");

    // A cdata section containing ]]> is split, like libxml2.
    XMQNode *c = xml_node_new_cdata(dom, "a]]>b", 5);
    check_xml_save_node(c, "<![CDATA[a]]]]><![CDATA[>b]]>");
    xml_node_free(c);

    // Pi with and without data, comment, entity reference in an attribute.
    XMQNode *pi1 = xml_node_new_pi(dom, "t", NULL);
    check_xml_save_node(pi1, "<?t?>");
    XMQNode *pi2 = xml_node_new_pi(dom, "t", "d");
    check_xml_save_node(pi2, "<?t d?>");
    xml_node_free(pi1);
    xml_node_free(pi2);
    XMQAttr *y = xml_node_set_attr(root, "y", NULL, "v");
    xml_node_append_child(xml_attr_node(y), xml_node_new_entity_ref(dom, "e"));
    check_xml_save_node(root, "<r x=\"1&#9;2&#10;3&quot;4'5&amp;6&lt;7\" y=\"v&e;\"/>");

    // Quoting of doctype ids: " unless the string has ", then ' unless it has both.
    MemBuffer *mb = new_membuffer();
    xml_save_write_quoted(mb, "a");
    xml_save_write_quoted(mb, "b\"c");
    xml_save_write_quoted(mb, "d\"e'f");
    membuffer_append_null(mb);
    if (strcmp(mb->buffer_, "\"a\"'b\"c'\"d&quot;e'f\""))
    {
        all_ok_ = false;
        printf("ERROR: test_xml_save quoting got %s\n", mb->buffer_);
    }
    free_membuffer_and_free_content(mb);

    // Whole document with declaration, and formatted output.
    char *doc = xml_save_doc(dom, "utf-8", false, NULL);
    if (strcmp(doc, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<r x=\"1&#9;2&#10;3&quot;4'5&amp;6&lt;7\" y=\"v&e;\"/>\n"))
    {
        all_ok_ = false;
        printf("ERROR: test_xml_save doc got %s\n", doc);
    }
    free(doc);
    xml_node_remove_attr(root, "x", NULL);
    xml_node_remove_attr(root, "y", NULL);
    XMQNode *k = xml_node_append_child(root, xml_node_new_element(dom, "k", NULL));
    xml_node_append_child(k, xml_node_new_element(dom, "l", NULL));
    doc = xml_save_doc(dom, NULL, true, NULL);
    if (strcmp(doc, "<?xml version=\"1.0\"?>\n<r>\n  <k>\n    <l/>\n  </k>\n</r>\n"))
    {
        all_ok_ = false;
        printf("ERROR: test_xml_save formatted doc got %s\n", doc);
    }
    free(doc);
    xml_dom_free(dom);
}

static int parser_messages_ = 0;
static void count_parser_message(void *user, const char *msg)
{
    (void)user; (void)msg;
    parser_messages_++;
}

/** Parse and serialize, compare with expected (NULL expects a parse error containing error_part). */
static void check_xml_parse(const char *in, size_t len, XmlParseSettings *settings, const char *expected, const char *error_part)
{
    XmlParseError err;
    XmlDom *dom = xml_parse_buffer(in, in + len, settings, &err);
    if (!expected)
    {
        if (dom)
        {
            all_ok_ = false;
            printf("ERROR: test_xml_parser expected an error for \"%s\"\n", in);
            xml_dom_free(dom);
        }
        else if (error_part && (!err.short_message || !strstr(err.short_message, error_part)))
        {
            all_ok_ = false;
            printf("ERROR: test_xml_parser expected error \"%s\" got \"%s\"\n", error_part, err.short_message ? err.short_message : "(null)");
        }
        xml_parse_error_free(&err);
        return;
    }
    if (!dom)
    {
        all_ok_ = false;
        printf("ERROR: test_xml_parser failed to parse \"%s\": %s\n", in, err.short_message);
        xml_parse_error_free(&err);
        return;
    }
    char *got = xml_save_doc(dom, "utf-8", false, NULL);
    if (strcmp(got, expected))
    {
        all_ok_ = false;
        printf("ERROR: test_xml_parser\n  input:    %s\n  got:      %s  expected: %s", in, got, expected);
    }
    free(got);
    xml_dom_free(dom);
}

#define XML_DECL "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
#define PARSE_OK(in, out) check_xml_parse(in, strlen(in), &st, XML_DECL out, NULL)
#define PARSE_ERR(in, part) check_xml_parse(in, strlen(in), &st, NULL, part)

void test_xml_parser()
{
    XmlParseSettings st;
    xml_parse_settings_default(&st);
    st.no_cdata = true;

    // Character references and predefined entities are decoded, declared entities are kept.
    PARSE_OK("<r a='&#65;&lt;&amp;x'>&#x42;&gt;&apos;</r>", "<r a=\"A&lt;&amp;x\">B&gt;'</r>\n");
    PARSE_OK("<!DOCTYPE r [<!ENTITY e 'E'>]><r a='x&e;y'>&e;</r>",
             "<!DOCTYPE r [\n<!ENTITY e \"E\">\n]>\n<r a=\"x&e;y\">&e;</r>\n");
    // Parameter entity references between declarations are kept, never expanded.
    PARSE_OK("<!DOCTYPE r [<!ENTITY % p '<!ENTITY x \"y\">'>%p;]><r/>",
             "<!DOCTYPE r [\n<!ENTITY % p '<!ENTITY x \"y\">'>\n%p;\n]>\n<r/>\n");
    // A billion laughs document is harmless since nothing is expanded.
    PARSE_OK("<!DOCTYPE r [<!ENTITY a 'aaaaaaaaaa'><!ENTITY b '&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;'>"
             "<!ENTITY c '&b;&b;&b;&b;&b;&b;&b;&b;&b;&b;'><!ENTITY d '&c;&c;&c;&c;&c;&c;&c;&c;&c;&c;'>]><r>&d;&d;</r>",
             "<!DOCTYPE r [\n<!ENTITY a \"aaaaaaaaaa\">\n<!ENTITY b \"&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;\">\n"
             "<!ENTITY c \"&b;&b;&b;&b;&b;&b;&b;&b;&b;&b;\">\n<!ENTITY d \"&c;&c;&c;&c;&c;&c;&c;&c;&c;&c;\">\n]>\n<r>&d;&d;</r>\n");
    // Content models are printed like libxml2, including its normalizations.
    PARSE_OK("<!DOCTYPE r [<!ELEMENT r (a*)><!ELEMENT s (a | b? | c)*><!ELEMENT t (#PCDATA|a)*>]><r/>",
             "<!DOCTYPE r [\n<!ELEMENT r (a)*>\n<!ELEMENT s (a | b | c)*>\n<!ELEMENT t (#PCDATA | a)*>\n]>\n<r/>\n");
    // Values of non CDATA attributes are normalized.
    PARSE_OK("<!DOCTYPE r [<!ATTLIST r i ID #IMPLIED c CDATA #IMPLIED>]><r i='  a  b ' c='  a  b '/>",
             "<!DOCTYPE r [\n<!ATTLIST r i ID #IMPLIED>\n<!ATTLIST r c CDATA #IMPLIED>\n]>\n<r i=\"a b\" c=\"  a  b \"/>\n");
    // Namespaces.
    PARSE_OK("<a:r xmlns:a='urn:a' xmlns='urn:d'><b a:x='1'/></a:r>",
             "<a:r xmlns:a=\"urn:a\" xmlns=\"urn:d\"><b a:x=\"1\"/></a:r>\n");
    parser_messages_ = 0;
    st.message_callback = count_parser_message;
    PARSE_OK("<p:r/>", "<p:r/>\n");
    if (parser_messages_ != 1) { all_ok_ = false; printf("ERROR: test_xml_parser expected a namespace message\n"); }
    st.message_callback = NULL;
    // Line endings are normalized, cdata is merged into text when no_cdata.
    PARSE_OK("<r>a\r\nb\rc<![CDATA[<d>]]></r>", "<r>a\nb\nc&lt;d&gt;</r>\n");
    st.no_cdata = false;
    PARSE_OK("<r><![CDATA[<d>]]></r>", "<r><![CDATA[<d>]]></r>\n");
    st.no_cdata = true;

    // Ignorable blanks.
    st.no_blanks = true;
    PARSE_OK("<r>\n  <a/>\n  <b> x </b>\n</r>", "<r><a/><b> x </b></r>\n");
    PARSE_OK("<r xml:space='preserve'>\n  <a/>\n</r>", "<r xml:space=\"preserve\">\n  <a/>\n</r>\n");
    st.no_blanks = false;

    // Encodings: UTF-16 with bom, latin1 by declaration.
    const char utf16[] = "\xFF\xFE<\0r\0>\0\xE5\0<\0/\0r\0>\0";
    check_xml_parse(utf16, sizeof(utf16) - 1, &st, XML_DECL "<r>\xC3\xA5</r>\n", NULL);
    const char latin1[] = "<?xml version='1.0' encoding='ISO-8859-1'?><r>\xE5</r>";
    check_xml_parse(latin1, sizeof(latin1) - 1, &st,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<r>\xC3\xA5</r>\n", NULL);

    // Errors.
    PARSE_ERR("", "Document is empty");
    PARSE_ERR("<r>", "Premature end of data");
    PARSE_ERR("<r></s>", "Opening and ending tag mismatch");
    PARSE_ERR("<r a='1' a='2'/>", "redefined");
    PARSE_ERR("<r>&undeclared;</r>", "Entity 'undeclared' not defined");
    PARSE_ERR("<r>\xC3</r>", "not proper UTF-8");
    PARSE_ERR("<r>\x01</r>", "out of allowed range");
    PARSE_ERR("<r>&#0;</r>", "invalid xmlChar value");
    PARSE_ERR("<r><!-- a -- b --></r>", "Double hyphen");
    PARSE_ERR("<r>]]></r>", "']]>' not allowed");

    // Error location.
    XmlParseError err;
    const char *bad = "<r>\n<a>\n</b></r>";
    XmlDom *dom = xml_parse_buffer(bad, bad + strlen(bad), &st, &err);
    if (dom || err.line != 3)
    {
        all_ok_ = false;
        printf("ERROR: test_xml_parser expected error on line 3, got line %zu\n", err.line);
    }
    xml_dom_free(dom);
    xml_parse_error_free(&err);

    // Limits.
    st.max_depth = 3;
    PARSE_OK("<a><b><c/></b></a>", "<a><b><c/></b></a>\n");
    PARSE_ERR("<a><b><c><d/></c></b></a>", "Excessive depth");
    xml_parse_settings_default(&st);
    st.max_name_length = 4;
    PARSE_ERR("<abcde/>", "Name too long");
    xml_parse_settings_default(&st);
    st.max_text_length = 4;
    PARSE_ERR("<r>abcde</r>", "too long");

    // A deep document is parsed without recursion.
    xml_parse_settings_default(&st);
    st.max_depth = 100000;
    MemBuffer *deep = new_membuffer();
    for (int i = 0; i < 50000; ++i) membuffer_append(deep, "<a>");
    for (int i = 0; i < 50000; ++i) membuffer_append(deep, "</a>");
    dom = xml_parse_buffer(deep->buffer_, deep->buffer_ + deep->used_, &st, &err);
    if (!dom) { all_ok_ = false; printf("ERROR: test_xml_parser deep document: %s\n", err.short_message); }
    char *out = dom ? xml_save_doc(dom, "utf-8", false, NULL) : NULL;
    MemBuffer *expect = new_membuffer();
    membuffer_append(expect, XML_DECL);
    for (int i = 0; i < 49999; ++i) membuffer_append(expect, "<a>");
    membuffer_append(expect, "<a/>");
    for (int i = 0; i < 49999; ++i) membuffer_append(expect, "</a>");
    membuffer_append(expect, "\n");
    membuffer_append_null(expect);
    if (out && strcmp(out, expect->buffer_)) { all_ok_ = false; printf("ERROR: test_xml_parser deep document output\n"); }
    free_membuffer_and_free_content(expect);
    free(out);
    xml_dom_free(dom);
    xml_parse_error_free(&err);
    free_membuffer_and_free_content(deep);
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
