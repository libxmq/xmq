/* libxmq - Copyright 2023 Fredrik Öhrström (spdx: MIT)

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

#include<assert.h>
#include<ctype.h>
#include<errno.h>
#include<math.h>
#include<setjmp.h>
#include<stdarg.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/HTMLparser.h>
#include<libxml/xmlreader.h>

#include"xmq.h"
#include"grabbed_headers.h"

// DECLARATIONS //////////////////////////////////

bool TEST_MEM_BUFFER();


// DEFINITIONS ///////////////////////////////////

#define TESTS \
    X(test_indented_quotes) \
    X(test_buffer) \
    X(test_xmq) \
    X(test_trimming_quotes) \
    X(test_trimming_comments) \
    X(test_stack) \
    X(test_detect_content) \
    X(test_slashes) \
    X(test_whitespaces) \
    X(test_mem_buffer) \
    X(test_strlen) \
    X(test_escaping) \

bool all_ok_ = true;

const char *content_type_to_string(XMQContentType t)
{
    switch (t)
    {
    case XMQ_CONTENT_UNKNOWN: return "unknown";
    case XMQ_CONTENT_DETECT: return "detect";
    case XMQ_CONTENT_XMQ: return "xmq";
    case XMQ_CONTENT_XML: return "xml";
    case XMQ_CONTENT_HTMQ: return "htmq";
    case XMQ_CONTENT_HTML: return "html";
    case XMQ_CONTENT_JSON: return "json";
    }
    assert(0);
}

void test_buffer()
{
    /*
    InternalBuffer ib;
    new_buffer(&ib, 1);
    const char *s = "abc___";
    append_buffer(&ib, s, s+strlen(s));
    append_buffer(&ib, s, s+strlen(s));
    append_buffer(&ib, s, s+strlen(s));
    append_buffer(&ib, s, s+strlen(s));
    append_buffer(&ib, s, s+strlen(s));
    if (strncmp(ib.buf, "abc___abc___abc___abc___abc___", ib.used))
    {
        all_ok_ = false;
        printf("Buffer test failed!\n");
    }
    free_buffer(&ib);
    */
}


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

void test_xmq()
{
    /*
    char *in = "alfa";

    XMQCallbacks callbacks;
    xmq_setup_callbacks_debug_tokens(&callbacks);
    XMQParseState *state = xmqAllocateParseState(&callbacks);
    xmqTokenizeBuffer(state, in, in+strlen(in), XMQ_CONTENT_XMQ);
    free(state);
    */
}

void test_trim_quote(int first_indent, char *in, char *expected)
{
    char *out = xmq_un_quote(first_indent, ' ', in, in+strlen(in), true);
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Trimming \"%s\"\n", inb);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
    }
    free(out);
}

void test_trim_quote_special(char *in, char *expected)
{
    char *out = xmq_un_quote(0, 0, in, in+strlen(in), true);
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Trimming \"%s\"\n", inb);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
    }
    free(out);
}

void test_trim_comment(int first_indent, char *in, char *expected)
{
    char *out = xmq_un_comment(first_indent, ' ', in, in+strlen(in));
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Trimming \"%s\"\n", inb);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
    }
    free(out);
}

void test_trimming_quotes()
{
    // No newlines means no trimming.
    test_trim_quote(1, " ", " ");
    test_trim_quote(1, "  ", "  ");
    test_trim_quote(1, "  x  ", "  x  ");
    test_trim_quote(1, "  x", "  x");
    test_trim_quote(1, "x", "x");

    // A single newline is removed.
    test_trim_quote(1, "\n", "");
    // A lot of newlines and spaces are removed.
    test_trim_quote(1, "  \n \n    \n\n ", "");
    test_trim_quote(1, "   \n", "");
    test_trim_quote(1, "   \n   ", "");

    // First line leading spaces are kept if there is some non-space on the first line.
    test_trim_quote(10, " x\n ", " x");
    test_trim_quote(1, " x\n ", " x");

    // Incindental is removed.
    test_trim_quote(1, "\n x\n ", "x");
    test_trim_quote(1, "x\n          ", "x");

    // Remove incindental indentation.
    test_trim_quote(1, "abc\n def", "abc\ndef");

    test_trim_quote(1, " abc\n def", " abc\ndef");
    // Incindental is 1 because of first line and second line.
    test_trim_quote(1, "\n 'howdy'\n ", "'howdy'");
    // Incindental is 0 because of second line.
    test_trim_quote(1, "\n'howdy'\n ", "'howdy'");

    // Remove incindetal.
    test_trim_quote(10, "\n    x\n  y\n    z\n", "  x\ny\n  z");


    test_trim_quote_special("HOWDY\n    HOWDY\n    HOWDY", "HOWDY\nHOWDY\nHOWDY");
}

void test_trimming_comments()
{
    // The comment indictor is a triplet "/* " ie slash star space
    // Likewise the comment ending is also a triplet " */" space star slash
    // Thus "/* ALFA */" is just the string "ALFA"
    // However a duo "/*" "*/" is also allowed
    // Thus "/*ALFA*/" is also just the string "ALFA".
    // Apart from this the trimming is identical to the quote trimming.
    test_trim_comment(0, "/**/", "");
    test_trim_comment(0, "/*    */", "  ");
    test_trim_comment(0, "/*\n   ALFA\n   BETA\n   GAMMA\n*/", "ALFA\nBETA\nGAMMA");
    test_trim_comment(0, "/////* ALFA */////", "ALFA");
    test_trim_comment(0, "/*ALFA\n  BETA*/", "ALFA\nBETA");
    test_trim_comment(4,     "/* ALFA\n"
                         "       BETA\n"
                         "*/", "ALFA\nBETA");
}

/*
void test_quote(int indent, char *in, char *expected)
{
    XMQQuoteSettings s = {};
    char *out = xxmq_quote(indent, in, in+strlen(in), &s);
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Quoting \"%s\" with indent %d\n", inb, indent);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
        return;
    }

    char *back = xmq_un_quote(indent, ' ', out, out+strlen(out));
    if (strcmp(back, in))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(out, out+strlen(out));
        char *exb = xmq_quote_as_c(in, in+strlen(in));
        char *gob = xmq_quote_as_c(back, back+strlen(back));

        printf("Trimming back  \"%s\" with indent %d\n", inb, indent);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
    }
    free(back);
    free(out);
}
*/

/*
void test_quote_single_line(int indent, char *in, char *expected)
{
    XMQQuoteSettings s = {};
    s.compact = true;
    char *out = xxmq_quote(indent, in, in+strlen(in), &s);
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Quoting \"%s\" with indent %d\n", inb, indent);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
        free(out);
        return;
    }


    char *back = xmqTrimQuote(indent, out, out+strlen(out));
    if (strcmp(back, in))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(out, out+strlen(out));
        char *exb = xmq_quote_as_c(in, in+strlen(in));
        char *gob = xmq_quote_as_c(back, back+strlen(back));

        printf("Trimming back  \"%s\"\n", inb);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
    }

    free(back);
    free(out);
}
*/

/*
void test_quoting()
{
    test_quote_single_line(10, "howdy\ndowdy",
                               "'howdy'&#10;'dowdy'");
    test_quote_single_line(0, "   alfa\n beta  \n\n\nGamma Delta\n",
                              "'   alfa'&#10;' beta  '&#10;&#10;&#10;'Gamma Delta'&#10;");
    test_quote_single_line(0,
                           " alfa \n 'gurka' \n_''''_\n",
                           "' alfa '&#10;''' 'gurka' '''&#10;'''''_''''_'''''&#10;");

    test_quote_single_line(0, "'''X'''", "&#39;&#39;&#39;'X'&#39;&#39;&#39;");
    test_quote_single_line(0, "X'", "'X'&#39;");
    test_quote_single_line(0, "X'\n", "'X'&#39;&#10;");

    test_quote_single_line(0, "01", "01");

    test_quote(10, "", "''");
    test_quote(10, "x", "x");
    test_quote(10, "/root/home/bar.c", "/root/home/bar.c");
    test_quote(10, "C:\\root\\home", "C:\\root\\home");
    test_quote(10, "//root/home", "'//root/home'");
    test_quote(10, "=47", "'=47'");
    test_quote(10, "47=", "47=");
    test_quote(10, "&", "'&'");
    test_quote(10, "=", "'='");
    test_quote(10, "&nbsp;", "'&nbsp;'");
    test_quote(10, "https://www.vvv.zzz/aaa?x=3&y=4", "https://www.vvv.zzz/aaa?x=3&y=4");
    test_quote(10, " ", "' '");
    test_quote(4, "(", "'('");
    test_quote(-1, "(", "'\n(\n'");
    test_quote(4, ")", "')'");
    test_quote(-1, ")", "'\n)\n'");
    test_quote(4, "{", "'{'");
    test_quote(4, "}", "'}'");
    test_quote(4, "\"", "'\"'");
    test_quote(-1, "\"", "'\n\"\n'");
    test_quote(10, "   123   123   ", "'   123   123   '");
    test_quote(10, "   123\n123   ", "'   123\n           123   '");

    test_quote(4, " ' ", "''' ' '''");
    test_quote(4, " '' ", "''' '' '''");

    test_quote(0, "alfa\nbeta", "'alfa\n beta'");
    test_quote(-1, "alfa\nbeta", "'\nalfa\nbeta\n'");

//    test_quote(4, " ''' ", "'''' ''' ''''");
//    test_quote(4, " '''' ", "''''' '''' '''''");

//    test_quote(4, "'", "&quot;");
    test_quote(4, "'alfa", "'''\n    'alfa\n    '''");
    test_quote(4, "alfa'", "'''\n    alfa'\n    '''");
*/
/*
 Default formatting is like this:
 The implicit indent starts with the content after the quote for 1 or 3 quotes.
    x = 'hejsan'
    y = 'howdy
         bowdy'
    x = '''func(){'foo'};
           call func;'''

    BUT 4 or more quotes always adds newlines and the implicit indent is the start of the quotes.
    x = ''''
        Code with ''' quotes.
        ''''
    And if content starts or ends with quotes.
    x = '''
        'hejsan
        '''
*/
    // Indent 0 is first column, here indent is 8 for quote.
    // |    x = 'howdy
    // |         dowdy'
//    test_quote(8, "howdy\ndowdy", "'howdy\n         dowdy'");

//}

void test_indented_quotes()
{
    /*
'alfa
 beta'
    */
    test_trim_quote(0, "'alfa\n beta'", "alfa\nbeta");
    /*
 'alfa
  beta'
    */
    test_trim_quote(1, "'alfa\n  beta'", "alfa\nbeta");
    /*
  'alfa
   beta'
    */
    test_trim_quote(2, "'alfa\n   beta'", "alfa\nbeta");
    /*
   'alfa
beta'
    */
    test_trim_quote(3, "'alfa\nbeta'", "    alfa\nbeta");


    /*
'alfa
 beta
'
     */
//    test_quote(0, "alfa\nbeta", "'alfa\n beta'");

    /*
 'alfa
  beta'
     */
//    test_quote(1, "alfa\nbeta", "'alfa\n  beta'");

    /*
'
alfa
beta
'
     */
//    test_quote(-1, "alfa\nbeta", "'\nalfa\nbeta\n'");

}

void test_stack()
{
    /*
    Stack *stack = new_stack();
    push_stack(stack, (void*)42);
    assert(stack->size == 1);
    int64_t v = (int64_t)pop_stack(stack);
    if (v != 42) {
        printf("BAD STACK\n");
        all_ok_ = false;
    }
    free_stack(stack);
    */
}
/*
void test_quotec(int indent,
                 char *in,
                 char *expected,
                 const char *indentation_space,
                 const char *explicit_space,
                 const char *explicit_nl,
                 const char *prefix_line,
                 const char *postfix_line)
{
    XMQQuoteSettings s = {};
    s.indentation_space = indentation_space;
    s.explicit_space = explicit_space;
    s.explicit_nl = explicit_nl;
    s.prefix_line = prefix_line;
    s.postfix_line = postfix_line;
    char *out = xxmq_quote(indent, in, in+strlen(in), &s);

    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in));
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected));
        char *gob = xmq_quote_as_c(out, out+strlen(out));

        printf("Quoting \"%s\" with indent %d\n", inb, indent);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
        return;
    }
    free(out);
}
*/
/*
void test_quote_coloring()
{
    test_quotec(10,
                "FIRST",
                "<SPAN>FIRST</SPAN>",
                "&nbsp;",
                "<space>",
                "<BR>\n",
                "<SPAN>",
                "</SPAN>");

    test_quotec(-1,
                "FIRST\nSECOND",
                "'<SPAN></SPAN><BR><SPAN>FIRST</SPAN><BR><SPAN>SECOND</SPAN><BR><SPAN></SPAN>'",
                "&nbsp;",
                "<space>",
                "<BR>",
                "<SPAN>",
                "</SPAN>");

    test_quotec(10,
                "FIRST\nSECOND",
                "'<SPAN>FIRST</SPAN><BR><SPAN>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SECOND</SPAN>'",
                "&nbsp;",
                "<space>",
                "<BR>",
                "<SPAN>",
                "</SPAN>");

    test_quotec(10,
                "FIRST\nSECOND",
                "'<SPAN>FIRST</SPAN><BR><SPAN>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;SECOND</SPAN>'",
                "&nbsp;",
                "<space>",
                "<BR>",
                "<SPAN>",
                "</SPAN>");

    test_quotec(3,
                "FIRST\n    SECOND",
                "'<SPAN>FIRST</SPAN><BR><SPAN>&nbsp;&nbsp;&nbsp;&nbsp;<space><space><space><space>SECOND</SPAN>'",
                "&nbsp;",
                "<space>",
                "<BR>",
                "<SPAN>",
                "</SPAN>");

}
*/
void test_content(const char *content, XMQContentType expected_ct)
{
    XMQContentType ct = xmqDetectContentType(content, content+strlen(content));
    if (ct != expected_ct)
    {
        printf("ERROR: Expected %s but got %s for \"%s\"\n",
               content_type_to_string(expected_ct),
               content_type_to_string(ct),
               content);
        all_ok_ = false;
    }

}
void test_detect_content()
{
    test_content("alfa { beta }", XMQ_CONTENT_XMQ);
    // Yes, true and false are json nodes, but we default to xmq here.
    test_content("true", XMQ_CONTENT_XMQ);

    test_content("<alfa>foo</alfa>", XMQ_CONTENT_XML);
    test_content(" <!doctype   html><html>foo</html>", XMQ_CONTENT_HTML);
    test_content(" <  html>foo</html>", XMQ_CONTENT_HTML);
    test_content("<html", XMQ_CONTENT_HTML);

    test_content("{ }", XMQ_CONTENT_JSON);
    test_content("[ true, false ]", XMQ_CONTENT_JSON);
    test_content("1.123123", XMQ_CONTENT_JSON);
    test_content(" \"foo\" ", XMQ_CONTENT_JSON);
}

size_t count_necessary_slashes(const char *start, const char *stop);

void test_slashes()
{
    const char *howdy = "xxxxALFA*/xxxx";
    size_t n = count_necessary_slashes(howdy, howdy+strlen(howdy));
    if (n != 2) {
        printf("ERROR: Expected 2 slashes!\n");
        all_ok_ = false;
    }
}

size_t count_whitespace(const char *i, const char *stop);

void test_whitespaces()
{
    const char *howdy = " xxx"; // The first char is a non-breakable space U+A0 or utf8 C2 A0.
    size_t n = count_whitespace(howdy, howdy+5); // 5 because space is two bytes.
    if (n != 2) {
        printf("ERROR: Expected 2 utf8 bytes but got %zu!\n", n);
        all_ok_ = false;
    }
}

void test_mem_buffer()
{
    if (!TEST_MEM_BUFFER())
    {
        printf("ERROR: membuffer test failed!\n");
        all_ok_ = false;
    }
}

void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);

void test_sl(const char *s, size_t expected_b_len, size_t expected_u_len)
{
    size_t b_len, u_len;

    str_b_u_len(s, NULL, &b_len, &u_len);

    if (b_len != expected_b_len || u_len != expected_u_len)
    {
        printf("ERROR: test strlen test failed \"%s\" expected %zu %zu but got %zu %zu!\n",
               s, expected_b_len, expected_u_len, b_len, u_len);
        all_ok_ = false;
    }
}
void test_strlen()
{
    const char *start = "HEJSANåäö";
    const char *stop = start+strlen(start);
    size_t b_len = 0;
    size_t u_len = 0;
    str_b_u_len(start, stop, &b_len, &u_len);

    if (b_len != 12 || u_len != 9)
    {
        printf("Expected other strlen values!\n");
        all_ok_ = false;
    }
    test_sl("HOWDY", 5, 5);
    test_sl("åäö", 6, 3);
}




void test_escaping()
{
}


int main(int argc, char **argv)
{
#define X(name) name();
    TESTS
#undef X

    if (all_ok_) printf("OK: testinternals\n");
    else printf("ERROR: testinternals\n");
    return all_ok_ ? 0 : 1;
}
