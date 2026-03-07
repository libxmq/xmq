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

#ifndef XMQ_PRINTER_H
#define XMQ_PRINTER_H

struct XMQPrintState;
typedef struct XMQPrintState XMQPrintState;

#ifdef __cplusplus
enum Level : short;
#else
enum Level;
#endif
typedef enum Level Level;

void annotate_offsets(xmlDoc *doc, const char *attribute_name, const char *ns);
int count_necessary_quotes(const char *start, const char *stop, bool *add_nls, bool *add_compound, bool prefer_double_quotes, bool *use_double_quotes);
size_t count_necessary_slashes(const char *start, const char *stop);

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align);
void print_content_node(XMQPrintState *ps, xmlNode *node);
void print_entity_node(XMQPrintState *ps, xmlNode *node);
void print_color_post(XMQPrintState *ps, XMQColor c);
void print_color_pre(XMQPrintState *ps, XMQColor c);
void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_node(XMQPrintState *ps, xmlNode *node);
size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node);
void print_leaf_node(XMQPrintState *ps,
                     xmlNode *node);
void print_key_node(XMQPrintState *ps,
                    xmlNode *node,
                    size_t align);
void print_element_with_children(XMQPrintState *ps,
                                 xmlNode *node,
                                 size_t align);
void print_doctype(XMQPrintState *ps, xmlNode *node);
void print_pi_node(XMQPrintState *ps, xmlNode *node);
void print_node(XMQPrintState *ps, xmlNode *node, size_t align);

void print_white_spaces(XMQPrintState *ps, int num);
void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num);
void print_quoted_spaces(XMQPrintState *ps, XMQColor color, int num);
void print_quotes(XMQPrintState *ps, int num, XMQColor color, bool use_double_quotes);
void print_double_quote(XMQPrintState *ps, XMQColor color);
void print_nl(XMQPrintState *ps, const char *prefix, const char *postfix);
void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix);
size_t print_char_entity(XMQPrintState *ps, XMQColor color, const char *start, const char *stop);
void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n);


bool need_separation_before_attribute_key(XMQPrintState *ps);
bool need_separation_before_entity(XMQPrintState *ps);
bool need_separation_before_element_name(XMQPrintState *ps);
bool need_separation_before_quote(XMQPrintState *ps);
bool need_separation_before_comment(XMQPrintState *ps);
void check_space_before_attribute(XMQPrintState *ps);
void check_space_before_entity_node(XMQPrintState *ps);
void check_space_before_quote(XMQPrintState *ps, Level level);
void check_space_before_key(XMQPrintState *ps);
void check_space_before_opening_brace(XMQPrintState *ps);
void check_space_before_closing_brace(XMQPrintState *ps);
void check_space_before_comment(XMQPrintState *ps);

void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align);
void print_namespace_declaration(XMQPrintState *ps, xmlNs *ns, size_t align);
void print_attributes(XMQPrintState *ps, xmlNodePtr node);

void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps,
                                             XMQColor color,
                                             const char *start,
                                             const char *stop);
void print_safe_leaf_quote(XMQPrintState *ps,
                           XMQColor c,
                           const char *start,
                           const char *stop);
const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop);
const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop);
void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level);
bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop);
void print_value(XMQPrintState *ps, xmlNode *node, Level level);

#define XMQ_PRINTER_MODULE

#endif // XMQ_PRINTER_H
