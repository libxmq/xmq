#ifndef BUILDING_XMQ

#include"always.h"
#include"text.h"
#include"parts/xmq_internals.h"
#include"xmq_printer.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef XMQ_PRINTER_MODULE

bool xml_has_non_empty_namespace_defs(xmlNode *node);
bool xml_non_empty_namespace(xmlNs *ns);
const char *toHtmlEntity(int uc);

/**
    count_necessary_quotes:
    @start: Points to first byte of memory buffer to scan for quotes.
    @stop:  Points to byte after memory buffer.
    @forbid_nl: Compact mode.
    @add_nls: Returns whether we need leading and ending newlines.
    @add_compound: Compounds ( ) is necessary.

    Scan the content to determine how it must be quoted, or if the content can
    remain as text without quotes. Return 0, nl_begin=nl_end=false for safe text.
    Return 1,3 or more for unsafe text with at most a single quote '.
    If forbid_nl is true, then we are generating xmq on a single line.
    Set add_nls to true if content starts or end with a quote and forbid_nl==false.
    Set add_compound to true if content starts or ends with spaces/newlines or if forbid_nl==true and
    content starts/ends with quotes.
*/
size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound)
{
    size_t max = 0;
    size_t curr = 0;
    bool all_safe = true;

    assert(stop > start);

    if (unsafe_start(*start, start+1 < stop ? *(start+1):0))
    {
        // Content starts with = & // or /* so it must be quoted.
        all_safe = false;
    }

    if (*start == '\'' || *(stop-1) == '\'')
    {
        // If leading or ending quote, then add newlines both at the beginning and at the end.
        // Strictly speaking, if only a leading quote, then only newline at beginning is needed.
        // However to reduce visual confusion, we add newlines at beginning and end.
        if (!compact)
        {
            // We quote this using:
            // '''
            // 'howdy'
            // '''
            *add_nls = true;
        }
        else
        {
            // We quote this using:
            // ( &#39; 'howdy' &#39; )
            *add_compound = true;
        }
    }

    if (has_leading_space_nl(start, stop) || has_ending_nl_space(start, stop))
    {
        // Leading ending ws + nl, nl + ws will be trimmed, so we need a compound and entities.
        *add_compound = true;
    }

    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\'')
        {
            curr++;
            if (curr > max) max = curr;
        }
        else
        {
            curr = 0;
            all_safe &= is_safe_char(i, stop);
        }
    }
    // We found 3 quotes, thus we need 4 quotes to quote them.
    if (max > 0) max++;
    // Content contains no quotes ', but has unsafe chars, a single quote is enough.
    if (max == 0 && !all_safe) max = 1;
    // Content contains two sequential '' quotes, must bump nof required quotes to 3.
    // Since two quotes means the empty string.
    if (max == 2) max = 3;
    return max;
}

/**
    count_necessary_slashes:
    @start: Start of buffer in which to count the slashes.
    @stop: Points to byte after buffer.

    Scan the comment to determine how it must be commented.
    If the comment contains asterisk plus slashes, then find the max num
    slashes after an asterisk. The returned value is 1 + this max.
*/
size_t count_necessary_slashes(const char *start, const char *stop)
{
    int max = 0;
    int curr = 0;
    bool counting = false;

    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (counting)
        {
            if (c == '/')
            {
                curr++;
                if (curr > max) max = curr;
            }
            else
            {
                counting = false;
            }
        }

        if (!counting)
        {
            if (c == '*')
            {
                counting = true;
                curr = 0;
            }
        }
    }
    return max+1;
}

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align)
{
    xmlNode *i = from;
    xmlNode *restart_find_at_node = from;
    size_t max = 0;

    while (i)
    {
        // We need to search ahead to find the max width of the node names so that we can align the equal signs.
        if (!ps->output_settings->compact && i == restart_find_at_node)
        {
            max = find_element_key_max_width(i, &restart_find_at_node);
        }

        print_node(ps, i, max);
        i = xml_next_sibling(i);
    }
}

void print_content_node(XMQPrintState *ps, xmlNode *node)
{
    print_value(ps, node, LEVEL_XMQ);
}

void print_entity_node(XMQPrintState *ps, xmlNode *node)
{
    check_space_before_entity_node(ps);

    print_utf8(ps, COLOR_entity, 1, "&", NULL);
    print_utf8(ps, COLOR_entity, 1, (const char*)node->name, NULL);
    print_utf8(ps, COLOR_entity, 1, ";", NULL);
}

void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact)
{
    print_utf8(ps, COLOR_comment, 1, start, stop);
}

void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact)
{
    const char *i = start;
    const char *line = i;

    size_t num_slashes = count_necessary_slashes(start, stop);

    print_slashes(ps, NULL, "*", num_slashes);
    size_t add_spaces = ps->current_indent + 1 + num_slashes;
    if (!compact) {
        if (*i != '\n') print_white_spaces(ps, 1);
        add_spaces++;
    }

    size_t prev_line_indent = ps->line_indent;
    ps->line_indent = add_spaces;

    for (; i < stop; ++i)
    {
        if (*i == '\n')
        {
            if (line > start) {
                if (compact) {
                    print_slashes(ps, "*", "*", num_slashes);
                }
                else
                {
                    if (*(i-1) == 10 && *(i+1) != 0)
                    {
                        // This is an empty line. Do not indent.
                        // Except the last line which must be indented.
                        print_nl(ps, NULL, NULL);
                    }
                    else
                    {
                        print_nl_and_indent(ps, NULL, NULL);
                    }
                }
            }
            print_comment_line(ps, line, i, compact);
            line = i+1;
        }
    }
    if (line == start)
    {
        // No newlines found.
        print_comment_line(ps, line, i, compact);
    }
    else if (line < stop)
    {
        // There is a remaining line that ends with stop and not newline.
        if (line > start) {
            if (compact) {
                print_slashes(ps, "*", "*", num_slashes);
            }
            else
            {
                print_nl_and_indent(ps, NULL, NULL);
            }
        }
        print_comment_line(ps, line, i, compact);
    }
    if (!compact) print_white_spaces(ps, 1);
    print_slashes(ps, "*", NULL, num_slashes);
    ps->last_char = '/';
    ps->line_indent = prev_line_indent;
}

void print_comment_node(XMQPrintState *ps, xmlNode *node)
{
    const char *comment = xml_element_content(node);
    const char *start = comment;
    const char *stop = comment+strlen(comment);

    check_space_before_comment(ps);

    bool has_newline = has_newlines(start, stop);
    if (!has_newline)
    {
        if (ps->output_settings->compact)
        {
            print_utf8(ps, COLOR_comment, 3, "/*", NULL, start, stop, "*/", NULL);
            ps->last_char = '/';
        }
        else
        {
            print_utf8(ps, COLOR_comment, 2, "// ", NULL, start, stop);
            ps->last_char = 1;
        }
    }
    else
    {
        print_comment_lines(ps, start, stop, ps->output_settings->compact);
        ps->last_char = '/';
    }
}

size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node)
{
    size_t name_len, prefix_len, total_u_len;
    const char *name;
    const char *prefix;

    check_space_before_key(ps);

    node_strlen_name_prefix(node, &name, &name_len, &prefix, &prefix_len, &total_u_len);

    if (prefix)
    {
        print_utf8(ps, COLOR_element_ns, 1, prefix, NULL);
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
    }

    if (is_key_value_node(node) && !xml_first_attribute(node))
    {
        // Only print using key color if = and no attributes.
        // I.e. alfa=1
        print_utf8(ps, COLOR_element_key, 1, name, NULL);
    }
    else
    {
        // All other cases print with node color.
        // I.e. alfa{a b} alfa(x=1)=1
        print_utf8(ps, COLOR_element_name, 1, name, NULL);
    }

    bool has_non_empty_ns = xml_has_non_empty_namespace_defs(node);

    if (xml_first_attribute(node) || has_non_empty_ns)
    {
        print_utf8(ps, COLOR_apar_left, 1, "(", NULL);
        print_attributes(ps, node);
        print_utf8(ps, COLOR_apar_right, 1, ")", NULL);
    }

    return total_u_len;
}

void print_leaf_node(XMQPrintState *ps,
                     xmlNode *node)
{
    print_element_name_and_attributes(ps, node);
}

void print_key_node(XMQPrintState *ps,
                    xmlNode *node,
                    size_t align)
{
    print_element_name_and_attributes(ps, node);

    if (!ps->output_settings->compact)
    {
        size_t len = ps->current_indent - ps->line_indent;
        size_t pad = 1;
        if (len < align) pad = 1+align-len;
        print_white_spaces(ps, pad);
    }
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    print_value(ps, xml_first_child(node), LEVEL_ELEMENT_VALUE);
}

void print_element_with_children(XMQPrintState *ps,
                                 xmlNode *node,
                                 size_t align)
{
    print_element_name_and_attributes(ps, node);

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    check_space_before_opening_brace(ps);
    print_utf8(ps, COLOR_brace_left, 1, "{", NULL);

    ps->line_indent += ps->output_settings->add_indent;

    while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
    assert(from != NULL);

    print_nodes(ps, (xmlNode*)from, (xmlNode*)to, align);

    ps->line_indent -= ps->output_settings->add_indent;

    check_space_before_closing_brace(ps);
    print_utf8(ps, COLOR_brace_right, 1, "}", NULL);
}

void print_doctype(XMQPrintState *ps, xmlNode *node)
{
    if (!node) return;

    check_space_before_key(ps);
    print_utf8(ps, COLOR_element_key, 1, "!DOCTYPE", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, (xmlNodePtr)node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    if (ps->output_settings->compact)
    {
        size_t n = strlen(c);
        char *end = c+n;
        for (char *i = c; i < end; ++i)
        {
            if (*i == '\n') *i = ' ';
        }
    }
    print_value_internal_text(ps, c+10, c+strlen(c)-1, LEVEL_ELEMENT_VALUE);
    xmlBufferFree(buffer);
}

void print_pi_node(XMQPrintState *ps, xmlNode *node)
{
    if (!node) return;

    check_space_before_key(ps);
    size_t name_len = strlen((const char*)node->name);
    print_utf8(ps, COLOR_element_key, 2, "?", NULL, node->name, NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, (xmlNodePtr)node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    size_t n = strlen(c);
    // now detect if we need to add a leading/ending space.
    if (c[n-1] == '>' && c[n-2] == '?')
    {
        n-=2;
    }
    char *content = potentially_add_leading_ending_space(c+name_len+3, c+n);
    n = strlen(content);
    char *end = content+n;

    if (ps->output_settings->compact)
    {
        for (char *i = content; i < end; ++i)
        {
            if (*i == '\n') *i = ' ';
        }
    }

    print_value_internal_text(ps, content, end, LEVEL_ELEMENT_VALUE);

    free(content);
    xmlBufferFree(buffer);
}

void print_node(XMQPrintState *ps, xmlNode *node, size_t align)
{
    // Standalone quote must be quoted: 'word' 'some words'
    if (is_content_node(node))
    {
        return print_content_node(ps, node);
    }

    // This is an entity reference node. &something;
    if (is_entity_node(node))
    {
        return print_entity_node(ps, node);
    }

    // This is a comment // or /* ... */
    if (is_comment_node(node))
    {
        return print_comment_node(ps, node);
    }

    // This is a pi node ?something
    if (is_pi_node(node))
    {
        return print_pi_node(ps, node);
    }

    // This is doctype node.
    if (is_doctype_node(node))
    {
        return print_doctype(ps, node);
    }

    // This is a node with no children, ie br
    if (is_leaf_node(node))
    {
        return print_leaf_node(ps, node);
    }

    // This is a key = value or key = 'value value' node and there are no attributes.
    if (is_key_value_node(node))
    {
        return print_key_node(ps, node, align);
    }

    // All other nodes are printed
    return print_element_with_children(ps, node, align);
}

void print_white_spaces(XMQPrintState *ps, int num)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQColoring *c = os->default_coloring;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    if (c->whitespace.pre) write(writer_state, c->whitespace.pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->indentation_space, NULL);
    }
    ps->current_indent += num;
    if (c->whitespace.post) write(writer_state, c->whitespace.post, NULL);
}

void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level)
{
    const char *i = start;
    while (true)
    {
        if (i >= stop) break;
        if (*i == ' ')
        {
            const char *j = i;
            while (*j == ' ' && j < stop) j++;
            check_space_before_quote(ps, level);
            print_quoted_spaces(ps, level_to_quote_color(level), (size_t)(j-i));
            i += j-i;
        }
        else
        {
            check_space_before_entity_node(ps);
            print_char_entity(ps, level_to_entity_color(level), i, stop);
            i++;
        }
    }
}

void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre = NULL;
    const char *post = NULL;
    getColor(os, c, &pre, &post);

    write(writer_state, pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->explicit_space, NULL);
    }
    ps->current_indent += num;
    write(writer_state, post, NULL);
}

void print_quoted_spaces(XMQPrintState *ps, XMQColor color, int num)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQColoring *c = os->default_coloring;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (c->quote.pre) write(writer_state, c->quote.pre, NULL);
    write(writer_state, "'", NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->explicit_space, NULL);
    }
    ps->current_indent += num;
    ps->last_char = '\'';
    write(writer_state, "'", NULL);
    if (c->quote.post) write(writer_state, c->quote.post, NULL);
}

void print_quotes(XMQPrintState *ps, size_t num, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre = NULL;
    const char *post = NULL;
    getColor(os, color, &pre, &post);

    if (pre) write(writer_state, pre, NULL);
    for (size_t i=0; i<num; ++i)
    {
        write(writer_state, "'", NULL);
    }
    ps->current_indent += num;
    ps->last_char = '\'';
    if (post) write(writer_state, post, NULL);
}

void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (postfix) write(writer_state, postfix, NULL);
    write(writer_state, os->explicit_nl, NULL);
    ps->current_indent = 0;
    ps->last_char = 0;
    print_white_spaces(ps, ps->line_indent);
    if (ps->restart_line) write(writer_state, ps->restart_line, NULL);
    if (prefix) write(writer_state, prefix, NULL);
}

void print_nl(XMQPrintState *ps, const char *prefix, const char *postfix)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (postfix) write(writer_state, postfix, NULL);
    write(writer_state, os->explicit_nl, NULL);
    ps->current_indent = 0;
    ps->last_char = 0;
    if (ps->restart_line) write(writer_state, ps->restart_line, NULL);
    if (prefix) write(writer_state, prefix, NULL);
}

size_t print_char_entity(XMQPrintState *ps, XMQColor color, const char *start, const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    const char *pre, *post;
    getColor(os, color, &pre, &post);

    int uc = 0;
    size_t bytes = 0;
    if (decode_utf8(start, stop, &uc, &bytes))
    {
        // Max entity &#1114112; max buf is 11 bytes including terminating zero byte.
        char buf[32] = {};
        memset(buf, 0, sizeof(buf));

        const char *replacement = NULL;
        if (ps->output_settings->escape_non_7bit &&
            ps->output_settings->output_format == XMQ_CONTENT_HTMQ)
        {
            replacement = toHtmlEntity(uc);
        }

        if (replacement)
        {
            snprintf(buf, 32, "&%s;", replacement);
        }
        else
        {
            snprintf(buf, 32, "&#%d;", uc);
        }

        if (pre) write(writer_state, pre, NULL);
        print_utf8(ps, COLOR_none, 1, buf, NULL);
        if (post) write(writer_state, post, NULL);

        ps->last_char = ';';
        ps->current_indent += strlen(buf);
    }
    else
    {
        if (pre) write(writer_state, pre, NULL);
        write(writer_state, "&badutf8;", NULL);
        if (post) write(writer_state, post, NULL);
    }

    return bytes;
}

void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    const char *cpre = NULL;
    const char *cpost = NULL;
    getColor(os, COLOR_comment, &cpre, &cpost);

    if (cpre) write(writer_state, cpre, NULL);
    if (pre) write(writer_state, pre, NULL);
    for (size_t i = 0; i < n; ++i) write(writer_state, "/", NULL);
    if (post) write(writer_state, post, NULL);
    if (cpost) write(writer_state, cpost, NULL);
}

bool need_separation_before_attribute_key(XMQPrintState *ps)
{
    // If the previous value was quoted, then no space is needed, ie.
    // 'x y z'key=
    // If the previous text was attribute start, then no space is needed, ie.
    // (key=
    // If the previous text was compound endt, then no space is needed, ie.
    // ))key=
    // If the previous text was entity, then no space is needed, ie.
    // &x10;key=
    // if previous value was text, then a space is necessary, ie.
    // xyz key=
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '(' && c != ')' && c != ';';
}

bool need_separation_before_entity(XMQPrintState *ps)
{
    // No space needed for:
    // 'x y z'&nbsp;
    // =&nbsp;
    // {&nbsp;
    // }&nbsp;
    // ;&nbsp;
    // Otherwise a space is needed:
    // xyz &nbsp;
    char c = ps->last_char;
    return c != 0 && c != '=' && c != '\'' && c != '{' && c != '}' && c != ';' && c != '(' && c != ')';
}

bool need_separation_before_element_name(XMQPrintState *ps)
{
    // No space needed for:
    // 'x y z'key=
    // {key=
    // }key=
    // ;key=
    // */key=
    // )key=
    // Otherwise a space is needed:
    // xyz key=
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '{' && c != '}' && c != ';' && c != ')' && c != '/';
}

bool need_separation_before_quote(XMQPrintState *ps)
{
    // If the previous node was quoted, then a space is necessary, ie
    // 'a b c' 'next quote'
    // otherwise last char is the end of a text value, and no space is necessary, ie
    // key=value'next quote'
    char c = ps->last_char;
    return c == '\'';
}

bool need_separation_before_comment(XMQPrintState *ps)
{
    // If the previous value was quoted, then then no space is needed, ie.
    // 'x y z'/*comment*/
    // If the previous value was an entity &...; then then no space is needed, ie.
    // &nbsp;/*comment*/
    // if previous value was text, then a space is necessary, ie.
    // xyz /*comment*/
    // if previous value was } or )) then no space is is needed.
    // }/*comment*/   ((...))/*comment*/
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '{' && c != ')' && c != '}' && c != ';';
}

void check_space_before_attribute(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == '(') return;
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_attribute_key(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_entity_node(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == '(') return;
    if (!ps->output_settings->compact && c != '=')
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_entity(ps))
    {
        print_white_spaces(ps, 1);
    }
}


void check_space_before_quote(XMQPrintState *ps, Level level)
{
    char c = ps->last_char;
    if (c == 0) return;
    if (!ps->output_settings->compact && (c != '=' || level == LEVEL_XMQ) && c != '(')
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_quote(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_key(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == 0) return;

    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_element_name(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_opening_brace(XMQPrintState *ps)
{
    char c = ps->last_char;

    if (!ps->output_settings->compact)
    {
        if (c == ')')
        {
            print_nl_and_indent(ps, NULL, NULL);
        }
        else
        {
            print_white_spaces(ps, 1);
        }
    }
}

void check_space_before_closing_brace(XMQPrintState *ps)
{
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
}

void check_space_before_comment(XMQPrintState *ps)
{
    char c = ps->last_char;

    if (c == 0) return;
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_comment(ps))
    {
        print_white_spaces(ps, 1);
    }
}


void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align)
{
    check_space_before_attribute(ps);

    const char *key;
    const char *prefix;
    size_t total_u_len;

    attr_strlen_name_prefix(a, &key, &prefix, &total_u_len);

    if (prefix)
    {
        print_utf8(ps, COLOR_attr_ns, 1, prefix, NULL);
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
    }
    print_utf8(ps, COLOR_attr_key, 1, key, NULL);

    if (a->children != NULL)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1+align-total_u_len);

        print_utf8(ps, COLOR_equals, 1, "=", NULL);

        if (!ps->output_settings->compact) print_white_spaces(ps, 1);

        print_value(ps, a->children, LEVEL_ATTR_VALUE);
    }
}

void print_namespace_declaration(XMQPrintState *ps, xmlNs *ns, size_t align)
{
    if (!xml_non_empty_namespace(ns)) return;

    check_space_before_attribute(ps);

    const char *prefix;
    size_t total_u_len;

    namespace_strlen_prefix(ns, &prefix, &total_u_len);

    print_utf8(ps, COLOR_ns_declaration, 1, "xmlns", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
        print_utf8(ps, COLOR_attr_ns, 1, prefix, NULL);
    }

    const char *v = xml_namespace_href(ns);

    if (v != NULL)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1+align-total_u_len);

        print_utf8(ps, COLOR_equals, 1, "=", NULL);

        if (!ps->output_settings->compact) print_white_spaces(ps, 1);

        print_utf8(ps, COLOR_attr_value_text, 1, v, NULL);
    }
}


void print_attributes(XMQPrintState *ps,
                      xmlNodePtr node)
{
    xmlAttr *a = xml_first_attribute(node);

    size_t max = 0;
    if (!ps->output_settings->compact) max = find_attr_key_max_u_width(a);

    xmlNs *ns = xml_first_namespace_def(node);
    if (!ps->output_settings->compact) max = find_namespace_max_u_width(max, ns);

    size_t line_indent = ps->line_indent;
    ps->line_indent = ps->current_indent;
    while (a)
    {
        print_attribute(ps, a, max);
        a = xml_next_attribute(a);
    }

    while (ns)
    {
        print_namespace_declaration(ps, ns, max);
        ns = xml_next_namespace_def(ns);
    }

    ps->line_indent = line_indent;
}

bool is_safe_char(const char *i, const char *stop)
{
    char c = *i;
    return !(count_whitespace(i, stop) > 0 ||
             c == '\n' ||
             c == '(' ||
             c == ')' ||
             c == '\'' ||
             c == '\"' ||
             c == '{' ||
             c == '}' ||
             c == '\t' ||
             c == '\r');
}

bool unsafe_start(char c, char cc)
{
    return c == '=' || c == '&' || (c == '/' && (cc == '/' || cc == '*'));
}

void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps,
                                             XMQColor color,
                                             const char *start,
                                             const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre, *post;
    getColor(os, color, &pre, &post);

    if (pre) write(writer_state, pre, NULL);

    const char *old_restart_line = ps->restart_line;
    if (!post) ps->restart_line = pre;
    else ps->restart_line = NULL;

    for (const char *i = start; i < stop;)
    {
        if (*i == '\n')
        {
            print_nl_and_indent(ps, pre, post);
            i++;
        }
        else
        {
            i += print_utf8_char(ps, i, stop);
        }
    }
    if (*(stop-1) != '\n' && post) write(writer_state, post, NULL);
    ps->restart_line = old_restart_line;
}

void print_quotee(XMQPrintState *ps,
                 XMQColor c,
                 const char *start,
                 const char *stop)
{
    bool force = true;
    bool add_nls = false;
    bool add_compound = false;
    int numq = count_necessary_quotes(start, stop, false, &add_nls, &add_compound);
    size_t indent = ps->current_indent;

    if (numq > 0)
    {
        // If nl_begin is true and we have quotes, then we have to forced newline already due to quotes at
        // the beginning or end, therefore we use indent as is, however if
        if (add_nls == false) // then we might have to adjust the indent, or even introduce a nl_begin/nl_end.
        {
            if (indent == (size_t)-1)
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
    if (numq == 0 && force) numq = 1;

    size_t old_line_indent = 0;

    if (add_nls)
    {
        old_line_indent = ps->line_indent;
        ps->line_indent = ps->current_indent;
    }

    print_quotes(ps, numq, c);

    if (!add_nls)
    {
        old_line_indent = ps->line_indent;
        ps->line_indent = ps->current_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    print_quote_lines_and_color_uwhitespace(ps,
                                            c,
                                            start,
                                            stop);

    if (!add_nls)
    {
        ps->line_indent = old_line_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    print_quotes(ps, numq, c);

    if (add_nls)
    {
        ps->line_indent = old_line_indent;
    }
}

const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop)
{
    const char *i = start;

    while (i < stop)
    {
        int c = (int)((unsigned char)*i);
        if (c == '\n') break;
        i++;
    }

    return i;
}

const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop)
{
    bool compact = ps->output_settings->compact;
    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    const char *i = start;

    if (*i == '\'' && compact)
    {
        return i;
    }
    const char *pre_stop = stop-1;
    if (compact && *pre_stop == '\'')
    {
        while (pre_stop > start && *pre_stop == '\'') pre_stop--;
        pre_stop++;
    }

    while (i < stop)
    {
        int c = (int)((unsigned char)*i);
        if (compact && c == '\'' && i == pre_stop) break;
        if (newlines && c == '\n') break;
        if (non7bit && c > 126) break;
        if (c < 32 && c != '\n') break;
        i++;
    }

    return i;
}

void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level)
{
    if (!stop) stop = start+strlen(start);
    if (!start || start >= stop || start[0] == 0)
    {
        // This is for empty attribute values.
        // Empty elements do not have print_value invoked so there is no equal char printed here (eg = '')
        check_space_before_quote(ps, level);
        print_utf8(ps, level_to_quote_color(level), 1, "''", NULL);
        return;
    }

    if (has_all_quotes(start, stop))
    {
        // A text with all single quotes, print using &apos; only.
        // We could also be quoted using n+1 more quotes and newlines, but it seems a bit annoying:
        // ''''''
        // '''''
        // ''''''
        // compared to &apos;&apos;&apos;&apos;
        // The &apos; solution takes a little bit more space, but works for compact too,
        // lets use that for both normal and compact formatting.
        check_space_before_entity_node(ps);
        for (const char *i = start; i < stop; ++i)
        {
            print_utf8(ps, level_to_entity_color(level), 1, "&apos;", NULL);
        }
        return;
    }

    bool all_space = false;
    bool all_whitespace = has_all_whitespace(start, stop, &all_space);

    if (all_space)
    {
        // This are all normal ascii 32 spaces. Print like: '     '
        check_space_before_quote(ps, level);
        print_quoted_spaces(ps, level_to_quote_color(level), (size_t)(stop-start));
        return;
    }

    if (all_whitespace)
    {
        // All whitespace, but more than just normal spaces, ie newlines!
        // This is often the case with trimmed whitespace, lets print using
        // entities, which makes this content be easy to spot when --trim=none is used.
        // Also works both for normal and compact mode.
        print_all_whitespace(ps, start, stop, level);
        return;
    }

    if (is_xmq_text_value(start, stop) && (level == LEVEL_ELEMENT_VALUE || level == LEVEL_ATTR_VALUE))
    {
        // This is a key_node text value or an attribute text value, ie key = 123 or color=blue, ie no quoting needed.
        print_utf8(ps, level_to_quote_color(level), 1, start, stop);
        return;
    }

    const char *new_start = has_leading_space_nl(start, stop);
    if (new_start)
    {
        print_all_whitespace(ps, start, new_start, level);
        start = new_start;
    }

    const char *new_stop = has_ending_nl_space(start, stop);
    const char *old_stop = stop;
    if (new_stop)
    {
        stop = new_stop;
    }

    // Ok, normal content to be quoted. However we might need to split the content
    // at chars that need to be replaced with character entities. Normally no
    // chars need to be replaced. But in compact mode, the \n newlines are replaced with &#10;
    // Also one can replace all non-ascii chars with their entities if so desired.
    bool compact = ps->output_settings->compact;
    for (const char *from = start; from < stop; )
    {
        const char *to = find_next_char_that_needs_escape(ps, from, stop);
        if (from == to)
        {
            char c = *from;
            check_space_before_entity_node(ps);
            to += print_char_entity(ps, level_to_entity_color(level), from, stop);
            if (c == '\n' && !compact)
            {
                print_nl_and_indent(ps, NULL, NULL);
            }
        }
        else
        {
            check_space_before_quote(ps, level);
            bool add_nls = false;
            bool add_compound = false;
            bool compact = ps->output_settings->compact;
            count_necessary_quotes(from, to, false, &add_nls, &add_compound);
            if (!add_compound && (!add_nls || !compact))
            {
                print_quotee(ps, level_to_quote_color(level), from, to);
            }
            else
            {
                print_value_internal_text(ps, from, to, level);
            }
        }
        from = to;
    }
    if (new_stop)
    {
        print_all_whitespace(ps, new_stop, old_stop, level);
    }
}

/**
   print_value_internal:
   @ps: Print state.
   @node: Text node to be printed.
   @level: Printing node, key_value, kv_compound, attr_value, av_compound

   Print content as:
   EMPTY_: ''
   ENTITY: &#10;
   QUOTES: ( &apos;&apos; )
   WHITSP: ( &#32;&#32;&#10;&#32;&#32; )
   SPACES: '      '
   TEXT  : /root/home/foo&123
   QUOTE : 'x y z'
   QUOTEL: 'xxx
            yyy'
*/
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level)
{
    if (node->type == XML_ENTITY_REF_NODE ||
        node->type == XML_ENTITY_NODE)
    {
        print_entity_node(ps, node);
        return;
    }

    print_value_internal_text(ps, xml_element_content(node), NULL, level);
}

/**
   quote_needs_compounded:
   @ps: The print state.
   @start: Content buffer start.
   @stop: Points to after last buffer byte.

   Used to determine early if the quote needs to be compounded.
*/
bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop)
{
    bool compact = ps->output_settings->compact;
    if (stop == start+1)
    {
        // A single quote becomes &apos;
        // A single newline becomes &#10;
        // A single cr becomes &#13;
        // A single tab becomes &#9;
        if (*start == '\'') return false;
        if (*start == '\n') return false;
        if (*start == '\r') return false;
        if (*start == '\t') return false;
    }
    if (has_leading_space_nl(start, stop)) return true;
    if (has_ending_nl_space(start, stop)) return true;

    if (compact)
    {
        // In compact form newlines must be escaped: &#10;
        if (has_newlines(start, stop)) return true;
        // In compact form leading or ending single quotes triggers &#39; escapes
        // since we cannot use the multiline quote trick:
        // '''
        // 'alfa'
        // '''
        if (has_leading_ending_quote(start, stop)) return true;
    }

    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    for (const char *i = start; i < stop; ++i)
    {
        int c = (int)(unsigned char)(*i);
        if (newlines && c == '\n') return true;
        if (non7bit && c > 126) return true;
        if (c < 32 && c != '\n') return true;
    }
    return false;
}

void print_value(XMQPrintState *ps, xmlNode *node, Level level)
{
    // Check if there are more than one part, if so the value has to be compounded.
    bool is_compound = level != LEVEL_XMQ && node != NULL && node->next != NULL;

    // Check if the single part will split into multiple parts and therefore needs to be compounded.
    if (!is_compound && node && !is_entity_node(node) && level != LEVEL_XMQ)
    {
        // Check if there are leading ending quotes/whitespace. But also
        // if compact output and there are newlines inside.
        const char *start = xml_element_content(node);
        const char *stop = start+strlen(start);
        is_compound = quote_needs_compounded(ps, start, stop);
    }

    size_t old_line_indent = ps->line_indent;

    if (is_compound)
    {
        level = enter_compound_level(level);
        print_utf8(ps, COLOR_cpar_left, 1, "(", NULL);
        if (!ps->output_settings->compact) print_white_spaces(ps, 1);
        ps->line_indent = ps->current_indent;
    }

    for (xmlNode *i = node; i; i = xml_next_sibling(i))
    {
        print_value_internal(ps, i, level);
        if (level == LEVEL_XMQ) break;
    }

    if (is_compound)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1);
        print_utf8(ps, COLOR_cpar_right, 1, ")", NULL);
    }

    ps->line_indent = old_line_indent;
}

#endif // XMQ_PRINTER_MODULE
