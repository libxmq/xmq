/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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
#include"parts/colors.h"
#include"parts/core.h"
#include"parts/default_themes.h"
#include"parts/entities.h"
#include"parts/utf8.h"
#include"parts/hashmap.h"
#include"parts/ixml.h"
#include"parts/membuffer.h"
#include"parts/stack.h"
#include"parts/text.h"
#include"parts/vector.h"
#include"parts/xml.h"
#include"parts/xmq_parser.h"
#include"parts/xmq_printer.h"
#include"parts/yaep.h"

// XMQ STRUCTURES ////////////////////////////////////////////////

#include"parts/xmq_internals.h"

// FUNCTIONALITY /////////////////////////////////////////////////

#include"parts/json.h"
#include"parts/ixml.h"

//////////////////////////////////////////////////////////////////////////////////

void add_nl(XMQParseState *state);
XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data);
size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line);
void copy_and_insert(MemBuffer *mb, const char *start, const char *stop, int num_prefix_spaces, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
char *copy_lines(int num_prefix_spaces, const char *start, const char *stop, int num_quotes, bool add_nls, bool add_compound, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os);
xmlNodePtr create_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop, xmlNodePtr parent);
void create_node(XMQParseState *state, const char *start, const char *stop);
void update_namespace_href(XMQParseState *state, xmlNsPtr ns, const char *start, const char *stop);
xmlNodePtr create_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix,  xmlNodePtr parent);
void debug_content_comment(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void debug_content_value(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void debug_content_quote(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void do_attr_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_ns_declaration(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_value_compound_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_compound_quote(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_value_quote(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_comment(XMQParseState*state, size_t l, size_t c, const char *start, const char *stop, const char *suffix);
void do_comment_continuation(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_apar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_apar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_brace_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_brace_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_cpar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_cpar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_equals(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_name(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_compound_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_compound_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_ns_colon(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix);
void do_whitespace(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
bool find_line(const char *start, const char *stop, size_t *indent, const char **after_last_non_space, const char **eol);
const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop);
const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop);
void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared);
void fixup_comments(XMQDoc *doq, xmlNode *node, int depth);
void generate_dom_from_yaep_node(xmlDocPtr doc, xmlNodePtr node, YaepTreeNode *n, int depth, int index);
void handle_yaep_syntax_error(int err_tok_num, void *err_tok_attr, int start_ignored_tok_num, void *start_ignored_tok_attr,
                              int start_recovered_tok_num, void *start_recovered_tok_attr);

bool has_leading_ending_quote(const char *start, const char *stop);
bool is_safe_char(const char *i, const char *stop);
size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq);
bool load_file(XMQDoc *doq, const char *file, size_t *out_fsize, const char **out_buffer);
bool load_stdin(XMQDoc *doq, size_t *out_fsize, const char **out_buffer);
bool need_separation_before_entity(XMQPrintState *ps);
const char *node_yaep_type_to_string(YaepTreeNodeType t);
size_t num_utf8_bytes(char c);
void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num);
void reset_ansi(XMQParseState *state);
void reset_ansi_nl(XMQParseState *state);
const char *skip_any_potential_bom(const char *start, const char *stop);
void text_print_node(XMQPrintState *ps, xmlNode *node);
void text_print_nodes(XMQPrintState *ps, xmlNode *from);
bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop);
bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop);
void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop);
void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop);
bool xmqVerbose();
void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks);
bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);
void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_json(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_text(XMQDoc *doq, XMQOutputSettings *output_settings);
char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings);
char *xmq_quote_default(int indent, const char *start, const char *stop, XMQQuoteSettings *settings);
const char *xml_element_type_to_string(xmlElementType type);
const char *indent_depth(int i);
void free_indent_depths();

xmlNode *merge_surrounding_text_nodes(xmlNode *node);
xmlNode *merge_hex_chars_node(xmlNode *node);

// Declare tokenize_whitespace tokenize_name functions etc...
#define X(TYPE) void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, const char *stop, const char *suffix);
LIST_OF_XMQ_TOKENS
#undef X

// Declare debug_whitespace debug_name functions etc...
#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix);
LIST_OF_XMQ_TOKENS
#undef X

//////////////////////////////////////////////////////////////////////////////////

char ansi_reset_color[] = "\033[0m";

void xmqSetupDefaultColors(XMQOutputSettings *os)
{
    bool dark_mode = os->bg_dark_mode;
    XMQTheme *theme = os->theme;
    if (os->render_theme == NULL)
    {
        if (os->render_to == XMQ_RENDER_TEX) dark_mode = false;
        os->render_theme = dark_mode?"darkbg":"lightbg";
    }
    else
    {
        if (!strcmp(os->render_theme, "darkbg"))
        {
            dark_mode = true;
        }
        else if (!strcmp(os->render_theme, "lightbg"))
        {
            dark_mode = false;
        }
    }

    verbose("(xmq) use theme %s\n", os->render_theme);
    installDefaultThemeColors(theme);

    os->indentation_space = theme->indentation_space; // " ";
    os->explicit_space = theme->explicit_space; // " ";
    os->explicit_nl = theme->explicit_nl; // "\n";
    os->explicit_tab = theme->explicit_tab; // "\t";
    os->explicit_cr = theme->explicit_cr; // "\r";

    if (os->render_to == XMQ_RENDER_PLAIN)
    {
    }
    else
    if (os->render_to == XMQ_RENDER_TERMINAL)
    {
        setup_terminal_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_HTML)
    {
        setup_html_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_TEX)
    {
        setup_tex_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }

    if (os->only_style)
    {
        printf("%s\n", theme->style.pre);
        exit(0);
    }

}

const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp);
const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp)
{
#ifdef PLATFORM_WINAPI
    const char *tmp = ansiWin((int)n);
    char *p = *pp;
    char *color = p;
    strcpy(p, tmp);
    p += strlen(tmp);
    *p++ = 0;
    *pp = p;
    return color;
#else
    XMQColorDef *def = &colors[n];
    char *p = *pp;
    // Remember where the color starts in the buffer.
    char *color = p;
    char tmp[128];
    generate_ansi_color(tmp, 128, def);
    // Append the new color to the buffer.
    strcpy(p, tmp);
    p += strlen(tmp);
    *p++ = 0;
    // Export the new position in the buffer
    *pp = p;
    // Return the color position;
    return color;
#endif
}
void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    if (!use_color) return;

    XMQColorDef *colors = theme->colors_darkbg;
    if (!dark_mode) colors = theme->colors_lightbg;

    char *commands = (char*)malloc(4096);
    os->free_me = commands;
    char *p = commands;

    const char *c = add_color(colors, XMQ_COLOR_C, &p);
    theme->comment.pre = c;
    theme->comment_continuation.pre = c;

    c = add_color(colors, XMQ_COLOR_Q, &p);
    theme->quote.pre = c;

    c = add_color(colors, XMQ_COLOR_E, &p);
    theme->entity.pre = c;
    theme->element_value_entity.pre = c;
    theme->element_value_compound_entity.pre = c;
    theme->attr_value_entity.pre = c;
    theme->attr_value_compound_entity.pre = c;

    c = add_color(colors, XMQ_COLOR_NS, &p);
    theme->element_ns.pre = c;
    theme->attr_ns.pre = c;

    c = add_color(colors, XMQ_COLOR_EN, &p);
    theme->element_name.pre = c;

    c = add_color(colors,XMQ_COLOR_EK, &p);
    theme->element_key.pre = c;

    c = add_color(colors, XMQ_COLOR_EKV, &p);
    theme->element_value_text.pre = c;
    theme->element_value_quote.pre = c;
    theme->element_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_AK, &p);
    theme->attr_key.pre = c;

    c = add_color(colors, XMQ_COLOR_AKV, &p);
    theme->attr_value_text.pre = c;
    theme->attr_value_quote.pre = c;
    theme->attr_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_CP, &p);
    theme->cpar_left.pre  = c;
    theme->cpar_right.pre = c;

    c = add_color(colors, XMQ_COLOR_NSD, &p);
    theme->ns_declaration.pre = c;

    c = add_color(colors, XMQ_COLOR_UW, &p);
    theme->unicode_whitespace.pre = c;

    c = add_color(colors, XMQ_COLOR_XLS, &p);
    theme->ns_override_xsl.pre = c;

    theme->whitespace.pre  = NOCOLOR;
    theme->equals.pre      = NOCOLOR;
    theme->brace_left.pre  = NOCOLOR;
    theme->brace_right.pre = NOCOLOR;
    theme->apar_left.pre    = NOCOLOR;
    theme->apar_right.pre   = NOCOLOR;
    theme->ns_colon.pre = NOCOLOR;

}

void setup_html_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    os->indentation_space = " ";
    os->explicit_nl = "\n";
    if (!render_raw)
    {
        theme->document.pre =
            "<!DOCTYPE html>\n<html>\n";
        theme->document.post =
            "</html>";
        theme->header.pre =
            "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=5\"><style>";
        theme->header.post =
            "</style></head>";

        MemBuffer *style_pre = new_membuffer();

        membuffer_append(style_pre,
                         "@media screen and (orientation: portrait) { pre { font-size: 2vw; } }"
                         "@media screen and (orientation: landscape) { pre { max-width: 98%; } }"
                         "pre.xmq_dark {white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#263338;border:solid 1px #555555;display:inline-block;padding:1em;color:white;}\n"
                         "pre.xmq_light{white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#ffffcc;border:solid 1px #888888;display:inline-block;padding:1em;color:black;}\n"
                         "body.xmq_dark {background-color:black;}\n"
                         "body.xmq_light {}\n");

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_html_color(buf, 128, &theme->colors_darkbg[i], colorName(i));
            membuffer_append(style_pre, buf);
        }
        membuffer_append(style_pre, "pre.xmq_light {\n");

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_html_color(buf, 128, &theme->colors_lightbg[i], colorName(i));
            membuffer_append(style_pre, buf);
        }

        membuffer_append(style_pre, "pre.xmq_dark {}\n}\n");
        membuffer_append_null(style_pre);

        theme->style.pre = free_membuffer_but_return_trimmed_content(style_pre);
        os->free_me = (void*)theme->style.pre;
        if (dark_mode)
        {
            theme->body.pre = "<body class=\"xmq_dark\">";
        }
        else
        {
            theme->body.pre = "<body class=\"xmq_light\">";
        }

        theme->body.post =
            "</body>";
    }

    theme->content.pre = "<pre>";
    theme->content.post = "</pre>";

    const char *mode = "xmq_light";
    if (dark_mode) mode = "xmq_dark";

    char *buf = (char*)malloc(1024);
    os->free_and_me = buf;
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
    theme->content.pre = buf;

    theme->whitespace.pre  = NULL;
    theme->indentation_whitespace.pre = NULL;
    theme->unicode_whitespace.pre  = "<xmqUW>";
    theme->unicode_whitespace.post  = "</xmqUW>";
    theme->equals.pre      = NULL;
    theme->brace_left.pre  = NULL;
    theme->brace_right.pre = NULL;
    theme->apar_left.pre    = NULL;
    theme->apar_right.pre   = NULL;
    theme->cpar_left.pre = "<xmqCP>";
    theme->cpar_left.post = "</xmqCP>";
    theme->cpar_right.pre = "<xmqCP>";
    theme->cpar_right.post = "</xmqCP>";
    theme->quote.pre = "<xmqQ>";
    theme->quote.post = "</xmqQ>";
    theme->entity.pre = "<xmqE>";
    theme->entity.post = "</xmqE>";
    theme->comment.pre = "<xmqC>";
    theme->comment.post = "</xmqC>";
    theme->comment_continuation.pre = "<xmqC>";
    theme->comment_continuation.post = "</xmqC>";
    theme->element_ns.pre = "<xmqNS>";
    theme->element_ns.post = "</xmqNS>";
    theme->element_name.pre = "<xmqEN>";
    theme->element_name.post = "</xmqEN>";
    theme->element_key.pre = "<xmqEK>";
    theme->element_key.post = "</xmqEK>";
    theme->element_value_text.pre = "<xmqEKV>";
    theme->element_value_text.post = "</xmqEKV>";
    theme->element_value_quote.pre = "<xmqEKV>";
    theme->element_value_quote.post = "</xmqEKV>";
    theme->element_value_entity.pre = "<xmqE>";
    theme->element_value_entity.post = "</xmqE>";
    theme->element_value_compound_quote.pre = "<xmqEKV>";
    theme->element_value_compound_quote.post = "</xmqEKV>";
    theme->element_value_compound_entity.pre = "<xmqE>";
    theme->element_value_compound_entity.post = "</xmqE>";
    theme->attr_ns.pre = "<xmqNS>";
    theme->attr_ns.post = "</xmqNS>";
    theme->attr_key.pre = "<xmqAK>";
    theme->attr_key.post = "</xmqAK>";
    theme->attr_value_text.pre = "<xmqAKV>";
    theme->attr_value_text.post = "</xmqAKV>";
    theme->attr_value_quote.pre = "<xmqAKV>";
    theme->attr_value_quote.post = "</xmqAKV>";
    theme->attr_value_entity.pre = "<xmqE>";
    theme->attr_value_entity.post = "</xmqE>";
    theme->attr_value_compound_quote.pre = "<xmqAKV>";
    theme->attr_value_compound_quote.post = "</xmqAKV>";
    theme->attr_value_compound_entity.pre = "<xmqE>";
    theme->attr_value_compound_entity.post = "</xmqE>";
    theme->ns_declaration.pre = "<xmqNSD>";
    theme->ns_declaration.post = "</xmqNSD>";
    theme->ns_override_xsl.pre = "<xmqXSL>";
    theme->ns_override_xsl.post = "</xmqXSL>";
    theme->ns_colon.pre = NULL;
}

void setup_tex_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    XMQColorDef *colors = theme->colors_darkbg;
    if (!dark_mode) colors = theme->colors_lightbg;

    os->indentation_space = "\\xmqI ";
    os->explicit_space = " ";
    os->explicit_nl = "\\linebreak\n";

    if (!render_raw)
    {
        theme->document.pre =
            "\\documentclass[10pt,a4paper]{article}\n"
            "\\usepackage{color}\n"
            "\\usepackage{bold-extra}\n";

        char *style_pre = (char*)malloc(4096);
        char *p = style_pre;

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_tex_color(buf, 128, &theme->colors_lightbg[i], colorName(i));
            strcpy(p, buf);
            p += strlen(p);
            *p++ = '\n';
        }

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            const char *bold_pre = "";
            const char *bold_post = "";
            const char *underline_pre = "";
            const char *underline_post = "";

            if (colors[i].bold)
            {
                bold_pre = "\\textbf{";
                bold_post = "}";
            }
            if (colors[i].underline)
            {
                underline_pre = "\\underline{";
                underline_post = "}";
            }

            snprintf(buf, 128, "\\newcommand{\\%s}[1]{{\\color{%s}%s%s#1%s%s}}\n",
                     colorName(i), colorName(i), bold_pre, underline_pre, bold_post, underline_post);

            strcpy(p, buf);
            p += strlen(p);
        }

        const char *cmds =
            "\\newcommand{\\xmqI}[0]{{\\mbox{\\ }}}\n";

        strcpy(p, cmds);
        p += strlen(p);
        *p = 0;

        theme->style.pre = style_pre;

        theme->body.pre = "\n\\begin{document}\n";
        theme->body.post = "\n\\end{document}\n";
    }

    theme->content.pre = "\\texttt{\\flushleft\\noindent ";
    theme->content.post = "\n}\n";
    theme->whitespace.pre  = NULL;
    theme->indentation_whitespace.pre = NULL;
    theme->unicode_whitespace.pre  = "\\xmqUW{";
    theme->unicode_whitespace.post  = "}";
    theme->equals.pre      = NULL;
    theme->brace_left.pre  = NULL;
    theme->brace_right.pre = NULL;
    theme->apar_left.pre    = NULL;
    theme->apar_right.pre   = NULL;
    theme->cpar_left.pre = "\\xmqCP{";
    theme->cpar_left.post = "}";
    theme->cpar_right.pre = "\\xmqCP{";
    theme->cpar_right.post = "}";
    theme->quote.pre = "\\xmqQ{";
    theme->quote.post = "}";
    theme->entity.pre = "\\xmqE{";
    theme->entity.post = "}";
    theme->comment.pre = "\\xmqC{";
    theme->comment.post = "}";
    theme->comment_continuation.pre = "\\xmqC{";
    theme->comment_continuation.post = "}";
    theme->element_ns.pre = "\\xmqNS{";
    theme->element_ns.post = "}";
    theme->element_name.pre = "\\xmqEN{";
    theme->element_name.post = "}";
    theme->element_key.pre = "\\xmqEK{";
    theme->element_key.post = "}";
    theme->element_value_text.pre = "\\xmqEKV{";
    theme->element_value_text.post = "}";
    theme->element_value_quote.pre = "\\xmqEKV{";
    theme->element_value_quote.post = "}";
    theme->element_value_entity.pre = "\\xmqE{";
    theme->element_value_entity.post = "}";
    theme->element_value_compound_quote.pre = "\\xmqEKV{";
    theme->element_value_compound_quote.post = "}";
    theme->element_value_compound_entity.pre = "\\xmqE{";
    theme->element_value_compound_entity.post = "}";
    theme->attr_ns.pre = "\\xmqNS{";
    theme->attr_ns.post = "}";
    theme->attr_key.pre = "\\xmqAK{";
    theme->attr_key.post = "}";
    theme->attr_value_text.pre = "\\xmqAKV{";
    theme->attr_value_text.post = "}";
    theme->attr_value_quote.pre = "\\xmqAKV{";
    theme->attr_value_quote.post = "}";
    theme->attr_value_entity.pre = "\\xmqE{";
    theme->attr_value_entity.post = "}";
    theme->attr_value_compound_quote.pre = "\\xmqAKV{";
    theme->attr_value_compound_quote.post = "}";
    theme->attr_value_compound_entity.pre = "\\xmqE{";
    theme->attr_value_compound_entity.post = "}";
    theme->ns_declaration.pre = "\\xmqNSD{";
    theme->ns_declaration.post = "}";
    theme->ns_override_xsl.pre = "\\xmqXSL{";
    theme->ns_override_xsl.post = "}";
    theme->ns_colon.pre = NULL;
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

void xmqOverrideColor(XMQOutputSettings *os, const char *render_style, XMQSyntax sy, const char *pre, const char *post, const char *ns)
{
    //
}

int xmqStateErrno(XMQParseState *state)
{
    return (int)state->error_nr;
}

#define X(TYPE) \
    void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start,const char *stop,const char *suffix) { \
        if (!state->simulated) { \
            const char *pre, *post;  \
            getThemeStrings(state->output_settings, COLOR_##TYPE, &pre, &post); \
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
    if (!state->generated_error_msg && state->generating_error_msg)
    {
        state->generated_error_msg = free_membuffer_but_return_trimmed_content(state->generating_error_msg);
        state->generating_error_msg = NULL;
    }
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
    XMQTheme *theme = (XMQTheme*)malloc(sizeof(XMQTheme));
    memset(theme, 0, sizeof(XMQTheme));
    os->theme = theme;

    os->indentation_space = theme->indentation_space = " ";
    os->explicit_space = theme->explicit_space = " ";
    os->explicit_nl = theme->explicit_nl = "\n";
    os->explicit_tab = theme->explicit_tab = "\t";
    os->explicit_cr = theme->explicit_cr = "\r";
    os->add_indent = 4;
    os->use_color = false;

    return os;
}

void xmqFreeOutputSettings(XMQOutputSettings *os)
{
    if (os->theme)
    {
        free(os->theme);
        os->theme = NULL;
    }
    if (os->free_me)
    {
        free(os->free_me);
        os->free_me = NULL;
    }
    if (os->free_and_me)
    {
        free(os->free_and_me);
        os->free_and_me = NULL;
    }
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

void xmqSetBackgroundMode(XMQOutputSettings *os, bool bg_dark_mode)
{
    os->bg_dark_mode = bg_dark_mode;
}

void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines)
{
    os->escape_newlines = escape_newlines;
}

void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit)
{
    os->escape_non_7bit = escape_non_7bit;
}

void xmqSetEscapeTabs(XMQOutputSettings *os, bool escape_tabs)
{
    os->escape_tabs = escape_tabs;
}

void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format)
{
    os->output_format = output_format;
}

void xmqSetOmitDecl(XMQOutputSettings *os, bool omit_decl)
{
    os->omit_decl = omit_decl;
}

void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to)
{
    os->render_to = render_to;
}

void xmqSetRenderRaw(XMQOutputSettings *os, bool render_raw)
{
    os->render_raw = render_raw;
}

void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_name)
{
    os->render_theme = theme_name;
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
    os->content.write = (XMQWrite)(void*)membuffer_append_region;
    os->error.writer_state = os->output_buffer;
    os->error.write = (XMQWrite)(void*)membuffer_append_region;
}

void xmqSetupPrintSkip(XMQOutputSettings *os, size_t *skip)
{
    os->output_skip = skip;
}

XMQParseCallbacks *xmqNewParseCallbacks()
{
    XMQParseCallbacks *callbacks = (XMQParseCallbacks*)malloc(sizeof(XMQParseCallbacks));
    memset(callbacks, 0, sizeof(XMQParseCallbacks));
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
    state->element_stack = stack_create();

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

    if (!stop) stop = start + strlen(start);

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
    state->error_nr = XMQ_ERROR_NONE;

    if (state->parse->init) state->parse->init(state);

    XMQOutputSettings *output_settings = state->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    const char *pre = output_settings->theme->content.pre;
    const char *post = output_settings->theme->content.post;
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
        XMQParseError error_nr = state->error_nr;
        if (error_nr == XMQ_ERROR_INVALID_CHAR && state->last_suspicios_quote_end)
        {
            // Add warning about suspicious quote before the error.
            generate_state_error_message(state, XMQ_WARNING_QUOTES_NEEDED, start, stop);
        }
        generate_state_error_message(state, error_nr, start, stop);

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
    const char *buffer = NULL;
    size_t fsize = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    XMQDoc *doq = xmqNewDoc();

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

    free((void*)buffer);
    xmqFreeDoc(doq);

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


/** Scan a line, ie until \n pr \r\n or \r or NULL.
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
        if (*i == '\n' || *i == '\r')
        {
            if (*i == '\r' && *(i+1) == '\n') i++;
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
    xmq_debug_enabled_ = e;
}

bool xmqDebugging()
{
    return xmq_debug_enabled_;
}

void xmqSetTrace(bool e)
{
    xmq_trace_enabled_ = e;
}

bool xmqTracing()
{
    return xmq_trace_enabled_;
}

void xmqSetVerbose(bool e)
{
    xmq_verbose_enabled_ = e;
}

bool xmqVerbose() {
    return xmq_verbose_enabled_;
}

const char *build_error_message(const char* fmt, ...)
{
    char *buf = (char*)malloc(4096);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    buf[4095] = 0;
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
        if (*i == ' ') {
            i++;
        }
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
        indent++;
    }
    if (*(stop-1) == ' ')
    {
        if (stop-1 >= start)
        {
            stop--;
        }
    }

    assert(start <= stop);
    char *foo = xmq_trim_quote(indent, space, start, stop);
    return foo;
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
        char *buf = strndup(start, stop-start);
        return buf;
    }

    size_t append_newlines = 0;

    // Check if the final line is all spaces.
    if (has_ending_nl_space(start, stop, NULL))
    {
        // So it is, now trim from the end.
        while (stop > start)
        {
            char c = *(stop-1);
            if (c == '\n') append_newlines++;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            stop--;
        }
    }
    if (append_newlines > 0) append_newlines--;

    if (stop == start)
    {
        // Oups! Quote was all space and newlines.
        char *buf = (char*)malloc(append_newlines+1);
        size_t i;
        for (i = 0; i < append_newlines; ++i) buf[i] = '\n';
        buf[i] = 0;
        return buf;
    }

    size_t prepend_newlines = 0;

    // Check if the first line is all spaces.
    if (has_leading_space_nl(start, stop, NULL))
    {
        // The first line is all spaces, trim leading spaces and newlines!
        ignore_first_indent = true;
        // Skip the already scanned first line.
        start = eol;
        const char *i = start;
        while (i < stop)
        {
            char c = *i;
            if (c == '\n')
            {
                start = i+1; // Restart find lines from here.
                prepend_newlines++;
            }
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

    size_t prepend_spaces = 0;

    if (!ignore_first_indent &&
        indent >= incidental)
    {
        // The first indent is relevant and it is bigger than the incidental.
        // We need to prepend the output line with spaces that are not in the source!
        // But, only if there is more than one line with actual non spaces!
        prepend_spaces = indent - incidental;
    }

    // Allocate max size of output buffer, it usually becomes smaller
    // when incidental indentation and trailing whitespace is removed.
    size_t n = stop-start+prepend_spaces+prepend_newlines+append_newlines+1;
    char *output = (char*)malloc(n);
    char *o = output;

    // Insert any necessary prepended spaces due to source indentation of the line.
    if (space != 0)
    {
        while (prepend_spaces) { *o++ = space; prepend_spaces--; }
    }

    // Insert any necessary prepended newlines.
    while (prepend_newlines) { *o++ = '\n'; prepend_newlines--; }

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
            // Skip the incidental indentation.
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
    // Insert any necessary appended newlines.
    while (append_newlines) { *o++ = '\n'; append_newlines--; }
    *o++ = 0;
    size_t real_size = o-output;
    output = (char*)realloc(output, real_size);
    return output;
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

#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix) { \
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
                         const char *stop,
                         const char *suffix)
{
    char *tmp = xmq_quote_as_c(start, stop);
    WRITE_ARGS("{value \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
}


void debug_content_quote(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
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
                           const char *stop,
                           const char *suffix)
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

void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *callbacks, XMQRenderFormat render_format)
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
    doq->docptr_.xml = (xmlDocPtr)doc;
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

const char *xmqGetDocSourceName(XMQDoc *doq)
{
    return doq->source_name_;
}

XMQContentType xmqGetOriginalContentType(XMQDoc *doq)
{
    return doq->original_content_type_;
}

size_t xmqGetOriginalSize(XMQDoc *doq)
{
    return doq->original_size_;
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
    if (state->generating_error_msg)
    {
        free_membuffer_and_free_content(state->generating_error_msg);
        state->generating_error_msg = NULL;
    }
    stack_free(state->element_stack);
    state->element_stack = NULL;
    // Settings are not freed here.
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
    if (doq->yaep_grammar_)
    {
        yaepFreeGrammar ((YaepParseRun*)doq->yaep_parse_run_, (YaepGrammar*)doq->yaep_grammar_);
        yaepFreeParseRun ((YaepParseRun*)doq->yaep_parse_run_);
        doq->yaep_grammar_ = NULL;
        doq->yaep_parse_run_ = NULL;
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

bool xmqParseBuffer(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root, int flags)
{
    bool rc = true;
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->merge_text = !(flags & XMQ_FLAG_NOMERGE);
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    stack_push(state->element_stack, doq->docptr_.xml);
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

bool xmqParseFile(XMQDoc *doq, const char *file, const char *implicit_root, int flags)
{
    bool ok = true;
    char *buffer = NULL;
    size_t fsize = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;
    size_t block_size = 0;
    size_t n = 0;

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

    block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    n = 0;
    do {
        // We need to read smaller blocks because of a bug in Windows C-library..... blech.
        if (n + block_size > fsize) block_size = fsize - n;
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

    ok = xmqParseBuffer(doq, buffer, buffer+fsize, implicit_root, flags);

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
                   const char *stop,
                   const char *suffix)
{
}

xmlNodePtr create_quote(XMQParseState *state,
                       size_t l,
                       size_t col,
                       const char *start,
                       const char *stop,
                       const char *suffix,
                       xmlNodePtr parent)
{
    size_t indent = col - 1;
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(indent, ' ', start, stop, true);
    xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    if (state->merge_text)
    {
        n = xmlAddChild(parent, n);
    }
    else
    {
        // I want to prevent merging of this new text node with previous text nodes....
        // Alas there is no such setting in libxml2 so I perform the addition explicit here.
        // Check the source for xmlAddChild.
        n->parent = parent;
        if (parent->children == NULL)
        {
            parent->children = n;
            parent->last = n;
        }
        else
        {
            xmlNodePtr prev = parent->last;
	    prev->next = n;
            n->prev = prev;
            parent->last = n;
        }
    }
    free(trimmed);
    return n;
}

void do_quote(XMQParseState *state,
              size_t l,
              size_t col,
              const char *start,
              const char *stop,
              const char *suffix)
{
    state->element_last = create_quote(state, l, col, start, stop, suffix,
                                       (xmlNode*)state->element_stack->top->data);
}

xmlNodePtr create_entity(XMQParseState *state,
                         size_t l,
                         size_t c,
                         const char *start,
                         const char *stop,
                         const char *suffix,
                         xmlNodePtr parent)
{
    char *tmp = strndup(start, stop-start);
    xmlNodePtr n = NULL;
    if (tmp[1] == '#')
    {
        // Character entity.
        if (!state->merge_text)
        {
            // Do not merge with surrounding text.
            n = xmlNewCharRef(state->doq->docptr_.xml, (const xmlChar *)tmp);
        }
        else
        {
            // Make inte text that will be merged.
            UTF8Char uni;
            int uc = 0;
            if (tmp[2] == 'x') uc = strtol(tmp+3, NULL, 16);
            else uc = strtol(tmp+2, NULL, 10);
            size_t len = encode_utf8(uc, &uni);
            char buf[len+1];
            memcpy(buf, uni.bytes, len);
            buf[len] = 0;
            n = xmlNewDocText(state->doq->docptr_.xml, (xmlChar*)buf);
        }
    }
    else
    {
        // Named references are kept as is.
        n = xmlNewReference(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    n = xmlAddChild(parent, n);
    free(tmp);

    return n;
}

void do_entity(XMQParseState *state,
               size_t l,
               size_t c,
               const char *start,
               const char *stop,
               const char *suffix)
{
    state->element_last = create_entity(state, l, c, start, stop, suffix, (xmlNode*)state->element_stack->top->data);
}

void do_comment(XMQParseState*state,
                size_t line,
                size_t col,
                const char *start,
                const char *stop,
                const char *suffix)
{
    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    size_t indent = col-1;
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_comment(indent, ' ', start, stop);
    xmlNodePtr n = xmlNewDocComment(state->doq->docptr_.xml, (const xmlChar *)trimmed);

    if (state->add_pre_node_before)
    {
        // Insert comment before this node.
        xmlAddPrevSibling((xmlNodePtr)state->add_pre_node_before, n);
    }
    else if (state->add_post_node_after)
    {
        // Insert comment after this node.
        xmlAddNextSibling((xmlNodePtr)state->add_post_node_after, n);
    }
    else
    {
        xmlAddChild(parent, n);
    }
    state->element_last = n;
    free(trimmed);
}

void do_comment_continuation(XMQParseState*state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
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
    size_t indent = col-1;
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
                           const char *stop,
                           const char *suffix)
{
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(start, stop);
        xmlNodePtr n = (xmlNodePtr)xmlNewPI((xmlChar*)state->pi_name, (xmlChar*)content);
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(content);

        state->parsing_pi = false;
        free((char*)state->pi_name);
        state->pi_name = NULL;
    }
    else if (state->parsing_doctype)
    {
        size_t l = stop-start;
        char *tmp = (char*)malloc(l+1);
        memcpy(tmp, start, l);
        tmp[l] = 0;
        state->doq->docptr_.xml->intSubset = xmlNewDtd(state->doq->docptr_.xml, (xmlChar*)tmp, NULL, NULL);
        xmlNodePtr n = (xmlNodePtr)state->doq->docptr_.xml->intSubset;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(tmp);

        state->parsing_doctype = false;
        state->doctype_found = true;
    }
    else
    {
        xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
}

void do_element_value_quote(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            const char *stop,
                            const char *suffix)
{
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(col-1, ' ', start, stop, true);
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(trimmed, trimmed+strlen(trimmed));
        xmlNodePtr n = (xmlNodePtr)xmlNewPI((xmlChar*)state->pi_name, (xmlChar*)content);
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        state->parsing_pi = false;
        free((char*)state->pi_name);
        state->pi_name = NULL;
        free(content);
    }
    else if (state->parsing_doctype)
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
        if (state->add_pre_node_before)
        {
            // Insert doctype before this node.
            xmlAddPrevSibling((xmlNodePtr)state->add_pre_node_before, (xmlNodePtr)dtd);
        }
        else
        {
            // Append doctype to document.
            xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
            xmlAddChild(parent, (xmlNodePtr)dtd);
        }
        state->parsing_doctype = false;
        state->doctype_found = true;
        free(buf);
    }
    else
    {
        xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    free(trimmed);
}

void do_element_value_entity(XMQParseState *state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_element_value_compound_quote(XMQParseState *state,
                                     size_t line,
                                     size_t col,
                                     const char *start,
                                     const char *stop,
                                     const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
}

void do_element_value_compound_entity(XMQParseState *state,
                                      size_t line,
                                      size_t col,
                                      const char *start,
                                      const char *stop,
                                      const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
}

void do_attr_ns(XMQParseState *state,
                size_t line,
                size_t col,
                const char *start,
                const char *stop,
                const char *suffix)
{
    if (!state->declaring_xmlns)
    {
        // Normal attribute namespace found before the attribute key, eg x:alfa=123 xlink:href=http...
        char *ns = strndup(start, stop-start);
        state->attribute_namespace = ns;
    }
    else
    {
        // This is the first namespace after the xmlns declaration, eg. xmlns:xsl = http....
        // The xsl has already been handled in do_ns_declaration that used suffix
        // to peek ahead to the xsl name.
    }
}

void do_ns_declaration(XMQParseState *state,
                       size_t line,
                       size_t col,
                       const char *start,
                       const char *stop,
                       const char *suffix)
{
    // We found a namespace. It is either a default declaration xmlns=... or xmlns:prefix=...
    //
    // We can see the difference here since the parser will invoke with suffix
    // either pointing to stop (xmlns=) or after stop (xmlns:foo=)
    xmlNodePtr element = (xmlNode*)state->element_stack->top->data;
    xmlNsPtr ns = NULL;
    if (stop == suffix)
    {
        // Stop is the same as suffix, so no prefix has been added.
        // I.e. this is a default namespace, eg: xmlns=uri
        ns = xmlNewNs(element,
                      NULL,
                      NULL);
        debug("[XMQ] create default namespace in element %s\n", element->name);
        if (!ns)
        {
            // Oups, this element already has a default namespace.
            // This is probably due to multiple xmlns=uri declarations. Is this an error or not?
            xmlNsPtr *list = xmlGetNsList(state->doq->docptr_.xml,
                                          element);
            for (int i = 0; list[i]; ++i)
            {
                if (!list[i]->prefix)
                {
                    ns = list[i];
                    break;
                }
            }
            free(list);
        }
        if (element->ns == NULL)
        {
            debug("[XMQ] set default namespace in element %s prefix=%s href=%s\n", element->name, ns->prefix, ns->href);
            xmlSetNs(element, ns);
        }
        state->default_namespace = ns;
    }
    else
    {
        // This a new namespace with a prefix. xmlns:prefix=uri
        // Stop points to the colon, suffix points to =.
        // The prefix starts at stop+1.
        size_t len = suffix-(stop+1);
        char *name = strndup(stop+1, len);
        ns = xmlNewNs(element,
                      NULL,
                      (const xmlChar *)name);

        if (!ns)
        {
            // Oups, this namespace has already been created, for example due to the namespace prefix
            // of the element itself, eg: abc:element(xmlns:abc = uri)
            // Lets pick this ns up and reuse it.
            xmlNsPtr *list = xmlGetNsList(state->doq->docptr_.xml,
                                          element);
            for (int i = 0; list[i]; ++i)
            {
                if (list[i]->prefix && !strcmp((char*)list[i]->prefix, name))
                {
                    ns = list[i];
                    break;
                }
            }
            free(list);
        }
        free(name);
    }

    if (!ns)
    {
        fprintf(stderr, "Internal error: expected namespace to be created/found.\n");
        assert(ns);
    }
    state->declaring_xmlns = true;
    state->declaring_xmlns_namespace = ns;
}

void do_attr_key(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    size_t n = stop - start;
    char *key = (char*)malloc(n+1);
    memcpy(key, start, n);
    key[n] = 0;

    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    xmlAttrPtr attr = NULL;

    if (!state->attribute_namespace)
    {
        // A normal attribute with no namespace.
        attr =  xmlNewProp(parent, (xmlChar*)key, NULL);
    }
    else
    {
        xmlNsPtr ns = xmlSearchNs(state->doq->docptr_.xml,
                                  parent,
                                  (const xmlChar *)state->attribute_namespace);
        if (!ns)
        {
            // The namespaces does not yet exist. Lets create it.. Lets hope it will be declared
            // inside the attributes of this node. Use a temporary href for now.
            ns = xmlNewNs(parent,
                          NULL,
                          (const xmlChar *)state->attribute_namespace);
        }
        attr = xmlNewNsProp(parent, ns, (xmlChar*)key, NULL);
        free(state->attribute_namespace);
        state->attribute_namespace = NULL;
    }

    // The new attribute attr should now be added to the parent elements: properties list.
    // Remember this attr as the last element so that we can set the value.
    state->element_last = attr;

    free(key);
}

void update_namespace_href(XMQParseState *state,
                           xmlNsPtr ns,
                           const char *start,
                           const char *stop)
{
    if (!stop) stop = start+strlen(start);

    char *href = strndup(start, stop-start);
    ns->href = (const xmlChar*)href;
    debug("[XMQ] update namespace prefix=%s with href=%s\n", ns->prefix, href);

    if (start[0] == 0 && ns == state->default_namespace)
    {
        xmlNodePtr element = (xmlNode*)state->element_stack->top->data;
        debug("[XMQ] remove default namespace in element %s\n", element->name);
        xmlSetNs(element, NULL);
        state->default_namespace = NULL;
        return;
    }
}

void do_attr_value_text(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        const char *stop,
                        const char *suffix)
{
    if (state->declaring_xmlns)
    {
        assert(state->declaring_xmlns_namespace);

        update_namespace_href(state, (xmlNsPtr)state->declaring_xmlns_namespace, start, stop);
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        return;
    }
    xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
    xmlAddChild((xmlNode*)state->element_last, n);
}

void do_attr_value_quote(XMQParseState*state,
                         size_t line,
                         size_t col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
{
    if (state->declaring_xmlns)
    {
        char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(col-1, ' ', start, stop, true);
        update_namespace_href(state, (xmlNsPtr)state->declaring_xmlns_namespace, trimmed, NULL);
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        free(trimmed);
        return;
    }
    create_quote(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_attr_value_entity(XMQParseState *state,
                          size_t line,
                          size_t col,
                          const char *start,
                          const char *stop,
                          const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_attr_value_compound_quote(XMQParseState *state,
                                  size_t line,
                                  size_t col,
                                  const char *start,
                                  const char *stop,
                                  const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
}

void do_attr_value_compound_entity(XMQParseState *state,
                                             size_t line,
                                             size_t col,
                                             const char *start,
                                             const char *stop,
                                             const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
}

void create_node(XMQParseState *state, const char *start, const char *stop)
{
    size_t len = stop-start;
    char *name = strndup(start, len);

    if (!strcmp(name, "!DOCTYPE"))
    {
        state->parsing_doctype = true;
    }
    else if (name[0] == '?')
    {
        state->parsing_pi = true;
        state->pi_name = strdup(name+1); // Drop the ?
    }
    else
    {
        xmlNodePtr new_node = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)name, NULL);
        if (state->element_last == NULL)
        {
            if (!state->implicit_root || !strcmp(name, state->implicit_root))
            {
                // There is no implicit root, or name is the same as the implicit root.
                // Then create the root node with name.
                state->element_last = new_node;
                xmlDocSetRootElement(state->doq->docptr_.xml, new_node);
                state->doq->root_.node = new_node;
            }
            else
            {
                // We have an implicit root and it is different from name.
                xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)state->implicit_root, NULL);
                state->element_last = root;
                xmlDocSetRootElement(state->doq->docptr_.xml, root);
                state->doq->root_.node = root;
                stack_push(state->element_stack, state->element_last);
            }
        }
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, new_node);

        if (state->element_namespace)
        {
            // Have a namespace before the element name, eg abc:work
            xmlNsPtr ns = xmlSearchNs(state->doq->docptr_.xml,
                                      new_node,
                                      (const xmlChar *)state->element_namespace);
            if (!ns)
            {
                // The namespaces does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = xmlNewNs(new_node,
                              NULL,
                              (const xmlChar *)state->element_namespace);
                debug("[XMQ] created namespace prefix=%s in element %s\n", state->element_namespace, name);
            }
            debug("[XMQ] setting namespace prefix=%s for element %s\n", state->element_namespace, name);
            xmlSetNs(new_node, ns);
            free(state->element_namespace);
            state->element_namespace = NULL;
        }
        else if (state->default_namespace)
        {
            // We have a default namespace.
            xmlNsPtr ns = (xmlNsPtr)state->default_namespace;
            assert(ns->prefix == NULL);
            debug("[XMQ] set default namespace with href=%s for element %s\n", ns->href, name);
            xmlSetNs(new_node, ns);
        }

        state->element_last = new_node;
    }

    free(name);
}

void do_element_ns(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    char *ns = strndup(start, stop-start);
    state->element_namespace = ns;
}

void do_ns_colon(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
}

void do_element_name(XMQParseState *state,
                     size_t line,
                     size_t col,
                     const char *start,
                     const char *stop,
                     const char *suffix)
{
    create_node(state, start, stop);
}

void do_element_key(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    create_node(state, start, stop);
}

void do_equals(XMQParseState *state,
               size_t line,
               size_t col,
               const char *start,
               const char *stop,
               const char *suffix)
{
}

void do_brace_left(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
}

void do_brace_right(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
}

void do_apar_left(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
}

void do_apar_right(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
}

void do_cpar_left(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
}

void do_cpar_right(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
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
                        "utf-8");

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
    if (doq == NULL || doq->docptr_.xml == NULL) return;
    xmq_fixup_json_before_writeout(doq);

    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    ps.pre_nodes = stack_create();
    ps.post_nodes = stack_create();
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    ps.doq = doq;
    if (os->compact) os->escape_newlines = true;
    ps.output_settings = os;
    assert(os->content.write);

    // Find any leading (doctype/comments) and ending (comments) nodes and store in pre_nodes and post_nodes inside ps.
    // Adjust the first and last pointer.
    collect_leading_ending_comments_doctype(&ps, (xmlNode**)&first, (xmlNode**)&last);
    json_print_object_nodes(&ps, NULL, (xmlNode*)first, (xmlNode*)last);
    write(writer_state, "\n", NULL);

    stack_free(ps.pre_nodes);
    stack_free(ps.post_nodes);
}

void text_print_node(XMQPrintState *ps, xmlNode *node)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    if (is_content_node(node))
    {
        const char *content = xml_element_content(node);
        write(writer_state, content, NULL);
    }
    else if (is_entity_node(node))
    {
        const char *name = xml_element_name(node);
        write(writer_state, "<ENTITY>", NULL);
        write(writer_state, name, NULL);
    }
    else if (is_element_node(node))
    {
        text_print_nodes(ps, node->children);
    }
}

void text_print_nodes(XMQPrintState *ps, xmlNode *from)
{
    xmlNode *i = from;

    while (i)
    {
        text_print_node(ps, i);
        i = xml_next_sibling(i);
    }
}

void xmq_print_text(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;

    XMQPrintState ps = {};
    ps.doq = doq;
    ps.output_settings = os;

    text_print_nodes(&ps, (xmlNode*)first);
}

void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *os)
{
    if (doq == NULL || doq->docptr_.xml == NULL) return;
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
    XMQTheme *theme = os->theme;

    if (theme->document.pre) write(writer_state, theme->document.pre, NULL);
    if (theme->header.pre) write(writer_state, theme->header.pre, NULL);
    if (theme->style.pre) write(writer_state, theme->style.pre, NULL);
    if (theme->header.post) write(writer_state, theme->header.post, NULL);
    if (theme->body.pre) write(writer_state, theme->body.pre, NULL);

    if (theme->content.pre) write(writer_state, theme->content.pre, NULL);
    print_nodes(&ps, (xmlNode*)first, (xmlNode*)last, 0);
    if (theme->content.post) write(writer_state, theme->content.post, NULL);

    if (theme->body.post) write(writer_state, theme->body.post, NULL);
    if (theme->document.post) write(writer_state, theme->document.post, NULL);

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
    else if (output_settings->output_format == XMQ_CONTENT_TEXT)
    {
        xmq_print_text(doq, output_settings);
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
        if (output_settings->output_skip)
        {
            *output_settings->output_skip = 0;
            if (output_settings->output_format == XMQ_CONTENT_XML && output_settings->omit_decl)
            {
                // Skip <?xml version="1.0" encoding="utf-8"?>\n
                *output_settings->output_skip = 39;
            }
        }
    }
}

void trim_text_node(xmlNode *node, int flags)
{
    // If node has whitespace preserve set, then do not trim.
    // if (xmlNodeGetSpacePreserve (node)) return;

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

void trim_node(xmlNode *node, int flags)
{
    debug("[XMQ] trim %s\n", xml_element_type_to_string(node->type));

    if (is_content_node(node))
    {
        trim_text_node(node, flags);
        return;
    }

    if (is_comment_node(node))
    {
        trim_text_node(node, flags);
        return;
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return;

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

void xmqTrimWhitespace(XMQDoc *doq, int flags)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

/*
xmlNode *merge_surrounding_text_nodes(xmlNode *node)
{
    const char *val = (const char *)node->name;
    // Not a hex entity.
    if (val[0] != '#' || val[1] != 'x') return node->next;

    debug("[XMQ] merge hex %s chars %s\n", val, xml_element_type_to_string(node->type));

    UTF8Char uni;
    int uc = strtol(val+2, NULL, 16);
    size_t len = encode_utf8(uc, &uni);
    char buf[len+1];
    memcpy(buf, uni.bytes, len);
    buf[len] = 0;

    xmlNodePtr prev = node->prev;
    xmlNodePtr next = node->next;
    if (prev && prev->type == XML_TEXT_NODE)
    {
        xmlNodeAddContentLen(prev, (xmlChar*)buf, len);
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        debug("[XMQ] merge left\n");
    }
    if (next && next->type == XML_TEXT_NODE)
    {
        xmlNodeAddContent(prev, next->content);
        xmlNode *n = next->next;
        xmlUnlinkNode(next);
        xmlFreeNode(next);
        next = n;
        debug("[XMQ] merge right\n");
    }

    return next;
}

xmlNode *merge_hex_chars_node(xmlNode *node)
{
    if (node->type == XML_ENTITY_REF_NODE)
    {
        return merge_surrounding_text_nodes(node);
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return node->next;

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        i = merge_hex_chars_node(i);
    }
    return node->next;
}

void xmqMergeHexCharEntities(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        i = merge_hex_chars_node(i);
    }
}
*/

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

    assert( (size_t)((j-tmp)+1) == new_len);
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
    tmp = (char*)realloc(tmp, new_len);

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

char *depths_[64] = {};

const char *indent_depth(int i)
{
    if (i < 0 || i > 63) return "----";
    char *c = depths_[i];
    if (!c)
    {
        c = (char*)malloc(i*4+1);
        memset(c, ' ', i*4);
        c[i*4] = 0;
        depths_[i] = c;
    }
    return c;
}

void free_indent_depths()
{
    for (int i = 0; i < 64; ++i)
    {
        if (depths_[i])
        {
            free(depths_[i]);
            depths_[i] = NULL;
        }
    }
}

const char *xml_element_type_to_string(xmlElementType type)
{
    switch (type)
    {
	case XML_ELEMENT_NODE: return "element";
	case XML_ATTRIBUTE_NODE: return "attribute";
	case XML_TEXT_NODE: return "text";
	case XML_CDATA_SECTION_NODE: return "cdata";
	case XML_ENTITY_REF_NODE: return "entity_ref";
	case XML_ENTITY_NODE: return "entity";
	case XML_PI_NODE: return "pi";
	case XML_COMMENT_NODE: return "comment";
	case XML_DOCUMENT_NODE: return "document";
	case XML_DOCUMENT_TYPE_NODE: return "document_type";
	case XML_DOCUMENT_FRAG_NODE: return "document_frag";
	case XML_NOTATION_NODE: return "notation";
	case XML_HTML_DOCUMENT_NODE: return "html_document";
	case XML_DTD_NODE: return "dtd";
	case XML_ELEMENT_DECL: return "element_decl";
	case XML_ATTRIBUTE_DECL: return "attribute_decl";
	case XML_ENTITY_DECL: return "entity_decl";
	case XML_NAMESPACE_DECL: return "namespace_decl";
	case XML_XINCLUDE_START: return "xinclude_start";
	case XML_XINCLUDE_END: return "xinclude_end";
	case XML_DOCB_DOCUMENT_NODE: return "docb_document";
    }
    return "?";
}

void fixup_comments(XMQDoc *doq, xmlNode *node, int depth)
{
    debug("[XMQ] fixup comments %s|%s %s\n", indent_depth(depth), node->name, xml_element_type_to_string(node->type));
    if (node->type == XML_COMMENT_NODE)
    {
        // An xml comment containing dle escapes for example: -␐-␐- is replaceed with ---.
        // If multiple dle escapes exists, then for example: -␐␐- is replaced with -␐-.
        char *content_needed_escaping = unescape_xml_comment((const char*)node->content);
        if (content_needed_escaping)
        {
            if (xmq_debug_enabled_)
            {
                char *from = xmq_quote_as_c((const char*)node->content, NULL);
                char *to = xmq_quote_as_c(content_needed_escaping, NULL);
                debug("[XMQ] fix comment \"%s\" to \"%s\"\n", from, to);
            }

            xmlNodePtr new_node = xmlNewComment((const xmlChar*)content_needed_escaping);
            xmlReplaceNode(node, new_node);
            xmlFreeNode(node);
            free(content_needed_escaping);
        }
        return;
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return;

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_comments(doq, i, depth+1);
        i = next;
    }
}

void xmq_fixup_comments_after_readin(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    debug("[XMQ] fixup comments after readin\n");

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_comments.
        fixup_comments(doq, i, 0);
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

void xmqSetPrintAllParsesIXML(XMQParseState *state, bool all_parses)
{
    state->ixml_all_parses = all_parses;
}

void xmqSetTryToRecoverIXML(XMQParseState *state, bool try_recover)
{
    state->ixml_try_to_recover = try_recover;
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

        short_start = has_leading_space_nl(start, stop, NULL);
        if (!short_start) short_start = start;
        short_stop = has_ending_nl_space(start, stop, NULL);
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

int xmqForeach(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data)
{
    return xmqForeachRel(doq, xpath, cb, user_data, NULL);
}

int xmqForeachRel(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data, XMQNode *relative)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return 0;

    if (relative && relative->node)
    {
        xmlXPathSetContextNode(relative->node, ctx);
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

int32_t xmqGetInt(XMQDoc *doq, const char *xpath)
{
    return xmqGetIntRel(doq, xpath, NULL);
}

int32_t xmqGetIntRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

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

int64_t xmqGetLong(XMQDoc *doq, const char *xpath)
{
    return xmqGetLongRel(doq, xpath, NULL);
}

int64_t xmqGetLongRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

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

const char *xmqGetString(XMQDoc *doq, const char *xpath)
{
    return xmqGetStringRel(doq, xpath, NULL);
}

const char *xmqGetStringRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

    return content;
}

double xmqGetDouble(XMQDoc *doq, const char *xpath)
{
    return xmqGetDoubleRel(doq, xpath, NULL);
}

double xmqGetDoubleRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

    if (!content) return 0;

    return atof(content);
}

bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION ;

    int parse_options = XML_PARSE_NOCDATA | XML_PARSE_NONET;
    bool should_trim = false;
    if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
        flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;
    if (flags & XMQ_FLAG_TRIM_NONE) should_trim = false;

    if (should_trim) parse_options |= XML_PARSE_NOBLANKS;

    xmlDocPtr doc = xmlReadMemory(start, stop-start, doq->source_name_, NULL, parse_options);
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

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    htmlDocPtr doc;
    xmlNode *roo_element = NULL;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION

    int parse_options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    bool should_trim = false;
    if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
        flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;
    if (flags & XMQ_FLAG_TRIM_NONE) should_trim = false;

    if (should_trim) parse_options |= HTML_PARSE_NOBLANKS;

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
        return 0;
    }

    if (doq->docptr_.html)
    {
        xmlFreeDoc(doq->docptr_.html);
    }
    doq->docptr_.html = doc;

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root)
{
    char *buffer = strndup(start, stop-start);
    xmlNodePtr text = xmlNewDocText(doq->docptr_.xml, (xmlChar*)buffer);
    free(buffer);

    if (implicit_root && implicit_root[0])
    {
        // We have an implicit root must be created since input is text.
        xmlNodePtr root = xmlNewDocNode(doq->docptr_.xml, NULL, (const xmlChar *)implicit_root, NULL);
        xmlDocSetRootElement(doq->docptr_.xml, root);
        doq->root_.node = root;
        xmlAddChild(root, text);
    }
    else
    {
        // There is no implicit root. Text is the new root node.
        xmlDocSetRootElement(doq->docptr_.xml, text);
    }
    return true;
}

bool xmqParseBufferWithType(XMQDoc *doq,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            int flags)
{
    bool ok = true;

    if (!stop) stop = start+strlen(start);

    // Unicode files might lead with a byte ordering mark.
    start = skip_any_potential_bom(start, stop);
    if (!start) return false;

    XMQContentType detected_ct = XMQ_CONTENT_UNKNOWN;
    if (ct != XMQ_CONTENT_IXML) detected_ct = xmqDetectContentType(start, stop);
    else ct = XMQ_CONTENT_IXML;

    if (ct == XMQ_CONTENT_DETECT)
    {
        ct = detected_ct;
    }
    else
    {
        if (ct != detected_ct && ct != XMQ_CONTENT_TEXT && ct != XMQ_CONTENT_IXML)
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

    doq->original_content_type_ = detected_ct;
    doq->original_size_ = stop-start;

    switch (ct)
    {
    case XMQ_CONTENT_XMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root, flags); break;
    case XMQ_CONTENT_HTMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root, flags); break;
    case XMQ_CONTENT_XML: ok = xmq_parse_buffer_xml(doq, start, stop, flags); break;
    case XMQ_CONTENT_HTML: ok = xmq_parse_buffer_html(doq, start, stop, flags); break;
    case XMQ_CONTENT_JSON: ok = xmq_parse_buffer_json(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_IXML: ok = xmq_parse_buffer_ixml(doq, start, stop, flags); break;
    case XMQ_CONTENT_TEXT: ok = xmq_parse_buffer_text(doq, start, stop, implicit_root); break;
    default: break;
    }

exit:

    if (ok)
    {
        bool should_trim = false;

        if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
            flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;

        if (!(flags & XMQ_FLAG_TRIM_NONE) &&
            (ct == XMQ_CONTENT_XML ||
             ct == XMQ_CONTENT_HTML))
        {
            should_trim = true;
        }

        if (should_trim) xmqTrimWhitespace(doq, flags);
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
        if (n == 0) {
            break;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            PRINT_ERROR("Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        membuffer_append_region(mb, block, block + n);
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
    size_t block_size = 0;
    size_t n = 0;

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

    block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    n = 0;
    do {
        if (n + block_size > fsize) block_size = fsize - n;
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
                          int flags)
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

    rc = xmqParseBufferWithType(doq, buffer, buffer+fsize, implicit_root, ct, flags);

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
    state->no_trim_quotes = true;
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    stack_push(state->element_stack, doq->docptr_.xml);
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

bool xmq_parse_buffer_ixml(XMQDoc *ixml_grammar,
                           const char *start,
                           const char *stop,
                           int flags)
{
    assert(ixml_grammar->yaep_grammar_ == NULL);

    bool rc = true;
    if (!stop) stop = start+strlen(start);

    XMQOutputSettings *os = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();
    parse->magic_cookie = MAGIC_COOKIE;

    XMQParseState *state = xmqNewParseState(parse, os);
    xmqSetStateSourceName(state, xmqGetDocSourceName(ixml_grammar));

    state->doq = ixml_grammar;
    state->build_xml_of_ixml = false;
    YaepGrammar *grammar = yaepNewGrammar();
    YaepParseRun *run = yaepNewParseRun(grammar);
    ixml_grammar->yaep_grammar_ = grammar;
    ixml_grammar->yaep_parse_run_ = run;
    if (xmqVerbose()) run->verbose = true;
    if (xmqDebugging()) run->debug = true;
    if (xmqTracing()) run->trace = true;

    // Lets parse the ixml source to construct a yaep grammar.
    // This yaep grammar is cached in ixml_grammar->yaep_grammar_.
    ixml_build_yaep_grammar((YaepParseRun*)ixml_grammar->yaep_parse_run_,
                            (YaepGrammar*)ixml_grammar->yaep_grammar_,
                            state,
                            start,
                            stop);

    if (xmqStateErrno(state))
    {
        rc = false;
        ixml_grammar->errno_ = xmqStateErrno(state);
        ixml_grammar->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(os);

    return rc;
}

void xmq_set_yaep_grammar(XMQDoc *doc, YaepGrammar *g)
{
    doc->yaep_grammar_ = g;
}

YaepGrammar *xmq_get_yaep_grammar(XMQDoc *doc)
{
    return (YaepGrammar *)doc->yaep_grammar_;
}

YaepParseRun *xmq_get_yaep_parse_run(XMQDoc *doc)
{
    return (YaepParseRun*)doc->yaep_parse_run_;
}

static char *input_i_;
static char *input_start_;
static char *input_stop_;

static int read_yaep_token(YaepParseRun *ps, void **attr)
{
  *attr = NULL;
  if (input_i_ >= input_stop_) return -1;

  int uc = 0;
  size_t len = 0;
  bool ok = decode_utf8(input_i_, input_stop_, &uc, &len);
  if (!ok)
  {
      fprintf(stderr, "xmq: broken utf8\n");
      exit(1);
  }
  input_i_ += len;

  return uc;
}

void handle_yaep_syntax_error(int err_tok_num,
                              void *err_tok_attr,
                              int start_ignored_tok_num,
                              void *start_ignored_tok_attr,
                              int start_recovered_tok_num,
                              void *start_recovered_tok_attr)
{
    printf("ixml: syntax error\n");
    int start = err_tok_num - 10;
    if (start < 0) start = 0;
    int stop = err_tok_num + 10;

    for (int i = start; i < stop && input_i_[i] != 0; ++i)
    {
        printf("%c", input_i_[i]);
    }
    printf("\n");
    for (int i = start; i < err_tok_num; ++i) printf (" ");
    printf("^\n");
}

const char *node_yaep_type_to_string(YaepTreeNodeType t)
{
    switch (t)
    {
    case YAEP_NIL: return "NIL";
    case YAEP_ERROR: return "ERROR";
    case YAEP_TERM: return "TERM";
    case YAEP_ANODE: return "ANODE";
    case YAEP_ALT: return "ALT";
    default:
        return "?";
    }
    return "?";
}

void collect_text(YaepTreeNode *n, MemBuffer *mb);

void collect_text(YaepTreeNode *n, MemBuffer *mb)
{
    if (n == NULL) return;
    if (n->type == YAEP_ANODE)
    {
        YaepAbstractNode *an = &n->val.anode;
        for (int i=0; an->children[i] != NULL; ++i)
        {
            YaepTreeNode *nn = an->children[i];
            collect_text(nn, mb);
        }
    }
    else
    if (n->type == YAEP_TERM)
    {
        YaepTermNode *at = &n->val.term;
        if (at->mark != '-')
        {
            UTF8Char utf8;
            size_t len = encode_utf8(at->code, &utf8);
            utf8.bytes[len] = 0;
            membuffer_append(mb, utf8.bytes);
        }
    }
    else
    {
        assert(false);
    }
}

void generate_dom_from_yaep_node(xmlDocPtr doc, xmlNodePtr node, YaepTreeNode *n, int depth, int index)
{
    if (n == NULL) return;
    if (n->type == YAEP_ANODE)
    {
        YaepAbstractNode *an = &n->val.anode;

        if (an != NULL && an->name != NULL && an->name[0] != '/' && an->mark != '-')
        {
            if (an->mark == '@')
            {
                // This should become an attribute.
                MemBuffer *mb = new_membuffer();
                collect_text(n, mb);
                membuffer_append_null(mb);
                xmlNewProp(node, (xmlChar*)an->name, (xmlChar*)mb->buffer_);
                free_membuffer_and_free_content(mb);
            }
            else
            {
                // Normal node that should be generated.
                xmlNodePtr new_node = xmlNewDocNode(doc, NULL, (xmlChar*)an->name, NULL);

                if (node == NULL)
                {
                    xmlDocSetRootElement(doc, new_node);
                }
                else
                {
                    xmlAddChild(node, new_node);
                }

                for (int i=0; an->children[i] != NULL; ++i)
                {
                    YaepTreeNode *nn = an->children[i];
                    generate_dom_from_yaep_node(doc, new_node, nn, depth+1, i);
                }
            }
        }
        else
        {
            // Skip anonymous node whose name starts with / and deleted nodes with mark=-
            for (int i=0; an->children[i] != NULL; ++i)
            {
                YaepTreeNode *nn = an->children[i];
                generate_dom_from_yaep_node(doc, node, nn, depth+1, i);
            }
        }
    }
    else if (n->type == YAEP_ALT)
    {
        xmlNodePtr new_node = xmlNewDocNode(doc, NULL, (xmlChar*)"AMBIGUOUS", NULL);
        if (node == NULL)
        {
            xmlDocSetRootElement(doc, new_node);
        }
        else
        {
            xmlAddChild(node, new_node);
        }

        YaepTreeNode *alt = n;

        generate_dom_from_yaep_node(doc, new_node, alt->val.alt.node, depth+1, 0);

        alt = alt->val.alt.next;

        while (alt && alt->type == YAEP_ALT)
        {
            generate_dom_from_yaep_node(doc, new_node, alt->val.alt.node, depth+1, 0);
            alt = alt->val.alt.next;
        }
    }
    else
    if (n->type == YAEP_TERM)
    {
        YaepTermNode *at = &n->val.term;
        if (at->mark != '-')
        {
            UTF8Char utf8;
            size_t len = encode_utf8(at->code, &utf8);
            utf8.bytes[len] = 0;

            xmlNodePtr new_node = xmlNewDocText(doc, (xmlChar*)utf8.bytes);

            if (node == NULL)
            {
                xmlDocSetRootElement(doc, new_node);
            }
            else
            {
                xmlAddChild(node, new_node);
            }
        }
    }
    else
    {
        for (int i=0; i<depth; ++i) printf("    ");
        printf("[%d] ", index);
        printf("WOOT %s\n", node_yaep_type_to_string(n->type));
    }
}

bool xmqParseBufferWithIXML(XMQDoc *doc, const char *start, const char *stop, XMQDoc *ixml_grammar, int flags)
{
    if (!doc || !start || !ixml_grammar) return false;
    if (!stop) stop = start+strlen(start);

    input_start_ = input_i_ = strndup(start, stop-start);
    if (!input_start_) return false;

    input_stop_ = input_start_ + strlen(input_start_);

    yaep_set_one_parse_flag(xmq_get_yaep_grammar(ixml_grammar),
                            (flags & XMQ_FLAG_IXML_ALL_PARSES)?0:1);

    yaep_set_error_recovery_flag(xmq_get_yaep_grammar(ixml_grammar),
                                 (flags & XMQ_FLAG_IXML_TRY_TO_RECOVER)?1:0);

    YaepParseRun *run = xmq_get_yaep_parse_run(ixml_grammar);
    YaepGrammar *grammar = xmq_get_yaep_grammar(ixml_grammar);
    run->read_token = read_yaep_token;
    run->syntax_error = handle_yaep_syntax_error;

    // Parse source content using the yaep grammar, previously generated from the ixml source.
    int rc = yaepParse(run, grammar);

    if (rc)
    {
        printf("xmq: could not parse input using ixml grammar: %s\n", yaep_error_message(xmq_get_yaep_grammar(ixml_grammar)));
        return false;
    }

    if (run->ambiguous_p && !(flags & XMQ_FLAG_IXML_ALL_PARSES))
    {
        fprintf(stderr, "ixml: Warning! The input can be parsed in multiple ways, ie it is ambiguous!\n");
    }

    generate_dom_from_yaep_node(doc->docptr_.xml, NULL, run->root, 0, 0);

    if (run->root) yaepFreeTree(run->root, NULL, NULL);

    return true;
}

bool xmqParseFileWithIXML(XMQDoc *doc, const char *file_name, XMQDoc *ixml_grammar, int flags)
{
    const char *buffer;
    size_t buffer_len = 0;
    bool ok = load_file(doc, file_name, &buffer_len, &buffer);

    if (!ok) return false;

    ok = xmqParseBufferWithIXML(doc, buffer, buffer+buffer_len, ixml_grammar, flags);

    free((char*)buffer);

    return ok;
}

char *xmqLogDoc(XMQDoc *doc)
{
    return strdup("{howdy}");
}

char *xmqLogElement(const char *element_name, ...)
{
    va_list ap;
    va_start(ap, element_name);

    MemBuffer *mb = new_membuffer();
    membuffer_append(mb, element_name);

    char *buf = (char*)malloc(1024);
    size_t buf_size = 1024;

    for (;;)
    {
        const char *key = va_arg(ap, const char*);
        if (!key) break;
        if (*key == '}') break;
        size_t kl = strlen(key);
        if (key[kl-1] != '=') break;

        char last = membuffer_back(mb);

        if (last != '\'' && last != '(' && last != ')'  && last != '{' && last != '}')
        {
            membuffer_append(mb, " ");
        }
        membuffer_append(mb, key);

        const char *format = va_arg(ap, const char *);

        for (;;)
        {
            size_t n = vsnprintf(buf, buf_size, format, ap);
            if (n < buf_size) break;

            buf_size *= 2;
            free(buf);
            buf = (char*)malloc(buf_size);
        }

        XMQOutputSettings os;
        memset(&os, 0, sizeof(os));
        os.compact = true;
        os.escape_newlines = true;
        os.output_buffer = mb;
        os.content.writer_state = os.output_buffer;
        os.content.write = (XMQWrite)(void*)membuffer_append_region;
        os.error.writer_state = os.output_buffer;
        os.error.write = (XMQWrite)(void*)membuffer_append_region;
        os.explicit_space = " ";
        os.explicit_tab = "\t";

        XMQPrintState ps;
        memset(&ps, 0, sizeof(ps));
        ps.output_settings = &os;

        xmlNode node;
        memset(&node, 0, sizeof(node));
        node.content = (xmlChar*)buf;
        print_value(&ps , &node, LEVEL_ATTR_VALUE);
    }
    free(buf);

    va_end(ap);

    membuffer_append(mb, "}");
    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}

#include"parts/always.c"
#include"parts/colors.c"
#include"parts/core.c"
#include"parts/default_themes.c"
#include"parts/entities.c"
#include"parts/hashmap.c"
#include"parts/stack.c"
#include"parts/membuffer.c"
#include"parts/ixml.c"
#include"parts/json.c"
#include"parts/text.c"
#include"parts/utf8.c"
#include"parts/vector.c"
#include"parts/xml.c"
#include"parts/xmq_internals.c"
#include"parts/xmq_parser.c"
#include"parts/xmq_printer.c"
#include"parts/yaep.c"
