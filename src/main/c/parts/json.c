
#ifndef BUILDING_XMQ

#include"xmq.h"
#include"stack.h"
#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef JSON_MODULE

void handle_json_whitespace(XMQParseState *state)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;
    eat_whitespace(state, &start, &stop);
    //DO_CALLBACK(whitespace, state, start_line, start_col, start, start_col, start, stop, stop);
}

void handle_json_quote(XMQParseState *state)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    size_t depth = eat_json_quote(state, &content_start, &content_stop);
    const char *stop = state->i;
    size_t content_start_col = start_col+depth;
    //DO_CALLBACK(content_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);

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
    const char *stop = state->buffer_stop;
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

void handle_json_boolean(XMQParseState *state)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_boolean(state, &content_start, &content_stop);
    const char *stop = state->i;
    //DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
}

bool is_json_number(XMQParseState *state)
{
    char c = *state->i;

    return c >= '0' && c <='9';
}

void eat_json_number(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    while (i < stop)
    {
        char c = *i;
        if (! (c >= '0' && c <= '9')) break;
        increment(c, 1, &i, &line, &col);
    }

    state->i = i;
    state->line = line;
    state->col = col;
}

void handle_json_number(XMQParseState *state)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_number(state, &content_start, &content_stop);
    const char *stop = state->i;
    //DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
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
        //handle_json_nodes(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_UNEXPECTED_CLOSING_BRACE;
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        // Error detected
        PRINT_ERROR("Error while parsing json (errno %d) %s %zu:%zu\n", state->error_nr, state->generated_error_msg, state->line, state->col);
    }

    if (state->parse->done) state->parse->done(state);
    return true;
}

bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();
//    xmq_setup_parse_json_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->doq = doq;

    xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)("_"), NULL);
    push_stack(state->element_stack, root);
    xmlDocSetRootElement(state->doq->docptr_.xml, root);
    state->element_last = state->element_stack->top->data;

    // Tokenize the buffer and invoke the parse callbacks.
    xmqTokenizeBuffer(state, start, stop);

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(output_settings);

    return true;
}

void handle_json_array(XMQParseState *state)
{
    char c = *state->i;
    assert(c == '[');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, underline, state->col, underline, underline+1, underline+1);
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, state->col, leftpar, leftpar+1, leftpar+1);
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, array, state->col, array, array+1, array+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, state->col, rightpar, rightpar+1, rightpar+1);

    //handle_json_nodes(state);

    c = *state->i;
    assert(c == ']');
    increment(c, 1, &state->i, &state->line, &state->col);
}

void handle_json_nodes(XMQParseState *state)
{
    const char *stop = state->buffer_stop;

    while (state->i < stop)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) handle_json_whitespace(state);
        else if (is_json_quote_start(c)) handle_json_quote(state);
        else if (is_json_boolean(state)) handle_json_boolean(state);
        else if (is_json_number(state)) handle_json_number(state);
        else if (c == '[') handle_json_array(state);
        else if (c == ']') break;
        else
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }
    }
}

void handle_json_node(XMQParseState *state)
{
    char c = 0;
    const char *name = "_";
    const char *name_start = name;
    const char *name_stop = name+1;

    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    eat_xmq_text_name(state, &name_start, &name_stop);
    const char *stop = state->i;
}

#else

bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    return false;
}

#endif // JSON_MODULE
