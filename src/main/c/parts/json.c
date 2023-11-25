
#ifndef BUILDING_XMQ

#include"utils.h"
#include"parts/xmq_internals.h"
#include"json.h"
#include"stack.h"
#include"text.h"
#include"xml.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef JSON_MODULE

void eat_json_boolean(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_json_null(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_json_number(XMQParseState *state, const char **content_start, const char **content_stop);
size_t eat_json_quote(XMQParseState *state, const char **content_start, const char **content_stop);

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_array(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_boolean(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_null(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_number(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop);

bool has_number_ended(char c);
const char *is_jnumber(const char *start, const char *stop);
bool is_json_boolean(XMQParseState *state);
bool is_json_null(XMQParseState *state);
bool is_json_number(XMQParseState *state);
bool is_json_quote_start(char c);
bool is_json_whitespace(char c);
void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node);
void json_print_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node);
void json_print_value(XMQPrintState *ps, xmlNode *container, xmlNode *node, Level level);
void json_print_boolean_value(XMQPrintState *ps, xmlNode *container, xmlNode *node);
void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node);
void json_print_element_with_children(XMQPrintState *ps, xmlNode *container, xmlNode *node);
void json_print_key_node(XMQPrintState *ps, xmlNode *container, xmlNode *node);

void json_check_comma(XMQPrintState *ps);
void json_print_comma(XMQPrintState *ps);
bool json_is_number(const char *start, const char *stop);
bool json_is_keyword(const char *start, const char *stop);
void json_print_leaf_node(XMQPrintState *ps, xmlNode *container, xmlNode *node);

bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop);

char equals[] = "=";
char underline[] = "_";
char leftpar[] = "(";
char rightpar[] = ")";
char leftbrace[] = "{";
char rightbrace[] = "}";
char array[] = "A";
char string[] = "S";

bool is_json_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_json_quote_start(char c)
{
    return c == '"';
}

size_t eat_json_quote(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    increment('"', 1, &i, &line, &col);
    *content_start = i;

    while (i < end)
    {
        char c = *i;
        if (c == '"')
        {
            *content_stop = i;
            increment(c, 1, &i, &line, &col);
            break;
        }
/*        if (c == '\\')
        {
            increment(c, 1, &i, &line, &col);
            c = *i;
            if (c == '"' || c == '\\' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't')
            {
                increment(c, 1, &i, &line, &col);
                continue;
            }
            else if (c == 'u')
            {
                increment(c, 1, &i, &line, &col);
                c = *i;
                if (i+3 < end)
                {
                    if (is_hex(*(i+0)) && is_hex(*(i+1)) && is_hex(*(i+2)) && is_hex(*(i+3)))
                    {
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        continue;
                    }
                }
            }
            state->error_nr = XMQ_ERROR_JSON_INVALID_ESCAPE;
            longjmp(state->error_handler, 1);
        }
*/
        increment(c, 1, &i, &line, &col);
    }
    state->i = i;
    state->line = line;
    state->col = col;

    return 1;
}

void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop)
{
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;
    size_t depth = eat_json_quote(state, &content_start, &content_stop);
    size_t content_start_col = start_col+depth;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);

    bool need_string_type =
        content_stop > content_start && (
        !strncmp(content_start, "true", content_stop-content_start) ||
        !strncmp(content_start, "false", content_stop-content_start) ||
        !strncmp(content_start, "null", content_stop-content_start) ||
        is_jnumber(content_start, content_stop));

    if (need_string_type || unsafe_key_start)
    {
        // Ah, this is the string "false" not the boolean false. Mark this with the attribute S to show that it is a string.
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        if (need_string_type)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, string, state->col, string, string+1, string+1);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_quote, state, start_line, start_col, content_start, content_start_col, content_start, content_stop, content_stop);
}

bool is_json_null(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("null", i, 4)) return true;
    return false;
}

void eat_json_null(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    size_t line = state->line;
    size_t col = state->col;

    increment('n', 1, &i, &line, &col);
    increment('u', 1, &i, &line, &col);
    increment('l', 1, &i, &line, &col);
    increment('l', 1, &i, &line, &col);

    state->i = i;
    state->line = line;
    state->col = col;
}

void parse_json_null(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_null(state, &content_start, &content_stop);
    const char *stop = state->i;

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
}

bool has_number_ended(char c)
{
    return c == ' ' || c == '\n' || c == ',' || c == '}' || c == ']';
}

const char *is_jnumber(const char *start, const char *stop)
{
    if (stop == NULL) stop = start+strlen(start);
    if (start == stop) return NULL;

    bool found_e = false;
    bool found_e_sign = false;
    bool leading_zero = false;
    bool last_is_digit = false;
    bool found_dot = false;

    const char *i;
    for (i = start; i < stop; ++i)
    {
        char c = *i;

        last_is_digit = false;
        bool current_is_not_digit = (c < '0' || c > '9');

        if (i == start)
        {
            if (current_is_not_digit && c != '-' ) return NULL;
            if (c == '0') leading_zero = true;
            if (c != '-') last_is_digit = true;
            continue;
        }

        if (leading_zero)
        {
            leading_zero = false;
            if (has_number_ended(c)) return i;
            if (c != '.') return NULL;
            found_dot = true;
        }
        else if (c == '.')
        {
            if (found_dot) return NULL;
            found_dot = true;
        }
        else if (c == 'e' || c == 'E')
        {
            if (found_e) return NULL;
            found_e = true;
        }
        else if (found_e && !found_e_sign)
        {
            if (has_number_ended(c)) return i;
            if (current_is_not_digit && c != '-' && c != '+') return NULL;
            if (c == '+' || c == '-')
            {
                found_e_sign = true;
            }
            else
            {
                last_is_digit = true;
            }
        }
        else
        {
            found_e_sign = false;
            if (has_number_ended(c)) return i;
            if (current_is_not_digit) return NULL;
            last_is_digit = true;
        }
    }

    if (last_is_digit == false) return NULL;

    return i;
}

bool is_json_boolean(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("true", i, 4)) return true;
    if (i+5 <= stop && !strncmp("false", i, 5)) return true;
    return false;
}

void eat_json_boolean(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    //const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    if (*i == 't')
    {
        increment('t', 1, &i, &line, &col);
        increment('r', 1, &i, &line, &col);
        increment('u', 1, &i, &line, &col);
        increment('e', 1, &i, &line, &col);
    }
    else
    {
        increment('f', 1, &i, &line, &col);
        increment('a', 1, &i, &line, &col);
        increment('l', 1, &i, &line, &col);
        increment('s', 1, &i, &line, &col);
        increment('e', 1, &i, &line, &col);
    }

    state->i = i;
    state->line = line;
    state->col = col;
}

void parse_json_boolean(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_boolean(state, &content_start, &content_stop);
    const char *stop = state->i;

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
}

bool is_json_number(XMQParseState *state)
{
    return NULL != is_jnumber(state->i, state->buffer_stop);
}

void eat_json_number(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *start = state->i;
    const char *stop = state->buffer_stop;
    const char *i = start;
    size_t line = state->line;
    size_t col = state->col;

    const char *end = is_jnumber(i, stop);
    assert(end); // Must not call eat_json_number without check for a number before...
    increment('?', end-start, &i, &line, &col);

    state->i = i;
    state->line = line;
    state->col = col;
}

void parse_json_number(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_number(state, &content_start, &content_stop);
    const char *stop = state->i;

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
}

bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = 0;

    if (state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        parse_json(state, NULL, NULL);
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

    if (state->parse->done) state->parse->done(state);
    return true;
}

void parse_json_array(XMQParseState *state, const char *key_start, const char *key_stop)
{
    char c = *state->i;
    assert(c == '[');
    increment(c, 1, &state->i, &state->line, &state->col);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);

    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, state->col, leftpar, leftpar+1, leftpar+1);
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, array, state->col, array, array+1, array+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, state->col, rightpar, rightpar+1, rightpar+1);
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == ']') break;

        parse_json(state, NULL, NULL);
        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }

    assert(c == ']');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop)
{
    eat_whitespace(state, NULL, NULL);

    char c = *(state->i);

    if (is_json_quote_start(c)) parse_json_quote(state, key_start, key_stop);
    else if (is_json_boolean(state)) parse_json_boolean(state, key_start, key_stop);
    else if (is_json_null(state)) parse_json_null(state, key_start, key_stop);
    else if (is_json_number(state)) parse_json_number(state, key_start, key_stop);
    else if (c == '{') parse_json_object(state, key_start, key_stop);
    else if (c == '[') parse_json_array(state, key_start, key_stop);
    else
    {
        state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
        longjmp(state->error_handler, 1);
    }
    eat_whitespace(state, NULL, NULL);
}

void json_print_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
    xmlNode *i = from;

    while (i)
    {
        json_print_node(ps, container, i);
        i = xml_next_sibling(i);
    }
}

void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node)
{
    // Standalone quote must be quoted: 'word' 'some words'
/*    if (is_content_node(node))
    {
        json_print_value(ps, container, node, LEVEL_XMQ);
        return;
        }*/
/*
    // This is an entity reference node. &something;
    if (is_entity_node(node))
    {
        return json_print_entity_node(ps, node);
    }

    // This is a comment translated into "_//":"Comment text"
    if (is_comment_node(node))
    {
        return json_print_comment_node(ps, node);
    }

    // This is doctype node.
    if (is_doctype_node(node))
    {
        return json_print_doctype(ps, node);
    }
*/
    // This is a node with no children, but the only such valid json nodes are
    // the empty object _ ---> {} or _(A) ---> [].
    if (is_leaf_node(node))
    {
        return json_print_leaf_node(ps, container, node);
    }

    // This is a key = value or key = 'value value' node and there are no attributes.
    if (is_key_value_node(node))
    {
        return json_print_key_node(ps, container, node);
    }

    // The node is marked foo(A) { } translate this into: "foo":[ ]
    if (xml_get_attribute(node, "A"))
    {
        return json_print_array_with_children(ps, container, node);
    }
    // All other nodes are printed
    json_print_element_with_children(ps, container, node);
}

void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop)
{
    char c = *state->i;
    assert(c == '{');
    increment(c, 1, &state->i, &state->line, &state->col);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, state->col, key_start, key_stop, key_stop);
    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == '}') break;

        if (!is_json_quote_start(c))
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }

        // Find the key string, ie speed in { "speed":123 }
        const char *key_start, *key_stop;
        eat_json_quote(state, &key_start, &key_stop);

        eat_whitespace(state, NULL, NULL);
        c = *(state->i);

        if (c == ':')
        {
            increment(c, 1, &state->i, &state->line, &state->col);
        }
        else
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }

        parse_json(state, key_start, key_stop);

        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }
    while (c == ',');

    assert(c == '}');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void json_print_value(XMQPrintState *ps, xmlNode *container, xmlNode *node, Level level)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    const char *content = xml_element_content(node);

    if (!xml_next_sibling(node) &&
        (json_is_number(xml_element_content(node), NULL)
         || json_is_keyword(xml_element_content(node), NULL)))
    {
        // This is a number or a keyword. E.g. 123 true false null
        write(writer_state, content, NULL);
        ps->last_char = content[strlen(content)-1];
    }
    else if (!xml_next_sibling(node) && content[0] == 0)
    {
        write(writer_state, "\"\"", NULL);
        ps->last_char = '"';
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "\"", NULL);

        for (xmlNode *i = node; i; i = xml_next_sibling(i))
        {
            if (is_entity_node(i))
            {
                write(writer_state, "&", NULL);
                write(writer_state, (const char*)i->name, NULL);
                write(writer_state, ";", NULL);
            }
            else
            {
                write(writer_state, xml_element_content(node), NULL);
            }
        }

        print_utf8(ps, COLOR_none, 1, "\"", NULL);
        ps->last_char = '"';
    }
}

void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node)
{
    if (container)
    {
        // We have a containing node, then we can print this using "name" : [ ... ]
        json_check_comma(ps);
        json_print_element_name(ps, container, node);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    print_utf8(ps, COLOR_brace_left, 1, "[", NULL);
    ps->last_char = '[';

    ps->line_indent += ps->output_settings->add_indent;

    if (!container)
    {
        // Top level object or object inside array. [ {} {} ]
        // Dump the element name! It cannot be represented!
    }
    while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
    assert(from != NULL);

    json_print_nodes(ps, NULL, (xmlNode*)from, (xmlNode*)to);

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "]", NULL);
    ps->last_char = ']';
}

void json_print_element_with_children(XMQPrintState *ps,
                                      xmlNode *container,
                                      xmlNode *node)
{
    json_check_comma(ps);

    if (container)
    {
        // We have a containing node, then we can print this using "name" : { ... }
        json_print_element_name(ps, container, node);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    print_utf8(ps, COLOR_brace_left, 1, "{", NULL);
    ps->last_char = '{';

    ps->line_indent += ps->output_settings->add_indent;

    const char *name = xml_element_name(node);
    if (!container && name && name[0] != '_' && name[1] != 0)
    {
        // Top level object or object inside array.
        print_utf8(ps, COLOR_none, 1, "\"_\":", NULL);
        ps->last_char = ':';
        json_print_element_name(ps, container, node);
    }

    while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
    assert(from != NULL);

    json_print_nodes(ps, node, (xmlNode*)from, (xmlNode*)to);

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "}", NULL);
    ps->last_char = '}';
}

void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node)
{
    const char *name = (const char*)node->name;
    const char *prefix = NULL;

    if (node->ns && node->ns->prefix)
    {
        prefix = (const char*)node->ns->prefix;
    }

    print_utf8(ps, COLOR_none, 1, "\"", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_none, 1, prefix, NULL);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    print_utf8(ps, COLOR_none, 1, name, NULL);

    print_utf8(ps, COLOR_none, 1, "\"", NULL);

    ps->last_char = '"';
    /*
    if (xml_first_attribute(node) || xml_first_namespace_def(node))
    {
        print_utf8(ps, COLOR_apar_left, 1, "(", NULL);
        print_attributes(ps, node);
        print_utf8(ps, COLOR_apar_right, 1, ")", NULL);
    }
    */
}

void json_print_key_node(XMQPrintState *ps,
                         xmlNode *container,
                         xmlNode *node)
{
    json_check_comma(ps);

    const char *name = xml_element_name(node);
    if (name[0] != '_')
    {
        json_print_element_name(ps, container, node);
        print_utf8(ps, COLOR_equals, 1, ":", NULL);
        ps->last_char = ':';
    }
    else if (name[1] == 0)
    {
        xmlAttr *a = xml_get_attribute(node, "_");
        if (a)
        {
            // The key was stored inside the attribute because it could not
            // be used as the element name.
            char *value = (char*)xmlNodeListGetString(node->doc, a->children, 1);
            char *quoted_value = xmq_quote_as_c(value, value+strlen(value));
            print_utf8(ps, COLOR_none, 3, "\"", NULL, quoted_value, NULL, "\":", NULL);
            free(quoted_value);
            xmlFree(value);
            ps->last_char = ':';
        }
    }
    json_print_value(ps, container, xml_first_child(node), LEVEL_ELEMENT_VALUE);
}

void json_check_comma(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == 0) return;

    if (c != '{' && c != '[')
    {
        json_print_comma(ps);
    }
}

void json_print_comma(XMQPrintState *ps)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    write(writer_state, ",", NULL);
    ps->last_char = ',';
    ps->current_indent ++;
}

bool json_is_number(const char *start, const char *stop)
{
    return NULL != is_jnumber(start, stop);
}

bool json_is_keyword(const char *start, const char *stop)
{
    if (!strcmp(start, "true")) return true;
    if (!strcmp(start, "false")) return true;
    if (!strcmp(start, "null")) return true;
    return false;
}

void json_print_leaf_node(XMQPrintState *ps,
                          xmlNode *container,
                          xmlNode *node)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    const char *name = xml_element_name(node);

    json_check_comma(ps);

    if (name &&
        name[0] != '_' &&
        name[1] != 0)
    {
        json_print_element_name(ps, container, node);
        write(writer_state, ":", NULL);
    }

    if (xml_get_attribute(node, "A"))
    {
        write(writer_state, "[]", NULL);
        ps->last_char = ']';
    }
    else
    {
        write(writer_state, "{}", NULL);
        ps->last_char = '}';
    }
}

#else

// Empty function when XMQ_NO_JSON is defined.
bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    return false;
}

// Empty function when XMQ_NO_JSON is defined.
void json_print_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
}

#endif // JSON_MODULE
