#ifndef BUILDING_XMQ

#include"parts/utils.h"
#include"parts/xmq_internals.h"

#endif

#ifdef XMQ_INTERNALS_MODULE

const char *color_names[13] = {
    "xmq_c",
    "xmq_q",
    "xmq_e",
    "xmq_ens",
    "xmq_en",
    "xmq_ek",
    "xmq_ekv",
    "xmq_ans",
    "xmq_ak",
    "xmq_akv",
    "xmq_cp",
    "xmq_uw",
    "xmq_tw",
};

void build_state_error_message(XMQParseState *state, const char *start, const char *stop)
{
    // Error detected during parsing and this is where the longjmp will end up!
    state->generated_error_msg = (char*)malloc(2048);

    XMQParseError error_nr = (XMQParseError)state->error_nr;
    const char *error = xmqParseErrorToString(error_nr);

    const char *statei = state->i;
    size_t line = state->line;
    size_t col = state->col;

    if (error_nr == XMQ_ERROR_BODY_NOT_CLOSED)
    {
        statei = state->last_body_start;
        line = state->last_body_start_line;
        col = state->last_body_start_col;
    }
    if (error_nr == XMQ_ERROR_ATTRIBUTES_NOT_CLOSED)
    {
        statei = state->last_attr_start;
        line = state->last_attr_start_line;
        col = state->last_attr_start_col;
    }
    if (error_nr == XMQ_ERROR_QUOTE_NOT_CLOSED)
    {
        statei = state->last_quote_start;
        line = state->last_quote_start_line;
        col = state->last_quote_start_col;
    }
    if (error_nr == XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS)
    {
        statei = state->last_equals_start;
        line = state->last_equals_start_line;
        col = state->last_equals_start_col;
    }

    size_t n = 0;
    size_t offset = 0;
    const char *line_start = statei;
    while (line_start > start && *(line_start-1) != '\n' && n < 1024)
    {
        n++;
        offset++;
        line_start--;
    }

    const char *i = statei;
    while (i < stop && *i && *i != '\n' && n < 1024)
    {
        n++;
        i++;
    }
    const char *char_error = "";
    char buf[32];

    if (error_nr == XMQ_ERROR_INVALID_CHAR ||
        error_nr == XMQ_ERROR_JSON_INVALID_CHAR)
    {
        UTF8Char utf8_char;
        peek_utf8_char(statei, stop, &utf8_char);
        char utf8_codepoint[8];
        utf8_char_to_codepoint_string(&utf8_char, utf8_codepoint);

        snprintf(buf, 32, " \"%s\" %s", utf8_char.bytes, utf8_codepoint);
        char_error = buf;
    }

    char line_error[1024];
    line_error[0] = 0;
    if (statei < stop)
    {
        snprintf(line_error, 1024, "\n%.*s\n %*s",
                 (int)n,
                 line_start,
                 (int)offset,
                 "^");
    }

    snprintf(state->generated_error_msg, 2048,
             "%s:%zu:%zu: error: %s%s%s",
             state->source_name,
             line, col,
             error,
             char_error,
             line_error
        );
    state->generated_error_msg[2047] = 0;
}

size_t count_whitespace(const char *i, const char *stop)
{
    unsigned char c = *i;
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
    {
        return 1;
    }

    if (i+1 >= stop) return 0;

    // If not a unbreakable space U+00A0 (utf8 0xc2a0)
    // or the other unicode whitespaces (utf8 starts with 0xe2)
    // then we have not whitespaces here.
    if (c != 0xc2 && c != 0xe2) return 0;

    unsigned char cc = *(i+1);

    if (c == 0xC2 && cc == 0xA0)
    {
        // Unbreakable space. 0xA0
        return 2;
    }
    if (c == 0xE2 && cc == 0x80)
    {
        if (i+2 >= stop) return 0;

        unsigned char ccc = *(i+2);

        if (ccc == 0x80)
        {
            // EN quad. &#8192; U+2000 utf8 E2 80 80
            return 3;
        }
        if (ccc == 0x81)
        {
            // EM quad. &#8193; U+2001 utf8 E2 80 81
            return 3;
        }
        if (ccc == 0x82)
        {
            // EN space. &#8194; U+2002 utf8 E2 80 82
            return 3;
        }
        if (ccc == 0x83)
        {
            // EM space. &#8195; U+2003 utf8 E2 80 83
            return 3;
        }
    }
    return 0;
}

void eat_whitespace(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *buffer_stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    if (start) *start = i;

    size_t nw = count_whitespace(i, buffer_stop);
    if (!nw) return;

    while (i < buffer_stop)
    {
        size_t nw = count_whitespace(i, buffer_stop);
        if (!nw) break;
        // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
        increment(*i, nw, &i, &line, &col);
    }

    if (stop) *stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

/**
   get_color: Lookup the color strings
   coloring: The table of colors.
   c: The color to use from the table.
   pre: Store a pointer to the start color string here.
   post: Store a pointer to the end color string here.
*/
void get_color(XMQColoring *coloring, XMQColor c, const char **pre, const char **post)
{
    switch(c)
    {
#define X(TYPE) case COLOR_##TYPE: *pre = coloring->TYPE.pre; *post = coloring->TYPE.post; return;
LIST_OF_XMQ_TOKENS
#undef X
    case COLOR_unicode_whitespace: *pre = coloring->unicode_whitespace.pre; *post = coloring->unicode_whitespace.post; return;
    case COLOR_indentation_whitespace: *pre = coloring->indentation_whitespace.pre; *post = coloring->indentation_whitespace.post; return;
    default:
        *pre = NULL;
        *post = NULL;
        return;
    }
    assert(false);
    *pre = "";
    *post = "";
}

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col)
{
    if ((c & 0xc0) != 0x80) // Just ignore UTF8 parts since they do not change the line or col.
    {
        (*col)++;
        if (c == '\n')
        {
            (*line)++;
            (*col) = 1;
        }
    }
    assert(num_bytes > 0);
    (*i)+=num_bytes;
}

bool is_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

bool is_unicode_whitespace(const char *start, const char *stop)
{
    size_t n = count_whitespace(start, stop);

    // Single char whitespace is ' ' '\t' '\n' '\r'
    // First unicode whitespace is 160 nbsp require two or more chars.
    return n > 1;
}

const char *needs_escape(XMQRenderFormat f, const char *start, const char *stop)
{
    if (f == XMQ_RENDER_HTML)
    {
        char c = *start;
        if (c == '&') return "&amp;";
        if (c == '<') return "&lt;";
        if (c == '>') return "&gt;";
        return NULL;
    }
    else if (f == XMQ_RENDER_TEX)
    {
        char c = *start;
        if (c == '\\') return "\\backslash;";
        if (c == '&') return "\\&";
        if (c == '#') return "\\#";
        if (c == '{') return "\\{";
        if (c == '}') return "\\}";
        if (c == '_') return "\\_";
        if (c == '\'') return "{'}";

        return NULL;
    }

    return NULL;
}

void print_color_pre(XMQPrintState *ps, XMQColor c)
{
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    if (pre)
    {
        XMQWrite write = ps->output_settings->content.write;
        void *writer_state = ps->output_settings->content.writer_state;
        write(writer_state, pre, NULL);
    }
}

void print_color_post(XMQPrintState *ps, XMQColor c)
{
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    if (post)
    {
        XMQWrite write = ps->output_settings->content.write;
        void *writer_state = ps->output_settings->content.writer_state;
        write(writer_state, post, NULL);
    }
}

size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;

    const char *i = start;

    // Find next utf8 char....
    const char *j = i+1;
    while (j < stop && (*j & 0xc0) == 0x80) j++;

    // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
    bool uw = is_unicode_whitespace(i, j);

    // If so, then color it. This will typically red underline the non-breakable space.
    if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

    if (*i == ' ')
    {
        write(writer_state, ps->output_settings->coloring.explicit_space, NULL);
    }
    else
    {
        const char *e = needs_escape(ps->output_settings->render_to, i, j);
        if (!e)
        {
            write(writer_state, i, j);
        }
        else
        {
            write(writer_state, e, NULL);
        }
    }
    if (uw) print_color_post(ps, COLOR_unicode_whitespace);

    ps->last_char = *i;
    ps->current_indent++;

    return j-start;
}

/**
   print_utf8_internal: Print a single string
   ps: The print state.
   start: Points to bytes to be printed.
   stop: Points to byte after last byte to be printed. If NULL then assume start is null-terminated.

   Returns number of bytes printed.
*/
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;

    size_t u_len = 0;

    const char *i = start;
    while (*i && (!stop || i < stop))
    {
        // Find next utf8 char....
        const char *j = i+1;
        while (j < stop && (*j & 0xc0) == 0x80) j++;

        // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
        bool uw = is_unicode_whitespace(i, j);

        // If so, then color it. This will typically red underline the non-breakable space.
        if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

        if (*i == ' ')
        {
            write(writer_state, ps->output_settings->coloring.explicit_space, NULL);
        }
        else
        {
            const char *e = needs_escape(ps->output_settings->render_to, i, j);
            if (!e)
            {
                write(writer_state, i, j);
            }
            else
            {
                write(writer_state, e, NULL);
            }
        }
        if (uw) print_color_post(ps, COLOR_unicode_whitespace);
        u_len++;
        i = j;
    }

    ps->last_char = *(i-1);
    ps->current_indent += u_len;
    return i-start;
}

/**
   print_utf8:
   @ps: The print state.
   @c:  The color.
   @num_pairs:  Number of start, stop pairs.
   @start: First utf8 byte to print.
   @stop: Points to byte after the last utf8 content.

   Returns the number of bytes used after start.
*/
size_t print_utf8(XMQPrintState *ps, XMQColor c, size_t num_pairs, ...)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;
    XMQColoring *coloring = &ps->output_settings->coloring;

    const char *pre, *post;
    get_color(coloring, c, &pre, &post);

    if (pre) write(writer_state, pre, NULL);

    size_t b_len = 0;

    va_list ap;
    va_start(ap, num_pairs);
    for (size_t x = 0; x < num_pairs; ++x)
    {
        const char *start = va_arg(ap, const char *);
        const char *stop = va_arg(ap, const char *);
        b_len += print_utf8_internal(ps, start, stop);
    }
    va_end(ap);

    if (post) write(writer_state, post, NULL);

    return b_len;
}

const char *xmqParseErrorToString(XMQParseError e)
{
    switch (e)
    {
    case XMQ_ERROR_CANNOT_READ_FILE: return "cannot read file";
    case XMQ_ERROR_NOT_XMQ: return "input file is not xmq";
    case XMQ_ERROR_QUOTE_NOT_CLOSED: return "quote is not closed";
    case XMQ_ERROR_ENTITY_NOT_CLOSED: return "entity is not closed";
    case XMQ_ERROR_COMMENT_NOT_CLOSED: return "comment is not closed";
    case XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES: return "comment closed with too many slashes";
    case XMQ_ERROR_BODY_NOT_CLOSED: return "body is not closed";
    case XMQ_ERROR_ATTRIBUTES_NOT_CLOSED: return "attributes are not closed";
    case XMQ_ERROR_COMPOUND_NOT_CLOSED: return "compound is not closed";
    case XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN: return "compound may only contain quotes and entities";
    case XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES: return "quote closed with too many quotes";
    case XMQ_ERROR_UNEXPECTED_CLOSING_BRACE: return "unexpected closing brace";
    case XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS: return "expected content after equals";
    case XMQ_ERROR_INVALID_CHAR: return "unexpected character";
    case XMQ_ERROR_BAD_DOCTYPE: return "doctype could not be parsed";
    case XMQ_ERROR_CANNOT_HANDLE_XML: return "cannot handle xml use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_HTML: return "cannot handle html use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_JSON: return "cannot handle json use libxmq-all for this!";
    case XMQ_ERROR_JSON_INVALID_ESCAPE: return "invalid json escape";
    case XMQ_ERROR_JSON_INVALID_CHAR: return "unexpected json character";
    case XMQ_ERROR_EXPECTED_XMQ: return "expected xmq source";
    case XMQ_ERROR_EXPECTED_HTMQ: return "expected htmlq source";
    case XMQ_ERROR_EXPECTED_XML: return "expected xml source";
    case XMQ_ERROR_EXPECTED_HTML: return "expected html source";
    case XMQ_ERROR_EXPECTED_JSON: return "expected json source";
    }
    assert(false);
    return "unknown error";
}

#endif // XMQ_INTERNALS_MODULE
