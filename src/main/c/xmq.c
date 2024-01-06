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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include"xmq.h"

#define BUILDING_XMQ

// PART HEADERS //////////////////////////////////////////////////

#include"parts/always.h"
#include"parts/utf8.h"
#include"parts/hashmap.h"
#include"parts/membuffer.h"
#include"parts/stack.h"
#include"parts/text.h"
#include"parts/entities.h"
#include"parts/xml.h"
#include"parts/xmq_parser.h"
#include"parts/xmq_printer.h"

// XMQ STRUCTURES ////////////////////////////////////////////////

#include"parts/xmq_internals.h"

// FUNCTIONALITY /////////////////////////////////////////////////

#include"parts/json.h"

//////////////////////////////////////////////////////////////////////////////////

void add_nl(XMQParseState *state);
XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data);
size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line);
void copy_and_insert(MemBuffer *mb, const char *start, const char *stop, int num_prefix_spaces, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
char *copy_lines(int num_prefix_spaces, const char *start, const char *stop, int num_quotes, bool add_nls, bool add_compound, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os);
xmlNodePtr create_entity(XMQParseState *state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char*stop, xmlNodePtr parent);
void create_node(XMQParseState *state, const char *start, const char *stop);
xmlNodePtr create_quote(XMQParseState *state, size_t l, size_t col, const char *start, size_t ccol, const char *cstart, const char *cstop, const char *stop,  xmlNodePtr parent);
void debug_content_comment(XMQParseState *state, size_t line, size_t start_col, const char *start, size_t inden, const char *cstart, const char *cstop, const char*stop);
void debug_content_value(XMQParseState *state, size_t line, size_t start_col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void debug_content_quote(XMQParseState *state, size_t line, size_t start_col, const char *start, size_t inden, const char *cstart, const char *cstop, const char*stop);
void do_attr_key(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_attr_ns(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_attr_ns_declaration(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_attr_value_compound_entity(XMQParseState *state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_compound_quote(XMQParseState *state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_entity(XMQParseState *state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_text(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_attr_value_quote(XMQParseState*state, size_t line, size_t col, const char *start, size_t i, const char *cstart, const char *cstop, const char *stop);
void do_comment(XMQParseState*state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_comment_continuation(XMQParseState*state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_apar_left(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_apar_right(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_brace_left(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_brace_right(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_cpar_left(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_cpar_right(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_equals(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_key(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_name(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_ns(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_value_compound_entity(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_value_compound_quote(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_value_entity(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_value_text(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_element_value_quote(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_entity(XMQParseState *state, size_t l, size_t c, const char *start, size_t indent, const char *cstart, const char *cstop, const char*stop);
void do_ns_colon(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
void do_quote(XMQParseState *state, size_t l, size_t col, const char *start, size_t ccol, const char *cstart, const char *cstop, const char *stop);
void do_whitespace(XMQParseState *state, size_t line, size_t col, const char *start, size_t indent, const char *cstart, const char *cstop, const char *stop);
bool find_line(const char *start, const char *stop, size_t *indent, const char **after_last_non_space, const char **eol);
const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop);
const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop);
void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared);
void fixup_comments(XMQDoc *doq, xmlNode *node);
bool has_leading_ending_quote(const char *start, const char *stop);
bool is_safe_char(const char *i, const char *stop);
size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq);
bool load_file(XMQDoc *doq, const char *file, size_t *out_fsize, const char **out_buffer);
bool load_stdin(XMQDoc *doq, size_t *out_fsize, const char **out_buffer);
bool need_separation_before_entity(XMQPrintState *ps);
size_t num_utf8_bytes(char c);
void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num);
void print_namespace(XMQPrintState *ps, xmlNs *ns, size_t align);
void reset_ansi(XMQParseState *state);
void reset_ansi_nl(XMQParseState *state);
void setup_htmq_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw);
const char *skip_any_potential_bom(const char *start, const char *stop);
bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop);
bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop);
void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop);
void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop);
bool xmqVerbose();
void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks);
bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt);
bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt);
void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_json(XMQDoc *doq, XMQOutputSettings *output_settings);
char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings);
char *xmq_quote_default(int indent, const char *start, const char *stop, XMQQuoteSettings *settings);
bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);

// Declare tokenize_whitespace tokenize_name functions etc...
#define X(TYPE) void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, size_t indent,const char *cstart, const char *cstop, const char *stop);
LIST_OF_XMQ_TOKENS
#undef X

// Declare debug_whitespace debug_name functions etc...
#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,size_t indent,const char*cstart,const char*cstop,const char*stop);
LIST_OF_XMQ_TOKENS
#undef X

//////////////////////////////////////////////////////////////////////////////////
char ansi_reset_color[] = "\033[0m";

void xmqSetupDefaultColors(XMQOutputSettings *os, bool dark_mode)
{
    XMQColoring *c = hashmap_get(os->colorings, "");
    assert(c);
    memset(c, 0, sizeof(XMQColoring));
    os->indentation_space = " ";
    os->explicit_space = " ";
    os->explicit_nl = "\n";
    os->explicit_tab = "\t";
    os->explicit_cr = "\r";

    if (os->render_to == XMQ_RENDER_PLAIN)
    {
    }
    else
    if (os->render_to == XMQ_RENDER_TERMINAL)
    {
        setup_terminal_coloring(os, c, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_HTML)
    {
        setup_html_coloring(os, c, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_TEX)
    {
        setup_tex_coloring(os, c, dark_mode, os->use_color, os->render_raw);
    }

    if (os->only_style)
    {
        printf("%s\n", c->style.pre);
        exit(0);
    }

}

void setup_terminal_coloring(XMQOutputSettings *os, XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    if (!use_color) return;
    if (dark_mode)
    {
        c->whitespace.pre  = NOCOLOR;
        c->tab_whitespace.pre  = RED_BACKGROUND;
        c->unicode_whitespace.pre  = RED_UNDERLINE;
        c->equals.pre      = NOCOLOR;
        c->brace_left.pre  = NOCOLOR;
        c->brace_right.pre = NOCOLOR;
        c->apar_left.pre    = NOCOLOR;
        c->apar_right.pre   = NOCOLOR;
        c->cpar_left.pre    = MAGENTA;
        c->cpar_right.pre   = MAGENTA;
        c->quote.pre = GREEN;
        c->entity.pre = MAGENTA;
        c->comment.pre = CYAN;
        c->comment_continuation.pre = CYAN;
        c->element_ns.pre = GRAY;
        c->element_name.pre = ORANGE;
        c->element_key.pre = LIGHT_BLUE;
        c->element_value_text.pre = GREEN;
        c->element_value_quote.pre = GREEN;
        c->element_value_entity.pre = MAGENTA;
        c->element_value_compound_quote.pre = GREEN;
        c->element_value_compound_entity.pre = MAGENTA;
        c->attr_ns.pre = GRAY;
        c->attr_ns_declaration.pre = GRAY_UNDERLINE;
        c->attr_key.pre = LIGHT_BLUE;
        c->attr_value_text.pre = BLUE;
        c->attr_value_quote.pre = BLUE;
        c->attr_value_entity.pre = MAGENTA;
        c->attr_value_compound_quote.pre = BLUE;
        c->attr_value_compound_entity.pre = MAGENTA;
        c->ns_colon.pre = NOCOLOR;
    }
    else
    {
        c->whitespace.pre  = NOCOLOR;
        c->tab_whitespace.pre  = RED_BACKGROUND;
        c->unicode_whitespace.pre  = RED_UNDERLINE;
        c->equals.pre      = NOCOLOR;
        c->brace_left.pre  = NOCOLOR;
        c->brace_right.pre = NOCOLOR;
        c->apar_left.pre    = NOCOLOR;
        c->apar_right.pre   = NOCOLOR;
        c->cpar_left.pre = MAGENTA;
        c->cpar_right.pre = MAGENTA;
        c->quote.pre = DARK_GREEN;
        c->entity.pre = MAGENTA;
        c->comment.pre = CYAN;
        c->comment_continuation.pre = CYAN;
        c->element_ns.pre = DARK_GRAY;
        c->element_name.pre = DARK_ORANGE;
        c->element_key.pre = BLUE;
        c->element_value_text.pre = DARK_GREEN;
        c->element_value_quote.pre = DARK_GREEN;
        c->element_value_entity.pre = MAGENTA;
        c->element_value_compound_quote.pre = DARK_GREEN;
        c->element_value_compound_entity.pre = MAGENTA;
        c->attr_ns.pre = DARK_GRAY;
        c->attr_ns_declaration.pre = DARK_GRAY_UNDERLINE;
        c->attr_key.pre = BLUE;
        c->attr_value_text.pre = DARK_BLUE;
        c->attr_value_quote.pre = DARK_BLUE;
        c->attr_value_entity.pre = MAGENTA;
        c->attr_value_compound_quote.pre = DARK_BLUE;
        c->attr_value_compound_entity.pre = MAGENTA;
        c->ns_colon.pre = NOCOLOR;
    }
}

void setup_html_coloring(XMQOutputSettings *os, XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    os->indentation_space = " ";
    os->explicit_nl = "\n";
    if (!render_raw)
    {
        c->document.pre =
            "<!DOCTYPE html>\n<html>\n";
        c->document.post =
            "</html>";
        c->header.pre =
            "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\"><style>";
        c->header.post =
            "</style></head>";
        c->style.pre =
            "pre.xmq_dark {border-radius:2px;background-color:#263338;border:solid 1px #555555;display:inline-block;padding:1em;color:white;}\n"
            "pre.xmq_light{border-radius:2px;background-color:#f8f9fb;border:solid 1px #888888;display:inline-block;padding:1em;color:black;}\n"
            "xmqC{color:#2aa1b3;}\n"
            "xmqQ{color:#26a269;}\n"
            "xmqE{color:magenta;}\n"
            "xmqENS_ens{text-decoration:underline; color:darkorange;}\n"
            "xmqEN{color:darkorange;}\n"
            "xmqEK{color:#88b4f7;}\n"
            "xmqEKV{color:#26a269;}\n"
            "xmqAK{color:#88b4f7;}\n"
            "xmqAKV{color:#3166cc;}\n"
            "xmqANS{text-decoration:underline;color:#88b4f7;}\n"
            "xmqCP{color:#c061cb;}\n"
            "pre.xmq_light { xmqQ{color:darkgreen;} xmqEKV{color:darkgreen;} xmqEK{color:#1f61ff;}; xmq_AK{color:#1f61ff;}\n"
            "pre.xmq_dark { }\n"
            ;

        c->body.pre =
            "<body>";
        c->body.post =
            "</body>";
    }

    c->content.pre = "<pre>";
    c->content.post = "</pre>";

    const char *mode = "xmq_light";
    if (dark_mode) mode = "xmq_dark";

    char *buf = malloc(1024);
    os->free_me = buf;
    const char *id = os->use_id;
    const char *idb = "id=\"";
    const char *ide = "\" ";
    if (!id)
    {
        id = "";
        idb = "";
        ide = "";
    }
    const char *clazz = os->use_class;
    const char *space = " ";
    if (!clazz)
    {
        clazz = "";
        space = "";
    }
    snprintf(buf, 1023, "<pre %s%s%sclass=\"xmq %s%s%s\">", idb, id, ide, mode, space, clazz);
    c->content.pre = buf;

    c->whitespace.pre  = NULL;
    c->indentation_whitespace.pre = NULL;
    c->unicode_whitespace.pre  = "<xmqUW>";
    c->unicode_whitespace.post  = "</xmqUW>";
    c->equals.pre      = NULL;
    c->brace_left.pre  = NULL;
    c->brace_right.pre = NULL;
    c->apar_left.pre    = NULL;
    c->apar_right.pre   = NULL;
    c->cpar_left.pre = "<xmqCP>";
    c->cpar_left.post = "</xmqCP>";
    c->cpar_right.pre = "<xmqCP>";
    c->cpar_right.post = "</xmqCP>";
    c->quote.pre = "<xmqQ>";
    c->quote.post = "</xmqQ>";
    c->entity.pre = "<xmqE>";
    c->entity.post = "</xmqE>";
    c->comment.pre = "<xmqC>";
    c->comment.post = "</xmqC>";
    c->comment_continuation.pre = "<xmqC>";
    c->comment_continuation.post = "</xmqC>";
    c->element_ns.pre = "<xmqENS>";
    c->element_ns.post = "</xmqENS>";
    c->element_name.pre = "<xmqEN>";
    c->element_name.post = "</xmqEN>";
    c->element_key.pre = "<xmqEK>";
    c->element_key.post = "</xmqEK>";
    c->element_value_text.pre = "<xmqEKV>";
    c->element_value_text.post = "</xmqEKV>";
    c->element_value_quote.pre = "<xmqEKV>";
    c->element_value_quote.post = "</xmqEKV>";
    c->element_value_entity.pre = "<xmqE>";
    c->element_value_entity.post = "</xmqE>";
    c->element_value_compound_quote.pre = "<xmqEKV>";
    c->element_value_compound_quote.post = "</xmqEKV>";
    c->element_value_compound_entity.pre = "<xmqE>";
    c->element_value_compound_entity.post = "</xmqE>";
    c->attr_ns.pre = "<xmqANS>";
    c->attr_ns.post = "</xmqANS>";
    c->attr_key.pre = "<xmqAK>";
    c->attr_key.post = "</xmqAK>";
    c->attr_value_text.pre = "<xmqAKV>";
    c->attr_value_text.post = "</xmqAKV>";
    c->attr_value_quote.pre = "<xmqAKV>";
    c->attr_value_quote.post = "</xmqAKV>";
    c->attr_value_entity.pre = "<xmqE>";
    c->attr_value_entity.post = "</xmqE>";
    c->attr_value_compound_quote.pre = "<xmqAKV>";
    c->attr_value_compound_quote.post = "</xmqAKV>";
    c->attr_value_compound_entity.pre = "<xmqE>";
    c->attr_value_compound_entity.post = "</xmqE>";
    c->ns_colon.pre = NULL;
}

void setup_htmq_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
}

void setup_tex_coloring(XMQOutputSettings *os, XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    os->indentation_space = "\\xmqI ";
    os->explicit_space = " ";
    os->explicit_nl = "\\linebreak\n";

    if (!render_raw)
    {
        c->document.pre =
            "\\documentclass[10pt,a4paper]{article}\n"
            "\\usepackage{color}\n";

        c->style.pre =
            "\\definecolor{Brown}{rgb}{0.86,0.38,0.0}\n"
            "\\definecolor{Blue}{rgb}{0.0,0.37,1.0}\n"
            "\\definecolor{DarkSlateBlue}{rgb}{0.28,0.24,0.55}\n"
            "\\definecolor{Green}{rgb}{0.0,0.46,0.0}\n"
            "\\definecolor{Red}{rgb}{0.77,0.13,0.09}\n"
            "\\definecolor{LightBlue}{rgb}{0.40,0.68,0.89}\n"
            "\\definecolor{MediumBlue}{rgb}{0.21,0.51,0.84}\n"
            "\\definecolor{LightGreen}{rgb}{0.54,0.77,0.43}\n"
            "\\definecolor{Grey}{rgb}{0.5,0.5,0.5}\n"
            "\\definecolor{Purple}{rgb}{0.69,0.02,0.97}\n"
            "\\definecolor{Yellow}{rgb}{0.5,0.5,0.1}\n"
            "\\definecolor{Cyan}{rgb}{0.3,0.7,0.7}\n"
            "\\newcommand{\\xmqC}[1]{{\\color{Cyan}#1}}\n"
            "\\newcommand{\\xmqQ}[1]{{\\color{Green}#1}}\n"
            "\\newcommand{\\xmqE}[1]{{\\color{Purple}#1}}\n"
            "\\newcommand{\\xmqENS}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqEN}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqEK}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqEKV}[1]{{\\color{Green}#1}}\n"
            "\\newcommand{\\xmqANS}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqAK}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqAKV}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmqCP}[1]{{\\color{Purple}#1}}\n"
            "\\newcommand{\\xmqI}[0]{{\\mbox{\\ }}}\n";

        c->body.pre = "\n\\begin{document}\n";
        c->body.post = "\n\\end{document}\n";
    }

    c->content.pre = "\\texttt{\\flushleft\\noindent ";
    c->content.post = "\n}\n";
    c->whitespace.pre  = NULL;
    c->indentation_whitespace.pre = NULL;
    c->unicode_whitespace.pre  = "\\xmqUW{";
    c->unicode_whitespace.post  = "}";
    c->equals.pre      = NULL;
    c->brace_left.pre  = NULL;
    c->brace_right.pre = NULL;
    c->apar_left.pre    = NULL;
    c->apar_right.pre   = NULL;
    c->cpar_left.pre = "\\xmqCP{";
    c->cpar_left.post = "}";
    c->cpar_right.pre = "\\xmqCP{";
    c->cpar_right.post = "}";
    c->quote.pre = "\\xmqQ{";
    c->quote.post = "}";
    c->entity.pre = "\\xmqE{";
    c->entity.post = "}";
    c->comment.pre = "\\xmqC{";
    c->comment.post = "}";
    c->comment_continuation.pre = "\\xmqC{";
    c->comment_continuation.post = "}";
    c->element_ns.pre = "\\xmqENS{";
    c->element_ns.post = "}";
    c->element_name.pre = "\\xmqEN{";
    c->element_name.post = "}";
    c->element_key.pre = "\\xmqEK{";
    c->element_key.post = "}";
    c->element_value_text.pre = "\\xmqEKV{";
    c->element_value_text.post = "}";
    c->element_value_quote.pre = "\\xmqEKV{";
    c->element_value_quote.post = "}";
    c->element_value_entity.pre = "\\xmqE{";
    c->element_value_entity.post = "}";
    c->element_value_compound_quote.pre = "\\xmqEKV{";
    c->element_value_compound_quote.post = "}";
    c->element_value_compound_entity.pre = "\\xmqE{";
    c->element_value_compound_entity.post = "}";
    c->attr_ns.pre = "\\xmqANS{";
    c->attr_ns.post = "}";
    c->attr_key.pre = "\\xmqAK{";
    c->attr_key.post = "}";
    c->attr_value_text.pre = "\\xmqAKV{";
    c->attr_value_text.post = "}";
    c->attr_value_quote.pre = "\\xmqAKV{";
    c->attr_value_quote.post = "}";
    c->attr_value_entity.pre = "\\xmqE{";
    c->attr_value_entity.post = "}";
    c->attr_value_compound_quote.pre = "\\xmqAKV{";
    c->attr_value_compound_quote.post = "}";
    c->attr_value_compound_entity.pre = "\\xmqE{";
    c->attr_value_compound_entity.post = "}";
    c->ns_colon.pre = NULL;
}

void xmqOverrideSettings(XMQOutputSettings *settings,
                         const char *indentation_space,
                         const char *explicit_space,
                         const char *explicit_tab,
                         const char *explicit_cr,
                         const char *explicit_nl)
{
    if (indentation_space) settings->indentation_space = indentation_space;
    if (explicit_space) settings->explicit_space = explicit_space;
    if (explicit_tab) settings->explicit_tab = explicit_tab;
    if (explicit_cr) settings->explicit_cr = explicit_cr;
    if (explicit_nl) settings->explicit_nl = explicit_nl;
}

void xmqRenderHtmlSettings(XMQOutputSettings *settings,
                           const char *use_id,
                           const char *use_class)
{
    if (use_id) settings->use_id = use_id;
    if (use_class) settings->use_class = use_class;
}

void xmqOverrideColorType(XMQOutputSettings *settings, XMQColorType ct, const char *pre, const char *post, const char *namespace)
{
    switch (ct)
    {
    case COLORTYPE_xmq_c:
    case COLORTYPE_xmq_q:
    case COLORTYPE_xmq_e:
    case COLORTYPE_xmq_ens:
    case COLORTYPE_xmq_en:
    case COLORTYPE_xmq_ek:
    case COLORTYPE_xmq_ekv:
    case COLORTYPE_xmq_ans:
    case COLORTYPE_xmq_ak:
    case COLORTYPE_xmq_akv:
    case COLORTYPE_xmq_cp:
    case COLORTYPE_xmq_uw:
        return;
    }
}


void xmqOverrideColor(XMQOutputSettings *os, XMQColor c, const char *pre, const char *post, const char *namespace)
{
    if (!os->colorings)
    {
        fprintf(stderr, "Internal error: you have to invoke xmqSetupDefaultColors first before overriding.\n");
        exit(1);
    }
    if (!namespace) namespace = "";
    XMQColoring *cols = hashmap_get(os->colorings, namespace);
    assert(cols);

    switch (c)
    {
    case COLOR_none: break;
    case COLOR_whitespace: cols->whitespace.pre = pre; cols->whitespace.post = post;
    case COLOR_unicode_whitespace:
    case COLOR_indentation_whitespace:
    case COLOR_equals:
    case COLOR_brace_left:
    case COLOR_brace_right:
    case COLOR_apar_left:
    case COLOR_apar_right:
    case COLOR_cpar_left:
    case COLOR_cpar_right:
    case COLOR_quote:
    case COLOR_entity:
    case COLOR_comment:
    case COLOR_comment_continuation:
    case COLOR_ns_colon:
    case COLOR_element_ns:
    case COLOR_element_name:
    case COLOR_element_key:
    case COLOR_element_value_text:
    case COLOR_element_value_quote:
    case COLOR_element_value_entity:
    case COLOR_element_value_compound_quote:
    case COLOR_element_value_compound_entity:
    case COLOR_attr_ns:
    case COLOR_attr_ns_declaration:
    case COLOR_attr_key:
    case COLOR_attr_value_text:
    case COLOR_attr_value_quote:
    case COLOR_attr_value_entity:
    case COLOR_attr_value_compound_quote:
    case COLOR_attr_value_compound_entity:
        return;
    }
}

int xmqStateErrno(XMQParseState *state)
{
    return (int)state->error_nr;
}

#define X(TYPE) \
    void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, size_t indent,const char *cstart, const char *cstop, const char *stop) { \
        if (!state->simulated) { \
            const char *pre, *post;  \
            get_color(state->output_settings, COLOR_##TYPE, &pre, &post); \
            if (pre) state->output_settings->content.write(state->output_settings->content.writer_state, pre, NULL); \
            if (state->output_settings->render_to == XMQ_RENDER_TERMINAL) { \
                state->output_settings->content.write(state->output_settings->content.writer_state, start, stop); \
            } else if (state->output_settings->render_to == XMQ_RENDER_HTML) { \
                write_safe_html(state->output_settings->content.write, state->output_settings->content.writer_state, start, stop); \
            } else if (state->output_settings->render_to == XMQ_RENDER_TEX) { \
                write_safe_tex(state->output_settings->content.write, state->output_settings->content.writer_state, start, stop); \
            } \
            if (post) state->output_settings->content.write(state->output_settings->content.writer_state, post, NULL); \
        } \
    }
LIST_OF_XMQ_TOKENS
#undef X


const char *xmqStateErrorMsg(XMQParseState *state)
{
    return state->generated_error_msg;
}

void reset_ansi(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, ansi_reset_color, NULL);
}

void reset_ansi_nl(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, ansi_reset_color, NULL);
    state->output_settings->content.write(state->output_settings->content.writer_state, "\n", NULL);
}

void add_nl(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, "\n", NULL);
}

XMQOutputSettings *xmqNewOutputSettings()
{
    XMQOutputSettings *os = (XMQOutputSettings*)malloc(sizeof(XMQOutputSettings));
    memset(os, 0, sizeof(XMQOutputSettings));
    os->colorings = hashmap_create(11);
    XMQColoring *c = malloc(sizeof(XMQColoring));
    memset(c, 0, sizeof(XMQColoring));
    hashmap_put(os->colorings, "", c);
    os->default_coloring = c;

    os->indentation_space = " ";
    os->explicit_space = " ";
    os->explicit_nl = "\n";
    os->explicit_tab = "\t";
    os->explicit_cr = "\r";
    os->add_indent = 4;
    os->use_color = false;

    return os;
}

void xmqFreeOutputSettings(XMQOutputSettings *os)
{
    if (os->free_me)
    {
        free(os->free_me);
        os->free_me = NULL;
    }
    hashmap_free_and_values(os->colorings);
    os->colorings = NULL;
    free(os);
}

void xmqSetAddIndent(XMQOutputSettings *os, int add_indent)
{
    os->add_indent = add_indent;
}

void xmqSetCompact(XMQOutputSettings *os, bool compact)
{
    os->compact = compact;
}

void xmqSetUseColor(XMQOutputSettings *os, bool use_color)
{
    os->use_color = use_color;
}

void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines)
{
    os->escape_newlines = escape_newlines;
}

void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit)
{
    os->escape_non_7bit = escape_non_7bit;
}

void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format)
{
    os->output_format = output_format;
}

/*void xmqSetColoring(XMQOutputSettings *os, XMQColoring coloring)
{
    os->coloring = coloring;
    }*/

void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to)
{
    os->render_to = render_to;
}

void xmqSetRenderRaw(XMQOutputSettings *os, bool render_raw)
{
    os->render_raw = render_raw;
}

void xmqSetRenderOnlyStyle(XMQOutputSettings *os, bool only_style)
{
    os->only_style = only_style;
}

void xmqSetWriterContent(XMQOutputSettings *os, XMQWriter content)
{
    os->content = content;
}

void xmqSetWriterError(XMQOutputSettings *os, XMQWriter error)
{
    os->error = error;
}

bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop)
{
    if (!start) return true;
    if (!stop)
    {
        fputs(start, stdout);
    }
    else
    {
        assert(stop > start);
        fwrite(start, stop-start, 1, stdout);
    }
    return true;
}

bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop)
{
    if (!start) return true;
    if (!stop)
    {
        fputs(start, stderr);
    }
    else
    {
        fwrite(start, stop-start, 1, stderr);
    }
    return true;
}

void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        const char *amp = "&amp;";
        const char *lt = "&lt;";
        const char *gt = "&gt;";
        const char *quot = "&quot;";
        if (*i == '&') write(writer_state, amp, amp+5);
        else if (*i == '<') write(writer_state, lt, lt+4);
        else if (*i == '>') write(writer_state, gt, gt+4);
        else if (*i == '"') write(writer_state, quot, quot+6); //"
        else write(writer_state, i, i+1);
    }
}

void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        const char *amp = "\\&";
        const char *bs = "\\\\";
        const char *us = "\\_";
        if (*i == '&') write(writer_state, amp, amp+2);
        else if (*i == '\\') write(writer_state, bs, bs+2);
        else if (*i == '_') write(writer_state, us, us+2);
        else write(writer_state, i, i+1);
    }
}

void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps)
{
    ps->content.writer_state = NULL; // Not needed
    ps->content.write = write_print_stdout;
    ps->error.writer_state = NULL; // Not needed
    ps->error.write = write_print_stderr;
}

void xmqSetupPrintMemory(XMQOutputSettings *os, char **start, char **stop)
{
    os->output_buffer_start = start;
    os->output_buffer_stop = stop;
    os->output_buffer = new_membuffer();
    os->content.writer_state = os->output_buffer;
    os->content.write = (void*)membuffer_append_region;
    os->error.writer_state = os->output_buffer;
    os->error.write = (void*)membuffer_append_region;
}

XMQParseCallbacks *xmqNewParseCallbacks()
{
    XMQParseCallbacks *callbacks = (XMQParseCallbacks*)malloc(sizeof(XMQParseCallbacks));
    memset(callbacks, 0, sizeof(sizeof(XMQParseCallbacks)));
    return callbacks;
}

XMQParseState *xmqNewParseState(XMQParseCallbacks *callbacks, XMQOutputSettings *output_settings)
{
    if (!callbacks)
    {
        PRINT_ERROR("xmqNewParseState is given a NULL callback structure!\n");
        assert(0);
        exit(1);
    }
    if (!output_settings)
    {
        PRINT_ERROR("xmqNewParseState is given a NULL print output_settings structure!\n");
        assert(0);
        exit(1);
    }
    if (callbacks->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("xmqNewParseState is given a callback structure which is not initialized!\n");
        assert(0);
        exit(1);
    }
    XMQParseState *state = (XMQParseState*)malloc(sizeof(XMQParseState));
    memset(state, 0, sizeof(XMQParseState));
    state->parse = callbacks;
    state->output_settings = output_settings;
    state->magic_cookie = MAGIC_COOKIE;
    state->element_stack = new_stack();

    return state;
}

bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    XMQContentType detected_ct = xmqDetectContentType(start, stop);
    if (detected_ct != XMQ_CONTENT_XMQ)
    {
        state->generated_error_msg = strdup("xmq: you can only tokenize the xmq format");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        return false;
    }

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = 0;

    if (state->parse->init) state->parse->init(state);

    XMQOutputSettings *output_settings = state->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    const char *pre = output_settings->default_coloring->content.pre;
    const char *post = output_settings->default_coloring->content.post;
    if (pre) write(writer_state, pre, NULL);

    if (!setjmp(state->error_handler))
    {
        // Start parsing!
        parse_xmq(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_UNEXPECTED_CLOSING_BRACE;
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        build_state_error_message(state, start, stop);
        return false;
    }

    if (post) write(writer_state, post, NULL);

    if (state->parse->done) state->parse->done(state);

    if (output_settings->output_buffer &&
        output_settings->output_buffer_start &&
        output_settings->output_buffer_stop)
    {
        size_t size = membuffer_used(output_settings->output_buffer);
        char *buffer = free_membuffer_but_return_trimmed_content(output_settings->output_buffer);
        *output_settings->output_buffer_start = buffer;
        *output_settings->output_buffer_stop = buffer+size;
    }

    return true;
}

bool xmqTokenizeFile(XMQParseState *state, const char *file)
{
    bool rc = false;
    char *buffer = NULL;
    long fsize = 0;
    size_t n = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    FILE *f = fopen(file, "rb");
    if (!f) {
        state->error_nr = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer) return false;

    n = fread(buffer, fsize, 1, f);

    if (n != 1) {
        rc = false;
        state->error_nr = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;

    xmqSetStateSourceName(state, file);

    content = xmqDetectContentType(buffer, buffer+fsize);
    if (content != XMQ_CONTENT_XMQ)
    {
        state->generated_error_msg = strdup("You can only tokenize xmq!");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        rc = false;
        goto exit;
    }

    rc = xmqTokenizeBuffer(state, buffer, buffer+fsize);

    exit:

    free(buffer);

    return rc;
}

/** This function is used only for detecting the kind of content: xmq, xml, html, json. */
const char *find_word_ignore_case(const char *start, const char *stop, const char *word)
{
    const char *i = start;
    size_t len = strlen(word);

    while (i < stop && is_xml_whitespace(*i)) i++;
    if (!strncasecmp(i, word, len))
    {
        const char *next = i+len;
        if (next <= stop && (is_xml_whitespace(*next) || *next == 0 || !isalnum(*next)))
        {
            // The word was properly terminated with a 0, or a whitespace or something not alpha numeric.
            return i+strlen(word);
        }
    }
    return NULL;
}

XMQContentType xmqDetectContentType(const char *start, const char *stop)
{
    const char *i = start;

    while (i < stop)
    {
        char c = *i;
        if (!is_xml_whitespace(c))
        {
            if (c == '<')
            {
                if (i+4 < stop &&
                    *(i+1) == '?' &&
                    *(i+2) == 'x' &&
                    *(i+3) == 'm' &&
                    *(i+4) == 'l')
                {
                    debug("[XMQ] content detected as xml since <?xml found\n");
                    return XMQ_CONTENT_XML;
                }

                if (i+3 < stop &&
                    *(i+1) == '!' &&
                    *(i+2) == '-' &&
                    *(i+3) == '-')
                {
                    // This is a comment, zip past it.
                    while (i+2 < stop &&
                           !(*(i+0) == '-' &&
                             *(i+1) == '-' &&
                             *(i+2) == '>'))
                    {
                        i++;
                    }
                    i += 3;
                    // No closing comment, return as xml.
                    if (i >= stop)
                    {
                        debug("[XMQ] content detected as xml since comment start found\n");
                        return XMQ_CONTENT_XML;
                    }
                    // Pick up after the comment.
                    c = *i;
                }

                // Starts with <html or < html
                const char *is_html = find_word_ignore_case(i+1, stop, "html");
                if (is_html)
                {
                    debug("[XMQ] content detected as html since html found\n");
                    return XMQ_CONTENT_HTML;
                }

                // Starts with <!doctype  html
                const char *is_doctype = find_word_ignore_case(i, stop, "<!doctype");
                if (is_doctype)
                {
                    i = is_doctype;
                    is_html = find_word_ignore_case(is_doctype+1, stop, "html");
                    if (is_html)
                    {
                        debug("[XMQ] content detected as html since doctype html found\n");
                        return XMQ_CONTENT_HTML;
                    }
                }
                // Otherwise we assume it is xml. If you are working with html content inside
                // the html, then use --html
                debug("[XMQ] content assumed to be xml\n");
                return XMQ_CONTENT_XML; // Or HTML...
            }
            if (c == '{' || c == '"' || c == '[' || (c >= '0' && c <= '9')) // "
            {
                debug("[XMQ] content detected as json\n");
                return XMQ_CONTENT_JSON;
            }
            // Strictly speaking true,false and null are valid xmq files. But we assume
            // it is json since it must be very rare with a single <true> <false> <null> tag in xml/xmq/html/htmq etc.
            // Force xmq with --xmq for the cli command.
            size_t l = 0;
            if (c == 't' || c == 'n') l = 4;
            else if (c == 'f') l = 5;

            if (l != 0)
            {
                if (i+l-1 < stop)
                {
                    if (i+l == stop || (*(i+l) == '\n' && i+l+1 == stop))
                    {
                        if (!strncmp(i, "true", 4) ||
                            !strncmp(i, "false", 5) ||
                            !strncmp(i, "null", 4))
                        {
                            debug("[XMQ] content detected as json since true/false/null found\n");
                            return XMQ_CONTENT_JSON;
                        }
                    }
                }
            }
            debug("[XMQ] content assumed to be xmq\n");
            return XMQ_CONTENT_XMQ;
        }
        i++;
    }

    debug("[XMQ] empty content assumed to be xmq\n");
    return XMQ_CONTENT_XMQ;
}


/** Scan a line, ie until \n or NULL.
    Return true if a newline was found. */
bool find_line(const char *start, // Start scanning the line from here.
               const char *stop,  // Points to char after end of buffer.
               size_t *indent,  // Store indentation in number of spaces.
               const char **after_last_non_space, // Points to char after last non-space char on line, start if whole line is spaces.
               const char **eol)    // Points to char after '\n' or to stop.
{
    assert(start <= stop);

    bool has_nl = false;
    size_t ndnt = 0;
    const char *lnws = start;
    const char *i = start;

    // Skip spaces as indententation.
    while (i < stop && (*i == ' ' || *i == '\t'))
    {
        if (*i == ' ') ndnt++;
        else ndnt += 8; // Count tab as 8 chars.
        i++;
    }
    *indent = ndnt;

    // Find eol \n and the last non-space char.
    while (i < stop)
    {
        if (*i == '\n')
        {
            i++;
            has_nl = true;
            break;
        }
        if (*i != ' ' && *i != '\t') lnws = i+1;
        i++;
    }

    *after_last_non_space = lnws;
    *eol = i;

    return has_nl;
}

void xmqSetDebug(bool e)
{
    debug_enabled_ = e;
}

bool xmqDebugging()
{
    return debug_enabled_;
}

void xmqSetVerbose(bool e)
{
    verbose_enabled_ = e;
}

bool xmqVerbose() {
    return verbose_enabled_;
}

static const char *build_error_message(const char* fmt, ...)
{
    char *buf = (char*)malloc(1024);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);
    buf[1023] = 0;
    buf = (char*)realloc(buf, strlen(buf)+1);
    return buf;
}

/**
    xmq_un_quote:
    @indent: The number of chars before the quote starts on the first line.
    @space: Use this space to prepend the first line if needed due to indent.
    @start: Start of buffer to unquote.
    @stop: Points to byte after buffer.

    Do the reverse of xmq_quote, take a quote (with or without the surrounding single quotes)
    and removes any incidental indentation. Returns a newly malloced buffer
    that must be free:ed later.

    Use indent 0 if the quote ' is first on the line.
    The indent is 1 if there is a single space before the starting quote ' etc.

    As a special case, if both indent is 0 and space is 0, then the indent of the
    first line is picked from the second line.
*/
char *xmq_un_quote(size_t indent, char space, const char *start, const char *stop, bool remove_qs)
{
    if (!stop) stop = start+strlen(start);

    // Remove the surrounding quotes.
    size_t j = 0;
    if (remove_qs)
    {
        while (*(start+j) == '\'' && *(stop-j-1) == '\'' && (start+j) < (stop-j)) j++;
    }

    indent += j;
    start = start+j;
    stop = stop-j;

    return xmq_trim_quote(indent, space, start, stop);
}

/**
    xmq_un_comment:

    Do the reverse of xmq_comment, Takes a comment (including /✻ ✻/ ///✻ ✻///) and removes any incidental
    indentation and trailing spaces. Returns a newly malloced buffer
    that must be free:ed later.

    The indent is 0 if the / first on the line.
    The indent is 1 if there is a single space before the starting / etc.
*/
char *xmq_un_comment(size_t indent, char space, const char *start, const char *stop)
{
    assert(start < stop);
    assert(*start == '/');

    const char *i = start;
    while (i < stop && *i == '/') i++;

    if (i == stop)
    {
        // Single line all slashes. Drop the two first slashes which are the single line comment.
        return xmq_trim_quote(indent, space, start+2, stop);
    }

    if (*i != '*')
    {
        // No asterisk * after the slashes. This is a single line comment.
        // If there is a space after //, skip it.
        if (*i == ' ') i++;
        // Remove trailing spaces.
        while (i < stop && *(stop-1) == ' ') stop--;
        assert(i <= stop);
        return xmq_trim_quote(indent, space, i, stop);
    }

    // There is an asterisk after the slashes. A standard /* */ comment
    // Remove the surrounding / slashes.
    size_t j = 0;
    while (*(start+j) == '/' && *(stop-j-1) == '/' && (start+j) < (stop-j)) j++;

    indent += j;
    start = start+j;
    stop = stop-j;

    // Check that the star is there.
    assert(*start == '*' && *(stop-1) == '*');
    indent++;
    start++;
    stop--;

    // Skip a single space immediately after the asterisk or before the ending asterisk.
    // I.e. /* Comment */ results in the comment text "Comment"
    if (*start == ' ')
    {
        start++;
    }
    if (*(stop-1) == ' ')
    {
        if (stop-1 >= start)
        {
            stop--;
        }
    }

    assert(start <= stop);
    return xmq_trim_quote(indent, space, start, stop);
}

char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop)
{
    // Special case, find the next indent and use as original indent.
    if (indent == 0 && space == 0)
    {
        size_t i;
        const char *after;
        const char *eol;
        // Skip first line.
        bool found_nl = find_line(start, stop, &i, &after, &eol);
        if (found_nl && eol != stop)
        {
            // Now grab the indent from the second line.
            find_line(eol, stop, &indent, &after, &eol);
        }
    }
    // If the first line is all spaces and a newline, then
    // the first_indent should be ignored.
    bool ignore_first_indent = false;

    // For each found line, the found indent is the number of spaces before the first non space.
    size_t found_indent;

    // For each found line, this is where the line ends after trimming line ending whitespace.
    const char *after_last_non_space;

    // This is where the newline char was found.
    const char *eol;

    // Did the found line end with a newline or NULL.
    bool has_nl = false;

    // Lets examine the first line!
    has_nl = find_line(start, stop, &found_indent, &after_last_non_space, &eol);

    // We override the found indent (counting from start)
    // with the actual source indent from the beginning of the line.
    found_indent = indent;

    if (!has_nl)
    {
        // No newline was found, then do not trim! Return as is.
        size_t n = 1+stop-start;
        char *buf = (char*)malloc(n);
        memcpy(buf, start, n-1);
        buf[n-1] = 0;
        return buf;
    }

    // Check if the final line is all spaces.
    if (has_ending_nl_space(start, stop))
    {
        // So it is, now trim from the end.
        while (stop > start)
        {
            char c = *(stop-1);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            stop--;
        }
    }

    if (stop == start)
    {
        // Oups! Quote was all space and newlines. I.e. it is an empty quote.
        char *buf = (char*)malloc(1);
        buf[0] = 0;
        return buf;
    }

    // Check if the first line is all spaces.
    if (has_leading_space_nl(start, stop))
    {
        // The first line is all spaces, trim leading spaces and newlines!
        ignore_first_indent = true;
        // Skip the already scanned first line.
        start = eol;
        const char *i = start;
        while (i < stop)
        {
            char c = *i;
            if (c == '\n') start = i+1; // Restart find lines from here.
            else if (c != ' ' && c != '\t' && c != '\r') break;
            i++;
        }
    }
    size_t incidental = (size_t)-1;

    if (!ignore_first_indent)
    {
        incidental = indent;
    }

    // Now scan remaining lines at the first line.
    const char *i = start;
    bool first_line = true;
    while (i < stop)
    {
        has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (after_last_non_space != i)
        {
            // There are non-space chars.
            if (found_indent < incidental)  // Their indent is lesser than the so far found.
            {
                // Yep, remember it.
                if (!first_line || ignore_first_indent)
                {
                    incidental = found_indent;
                }
            }
            first_line = false;
        }
        i = eol; // Start at next line, or end at stop.
    }

    size_t prepend = 0;

    if (!ignore_first_indent &&
        indent >= incidental)
    {
        // The first indent is relevant and it is bigger than the incidental.
        // We need to prepend the output line with spaces that are not in the source!
        // But, only if there is more than one line with actual non spaces!
        prepend = indent - incidental;
    }

    // Allocate max size of output buffer, it usually becomes smaller
    // when incidental indentation and trailing whitespace is removed.
    size_t n = stop-start+prepend+1;
    char *buf = (char*)malloc(n);
    char *o = buf;

    // Insert any necessary prepended spaces due to source indentation of the line.
    while (prepend) { *o++ = space; prepend--; }

    // Start scanning the lines from the beginning again.
    // Now use the found incidental to copy the right parts.
    i = start;

    first_line = true;
    while (i < stop)
    {
        bool has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (!first_line || ignore_first_indent)
        {
            // For all lines except the first. And for the first line if ignore_first_indent is true.
            // Skip the incidental indentation, space counts as one tab counts as 8.
            size_t n = incidental;
            while (n > 0)
            {
                char c = *i;
                i++;
                if (c == ' ') n--;
                else if (c == '\t')
                {
                    if (n >= 8) n -= 8;
                    else break; // safety check.
                }
            }
        }
        // Copy content, but not line ending xml whitespace ie space, tab, cr.
        while (i < after_last_non_space) { *o++ = *i++; }

        if (has_nl)
        {
            *o++ = '\n';
        }
        else
        {
            // The final line has no nl, here we copy any ending spaces as well!
            while (i < eol) { *o++ = *i++; }
        }
        i = eol;
        first_line = false;
    }
    *o++ = 0;
    size_t real_size = o-buf;
    buf = (char*)realloc(buf, real_size);
    return buf;
}




void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = NULL;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

#define WRITE_ARGS(...) state->output_settings->content.write(state->output_settings->content.writer_state, __VA_ARGS__)

#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,size_t indent,const char*cstart,const char*cstop,const char*stop) { \
    WRITE_ARGS("["#TYPE, NULL); \
    if (state->simulated) { WRITE_ARGS(" SIM", NULL); } \
    WRITE_ARGS(" \"", NULL); \
    char *tmp = xmq_quote_as_c(start, stop); \
    WRITE_ARGS(tmp, NULL); \
    free(tmp); \
    WRITE_ARGS("\"", NULL); \
    char buf[32]; \
    snprintf(buf, 32, " %zu:%zu]", line, col); \
    buf[31] = 0; \
    WRITE_ARGS(buf, NULL); \
};
LIST_OF_XMQ_TOKENS
#undef X

void xmqSetupParseCallbacksDebugTokens(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
#define X(TYPE) callbacks->handle_##TYPE = debug_token_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->done = add_nl;

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void debug_content_value(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         size_t indent,
                         const char *cstart,
                         const char *cstop,
                         const char *stop)
{
    char *tmp = xmq_quote_as_c(cstart, cstop);
    WRITE_ARGS("{value \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
}


void debug_content_quote(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         size_t inden,
                         const char *cstart,
                         const char *cstop,
                         const char*stop)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_quote(indent, ' ', start, stop, true);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    WRITE_ARGS("{quote \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
}

void debug_content_comment(XMQParseState *state,
                           size_t line,
                           size_t start_col,
                           const char *start,
                           size_t inden,
                           const char *cstart,
                           const char *cstop,
                           const char*stop)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_comment(indent, ' ', start, stop);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    WRITE_ARGS("{comment \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
}

void xmqSetupParseCallbacksDebugContent(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->handle_element_value_text = debug_content_value;
    callbacks->handle_attr_value_text = debug_content_value;
    callbacks->handle_quote = debug_content_quote;
    callbacks->handle_comment = debug_content_comment;
    callbacks->handle_element_value_quote = debug_content_quote;
    callbacks->handle_element_value_compound_quote = debug_content_quote;
    callbacks->handle_attr_value_quote = debug_content_quote;
    callbacks->handle_attr_value_compound_quote = debug_content_quote;
    callbacks->done = add_nl;

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *callbacks, XMQRenderFormat render_format, bool dark_mode)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = tokenize_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

XMQDoc *xmqNewDoc()
{
    XMQDoc *d = (XMQDoc*)malloc(sizeof(XMQDoc));
    memset(d, 0, sizeof(XMQDoc));
    d->docptr_.xml = xmlNewDoc((const xmlChar*)"1.0");
    return d;
}

void *xmqGetImplementationDoc(XMQDoc *doq)
{
    return doq->docptr_.xml;
}

void xmqSetImplementationDoc(XMQDoc *doq, void *doc)
{
    doq->docptr_.xml = doc;
}

void xmqSetDocSourceName(XMQDoc *doq, const char *source_name)
{
    if (source_name)
    {
        char *buf = (char*)malloc(strlen(source_name)+1);
        strcpy(buf, source_name);
        doq->source_name_ = buf;
    }
}

XMQNode *xmqGetRootNode(XMQDoc *doq)
{
    return &doq->root_;
}

void xmqFreeParseCallbacks(XMQParseCallbacks *cb)
{
    free(cb);
}

void xmqFreeParseState(XMQParseState *state)
{
    if (!state) return;
    free(state->source_name);
    state->source_name = NULL;
    free(state->generated_error_msg);
    state->generated_error_msg = NULL;
    free_stack(state->element_stack);
//    free(state->settings);
    state->output_settings = NULL;
    free(state);
}

void xmqFreeDoc(XMQDoc *doq)
{
    if (!doq) return;
    if (doq->source_name_)
    {
        debug("[XMQ] freeing source name\n");
        free((void*)doq->source_name_);
        doq->source_name_ = NULL;
    }
    if (doq->error_)
    {
        debug("[XMQ] freeing error message\n");
        free((void*)doq->error_);
        doq->error_ = NULL;
    }
    if (doq->docptr_.xml)
    {
        debug("[XMQ] freeing xml doc\n");
        xmlFreeDoc(doq->docptr_.xml);
        doq->docptr_.xml = NULL;
    }
    debug("[XMQ] freeing xmq doc\n");
    free(doq);
}

const char *skip_any_potential_bom(const char *start, const char *stop)
{
    if (start + 3 < stop)
    {
        int a = (unsigned char)*(start+0);
        int b = (unsigned char)*(start+1);
        int c = (unsigned char)*(start+2);
        if (a == 0xef && b == 0xbb && c == 0xbf)
        {
            // The UTF8 bom, this is fine, just skip it.
            return start+3;
        }
    }

    if (start+2 < stop)
    {
        unsigned char a = *(start+0);
        unsigned char b = *(start+1);
        if ((a == 0xff && b == 0xfe) ||
            (a == 0xfe && b == 0xff))
        {
            // We cannot handle UTF-16 files.
            return NULL;
        }
    }

    // Assume content, no bom.
    return start;
}

bool xmqParseBuffer(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root)
{
    bool rc = true;
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    push_stack(state->element_stack, doq->docptr_.xml);
    // Now state->element_stack->top->data points to doq->docptr_;
    state->element_last = NULL; // Will be set when the first node is created.
    // The doc root will be set when the first element node is created.


    // Time to tokenize the buffer and invoke the parse callbacks.
    xmqTokenizeBuffer(state, start, stop);

    if (xmqStateErrno(state))
    {
        rc = false;
        doq->errno_ = xmqStateErrno(state);
        doq->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(output_settings);

    return rc;
}

bool xmqParseFile(XMQDoc *doq, const char *file, const char *implicit_root)
{
    bool ok = true;
    char *buffer = NULL;
    size_t fsize = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    xmqSetDocSourceName(doq, file);

    FILE *f = fopen(file, "rb");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: No such file or directory\n", file);
        ok = false;
        goto exit;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer)
    {
        doq->errno_ = XMQ_ERROR_OOM;
        doq->error_ = build_error_message("xmq: %s: File too big, out of memory\n", file);
        ok = false;
        goto exit;
    }

    size_t block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    size_t n = 0;
    do {
        // We need to read smaller blocks because of a bug in Windows C-library..... blech.
        size_t r = fread(buffer+n, 1, block_size, f);
        debug("[XMQ] read %zu bytes total %zu\n", r, n);
        if (!r) break;
        n += r;
    } while (n < fsize);

    debug("[XMQ] read total %zu bytes\n", n);

    if (n != fsize)
    {
        ok = false;
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;

    content = xmqDetectContentType(buffer, buffer+fsize);
    if (content != XMQ_CONTENT_XMQ)
    {
        ok = false;
        doq->errno_ = XMQ_ERROR_NOT_XMQ;
        goto exit;
    }

    ok = xmqParseBuffer(doq, buffer, buffer+fsize, implicit_root);

    exit:

    free(buffer);

    return ok;
}

const char *xmqVersion()
{
    return VERSION;
}

void do_whitespace(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
}

xmlNodePtr create_quote(XMQParseState *state,
                       size_t l,
                       size_t col,
                       const char *start,
                       size_t ccol,
                       const char *cstart,
                       const char *cstop,
                       const char *stop,
                       xmlNodePtr parent)
{
    size_t indent = col - 1;
    char *trimmed = xmq_un_quote(indent, ' ', start, stop, true);
    xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    xmlAddChild(parent, n);
    free(trimmed);
    return n;
}

void do_quote(XMQParseState *state,
              size_t l,
              size_t col,
              const char *start,
              size_t ccol,
              const char *cstart,
              const char *cstop,
              const char *stop)
{
    state->element_last = create_quote(state, l, col, start, ccol, cstart, cstop, stop,
                                       (xmlNode*)state->element_stack->top->data);
}

xmlNodePtr create_entity(XMQParseState *state,
                         size_t l,
                         size_t c,
                         const char *start,
                         size_t indent,
                         const char *cstart,
                         const char *cstop,
                         const char*stop,
                         xmlNodePtr parent)
{
    size_t len = stop-start;
    char *tmp = (char*)malloc(len+1);
    memcpy(tmp, start, len);
    tmp[len] = 0;
    xmlNodePtr n = NULL;
    if (tmp[1] == '#')
    {
        n = xmlNewCharRef(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    else
    {
        n = xmlNewReference(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    xmlAddChild(parent, n);
    free(tmp);

    return n;
}

void do_entity(XMQParseState *state,
               size_t l,
               size_t c,
               const char *start,
               size_t indent,
               const char *cstart,
               const char *cstop,
               const char*stop)
{
    state->element_last = create_entity(state, l, c, start, indent, cstart, cstop, stop, (xmlNode*)state->element_stack->top->data);
}

void do_comment(XMQParseState*state,
                size_t l,
                size_t c,
                const char *start,
                size_t indent,
                const char *cstart,
                const char *cstop,
                const char *stop)
{
    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    char *trimmed = xmq_un_comment(indent, ' ', start, stop);
    xmlNodePtr n = xmlNewDocComment(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    xmlAddChild(parent, n);
    state->element_last = n;
    free(trimmed);
}

void do_comment_continuation(XMQParseState*state,
                             size_t line,
                             size_t col,
                             const char *start,
                             size_t indent,
                             const char *cstart,
                             const char *cstop,
                             const char *stop)
{
    xmlNodePtr last = (xmlNode*)state->element_last;
    // We have ///* alfa *///* beta *///* gamma *///
    // and this function is invoked with "* beta *///"
    const char *i = stop-1;
    // Count n slashes from the end
    size_t n = 0;
    while (i > start && *i == '/') { n++; i--; }
    // Since we know that we are invoked pointing into a buffer with /// before start, we
    // can safely do start-n.
    char *trimmed = xmq_un_comment(indent, ' ', start-n, stop);
    size_t l = strlen(trimmed);
    char *tmp = (char*)malloc(l+2);
    tmp[0] = '\n';
    memcpy(tmp+1, trimmed, l);
    tmp[l+1] = 0;
    xmlNodeAddContent(last, (const xmlChar *)tmp);
    free(trimmed);
    free(tmp);
}

void do_element_value_text(XMQParseState *state,
                           size_t line,
                           size_t col,
                           const char *start,
                           size_t indent,
                           const char *cstart,
                           const char *cstop,
                           const char *stop)
{
    if (!state->parsing_doctype)
    {
        xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    else
    {
        size_t l = cstop-cstart;
        char *tmp = (char*)malloc(l+1);
        memcpy(tmp, cstart, l);
        tmp[l] = 0;
        state->doq->docptr_.xml->intSubset = xmlNewDtd(state->doq->docptr_.xml, (xmlChar*)tmp, NULL, NULL);
        xmlNodePtr n = (xmlNodePtr)state->doq->docptr_.xml->intSubset;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(tmp);

        state->parsing_doctype = false;
    }
}

void do_element_value_quote(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            size_t indent,
                            const char *cstart,
                            const char *cstop,
                            const char *stop)
{
    char *trimmed = xmq_un_quote(col-1, ' ', start, stop, true);
    if (!state->parsing_doctype)
    {
        xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    else
    {
        // "<!DOCTYPE "=10  ">"=1 NULL=1
        size_t tn = strlen(trimmed);
        size_t n = tn+10+1+12;
        char *buf = (char*)malloc(n);
        strcpy(buf, "<!DOCTYPE ");
        memcpy(buf+10, trimmed, tn);
        memcpy(buf+10+tn, "><foo></foo>", 12);
        buf[n-1] = 0;
        xmlDtdPtr dtd = parse_doctype_raw(state->doq, buf, buf+n-1);
        if (!dtd)
        {
            state->error_nr = XMQ_ERROR_BAD_DOCTYPE;
            longjmp(state->error_handler, 1);
        }
        state->doq->docptr_.xml->intSubset = dtd;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, (xmlNodePtr)dtd);
        state->parsing_doctype = false;
        free(buf);
    }
    free(trimmed);
}

void do_element_value_entity(XMQParseState *state,
                             size_t line,
                             size_t col,
                             const char *start,
                             size_t indent,
                             const char *cstart,
                             const char *cstop,
                             const char *stop)
{
    create_entity(state, line, col, start, indent, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_element_value_compound_quote(XMQParseState *state,
                                     size_t line,
                                     size_t col,
                                     const char *start,
                                     size_t indent,
                                     const char *cstart,
                                     const char *cstop,
                                     const char *stop)
{
    do_quote(state, line, col, start, indent, cstart, cstop, stop);
}

void do_element_value_compound_entity(XMQParseState *state,
                                      size_t line,
                                      size_t col,
                                      const char *start,
                                      size_t indent,
                                      const char *cstart,
                                      const char *cstop,
                                      const char *stop)
{
    do_entity(state, line, col, start, indent, cstart, cstop, stop);
}

void do_attr_ns(XMQParseState *state,
                size_t line,
                size_t col,
                const char *start,
                size_t indent,
                const char *cstart,
                const char *cstop,
                const char *stop)
{
    // printf("ATTR NS >%.*s<\n", (int)(cstop-cstart), cstart);

}

void do_attr_ns_declaration(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            size_t indent,
                            const char *cstart,
                            const char *cstop,
                            const char *stop)
{
    //printf("ATTR NS DECLARATION >%.*s<\n", (int)(cstop-cstart), cstart);
}

void do_attr_key(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
    size_t n = stop - start;
    char *key = (char*)malloc(n+1);
    memcpy(key, start, n);
    key[n] = 0;

    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    xmlAttrPtr attr =  xmlNewProp(parent, (xmlChar*)key, NULL);

    //xmlAttrPtr attr_with_ns = xmlNewNsProp(parent, ns, (xmlChar*)key, NULL);
    //xmlAttrPtr an           = xmlNewNsProp(parent, ns, (xmlChar*)key, NULL);

    // The new attribute attr should now be added to the parent elements: properties list.
    // Remember this attr so that we can set the value.
    state->element_last = attr;

    free(key);
}

void do_attr_value_text(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        size_t indent,
                        const char *cstart,
                        const char *cstop,
                        const char *stop)
{
    xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
    xmlAddChild((xmlNode*)state->element_last, n);
}

void do_attr_value_quote(XMQParseState*state,
                         size_t line,
                         size_t col,
                         const char *start,
                         size_t i,
                         const char *cstart,
                         const char *cstop,
                         const char *stop)
{
    create_quote(state, line, col, start, i, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_attr_value_entity(XMQParseState *state,
                          size_t l,
                          size_t c,
                          const char *start,
                          size_t indent,
                          const char *cstart,
                          const char *cstop,
                          const char*stop)
{
    create_entity(state, l, c, start, indent, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_attr_value_compound_quote(XMQParseState *state,
                                             size_t l,
                                             size_t c,
                                             const char *start,
                                             size_t indent,
                                             const char *cstart,
                                             const char *cstop,
                                             const char*stop)
{
    do_quote(state, l, c, start, indent, cstart, cstop, stop);
}

void do_attr_value_compound_entity(XMQParseState *state,
                                             size_t l,
                                             size_t c,
                                             const char *start,
                                             size_t indent,
                                             const char *cstart,
                                             const char *cstop,
                                             const char*stop)
{
    do_entity(state, l, c, start, indent, cstart, cstop, stop);
}

void create_node(XMQParseState *state, const char *start, const char *stop)
{
    size_t len = stop-start;
    char *name = (char*)malloc(len+1);
    memcpy(name, start, len);
    name[len] = 0;

    if (!strcmp(name, "!DOCTYPE"))
    {
        state->parsing_doctype = true;
    }
    else
    {
        xmlNodePtr n = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)name, NULL);
        if (state->element_last == NULL)
        {
            if (!state->implicit_root || !strcmp(name, state->implicit_root))
            {
                // There is no implicit root, or name is the same as the implicit root.
                // Then create the root node with name.
                state->element_last = n;
                xmlDocSetRootElement(state->doq->docptr_.xml, n);
                state->doq->root_.node = n;
            }
            else
            {
                // We have an implicit root and it is different from name.
                xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)state->implicit_root, NULL);
                state->element_last = root;
                xmlDocSetRootElement(state->doq->docptr_.xml, root);
                state->doq->root_.node = root;
                push_stack(state->element_stack, state->element_last);
            }
        }
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        state->element_last = n;
    }

    free(name);
}

void do_element_ns(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
    //printf("ELEMENT NS >%.*s<\n", (int)(cstop-cstart), cstart);
}

void do_ns_colon(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
}

void do_element_name(XMQParseState *state,
                     size_t line,
                     size_t col,
                     const char *start,
                     size_t indent,
                     const char *cstart,
                     const char *cstop,
                     const char *stop)
{
    create_node(state, start, stop);
}

void do_element_key(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    size_t indent,
                    const char *cstart,
                    const char *cstop,
                    const char *stop)
{
    create_node(state, start, stop);
}

void do_equals(XMQParseState *state,
               size_t line,
               size_t col,
               const char *start,
               size_t indent,
               const char *cstart,
               const char *cstop,
               const char *stop)
{
}

void do_brace_left(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_brace_right(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    size_t indent,
                    const char *cstart,
                    const char *cstop,
                    const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_apar_left(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_apar_right(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  size_t indent,
                  const char *cstart,
                  const char *cstop,
                  const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_cpar_left(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  size_t indent,
                  const char *cstart,
                  const char *cstop,
                  const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_cpar_right(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = do_##TYPE;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os)
{
    qs->indentation_space = os->indentation_space;
    qs->explicit_space = os->explicit_space;
    qs->explicit_nl = os->explicit_nl;
    qs->prefix_line = os->prefix_line;
    qs->postfix_line = os->prefix_line;
    qs->compact = os->compact;
}


void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);

    xmlChar *buffer;
    int size;
    xmlDocDumpMemoryEnc(doq->docptr_.xml,
                        &buffer,
                        &size,
                        "utf8");

    membuffer_reuse(output_settings->output_buffer,
                    (char*)buffer,
                    size);

    debug("[XMQ] xmq_print_xml wrote %zu bytes\n", size);
}

void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);
    xmlOutputBufferPtr out = xmlAllocOutputBuffer(NULL);
    if (out)
    {
        htmlDocContentDumpOutput(out, doq->docptr_.html, "utf8");
        const xmlChar *buffer = xmlBufferContent((xmlBuffer *)out->buffer);
        MemBuffer *membuf = output_settings->output_buffer;
        membuffer_append(membuf, (char*)buffer);
        debug("[XMQ] xmq_print_html wrote %zu bytes\n", membuf->used_);
        xmlOutputBufferClose(out);
    }
    /*
    xmlNodePtr child = doq->docptr_.xml->children;
    xmlBuffer *buffer = xmlBufferCreate();
    while (child != NULL)
    {
        xmlNodeDump(buffer, doq->docptr_.xml, child, 0, 0);
        child = child->next;
        }
    const char *c = (const char *)xmlBufferContent(out);
    fputs(c, stdout);
    fputs("\n", stdout);
    xmlBufferFree(buffer);
    */
}

void xmq_print_json(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    ps.doq = doq;
    if (os->compact) os->escape_newlines = true;
    ps.output_settings = os;
    assert(os->content.write);

    json_print_nodes(&ps, NULL, (xmlNode*)first, (xmlNode*)last);
    write(writer_state, "\n", NULL);
}

void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    ps.doq = doq;
    if (os->compact) os->escape_newlines = true;
    ps.output_settings = os;
    assert(os->content.write);

    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    XMQColoring *c = os->default_coloring;

    if (c->document.pre) write(writer_state, c->document.pre, NULL);
    if (c->header.pre) write(writer_state, c->header.pre, NULL);
    if (c->style.pre) write(writer_state, c->style.pre, NULL);
    if (c->header.post) write(writer_state, c->header.post, NULL);
    if (c->body.pre) write(writer_state, c->body.pre, NULL);

    if (c->content.pre) write(writer_state, c->content.pre, NULL);
    print_nodes(&ps, (xmlNode*)first, (xmlNode*)last, 0);
    if (c->content.post) write(writer_state, c->content.post, NULL);

    if (c->body.post) write(writer_state, c->body.post, NULL);
    if (c->document.post) write(writer_state, c->document.post, NULL);

    write(writer_state, "\n", NULL);
}

void xmqPrint(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    if (output_settings->output_format == XMQ_CONTENT_XML)
    {
        xmq_print_xml(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_HTML)
    {
        xmq_print_html(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_JSON)
    {
        xmq_print_json(doq, output_settings);
    }
    else
    {
        xmq_print_xmq(doq, output_settings);
    }

    if (output_settings->output_buffer &&
        output_settings->output_buffer_start &&
        output_settings->output_buffer_stop)
    {
        size_t size = membuffer_used(output_settings->output_buffer);
        char *buffer = free_membuffer_but_return_trimmed_content(output_settings->output_buffer);
        *output_settings->output_buffer_start = buffer;
        *output_settings->output_buffer_stop = buffer+size;
    }
}

void trim_text_node(xmlNode *node, XMQTrimType tt)
{
    const char *content = xml_element_content(node);
    // We remove any all whitespace node.
    // This ought to have been removed with XML_NOBLANKS alas that does not always happen.
    // Perhaps because libxml thinks that some of these are signficant whitespace.
    //
    // However we cannot really figure out exactly what is significant and what is not from
    // the default trimming. We will over-trim when going from html to htmq unless
    // --trim=none is used.
    if (is_all_xml_whitespace(content))
    {
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        return;
    }
    // This is not entirely whitespace, now use the xmq_un_quote function to remove any incidental indentation.
    // Use indent==0 and space==0 to indicate to the unquote function to assume the the first line indent
    // is the same as the second line indent! This is necessary to gracefully handle all the possible xml indentations.
    const char *start = content;
    const char *stop = start+strlen(start);
    while (start < stop && *start == ' ') start++;
    while (stop > start && *(stop-1) == ' ') stop--;

    char *trimmed = xmq_un_quote(0, 0, start, stop, false);
    if (trimmed[0] == 0)
    {
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        free(trimmed);
        return;
    }
    xmlNodeSetContent(node, (xmlChar*)trimmed);
    free(trimmed);
}

void trim_node(xmlNode *node, XMQTrimType tt)
{
    if (is_content_node(node))
    {
        trim_text_node(node, tt);
        return;
    }

    if (is_comment_node(node))
    {
        trim_text_node(node, tt);
        return;
    }

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, tt);
        i = next;
    }
}

void xmqTrimWhitespace(XMQDoc *doq, XMQTrimType tt)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        trim_node(i, tt);
        i = xml_next_sibling(i);
    }
}

char *escape_xml_comment(const char *comment)
{
    // The escape char is ␐ which is utf8 0xe2 0x90 0x90
    size_t escapes = 0;
    const char *i = comment;
    for (; *i; ++i)
    {
        if (*i == '-' && ( *(i+1) == '-' ||
                           (*(const unsigned char*)(i+1) == 0xe2 &&
                            *(const unsigned char*)(i+2) == 0x90 &&
                            *(const unsigned char*)(i+3) == 0x90)))
        {
            escapes++;
        }
    }

    // If no escapes are needed, return NULL.
    if (!escapes) return NULL;

    size_t len = i-comment;
    size_t new_len = len+escapes*3+1;
    char *tmp = (char*)malloc(new_len);

    i = comment;
    char *j = tmp;
    for (; *i; ++i)
    {
        *j++ = *i;
        if (*i == '-' && ( *(i+1) == '-' ||
                           (*(const unsigned char*)(i+1) == 0xe2 &&
                            *(const unsigned char*)(i+2) == 0x90 &&
                            *(const unsigned char*)(i+3) == 0x90)))
        {
            *j++ = 0xe2;
            *j++ = 0x90;
            *j++ = 0x90;
        }
    }
    *j = 0;

    assert( j-tmp+1 == new_len);
    return tmp;
}

char *unescape_xml_comment(const char *comment)
{
    // The escape char is ␐ which is utf8 0xe2 0x90 0x90
    size_t escapes = 0;
    const char *i = comment;

    for (; *i; ++i)
    {
        if (*i == '-' && (*(const unsigned char*)(i+1) == 0xe2 &&
                          *(const unsigned char*)(i+2) == 0x90 &&
                          *(const unsigned char*)(i+3) == 0x90))
        {
            escapes++;
        }
    }

    // If no escapes need to be removed, then return NULL.
    if (!escapes) return NULL;

    size_t len = i-comment;
    char *tmp = (char*)malloc(len+1);

    i = comment;
    char *j = tmp;
    for (; *i; ++i)
    {
        *j++ = *i;
        if (*i == '-' && (*(const unsigned char*)(i+1) == 0xe2 &&
                          *(const unsigned char*)(i+2) == 0x90 &&
                          *(const unsigned char*)(i+3) == 0x90))
        {
            // Skip the dle quote character.
            i += 3;
        }
    }
    *j++ = 0;

    size_t new_len = j-tmp;
    tmp = realloc(tmp, new_len);

    return tmp;
}

void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared)
{
    if (node->type == XML_COMMENT_NODE)
    {
        // When writing an xml comment we must replace --- with -␐-␐-.
        // An already existing -␐- is replaced with -␐␐- etc.
        char *new_content = escape_xml_comment((const char*)node->content);
        if (new_content)
        {
            // Oups, the content contains -- which must be quoted as -␐-␐
            // Likewise, if the content contains -␐-␐ it will be quoted as -␐␐-␐␐
            xmlNodePtr new_node = xmlNewComment((const xmlChar*)new_content);
            xmlReplaceNode(node, new_node);
            xmlFreeNode(node);
            free(new_content);
        }
        return;
    }
    else if (node->type == XML_CDATA_SECTION_NODE)
    {
        // When the html is loaded by the libxml2 parser it creates a cdata
        // node instead of a text node for the style content.
        // If this is allowed to be printed as is, then this creates broken html.
        // I.e. <style><![CDATA[h1{color:red;}]]></style>
        // But we want: <style>h1{color:red;}</style>
        // Workaround until I understand the proper fix, just make it a text node.
        node->type = XML_TEXT_NODE;
    }
    else if (is_entity_node(node) && inside_cdata_declared)
    {
        const char *new_content = (const char*)node->content;
        char buf[2];
        if (!node->content)
        {
            if (node->name[0] == '#')
            {
                int v = atoi(((const char*)node->name)+1);
                buf[0] = v;
                buf[1] = 0;
                new_content = buf;
            }
        }
        xmlNodePtr new_node = xmlNewDocText(doq->docptr_.xml, (const xmlChar*)new_content);
        xmlReplaceNode(node, new_node);
        xmlFreeNode(node);
        return;
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.

        bool r = inside_cdata_declared;

        if (i->name &&
            (!strcasecmp((const char*)i->name, "style") ||
             !strcasecmp((const char*)i->name, "script")))
        {
            // The html style and script nodes are declared as cdata nodes.
            // The &#10; will not be decoded, instead remain as is a ruin the style.
            // Since htmq does not care about this distinction, we have to remove any
            // quoting specified in the htmq before entering the cdata declared node.
            r = true;
        }

        fixup_html(doq, i, r);
        i = next;
    }
}

void xmq_fixup_html_before_writeout(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_html.
        fixup_html(doq, i, false);
        i = next;
    }
}

void fixup_comments(XMQDoc *doq, xmlNode *node)
{
    if (node->type == XML_COMMENT_NODE)
    {
        // An xml comment containing dle quotes for example: -␐-␐- is replaceed with ---.
        // If multiple dle quotes exists, then for example: -␐␐- is replaced with -␐-.
        char *new_content = unescape_xml_comment((const char*)node->content);
        if (new_content)
        {
            // Oups, the content contains -- which must be quoted as -␐-␐
            // Likewise, if the content contains -␐-␐ it will be quoted as -␐␐-␐␐
            xmlNodePtr new_node = xmlNewComment((const xmlChar*)new_content);
            xmlReplaceNode(node, new_node);
            xmlFreeNode(node);
            free(new_content);
        }
        return;
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_comments(doq, i);
        i = next;
    }
}

void xmq_fixup_comments_after_readin(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_comments.
        fixup_comments(doq, i);
        i = next;
    }
}

const char *xmqDocError(XMQDoc *doq)
{
    return doq->error_;
}

XMQParseError xmqDocErrno(XMQDoc *doq)
{
    return (XMQParseError)doq->errno_;
}

void xmqSetStateSourceName(XMQParseState *state, const char *source_name)
{
    if (source_name)
    {
        size_t l = strlen(source_name);
        state->source_name = (char*)malloc(l+1);
        strcpy(state->source_name, source_name);
    }
}

size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line)
{
    size_t pre_n = strlen(pre_line);
    size_t post_n = strlen(post_line);
    const char *o = start;
    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\n')
        {
            // Indent the line.
            for (int i=0; i<indent; ++i) o++;
            o--; // Remove the \n
            o += pre_n; // Add any pre line prefixes.
            o += post_n; // Add any post line suffixes (default is 1 for "\n")
        }
        o++;
    }
    return o-start;
}

void copy_and_insert(MemBuffer *mb,
                     const char *start,
                     const char *stop,
                     int num_prefix_spaces,
                     const char *implicit_indentation,
                     const char *explicit_space,
                     const char *newline,
                     const char *prefix_line,
                     const char *postfix_line)
{
    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\n')
        {
            membuffer_append_region(mb, postfix_line, NULL);
            membuffer_append_region(mb, newline, NULL);
            membuffer_append_region(mb, prefix_line, NULL);

            // Indent the next line.
            for (int i=0; i<num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
        }
        else if (c == ' ')
        {
            membuffer_append_region(mb, explicit_space, NULL);
        }
        else
        {
            membuffer_append_char(mb, c);
        }
    }
}

char *copy_lines(int num_prefix_spaces,
                 const char *start,
                 const char *stop,
                 int num_quotes,
                 bool add_nls,
                 bool add_compound,
                 const char *implicit_indentation,
                 const char *explicit_space,
                 const char *newline,
                 const char *prefix_line,
                 const char *postfix_line)
{
    MemBuffer *mb = new_membuffer();

    const char *short_start = start;
    const char *short_stop = stop;

    if (add_compound)
    {
        membuffer_append(mb, "( ");

        short_start = has_leading_space_nl(start, stop);
        if (!short_start) short_start = start;
        short_stop = has_ending_nl_space(start, stop);
        if (!short_stop || short_stop == start) short_stop = stop;

        const char *i = start;

        while (i < short_start)
        {
            membuffer_append_entity(mb, *i);
            i++;
        }
    }

    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, '\'');
    membuffer_append_region(mb, prefix_line, NULL);
    if (add_nls)
    {
        membuffer_append_region(mb, postfix_line, NULL);
        membuffer_append_region(mb, newline, NULL);
        membuffer_append_region(mb, prefix_line, NULL);
        for (int i = 0; i < num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
    }
    // Copy content into quote.
    copy_and_insert(mb, short_start, short_stop, num_prefix_spaces, implicit_indentation, explicit_space, newline, prefix_line, postfix_line);
    // Done copying content.

    if (add_nls)
    {
        membuffer_append_region(mb, postfix_line, NULL);
        membuffer_append_region(mb, newline, NULL);
        membuffer_append_region(mb, prefix_line, NULL);
        for (int i = 0; i < num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
    }

    membuffer_append_region(mb, postfix_line, NULL);
    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, '\'');

    if (add_compound)
    {
        const char *i = short_stop;

        while (i < stop)
        {
            membuffer_append_entity(mb, *i);
            i++;
        }

        membuffer_append(mb, " )");
    }

    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}

size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq)
{
    const char *i = start;
    int llq = 0, eeq = 0;
    int num = 0, max = 0;
    // Skip line leading quotes
    while (*i == '\'') { i++; llq++;  }
    const char *lstart = i; // Points to text after leading quotes.
    // Find end of line.
    while (i < stop && *i != '\n') i++;
    const char *eol = i;
    i--;
    while (i > lstart && *i == '\'') { i--; eeq++; }
    i++;
    const char *lstop = i;
    // Mark endof text inside ending quotes.
    for (i = lstart; i < lstop; ++i)
    {
        if (*i == '\'')
        {
            num++;
            if (num > max) max = num;
        }
        else
        {
            num = 0;
        }
    }
    *numq = max;
    *lq = llq;
    *eq = eeq;
    assert( (llq+eeq+(lstop-lstart)) == eol-start);
    return lstop-lstart;
}

char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings)
{
    // This code must only be invoked if there is at least one newline inside the to-be quoted text!
    MemBuffer *mb = new_membuffer();

    const char *i = start;
    bool found_nl = false;
    while (i < stop)
    {
        int numq;
        int lq = 0;
        int eq = 0;
        size_t line_len = line_length(i, stop, &numq, &lq, &eq);
        i += lq;
        for (int j = 0; j < lq; ++j) membuffer_append(mb, "&#39;");
        if (line_len > 0)
        {
            if (numq == 0 && (settings->force)) numq = 1; else numq++;
            if (numq == 2) numq++;
            for (int i=0; i<numq; ++i) membuffer_append(mb, "'");
            membuffer_append_region(mb, i, i+line_len);
            for (int i=0; i<numq; ++i) membuffer_append(mb, "'");
        }
        for (int j = 0; j < eq; ++j) membuffer_append(mb, "&#39;");
        i += line_len+eq;
        if (i < stop && *i == '\n')
        {
            if (!found_nl) found_nl = true;
            membuffer_append(mb, "&#10;");
            i++;
        }
    }
    return free_membuffer_but_return_trimmed_content(mb);
}

char *xmq_quote_default(int indent,
                        const char *start,
                        const char *stop,
                        XMQQuoteSettings *settings)
{
    bool add_nls = false;
    bool add_compound = false;
    int numq = count_necessary_quotes(start, stop, false, &add_nls, &add_compound);

    if (numq > 0)
    {
        // If nl_begin is true and we have quotes, then we have to forced newline already due to quotes at
        // the beginning or end, therefore we use indent as is, however if
        if (add_nls == false) // then we might have to adjust the indent, or even introduce a nl_begin/nl_end.
        {
            if (indent == -1)
            {
                // Special case, -1 indentation requested this means the quote should be on its own line.
                // then we must insert newlines.
                // Otherwise the indentation will be 1.
                // e.g.
                // |'
                // |alfa beta
                // |gamma delta
                // |'
                add_nls = true;
                indent = 0;
            }
            else
            {
                // We have a nonzero indentation and number of quotes is 1 or 3.
                // Then the actual source indentation will be +1 or +3.
                if (numq < 4)
                {
                    // e.g. quote at 4 will have source at 5.
                    // |    'alfa beta
                    // |     gamma delta'
                    // e.g. quote at 4 will have source at 7.
                    // |    '''alfa beta
                    // |       gamma delta'
                    indent += numq;
                }
                else
                {
                    // More than 3 quotes, then we add newlines.
                    // e.g. quote at 4 will have source at 4.
                    // |    ''''
                    // |    alfa beta '''
                    // |    gamma delta
                    // |    ''''
                    add_nls = true;
                }
            }
        }
    }
    if (numq == 0 && settings->force) numq = 1;
    return copy_lines(indent,
                      start,
                      stop,
                      numq,
                      add_nls,
                      add_compound,
                      settings->indentation_space,
                      settings->explicit_space,
                      settings->explicit_nl,
                      settings->prefix_line,
                      settings->postfix_line);
}


/**
    xmq_comment:

    Make a single line or multi line comment. Support compact mode with multiple line comments.
*/
char *xmq_comment(int indent, const char *start, const char *stop, XMQQuoteSettings *settings)
{
    assert(indent >= 0);
    assert(start);

    if (stop == NULL) stop = start+strlen(start);

    if (settings->compact)
    {
        return xmq_quote_with_entity_newlines(start, stop, settings);
    }

    return xmq_quote_default(indent, start, stop, settings);
}

int xmqForeach(XMQDoc *doq, XMQNode *xmq_node, const char *xpath, NodeCallback cb, void *user_data)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return 0;

    if (xmq_node && xmq_node->node)
    {
        xmlXPathSetContextNode(xmq_node->node, ctx);
    }

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)xpath, ctx);

    if (objects == NULL)
    {
        xmlXPathFreeContext(ctx);
        return 0;
    }

    xmlNodeSetPtr nodes = objects->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;

    if (cb)
    {
        for(int i = 0; i < size; i++)
        {
            xmlNodePtr node = nodes->nodeTab[i];
            XMQNode xn;
            xn.node = node;
            XMQProceed proceed = cb(doq, &xn, user_data);
            if (proceed == XMQ_STOP) break;
        }
    }

    xmlXPathFreeObject(objects);
    xmlXPathFreeContext(ctx);

    return size;
}

const char *xmqGetName(XMQNode *node)
{
    xmlNodePtr p = node->node;
    if (p)
    {
        return (const char*)p->name;
    }
    return NULL;
}

const char *xmqGetContent(XMQNode *node)
{
    xmlNodePtr p = node->node;
    if (p && p->children)
    {
        return (const char*)p->children->content;
    }
    return NULL;
}

XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data)
{
    const char **out = (const char **)user_data;
    xmlNodePtr n = node->node;
    if (n && n->children)
    {
        *out = (const char*)n->children->content;
    }
    else
    {
        *out = NULL;
    }
    return XMQ_STOP;
}

int32_t xmqGetInt(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    if (content[0] == '0' &&
        content[1] == 'x')
    {
        int64_t tmp = strtol(content, NULL, 16);
        return tmp;
    }

    if (content[0] == '0')
    {
        int64_t tmp = strtol(content, NULL, 8);
        return tmp;
    }

    return atoi(content);
}

int64_t xmqGetLong(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    if (content[0] == '0' &&
        content[1] == 'x')
    {
        int64_t tmp = strtol(content, NULL, 16);
        return tmp;
    }

    if (content[0] == '0')
    {
        int64_t tmp = strtol(content, NULL, 8);
        return tmp;
    }

    return atol(content);
}

const char *xmqGetString(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    return content;
}

double xmqGetDouble(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    return atof(content);
}

bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt)
{
    xmlDocPtr doc;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION ;

    int parse_options = XML_PARSE_NOCDATA | XML_PARSE_NONET;
    if (tt != XMQ_TRIM_NONE) parse_options |= XML_PARSE_NOBLANKS;

    doc = xmlReadMemory(start, stop-start, doq->source_name_, NULL, parse_options);
    if (doc == NULL)
    {
        doq->errno_ = XMQ_ERROR_PARSING_XML;
        // Let libxml2 print the error message.
        doq->error_ = NULL;
        return false;
    }

    if (doq->docptr_.xml)
    {
        xmlFreeDoc(doq->docptr_.xml);
    }
    doq->docptr_.xml = doc;
    xmlCleanupParser();

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt)
{
    htmlDocPtr doc;
    xmlNode *roo_element = NULL;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION

    int parse_options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    if (tt != XMQ_TRIM_NONE) parse_options |= HTML_PARSE_NOBLANKS;

    doc = htmlReadMemory(start, stop-start, "foof", NULL, parse_options);

    if (doc == NULL)
    {
        doq->errno_ = XMQ_ERROR_PARSING_HTML;
        // Let libxml2 print the error message.
        doq->error_ = NULL;
        return false;
    }

    roo_element = xmlDocGetRootElement(doc);

    if (roo_element == NULL)
    {
        PRINT_ERROR("empty document\n");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return 0;
    }

    if (doq->docptr_.html)
    {
        xmlFreeDoc(doq->docptr_.html);
    }
    doq->docptr_.html = doc;
    xmlCleanupParser();

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmqParseBufferWithType(XMQDoc *doq,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            XMQTrimType tt)
{
    bool ok = true;

    // Unicode files might lead with a byte ordering mark.
    start = skip_any_potential_bom(start, stop);
    if (!start) return false;

    XMQContentType detected_ct = xmqDetectContentType(start, stop);
    if (ct == XMQ_CONTENT_DETECT)
    {
        ct = detected_ct;
    }
    else
    {
        if (ct != detected_ct)
        {
            if (detected_ct == XMQ_CONTENT_XML && ct == XMQ_CONTENT_HTML)
            {
                // This is fine! We might be loading a fragment of html
                // that is detected as xml.
            }
            else
            {
                switch (ct) {
                case XMQ_CONTENT_XMQ: doq->errno_ = XMQ_ERROR_EXPECTED_XMQ; break;
                case XMQ_CONTENT_HTMQ: doq->errno_ = XMQ_ERROR_EXPECTED_HTMQ; break;
                case XMQ_CONTENT_XML: doq->errno_ = XMQ_ERROR_EXPECTED_XML; break;
                case XMQ_CONTENT_HTML: doq->errno_ = XMQ_ERROR_EXPECTED_HTML; break;
                case XMQ_CONTENT_JSON: doq->errno_ = XMQ_ERROR_EXPECTED_JSON; break;
                default: break;
                }
                ok = false;
                goto exit;
            }
        }
    }

    switch (ct)
    {
    case XMQ_CONTENT_XMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_HTMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_XML: ok = xmq_parse_buffer_xml(doq, start, stop, tt); break;
    case XMQ_CONTENT_HTML: ok = xmq_parse_buffer_html(doq, start, stop, tt); break;
    case XMQ_CONTENT_JSON: ok = xmq_parse_buffer_json(doq, start, stop, implicit_root); break;
    default: break;
    }

exit:

    if (ok)
    {
        if (tt == XMQ_TRIM_HEURISTIC ||
            (tt == XMQ_TRIM_DEFAULT && (
                ct == XMQ_CONTENT_XML ||
                ct == XMQ_CONTENT_HTML)))
        {
            xmqTrimWhitespace(doq, tt);
        }
    }

    return ok;
}

bool load_stdin(XMQDoc *doq, size_t *out_fsize, const char **out_buffer)
{
    bool rc = true;
    int blocksize = 1024;
    char block[blocksize];

    MemBuffer *mb = new_membuffer();

    int fd = 0;
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            PRINT_ERROR("Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        membuffer_append_region(mb, block, block + n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);

    membuffer_append_null(mb);

    *out_fsize = mb->used_-1;
    *out_buffer = free_membuffer_but_return_trimmed_content(mb);

    return rc;
}

bool load_file(XMQDoc *doq, const char *file, size_t *out_fsize, const char **out_buffer)
{
    bool rc = false;
    char *buffer = NULL;

    FILE *f = fopen(file, "rb");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: No such file or directory\n", file);
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    debug("[XMQ] file size %zu\n", fsize);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer)
    {
        doq->errno_ = XMQ_ERROR_OOM;
        doq->error_ = build_error_message("xmq: %s: File too big, out of memory\n", file);
        goto exit;
    }

    size_t block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    size_t n = 0;
    do {
        size_t r = fread(buffer+n, 1, block_size, f);
        debug("[XMQ] read %zu bytes total %zu\n", r, n);
        if (!r) break;
        n += r;
    } while (n < fsize);

    debug("[XMQ] read total %zu bytes fsize %zu bytes\n", n, fsize);

    if (n != fsize) {
        rc = false;
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: Cannot read file\n", file);
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;
    rc = true;

exit:

    *out_fsize = fsize;
    *out_buffer = buffer;
    return rc;
}

bool xmqParseFileWithType(XMQDoc *doq,
                          const char *file,
                          const char *implicit_root,
                          XMQContentType ct,
                          XMQTrimType tt)
{
    bool rc = true;
    size_t fsize;
    const char *buffer;

    if (file)
    {
        xmqSetDocSourceName(doq, file);
        rc = load_file(doq, file, &fsize, &buffer);
    }
    else
    {
        xmqSetDocSourceName(doq, "-");
        rc = load_stdin(doq, &fsize, &buffer);
    }
    if (!rc) return false;

    rc = xmqParseBufferWithType(doq, buffer, buffer+fsize, implicit_root, ct, tt);

    free((void*)buffer);

    return rc;
}


xmlDtdPtr parse_doctype_raw(XMQDoc *doq, const char *start, const char *stop)
{
    size_t n = stop-start;
    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
    if (ctxt == NULL) {
        return NULL;
    }

    xmlParseChunk(ctxt, start, n, 0);
    xmlParseChunk(ctxt, start, 0, 1);

    doc = ctxt->myDoc;
    int rc = ctxt->wellFormed;
    xmlFreeParserCtxt(ctxt);

    if (!rc) {
        return NULL;
    }

    xmlDtdPtr dtd = xmlCopyDtd(doc->intSubset);
    xmlFreeDoc(doc);

    return dtd;
}

bool xmq_parse_buffer_json(XMQDoc *doq,
                           const char *start,
                           const char *stop,
                           const char *implicit_root)
{
    bool rc = true;
    XMQOutputSettings *os = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, os);
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    push_stack(state->element_stack, doq->docptr_.xml);
    // Now state->element_stack->top->data points to doq->docptr_;
    state->element_last = NULL; // Will be set when the first node is created.
    // The doc root will be set when the first element node is created.

    // Time to tokenize the buffer and invoke the parse callbacks.
    xmq_tokenize_buffer_json(state, start, stop);

    if (xmqStateErrno(state))
    {
        rc = false;
        doq->errno_ = xmqStateErrno(state);
        doq->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(os);

    return rc;
}

#include"parts/always.c"
#include"parts/entities.c"
#include"parts/hashmap.c"
#include"parts/stack.c"
#include"parts/membuffer.c"
#include"parts/json.c"
#include"parts/text.c"
#include"parts/utf8.c"
#include"parts/xml.c"
#include"parts/xmq_internals.c"
#include"parts/xmq_parser.c"
#include"parts/xmq_printer.c"
