/* libxmq - Copyright 2023-2025 Fredrik Öhrström (spdx: MIT)

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
#include"parts/xmq_internals.h"
#include"parts/text.h"
#include"parts/membuffer.h"
#include"parts/xmq_printer.h"

// DEFINITIONS ///////////////////////////////////

const char *test_content_type_to_string(XMQContentType t);
void test_content(const char *content, XMQContentType expected_ct);
void test_mem_buffer();
void test_sl(const char *s, size_t expected_b_len, size_t expected_u_len);
void test_trim_comment(int start_col, const char *in, const char *expected);
void test_trim_quote(const char *in, const char *expected);
void test_quote(int indent, bool compact, char *in, char *expected);

#define TESTS \
    X(test_indented_quotes) \
    X(test_buffer) \
    X(test_xmq) \
    X(test_trimming_quotes) \
    X(test_trimming_comments) \
    X(test_detect_content) \
    X(test_slashes) \
    X(test_quoting) \
    X(test_whitespaces) \
    X(test_strlen) \
    X(test_escaping) \
    X(test_yaep) \
    X(test_yaep_reuse_grammar) \

#define X(name) void name();
    TESTS
#undef X

bool all_ok_ = true;

const char *test_content_type_to_string(XMQContentType t)
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
    case XMQ_CONTENT_IXML: return "ixml";
    case XMQ_CONTENT_TEXT: return "text";
    case XMQ_CONTENT_CLINES: return "clines";
    }
    assert(0);
    return "?";
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

void test_trim_quote(const char *in, const char *expected)
{
    char *out = xmq_un_quote(in, in+strlen(in), true, true);
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in), false);
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected), false);
        char *gob = xmq_quote_as_c(out, out+strlen(out), false);

        printf("Trimming \"%s\"\n", inb);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
        exit(1);
    }
    free(out);
}


void test_trim_comment(int start_col, const char *in, const char *expected)
{
    char *out = xmq_un_comment(in, in+strlen(in));
    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in), false);
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected), false);
        char *gob = xmq_quote_as_c(out, out+strlen(out), false);

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
    test_trim_quote(" ", " ");
    test_trim_quote("  ", "  ");
    test_trim_quote("  x  ", "  x  ");
    test_trim_quote("  x", "  x");
    test_trim_quote("x", "x");

    // A single newline is removed.
    test_trim_quote("\n", "");
    // A lot spaces are removed and one less newline.
    test_trim_quote("  \n \n    \n\n ", "\n\n\n");
    test_trim_quote("   \n", "");
    test_trim_quote("   \n   ", "");

    // First line leading spaces are kept if there is some non-space on the first line.
    test_trim_quote(" x\n ", " x");
    test_trim_quote(" x\n ", " x");

    // Incidental is removed.
    test_trim_quote("\n x\n ", "x");
    test_trim_quote("x\n          ", "x");

    // Remove incidental indentation. Source code indentation is colum 2 (= 1 space before)
    // which corresponds to abc and def being aligned.
    test_trim_quote("abc\n def", "abc\ndef");

    // Yes, the abc has one extra indentation.
    test_trim_quote(" abc\n def", " abc\ndef");
    // Incidental is 1 because of first line and second line.
    test_trim_quote("\n QhowdyQ\n ", "QhowdyQ");
    // Incidental is 0 because of second line.
    test_trim_quote("\nQhowdyQ\n ", "QhowdyQ");

    // Remove incidetal. Indentation number irrelevant since first line is empty.
    test_trim_quote("\n    x\n  y\n    z\n", "  x\ny\n  z");

    // Assume first line has the found incidental indentation.
    test_trim_quote("HOWDY\n    HOWDY\n    HOWDY", "HOWDY\nHOWDY\nHOWDY");

    // Remove incidental. Indentation number irrelevant since first line is empty.
    test_trim_quote("\n    x\n  y\n    z\n", "  x\ny\n  z");

    // Last line influences incidental indentation, even if it is all spaces.
    test_trim_quote("\n    x\n  ", "  x");
    test_trim_quote("\n    x\n\n  ", "  x\n");

}

void test_trimming_comments()
{
    // The comment indictor is a triplet "/* " ie slash star space
    // Likewise the comment ending is also a triplet " */" space star slash
    // Thus "/* ALFA */" is just the string "ALFA"
    // However a duo "/*" "*/" is also allowed
    // Thus "/*ALFA*/" is also just the string "ALFA".
    // Apart from this the trimming is identical to the quote trimming.
    test_trim_comment(17, "/**/", "");
    test_trim_comment(17, "/*    */", "  ");
    // The indent is ignored since the first line is empty.
    test_trim_comment(17, "/*\n   ALFA\n   BETA\n   GAMMA\n*/", "ALFA\nBETA\nGAMMA");
    test_trim_comment(17, "/////* ALFA */////", "ALFA");
    test_trim_comment(17, "/////*ALFA*/////", "ALFA");
    test_trim_comment(1, "/*ALFA\n  BETA*/", "ALFA\nBETA");
    test_trim_comment(1, "/* ALFA\n   BETA*/", "ALFA\nBETA");
    test_trim_comment(5,     "/* ALFA\n"
                         "       BETA */"
                       , "ALFA\nBETA");
}

void test_quote(int indent, bool compact, char *in, char *expected)
{
    XMQOutputSettings *os = xmqNewOutputSettings();
    if (compact)
    {
        os->compact = true;
        os->escape_newlines = true;
    }
    os->allow_json_quotes = false;
    xmqSetupPrintMemory(os, NULL, NULL);

    XMQPrintState ps;
    memset(&ps, 0, sizeof(XMQPrintState));
    ps.output_settings = os;

    xmlNode *parent = xmlNewNode(NULL, (xmlChar*)"test");
    xmlNode *node = xmlNewText((const xmlChar *)in);
    xmlAddChild(parent,node);

    ps.current_indent = indent;
    ps.line_indent = indent;
    print_node(&ps, parent, LEVEL_XMQ);

    xmlUnlinkNode(node);
    xmlFreeNode(node);
    xmlFreeNode(parent);

    size_t size = membuffer_used(os->output_buffer);
    char *out = free_membuffer_but_return_trimmed_content(os->output_buffer);
    out = realloc(out, size+1);
    out[size] = 0;

    if (strcmp(out, expected))
    {
        all_ok_ = false;
        char *inb = xmq_quote_as_c(in, in+strlen(in), false);
        char *exb = xmq_quote_as_c(expected, expected+strlen(expected), false);
        char *gob = xmq_quote_as_c(out, out+strlen(out), false);

        printf("Quoting \"%s\" with indent %d\n", inb, indent);
        printf("expected \"%s\"\n", exb);
        printf("but got  \"%s\"\n", gob);

        free(inb);
        free(exb);
        free(gob);
        free(out);
        exit(1);
    }

    if (!compact)
    {
        // Can only test non-compact quotes back for the moment.
        // "test = " or "test="
        size_t skip = 7;
        if (compact) skip = 5;
        char *trimmed = xmq_un_quote(out+skip, out+size, true, true);

        if (strcmp(trimmed, in))
        {
            all_ok_ = false;
            char *inb = xmq_quote_as_c(out, out+strlen(out), false);
            char *exb = xmq_quote_as_c(in, in+strlen(in), false);
            char *gob = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed), false);

            printf("Trimming back  \"%s\"\n", inb);
            printf("expected \"%s\"\n", exb);
            printf("but got  \"%s\"\n", gob);

            free(inb);
            free(exb);
            free(gob);
        }
        free(trimmed);
    }
    free(out);
    xmqFreeOutputSettings(os);
}

void test_quoting()
{
    test_quote(10, true, "howdy\ndowdy", "test=('howdy'&#10;'dowdy')");
    test_quote(0, false, "howdy\ndowdy", "test = 'howdy\n        dowdy'");

    test_quote(0, true, "   alfa\n beta  \n\n\nGamma Delta\n",
                        "test=('   alfa'&#10;' beta  '&#10;&#10;&#10;'Gamma Delta'&#10;)");
/* TODO    test_quote(0, true,
               " alfa \n 'gurka' \n_''''_\n",
               "test=(' alfa '&#10;''' 'gurka' '''&#10;'''''_''''_'''''&#10;)");*/

    test_quote(0, true,
               "'''X'''",
               "test=(&#39;&#39;&#39;'X'&#39;&#39;&#39;)");
    test_quote(0, true,
               "X'",
               "test=('X'&#39;)");
    test_quote(0, true,
               "X'\n",
               "test=('X'&#39;&#10;)");

    test_quote(0, true,
               "01", "test=01");
    test_quote(10, false, "", "test = ''");
    test_quote(10, false, "x", "test = x");
    test_quote(10, false, "/root/home/bar.c", "test = /root/home/bar.c");
    test_quote(10, false, "C:\\root\\home", "test = C:\\root\\home");
// TODO    test_quote(10, false, "//root/home", "test = '//root/home'");
// TODO    test_quote(10, false, "=47", "test = '=47'");
    test_quote(10, false, "47=", "test = 47=");

// TODO    test_quote(10, false,"&", "test = '&'");
// TODO    test_quote(10, false, "=", "test = '='");
// TODO    test_quote(10, false,"&nbsp;", "test = '&nbsp;'");
    test_quote(10, false, "https://www.vvv.zzz/aaa?x=3&y=4", "test = https://www.vvv.zzz/aaa?x=3&y=4");
    test_quote(10, false, " ", "test = ' '");
    test_quote(0, false, "(", "test = '('");
// TODO    test_quote(-1, false, "(", "test = '\n(\n'");
    /*
    test_quote(4, ")", "')'");
    test_quote(-1, ")", "'\n)\n'");
    test_quote(4, "{", "'{'");
    test_quote(4, "}", "'}'");
    test_quote(4, "\"", "'\"'");
    test_quote(-1, "\"", "'\n\"\n'");
    test_quote(10, "   123   123   ", "'   123   123   '");
    test_quote(10, "   123\n123   ", "'   123\n           123   '");
    */
    test_quote(4, false, " ' ", "test = ''' ' '''");
    test_quote(4, false, " '' ", "test = ''' '' '''");

    test_quote(0, false, "alfa\nbeta", "test = 'alfa\n        beta'");
    test_quote(1, false, "alfa\nbeta", "test = 'alfa\n         beta'");

//TODO     test_quote(4, false, " ''' ", "test = '''' ''' ''''");
//TODO //    test_quote(4, " '''' ", "''''' '''' '''''");

//TODO    test_quote(0, false, "'", "test = &apos;");
    test_quote(0, false, "'alfa", "test = '''\n       'alfa\n       '''");
    test_quote(1, false, "'alfa", "test = '''\n        'alfa\n        '''");
    test_quote(0, false, "alfa'", "test = '''\n       alfa'\n       '''");
    test_quote(1, false, "alfa'", "test = '''\n        alfa'\n        '''");

}

void test_indented_quotes()
{
    /*
'alfa
 beta'
    */
//    test_trim_quote(1, "'alfa\n beta'", "alfa\nbeta");
    /*
 'alfa
  beta'
    */
//    test_trim_quote(1, "'alfa\n  beta'", "alfa\nbeta");
    /*
  'alfa
   beta'
    */
//    test_trim_quote(2, "'alfa\n   beta'", "alfa\nbeta");
    /*
   'alfa
beta'
    */
//    test_trim_quote(3, "'alfa\nbeta'", "    alfa\nbeta");


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
               test_content_type_to_string(expected_ct),
               test_content_type_to_string(ct),
               content);
        all_ok_ = false;
    }

}
void test_detect_content()
{
    test_content("alfa { beta }", XMQ_CONTENT_XMQ);
    // true false and null could be valid xmq/xml nodes
    // however it is much more likely that this is json...
    test_content("true", XMQ_CONTENT_JSON);

    test_content("<alfa>foo</alfa>", XMQ_CONTENT_XML);
    test_content(" <!doctype   html><html>foo</html>", XMQ_CONTENT_HTML);
    test_content(" <  html>foo</html>", XMQ_CONTENT_HTML);
    test_content("<html", XMQ_CONTENT_HTML);

    test_content("{ }", XMQ_CONTENT_JSON);
    test_content("[ true, false ]", XMQ_CONTENT_JSON);
    test_content("1.123123", XMQ_CONTENT_JSON);
    test_content(" \"foo\" ", XMQ_CONTENT_JSON);
}

void test_slashes()
{
    const char *howdy = "xxxxALFA*/xxxx";
    size_t n = count_necessary_slashes(howdy, howdy+strlen(howdy));
    if (n != 2) {
        printf("ERROR: Expected 2 slashes!\n");
        all_ok_ = false;
    }
}

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
    /*
    if (!TEST_MEM_BUFFER())
    {
        printf("ERROR: membuffer test failed!\n");
        all_ok_ = false;
    }
    */
}

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

void test_ixml_case(const char *ixml, const char *input, const char *expected);

void test_ixml_case(const char *ixml, const char *input, const char *expected)
{
    bool ok = false;

    XMQDoc *grammar = xmqNewDoc();
    ok = xmqParseBufferWithType(grammar, ixml, NULL, NULL, XMQ_CONTENT_IXML, 0);
    if (!ok)
    {
        printf("Could not parse ixml!\n");
        all_ok_ = false;
        return;
    }

    XMQDoc *dom = xmqNewDoc();
    ok = xmqParseBufferWithIXML(dom, input, NULL, grammar, 0);
    if (!ok)
    {
        printf("Could not parse content with ixml grammar!\n");
        all_ok_ = false;
        return;
    }

    XMQLineConfig *lc = xmqNewLineConfig();
    char *line = xmqLineDoc(lc, dom);

    if (strcmp(line, expected))
    {
        printf("ERROR: ixml parsing failed.\n"
               "ixml:   %s\n"
               "input:  %s\n"
               "expect: %s\n"
               "got:    %s\n",
               ixml, input, expected, line);
    }
    free(line);
    xmqFreeLineConfig(lc);
    xmqFreeDoc(dom);
    xmqFreeDoc(grammar);
}

void test_yaep()
{
    test_ixml_case("words = ~[]*.", "alfa beta gamma", "words='alfa beta gamma'\n");
    test_ixml_case("a=n++-','.n=[N]+.", "123,9,455,123", "a{n=123 n=9 n=455 n=123}\n");
}

void test_ixml_grammar(XMQDoc *grammar, const char *ixml, const char *input, const char *expected);

void test_ixml_grammar(XMQDoc *grammar, const char *ixml, const char *input, const char *expected)
{
    bool ok = false;

    XMQDoc *dom = xmqNewDoc();
    ok = xmqParseBufferWithIXML(dom, input, NULL, grammar, 0);
    if (!ok)
    {
        printf("Could not parse content with ixml grammar!\n");
        all_ok_ = false;
        return;
    }

    XMQLineConfig *lc = xmqNewLineConfig();
    char *line = xmqLineDoc(lc, dom);

    if (strcmp(line, expected))
    {
        printf("ERROR: ixml parsing failed.\n"
               "ixml:   %s\n"
               "input:  %s\n"
               "expect: %s\n"
               "got:    %s\n",
               ixml, input, expected, line);
    }
    free(line);
    xmqFreeLineConfig(lc);
    xmqFreeDoc(dom);
}

void test_yaep_reuse_grammar()
{
    bool ok = false;

    XMQDoc *grammar = xmqNewDoc();
    const char *ixml = "words = ~[]*.";
    ok = xmqParseBufferWithType(grammar, ixml, NULL, NULL, XMQ_CONTENT_IXML, 0);
    if (!ok)
    {
        printf("Could not parse ixml!\n");
        all_ok_ = false;
        return;
    }

    test_ixml_grammar(grammar, ixml, "abc", "words=abc\n");
    test_ixml_grammar(grammar, ixml, "xyz", "words=xyz\n");

    xmqFreeDoc(grammar);
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
