
#include"json.h"

#ifndef BUILDING_DIST_XMQ

#include"always.h"
#include"colors.h"
#include"hashmap.h"
#include"membuffer.h"
#include"xmq_internals.h"
#include"stack.h"
#include"text.h"
#include"xml.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef JSON_MODULE

void eat_json_boolean(XMQParseState *state);
void eat_json_null(XMQParseState *state);
void eat_json_number(XMQParseState *state);
void eat_json_quote(XMQParseState *state, char **content_start, char **content_stop);

void fixup_json(XMQDoc *doq, xmlNode *node);

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_array(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_boolean(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_null(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_number(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop);

bool has_number_ended(char c);
bool has_attr_other_than_AS_(xmlNode *node);
const char *is_jnumber(const char *start, const char *stop);
bool is_json_boolean(XMQParseState *state);
bool is_json_null(XMQParseState *state);
bool is_json_number(XMQParseState *state);
bool is_json_quote_start(char c);
bool is_json_whitespace(char c);
void json_print_namespace_declaration(XMQPrintState *ps, xmlNs *ns);
void json_print_attribute(XMQPrintState *ps, xmlAttrPtr a);
void json_print_attributes(XMQPrintState *ps, xmlNodePtr node);
void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node);
void json_print_comment_node(XMQPrintState *ps, xmlNodePtr node, bool prefix_ul, size_t total, size_t used);
void json_print_doctype_node(XMQPrintState *ps, xmlNodePtr node);
void json_print_entity_node(XMQPrintState *ps, xmlNodePtr node);
void json_print_standalone_quote(XMQPrintState *ps, xmlNode *container, xmlNodePtr node, size_t total, size_t used);
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_value(XMQPrintState *ps, xmlNode *from, xmlNode *to, Level level, bool force_string);
void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_element_with_children(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_key_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used, bool force_string);

void json_check_comma(XMQPrintState *ps);
void json_print_comma(XMQPrintState *ps);
bool json_is_number(const char *start);
bool json_is_keyword(const char *start);
void json_print_leaf_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);

void trim_index_suffix(const char *key_start, const char **stop);

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

void eat_json_quote(XMQParseState *state, char **content_start, char **content_stop)
{
    const char *start = state->i;
    const char *stop = state->buffer_stop;

    MemBuffer *buf = new_membuffer();

    const char *i = start;
    size_t line = state->line;
    size_t col = state->col;

    increment('"', 1, &i, &line, &col);

    while (i < stop)
    {
        char c = *i;
        if (c == '"')
        {
            increment(c, 1, &i, &line, &col);
            break;
        }
        if (c == '\\')
        {
            increment(c, 1, &i, &line, &col);
            c = *i;
            if (c == '"' || c == '\\' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == '/')
            {
                increment(c, 1, &i, &line, &col);
                switch(c)
                {
                case 'b': c = 8; break;
                case 'f': c = 12; break;
                case 'n': c = 10; break;
                case 'r': c = 13; break;
                case 't': c = 9; break;
                case '/': c = '/'; break; // Silly, but actually used sometimes in json....
                }
                membuffer_append_char(buf, c);
                continue;
            }
            else if (c == 'u')
            {
                increment(c, 1, &i, &line, &col);
                c = *i;
                if (i+3 < stop)
                {
                    // Woot? Json can only escape unicode up to 0xffff ? What about 10000 up to 10ffff?
                    if (is_hex(*(i+0)) && is_hex(*(i+1)) && is_hex(*(i+2)) && is_hex(*(i+3)))
                    {
                        unsigned char c1 = hex_value(*(i+0));
                        unsigned char c2 = hex_value(*(i+1));
                        unsigned char c3 = hex_value(*(i+2));
                        unsigned char c4 = hex_value(*(i+3));
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);

                        int uc = (c1<<12)|(c2<<8)|(c3<<4)|c4;
                        UTF8Char utf8;
                        size_t n = encode_utf8(uc, &utf8);

                        for (size_t j = 0; j < n; ++j)
                        {
                            membuffer_append_char(buf, utf8.bytes[j]);
                        }
                        continue;
                    }
                }
            }
            state->error_nr = XMQ_ERROR_JSON_INVALID_ESCAPE;
            longjmp(state->error_handler, 1);
        }
        membuffer_append_char(buf, c);
        increment(c, 1, &i, &line, &col);
    }
    // Add a zero termination to the string which is not used except for
    // guaranteeing that there is at least one allocated byte for empty strings.
    membuffer_append_null(buf);
    state->i = i;
    state->line = line;
    state->col = col;

    // Calculate the real length which might be less than the original
    // since escapes have disappeared. Add 1 to have at least something to allocate.
    size_t len = membuffer_used(buf);
    char *quote = free_membuffer_but_return_trimmed_content(buf);
    *content_start = quote;
    *content_stop = quote+len-1; // Drop the zero byte.
}

void trim_index_suffix(const char *key_start, const char **stop)
{
    const char *key_stop = *stop;

    if (key_start && key_stop && key_start < key_stop && *(key_stop-1) == ']')
    {
        // This is an indexed element name "path[32]":"123" ie the 32:nd xml element
        // which has been indexed because json objects must have unique keys.
        // Well strictly speaking the json permits multiple keys, but no implementation does....
        //
        // Lets drop the suffix [32].
        const char *i = key_stop-2;
        // Skip the digits stop at [ or key_start
        while (i > key_start && *i >= '0' && *i <= '9' && *i != '[') i--;
        if (i > key_start && *i == '[')
        {
            // We found a [ which is not at key_start. Trim off suffix!
            *stop = i;
        }
    }
}

void set_node_namespace(XMQParseState *state, xmlNodePtr node, const char *node_name)
{
    if (state->element_namespace)
    {
        // Have a namespace before the element name, eg abc:work
        xmlNsPtr ns = xmlSearchNs(state->doq->docptr_.xml,
                                  node,
                                  (const xmlChar *)state->element_namespace);
        if (!ns)
        {
            // The namespaces does not yet exist. Lets hope it will be declared
            // inside the attributes of this node. Use a temporary href for now.
            ns = xmlNewNs(node,
                          NULL,
                          (const xmlChar *)state->element_namespace);
            debug("[XMQ] created namespace prefix=%s in element %s\n", state->element_namespace, node_name);
        }
        debug("[XMQ] setting namespace prefix=%s for element %s\n", state->element_namespace, node_name);
        xmlSetNs(node, ns);
        free(state->element_namespace);
        state->element_namespace = NULL;
    }
    else if (state->default_namespace)
    {
        // We have a default namespace.
        xmlNsPtr ns = (xmlNsPtr)state->default_namespace;
        assert(ns->prefix == NULL);
        debug("[XMQ] set default namespace with href=%s for element %s\n", ns->href, node_name);
        xmlSetNs(node, ns);
    }
}

void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop)
{
    int start_line = state->line;
    int start_col = state->col;

    char *content_start = NULL;
    char *content_stop = NULL;

    // Decode and content_start points to newly allocated buffer where escapes have been removed.
    eat_json_quote(state, &content_start, &content_stop);
    size_t content_len = content_stop-content_start;

    trim_index_suffix(key_start, &key_stop);

    if (key_start && *key_start == '|' && key_stop == key_start+1)
    {
        // This is "|":"symbol" which means a pure text node in xml.
        DO_CALLBACK_SIM(quote, state, start_line, 1, content_start, content_stop, content_stop);
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+2 && *key_start == '/' && *(key_start+1) == '/')
    {
        // This is "//":"symbol" which means a comment node in xml.
        DO_CALLBACK_SIM(comment, state, start_line, start_col, content_start, content_stop, content_stop);
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+3 && *key_start == '_' && *(key_start+1) == '/' && *(key_start+2) == '/')
    {
        // This is "_//":"symbol" which means a comment node in xml prefixing the root xml node.
        if (!state->root_found) state->add_pre_node_before = (xmlNode*)state->element_stack->top->data;
        else                    state->add_post_node_after = (xmlNode*)state->element_stack->top->data;
        DO_CALLBACK_SIM(comment, state, start_line, start_col, content_start, content_stop, content_stop);
        if (!state->root_found) state->add_pre_node_before = NULL;
        else                    state->add_post_node_after = NULL;
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+1 && *key_start == '_' )
    {
        // This is the element name "_":"symbol" stored inside the json object,
        // in situations where the name is not visible as a key. For example
        // the root json object and any object in arrays.
        xmlNodePtr container = (xmlNodePtr)state->element_stack->top->data;
        size_t len = content_stop - content_start;
        char *name = (char*)malloc(len+1);
        memcpy(name, content_start, len);
        name[len] = 0;
        const char *colon = NULL;
        bool do_return = true;
        if (is_xmq_element_name(name, name+len, &colon))
        {
            if (!colon)
            {
                xmlNodeSetName(container, (xmlChar*)name);
            }
            else
            {
                DO_CALLBACK_SIM(element_ns, state, state->line, state->col, name, colon, colon);
                xmlNodeSetName(container, (xmlChar*)colon+1);
                set_node_namespace(state, container, colon+1);
            }
        }
        else
        {
            // Oups! we cannot use this as an element name! What should we do?
            //xmlNodeSetName(container, "Baaad")
            PRINT_WARNING("xmq: Warning! \"_\":\"%s\" cannot be converted into an valid element name!\n", name);
            do_return = false;
        }
        free(name);
        if (do_return)
        {
            free(content_start);
            // This will be set many times.
            state->root_found = true;
            return;
        }
    }

    if (key_start && *key_start == '!' && !state->doctype_found)
    {
        size_t len = key_stop-key_start;
        if (len == 8 && !strncmp("!DOCTYPE", key_start, 8))
        {
            // This is the one and only !DOCTYPE element.
            DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
            state->parsing_doctype = true;
            state->add_pre_node_before = (xmlNode*)state->element_stack->top->data;
            DO_CALLBACK_SIM(element_value_quote, state, state->line, state->col, content_start, content_stop, content_stop);
            state->add_pre_node_before = NULL;
            free(content_start);
            return;
        }
    }

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (*key_start == '_' && key_stop > key_start+1)
    {
        // Check if this is an xmlns ns declaration.
        if (key_start+6 <= key_stop && !strncmp(key_start, "_xmlns", 6))
        {
            // Declaring the use of a namespace.
            if (colon)
            {
                // We have for example: "_xmlns:xls":"http://a.b.c."
                DO_CALLBACK_SIM(ns_declaration, state, state->line, state->col, key_start+1, colon?colon:key_stop, key_stop);
                assert (state->declaring_xmlns == true);
                DO_CALLBACK_SIM(attr_value_quote, state, start_line, start_col, content_start, content_stop, content_stop)
            }
            else
            {
                // The default namespace. "_xmlns":"http://a.b.c"
                DO_CALLBACK_SIM(ns_declaration, state, state->line, state->col, key_start+1, key_stop, key_stop);
                DO_CALLBACK_SIM(attr_value_quote, state, start_line, start_col, content_start, content_stop, content_stop)
            }
        }
        else
        {
            // This is a normal attribute that was stored as "_attr":"value"
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, key_start+1, key_stop, key_stop);
            DO_CALLBACK_SIM(attr_value_quote, state, start_line, start_col, content_start, content_stop, content_stop);
        }
        free(content_start);
        return;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);

    bool need_string_type =
        content_len > 0 && (
            (content_len == 4 && !strncmp(content_start, "true", 4)) ||
            (content_len == 5 && !strncmp(content_start, "false", 5)) ||
            (content_len == 4 && !strncmp(content_start, "null", 4)) ||
            content_stop == is_jnumber(content_start, content_stop));

    if (need_string_type || unsafe_key_start)
    {
        // Ah, this is the string "false" not the boolean false. Mark this with the attribute S to show that it is a string.
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        if (need_string_type)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, string, string+1, string+1);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK_SIM(element_value_text, state, start_line, start_col, content_start, content_stop, content_stop);
    free(content_start);
}

bool is_json_null(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("null", i, 4)) return true;
    return false;
}

void eat_json_null(XMQParseState *state)
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

    eat_json_null(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (key_start && *key_start == '_' && key_stop > key_start+1)
    {
        // script:{"_async":null "_href":"abc"}
        // translates into scripts(async href=abc)
        // detect attribute before this null. Make into attribute without value.
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, key_start+1, key_stop, key_stop);
        return;
    }

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
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

void eat_json_boolean(XMQParseState *state)
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

    eat_json_boolean(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
}

bool is_json_number(XMQParseState *state)
{
    return NULL != is_jnumber(state->i, state->buffer_stop);
}

void eat_json_number(XMQParseState *state)
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

    eat_json_number(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
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
    state->error_nr = XMQ_ERROR_NONE;

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
        XMQParseError error_nr = state->error_nr;
        generate_state_error_message(state, error_nr, start, stop);
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

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
        DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
    }
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, array, array+1, array+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);

    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_xml_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == ']') break;

        parse_json(state, NULL, NULL);
        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }

    assert(c == ']');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop)
{
    eat_xml_whitespace(state, NULL, NULL);

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
    eat_xml_whitespace(state, NULL, NULL);
}

typedef struct
{
    size_t total;
    size_t used;
} Counter;

void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
    xmlNode *i = from;

    HashMap* map = hashmap_create(100);

    while (i)
    {
        const char *name = (const char*)i->name;
        if (name && strcmp(name, "_")) // We have a name and it is NOT a single _
        {
            Counter *c = (Counter*)hashmap_get(map, name);
            if (!c)
            {
                c = (Counter*)malloc(sizeof(Counter));
                memset(c, 0, sizeof(Counter));
                hashmap_put(map, name, c);
            }
            c->total++;
        }
        if (i == to) break;
        i = xml_next_sibling(i);
    }

    i = from;
    while (i)
    {
        const char *name = (const char*)i->name;
        if (name && strcmp(name, "_"))
        {
            Counter *c = (Counter*)hashmap_get(map, (const char*)i->name);
            json_print_node(ps, container, i, c->total, c->used);
            c->used++;
        }
        else
        {
            json_print_node(ps, container, i, 1, 0);
        }
        if (i == to) break;
        i = xml_next_sibling(i);
    }

    hashmap_free_and_values(map, free);
}

void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
    xmlNode *i = from;
    while (i)
    {
        json_check_comma(ps);
        bool force_string = xml_get_attribute(i, "S");
        bool is_number = xml_element_content(i) && json_is_number(xml_element_content(i));
        bool is_keyword = xml_element_content(i) && json_is_keyword(xml_element_content(i));

        if (force_string || is_number || is_keyword)
        {
            json_print_value(ps, xml_first_child(i), xml_last_child(i), LEVEL_ELEMENT_VALUE, force_string);
        }
        else
        {
            json_print_node(ps, NULL, i, 1, 0);
        }
        i = xml_next_sibling(i);
    }
}

bool has_attr_other_than_AS_(xmlNode *node)
{
    xmlAttr *a = xml_first_attribute(node);

    while (a)
    {
        if (strcmp((const char*)a->name, "A") &&
            strcmp((const char*)a->name, "S") &&
            strcmp((const char*)a->name, "_")) return true;
        a = a->next;
    }

    return false;
}

void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used)
{
    // This is a comment translated into "//":"Comment text"
    if (is_comment_node(node))
    {
        json_print_comment_node(ps, node, false, total, used);
        return;
    }

    // Standalone quote must be quoted: 'word' 'some words'
    if (is_content_node(node))
    {
        json_print_standalone_quote(ps, container, node, total, used);
        return;
    }

    // This is an entity reference node. &something;
    if (is_entity_node(node))
    {
        json_print_entity_node(ps, node);
        return;
    }

    // This is a node with no children, but the only such valid json nodes are
    // the empty object _ ---> {} or _(A) ---> [].
    if (is_leaf_node(node) && container)
    {
        return json_print_leaf_node(ps, container, node, total, used);
    }

    // This is a key = value or key = 'value value' or key = ( 'value' &#10; )
    // Also! If the node has attributes, then we cannot print as key value in json.
    // It has to be an object.
    if (is_key_value_node(node) &&
        (!has_attributes(node) ||
         !has_attr_other_than_AS_(node)))
    {
        bool force_string = xml_get_attribute(node, "S");
        return json_print_key_node(ps, container, node, total, used, force_string);
    }

    // The node is marked foo(A) { } translate this into: "foo":[ ]
    if (xml_get_attribute(node, "A"))
    {
        const char *name = xml_element_name(node);
        bool is_underline = (name[0] == '_' && name[1] == 0);
        bool has_attr = has_attr_other_than_AS_(node);
        if (!is_underline && !container)
        {
            // The xmq "alfa(A) { _=hello _=there }" translates into
            // the json ["hello","there"] and there is no sensible
            // way of storing the alfa element name. Fixable?
            PRINT_WARNING("xmq: Warning! The element name \"%s\" is lost when converted to an unnamed json array!\n", name);
        }
        if (has_attr)
        {
            // The xmq "_(A beta=123) { _=hello _=there }" translates into
            // the json ["hello","there"] and there is no sensible
            // way of storing the beta attribute. Fixable?
            PRINT_WARNING("xmq: Warning! The element \"%s\" loses its attributes when converted to a json array!\n", name);
        }
        return json_print_array_with_children(ps, container, node);
    }

    // All other nodes are printed
    json_print_element_with_children(ps, container, node, total, used);
}

void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop)
{
    char c = *state->i;
    assert(c == '{');
    increment(c, 1, &state->i, &state->line, &state->col);

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;
    const char *colon = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start || key_start == key_stop)
    {
        // No key and the empty key translates into a _
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop, &colon))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (!unsafe_key_start && colon)
    {
        DO_CALLBACK_SIM(element_ns, state, state->line, state->col, key_start, colon, colon);
        key_start = colon+1;
    }
    DO_CALLBACK_SIM(element_key, state, state->line, state->col, colon?colon+1:key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
        DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_xml_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == '}') break;

        if (!is_json_quote_start(c))
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }

        // Find the key string, ie speed in { "speed":123 }
        char *new_key_start = NULL;
        char *new_key_stop = NULL;
        eat_json_quote(state, &new_key_start, &new_key_stop);

        eat_xml_whitespace(state, NULL, NULL);
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

        parse_json(state, new_key_start, new_key_stop);
        free(new_key_start);

        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }
    while (c == ',');

    assert(c == '}');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void json_print_value(XMQPrintState *ps, xmlNode *from, xmlNode *to, Level level, bool force_string)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    xmlNode *node = from;
    const char *content = xml_element_content(node);

    if (!xml_next_sibling(node) &&
        !force_string &&
        (json_is_number(xml_element_content(node))
         || json_is_keyword(xml_element_content(node))))
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

        if (is_entity_node(node))
        {
            write(writer_state, "&", NULL);
            write(writer_state, (const char*)node->name, NULL);
            write(writer_state, ";", NULL);
        }
        else
        {
            do
            {
                if (is_entity_node(node))
                {
                    const char *name = xml_element_name(node);
                    print_utf8(ps, COLOR_none, 3, "&", NULL, name, NULL, ";", NULL);
                }
                else
                {
                    const char *value = xml_element_content(node);
                    if (value)
                    {
                        char *quoted_value = xmq_quote_as_c(value, value+strlen(value), false);
                        print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
                        free(quoted_value);
                    }
                }
                if (node == to) break;
                node = xml_next_sibling(node);
            } while (node);
        }

        print_utf8(ps, COLOR_none, 1, "\"", NULL);
        ps->last_char = '"';
    }
}

void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node)
{
    json_check_comma(ps);

    if (container)
    {
        // We have a containing node, then we can print this using "name" : [ ... ]
        json_print_element_name(ps, container, node, 1, 0);
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

    if (from)
    {
        while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
        assert(from != NULL);
    }

    json_print_array_nodes(ps, NULL, (xmlNode*)from, (xmlNode*)to);

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "]", NULL);
    ps->last_char = ']';
}

void json_print_attribute(XMQPrintState *ps, xmlAttr *a)
{
    const char *key;
    const char *prefix;
    size_t total_u_len;
    attr_strlen_name_prefix(a, &key, &prefix, &total_u_len);

    // Do not print "_" attributes since they are the name of the element
    // when the element name is not valid xml.
    if (!strcmp(key, "_")) return;

    json_check_comma(ps);

    char *quoted_key = xmq_quote_as_c(key, key+strlen(key), false);
    print_utf8(ps, COLOR_none, 1, "\"_", NULL);
    if (prefix)
    {
        print_utf8(ps, COLOR_none, 1, prefix, NULL);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }
    print_utf8(ps, COLOR_none, 2, quoted_key, NULL, "\":", NULL);
    free(quoted_key);

    if (a->children != NULL)
    {
        char *value = (char*)xmlNodeListGetString(a->doc, a->children, 1);
        char *quoted_value = xmq_quote_as_c(value, value+strlen(value), true);
        print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
        free(quoted_value);
        xmlFree(value);
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "null", NULL);
    }
}

void json_print_namespace_declaration(XMQPrintState *ps, xmlNs *ns)
{
    const char *prefix;
    size_t total_u_len;

    namespace_strlen_prefix(ns, &prefix, &total_u_len);

    json_check_comma(ps);

    print_utf8(ps, COLOR_none, 1, "\"_xmlns", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_none, 1, ":", NULL);
        print_utf8(ps, COLOR_none, 1, prefix, NULL);
    }
    print_utf8(ps, COLOR_none, 1, "\":", NULL);

    const char *v = xml_namespace_href(ns);

    if (v != NULL)
    {
        print_utf8(ps, COLOR_none, 3, "\"", NULL, v, NULL, "\"", NULL);
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "null", NULL);
    }

}

void json_print_attributes(XMQPrintState *ps,
                           xmlNodePtr node)
{
    xmlAttr *a = xml_first_attribute(node);
    xmlNs *ns = xml_first_namespace_def(node);

    while (a)
    {
        json_print_attribute(ps, a);
        a = xml_next_attribute(a);
    }

    while (ns)
    {
        json_print_namespace_declaration(ps, ns);
        ns = xml_next_namespace_def(ns);
    }
}

void json_print_element_with_children(XMQPrintState *ps,
                                      xmlNode *container,
                                      xmlNode *node,
                                      size_t total,
                                      size_t used)
{
    json_check_comma(ps);

    if (container)
    {
        // We have a containing node, then we can print this using "name" : { ... }
        json_print_element_name(ps, container, node, total, used);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    print_utf8(ps, COLOR_brace_left, 1, "{", NULL);
    ps->last_char = '{';

    ps->line_indent += ps->output_settings->add_indent;

    while (!container && ps->pre_nodes && ps->pre_nodes->size > 0)
    {
        xmlNodePtr node = (xmlNodePtr)stack_rock(ps->pre_nodes);

        if (is_doctype_node(node))
        {
            json_print_doctype_node(ps, node);
        }
        else if (is_comment_node(node))
        {
            json_print_comment_node(ps, node, true, ps->pre_post_num_comments_total, ps->pre_post_num_comments_used++);
        }
        else
        {
            assert(false);
        }
    }

    const char *name = xml_element_name(node);
    bool is_underline = (name[0] == '_' && name[1] == 0);
    if (!container && name && !is_underline)
    {
        // Top level object or object inside array.
        // Hide the name of the object inside the json object with the key "_".
        // I.e. x { a=1 } -> { "_":"x", "a":1 }
        json_check_comma(ps);
        print_utf8(ps, COLOR_none, 1, "\"_\":", NULL);
        ps->last_char = ':';
        json_print_element_name(ps, container, node, total, used);
    }

    json_print_attributes(ps, node);

    if (from)
    {
        while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
        assert(from != NULL);
    }

    json_print_object_nodes(ps, node, (xmlNode*)from, (xmlNode*)to);

    while (!container && ps->post_nodes && ps->post_nodes->size > 0)
    {
        xmlNodePtr node = (xmlNodePtr)stack_rock(ps->post_nodes);

        if (is_comment_node(node))
        {
            json_print_comment_node(ps, node, true, ps->pre_post_num_comments_total, ps->pre_post_num_comments_used++);
        }
        else
        {
            assert(false);
        }
    }

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "}", NULL);
    ps->last_char = '}';
}

void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used)
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

    if (name[0] != '_' || name[1] != 0)
    {
        print_utf8(ps, COLOR_none, 1, name, NULL);
    }
    else
    {
        xmlAttr *a = xml_get_attribute(node, "_");
        if (a)
        {
            // The key was stored inside the attribute because it could not
            // be used as the element name.
            char *value = (char*)xmlNodeListGetString(node->doc, a->children, 1);
            char *quoted_value = xmq_quote_as_c(value, value+strlen(value), false);
            print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
            free(quoted_value);
            xmlFree(value);
            ps->last_char = '"';
        }
    }

    if (total > 1)
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "[%zu]", used);
        print_utf8(ps, COLOR_none, 1, buf, NULL);
    }
    print_utf8(ps, COLOR_none, 1, "\"", NULL);

    ps->last_char = '"';
}

void json_print_key_node(XMQPrintState *ps,
                         xmlNode *container,
                         xmlNode *node,
                         size_t total,
                         size_t used,
                         bool force_string)
{
    json_check_comma(ps);

    if (container)
    {
        json_print_element_name(ps, container, node, total, used);
        print_utf8(ps, COLOR_equals, 1, ":", NULL);
        ps->last_char = ':';
    }

    json_print_value(ps, xml_first_child(node), xml_last_child(node), LEVEL_ELEMENT_VALUE, force_string);
}

void json_check_comma(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == 0) return;

    if (c != '{' && c != '[' && c != ',')
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

void json_print_comment_node(XMQPrintState *ps,
                             xmlNode *node,
                             bool prefix_ul,
                             size_t total,
                             size_t used)
{
    json_check_comma(ps);

    if (prefix_ul) print_utf8(ps, COLOR_equals, 1, "\"_//", NULL);
    else print_utf8(ps, COLOR_equals, 1, "\"//", NULL);

    if (total > 1)
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "[%zu]\":", used);
        print_utf8(ps, COLOR_equals, 1, buf, NULL);
    }
    else
    {
        print_utf8(ps, COLOR_equals, 1, "\":", NULL);
    }
    ps->last_char = ':';
    json_print_value(ps, node, node, LEVEL_XMQ, true);
    ps->last_char = '"';
}

void json_print_doctype_node(XMQPrintState *ps, xmlNodePtr node)
{
    json_check_comma(ps);

    // Print !DOCTYPE inside top level object.
    // I.e. !DOCTYPE=html html { body = a } -> { "!DOCTYPE":"html", "html":{ "body":"a"}}
    print_utf8(ps, COLOR_none, 1, "\"!DOCTYPE\":", NULL);
    ps->last_char = ':';
    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    char *quoted_value = xmq_quote_as_c(c+10, c+strlen(c)-1, true);
    print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
    free(quoted_value);
    xmlBufferFree(buffer);
    ps->last_char = '"';
}

void json_print_entity_node(XMQPrintState *ps, xmlNodePtr node)
{
    json_check_comma(ps);

    const char *name = xml_element_name(node);

    print_utf8(ps, COLOR_none, 3, "\"&\":\"&", NULL, name, NULL, ";\"", NULL);
    ps->last_char = '"';
}

void json_print_standalone_quote(XMQPrintState *ps, xmlNodePtr container, xmlNodePtr node, size_t total, size_t used)
{
    json_check_comma(ps);
    const char *value = xml_element_content(node);
    char *quoted_value = xmq_quote_as_c(value, value+strlen(value), false);
    if (total == 1)
    {
        print_utf8(ps, COLOR_none, 3, "\"|\":\"", NULL, quoted_value, NULL, "\"", NULL);
    }
    else
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "\"|[%zu]\":\"", used);
        print_utf8(ps, COLOR_none, 3, buf, NULL, quoted_value, NULL, "\"", NULL);
    }
    free(quoted_value);
    ps->last_char = '"';
}

bool json_is_number(const char *start)
{
    const char *stop = start+strlen(start);
    return stop == is_jnumber(start, stop);
}

bool json_is_keyword(const char *start)
{
    if (!strcmp(start, "true")) return true;
    if (!strcmp(start, "false")) return true;
    if (!strcmp(start, "null")) return true;
    return false;
}

void json_print_leaf_node(XMQPrintState *ps,
                          xmlNode *container,
                          xmlNode *node,
                          size_t total,
                          size_t used)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    const char *name = xml_element_name(node);

    json_check_comma(ps);

    if (name)
    {
        if (!(name[0] == '_' && name[1] == 0))
        {
            json_print_element_name(ps, container, node, total, used);
            write(writer_state, ":", NULL);
        }
    }

    if (xml_get_attribute(node, "A"))
    {
        write(writer_state, "[]", NULL);
        ps->last_char = ']';
    }
    else
    {
        if (xml_first_attribute(node))
        {
            write(writer_state, "{", NULL);
            ps->last_char = '{';
            json_print_attributes(ps, node);
            write(writer_state, "}", NULL);
            ps->last_char = '}';
        }
        else
        {
            write(writer_state, "{}", NULL);
            ps->last_char = '}';
        }
    }
}

void fixup_json(XMQDoc *doq, xmlNode *node)
{
    if (is_element_node(node))
    {
        char *new_content = xml_collapse_text(node);
        if (new_content)
        {
            xmlNodePtr new_child = xmlNewDocText(doq->docptr_.xml, (const xmlChar*)new_content);
            xmlNode *i = node->children;
            while (i) {
                xmlNode *next = i->next;
                xmlUnlinkNode(i);
                xmlFreeNode(i);
                i = next;
            }
            assert(node);
            assert(new_child);
            xmlAddChild(node, new_child);
            free(new_content);
            return;
        }
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_json(doq, i);
        i = next;
    }
}

void xmq_fixup_json_before_writeout(XMQDoc *doq)
{
    if (doq == NULL || doq->docptr_.xml == NULL) return;
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_json.
        fixup_json(doq, i);
        i = next;
    }
}

void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last)
{
    xmlNodePtr f = *first;
    xmlNodePtr l = *last;
    xmlNodePtr node;

    for (node = f; node && node != l; node = node->next)
    {
        if (is_doctype_node(node) || is_comment_node(node))
        {
            stack_push(ps->pre_nodes, node);
            if (is_comment_node(node)) ps->pre_post_num_comments_total++;
            continue;
        }
        break;
    }

    if (*first != node)
    {
        *first = node;
        f = node;
    }

    for (node = l; node && node != f; node = node->prev)
    {
        if (is_comment_node(node))
        {
            stack_push(ps->post_nodes, node);
            ps->pre_post_num_comments_total++;
            continue;
        }
        break;
    }

    if (*last != node)
    {
        *last = node;
    }
}

#else

// Empty function when XMQ_NO_JSON is defined.
void xmq_fixup_json_before_writeout(XMQDoc *doq)
{
}

// Empty function when XMQ_NO_JSON is defined.
bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    return false;
}

// Empty function when XMQ_NO_JSON is defined.
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
}

void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last)
{
}

void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
}

#endif // JSON_MODULE
