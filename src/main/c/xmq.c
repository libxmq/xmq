/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

#include<assert.h>
#include<ctype.h>
#include<float.h>
#include<limits.h>
#include<math.h>
#include<setjmp.h>
#include<stdint.h>
#include<stdio.h>
#include<string.h>

// If we are concatening a single source file dist/xmq.dc then add the define BUILDING_DIST_XMQ
//#define BUILDING_DIST_XMQ

// PART HEADERS //////////////////////////////////////////////////

#include"parts/always.h"
#include"parts/colors.h"
#include"parts/core.h"
#include"parts/default_themes.h"
#include"parts/entities.h"
#include"parts/utf8.h"
#include"parts/hashmap.h"
#include"parts/lang.h"
#include"parts/ixml.h"
#include"parts/membuffer.h"
#include"parts/stack.h"
#include"parts/text.h"
#include"parts/vector.h"
#include"parts/xml_chars.h"
#include"parts/xml_dom.h"
#include"parts/xml_parser.h"
#include"parts/xml_save.h"
#include"parts/xml_html.h"
#include"parts/xml_html_parser.h"
#include"parts/xml_html_save.h"
#include"parts/xml_xpath.h"
#include"parts/xml_uri.h"
#include"parts/xml_unicode.h"
#include"parts/xml_regexp.h"
#include"parts/xml_pattern.h"
#include"parts/xml_xsd10_types.h"
#include"parts/xml_xsd10_schemas.h"
#include"parts/xml_xsd10_parse.h"
#include"parts/xml_xsd10_build.h"
#include"parts/xml_xsd10_valid.h"
#include"parts/xml_libxslt1_xslt.h"
#include"parts/xml_libxslt1_transform.h"
#include"parts/xml.h"
#include"parts/xmq_parser.h"
#include"parts/xmq_printer.h"
#include"parts/yaep_allocate.h"
#include"parts/yaep_hashtab.h"
#include"parts/yaep_objstack.h"
#include"parts/yaep_vlobject.h"
#include"parts/yaep.h"
#include"parts/yaep_structs.h"
#include"parts/yaep_cspc.h"
#include"parts/yaep_util.h"
#include"parts/yaep_symbols.h"
#include"parts/yaep_terminal_bitset.h"
#include"parts/yaep_tree.h"
#include"parts/yaep_print.h"

// XMQ STRUCTURES ////////////////////////////////////////////////

#include"parts/xmq_internals.h"

// FUNCTIONALITY /////////////////////////////////////////////////

#include"parts/json.h"
#include"parts/yaml.h"
#include"parts/ixml.h"

//////////////////////////////////////////////////////////////////////////////////

void add_key_number(XmlDom *doc, XMQNode *root, const char *key, int number);
void add_key_string(XmlDom *doc, XMQNode *root, const char *key, const char *value);
void add_nl(XMQParseState *state);
XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data);
XMQProceed catch_single_node(XMQDoc *doc, XMQNode *node, void *user_data);
size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line);
bool check_leading_space_nl(const char *start, const char *stop);
void copy_and_insert(MemBuffer *mb, const char *start, const char *stop, int num_prefix_spaces, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
char *copy_lines(int num_prefix_spaces, const char *start, const char *stop, int num_quotes, bool use_dqs, bool add_nls, bool add_compound, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os);
XMQNode* create_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop, XMQNode* parent);
XMQStatus create_node(XMQParseState *state, const char *start, const char *stop);
XmlNs* find_ns(XMQNode* node, const char *prefix);
void update_namespace_href(XMQParseState *state, XmlNs* ns, const char *start, const char *stop);
XMQReturnXMLNode create_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix,  XMQNode* parent);
XMQStatus debug_content_comment(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
XMQStatus debug_content_comment_continuation(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
XMQStatus debug_content_value(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
XMQStatus debug_content_quote(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
XMQStatus do_attr_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_attr_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_ns_declaration(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_attr_value_compound_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
XMQStatus do_attr_value_compound_quote(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
XMQStatus do_attr_value_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
XMQStatus do_attr_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_attr_value_quote(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_comment(XMQParseState*state, size_t l, size_t c, const char *start, const char *stop, const char *suffix);
XMQStatus do_comment_continuation(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_apar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_apar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_brace_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_brace_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_cpar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_cpar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_equals(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_name(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_value_compound_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_value_compound_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_value_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_element_value_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
XMQStatus do_ns_colon(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix);
XMQStatus do_whitespace(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
bool find_line(const char *start, const char *stop, size_t *indent, const char **after_last_non_space, const char **eol);
void fixup_html(XMQDoc *doq, XMQNode *node, bool inside_cdata_declared);
void fixup_comments(XMQDoc *doq, XMQNode *node, int depth);
void fixup_ns(XMQNode* new_node, XmlNs* pns, XMQNS ns);
XmlNs *prep_ancestor_namespace(XMQNode *node, const char *uri, const char *prefix);
XmlNs *create_unique_ns(XMQNode *unique_prefix_check_node, XMQNode *declaration_node, const char *uri);
void generate_dom_from_yaep_node(XmlDom* doc, XMQNode* node, YaepTreeNode *n, YaepTreeNode *parent, int depth, int index);
void handle_yaep_syntax_error(YaepParseRun *pr,
                              int err_tok_num,
                              void *err_tok_attr,
                              int start_ignored_tok_num,
                              void *start_ignored_tok_attr,
                              int start_recovered_tok_num,
                              void *start_recovered_tok_attr);

size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq);
const char *node_yaep_type_to_string(YaepTreeNodeType t);
void reset_ansi(XMQParseState *state);
void reset_ansi_nl(XMQParseState *state);
const char *skip_any_potential_bom(const char *start, const char *stop);
void text_print_node(XMQPrintState *ps, XMQNode *node);
void text_print_nodes(XMQPrintState *ps, XMQNode *from);
void cline_print_node(XMQPrintState *ps, XMQNode *node);
void cline_print_nodes(XMQPrintState *ps, XMQNode *from);
void cline_print_xpath(XMQPrintState *ps, XMQNode *node);
void cline_print_attributes(XMQPrintState *ps, XMQNode *node);
void cline_print_attr(XMQPrintState *ps, XMQAttr *a);
bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop);
bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop);
bool write_print_memory(void *writer_state_ignored, const char *start, const char *stop);
void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop);
void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop);
bool xmqVerbose();
void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks);
bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);
bool xmq_parse_buffer_clines(XMQDoc *doq, const char *start, const char *stop);
void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_json(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_text(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_clines(XMQDoc *doq, XMQOutputSettings *output_settings);
char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings);
char *xmq_quote_default(int indent, const char *start, const char *stop, XMQQuoteSettings *settings);
const char *xml_element_type_to_string(XmlNodeKind type);
const char *indent_depth(int i);
void free_indent_depths();
int move_element(XmlDom* d, XMQNode* e, XMQNode* p, XMQMoveTo position, XMQNode* sibling);

// Declare tokenize_whitespace tokenize_name functions etc...
#define X(TYPE) XMQStatus tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, const char *stop, const char *suffix);
LIST_OF_XMQ_TOKENS
#undef X

// Declare debug_whitespace debug_name functions etc...
#define X(TYPE) XMQStatus debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix);
LIST_OF_XMQ_TOKENS
#undef X

//////////////////////////////////////////////////////////////////////////////////

char ansi_reset_color[] = "\033[0m";

void xmqSetupDefaultColors(XMQOutputSettings *os)
{
    bool use_color = os->color_mode != XMQ_MONO;
    bool dark_mode = (os->color_mode == XMQ_BG_DARK) || (os->color_mode == XMQ_BG_AUTO);
//    bool truecolor = os->truecolor;
    XMQTheme *theme = os->theme;
    if (os->render_theme_spec == NULL)
    {
        if (os->render_to == XMQ_RENDER_TEX) dark_mode = false;
        os->render_theme_spec = dark_mode?"dark":"light";
    }
    else
    {
        if (ends_with(os->render_theme_spec, NULL, "-dark"))
        {
            dark_mode = true;
        }
        if (ends_with(os->render_theme_spec, NULL, "-light"))
        {
            dark_mode = false;
        }
    }

    verbose("xmq=", "use theme %s", os->render_theme_spec);
    installTheme(theme, os->render_theme_spec);

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
        setup_terminal_coloring(os, theme, dark_mode, use_color, os->truecolor, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_HTML)
    {
        setup_html_coloring(os, theme, dark_mode, use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_TEX)
    {
        setup_tex_coloring(os, theme, dark_mode, use_color, os->render_raw);
    }

    if (os->only_style)
    {
        printf("%s\n", theme->style.pre);
        exit(0);
    }

}

const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp, bool truecolor);
const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp, bool truecolor)
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
    generate_ansi_color(tmp, 128, def, truecolor);
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
void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool truecolor, bool render_raw)
{
    if (!use_color) return;

    XMQColorDef *colors = theme->colors_darkbg;
    if (!dark_mode) colors = theme->colors_lightbg;

    char *commands = (char*)malloc(4096);
    xmq_output_settings_own(os, commands);
    char *p = commands;

    const char *c = add_color(colors, XMQ_COLOR_C, &p, truecolor);
    theme->comment.pre = c;
    theme->comment_continuation.pre = c;

    c = add_color(colors, XMQ_COLOR_Q, &p, truecolor);
    theme->quote.pre = c;

    c = add_color(colors, XMQ_COLOR_E, &p, truecolor);
    theme->entity.pre = c;
    theme->element_value_entity.pre = c;
    theme->element_value_compound_entity.pre = c;
    theme->attr_value_entity.pre = c;
    theme->attr_value_compound_entity.pre = c;

    c = add_color(colors, XMQ_COLOR_NS, &p, truecolor);
    theme->element_ns.pre = c;
    theme->attr_ns.pre = c;

    c = add_color(colors, XMQ_COLOR_EN, &p, truecolor);
    theme->element_name.pre = c;

    c = add_color(colors,XMQ_COLOR_EK, &p, truecolor);
    theme->element_key.pre = c;

    c = add_color(colors, XMQ_COLOR_EKV, &p, truecolor);
    theme->element_value_text.pre = c;
    theme->element_value_quote.pre = c;
    theme->element_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_AK, &p, truecolor);
    theme->attr_key.pre = c;

    c = add_color(colors, XMQ_COLOR_AKV, &p, truecolor);
    theme->attr_value_text.pre = c;
    theme->attr_value_quote.pre = c;
    theme->attr_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_CP, &p, truecolor);
    theme->cpar_left.pre  = c;
    theme->cpar_right.pre = c;

    c = add_color(colors, XMQ_COLOR_NSD, &p, truecolor);
    theme->ns_declaration.pre = c;

    c = add_color(colors, XMQ_COLOR_UW, &p, truecolor);
    theme->unicode_whitespace.pre = c;

    c = add_color(colors, XMQ_COLOR_XLS, &p, truecolor);
    theme->ns_override_xsl.pre = c;

    theme->whitespace.pre  = NOCOLOR;
    theme->equals.pre      = NOCOLOR;
    theme->brace_left.pre  = NOCOLOR;
    theme->brace_right.pre = NOCOLOR;
    theme->apar_left.pre    = NOCOLOR;
    theme->apar_right.pre   = NOCOLOR;
    theme->ns_colon.pre = NOCOLOR;

    theme->document.post = NOCOLOR;
}

// Append the pre-element CSS for the color class clazz
// using either the dark or the light background colors.
static void append_pre_html_coloring(MemBuffer *style_pre, XMQTheme *theme, bool dark_mode, const char *clazz)
{
    XMQColorDef *def;

    membuffer_append(style_pre, "pre.");
    membuffer_append(style_pre, clazz);
    membuffer_append(style_pre, "{white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#");

    // Lookup the bg color in dark/light.
    def = dark_mode?&theme->colors_darkbg[XMQ_COLOR_BG_INDEX]:&theme->colors_lightbg[XMQ_COLOR_BG_INDEX];
    if (def->r == -1)
    {
        // No override, use default.
        membuffer_append(style_pre, dark_mode?"263338":"ffffcc");
    }
    else
    {
        // Override of the bg color. Use it.
        char buf[7];
        snprintf(buf, 7, "%02x%02x%02x", def->r, def->g, def->b);
        membuffer_append(style_pre, buf);
    }
    membuffer_append(style_pre, ";border:solid 1px ");
    membuffer_append(style_pre, dark_mode?"#555555":"#888888");
    membuffer_append(style_pre, ";display:inline-block;padding:1em;color:#");

    // Lookup the fg color in dark/light.
    def = dark_mode?&theme->colors_darkbg[XMQ_COLOR_FG_INDEX]:&theme->colors_lightbg[XMQ_COLOR_FG_INDEX];
    if (def->r == -1)
    {
        // No override, use default.
        membuffer_append(style_pre, dark_mode?"ffffff":"000000");
    }
    else
    {
        // Override of the fg color. Use it.
        char buf[7];
        snprintf(buf, 7, "%02x%02x%02x", def->r, def->g, def->b);
        membuffer_append(style_pre, buf);
    }
    membuffer_append(style_pre, ";}\n");

    for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
    {
        char buf[128];
        generate_html_color(buf, 128, dark_mode?&theme->colors_darkbg[i]:&theme->colors_lightbg[i], colorName(i));
        membuffer_append(style_pre, "pre.");
        membuffer_append(style_pre, clazz);
        membuffer_append(style_pre, " ");
        membuffer_append(style_pre, buf);
    }
}

void setup_html_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    os->indentation_space = " ";
    os->explicit_nl = "\n";
    if (!render_raw)
    {
        // Setup automatic dark/light mode detection:
        // By default the page follows the browser setting via the CSS
        // 'prefers-color-scheme' media query and uses the class xmq_auto.
        // The default (when the browser has no preference) is light mode.
        // The dark or light mode can be forced with XMQ_BG=dark|light or
        // --bg=dark|light in which case the classes xmq_dark/xmq_light are used.
        const char *body_class = "xmq_auto";
        if (os->color_mode == XMQ_BG_DARK)
        {
            body_class = "xmq_dark";
        }
        if (os->color_mode == XMQ_BG_LIGHT)
        {
            body_class = "xmq_light";
        }

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
                         "@media screen and (orientation: landscape) { pre { max-width: 98%; } }");

        // Body class used on the <body> element.
        {
            char *body_pre = (char*)malloc(64);
            xmq_output_settings_own(os, body_pre);
            snprintf(body_pre, 64, "<body class=\"%s\">", body_class);
            theme->body.pre = body_pre;
        }

        // Default body background (light mode / no preference).
        XMQColorDef *def = &theme->colors_lightbg[XMQ_COLOR_BG_INDEX];
        if (def->r == -1) def = NULL; // No override, use default.
        membuffer_append(style_pre, "body{background-color:#");
        if (def) { char buf[7]; snprintf(buf, 7, "%02x%02x%02x", def->r, def->g, def->b); membuffer_append(style_pre, buf); }
        else membuffer_append(style_pre, "ffffff");
        membuffer_append(style_pre, ";}\n");

        // Setup CSS for dark mode (applied when the browser prefers dark mode).
        membuffer_append(style_pre, "@media (prefers-color-scheme: dark) {\n");
        membuffer_append(style_pre, "body.xmq_auto{background-color:black;}\n");
        append_pre_html_coloring(style_pre, theme, true, "xmq_auto");
        membuffer_append(style_pre, "}\n");

        // Setup CSS for forced dark mode (XMQ_BG=dark or --bg=dark).
        membuffer_append(style_pre, "body.xmq_dark{background-color:black;}\n");
        append_pre_html_coloring(style_pre, theme, true, "xmq_dark");

        // Setup CSS for light mode (applied when the browser prefers light mode).
        membuffer_append(style_pre, "@media (prefers-color-scheme: light) {\n");
        append_pre_html_coloring(style_pre, theme, false, "xmq_auto");
        membuffer_append(style_pre, "}\n");

        // Setup CSS for forced light mode (XMQ_BG=light or --bg=light).
        append_pre_html_coloring(style_pre, theme, false, "xmq_light");

        membuffer_append_null(style_pre);

        theme->style.pre = free_membuffer_but_return_trimmed_content(style_pre);
        xmq_output_settings_own(os, (void*)theme->style.pre);

        theme->body.post =
            "</body>";
    }

    theme->content.pre = "<pre>";
    theme->content.post = "</pre>";

    // The class xmq_auto automatically follows the browser dark/light mode
    // preference, see the 'prefers-color-scheme' media queries in the style
    // section above. When a dark or a light background was forced (XMQ_BG or
    // --bg) the matching xmq_dark/xmq_light class is used instead.
    const char *mode_class = "xmq_auto";
    if (os->color_mode == XMQ_BG_DARK)
    {
        mode_class = "xmq_dark";
    }
    if (os->color_mode == XMQ_BG_LIGHT)
    {
        mode_class = "xmq_light";
    }

    char *buf = (char*)malloc(1024);
    xmq_output_settings_own(os, buf);
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
    snprintf(buf, 1023, "<pre %s%s%sclass=\"xmq %s%s%s\">", idb, id, ide, mode_class, space, clazz);
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
        xmq_output_settings_own(os, style_pre);

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

void xmqOverrideColor(XMQOutputSettings *os, const char *render_style, XMQColorName cn, const char *pre, const char *post, const char *ns)
{
    //
}

int xmqStateErrno(XMQParseState *state)
{
    return (int)state->error_nr;
}

#define X(TYPE) \
    XMQStatus tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start,const char *stop,const char *suffix) { \
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
        return XMQ_OK; \
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
    os->color_mode = XMQ_MONO;
    os->allow_json_quotes = true;
    os->final_newline = true;

    return os;
}

void xmq_output_settings_own(XMQOutputSettings *os, void *buffer)
{
    assert(os->num_owned < (int)(sizeof(os->owned) / sizeof(os->owned[0])));
    os->owned[os->num_owned++] = buffer;
}

void xmqFreeOutputSettings(XMQOutputSettings *os)
{
    if (os->theme)
    {
        free(os->theme);
        os->theme = NULL;
    }
    for (int i = 0; i < os->num_owned; ++i) free(os->owned[i]);
    os->num_owned = 0;
    // A memory output buffer that was never handed over (eg after a parse error).
    if (os->output_buffer)
    {
        free_membuffer_and_free_content(os->output_buffer);
        os->output_buffer = NULL;
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

void xmqSetColorMode(XMQOutputSettings *os, XMQColorMode color_mode)
{
    os->color_mode = color_mode;
}

void xmqSetTrueColor(XMQOutputSettings *os, bool truecolor)
{
    os->truecolor = truecolor;
}

void xmqSetPreferDoubleQuotes(XMQOutputSettings *os, bool prefer_double_quotes)
{
    os->prefer_double_quotes = prefer_double_quotes;
}

void xmqSetFinalNewline(XMQOutputSettings *os, bool final_newline)
{
    os->final_newline = final_newline;
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

void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_spec)
{
    os->render_theme_spec = theme_spec;
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

bool write_print_memory(void *writer_state_ignored, const char *start, const char *stop)
{
    membuffer_append_region((MemBuffer*)writer_state_ignored, start, stop);
    return true;
}

void xmqSetupPrintMemory(XMQOutputSettings *os, char **start, char **stop)
{
    os->output_buffer_start = start;
    os->output_buffer_stop = stop;
    os->output_buffer = new_membuffer();
    os->content.writer_state = os->output_buffer;
    os->content.write = write_print_memory;
    os->error.writer_state = os->output_buffer;
    os->error.write = write_print_memory;
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

bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stopp)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    state->buffer_start = start;
    state->buffer_stop =  stopp?stopp:start + strlen(start);
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = XMQ_OK;

    XMQContentType detected_ct = xmqDetectContentType(state->buffer_start, state->buffer_stop);
    if (detected_ct != XMQ_CONTENT_XMQ)
    {
        state->generated_error_msg = strdup("xmq: you can only tokenize the xmq format");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        return false;
    }


    // The xmq source must be valid UTF-8.
    if (xmq_reject_invalid_utf8(state)) return false;

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
        XMQStatus error_nr = state->error_nr;
        if (error_nr == XMQ_ERROR_INVALID_CHAR && state->last_suspicios_quote_end)
        {
            // Add warning about suspicious quote before the error.
            generate_state_error_message(state, XMQ_WARNING_QUOTES_NEEDED, state->buffer_start, state->buffer_stop);
        }
        generate_state_error_message(state, error_nr, state->buffer_start, state->buffer_stop);

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
        output_settings->output_buffer = NULL; // Handed over to the caller.
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

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doq = rd.doc;

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
                    debug("xmq=", "content detected as xml since <?xml found");
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
                        debug("xmq=", "content detected as xml since comment start found");
                        return XMQ_CONTENT_XML;
                    }
                    // Pick up after the comment.
                    c = *i;
                }

                // Starts with <html or < html
                const char *is_html = find_word_ignore_case(i+1, stop, "html");
                if (is_html)
                {
                    debug("xmq=", "content detected as html since html found");
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
                        debug("xmq=", "content detected as html since doctype html found");
                        return XMQ_CONTENT_HTML;
                    }
                }
                // Otherwise we assume it is xml. If you are working with html content inside
                // the html, then use --html
                debug("xmq=", "content assumed to be xml");
                return XMQ_CONTENT_XML; // Or HTML...
            }
            if (c == '{' || c == '"' || c == '[' || (c >= '0' && c <= '9')) // "
            {
                debug("xmq=", "content detected as json");
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
                            debug("xmq=", "content detected as json since true/false/null found");
                            return XMQ_CONTENT_JSON;
                        }
                    }
                }
            }
            debug("xmq=", "content assumed to be xmq");
            return XMQ_CONTENT_XMQ;
        }
        i++;
    }

    debug("xmq=", "empty content assumed to be xmq");
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

void xmqSetProgress(bool e)
{
    xmq_progress_enabled_ = e;
}

void xmqSetLogHumanReadable(bool e)
{
    xmq_log_line_config_.human_readable_ = e;
}

void xmqLogFilter(const char *log_filter)
{
    xmq_log_filter_ = log_filter;
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
XMQReturnString xmq_un_quote(const char *start, const char *stop, bool remove_qs, bool is_xmq)
{
    if (!stop) stop = start+strlen(start);

    size_t j = 0;
    if (remove_qs)
    {
        // Remove the surrounding quotes, if requested.
        const char q = *start;
        if (q == '\'' || q == '"')
        {
            while (*(start+j) == q && *(stop-j-1) == q && (start+j) < (stop-j)) j++;
        }
    }

    start = start+j;
    stop = stop-j;

    return xmq_trim_quote(start, stop, is_xmq, false);
}

/**
    xmq_un_comment:

    Do the reverse of xmq_comment, Takes a comment (including /✻ ✻/ ///✻ ✻///) and removes any incidental
    indentation and trailing spaces. Returns a newly malloced buffer
    that must be free:ed later.

    The indent is 0 if the / first on the line.
    The indent is 1 if there is a single space before the starting / etc.
*/
XMQReturnString xmq_un_comment(const char *start, const char *stop)
{
    if (start > stop) return (XMQReturnString){ XMQ_ERROR_BAD_RANGE, NULL };

    const char *i = start;

    // Eat slashes.
    while (i < stop && *i == '/') i++;

    if (i == stop || *i != '*')
    {
        // No asterisk * after the slashes. This is a single line comment //.....
        i = start+2;
        // If there is a space after //, skip it.
        if (*i == ' ') i++;
        // Remove trailing spaces.
        while (i < stop && *(stop-1) == ' ') stop--;
        assert(i <= stop);
        return xmq_trim_quote(i, stop, true, true);
    }

    // Continue to eat slashes.
    while (i < stop && *i == '/') i++;

    if (i == start)
    {
        // The asterisk is first, this is a comment continuation.
        // Remove the ending / slashes.
        while (*(stop-1) == '/' && stop > start) stop--;
    }
    else
    {
        // There is an asterisk after the slashes. A standard /* */ comment
        // Remove the surrounding / slashes.
        size_t j = 0;
        while (*(start+j) == '/' && *(stop-j-1) == '/' && (start+j) < (stop-j))
        {
            j++;
        }

        start = start+j;
        stop = stop-j;
    }

    // Check that the star is there.
    assert(*start == '*' && *(stop-1) == '*');
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

    return xmq_trim_quote(start, stop, true, true);
}

bool check_leading_space_nl(const char *start, const char *stop)
{
    while (start < stop && *start == ' ') start++;
    // All spaces, not space_nl.
    if (start == stop) return false;
    // Spaces and now nl, then it is space_nl!
    if (*start == '\n') return true;
    // Anything else, then it is not space_nl.
    return false;
}

size_t calculate_incidental_indent(const char *start, const char *stop)
{
    size_t indent = (size_t)-1; // Maximum indentation possible.
    const char *i = start;

    // Find first newline.
    while (i < stop && *i != 10) i++;

    // No newlines?
    if (i >= stop) return (size_t)-1; // No incidental indentation.

    for (;;)
    {
        // We have reached a newline character.
        assert(*i == '\n');
        i++;

        const char *line_start = i;
        // Look for content.
        while (i < stop && *i == ' ') i++;
        if (i >= stop) break;
        if (*i != 10 && *i != 13)
        {
            // We reached real content.
            size_t ind = i-line_start;
            if (ind < indent) indent = ind;
        }
        // Look for end of line.
        while (i < stop && *i != 10) i++;
        if (i >= stop)
        {
            break;
        }
    }

    return indent;
}

XMQReturnString xmq_trim_quote(const char *start, const char *stop, bool is_xmq, bool is_comment)
{
    size_t append_newlines = 0;
    size_t last_line_spaces = (size_t)-1;

    if (has_ending_nl_space(start, stop, NULL))
    {
        // So it is, now trim from the end.
        while (stop > start)
        {
            char c = *(stop-1);
            if (c == '\n') append_newlines++;
            if (c == ' ' && append_newlines == 0)
            {
                if (last_line_spaces == (size_t)-1) last_line_spaces = 0;
                last_line_spaces++;
            }
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            stop--;
        }
    }
    // If there is one or more ending newlines, then one is dropped, but keep the rest.
    if (append_newlines > 0) append_newlines--;

    if (stop == start)
    {
        // Oups! Quote was all space and newlines.
        char *buf = (char*)malloc(append_newlines+1);
        if (!buf) return (XMQReturnString){ XMQ_ERROR_OOM, NULL };
        size_t i;
        for (i = 0; i < append_newlines; ++i) buf[i] = '\n';
        buf[i] = 0;
        return (XMQReturnString){ XMQ_OK, buf };
    }

    // Set to false if quote starts with 'content\n...
    // Set to true if quote starts with '    \n....
    bool leads_space_nl = check_leading_space_nl(start, stop);
    // The leading space nl is used to determine the last lines incidental effect.
    // If we have foo = 'quote
    //                     again
    //                   again
    //                  '
    // Since the quotes are aligned, then we assume that the incidental is the last line+1
    // But if we have foo = '
    //                      quote
    //                        again
    //                      again
    //                      '
    // Then we assume that the incidental is the last line + 0.
    // This makes more sense when using 3 or more quotes, ie.
    //                foo = ''''
    //                      quote
    //                        again
    //                      again
    //                      ''''
    // and            foo = ''''quote
    //                            again
    //                          again
    //                      ''''

    size_t incidental = calculate_incidental_indent(start, stop);
    if (is_xmq && last_line_spaces < incidental)
    {
        incidental = last_line_spaces;
        if (!leads_space_nl)
        {
            incidental++;
            if (is_comment) incidental += 2;
        }
    }

    if (incidental == (size_t)-1)
    {
        // No newline was found, then do not trim, but re-add ending newlines.
        char *buf = (char*)malloc(stop-start+append_newlines+1);
        if (!buf) return (XMQReturnString){ XMQ_ERROR_OOM, NULL };
        memcpy(buf, start, stop-start);
        size_t i = stop-start;
        for (size_t j = 0; j < append_newlines; ++j) buf[i++] = '\n';
        buf[i++] = 0;
        return (XMQReturnString){ XMQ_OK, buf };
    }

    size_t prepend_newlines = 0;

    // For each found line, this is where the line ends after trimming line ending whitespace.
    const char *after_last_non_space;

    // This is where the newline char was found.
    const char *eol;

    size_t found_indent = 0;

    bool first_line = true;

    // Now find the first line.
    find_line(start, stop, &found_indent, &after_last_non_space, &eol);

    // Check if the first line is all spaces.
    if (has_leading_space_nl(start, stop, NULL))
    {
        // There is no first line (there was leading space/newline) thus we
        // start removing incidental indentation immediately below.
        first_line = false;
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
            else if (c != '\r') break;
            i++;
        }
    }

    // Allocate max size of output buffer, it usually becomes smaller
    // when incidental indentation and trailing whitespace is removed.
    size_t n = stop-start+prepend_newlines+append_newlines+1;
    char *output = (char*)malloc(n);
    char *o = output;

    // Insert any necessary prepended newlines.
    while (prepend_newlines) { *o++ = '\n'; prepend_newlines--; }

    // Start scanning the lines from beginning.
    // Now use the found incidental to copy the right parts.
    const char *i = start;

    while (i < stop)
    {
        bool has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (first_line)
        {
            first_line = false;
        }
        else
        {
            // Skip the incidental indentation, if there is indentation to be skipped.
            // Since empty lines do not count.
            if (*i == ' ')
            {
                size_t n = incidental;
                //assert(found_indent >= incidental);
                while (n > 0)
                {
                    char c = *i;
                    if (c != ' ') break;
                    i++;
                    n--;
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
    }

    // Insert any necessary appended newlines.
    while (append_newlines) { *o++ = '\n'; append_newlines--; }
    *o++ = 0;
    size_t real_size = o-output;
    output = (char*)realloc(output, real_size);
    if (!output) return (XMQReturnString){ XMQ_ERROR_OOM, NULL };
    return (XMQReturnString){ XMQ_OK, output };
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

#define X(TYPE) XMQStatus debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix) { \
    WRITE_ARGS("["#TYPE, NULL); \
    if (state->simulated) { WRITE_ARGS(" SIM", NULL); } \
    WRITE_ARGS(" \"", NULL); \
    XMQReturnString rs = xmq_quote_as_c(start, stop, false);   \
    if (rs.status != XMQ_OK) return rs.status; \
    char *tmp = rs.string; \
    WRITE_ARGS(tmp, NULL); \
    free(tmp); \
    WRITE_ARGS("\"", NULL); \
    char buf[32]; \
    snprintf(buf, 32, " %zu:%zu]", line, col); \
    buf[31] = 0; \
    WRITE_ARGS(buf, NULL); \
    return XMQ_OK; \
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

XMQStatus debug_content_value(XMQParseState *state,
                              size_t line,
                              size_t start_col,
                              const char *start,
                              const char *stop,
                              const char *suffix)
{
    XMQReturnString rs = xmq_quote_as_c(start, stop, false);
    if (rs.status != XMQ_OK) return rs.status;
    char *tmp = rs.string;
    WRITE_ARGS("{value \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    return XMQ_OK;
}


XMQStatus debug_content_quote(XMQParseState *state,
                              size_t line,
                              size_t start_col,
                              const char *start,
                              const char *stop,
                              const char *suffix)
{
    XMQReturnString rs = xmq_un_quote(start, stop, true, true);
    if (rs.status != XMQ_OK) return rs.status;
    char *trimmed = rs.string;
    rs = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed), false);
    if (rs.status != XMQ_OK)
    {
        free(trimmed);
        return rs.status;
    }
    char *tmp = rs.string;
    WRITE_ARGS("{quote \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
    return XMQ_OK;
}

XMQStatus debug_content_comment(XMQParseState *state,
                                size_t line,
                                size_t start_col,
                                const char *start,
                                const char *stop,
                                const char *suffix)
{
    XMQReturnString rs = xmq_un_comment(start, stop);
    if (rs.status != XMQ_OK) return rs.status;
    char *trimmed = rs.string;
    rs = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed), false);
    if (rs.status != XMQ_OK)
    {
        free(trimmed);
        return rs.status;
    }
    char *tmp = rs.string;
    WRITE_ARGS("{comment \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
    return XMQ_OK;
}

XMQStatus debug_content_comment_continuation(XMQParseState *state,
                                             size_t line,
                                             size_t start_col,
                                             const char *start,
                                             const char *stop,
                                             const char *suffix)
{
    XMQReturnString rs = xmq_un_comment(start, stop);
    if (rs.status != XMQ_OK) return rs.status;
    char *trimmed = rs.string;
    rs = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed), false);
    if (rs.status != XMQ_OK)
    {
        free(trimmed);
        return rs.status;
    }
    char *tmp = rs.string;
    WRITE_ARGS("{comment_continuation \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
    return XMQ_OK;
}

void xmqSetupParseCallbacksDebugContent(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->handle_element_value_text = debug_content_value;
    callbacks->handle_attr_value_text = debug_content_value;
    callbacks->handle_quote = debug_content_quote;
    callbacks->handle_comment = debug_content_comment;
    callbacks->handle_comment_continuation = debug_content_comment_continuation;
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

XMQReturnDoc xmqNewDoc()
{
    XMQDoc *d = (XMQDoc*)malloc(sizeof(XMQDoc));
    if (!d) return (XMQReturnDoc){ XMQ_ERROR_OOM, NULL };
    memset(d, 0, sizeof(XMQDoc));
    d->dom_ = xml_dom_new();
    return (XMQReturnDoc){ XMQ_OK, d };
}

XMQStatus xmqSetDocType(XMQDoc *doq, const char *name)
{
    XMQNode *dtd = xml_dom_set_dtd(doq->dom_, name, NULL, NULL);
    if (dtd) return XMQ_OK;
    return XMQ_ERROR_OOM;
}

const char *get_prefix_and_uri(const char *puri, char **out_prefix);

const char *get_prefix_and_uri(const char *puri, char **out_prefix)
{
    return xml_ns_split_puri(puri, out_prefix);
}

XMQReturnNode xmqAddRootElement(XMQDoc *doq, const char *name, XMQNS ns)
{
    if (!doq || !name) return (XMQReturnNode){ XMQ_ERROR_BAD_VALUE, NULL };

    XMQNode *new_node = xml_node_new_element(doq->dom_, name, NULL);
    xml_dom_set_root_element(doq->dom_, new_node);
    if (ns.action != XMQ_NS_PARENT) xml_node_apply_xmqns(new_node, ns);
    doq->root_ = new_node;
    return (XMQReturnNode){ XMQ_OK, new_node };
}

XMQStatus xmqAddNamespace(XMQDoc *doq, XMQNode *node, XMQNS ns)
{
    if (!doq || !node || ns.action != XMQ_NS_HERE) return XMQ_ERROR_BAD_VALUE;

    char *prefix = NULL;
    const char *uri = get_prefix_and_uri(ns.uri, &prefix);

    // The new namespace must have a prefix.
    if (!uri || !prefix)
    {
        free(prefix);
        return XMQ_ERROR_BAD_VALUE;
    }

    XMQStatus rc = XMQ_OK;
    XmlNs *found = xml_ns_lookup_prefix(node, prefix);
    if (!found || !xml_ns_href(found) || strcmp(xml_ns_href(found), uri))
    {
        if (!xml_ns_declare(node, prefix, uri)) rc = XMQ_ERROR_BAD_VALUE;
    }
    free(prefix);
    return rc;
}

XMQReturnNode xmqAddElement(XMQDoc *doq, XMQNode *parent, const char *name, XMQNS ns)
{
    if (!doq || !parent || !name) return (XMQReturnNode){ XMQ_ERROR_BAD_VALUE, NULL };
    // Default to place the new node in the same namespace as the parent node.
    // But this can be overridden below.
    XMQNode *new_node = xml_node_new_element(doq->dom_, name, xml_node_ns(parent));
    xml_node_append_child(parent, new_node);
    if (ns.action != XMQ_NS_PARENT) xml_node_apply_xmqns(new_node, ns);

    return (XMQReturnNode){ XMQ_OK, new_node };
}

XMQReturnNode xmqAddKeyValue(XMQDoc *doq, XMQNode *parent, const char *key, const char *value, XMQNS ns)
{
    XMQReturnNode rn = xmqAddElement(doq, parent, key, ns);
    if (rn.status != XMQ_OK) return rn;
    if (value && *value) xml_node_append_child(rn.node, xml_node_new_text(doq->dom_, value));
    return rn;
}

XMQStatus xmqAddText(XMQDoc *doq, XMQNode *parent, const char *text, size_t len)
{
    if (!doq || !parent || (!text && len > 0)) return XMQ_ERROR_BAD_VALUE;
    if (len == 0) return XMQ_OK;
    XMQNode *t = xml_node_new_text_len(doq->dom_, text, len);
    if (!t) return XMQ_ERROR_OOM;
    // Merges the text with a previous text node.
    xml_node_add_child(parent, t);
    return XMQ_OK;
}

bool xmqHasAttribute(XMQNode *node, const char *name)
{
    if (!node || !name || xml_node_kind(node) != XML_DOM_ELEMENT) return false;
    for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
    {
        // Like libxml2 xmlHasProp: the name matches regardless of namespace.
        if (!strcmp(xml_attr_name(a), name)) return true;
    }
    return false;
}

XMQContentMark xmqContentMark(XMQNode *node)
{
    XMQContentMark m = { NULL, 0, NULL };
    if (!node) return m;
    XMQNode *last = xml_node_last_child(node);
    m.child = last;
    if (last && xml_node_kind(last) == XML_DOM_TEXT && xml_node_content(last))
    {
        m.text_len = strlen(xml_node_content(last));
    }
    XMQAttr *a = xml_node_first_attr(node);
    while (a && xml_attr_next(a)) a = xml_attr_next(a);
    m.attr = a;
    return m;
}

XMQReturnNode xmqWrapContent(XMQDoc *doq, XMQNode *node, XMQContentMark mark, const char *name)
{
    XMQNode *n = node;
    if (!doq || !n || !name) return (XMQReturnNode){ XMQ_ERROR_BAD_VALUE, NULL };
    XMQNode *w = xml_node_new_element(doq->dom_, name, xml_node_ns(n));
    if (!w) return (XMQReturnNode){ XMQ_ERROR_OOM, NULL };
    XMQNode *mc = (XMQNode*)mark.child;
    XMQNode *c = mc ? xml_node_next(mc) : xml_node_first_child(n);
    if (mc && xml_node_kind(mc) == XML_DOM_TEXT && xml_node_content(mc))
    {
        // Text added after the mark may have been merged into the marked text node, split it.
        const char *content = xml_node_content(mc);
        size_t len = strlen(content);
        if (len > mark.text_len)
        {
            XMQNode *t = xml_node_new_text_len(doq->dom_, content + mark.text_len, len - mark.text_len);
            char *prefix = strndup(content, mark.text_len);
            xml_node_set_content(mc, prefix);
            free(prefix);
            xml_node_append_child(w, t);
        }
    }
    while (c)
    {
        XMQNode *next = xml_node_next(c);
        xml_node_add_child(w, c);
        c = next;
    }
    XMQAttr *a = mark.attr ? xml_attr_next((XMQAttr*)mark.attr) : xml_node_first_attr(n);
    while (a)
    {
        XMQAttr *next = xml_attr_next(a);
        xml_node_move_attr(w, a);
        a = next;
    }
    xml_node_add_child(n, w);
    return (XMQReturnNode){ XMQ_OK, w };
}

XMQStatus xmq_add_attrs(XMQDoc *doc, XMQNode *node, const XMQAddAttr *attrs, size_t num_attrs);
XMQStatus xmq_add_attrs(XMQDoc *doc, XMQNode *node, const XMQAddAttr *attrs, size_t num_attrs)
{
    for (size_t i = 0; i < num_attrs; ++i)
    {
        const XMQAddAttr *aa = attrs+i;
        const char *name = aa->name;
        if (!name) break;
        const char *value = aa->value;
        if (!value) value = "";
        XMQReturnAttr ra = xmqSetAttribute(doc, node, name, value, NS_NONE);
        if (ra.status != XMQ_OK) break;
    }
    return XMQ_OK;
}

XMQReturnNode xmqAddKeyValueWithAttrs(XMQDoc *doq,
                                      XMQNode *parent,
                                      const char *key,
                                      const char *value,
                                      XMQNS ns,
                                      XMQAttrList attrs)
{
    XMQReturnNode rn = xmqAddKeyValue(doq, parent, key, value, ns);
    if (rn.status == XMQ_OK)
    {
        xmq_add_attrs(doq, rn.node, attrs.array, attrs.count);
    }

    return rn;
}

XMQReturnNode xmqAddElementWithAttrs(XMQDoc *doq,
                                     XMQNode *parent,
                                     const char *name,
                                     XMQNS ns,
                                     XMQAttrList attrs)
{
    XMQReturnNode rn = xmqAddElement(doq, parent, name, ns);
    if (rn.status == XMQ_OK)
    {
        xmq_add_attrs(doq, rn.node, attrs.array, attrs.count);
    }

    return rn;
}

/*
 * Move element e under parent p.
 *
 * position:
 *   XMQ_MOVE_FIRST   - make e the first child of p
 *   XMQ_MOVE_LAST    - make e the last child of p
 *   XMQ_MOVE_BEFORE  - insert e immediately before sibling
 *   XMQ_MOVE_AFTER   - insert e immediately after sibling
 *
 * sibling is ignored for FIRST/LAST and must be a child of p
 * for BEFORE/AFTER.
 *
 * Returns 0 on success, -1 on invalid input/failure.
 */
int move_element(XmlDom *d, XMQNode *e, XMQNode *p, XMQMoveTo position, XMQNode *sibling)
{
    if (d == NULL || e == NULL || p == NULL) return -1;
    if (xml_node_dom(e) != d || xml_node_dom(p) != d) return -1;
    if (e == p) return -1;

    // Moving p underneath one of its descendants would create a cycle.
    for (XMQNode *n = p; n != NULL; n = xml_node_parent(n))
    {
        if (n == e) return -1;
    }

    switch (position)
    {
    case XMQ_MOVE_FIRST:
    case XMQ_MOVE_LAST:
        break;
    case XMQ_MOVE_BEFORE:
    case XMQ_MOVE_AFTER:
        if (sibling == NULL || sibling == e || xml_node_dom(sibling) != d || xml_node_parent(sibling) != p) return -1;
        break;
    default:
        return -1;
    }

    xml_node_unlink(e);

    switch (position) {
    case XMQ_MOVE_FIRST:
        if (xml_node_first_child(p)) xml_node_add_prev_sibling(xml_node_first_child(p), e);
        else xml_node_add_child(p, e);
        break;
    case XMQ_MOVE_LAST:
        xml_node_add_child(p, e);
        break;
    case XMQ_MOVE_BEFORE:
        xml_node_add_prev_sibling(sibling, e);
        break;
    case XMQ_MOVE_AFTER:
        xml_node_add_next_sibling(sibling, e);
        break;
    default:
        return -1;
    }
    xml_node_reconcile_ns(e);
    return 0;
}

bool xmqMoveElement(XMQDoc *from_doq,
                    XMQNode *from_parent,
                    XMQNode *element,
                    XMQDoc *to_doq,
                    XMQNode *to_parent,
                    XMQMoveTo where,
                    XMQNode *sibling)
{
    if (from_doq == to_doq)
    {
        int rc = move_element(from_doq->dom_, element, to_parent, where, sibling);
        return (rc != 0);
    }
    assert(false);
    return false;
}

XMQReturnAttr xmqSetAttribute(XMQDoc *doq, XMQNode *node, const char *name, const char *value, XMQNS ns)
{
    // NULLs not accepted.
    if (!doq || !node || !name || !value) return (XMQReturnAttr){ XMQ_ERROR_BAD_VALUE, NULL };
    return xml_node_set_attr_xmqns(node, name, value, ns);
}

void *xmqGetImplementationDoc(XMQDoc *doq)
{
    return doq->dom_;
}

void xmqSetImplementationDoc(XMQDoc *doq, void *doc)
{
    doq->dom_ = (XmlDom*)doc;
    doq->root_ = doq->dom_ ? xml_dom_root_element(doq->dom_) : NULL;
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

void xmqSetOriginalSize(XMQDoc *doq, size_t size)
{
    doq->original_size_ = size;
}

XMQNode *xmqGetRootNode(XMQDoc *doq)
{
    return doq->root_;
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

    if (state->yaep_tmp_rhs_) free(state->yaep_tmp_rhs_);
    state->yaep_tmp_rhs_ = NULL;
    if (state->yaep_tmp_transl_) free(state->yaep_tmp_transl_);
    state->yaep_tmp_transl_ = NULL;
    if (state->yaep_tmp_marks_) free(state->yaep_tmp_marks_);
    state->yaep_tmp_marks_ = NULL;
    if (state->yaep_tmp_aliases_) free(state->yaep_tmp_aliases_);
    state->yaep_tmp_aliases_ = NULL;

    if (state->ixml_rules) vector_free_and_values(state->ixml_rules, (FreeFuncPtr)free_ixml_rule);
    state->ixml_rules = NULL;

    if (state->ixml_terminals_map) hashmap_free_and_values(state->ixml_terminals_map, (FreeFuncPtr)free_ixml_terminal);
    state->ixml_terminals_map = NULL;

    if (state->ixml_non_terminals_map) hashmap_free(state->ixml_non_terminals_map);
    state->ixml_non_terminals_map = NULL;

    if (state->ixml_non_terminals) vector_free_and_values(state->ixml_non_terminals, (FreeFuncPtr)free_ixml_nonterminal);
    state->ixml_non_terminals = NULL;

    if (state->ixml_rule_stack) stack_free(state->ixml_rule_stack);
    state->ixml_rule_stack = NULL;

    if (state->used_unicodes)
    {
        free(state->used_unicodes);
        state->used_unicodes = NULL;
    }
    free(state);
}

void xmqClearDoc(XMQDoc *doq)
{
    if (!doq) return;
    if (doq->error_)
    {
        debug("xmq=", "freeing error message");
        free((void*)doq->error_);
        doq->error_ = NULL;
    }
    if (doq->dom_)
    {
        debug("xmq=", "freeing xml doc");
        xml_dom_free(doq->dom_);
        doq->dom_ = xml_dom_new();
        doq->root_ = NULL;
    }
    if (doq->yaep_grammar_)
    {
        yaepFreeGrammar (doq->yaep_parse_run_, doq->yaep_grammar_);
        yaepFreeParseRun (doq->yaep_parse_run_);
        xmqFreeParseState(doq->xmq_parse_state_);
        doq->yaep_grammar_ = NULL;
        doq->yaep_parse_run_ = NULL;
        doq->xmq_parse_state_ = NULL;
    }
    debug("xmq=", "clearing xmq doc");
}

void xmqFreeDoc(XMQDoc *doq)
{
    if (!doq) return;
    if (doq->source_name_)
    {
        debug("xmq=", "freeing source name");
        free((void*)doq->source_name_);
        doq->source_name_ = NULL;
    }
    if (doq->error_)
    {
        debug("xmq=", "freeing error message");
        free((void*)doq->error_);
        doq->error_ = NULL;
    }
    if (doq->dom_)
    {
        debug("xmq=", "freeing xml doc");
        xml_dom_free(doq->dom_);
        doq->dom_ = NULL;
    }
    if (doq->yaep_grammar_)
    {
        yaepFreeGrammar (doq->yaep_parse_run_, doq->yaep_grammar_);
        yaepFreeParseRun (doq->yaep_parse_run_);
        xmqFreeParseState(doq->xmq_parse_state_);
        doq->yaep_grammar_ = NULL;
        doq->yaep_parse_run_ = NULL;
        doq->xmq_parse_state_ = NULL;
    }

    debug("xmq=", "freeing xmq doc");
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

    stack_push(state->element_stack, xml_dom_document(doq->dom_));
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
        debug("xmq=", "read %zu bytes total %zu", r, n);
        if (!r) break;
        n += r;
    } while (n < fsize);

    debug("xmq=", "read total %zu bytes", n);

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

XMQStatus do_whitespace(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    return XMQ_OK;
}

XMQReturnXMLNode create_quote(XMQParseState *state,
                              size_t l,
                              size_t col,
                              const char *start,
                              const char *stop,
                              const char *suffix,
                              XMQNode* parent)
{
    char *trimmed = NULL;
    if (state->no_trim_quotes)
    {
        trimmed = strndup(start, stop-start);
        if (!trimmed) return (XMQReturnXMLNode){ XMQ_ERROR_OOM, NULL };
    }
    else
    {
        XMQReturnString rs = xmq_un_quote(start, stop, true, true);
        if (rs.status != XMQ_OK) return (XMQReturnXMLNode){ rs.status, NULL };
        trimmed = rs.string;
    }
    XMQNode *n = xml_node_new_text(state->doq->dom_, trimmed);
    if (state->merge_text)
    {
        n = xml_node_add_child(parent, n);
    }
    else
    {
        // Prevent merging of this new text node with previous text nodes.
        xml_node_append_child(parent, n);
    }
    free(trimmed);
    return (XMQReturnXMLNode){ XMQ_OK, n };
}

XMQStatus do_quote(XMQParseState *state,
                   size_t l,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    XMQReturnXMLNode rn = create_quote(state, l, col, start, stop, suffix,
                                       (XMQNode*)state->element_stack->top->data);
    if (rn.status != XMQ_OK) return rn.status;
    state->element_last = rn.node;
    return XMQ_OK;
}

XMQNode* create_entity(XMQParseState *state,
                         size_t l,
                         size_t c,
                         const char *start,
                         const char *stop,
                         const char *suffix,
                         XMQNode* parent)
{
    char *tmp = strndup(start, stop-start);
    XMQNode* n = NULL;
    if (tmp[1] == '#')
    {
        // Character entity.
        if (!state->merge_text)
        {
            // Do not merge with surrounding text.
            tmp[strlen(tmp)-1] = 0;
            n = xml_node_new_entity_ref(state->doq->dom_, tmp+1);
        }
        else
        {
            // Make inte text that will be merged.
            UTF8Char uni;
            long uc = 0;
            if (tmp[2] == 'x') uc = strtol(tmp+3, NULL, 16);
            else uc = strtol(tmp+2, NULL, 10);
            // Reject character entities outside the valid Unicode range, ie #2000000. Otherwise encode_utf8 would abort on an assert.
            if (uc < 0 || uc > 0x10ffff)
            {
                free(tmp);
                state->error_nr = XMQ_ERROR_INVALID_CHAR;
                longjmp(state->error_handler, 1);
            }
            size_t len = encode_utf8(uc, &uni);
            char buf[len+1];
            memcpy(buf, uni.bytes, len);
            buf[len] = 0;
            n = xml_node_new_text(state->doq->dom_, buf);
        }
    }
    else
    {
        // Named references are kept as is.
        tmp[strlen(tmp)-1] = 0;
        n = xml_node_new_entity_ref(state->doq->dom_, tmp+1);
    }
    if (xml_node_kind(n) == XML_DOM_TEXT) n = xml_node_add_child(parent, n);
    else xml_node_append_child(parent, n);
    free(tmp);

    return n;
}

XMQStatus do_entity(XMQParseState *state,
               size_t l,
               size_t c,
               const char *start,
               const char *stop,
               const char *suffix)
{
    state->element_last = create_entity(state, l, c, start, stop, suffix, (XMQNode*)state->element_stack->top->data);
    return XMQ_OK;
}

XMQStatus do_comment(XMQParseState*state,
                size_t line,
                size_t col,
                const char *start,
                const char *stop,
                const char *suffix)
{
    XMQNode* parent = (XMQNode*)state->element_stack->top->data;
    char *trimmed = NULL;
    if (state->no_trim_quotes)
    {
        trimmed = strndup(start, stop-start);
        if (!trimmed) return XMQ_ERROR_OOM;
    }
    else
    {
        XMQReturnString rs = xmq_un_comment(start, stop);
        if (rs.status != XMQ_OK) return XMQ_ERROR_OOM;
        trimmed = rs.string;
    }
    XMQNode *n = xml_node_new_comment(state->doq->dom_, trimmed);
    if (state->add_pre_node_before)
    {
        // Insert comment before this node.
        xml_node_add_prev_sibling((XMQNode*)state->add_pre_node_before, n);
    }
    else if (state->add_post_node_after)
    {
        // Insert comment after this node.
        xml_node_add_next_sibling((XMQNode*)state->add_post_node_after, n);
    }
    else
    {
        xml_node_add_child(parent, n);
    }
    state->element_last = n;
    free(trimmed);
    return XMQ_OK;
}

XMQStatus do_comment_continuation(XMQParseState*state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
{
    XMQNode* last = (XMQNode*)state->element_last;
    // We have ///* alfa *///* beta *///* gamma *///
    // and this function is invoked with "* beta *///"
    const char *i = stop-1;
    // Count n slashes from the end
    size_t n = 0;
    while (i > start && *i == '/') { n++; i--; }
    // Since we know that we are invoked pointing into a buffer with /// before start, we
    // can safely do start-n.
    XMQReturnString rs = xmq_un_comment(start-n, stop);
    if (rs.status != XMQ_OK) return rs.status;
    char *trimmed = rs.string;
    size_t l = strlen(trimmed);
    char *tmp = (char*)malloc(l+2);
    if (!tmp)
    {
        free(trimmed);
        return XMQ_OK;
    }
    tmp[0] = '\n';
    memcpy(tmp+1, trimmed, l);
    tmp[l+1] = 0;
    xml_node_append_content(last, tmp, l+1);
    free(trimmed);
    free(tmp);
    return XMQ_OK;
}

XMQStatus do_element_value_text(XMQParseState *state,
                           size_t line,
                           size_t col,
                           const char *start,
                           const char *stop,
                           const char *suffix)
{
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(start, stop);
        XMQNode *n = xml_node_new_pi(state->doq->dom_, state->pi_name, content);
        XMQNode *parent = (XMQNode*)state->element_stack->top->data;
        xml_node_add_child(parent, n);
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
        XMQNode *n = xml_dom_set_dtd(state->doq->dom_, tmp, NULL, NULL);
        XMQNode *parent = (XMQNode*)state->element_stack->top->data;
        xml_node_add_child(parent, n);
        free(tmp);

        state->parsing_doctype = false;
        state->doctype_found = true;
    }
    else
    {
        XMQNode *n = xml_node_new_text_len(state->doq->dom_, start, stop-start);
        xml_node_add_child((XMQNode*)state->element_last, n);
    }
    return XMQ_OK;
}

XMQStatus do_element_value_quote(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            const char *stop,
                            const char *suffix)
{
    char *trimmed = NULL;
    if (state->no_trim_quotes)
    {
        trimmed = strndup(start, stop-start);
        if (!trimmed) return XMQ_ERROR_OOM;
    }
    else
    {
        XMQReturnString rs = xmq_un_quote(start, stop, true, true);
        if (rs.status != XMQ_OK) return rs.status;
        trimmed = rs.string;
    }
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(trimmed, trimmed+strlen(trimmed));
        XMQNode *n = xml_node_new_pi(state->doq->dom_, state->pi_name, content);
        XMQNode *parent = (XMQNode*)state->element_stack->top->data;
        xml_node_add_child(parent, n);
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
        XMQNode* dtd = parse_doctype_raw(state->doq, buf, buf+n-1);
        if (!dtd)
        {
            state->error_nr = XMQ_ERROR_BAD_DOCTYPE;
            longjmp(state->error_handler, 1);
        }
        XMQNode *old = xml_dom_dtd(state->doq->dom_);
        if (old) xml_node_free(old);
        if (state->add_pre_node_before)
        {
            // Insert doctype before this node.
            xml_node_add_prev_sibling((XMQNode*)state->add_pre_node_before, dtd);
        }
        else
        {
            // Append doctype to document.
            XMQNode *parent = (XMQNode*)state->element_stack->top->data;
            xml_node_add_child(parent, dtd);
        }
        state->parsing_doctype = false;
        state->doctype_found = true;
        free(buf);
    }
    else
    {
        XMQNode *n = xml_node_new_text(state->doq->dom_, trimmed);
        xml_node_add_child((XMQNode*)state->element_last, n);
    }
    free(trimmed);
    return XMQ_OK;
}

XMQStatus do_element_value_entity(XMQParseState *state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (XMQNode*)state->element_last);
    return XMQ_OK;
}

XMQStatus do_element_value_compound_quote(XMQParseState *state,
                                     size_t line,
                                     size_t col,
                                     const char *start,
                                     const char *stop,
                                     const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
    return XMQ_OK;
}

XMQStatus do_element_value_compound_entity(XMQParseState *state,
                                      size_t line,
                                      size_t col,
                                      const char *start,
                                      const char *stop,
                                      const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
    return XMQ_OK;
}

XMQStatus do_attr_ns(XMQParseState *state,
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
    return XMQ_OK;
}

XMQStatus do_ns_declaration(XMQParseState *state,
                       size_t line,
                       size_t col,
                       const char *start,
                       const char *stop,
                       const char *suffix)
{
    // We found a namespace declaration attribute. It is either a default declaration xmlns=... or xmlns:prefix=...
    //
    // We can see the difference here since the parser will invoke with suffix
    // either pointing to stop (xmlns=) or after stop (xmlns:foo=)
    XMQNode* element = (XMQNode*)state->element_stack->top->data;
    if (!element)
    {
        // There is no element node, eg a processing instruction element
        // like ?root( ... ) has no node to attach a namespace to.
        // Declare nothing; the value handlers will just eat the uri.
        state->declaring_xmlns = true;
        state->declaring_xmlns_namespace = NULL;
        return XMQ_OK;
    }
    XmlNs* ns = NULL;
    if (stop == suffix)
    {
        // Stop is the same as suffix, so no prefix has been added.
        // I.e. this is a default namespace, eg: xmlns=uri
        if (xml_node_kind(element) == XML_DOM_ELEMENT)
        {
            ns = xml_ns_declare(element, NULL, NULL);
            debug("xmq=", "create default namespace in element %s", xml_node_name(element));
            if (!ns)
            {
                // Oups, this element already has a default namespace.
                // This is probably due to multiple xmlns=uri declarations. Is this an error or not?
                ns = xml_ns_lookup_prefix(element, NULL);
            }
            // An element without prefix has the inherited default namespace (or none),
            // the default namespace declared on the element itself replaces it.
            if (ns && (xml_node_ns(element) == NULL || xml_ns_prefix(xml_node_ns(element)) == NULL))
            {
                debug("xmq=", "set default namespace in element %s prefix=%s href=%s", xml_node_name(element), xml_ns_prefix(ns), xml_ns_href(ns));
                xml_node_set_ns(element, ns);
            }
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

        ns = find_ns(element, name);
        if (!ns && !strcmp(name, "xml")) ns = xml_ns_lookup_prefix(element, name);

        if (ns)
        {
            // Oups, this exact namespace has already been created, for example due to the namespace prefix
            // of the element itself, eg: abc:element(xmlns:abc = uri)
            // or from an attribute elment(gurka:alfa = 123 xmlns:gurka = uri)
            // Lets pick this ns up and reuse it.
            debug("xmq=", "found existing element namespace for xmlns:%s", name);
        }
        else
        {
            // Not used before, create a new namespace.
            if (xml_node_kind(element) == XML_DOM_ELEMENT) ns = xml_ns_declare(element, name, NULL);
            debug("xmq=", "created new namespace declaration xmlns:%s", name);
        }
        free(name);
    }

    if (!ns)
    {
        // The element is a node that can not hold a namespace declaration,
        // eg an entity reference. There is nothing to attach the namespace
        // to, so just have the value handlers eat the uri without error.
        state->declaring_xmlns = true;
        state->declaring_xmlns_namespace = NULL;
        return XMQ_OK;
    }
    state->declaring_xmlns = true;
    state->declaring_xmlns_namespace = ns;
    return XMQ_OK;
}

XmlNs* find_ns(XMQNode* node, const char *prefix)
{
    for (XMQNode* cur = node; cur; cur = xml_node_parent(cur))
    {
        if (xml_node_kind(cur) != XML_DOM_ELEMENT) continue;
        for (XmlNs *ns = xml_node_first_ns_decl(cur); ns; ns = xml_ns_next_decl(ns))
        {
            if ((prefix == NULL && xml_ns_prefix(ns) == NULL) ||
                (prefix && xml_ns_prefix(ns) && !strcmp(prefix, xml_ns_prefix(ns))))
            {
                return ns;
            }
        }
    }
    return NULL;
}

XMQStatus do_attr_key(XMQParseState *state,
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

    XMQNode* parent = (XMQNode*)state->element_stack->top->data;
    if (!parent)
    {
        // There is no element node, eg a processing instruction element
        // like ?root( ... ) has no node. Use the document node instead.
        parent = xml_dom_document(state->doq->dom_);
    }
    XMQAttr* attr = NULL;

    if (xml_node_kind(parent) != XML_DOM_ELEMENT)
    {
        // Attributes can only be attached to elements, the value is dropped.
        free(state->attribute_namespace);
        state->attribute_namespace = NULL;
    }
    else if (!state->attribute_namespace)
    {
        // A normal attribute with no namespace.
        attr = xml_node_append_attr(parent, key, NULL, NULL);
    }
    else
    {
        XmlNs *ns = find_ns(parent, state->attribute_namespace);
        if (!ns && !strcmp(state->attribute_namespace, "xml")) ns = xml_ns_lookup_prefix(parent, "xml");

        if (ns)
        {
            debug("xmq=", "found existing namespace for attribute %s:%s inside %s", state->attribute_namespace, key, xml_node_name(parent));
        }
        else
        {
            // The namespaces does not yet exist. Lets create it.. Lets hope it will be declared
            // inside the attributes of this node. Use a temporary href for now.
            ns = xml_ns_declare(parent, state->attribute_namespace, NULL);
            debug("xmq=", "created new namespace for attribute %s:%s inside %s", state->attribute_namespace, key, xml_node_name(parent));
        }
        attr = xml_node_append_attr(parent, key, ns, NULL);
        free(state->attribute_namespace);
        state->attribute_namespace = NULL;
    }

    // The new attribute attr should now be added to the parent elements: properties list.
    // Remember this attr as the last element so that we can set the value.
    state->element_last = attr ? xml_attr_node(attr) : NULL;

    free(key);
    return XMQ_OK;
}

void update_namespace_href(XMQParseState *state,
                           XmlNs* ns,
                           const char *start,
                           const char *stop)
{
    if (!stop) stop = start+strlen(start);

    char *href = strndup(start, stop-start);
    xml_ns_set_href(ns, href);
    free(href);
    debug("xmq=", "update namespace %s with href=%s", xml_ns_prefix(ns), xml_ns_href(ns));

    if (start[0] == 0 && ns == state->default_namespace)
    {
        XMQNode* element = (XMQNode*)state->element_stack->top->data;
        if (element)
        {
            debug("xmq=", "remove default namespace in element %s", xml_node_name(element));
            if (xml_node_ns(element) == ns) xml_node_set_ns(element, NULL);
        }
        state->default_namespace = NULL;
        return;
    }
}

XMQStatus do_attr_value_text(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        const char *stop,
                        const char *suffix)
{
    if (state->declaring_xmlns)
    {
        // Note: declaring_xmlns_namespace may be NULL if the declaration
        // happened on an element without a node, eg a processing instruction.
        if (state->declaring_xmlns_namespace != NULL)
        {
            update_namespace_href(state, (XmlNs*)state->declaring_xmlns_namespace, start, stop);
        }
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        return XMQ_OK;
    }
    if (!state->element_last) return XMQ_OK;
    XMQNode *n = xml_node_new_text_len(state->doq->dom_, start, stop-start);
    xml_node_add_child((XMQNode*)state->element_last, n);
    return XMQ_OK;
}

XMQStatus do_attr_value_quote(XMQParseState*state,
                         size_t line,
                         size_t col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
{
    if (state->declaring_xmlns)
    {
        char *trimmed = NULL;
        if (state->no_trim_quotes)
        {
            trimmed = strndup(start, stop-start);
            if (!trimmed) return XMQ_ERROR_OOM;
        }
        else
        {
            XMQReturnString rs = xmq_un_quote(start, stop, true, true);
            if (rs.status != XMQ_OK) return rs.status;
            trimmed = rs.string;
        }
        // Note: declaring_xmlns_namespace may be NULL if the declaration
        // happened on an element without a node, eg a processing instruction.
        if (state->declaring_xmlns_namespace != NULL)
        {
            update_namespace_href(state, (XmlNs*)state->declaring_xmlns_namespace, trimmed, NULL);
        }
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        free(trimmed);
        return XMQ_OK;
    }
    XMQReturnXMLNode rn = create_quote(state, line, col, start, stop, suffix, (XMQNode*)state->element_last);
    return rn.status;
}

XMQStatus do_attr_value_entity(XMQParseState *state,
                          size_t line,
                          size_t col,
                          const char *start,
                          const char *stop,
                          const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (XMQNode*)state->element_last);
    return XMQ_OK;
}

XMQStatus do_attr_value_compound_quote(XMQParseState *state,
                                  size_t line,
                                  size_t col,
                                  const char *start,
                                  const char *stop,
                                  const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
    return XMQ_OK;
}

XMQStatus do_attr_value_compound_entity(XMQParseState *state,
                                             size_t line,
                                             size_t col,
                                             const char *start,
                                             const char *stop,
                                             const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
    return XMQ_OK;
}

XMQStatus create_node(XMQParseState *state, const char *start, const char *stop)
{
    size_t len = stop-start;
    char *name = strndup(start, len);
    if (!name) return XMQ_ERROR_OOM;

    if (!strcmp(name, "!DOCTYPE"))
    {
        state->parsing_doctype = true;
    }
    else if (name[0] == '?')
    {
        state->parsing_pi = true;
        state->pi_name = strdup(name+1); // Drop the ?
        if (!state->pi_name)
        {
            free(name);
            return XMQ_ERROR_OOM;
        }
    }
    else
    {
        XMQNode *new_node = xml_node_new_element(state->doq->dom_, name, NULL);
        if (state->element_last == NULL)
        {
            if (!state->implicit_root || !strcmp(name, state->implicit_root))
            {
                // There is no implicit root, or name is the same as the implicit root.
                // Then create the root node with name.
                state->element_last = new_node;
                xml_dom_set_root_element(state->doq->dom_, new_node);
                state->doq->root_ = new_node;
            }
            else
            {
                // We have an implicit root and it is different from name.
                XMQNode *root = xml_node_new_element(state->doq->dom_, state->implicit_root, NULL);
                state->element_last = root;
                xml_dom_set_root_element(state->doq->dom_, root);
                state->doq->root_ = root;
                stack_push(state->element_stack, state->element_last);
            }
        }
        XMQNode *parent = (XMQNode*)state->element_stack->top->data;
        xml_node_add_child(parent, new_node);

        if (state->element_namespace)
        {
            // Have a namespace before the element name, eg abc:work
            XmlNs *ns = find_ns(new_node, state->element_namespace);
            if (!ns && !strcmp(state->element_namespace, "xml")) ns = xml_ns_lookup_prefix(new_node, "xml");
            if (!ns)
            {
                // The namespaces does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = xml_ns_declare(new_node, state->element_namespace, NULL);
                debug("xmq=", "created namespace prefix=%s in element %s", state->element_namespace, name);
            }
            debug("xmq=", "setting namespace prefix=%s for element %s", state->element_namespace, name);
            xml_node_set_ns(new_node, ns);
            free(state->element_namespace);
            state->element_namespace = NULL;
        }
        else if (state->default_namespace)
        {
            // We have a default namespace.
            XmlNs* ns = (XmlNs*)state->default_namespace;
            assert(xml_ns_prefix(ns) == NULL);
            debug("xmq=", "set default namespace with href=%s for element %s", xml_ns_href(ns), name);
            xml_node_set_ns(new_node, ns);
        }

        state->element_last = new_node;
    }

    free(name);
    return XMQ_OK;
}

XMQStatus do_element_ns(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    char *ns = strndup(start, stop-start);
    state->element_namespace = ns;
    return XMQ_OK;
}

XMQStatus do_ns_colon(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    return XMQ_OK;
}

XMQStatus do_element_name(XMQParseState *state,
                     size_t line,
                     size_t col,
                     const char *start,
                     const char *stop,
                     const char *suffix)
{
    create_node(state, start, stop);
    return XMQ_OK;
}

XMQStatus do_element_key(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    return create_node(state, start, stop);
}

XMQStatus do_equals(XMQParseState *state,
               size_t line,
               size_t col,
               const char *start,
               const char *stop,
               const char *suffix)
{
    return XMQ_OK;
}

XMQStatus do_brace_left(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
    return XMQ_OK;
}

XMQStatus do_brace_right(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
    return XMQ_OK;
}

XMQStatus do_apar_left(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
    return XMQ_OK;
}

XMQStatus do_apar_right(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
    return XMQ_OK;
}

XMQStatus do_cpar_left(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    stack_push(state->element_stack, state->element_last);
    return XMQ_OK;
}

XMQStatus do_cpar_right(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        const char *stop,
                        const char *suffix)
{
    state->element_last = stack_pop(state->element_stack);
    return XMQ_OK;
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
    qs->allow_json_quotes = os->allow_json_quotes;
}

void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);

    size_t size = 0;
    char *buffer = xml_save_doc(doq->dom_, "utf-8", false, &size);

    membuffer_reuse(output_settings->output_buffer,
                    buffer,
                    size);

    debug("xmq=", "xmq_print_xml wrote %zu bytes", size);
}

void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);
    char *buffer = xml_html_save_doc(doq->dom_, true, NULL);
    MemBuffer *membuf = output_settings->output_buffer;
    membuffer_append(membuf, buffer);
    free(buffer);
    debug("xmq=", "xmq_print_html wrote %zu bytes", membuf->used_);
}

void xmq_print_json(XMQDoc *doq, XMQOutputSettings *os)
{
    if (doq == NULL || doq->dom_ == NULL) return;
    xmq_fixup_json_before_writeout(doq);

    void *first = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !first) return;
    void *last = xml_node_last_child(xml_dom_document(doq->dom_));

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
    collect_leading_ending_comments_doctype(&ps, (XMQNode**)&first, (XMQNode**)&last);
    json_print_object_nodes(&ps, NULL, (XMQNode*)first, (XMQNode*)last);
    if (os->final_newline) write(writer_state, "\n", NULL);

    stack_free(ps.pre_nodes);
    stack_free(ps.post_nodes);
}

void text_print_node(XMQPrintState *ps, XMQNode *node)
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
        text_print_nodes(ps, xml_node_first_child(node));
    }
}

void text_print_nodes(XMQPrintState *ps, XMQNode *from)
{
    XMQNode *i = from;

    while (i)
    {
        text_print_node(ps, i);
        i = xml_next_sibling(i);
    }
}

void xmq_print_text(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !first) return;

    XMQPrintState ps = {};
    ps.doq = doq;
    ps.output_settings = os;

    text_print_nodes(&ps, (XMQNode*)first);
}

void cline_print_xpath(XMQPrintState *ps, XMQNode *node)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    if (!node || xml_node_kind(node) == XML_DOM_DOCUMENT) return;
    if (xml_node_parent(node))
    {
        cline_print_xpath(ps, xml_node_parent(node));
        write(writer_state, "/", NULL);
    }
    write(writer_state, xml_element_name(node), NULL);
}

void cline_print_node(XMQPrintState *ps, XMQNode *node)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    // Skip the dtd and entity references: the children of an entity reference
    // is its declaration, which can refer back to it (infinite recursion).
    if (xml_node_kind(node) == XML_DOM_DTD || is_entity_node(node)) return;

    if (has_attributes(node))
    {
        cline_print_attributes(ps, node);
    }
    if (is_content_node(node))
    {
        cline_print_xpath(ps, xml_node_parent(node));
        write(writer_state, "=", NULL);
        const char *content = xml_element_content(node);
        XMQReturnString rs = xmq_quote_as_c(content, NULL, false);
        assert(rs.status == XMQ_OK);
        char *q = rs.string;
        write(writer_state, "\"", NULL);
        write(writer_state, q, NULL);
        write(writer_state, "\"", NULL);
        free(q);
        write(writer_state, "\n", NULL);
    }
    else if (xml_node_first_child(node))
    {
        cline_print_nodes(ps, xml_node_first_child(node));
    }
    else if (is_element_node(node))
    {
        cline_print_xpath(ps, node);
        write(writer_state, "=\"\"\n", NULL);
    }
    else if (is_entity_node(node))
    {
    }
}

void cline_print_nodes(XMQPrintState *ps, XMQNode *from)
{
    XMQNode *i = from;

    while (i)
    {
        cline_print_node(ps, i);
        i = xml_next_sibling(i);
    }
}

void cline_print_attributes(XMQPrintState *ps, XMQNode *node)
{
    XMQAttr *i = xml_first_attribute(node);

    while (i)
    {
        cline_print_attr(ps, i);
        i = xml_next_attribute(i);
    }
}

void cline_print_attr(XMQPrintState *ps, XMQAttr *a)
{
    cline_print_xpath(ps, xml_attr_owner(a));

    const char *key;
    const char *prefix;
    size_t total_u_len;
    attr_strlen_name_prefix(a, &key, &prefix, &total_u_len);

    print_utf8(ps, COLOR_none, 1, "/@", NULL);
    if (prefix)
    {
        print_utf8(ps, COLOR_none, 2, prefix, NULL, ":", NULL);
    }
    print_utf8(ps, COLOR_none, 2, key, NULL, "=", NULL);

    if (xml_node_first_child(xml_attr_node(a)) != NULL)
    {
        char *value = xml_node_string_value(xml_attr_node(a));
        XMQReturnString rs = xmq_quote_as_c(value, value+strlen(value), true);
        assert(rs.status == XMQ_OK);
        char *quoted_value = rs.string;
        print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
        free(quoted_value);
        free(value);
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "\"\"", NULL);
    }

    print_utf8(ps, COLOR_none, 1, "\n", NULL);
}

void xmq_print_clines(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !first) return;

    XMQPrintState ps = {};
    ps.doq = doq;
    ps.output_settings = os;

    cline_print_nodes(&ps, (XMQNode*)first);
}

void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *os)
{
    if (doq == NULL || doq->dom_ == NULL) return;
    void *first = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !first) return;
    void *last = xml_node_last_child(xml_dom_document(doq->dom_));

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
    print_nodes(&ps, (XMQNode*)first, (XMQNode*)last, 0);
    if (theme->content.post) write(writer_state, theme->content.post, NULL);

    if (theme->body.post) write(writer_state, theme->body.post, NULL);
    if (theme->document.post) write(writer_state, theme->document.post, NULL);

    if (os->final_newline) write(writer_state, "\n", NULL);
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
    else if (output_settings->output_format == XMQ_CONTENT_YAML)
    {
        xmq_print_yaml(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_TEXT)
    {
        xmq_print_text(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_CLINES)
    {
        xmq_print_clines(doq, output_settings);
    }
    else
    {
        xmq_print_xmq(doq, output_settings);
    }

    if (output_settings->output_buffer &&
        output_settings->output_buffer_start &&
        output_settings->output_buffer_stop)
    {
        membuffer_append_null(output_settings->output_buffer);
        size_t size = membuffer_used(output_settings->output_buffer);
        char *buffer = free_membuffer_but_return_trimmed_content(output_settings->output_buffer);
        output_settings->output_buffer = NULL; // Handed over to the caller.
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

void xmqAnnotateOffsets(XMQDoc *doq, const char *attribute_name, const char *ns)
{
    annotate_offsets(doq->dom_, attribute_name, ns);
}

// Use an internal bit for signaling comment trimming.
#define TRIM_COMMENT 65536
// Use an internal bit for signaling treating tabs as part of incidental indentation.
#define TRIM_TABS    65536*2

void trim_text_node(XMQNode *node, int flags)
{
    // If node has whitespace preserve set, then do not trim.
    //if (xmlNodeGetSpacePreserve (node)) return;

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
        xml_node_free(node);
        return;
    }
    // This is not entirely whitespace, now use the xmq_un_quote function to remove any incidental indentation.
    // Use indent==0 and space==0 to indicate to the unquote function to assume the the first line indent
    // is the same as the second line indent! This is necessary to gracefully handle all the possible xml indentations.
    const char *start = content;
    const char *stop = start+strlen(start);

    if (flags & TRIM_COMMENT)
    {
        // For comments we always remove leading and ending spaces.
        while (start < stop && *start == ' ') start++;
        while (stop > start && *(stop-1) == ' ') stop--;
    }

    XMQReturnString rs = xmq_un_quote(start, stop, false, false);
    assert(rs.status == XMQ_OK);
    char *trimmed = rs.string;
    if (trimmed[0] == 0)
    {
        xml_node_free(node);
        free(trimmed);
        return;
    }
    xml_node_set_content(node, trimmed);
    free(trimmed);
}

void trim_node(XMQNode *node, int flags)
{
    debug("xmq=", "trim %s", xml_element_type_to_string(xml_node_kind(node)));

    if (is_content_node(node))
    {
        trim_text_node(node, flags);
        return;
    }

    if (is_comment_node(node))
    {
        trim_text_node(node, flags | TRIM_COMMENT);
        return;
    }

    // Do not recurse into these
    if (xml_node_kind(node) == XML_DOM_ENTITY_DECL) return;

    XMQNode* i = xml_first_child(node);
    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

void xmqTrimWhitespace(XMQDoc *doq, int flags)
{
    XMQNode *i = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !i) return;

    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

/*
XMQNode *merge_surrounding_text_nodes(XMQNode *node)
{
    const char *val = (const char *)node->name;
    // Not a hex entity.
    if (val[0] != '#' || val[1] != 'x') return node->next;

    debug("xmq=", "merge hex %s chars %s", val, xml_element_type_to_string(node->type));

    UTF8Char uni;
    int uc = strtol(val+2, NULL, 16);
    size_t len = encode_utf8(uc, &uni);
    char buf[len+1];
    memcpy(buf, uni.bytes, len);
    buf[len] = 0;

    XMQNode* prev = node->prev;
    XMQNode* next = node->next;
    if (prev && prev->type == XML_TEXT_NODE)
    {
        xmlNodeAddContentLen(prev, (char*)buf, len);
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        debug("xmq=", "merge left");
    }
    if (next && next->type == XML_TEXT_NODE)
    {
        xmlNodeAddContent(prev, next->content);
        XMQNode *n = next->next;
        xmlUnlinkNode(next);
        xmlFreeNode(next);
        next = n;
        debug("xmq=", "merge right");
    }

    return next;
}

XMQNode *merge_hex_chars_node(XMQNode *node)
{
    if (node->type == XML_ENTITY_REF_NODE)
    {
        return merge_surrounding_text_nodes(node);
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return node->next;

    XMQNode* i = xml_first_child(node);
    while (i)
    {
        i = merge_hex_chars_node(i);
    }
    return node->next;
}

void xmqMergeHexCharEntities(XMQDoc *doq)
{
    XMQNode *i = xml_node_first_child(xml_dom_document(doq->dom_));
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

void fixup_html(XMQDoc *doq, XMQNode *node, bool inside_cdata_declared)
{
    if (xml_node_kind(node) == XML_DOM_COMMENT)
    {
        // When writing an xml comment we must replace --- with -␐-␐-.
        // An already existing -␐- is replaced with -␐␐- etc.
        char *new_content = escape_xml_comment(xml_node_content(node));
        if (new_content)
        {
            // Oups, the content contains -- which must be quoted as -␐-␐
            // Likewise, if the content contains -␐-␐ it will be quoted as -␐␐-␐␐
            xml_node_set_content(node, new_content);
            free(new_content);
        }
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_CDATA)
    {
        // When the html is loaded by the libxml2 parser it creates a cdata
        // node instead of a text node for the style content.
        // If this is allowed to be printed as is, then this creates broken html.
        // I.e. <style><![CDATA[h1{color:red;}]]></style>
        // But we want: <style>h1{color:red;}</style>
        // Workaround until I understand the proper fix, just make it a text node.
        XMQNode *text = xml_node_new_text(doq->dom_, xml_node_content(node));
        xml_node_replace(node, text);
        xml_node_free(node);
        return;
    }
    else if (is_entity_node(node) && inside_cdata_declared)
    {
        // A character reference becomes its character, an entity reference is dropped.
        char *new_content = xml_node_string_value(node);
        XMQNode *new_node = xml_node_new_text(doq->dom_, new_content);
        free(new_content);
        xml_node_replace(node, new_node);
        xml_node_free(node);
        return;
    }

    // Do not descend into the dtd or into entity references: the children of an
    // entity reference is its declaration, which can refer back to it.
    if (xml_node_kind(node) == XML_DOM_DTD || xml_node_kind(node) == XML_DOM_ENTITY_REF) return;

    XMQNode *i = xml_first_child(node);
    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in trim.

        bool r = inside_cdata_declared;

        if (xml_node_name(i) &&
            (!strcasecmp(xml_node_name(i), "style") ||
             !strcasecmp(xml_node_name(i), "script")))
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
    XMQNode *i = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !i) return;

    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in fixup_html.
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

const char *xml_element_type_to_string(XmlNodeKind type)
{
    switch (type)
    {
    case XML_DOM_ELEMENT: return "element";
    case XML_DOM_ATTRIBUTE: return "attribute";
    case XML_DOM_TEXT: return "text";
    case XML_DOM_CDATA: return "cdata";
    case XML_DOM_ENTITY_REF: return "entity_ref";
    case XML_DOM_PI: return "pi";
    case XML_DOM_COMMENT: return "comment";
    case XML_DOM_DOCUMENT: return "document";
    case XML_DOM_DTD: return "dtd";
    case XML_DOM_ELEMENT_DECL: return "element_decl";
    case XML_DOM_ATTRIBUTE_DECL: return "attribute_decl";
    case XML_DOM_ENTITY_DECL: return "entity_decl";
    case XML_DOM_NAMESPACE: return "namespace_decl";
    case XML_DOM_NOTATION_DECL: return "notation";
    case XML_DOM_PE_REF: return "pe_ref";
    }
    return "?";
}

void fixup_comments(XMQDoc *doq, XMQNode *node, int depth)
{
    debug("xmq=", "fixup comments %s|%s %s", indent_depth(depth), xml_node_name(node), xml_element_type_to_string(xml_node_kind(node)));
    if (xml_node_kind(node) == XML_DOM_COMMENT)
    {
        // An xml comment containing dle escapes for example: -␐-␐- is replaceed with ---.
        // If multiple dle escapes exists, then for example: -␐␐- is replaced with -␐-.
        char *content_needed_escaping = unescape_xml_comment(xml_node_content(node));
        if (content_needed_escaping)
        {
            if (xmq_debug_enabled_)
            {
                XMQReturnString rs = xmq_quote_as_c(xml_node_content(node), NULL, false);
                assert(rs.status == XMQ_OK);
                char *from = rs.string;
                rs = xmq_quote_as_c(content_needed_escaping, NULL, false);
                assert(rs.status == XMQ_OK);
                char *to = rs.string;
                debug("xmq=", "fix comment \"%s\" to \"%s\"", from, to);
            }

            xml_node_set_content(node, content_needed_escaping);
            free(content_needed_escaping);
        }
        return;
    }

    // Do not recurse into these
    if (xml_node_kind(node) == XML_DOM_ENTITY_DECL) return;

    XMQNode *i = xml_first_child(node);
    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_comments(doq, i, depth+1);
        i = next;
    }
}

void xmq_fixup_comments_after_readin(XMQDoc *doq)
{
    XMQNode *i = xml_node_first_child(xml_dom_document(doq->dom_));
    if (!doq || !i) return;

    debug("xmq=", "fixup comments after readin");

    while (i)
    {
        XMQNode *next = xml_next_sibling(i); // i might be freed in fixup_comments.
        fixup_comments(doq, i, 0);
        i = next;
    }
}

const char *xmqDocError(XMQDoc *doq)
{
    return doq->error_;
}

XMQStatus xmqDocErrno(XMQDoc *doq)
{
    return (XMQStatus)doq->errno_;
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
                 bool use_dqs,
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

    const char q = use_dqs?'"':'\'';

    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, q);
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
    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, q);

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
    const char q = *i;
    assert(q == '\'' || q == '"');
    while (*i == q) { i++; llq++;  }
    const char *lstart = i; // Points to text after leading quotes.
    // Find end of line.
    while (i < stop && *i != '\n') i++;
    const char *eol = i;
    i--;
    while (i > lstart && *i == q) { i--; eeq++; }
    i++;
    const char *lstop = i;
    // Mark endof text inside ending quotes.
    for (i = lstart; i < lstop; ++i)
    {
        if (*i == q)
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
    bool use_double_quotes = false;
    bool prefer_double_quotes = false;
    int numq = count_necessary_quotes(start, stop, &add_nls, &add_compound, prefer_double_quotes, &use_double_quotes);

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
                      use_double_quotes,
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
    if (!doq || !doq->dom_ || !xpath) return 0;
    XmlXPathSettings settings;
    xml_xpath_settings_default(&settings);
    // Without a relative node there is no context node, only the document (like libxml2).
    settings.no_context_node = (relative == NULL);
    XMQNode *context = relative ? relative : xml_dom_document(doq->dom_);
    XmlXPathResult *r = xml_xpath_eval(xpath, context, &settings, NULL);
    if (!r) return 0;

    int size = (int)xml_xpath_result_size(r);
    if (cb)
    {
        for (int i = 0; i < size; i++)
        {
            XMQProceed proceed = cb(doq, xml_xpath_result_node(r, i), user_data);
            if (proceed == XMQ_STOP) break;
        }
    }
    xml_xpath_result_free(r);
    return size;
}

const char *xmqGetName(XMQNode *node)
{
    XMQNode* p = (XMQNode*)node;
    if (p)
    {
        return xml_node_name(p);
    }
    return NULL;
}

const char *xmqGetContent(XMQNode *node)
{
    if (!node) return NULL;
    switch (xml_node_kind(node))
    {
    case XML_DOM_ELEMENT:
        if (xml_node_first_child(node)) return xml_node_content(xml_node_first_child(node));
        return NULL;
    case XML_DOM_TEXT:
    case XML_DOM_CDATA:
    case XML_DOM_COMMENT:
    case XML_DOM_PI:
        return xml_node_content(node);
    default:
        return NULL;
    }
}

XMQNodeType xmqGetNodeType(XMQNode *node)
{
    if (!node) return XMQ_NODE_UNKNOWN;
    switch (xml_node_kind(node))
    {
    case XML_DOM_ELEMENT: return XMQ_NODE_ELEMENT;
    case XML_DOM_TEXT: return XMQ_NODE_TEXT;
    case XML_DOM_CDATA: return XMQ_NODE_CDATA;
    case XML_DOM_ENTITY_REF: return XMQ_NODE_ENTITY_REF;
    case XML_DOM_PI: return XMQ_NODE_PI;
    case XML_DOM_COMMENT: return XMQ_NODE_COMMENT;
    case XML_DOM_DOCUMENT: return XMQ_NODE_DOCUMENT;
    case XML_DOM_DTD: return XMQ_NODE_DOCTYPE;
    default: return XMQ_NODE_UNKNOWN;
    }
}

XMQNode *xmqGetDocNode(XMQDoc *doq)
{
    if (!doq || !doq->dom_) return NULL;
    return xml_dom_document(doq->dom_);
}

XMQNode *xmqGetParent(XMQNode *node)
{
    return node ? xml_node_parent(node) : NULL;
}

XMQNode *xmqFirstChild(XMQNode *node)
{
    return node ? xml_node_first_child(node) : NULL;
}

XMQNode *xmqLastChild(XMQNode *node)
{
    return node ? xml_node_last_child(node) : NULL;
}

XMQNode *xmqNextSibling(XMQNode *node)
{
    return node ? xml_node_next(node) : NULL;
}

XMQNode *xmqPrevSibling(XMQNode *node)
{
    return node ? xml_node_prev(node) : NULL;
}

const char *xmqGetNamespaceUri(XMQNode *node)
{
    if (!node || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    return xml_node_ns_uri(node);
}

const char *xmqGetNamespacePrefix(XMQNode *node)
{
    if (!node || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    const char *p = xml_node_ns_prefix(node);
    return (p && *p) ? p : NULL;
}

int xmqGetLine(XMQNode *node)
{
    return node ? xml_node_line(node) : 0;
}

XMQAttr *xmqFirstAttr(XMQNode *node)
{
    if (!node || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    return xml_node_first_attr(node);
}

XMQAttr *xmqNextAttr(XMQAttr *attr)
{
    return attr ? xml_attr_next(attr) : NULL;
}

XMQAttr *xmqGetAttr(XMQNode *node, const char *name)
{
    if (!node || !name || xml_node_kind(node) != XML_DOM_ELEMENT) return NULL;
    for (XMQAttr *a = xml_node_first_attr(node); a; a = xml_attr_next(a))
    {
        if (!strcmp(xml_attr_name(a), name)) return a;
    }
    return NULL;
}

const char *xmqGetAttrName(XMQAttr *attr)
{
    return attr ? xml_attr_name(attr) : NULL;
}

const char *xmqGetAttrValue(XMQAttr *attr)
{
    return attr ? xml_attr_simple_value(attr) : NULL;
}

const char *xmqGetAttrNamespaceUri(XMQAttr *attr)
{
    return attr ? xml_node_ns_uri(xml_attr_node(attr)) : NULL;
}

const char *xmqGetAttrNamespacePrefix(XMQAttr *attr)
{
    if (!attr) return NULL;
    const char *p = xml_node_ns_prefix(xml_attr_node(attr));
    return (p && *p) ? p : NULL;
}

XMQNode *xmqGetAttrOwner(XMQAttr *attr)
{
    return attr ? xml_attr_owner(attr) : NULL;
}

void xmqSetContent(XMQNode *node, const char *raw_content)
{
    xml_node_set_content(node, raw_content);
}

XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data)
{
    const char **out = (const char **)user_data;
    XMQNode* n = (XMQNode*)node;
    if (n && xml_node_first_child(n))
    {
        *out = xml_node_content(xml_node_first_child(n));
    }
    else
    {
        *out = NULL;
    }
    return XMQ_STOP;
}

XMQProceed catch_single_node(XMQDoc *doc, XMQNode *node, void *user_data)
{
    XMQNode **out = (XMQNode**)user_data;
    XMQNode* n = (XMQNode*)node;
    *out = (XMQNode*)n;
    return XMQ_STOP;
}

XMQNode *xmqGetNode(XMQDoc *doq, const char *xpath)
{
    return xmqGetNodeRel(doq, xpath, NULL);
}

XMQNode *xmqGetNodeRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    XMQNode *node = NULL;

    xmqForeachRel(doq, xpath, catch_single_node, (void*)&node, relative);

    return node;
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

static void xmq_print_parse_message(void *user_data, const char *message)
{
    // Like the libxml2 default error handler.
    fputs(message, stderr);
}

static bool xmq_should_trim(int flags)
{
    bool should_trim = false;
    if ((flags & XMQ_FLAG_TRIM_HEURISTIC) ||
        (flags & XMQ_FLAG_TRIM_EXACT)) should_trim = true;
    if (flags & XMQ_FLAG_TRIM_NONE) should_trim = false;
    return should_trim;
}

bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = doq->source_name_;
    settings.no_cdata = true;
    settings.no_blanks = xmq_should_trim(flags);
    settings.message_callback = xmq_print_parse_message;

    XmlParseError error;
    XmlDom *doc = xml_parse_buffer(start, stop, &settings, &error);
    if (doc == NULL)
    {
        doq->errno_ = XMQ_ERROR_PARSING_XML;
        // Print the error message like libxml2 did.
        if (error.message) fputs(error.message, stderr);
        doq->error_ = NULL;
        xml_parse_error_free(&error);
        return false;
    }
    xml_parse_error_free(&error);

    if (doq->dom_)
    {
        xml_dom_free(doq->dom_);
    }

    doq->dom_ = doc;
    doq->root_ = xml_dom_root_element(doc);

    xmq_fixup_comments_after_readin(doq);

    return true;
}

XMQStatus xmqExpandEntities(XMQDoc *doq)
{
    if (!doq || !doq->dom_) return XMQ_OK;
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = doq->source_name_;
    settings.no_cdata = true;
    settings.message_callback = xmq_print_parse_message;

    XmlParseError error;
    bool ok = xml_expand_entities(doq->dom_, NULL, NULL, doq->original_size_, &settings, &error);
    if (!ok)
    {
        doq->errno_ = XMQ_ERROR_ENTITY_EXPANSION;
        free((void*)doq->error_);
        doq->error_ = build_error_message("xmq: %s\n", error.short_message ? error.short_message : "entity expansion failed");
        xml_parse_error_free(&error);
        return XMQ_ERROR_ENTITY_EXPANSION;
    }
    xml_parse_error_free(&error);
    doq->root_ = xml_dom_root_element(doq->dom_);
    return XMQ_OK;
}

bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    XmlHtmlParseSettings settings;
    xml_html_parse_settings_default(&settings);
    settings.source_name = doq->source_name_;
    settings.no_blanks = xmq_should_trim(flags);

    XmlDom *doc = xml_html_parse_buffer(start, stop, &settings);

    if (xml_dom_root_element(doc) == NULL)
    {
        PRINT_ERROR("empty document\n");
        xml_dom_free(doc);
        return 0;
    }

    if (doq->dom_)
    {
        xml_dom_free(doq->dom_);
    }
    doq->dom_ = doc;
    doq->root_ = xml_dom_root_element(doc);

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root)
{
    XMQNode *text = xml_node_new_text_len(doq->dom_, start, stop-start);

    if (implicit_root && implicit_root[0])
    {
        // We have an implicit root must be created since input is text.
        XMQNode *root = xml_node_new_element(doq->dom_, implicit_root, NULL);
        xml_dom_set_root_element(doq->dom_, root);
        doq->root_ = root;
        xml_node_append_child(root, text);
    }
    else
    {
        // There is no implicit root. Text is the new root node.
        xml_node_append_child(xml_dom_document(doq->dom_), text);
    }
    return true;
}

bool xmq_prepare_node(const char *start, const char *stop);

bool xmq_prepare_node(const char *start, const char *stop)
{
    const char *i = start;

    if (*i != '/') return false;

    while (i < stop)
    {
    }
    return false;
}

bool xmq_parse_buffer_clines(XMQDoc *doq, const char *start, const char *stop)
{
    XMQNode *text = xml_node_new_text_len(doq->dom_, start, stop-start);
    xml_node_append_child(xml_dom_document(doq->dom_), text);

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
        if (ct != detected_ct && ct != XMQ_CONTENT_TEXT && ct != XMQ_CONTENT_IXML && ct != XMQ_CONTENT_YAML)
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
    case XMQ_CONTENT_YAML: ok = xmq_parse_buffer_yaml(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_IXML: ok = xmq_parse_buffer_ixml(doq, start, stop, flags); break;
    case XMQ_CONTENT_TEXT: ok = xmq_parse_buffer_text(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_CLINES: ok = xmq_parse_buffer_clines(doq, start, stop); break;
    default: break;
    }

exit:

    if (ok)
    {
        bool should_trim = false;

        if ((flags & XMQ_FLAG_TRIM_HEURISTIC) ||
            (flags & XMQ_FLAG_TRIM_EXACT)) should_trim = true;

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

static void infer_missing_element_names_rec(XMQNode *node)
{
    for (XMQNode *child = xml_node_first_child(node); child; child = xml_node_next(child))
    {
        if (xml_node_kind(child) != XML_DOM_ELEMENT) continue;

        // Give an anonymous child the singular form of the name of the
        // parent, items -> item, steps -> step, branches -> branch.
        if (xml_node_name(child) &&
            xml_node_name(node) &&
            strcmp(xml_node_name(node), "_") != 0 &&
            !strcmp(xml_node_name(child), "_"))
        {
            char *nm = plural_to_singular(xml_node_name(node));
            if (nm != NULL)
            {
                xml_node_set_name(child, nm);
                free(nm);
            }
        }

        infer_missing_element_names_rec(child);
    }
}

bool xmqInferMissingElementNames(XMQDoc *doq, const char *root_name)
{
    if (doq == NULL || doq->dom_ == NULL) return false;

    XMQNode *root = xml_dom_root_element(doq->dom_);
    if (root == NULL) return true; // No root, nothing to do.

    // Give the root a name when it is anonymous and a root name is supplied.
    if (root_name != NULL &&
        root_name[0] != 0 &&
        strcmp(xml_node_name(root), "_") == 0)
    {
        xml_node_set_name(root, root_name);
    }

    if (xml_node_kind(root) == XML_DOM_ELEMENT)
    {
        infer_missing_element_names_rec(root);
    }

    return true;
}


XMQNode* parse_doctype_raw(XMQDoc *doq, const char *start, const char *stop)
{
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.source_name = doq->source_name_;
    settings.message_callback = xmq_print_parse_message;
    XmlParseError error;
    XmlDom *doc = xml_parse_buffer(start, stop, &settings, &error);
    if (!doc)
    {
        if (error.message) fputs(error.message, stderr);
        xml_parse_error_free(&error);
        return NULL;
    }
    xml_parse_error_free(&error);

    XMQNode *dtd = xml_dom_dtd(doc);
    XMQNode *copy = dtd ? xml_node_copy(doq->dom_, dtd, true) : NULL;
    xml_dom_free(doc);

    return copy;
}

/* Transform markers ////////////////////////////////////////////////////////

   An xslt 1.0 transform cannot create entity references, a doctype, an empty
   text node, a namespace declaration that is not used, or an element/attribute
   name with an undeclared prefix. A transform (eg library/core/xmq.xslq that
   builds the xmq document from the parse tree of library/core/xmq.ixml) can
   instead output marker elements in the namespace XMQ_MARKERS_NS, these are
   replaced after the transform:

     xmq:entity(name = nbsp)        the entity reference &nbsp;
     xmq:entity(name = #10)         the character (merged into the text)
     xmq:empty                      an empty text node
     xmq:comment = 'a--b'           a comment that xsl:comment refuses (-- or ending -)
     xmq:doctype = 'html'           the doctype <!DOCTYPE html>
     xmq:xml = '<a>..</a>'          the parsed xml
     xmq:xmlns(prefix = a) = uri    the declaration xmlns:a = uri on the parent
                                    (prefix empty for the default namespace)
     xmq:attribute(name = a:b ns = uri) { text and xmq:entity }
                                    the attribute on the parent, in namespace uri
                                    (an empty uri for an undeclared prefix, which
                                    is declared without uri like the xmq parser does)
     xmq:element(name = a:b) {...}  the element renamed to a:b, the undeclared
                                    prefix is declared without uri on it, like
                                    the xmq parser does

   Namespace declarations that the transform added and that repeat an
   identical declaration of an ancestor are removed, the explicit xmq:xmlns
   declarations are kept. */

#define XMQ_MARKERS_NS "https://libxmq.org/xmq/markers"
#define XMQ_EXPLICIT_NS ((void*)1)

static bool is_marker(XMQNode* n, const char *name)
{
    return xml_node_kind(n) == XML_DOM_ELEMENT && xml_node_ns(n) && xml_ns_href(xml_node_ns(n)) &&
        !strcmp(xml_ns_href(xml_node_ns(n)), XMQ_MARKERS_NS) &&
        (name == NULL || !strcmp(xml_node_name(n), name));
}

static char *marker_prop(XMQNode* n, const char *name)
{
    XMQAttr *a = xml_node_get_attr(n, name, NULL);
    if (!a) return strdup("");
    return xml_node_string_value(xml_attr_node(a));
}

static char *marker_content(XMQNode* n)
{
    return xml_node_string_value(n);
}

/** An entity marker becomes a text node (character entity) or an entity reference. */
static XMQNode* marker_entity_node(XmlDom* doc, XMQNode* marker)
{
    char *name = marker_prop(marker, "name");
    XMQNode* n = NULL;
    if (name[0] == '#')
    {
        long uc = (name[1] == 'x') ? strtol(name+2, NULL, 16) : strtol(name+1, NULL, 10);
        if (uc > 0 && uc <= 0x10ffff)
        {
            UTF8Char uni;
            size_t len = encode_utf8(uc, &uni);
            char buf[len+1];
            memcpy(buf, uni.bytes, len);
            buf[len] = 0;
            n = xml_node_new_text(doc, buf);
        }
    }
    if (!n)
    {
        n = xml_node_new_entity_ref(doc, name);
    }
    free(name);
    return n;
}

static void marker_replace(XMQNode* marker, XMQNode* n)
{
    // Insert without merging text, adjacent text is merged by cleanup_after_markers.
    if (n) xml_node_add_prev_sibling(marker, n);
    xml_node_free(marker);
}

static XmlNs* marker_find_ns_def(XMQNode* e, const char *prefix)
{
    for (XmlNs* ns = xml_node_first_ns_decl(e); ns; ns = xml_ns_next_decl(ns))
    {
        if (prefix == NULL && xml_ns_prefix(ns) == NULL) return ns;
        if (prefix && xml_ns_prefix(ns) && !strcmp(prefix, xml_ns_prefix(ns))) return ns;
    }
    return NULL;
}

static void marker_xmlns(XMQNode* parent, XMQNode* marker)
{
    char *prefix = marker_prop(marker, "prefix");
    char *href = marker_content(marker);
    const char *p = prefix[0] ? prefix : NULL;
    if (xml_node_kind(parent) == XML_DOM_ELEMENT)
    {
        XmlNs* ns = marker_find_ns_def(parent, p);
        if (!ns) ns = xml_ns_declare(parent, p, href);
        if (ns)
        {
            xml_ns_set_user_data(ns, XMQ_EXPLICIT_NS);
            // Move the declaration last, the markers come in source order.
            xml_ns_move_decl_last(parent, ns);
        }
    }
    free(prefix);
    free(href);
}

static void marker_attribute(XmlDom* doc, XMQNode* parent, XMQNode* marker)
{
    if (xml_node_kind(parent) != XML_DOM_ELEMENT) return;
    char *name = marker_prop(marker, "name");
    char *uri = marker_prop(marker, "ns");
    XMQAttr* a = NULL;
    char *colon = strchr(name, ':');
    if (colon)
    {
        // A prefixed name, an undeclared prefix (empty uri) is declared with
        // an empty uri like the xmq parser does.
        *colon = 0;
        XmlNs* ns = NULL;
        if (uri[0])
        {
            ns = xml_ns_lookup_prefix(parent, name);
            if (!ns || !xml_ns_href(ns) || strcmp(xml_ns_href(ns), uri))
            {
                ns = xml_ns_declare(parent, name, uri);
            }
        }
        else
        {
            ns = marker_find_ns_def(parent, name);
            if (!ns) ns = xml_ns_declare(parent, name, NULL);
        }
        if (ns) xml_ns_set_user_data(ns, XMQ_EXPLICIT_NS);
        a = xml_node_append_attr(parent, colon+1, ns, NULL);
        *colon = ':';
    }
    else
    {
        a = xml_node_append_attr(parent, name, NULL, NULL);
    }
    if (a)
    {
        for (XMQNode* c = xml_node_first_child(marker); c; c = xml_node_next(c))
        {
            XMQNode* n = NULL;
            if (xml_node_kind(c) == XML_DOM_TEXT) n = xml_node_new_text(doc, xml_node_content(c));
            else if (is_marker(c, "entity")) n = marker_entity_node(doc, c);
            if (n) xml_node_add_child(xml_attr_node(a), n);
        }
    }
    free(name);
    free(uri);
}

static void marker_doctype(XMQDoc *doq, XMQNode* marker)
{
    XmlDom* doc = doq->dom_;
    char *value = marker_content(marker);
    size_t tn = strlen(value);
    size_t n = tn+10+1+12;
    char *buf = (char*)malloc(n);
    strcpy(buf, "<!DOCTYPE ");
    memcpy(buf+10, value, tn);
    memcpy(buf+10+tn, "><foo></foo>", 12);
    buf[n-1] = 0;
    XMQNode* dtd = parse_doctype_raw(doq, buf, buf+n-1);
    if (dtd && !xml_dom_dtd(doc))
    {
        xml_node_add_prev_sibling(marker, dtd);
    }
    else if (dtd)
    {
        xml_node_free(dtd);
    }
    free(buf);
    free(value);
}

static XMQNode* marker_xml(XmlDom* doc, XMQNode* marker)
{
    char *value = marker_content(marker);
    XMQNode* node = NULL;
    XmlParseSettings settings;
    xml_parse_settings_default(&settings);
    settings.no_cdata = true;
    settings.no_blanks = true;
    settings.message_callback = xmq_print_parse_message;
    XmlParseError error;
    XmlDom *fdoc = xml_parse_buffer(value, value+strlen(value), &settings, &error);
    if (!fdoc && error.message) fputs(error.message, stderr);
    xml_parse_error_free(&error);
    if (fdoc)
    {
        XMQNode* root = xml_dom_root_element(fdoc);
        if (root) node = xml_node_copy(doc, root, true);
        xml_dom_free(fdoc);
    }
    free(value);
    return node;
}

static void replace_markers(XMQDoc *doq, XMQNode* parent)
{
    XmlDom* doc = doq->dom_;
    XMQNode* next = NULL;
    for (XMQNode* i = xml_node_first_child(parent); i; i = next)
    {
        next = xml_node_next(i);
        if (!is_marker(i, NULL))
        {
            if (xml_node_kind(i) == XML_DOM_ELEMENT) replace_markers(doq, i);
            continue;
        }
        const char *m = xml_node_name(i);
        if (!strcmp(m, "entity")) marker_replace(i, marker_entity_node(doc, i));
        else if (!strcmp(m, "empty")) marker_replace(i, xml_node_new_text(doc, ""));
        else if (!strcmp(m, "comment"))
        {
            char *c = marker_content(i);
            marker_replace(i, xml_node_new_comment(doc, c));
            free(c);
        }
        else if (!strcmp(m, "xml")) marker_replace(i, marker_xml(doc, i));
        else if (!strcmp(m, "doctype"))
        {
            marker_doctype(doq, i);
            marker_replace(i, NULL);
        }
        else if (!strcmp(m, "xmlns"))
        {
            marker_xmlns(parent, i);
            marker_replace(i, NULL);
        }
        else if (!strcmp(m, "attribute"))
        {
            marker_attribute(doc, parent, i);
            marker_replace(i, NULL);
        }
        else if (!strcmp(m, "element"))
        {
            // An element with an undeclared prefix, declare the prefix
            // without uri on it like the xmq parser does.
            char *name = marker_prop(i, "name");
            xml_node_remove_attr(i, "name", NULL);
            XmlNs* ns = NULL;
            char *colon = strchr(name, ':');
            if (colon)
            {
                *colon = 0;
                ns = xml_ns_declare(i, name, NULL);
                if (ns) xml_ns_set_user_data(ns, XMQ_EXPLICIT_NS);
                *colon = ':';
            }
            xml_node_set_name(i, colon ? colon+1 : name);
            xml_node_set_ns(i, ns);
            free(name);
            replace_markers(doq, i);
        }
        else
        {
            replace_markers(doq, i);
        }
    }
}

/** Remove the marker namespace declarations and the declarations that
    repeat an identical declaration of an ancestor. Merge adjacent text nodes
    created when markers were replaced. */
static void cleanup_after_markers(XmlDom* doc, XMQNode* node)
{
    if (xml_node_kind(node) == XML_DOM_ELEMENT)
    {
        XmlNs* next = NULL;
        for (XmlNs* ns = xml_node_first_ns_decl(node); ns; ns = next)
        {
            next = xml_ns_next_decl(ns);
            bool remove = false;
            XmlNs* replacement = NULL;
            if (xml_ns_href(ns) && !strcmp(xml_ns_href(ns), XMQ_MARKERS_NS))
            {
                remove = true;
            }
            else if (xml_ns_user_data(ns) != XMQ_EXPLICIT_NS && xml_node_parent(node) && xml_node_kind(xml_node_parent(node)) == XML_DOM_ELEMENT)
            {
                XmlNs* anc = xml_ns_lookup_prefix(xml_node_parent(node), xml_ns_prefix(ns));
                if (anc && xml_ns_href(anc) && xml_ns_href(ns) && !strcmp(xml_ns_href(anc), xml_ns_href(ns)))
                {
                    remove = true;
                    replacement = anc;
                }
            }
            if (remove)
            {
                xml_ns_remove_decl(node, ns, replacement);
            }
        }
    }
    for (XMQNode* c = xml_node_first_child(node); c; c = xml_node_next(c))
    {
        while (xml_node_kind(c) == XML_DOM_TEXT && xml_node_next(c) && xml_node_kind(xml_node_next(c)) == XML_DOM_TEXT)
        {
            XMQNode* n = xml_node_next(c);
            const char *t = xml_node_content(n);
            xml_node_append_content(c, t, strlen(t));
            xml_node_free(n);
        }
        if (xml_node_kind(c) == XML_DOM_ELEMENT) cleanup_after_markers(doc, c);
    }
}

static bool has_markers(XMQNode* node)
{
    for (XMQNode* c = xml_node_first_child(node); c; c = xml_node_next(c))
    {
        if (is_marker(c, NULL)) return true;
        if (xml_node_kind(c) == XML_DOM_ELEMENT && has_markers(c)) return true;
    }
    return false;
}

void xmq_replace_transform_markers(XMQDoc *doq)
{
    XmlDom* doc = doq->dom_;
    if (!doc || !has_markers(xml_dom_document(doc))) return;
    replace_markers(doq, xml_dom_document(doc));
    cleanup_after_markers(doc, xml_dom_document(doc));
    doq->root_ = xml_dom_root_element(doc);
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

    stack_push(state->element_stack, xml_dom_document(doq->dom_));
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
    ixml_grammar->xmq_parse_state_ = state;
    if (xmqVerbose()) run->verbose = true;
    if (xmqDebugging()) run->debug = run->verbose = true;
    if (xmqTracing()) run->trace = run->debug = run->verbose = true;

    // Lets parse the ixml source to construct a yaep grammar.
    // This yaep grammar is cached in ixml_grammar->yaep_grammar_.
    ixml_build_yaep_grammar((YaepParseRun*)ixml_grammar->yaep_parse_run_,
                            (YaepGrammar*)ixml_grammar->yaep_grammar_,
                            state,
                            start,
                            stop,
                            NULL,
                            NULL);

    if (xmqStateErrno(state))
    {
        rc = false;
        ixml_grammar->errno_ = xmqStateErrno(state);
        ixml_grammar->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    // Do not free the xmq state, the yaep run nor the state,
    // it must be kept alive with the doc
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
    return doc->yaep_grammar_;
}

YaepParseRun *xmq_get_yaep_parse_run(XMQDoc *doc)
{
    return doc->yaep_parse_run_;
}

XMQParseState *xmq_get_xmq_parse_state(XMQDoc *doc)
{
    return doc->xmq_parse_state_;
}

void add_key_number(XmlDom *doc, XMQNode *root, const char *key, int value)
{
    char buf[1024];
    snprintf(buf, 1024, "%d", value);
    add_key_string(doc, root, key, buf);
}

void add_key_string(XmlDom *doc, XMQNode *root, const char *key, const char *value)
{
    XMQNode *element = xml_node_new_element(doc, key, NULL);
    xml_node_append_child(root, element);
    xml_node_append_child(element, xml_node_new_text(doc, value));
}

/** Add a top level node to the document: an element becomes the root element. */
static void xmq_dom_add_top(XmlDom *doc, XMQNode *n)
{
    if (xml_node_kind(n) == XML_DOM_ELEMENT) xml_dom_set_root_element(doc, n);
    else xml_node_add_child(xml_dom_document(doc), n);
}

void handle_yaep_syntax_error(YaepParseRun *pr,
                              int err_tok_num,
                              void *err_tok_attr,
                              int start_ignored_tok_num,
                              void *start_ignored_tok_attr,
                              int start_recovered_tok_num,
                              void *start_recovered_tok_attr)
{
    int line = 0, col = 0;
    find_line_col(pr->buffer_start, pr->buffer_stop, err_tok_num, &line, &col);

    char buf[1024];
    snprintf(buf, 1024, "ixml:%d:%d: syntax error\n", line, col);
    int start = err_tok_num - 20;
    if (start < 0) start = 0;
    int stop = err_tok_num + 20;

    size_t len = strlen(buf);
    size_t j = len;
    for (int i = start; i < stop && pr->buffer_start[i] != 0; ++i)
    {
        buf[j++] =  pr->buffer_start[i];
    }
    buf[j++] = '\n';
    for (int i = start; i < err_tok_num; ++i)
    {
        buf[j++] = ' ';
    }
    buf[j++] = '^';
    buf[j++] = 0;

    XmlDom *new_doc = xml_dom_new();
    XMQNode *root = xml_node_new_element(new_doc, "ixml", NULL);
    xml_dom_set_root_element(new_doc, root);
    XmlNs *ns = xml_ns_declare(root, "ixml", "http://invisiblexml.org/NS");
    xml_node_set_attr(root, "state", ns, "failed");

    add_key_string(new_doc, root, "info", buf);
    add_key_number(new_doc, root, "line", line);
    add_key_number(new_doc, root, "column", col);
    add_key_number(new_doc, root, "pos", err_tok_num+1);

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *failure = rd.doc;
    xml_dom_free(failure->dom_);
    xmqSetImplementationDoc(failure, new_doc);
    pr->failure = failure;
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
        if (an->name[0] == '|' && an->name[1] == '+')
        {
            // The content to be inserted has been encoded in the rule name.
            // A hack yes. Does it work? Yes!
            if(an->name[2] == '#')
            {
                int value = (int)strtol(an->name+3, NULL, 16);
                UTF8Char utf8;
                size_t len = encode_utf8(value, &utf8);
                utf8.bytes[len] = 0;
                membuffer_append(mb, utf8.bytes);
            }
            else
            {
                membuffer_append(mb, an->name+2);
            }
        }
        else
        {
            // Normal node, recurse into it.
            for (int i=0; an->children[i] != NULL; ++i)
            {
                YaepTreeNode *nn = an->children[i];
                collect_text(nn, mb);
            }
        }
    }
    else
    if (n->type == YAEP_TERM)
    {
        YaepTerminalNode *at = &n->val.terminal;
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

// The first dynamic error found while generating the dom from the yaep tree, eg
// an attribute that occurs twice on an element (ixml error D02), or NULL.
static char *ixml_dynamic_error_ = NULL;

void generate_dom_from_yaep_node(XmlDom* doc, XMQNode* node, YaepTreeNode *n, YaepTreeNode *parent, int depth, int index)
{
    if (n == NULL) return;
    if (n->type == YAEP_TERM && n->val.terminal.code == -1) return;
    if (n->type == YAEP_ANODE)
    {
        YaepAbstractNode *an = &n->val.anode;

        if (an != NULL && an->name != NULL)
        {
            if (an->name[0] == '|' && an->name[1] == '!')
            {
                // The content is a not lookahead node. Discard it.
            }
            else if (an->name[0] == '|' && an->name[1] == '+')
            {
                // The content to be inserted has been encoded in the rule name.
                // A hack yes. Does it work? Yes!
                XMQNode* new_node = NULL;

                // Note: a |+// name used to create a comment node that was then
                // overwritten (and leaked) by the text node below, the text is kept.
                if(an->name[2] == '#')
                {
                    int value = (int)strtol(an->name+3, NULL, 16);
                    UTF8Char c;
                    size_t len = encode_utf8(value, &c);
                    new_node = xml_node_new_text_len(doc, c.bytes, len);
                }
                else
                {
                    new_node = xml_node_new_text(doc, an->name+2);
                }
                if (node == NULL)
                {
                    xmq_dom_add_top(doc, new_node);
                }
                else
                {
                    xml_node_add_child(node, new_node);
                }
            }
            else if (an->mark != '-')
            {
                if (an->mark == '@')
                {
                    // This should become an attribute.
                    MemBuffer *mb = new_membuffer();
                    collect_text(n, mb);
                    membuffer_append_null(mb);
                    // It is a dynamic error (D02) if an element gets the same attribute twice.
                    if (node && xml_node_kind(node) == XML_DOM_ELEMENT && xml_node_get_attr(node, an->name, NULL))
                    {
                        if (!ixml_dynamic_error_)
                        {
                            char buf[1024];
                            snprintf(buf, sizeof(buf), "dynamic error D02: the attribute %s occurs more than once on an element", an->name);
                            ixml_dynamic_error_ = strdup(buf);
                        }
                    }
                    else if (node && xml_node_kind(node) == XML_DOM_ELEMENT)
                    {
                        xml_node_set_attr(node, an->name, NULL, mb->buffer_);
                    }
                    free_membuffer_and_free_content(mb);
                }
                else
                {
                    // Normal node that should be generated.
                    XMQNode* new_node = xml_node_new_element(doc, an->name, NULL);

                    if (node == NULL)
                    {
                        xmq_dom_add_top(doc, new_node);
                    }
                    else
                    {
                        xml_node_add_child(node, new_node);
                    }

                    for (int i=0; an->children[i] != NULL; ++i)
                    {
                        YaepTreeNode *nn = an->children[i];
                        generate_dom_from_yaep_node(doc, new_node, nn, n, depth+1, i);
                    }
                }
            }
            else
            {
                // Skip anonymous node whose name starts with / and deleted nodes with mark=-
                for (int i=0; an->children[i] != NULL; ++i)
                {
                    YaepTreeNode *nn = an->children[i];
                    generate_dom_from_yaep_node(doc, node, nn, n, depth+1, i);
                }
            }
        }
    }
    else if (n->type == YAEP_ALT)
    {
        XMQNode* new_node = NULL;
        if (node == NULL)
        {
            new_node = xml_node_new_element(doc, "AMBIGUOUS", NULL);
            assert(xml_dom_root_element(doc) == NULL);
            xml_dom_set_root_element(doc, new_node);
        }
        else
        {
            YaepAbstractNode *an = NULL;

            if (parent) an = &parent->val.anode;

            if (!an || an->mark != '*')
            {
                new_node = xml_node_new_element(doc, "AMBIGUOUS", NULL);
                xml_node_add_child(node, new_node);
            }
            else
            {
                new_node = node;
            }
        }

        YaepTreeNode *alt = n;

        generate_dom_from_yaep_node(doc, new_node, alt->val.alt.node, n, depth+1, 0);

        alt = alt->val.alt.next;

        while (alt && alt->type == YAEP_ALT)
        {
            generate_dom_from_yaep_node(doc, new_node, alt->val.alt.node, n, depth+1, 0);
            alt = alt->val.alt.next;
        }
    }
    else
    if (n->type == YAEP_TERM)
    {
        YaepTerminalNode *at = &n->val.terminal;
        if (at->mark != '-')
        {
            UTF8Char utf8;
            size_t len = encode_utf8(at->code, &utf8);
            utf8.bytes[len] = 0;

            XMQNode* new_node = xml_node_new_text_len(doc, utf8.bytes, len);

            if (node == NULL)
            {
                xmq_dom_add_top(doc, new_node);
            }
            else
            {
                xml_node_add_child(node, new_node);
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
    bool ok = false;

    if (!doc || !start || !ixml_grammar) return false;
    if (!stop) stop = start+strlen(start);

    XMQParseState *state = xmq_get_xmq_parse_state(ixml_grammar);

    // Now add all character terminals in the content (not yet added) to the grammar.
    // And rewrite charsets into rules with multiple choice shrunk to the actual usage of characters in the input.
    scan_content_fixup_charsets(state, start, stop);

    ixml_print_grammar(state);

    state->yaep_i_ = hashmap_iterate(state->ixml_terminals_map);
    state->yaep_j_ = 0;
    int rc = yaep_read_grammar(xmq_get_yaep_parse_run(ixml_grammar),
                               xmq_get_yaep_grammar(ixml_grammar),
                               0,
                               ixml_to_yaep_read_terminal,
                               ixml_to_yaep_read_rule);
    hashmap_free_iterator(state->yaep_i_);

    if (rc != 0)
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "internal error, yaep did not accept generated yaep grammar";
        printf("xmq: internal error generating yaep grammar from ixml grammar %s\n", yaep_error_message(xmq_get_yaep_grammar(ixml_grammar)));
        longjmp(state->error_handler, 1);
    }

    yaep_set_one_parse_flag(xmq_get_yaep_grammar(ixml_grammar),
                            (flags & XMQ_FLAG_IXML_ALL_PARSES)?0:1);

    yaep_set_error_recovery_flag(xmq_get_yaep_grammar(ixml_grammar),
                                 (flags & XMQ_FLAG_IXML_TRY_TO_RECOVER)?1:0);

    if (state->ixml_costs_enabled)
    {
        verbose("xmq=", "ixml rule costs are used to resolve ambigiuity");
        // We have found =-in the grammar, ie that a rule has a certain cost. We assume this
        // means that there can be ambiguous results.
        // Keep parsing, finding all parses....
        yaep_set_one_parse_flag(xmq_get_yaep_grammar(ixml_grammar), 0);
        // Enable cost calculations...
        yaep_set_cost_flag(xmq_get_yaep_grammar(ixml_grammar), true);
    }

    if (state->ixml_controlled_ambiguity_enabled)
    {
        verbose("xmq=", "ixml controlled ambiguity enabled");
        // We have found *rule = ... the grammar, ie that a rule expects ambiguity.
        // Keep parsing, finding all parses....
        yaep_set_one_parse_flag(xmq_get_yaep_grammar(ixml_grammar), 0);
    }

    YaepParseRun *run = xmq_get_yaep_parse_run(ixml_grammar);
    // If we are compiling again with the same grammar.
    yaepResetParseRun(run);

    run->buffer_start = start;
    run->buffer_stop = stop;
    run->buffer_i = start;

    YaepGrammar *grammar = xmq_get_yaep_grammar(ixml_grammar);
    run->syntax_error = handle_yaep_syntax_error;

    // Parse source content using the yaep grammar, previously generated from the ixml source.
    rc = yaepParse(run, grammar);

    if (rc)
    {
        // There was an error, pick the generated error tree.
        if (flags & XMQ_FLAG_IXML_FAIL_SILENT)
        {
            // Create an empty document.
            xmqClearDoc(doc);

            // Free the unused failure document.
            xml_dom_free(run->failure->dom_);
            xmqSetImplementationDoc(run->failure, NULL);
            ok = false;
        }
        else
        {
            xml_dom_free(doc->dom_);
            // Copy the generated failure document as output.
            xmqSetImplementationDoc(doc, run->failure->dom_);
            xmqSetImplementationDoc(run->failure, NULL);
            ok = false;
        }
    }
    else
    {
        // IXML parse was fine, generate a DOM from the yaep tree.
        generate_dom_from_yaep_node(doc->dom_, NULL, run->root, NULL, 0, 0);
        doc->root_ = xml_dom_root_element(doc->dom_);
        ok = true;
        if (ixml_dynamic_error_)
        {
            // A dynamic error in the serialization, eg a duplicate attribute (D02).
            // The output is a failure document like for a syntax error.
            XmlDom *new_doc = xml_dom_new();
            if (!(flags & XMQ_FLAG_IXML_FAIL_SILENT))
            {
                XMQNode *root = xml_node_new_element(new_doc, "ixml", NULL);
                xml_dom_set_root_element(new_doc, root);
                XmlNs *ns = xml_ns_declare(root, "ixml", "http://invisiblexml.org/NS");
                xml_node_set_attr(root, "state", ns, "failed");
                add_key_string(new_doc, root, "info", ixml_dynamic_error_);
            }
            xml_dom_free(doc->dom_);
            xmqSetImplementationDoc(doc, new_doc);
            free(ixml_dynamic_error_);
            ixml_dynamic_error_ = NULL;
            run->ambiguous_p = false;
            ok = false;
        }
    }

    if (run->ambiguous_p)
    {
        XMQNode *element = xml_dom_root_element(doc->dom_);
        if (element)
        {
            XmlNs *ns = xml_ns_lookup_prefix(element, "ixml");
            if (ns == NULL)
            {
                ns = xml_ns_declare(element, "ixml", "http://invisiblexml.org/NS");
            }
            // This should be set with the namespace ixml, kept without namespace
            // as before (the libxml2 version leaked with xmlSetNsProp).
            xml_node_set_attr(element, "state", NULL, "ambiguous");
        }
    }

    if (run->root)
    {
        yaepFreeTree(run->root, NULL, NULL);
        run->root = NULL;
    }

    return ok;
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

char *xmqCompactQuote(const char *content)
{
    MemBuffer *mb = new_membuffer();

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

    XmlDom *tmp = xml_dom_new();
    XMQNode *node = xml_node_new_text(tmp, content);
    print_value(&ps , node, NULL, NULL, LEVEL_ELEMENT_VALUE, false);
    xml_node_free(node);
    xml_dom_free(tmp);

    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}


XMQLineConfig *xmqNewLineConfig()
{
    XMQLineConfig *lc = (XMQLineConfig*)malloc(sizeof(XMQLineConfig));
    memset(lc, 0, sizeof(XMQLineConfig));
    return lc;
}

void xmqFreeLineConfig(XMQLineConfig *lc)
{
    free(lc);
}

void xmqSetLineHumanReadable(XMQLineConfig *lc, bool enabled)
{
    lc->human_readable_ = enabled;
}

char *xmqLineDoc(XMQLineConfig *lc, XMQDoc *doc)
{
    XMQOutputSettings *settings = xmqNewOutputSettings();
    xmqSetCompact(settings, true);
    xmqSetEscapeNewlines(settings, true);
    xmqSetColorMode(settings, XMQ_MONO);
    xmqSetOutputFormat(settings, XMQ_CONTENT_XMQ);
    xmqSetRenderFormat(settings, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(settings, &start, &stop);
    xmqPrint(doc, settings);

    xmqFreeOutputSettings(settings);

    return start;
}

char *xmq_line_vprintf_hr(XMQLineConfig *lc, const char *element_name, va_list ap);
char *xmq_line_vprintf_hr(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    // Lets check that last character. We can have:
    // 1) A complete xmq object.
    //    debug("xmq{", "alfa=", "%d", 42, "}");
    //    logs as: (xmq) alfa=42
    // 2) A single string.
    //    debug("xmq=", "size=%zu name=%s", the_size, the_name);
    //    logs as: (xmq) size=42 name=info
    // 3) Forgot to indicate { or =.
    //    debug("Fix me!");
    //    logs as: (log) Fix me!

    char c = element_name[strlen(element_name)-1];

    char module[256];
    snprintf(module, 256, "(%.*s) ", (int)strlen(element_name)-1, element_name);

    if (c != '{')
    {
        MemBuffer *mb = new_membuffer();
        const char *format;
        if ( c != '=')
        {
            // User forgot to indicate either { or =.
            format = element_name;
            membuffer_append(mb, "(log) ");
        }
        else
        {
            format = va_arg(ap, const char *);
        }
        char *buf = buf_vsnprintf(format, ap);
        membuffer_append(mb, buf);
        free(buf);
        membuffer_prefix_lines(mb, module);
        membuffer_append_null(mb);
        return free_membuffer_but_return_trimmed_content(mb);
    }

    MemBuffer *mb = new_membuffer();

    char *buf = (char*)malloc(1024);
    size_t buf_size = 1024;
    memset(buf, 0, buf_size);

    bool first = true;

    for (;;)
    {
        const char *key = va_arg(ap, const char*);
        if (!key) break;
        if (*key == '}') break;

        size_t kl = strlen(key);

        if (!first)
        {
            membuffer_append(mb, " ");
            first = false;
        }

        if (key[kl-1] != '=')
        {
            warning("xmq.warn=", "Warning erroneous api usage! Line printf key \"%s\" does not end with = sign!", key);
            break;
        }

        membuffer_append_region(mb, key, key+kl-1);
        membuffer_append(mb, ":");

        const char *format = va_arg(ap, const char *);
        for (;;)
        {
            size_t n = vsnprintf(buf, buf_size, format, ap);

            if (n < buf_size)
            {
                // The generated output fitted in the allocated buffer.
                break;
            }

            buf_size *= 2;
            free(buf);
            buf = (char*)malloc(buf_size);
            memset(buf, 0, buf_size);
        }
        membuffer_append(mb, buf);
    }

    free(buf);

    membuffer_prefix_lines(mb, module);
    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}

char *xmq_line_vprintf_xmq(XMQLineConfig *lc, const char *element_name, va_list ap);
char *xmq_line_vprintf_xmq(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    // Lets check that last character. We can have:
    // 1) A complete xmq object.
    //    debug("xmq.cli{", "alfa=", "%d", 42, "}");
    //    logs as: xmq.cli{alfa=42}
    // 2) A single string.
    //    debug("xmq.cli=", "size=%zu name=%s", the_size, the_name);
    //    logs as: xmq.cli='size=42 name=info'
    // 3) Forgot to indicate { or =.
    //    debug("Fix me!");
    //    logs as: log='Fix me!'

    char c = element_name[strlen(element_name)-1];

    if (c != '{')
    {
        MemBuffer *mb = new_membuffer();
        const char *format;
        if ( c != '=')
        {
            // User forgot to indicate either { or =.
            format = element_name;
            membuffer_append(mb, "log=");
        }
        else
        {
            format = va_arg(ap, const char *);
            membuffer_append(mb, element_name);
        }
        char *buf = buf_vsnprintf(format, ap);
        char *q = xmqCompactQuote(buf);
        free(buf);
        membuffer_append(mb, q);
        membuffer_append_null(mb);
        return free_membuffer_but_return_trimmed_content(mb);
    }

    MemBuffer *mb = new_membuffer();
    membuffer_append(mb, element_name);

    char *buf = (char*)malloc(1024);
    size_t buf_size = 1024;
    memset(buf, 0, buf_size);

    for (;;)
    {
        const char *key = va_arg(ap, const char*);
        if (!key) break;
        if (*key == '}') break;

        size_t kl = strlen(key);
        char last = membuffer_back(mb);
        if (last != '"' && last != '\'' && last != '(' && last != ')'  && last != '{' && last != '}')
        {
            membuffer_append(mb, " ");
        }

        if (key[kl-1] != '=')
        {
            warning("xmq.warn=", "Warning erroneous api usage! Line printf key \"%s\" does not end with = sign!", key);
            break;
        }

        // The key has an = sign at the end. E.g. size=
        membuffer_append(mb, key);
        const char *format = va_arg(ap, const char *);
        for (;;)
        {
            size_t n = vsnprintf(buf, buf_size, format, ap);

            if (n < buf_size)
            {
                // The generated output fitted in the allocated buffer.
                break;
            }

            buf_size *= 2;
            free(buf);
            buf = (char*)malloc(buf_size);
            memset(buf, 0, buf_size);
        }

        char *quote = xmqCompactQuote(buf);
        membuffer_append(mb, quote);
        free(quote);
    }

    free(buf);
    membuffer_append(mb, "}");
    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}

char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap)
{
    if (lc->human_readable_)
    {
        return xmq_line_vprintf_hr(lc, element_name, ap);
    }
    return xmq_line_vprintf_xmq(lc, element_name, ap);
}

char *xmqLinePrintf(XMQLineConfig *lc, const char *element_name, ...)
{
    va_list ap;
    va_start(ap, element_name);

    char *v = xmqLineVPrintf(lc, element_name, ap);

    va_end(ap);

    return v;
}

/////////////////////////////////////////////////////////////////////////////////////
// XSLT 1.0, the native port of libxslt (parts/xml_libxslt1_*).

struct XMQXslt
{
    XmlLibxslt1Stylesheet *style;
    XMQXsltSettings settings;
    MemBuffer *messages;      // The messages of the current compilation or transformation.
    char **params;            // name, xpath value pairs, NULL terminated.
    size_t num_params;
};

static void xmq_xslt_message(void *user_data, const char *message)
{
    XMQXslt *x = (XMQXslt*)user_data;
    if (x->messages) membuffer_append(x->messages, message);
    if (x->settings.message_callback) x->settings.message_callback(x->settings.message_user_data, message);
    else fputs(message, stderr);
}

/** The native dom of an xmq document, taken over (the document is left empty). */
static XmlDom *xmq_take_dom(XMQDoc *doq, const char *url)
{
    XmlDom *dom = (XmlDom*)xmqGetImplementationDoc(doq);
    if (!dom) return NULL;
    xmqSetImplementationDoc(doq, NULL);
    if (url && !xml_dom_url(dom)) xml_dom_set_url(dom, url);
    return dom;
}

static XmlDom *xmq_xslt_loader(void *user_data, const char *uri, XmlLibxslt1LoadType type)
{
    XMQXslt *x = (XMQXslt*)user_data;
    XMQDoc *doq = NULL;
    // document('') of a stylesheet without url: the stylesheet itself.
    if (type == XML_LIBXSLT1_LOAD_DOCUMENT && (!uri || !*uri) && x->style) return xml_libxslt1_copy_stylesheet_doc(x->style);
    if (x->settings.loader)
    {
        doq = x->settings.loader(x->settings.loader_user_data, uri);
    }
    else
    {
        const char *path = uri;
        if (!strncmp(path, "file://", 7))
        {
            path += 7;
            if (!strncmp(path, "localhost/", 10)) path += 9;
        }
        XMQReturnDoc rd = xmqNewDoc();
        doq = rd.doc;
        // Like libxml2, an uri reference with %XX escapes is a file name once unescaped.
        char *unescaped = NULL;
        if (!strchr(path, ':') && strchr(path, '%'))
        {
            FILE *f = fopen(path, "rb");
            if (f) fclose(f);
            else unescaped = xml_uri_unescape_string(path, -1, NULL);
        }
        bool parsed = xmqParseFileWithType(doq, unescaped ? unescaped : path, NULL, XMQ_CONTENT_DETECT, XMQ_FLAG_TRIM_NONE);
        free(unescaped);
        if (!parsed)
        {
            const char *error = xmqDocError(doq);
            if (error) xmq_xslt_message(x, error);
            xmqFreeDoc(doq);
            return NULL;
        }
    }
    if (!doq) return NULL;
    XmlDom *dom = xmq_take_dom(doq, uri);
    if (x->settings.loader) xml_dom_set_url(dom, uri);
    xmqFreeDoc(doq);
    return dom;
}

static void xmq_set_doc_error(XMQDoc *doq, MemBuffer *mb)
{
    if (!doq || !mb) return;
    membuffer_append_null(mb);
    char *msg = free_membuffer_but_return_trimmed_content(mb);
    if (!*msg)
    {
        free(msg);
        return;
    }
    free((void*)doq->error_);
    doq->error_ = msg;
}

XMQReturnXslt xmqCompileXslt(XMQDoc *stylesheet, XMQXsltSettings *settings)
{
    XMQReturnXslt ret = { XMQ_ERROR_XSLT_COMPILE, NULL };
    XmlDom *src = stylesheet ? (XmlDom*)xmqGetImplementationDoc(stylesheet) : NULL;
    if (!src) return ret;
    XMQXslt *x = (XMQXslt*)calloc(1, sizeof(XMQXslt));
    if (settings) x->settings = *settings;
    // A copy of the stylesheet, the compiler changes it.
    XmlDom *dom = xml_dom_new();
    if (xml_dom_version(src)) xml_dom_set_version(dom, xml_dom_version(src));
    if (xml_dom_encoding(src)) xml_dom_set_encoding(dom, xml_dom_encoding(src));
    // Relative uris in the stylesheet (import, include, document()) are resolved against the
    // url of its dom, without one (xmq does not set it) against the current directory.
    if (xml_dom_url(src)) xml_dom_set_url(dom, xml_dom_url(src));
    for (XMQNode *c = xml_node_first_child(xml_dom_document(src)); c; c = xml_node_next(c))
    {
        XMQNode *copy = xml_node_copy(dom, c, true);
        if (copy) xml_node_append_child(xml_dom_document(dom), copy);
    }
    for (XMQNode *c = xml_node_first_child(xml_dom_document(dom)); c; c = xml_node_next(c))
    {
        if (xml_node_kind(c) == XML_DOM_ELEMENT) xml_node_reconcile_ns(c);
    }
    XmlLibxslt1Settings xs;
    xml_libxslt1_settings_default(&xs);
    xs.message_callback = xmq_xslt_message;
    xs.message_user_data = x;
    xs.loader = xmq_xslt_loader;
    xs.loader_user_data = x;
    x->messages = new_membuffer();
    x->style = xml_libxslt1_compile(dom, &xs);
    if (!x->style)
    {
        xml_dom_free(dom);
        xmq_set_doc_error(stylesheet, x->messages);
        x->messages = NULL;
        free(x);
        return ret;
    }
    free_membuffer_and_free_content(x->messages);
    x->messages = NULL;
    ret.status = XMQ_OK;
    ret.xslt = x;
    return ret;
}

void xmqSetXsltParam(XMQXslt *xslt, const char *name, const char *value, bool is_xpath)
{
    if (!xslt || !name || !value) return;
    char *v;
    if (is_xpath)
    {
        v = strdup(value);
    }
    else
    {
        // A string literal, like xsltQuoteUserParams (concat() when both quotes are used).
        if (!strchr(value, '\''))
        {
            v = (char*)malloc(strlen(value) + 3);
            sprintf(v, "'%s'", value);
        }
        else if (!strchr(value, '"'))
        {
            v = (char*)malloc(strlen(value) + 3);
            sprintf(v, "\"%s\"", value);
        }
        else
        {
            MemBuffer *mb = new_membuffer();
            membuffer_append(mb, "concat('");
            for (const char *p = value; *p; p++)
            {
                if (*p == '\'') membuffer_append(mb, "', \"'\", '");
                else membuffer_append_char(mb, *p);
            }
            membuffer_append(mb, "')");
            membuffer_append_null(mb);
            v = free_membuffer_but_return_trimmed_content(mb);
        }
    }
    // Replace an earlier value of the same parameter.
    for (size_t i = 0; i < xslt->num_params; i++)
    {
        if (!strcmp(xslt->params[2 * i], name))
        {
            free(xslt->params[2 * i + 1]);
            xslt->params[2 * i + 1] = v;
            return;
        }
    }
    xslt->params = (char**)realloc(xslt->params, (2 * xslt->num_params + 3) * sizeof(char*));
    xslt->params[2 * xslt->num_params] = strdup(name);
    xslt->params[2 * xslt->num_params + 1] = v;
    xslt->num_params++;
    xslt->params[2 * xslt->num_params] = NULL;
}

XMQReturnDoc xmqTransform(XMQXslt *xslt, XMQDoc *doc)
{
    XMQReturnDoc ret = { XMQ_ERROR_XSLT_TRANSFORM, NULL };
    XmlDom *source = doc ? (XmlDom*)xmqGetImplementationDoc(doc) : NULL;
    if (!xslt || !source) return ret;
    xslt->messages = new_membuffer();
    int type = XML_LIBXSLT1_OUTPUT_XML;
    XmlDom *res = xml_libxslt1_apply_stylesheet(xslt->style, source, (const char**)xslt->params, &type);
    if (!res)
    {
        xmq_set_doc_error(doc, xslt->messages);
        xslt->messages = NULL;
        return ret;
    }
    free_membuffer_and_free_content(xslt->messages);
    xslt->messages = NULL;
    XMQReturnDoc rd = xmqNewDoc();
    XmlDom *old = (XmlDom*)xmqGetImplementationDoc(rd.doc);
    xmqSetImplementationDoc(rd.doc, res);
    if (old) xml_dom_free(old);
    rd.doc->original_content_type_ = type == XML_LIBXSLT1_OUTPUT_HTML ? XMQ_CONTENT_HTML :
                                     type == XML_LIBXSLT1_OUTPUT_TEXT ? XMQ_CONTENT_TEXT : XMQ_CONTENT_XML;
    ret.status = XMQ_OK;
    ret.doc = rd.doc;
    return ret;
}

char *xmqSaveXsltResult(XMQXslt *xslt, XMQDoc *result, size_t *len)
{
    if (!xslt || !result) return NULL;
    XmlDom *dom = (XmlDom*)xmqGetImplementationDoc(result);
    XMQContentType ct = result->original_content_type_;
    int type = ct == XMQ_CONTENT_HTML ? XML_LIBXSLT1_OUTPUT_HTML : ct == XMQ_CONTENT_TEXT ? XML_LIBXSLT1_OUTPUT_TEXT : XML_LIBXSLT1_OUTPUT_XML;
    return xml_libxslt1_save_result_to_string(dom, xslt->style, type, len);
}

void xmqFreeXslt(XMQXslt *xslt)
{
    if (!xslt) return;
    xml_libxslt1_stylesheet_free(xslt->style);
    for (size_t i = 0; i < 2 * xslt->num_params; i++) free(xslt->params[i]);
    free(xslt->params);
    if (xslt->messages) free_membuffer_and_free_content(xslt->messages);
    free(xslt);
}

// XSD 1.0 ////////////////////////////////////////////////////////////////////////

struct XMQXsd
{
    XmlXsd10Schema *schema;
    XmlDom *dom;               // The copy of the schema document, used by the schema.
    XMQXsdSettings settings;
    MemBuffer *messages;       // The messages of the current compilation or validation.
};

static void xmq_xsd_deliver(XMQXsd *x, const char *message, bool is_warning)
{
    if (x->messages) membuffer_append(x->messages, message);
    if (x->settings.message_callback) x->settings.message_callback(x->settings.message_user_data, message, is_warning);
}

static void xmq_xsd_vdeliver(XMQXsd *x, bool is_warning, const char *fmt, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) return;
    char *buf = (char*)malloc((size_t)n + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    xmq_xsd_deliver(x, buf, is_warning);
    free(buf);
}

/** The plain libxml2 style callbacks, called as cb(ctx, "%s", message). */
static void xmq_xsd_error(void *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xmq_xsd_vdeliver((XMQXsd*)ctx, false, fmt, ap);
    va_end(ap);
}

static void xmq_xsd_warning(void *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xmq_xsd_vdeliver((XMQXsd*)ctx, true, fmt, ap);
    va_end(ap);
}

/** Without a message callback the messages are printed like libxml2 prints them without
    callbacks (xmlFormatError on stderr), and collected for xmqDocError. */
static void xmq_xsd_structured(void *ctx, const XmlXsd10Error *err)
{
    XMQXsd *x = (XMQXsd*)ctx;
    char *m = xml_xsd10_format_error_string(err);
    if (!m) return;
    fputs(m, stderr);
    if (x->messages) membuffer_append(x->messages, m);
    free(m);
}

static XmlDom *xmq_xsd_loader(void *user_data, const char *uri)
{
    XMQXsd *x = (XMQXsd*)user_data;
    XMQDoc *doq = x->settings.loader(x->settings.loader_user_data, uri);
    if (!doq) return NULL;
    XmlDom *dom = xmq_take_dom(doq, uri);
    xmqFreeDoc(doq);
    return dom;
}

XMQReturnXsd xmqCompileXsd(XMQDoc *schema, XMQXsdSettings *settings)
{
    XMQReturnXsd ret = { XMQ_ERROR_XSD_COMPILE, NULL };
    XmlDom *src = schema ? (XmlDom*)xmqGetImplementationDoc(schema) : NULL;
    if (!src) return ret;
    XMQXsd *x = (XMQXsd*)calloc(1, sizeof(XMQXsd));
    if (settings) x->settings = *settings;
    // A copy of the schema document, it must live as long as the schema.
    x->dom = xml_dom_new();
    if (xml_dom_version(src)) xml_dom_set_version(x->dom, xml_dom_version(src));
    if (xml_dom_encoding(src)) xml_dom_set_encoding(x->dom, xml_dom_encoding(src));
    if (xml_dom_url(src)) xml_dom_set_url(x->dom, xml_dom_url(src));
    for (XMQNode *c = xml_node_first_child(xml_dom_document(src)); c; c = xml_node_next(c))
    {
        XMQNode *copy = xml_node_copy(x->dom, c, true);
        if (copy) xml_node_append_child(xml_dom_document(x->dom), copy);
    }
    x->messages = new_membuffer();
    XmlXsd10ParserCtxt *pctxt = xml_xsd10_new_doc_parser_ctxt(x->dom);
    if (pctxt)
    {
        if (x->settings.message_callback) xml_xsd10_set_parser_errors(pctxt, xmq_xsd_error, xmq_xsd_warning, x);
        else xml_xsd10_set_parser_structured_errors(pctxt, xmq_xsd_structured, x);
        if (x->settings.loader) xml_xsd10_set_resource_loader(pctxt, xmq_xsd_loader, x);
        x->schema = xml_xsd10_parse(pctxt);
        xml_xsd10_free_parser_ctxt(pctxt);
    }
    if (!x->schema)
    {
        xmq_set_doc_error(schema, x->messages);
        x->messages = NULL;
        xml_dom_free(x->dom);
        free(x);
        return ret;
    }
    free_membuffer_and_free_content(x->messages);
    x->messages = NULL;
    ret.status = XMQ_OK;
    ret.xsd = x;
    return ret;
}

void xmqSetXsdMessageCallback(XMQXsd *xsd, XMQXsdMessageCallback callback, void *user_data)
{
    if (!xsd) return;
    xsd->settings.message_callback = callback;
    xsd->settings.message_user_data = user_data;
}

XMQStatus xmqValidate(XMQXsd *xsd, XMQDoc *doc)
{
    XmlDom *dom = doc ? (XmlDom*)xmqGetImplementationDoc(doc) : NULL;
    if (!xsd || !xsd->schema) return XMQ_ERROR_XSD_COMPILE;
    if (!dom) return XMQ_ERROR_XSD_INVALID;
    XmlXsd10ValidCtxt *vctxt = xml_xsd10_new_valid_ctxt(xsd->schema);
    if (!vctxt) return XMQ_ERROR_XSD_COMPILE;
    xsd->messages = new_membuffer();
    if (xsd->settings.message_callback) xml_xsd10_set_valid_errors(vctxt, xmq_xsd_error, xmq_xsd_warning, xsd);
    else xml_xsd10_set_valid_structured_errors(vctxt, xmq_xsd_structured, xsd);
    if (!xml_xsd10_is_valid(vctxt))
    {
        xml_xsd10_free_valid_ctxt(vctxt);
        free_membuffer_and_free_content(xsd->messages);
        xsd->messages = NULL;
        return XMQ_ERROR_XSD_COMPILE;
    }
    int rc = xml_xsd10_validate_doc(vctxt, dom);
    xml_xsd10_free_valid_ctxt(vctxt);
    if (rc != 0)
    {
        xmq_set_doc_error(doc, xsd->messages);
        xsd->messages = NULL;
        return XMQ_ERROR_XSD_INVALID;
    }
    free_membuffer_and_free_content(xsd->messages);
    xsd->messages = NULL;
    return XMQ_OK;
}

void xmqFreeXsd(XMQXsd *xsd)
{
    if (!xsd) return;
    xml_xsd10_free(xsd->schema);
    if (xsd->dom) xml_dom_free(xsd->dom);
    if (xsd->messages) free_membuffer_and_free_content(xsd->messages);
    free(xsd);
}

#ifdef BUILDING_DIST_XMQ

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
#include"parts/yaml.c"
#include"parts/text.c"
#include"parts/lang.c"
#include"parts/utf8.c"
#include"parts/vector.c"
#include"parts/xml_chars.c"
#include"parts/xml_dom.c"
#include"parts/xml_parser.c"
#include"parts/xml_save.c"
#include"parts/xml_html.c"
#include"parts/xml_html_parser.c"
#include"parts/xml_html_save.c"
#include"parts/xml_xpath.c"
#include"parts/xml_uri.c"
#include"parts/xml_unicode.c"
#include"parts/xml_regexp.c"
#include"parts/xml_pattern.c"
#include"parts/xml_xsd10_types.c"
#include"parts/xml_xsd10_schemas.c"
#include"parts/xml_xsd10_parse.c"
#include"parts/xml_xsd10_build.c"
#include"parts/xml_xsd10_valid.c"
#include"parts/xml_libxslt1_xslt.c"
#include"parts/xml_libxslt1_transform.c"
#include"parts/xml.c"
#include"parts/xmq_internals.c"
#include"parts/xmq_parser.c"
#include"parts/xmq_printer.c"
#include"parts/yaep_allocate.c"
#include"parts/yaep_cspc.c"
#include"parts/yaep_hashtab.c"
#include"parts/yaep_objstack.c"
#include"parts/yaep_vlobject.c"
#include"parts/yaep_util.c"
#include"parts/yaep_symbols.c"
#include"parts/yaep_terminal_bitset.c"
#include"parts/yaep_tree.c"
#include"parts/yaep_print.c"
#include"parts/yaep.c"

#endif
