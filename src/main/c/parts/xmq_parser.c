
#ifndef BUILDING_XMQ

#include"always.h"
#include"text.h"
#include"parts/xmq_internals.h"
#include"xmq_parser.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef XMQ_PARSER_MODULE

void eat_xmq_doctype(XMQParseState *state, const char **text_start, const char **text_stop);
void eat_xmq_pi(XMQParseState *state, const char **text_start, const char **text_stop);
void eat_xmq_text_name(XMQParseState *state, const char **content_start, const char **content_stop,
                       const char **namespace_start, const char **namespace_stop);
bool possibly_lost_content_after_equals(XMQParseState *state);
bool possibly_need_more_quotes(XMQParseState *state);

size_t count_xmq_quotes(const char *i, const char *stop)
{
    const char *start = i;

    while (i < stop && *i == '\'') i++;

    return i-start;
}

void eat_xmq_quote(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    size_t depth = count_xmq_quotes(i, end);
    size_t count = depth;

    state->last_quote_start = state->i;
    state->last_quote_start_line = state->line;
    state->last_quote_start_col = state->col;

    *start = i;

    while (count > 0)
    {
        increment('\'', 1, &i, &line, &col);
        count--;
    }

    if (depth == 2)
    {
        // The empty quote ''
        state->i = i;
        state->line = line;
        state->col = col;
        *stop = i;
        return;
    }

    while (i < end)
    {
        char c = *i;
        if (c != '\'')
        {
            increment(c, 1, &i, &line, &col);
            continue;
        }
        size_t count = count_xmq_quotes(i, end);
        if (count > depth)
        {
            state->error_nr = XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES;
            longjmp(state->error_handler, 1);
        }
        else
        if (count < depth)
        {
            while (count > 0)
            {
                increment('\'', 1, &i, &line, &col);
                count--;
            }
            continue;
        }
        else
        if (count == depth)
        {
            while (count > 0)
            {
                increment('\'', 1, &i, &line, &col);
                count--;
            }
            depth = 0;
            *stop = i;
            break;
        }
    }
    if (depth != 0)
    {
        state->error_nr = XMQ_ERROR_QUOTE_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }
    state->i = i;
    state->line = line;
    state->col = col;

    if (possibly_need_more_quotes(state))
    {
        state->last_suspicios_quote_end = state->i-1;
        state->last_suspicios_quote_end_line = state->line;
        state->last_suspicios_quote_end_col = state->col-1;
    }
}

void eat_xmq_entity(XMQParseState *state)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;

    size_t line = state->line;
    size_t col = state->col;
    increment('&', 1, &i, &line, &col);

    char c = 0;
    bool expect_semicolon = false;

    while (i < end)
    {
        c = *i;
        if (!is_xmq_text_name(c)) break;
        if (!is_lowercase_hex(c)) expect_semicolon = true;
        increment(c, 1, &i, &line, &col);
    }
    if (c == ';')
    {
        increment(c, 1, &i, &line, &col);
        c = *i;
        expect_semicolon = false;
    }
    if (expect_semicolon)
    {
        state->error_nr = XMQ_ERROR_ENTITY_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }

    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_comment_to_eol(XMQParseState *state, const char **comment_start, const char **comment_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;

    size_t line = state->line;
    size_t col = state->col;
    increment('/', 1, &i, &line, &col);
    increment('/', 1, &i, &line, &col);

    *comment_start = i;

    char c = 0;
    while (i < end && c != '\n')
    {
        c = *i;
        increment(c, 1, &i, &line, &col);
    }
    if (c == '\n') *comment_stop = i-1;
    else *comment_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_comment_to_close(XMQParseState *state, const char **comment_start, const char **comment_stop,
                              size_t num_slashes,
                              bool *found_asterisk)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;

    size_t line = state->line;
    size_t col = state->col;
    size_t n = num_slashes;

    if (*i == '/')
    {
        // Comment starts from the beginning ////* ....
        // Otherwise this is a continuation and *i == '*'
        while (n > 0)
        {
            assert(*i == '/');
            increment('/', 1, &i, &line, &col);
            n--;
        }
    }
    assert(*i == '*');
    increment('*', 1, &i, &line, &col);

    *comment_start = i;

    char c = 0;
    char cc = 0;
    n = 0;
    while (i < end)
    {
        cc = c;
        c = *i;
        if (cc != '*' || c != '/')
        {
            // Not a possible end marker */ or *///// continue eating.
            increment(c, 1, &i, &line, &col);
            continue;
        }
        // We have found */ or *//// not count the number of slashes.
        n = count_xmq_slashes(i, end, found_asterisk);

        if (n < num_slashes) continue; // Not a balanced end marker continue eating,

        if (n > num_slashes)
        {
            // Oups, too many slashes.
            state->error_nr = XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES;
            longjmp(state->error_handler, 1);
        }

        assert(n == num_slashes);
        // Found the ending slashes!
        *comment_stop = i-1;
        while (n > 0)
        {
            cc = c;
            c = *i;
            assert(*i == '/');
            increment(c, 1, &i, &line, &col);
            n--;
        }
        state->i = i;
        state->line = line;
        state->col = col;
        return;
    }
    // We reached the end of the xmq and no */ was found!
    state->error_nr = XMQ_ERROR_COMMENT_NOT_CLOSED;
    longjmp(state->error_handler, 1);
}

void eat_xmq_text_name(XMQParseState *state,
                       const char **text_start,
                       const char **text_stop,
                       const char **namespace_start,
                       const char **namespace_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    const char *colon = NULL;
    size_t line = state->line;
    size_t col = state->col;

    *text_start = i;

    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        if (c == ':') colon = i;
        increment(c, 1, &i, &line, &col);
    }

    if (colon)
    {
        *namespace_start = *text_start;
        *namespace_stop = colon;
        *text_start = colon+1;
    }
    else
    {
        *namespace_start = NULL;
        *namespace_stop = NULL;
    }
    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_text_value(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    while (i < stop)
    {
        char c = *i;
        if (!is_xmq_text_value_char(i, stop)) break;
        increment(c, 1, &i, &line, &col);
    }

    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_doctype(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    assert(*i == '!');
    increment('!', 1, &i, &line, &col);
    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        increment(c, 1, &i, &line, &col);
    }


    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_pi(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    assert(*i == '?');
    increment('?', 1, &i, &line, &col);
    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        increment(c, 1, &i, &line, &col);
    }

    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

bool is_xmq_quote_start(char c)
{
    return c == '\'';
}

bool is_xmq_entity_start(char c)
{
    return c == '&';
}

bool is_xmq_attribute_key_start(char c)
{
    bool t =
        c == '\'' ||
        c == '"' ||
        c == '(' ||
        c == ')' ||
        c == '{' ||
        c == '}' ||
        c == '/' ||
        c == '=' ||
        c == '&';

    return !t;
}

bool is_xmq_compound_start(char c)
{
    return (c == '(');
}

bool is_xmq_comment_start(char c, char cc)
{
    return c == '/' && (cc == '/' || cc == '*');
}

bool is_xmq_pi_start(const char *start, const char *stop)
{
    if (*start != '?') return false;
    // We need at least one character, eg. ?x
    if (start+2 > stop) return false;
    return true;
}

bool is_xmq_doctype_start(const char *start, const char *stop)
{
    if (*start != '!') return false;
    // !DOCTYPE
    if (start+8 > stop) return false;
    if (strncmp(start, "!DOCTYPE", 8)) return false;
    // Ooups, !DOCTYPE must have some value.
    if (start+8 == stop) return false;
    char c = *(start+8);
    // !DOCTYPE= or !DOCTYPE = etc
    if (c != '=' && c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
    return true;
}

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk)
{
    const char *start = i;

    while (i < stop && *i == '/') i++;

    if (*i == '*') *found_asterisk = true;
    else *found_asterisk = false;

    return i-start;
}

bool is_xmq_text_value_char(const char *i, const char *stop)
{
    char c = *i;
    if (count_whitespace(i, stop) > 0 ||
        c == '\'' ||
        c == '"' ||
        c == '(' ||
        c == ')' ||
        c == '{' ||
        c == '}')
    {
        return false;
    }
    return true;
}

bool is_xmq_text_value(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (!is_xmq_text_value_char(i, stop))
        {
            return false;
        }
    }
    return true;
}


bool peek_xmq_next_is_equal(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    char c = 0;

    while (i < stop)
    {
        c = *i;
        if (!is_xml_whitespace(c)) break;
        i++;
    }
    return c == '=';
}

void parse_xmq(XMQParseState *state)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);
        char cc = 0;
        if ((c == '/' || c == '(') && state->i+1 < end) cc = *(state->i+1);

        if (is_xmq_token_whitespace(c)) parse_xmq_whitespace(state);
        else if (is_xmq_quote_start(c)) parse_xmq_quote(state, LEVEL_XMQ);
        else if (is_xmq_entity_start(c)) parse_xmq_entity(state, LEVEL_XMQ);
        else if (is_xmq_comment_start(c, cc)) parse_xmq_comment(state, cc);
        else if (is_xmq_element_start(c)) parse_xmq_element(state);
        else if (is_xmq_doctype_start(state->i, end)) parse_xmq_doctype(state);
        else if (is_xmq_pi_start(state->i, end)) parse_xmq_pi(state);
        else if (c == '}') return;
        else
        {
            if (possibly_lost_content_after_equals(state))
            {
                state->error_nr = XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS;
            }
            else if (c == '\t')
            {
                state->error_nr = XMQ_ERROR_UNEXPECTED_TAB;
            }
            else
            {
                state->error_nr = XMQ_ERROR_INVALID_CHAR;
            }
            longjmp(state->error_handler, 1);
        }
    }
}

void parse_xmq_quote(XMQParseState *state, Level level)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;

    eat_xmq_quote(state, &start, &stop);

    switch(level)
    {
    case LEVEL_XMQ:
       DO_CALLBACK(quote, state, start_line, start_col, start, stop, stop);
       break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_quote, state, start_line, start_col, start, stop, stop);
        break;
    default:
        assert(false);
    }
}

void parse_xmq_entity(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_xmq_entity(state);
    const char *stop = state->i;

    switch (level) {
    case LEVEL_XMQ:
        DO_CALLBACK(entity, state, start_line, start_col, start,  stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_entity, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_entity, state, start_line, start_col, start,  stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_entity, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_entity, state, start_line, start_col, start, stop, stop);
        break;
    default:
        assert(false);
    }
}

void parse_xmq_comment(XMQParseState *state, char cc)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *comment_start;
    const char *comment_stop;
    bool found_asterisk = false;

    size_t n = count_xmq_slashes(start, state->buffer_stop, &found_asterisk);

    if (!found_asterisk)
    {
        // This is a single line asterisk.
        eat_xmq_comment_to_eol(state, &comment_start, &comment_stop);
        const char *stop = state->i;
        DO_CALLBACK(comment, state, start_line, start_col, start, stop, stop);
    }
    else
    {
        // This is a /* ... */ or ////*  ... *//// comment.
        eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
        const char *stop = state->i;
        DO_CALLBACK(comment, state, start_line, start_col, start, stop, stop);

        while (found_asterisk)
        {
            // Aha, this is a comment continuation /* ... */* ...
            start = state->i;
            start_line = state->line;
            start_col = state->col;
            eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
            stop = state->i;
            DO_CALLBACK(comment_continuation, state, start_line, start_col, start, stop, stop);
        }
    }
}

void parse_xmq_text_value(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_xmq_text_value(state);
    const char *stop = state->i;

    assert(level != LEVEL_XMQ);
    if (level == LEVEL_ATTR_VALUE)
    {
        DO_CALLBACK(attr_value_text, state, start_line, start_col, start, stop, stop);
    }
    else
    {
        DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
    }
}

void parse_xmq_value(XMQParseState *state, Level level)
{
    char c = *state->i;

    if (is_xml_whitespace(c))
    {
        parse_xmq_whitespace(state);
        c = *state->i;
    }

    if (is_xmq_quote_start(c))
    {
        parse_xmq_quote(state, level);
    }
    else if (is_xmq_entity_start(c))
    {
        parse_xmq_entity(state, level);
    }
    else if (is_xmq_compound_start(c))
    {
        parse_xmq_compound(state, level);
    }
    else
    {
        parse_xmq_text_value(state, level);
    }
}

void parse_xmq_element_internal(XMQParseState *state, bool doctype, bool pi)
{
    char c = 0;
    // Name
    const char *name_start = NULL;
    const char *name_stop = NULL;
    // Namespace
    const char *ns_start = NULL;
    const char *ns_stop = NULL;

    size_t start_line = state->line;
    size_t start_col = state->col;

    if (doctype)
    {
        eat_xmq_doctype(state, &name_start, &name_stop);
    }
    else if (pi)
    {
        eat_xmq_pi(state, &name_start, &name_stop);
    }
    else
    {
        eat_xmq_text_name(state, &name_start, &name_stop, &ns_start, &ns_stop);
    }
    const char *stop = state->i;

    // The only peek ahead in the whole grammar! And its only for syntax coloring. :-)
    // key = 123   vs    name { '123' }
    bool is_key = peek_xmq_next_is_equal(state);

    if (!ns_start)
    {
        // Normal key/name element.
        if (is_key)
        {
            DO_CALLBACK(element_key, state, start_line, start_col, name_start, name_stop, stop);
        }
        else
        {
            DO_CALLBACK(element_name, state, start_line, start_col, name_start, name_stop, stop);
        }
    }
    else
    {
        // We have a namespace prefixed to the element, eg: abc:working
        size_t ns_len = ns_stop - ns_start;
        DO_CALLBACK(element_ns, state, start_line, start_col, ns_start, ns_stop, ns_stop);
        DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);

        if (is_key)
        {
            DO_CALLBACK(element_key, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
        else
        {
            DO_CALLBACK(element_name, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
    }


    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '(')
    {
        const char *start = state->i;
        state->last_attr_start = state->i;
        state->last_attr_start_line = state->line;
        state->last_attr_start_col = state->col;
        start_line = state->line;
        start_col = state->col;
        increment('(', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(apar_left, state, start_line, start_col, start, stop, stop);

        parse_xmq_attributes(state);

        c = *state->i;
        if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }
        if (c != ')')
        {
            state->error_nr = XMQ_ERROR_ATTRIBUTES_NOT_CLOSED;
            longjmp(state->error_handler, 1);
        }

        start = state->i;
        const char *parentheses_right_start = state->i;
        const char *parentheses_right_stop = state->i+1;

        start_line = state->line;
        start_col = state->col;
        increment(')', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(apar_right, state, start_line, start_col, parentheses_right_start, parentheses_right_stop, stop);
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        state->last_equals_start = state->i;
        state->last_equals_start_line = state->line;
        state->last_equals_start_col = state->col;
        const char *start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;

        DO_CALLBACK(equals, state, start_line, start_col, start, stop, stop);
        parse_xmq_value(state, LEVEL_ELEMENT_VALUE);
        return;
    }

    if (c == '{')
    {
        const char *start = state->i;
        state->last_body_start = state->i;
        state->last_body_start_line = state->line;
        state->last_body_start_col = state->col;
        start_line = state->line;
        start_col = state->col;
        increment('{', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(brace_left, state, start_line, start_col, start, stop, stop);

        parse_xmq(state);
        c = *state->i;
        if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }
        if (c != '}')
        {
            state->error_nr = XMQ_ERROR_BODY_NOT_CLOSED;
            longjmp(state->error_handler, 1);
        }

        start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('}', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(brace_right, state, start_line, start_col, start, stop, stop);
    }
}

void parse_xmq_element(XMQParseState *state)
{
    parse_xmq_element_internal(state, false, false);
}

void parse_xmq_doctype(XMQParseState *state)
{
    parse_xmq_element_internal(state, true, false);
}

void parse_xmq_pi(XMQParseState *state)
{
    parse_xmq_element_internal(state, false, true);
}

/** Parse a list of attribute key = value, or just key children until a ')' is found. */
void parse_xmq_attributes(XMQParseState *state)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) parse_xmq_whitespace(state);
        else if (c == ')') return;
        else if (is_xmq_attribute_key_start(c)) parse_xmq_attribute(state);
        else break;
    }
}

void parse_xmq_attribute(XMQParseState *state)
{
    char c = 0;
    const char *name_start;
    const char *name_stop;
    const char *ns_start = NULL;
    const char *ns_stop = NULL;

    int start_line = state->line;
    int start_col = state->col;

    eat_xmq_text_name(state, &name_start, &name_stop, &ns_start, &ns_stop);
    const char *stop = state->i;

    if (!ns_start)
    {
        // No colon found, we have either a normal: key=123
        // or a default namespace declaration xmlns=...
        size_t len = name_stop - name_start;
        if (len == 5 && !strncmp(name_start, "xmlns", 5))
        {
            // A default namespace declaration, eg: xmlns=uri
            DO_CALLBACK(ns_declaration, state, start_line, start_col, name_start, name_stop, name_stop);
        }
        else
        {
            // A normal attribute key, eg: width=123
            DO_CALLBACK(attr_key, state, start_line, start_col, name_start, name_stop, stop);
        }
    }
    else
    {
        // We have a colon in the attribute key.
        // E.g. alfa:beta where alfa is attr_ns and beta is attr_key
        // However we can also have xmlns:xsl then it gets tokenized as ns_declaration and attr_ns.
        size_t ns_len = ns_stop - ns_start;
        if (ns_len == 5 && !strncmp(ns_start, "xmlns", 5))
        {
            // The xmlns signals a declaration of a namespace.
            DO_CALLBACK(ns_declaration, state, start_line, start_col, ns_start, ns_stop, name_stop);
            DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
            DO_CALLBACK(attr_ns, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
        else
        {
            // Normal namespaced attribute. Please try to avoid namespaced attributes because you only need to attach the
            // namespace to the element itself, from that follows automatically the unique namespaced attributes.
            // The exception being special use cases as: xlink:href.
            DO_CALLBACK(attr_ns, state, start_line, start_col, ns_start, ns_stop, ns_stop);
            DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
            DO_CALLBACK(attr_key, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        const char *start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(equals, state, start_line, start_col, start, stop, stop);
        parse_xmq_value(state, LEVEL_ATTR_VALUE);
        return;
    }
}

/** Parse a compound value, ie:  = ( '   ' &#10; '  info ' )
    a compound can only occur after an = (equals) character.
    The normal quoting with single quotes, is enough for all quotes except:
    1) An attribute value with leading/ending whitespace including leading/ending newlines.
    2) An attribute with a mix of quotes and referenced entities.
    3) Compact xmq where actual newlines have to be replaced with &#10;

    Note that an element key = ( ... ) can always  be replaced with key { ... }
    so compound values are not strictly necessary for element key values.
    However they are permitted for symmetry reasons.
*/
void parse_xmq_compound(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    increment('(', 1, &state->i, &state->line, &state->col);
    const char *stop = state->i;
    DO_CALLBACK(cpar_left, state, start_line, start_col, start, stop, stop);

    parse_xmq_compound_children(state, enter_compound_level(level));

    char c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c != ')')
    {
        state->error_nr = XMQ_ERROR_COMPOUND_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }

    start = state->i;
    start_line = state->line;
    start_col = state->col;
    increment(')', 1, &state->i, &state->line, &state->col);
    stop = state->i;
    DO_CALLBACK(cpar_right, state, start_line, start_col, start, stop, stop);
}

/** Parse each compound child (quote or entity) until end of file or a ')' is found. */
void parse_xmq_compound_children(XMQParseState *state, Level level)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) parse_xmq_whitespace(state);
        else if (c == ')') break;
        else if (is_xmq_quote_start(c)) parse_xmq_quote(state, level);
        else if (is_xmq_entity_start(c)) parse_xmq_entity(state, level);
        else
        {
            state->error_nr = XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN;
            longjmp(state->error_handler, 1);
        }
    }
}

bool possibly_lost_content_after_equals(XMQParseState *state)
{
    char c = *(state->i);

    // Look for unexpected = before 123 since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta = 123
    // Look for unexpected { since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta { 'more' }
    // Look for unexpected ( since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta(attr)

    // Not {(= then not lost content, probably.
    if (!(c == '{' || c == '(' || c == '=')) return false;

    const char *i = state->i-1;
    const char *start = state->buffer_start;

    // Scan backwards for newline accepting only texts and xml whitespace.
    while (i > start && *i != '\n' && (is_xmq_text_name(*i) || is_xml_whitespace(*i))) i--;
    if (i == start || *i != '\n') return false;

    // We found the newline lets see if the next character backwards is equals...
    while (i > start
           &&
           is_xml_whitespace(*i))
    {
        i--;
    }

    return *i == '=';
}

bool possibly_need_more_quotes(XMQParseState *state)
{
    if (state->i - 2 < state->buffer_start ||
        state->i >= state->buffer_stop)
    {
        return false;
    }
    // Should have triple quotes: 'There's a man.'
    // c0 = e
    // c1 = '
    // c2 = s
    char c0 = *(state->i-2);
    char c1 = *(state->i-1); // This is the apostrophe
    char c2 = *(state->i);

    // We have just parsed a quote. Check if this is a false ending and
    // there is a syntax error since we need more quotes. For example:
    // greeting = 'There's a man, a wolf and a boat.'
    // We get this error:
    // ../forgot.xmq:1:26: error: unexpected character "," U+2C
    // greeting = 'There's a man, a wolf and a boat.'
    //                          ^
    // The quote terminated too early, we need three quotes.
    //
    // This function detects a suspicious quote ending and remembers it,
    // but does not flag an error until the parser fails.

    // Any non-quote quote non-quote, is suspicios: for example: g's t's
    // or e'l or y'v etc....
    // But do not trigger on [space]'x since that is probably a valid quote start.
    if (c0 != '\'' &&
        c0 != ' ' &&
        c1 == '\'' &&
        c2 != '\'') return true;

    return false;

    // isn't doesn't shouldn't can't aren't won't
    // dog's it's
    // we'll
    // they've
    // he'd
    // she'd've
    // 'clock
    // Hallowe'en
    // fo'c's'le = forecastle
    // cat-o'-nine-tails = cat-of-nine-tails
    // ne'er-do-well = never-do-well
    // will-o'-the-wisp
    // 'tis = it is
    // o'er = over
    // 'twas = it was
    // e'en = even
    // 'Fraid so.'Nother drink?
    // I s'pose so.'S not funny.
    // O'Leary (Irish), d'Abbadie (French), D'Angelo (Italian), M'Tavish (Scots Gaelic)
    // Robert Burns poetry: gi' for give and a' for all
    // the generation of '98
    // James's shop (or James' shop)
    // a month's pay
    // For God's sake! (= exclamation of exasperation)
    // a stone's throw away (= very near)
    // at death's door (= very ill)
    // in my mind's eye (= in my imagination)
}

void parse_xmq_whitespace(XMQParseState *state)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;
    eat_xmq_token_whitespace(state, &start, &stop);
    DO_CALLBACK(whitespace, state, start_line, start_col, start, stop, stop);
}

#endif // XMQ_PARSER_MODULE
