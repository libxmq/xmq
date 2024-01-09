#ifndef BUILDING_XMQ

#include"parts/always.h"
#include"parts/text.h"
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

void eat_xml_whitespace(XMQParseState *state, const char **start, const char **stop)
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

void eat_xmq_token_whitespace(XMQParseState *state, const char **start, const char **stop)
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
        // Tabs are not permitted as xmq token whitespace.
        if (nw == 1 && *i == '\t') break;
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
void get_color(XMQOutputSettings *os, XMQColor color, const char **pre, const char **post)
{
    XMQColoring *coloring = os->default_coloring;
    switch(color)
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

unsigned char hex_value(char c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return 10+c-'a';
    if (c >= 'A' && c <= 'F') return 10+c-'A';
    assert(false);
    return 0;
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

void print_color_pre(XMQPrintState *ps, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(os, color, &pre, &post);

    if (pre)
    {
        XMQWrite write = os->content.write;
        void *writer_state = os->content.writer_state;
        write(writer_state, pre, NULL);
    }
}

void print_color_post(XMQPrintState *ps, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(os, color, &pre, &post);

    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (post)
    {
        write(writer_state, post, NULL);
    }
    else
    {
        write(writer_state, ps->replay_active_color_pre, NULL);
    }
}

const char *xmqParseErrorToString(XMQParseError e)
{
    switch (e)
    {
    case XMQ_ERROR_CANNOT_READ_FILE: return "cannot read file";
    case XMQ_ERROR_OOM: return "out of memory";
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
    case XMQ_ERROR_UNEXPECTED_TAB: return "unexpected tab character (remember tabs must be quoted)";
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
    case XMQ_ERROR_PARSING_XML: return "error parsing xml";
    case XMQ_ERROR_PARSING_HTML: return "error parsing html";
    }
    assert(false);
    return "unknown error";
}

void node_strlen_name_prefix(xmlNode *node,
                        const char **name, size_t *name_len,
                        const char **prefix, size_t *prefix_len,
                        size_t *total_len)
{
    *name_len = strlen((const char*)node->name);
    *name = (const char*)node->name;

    if (node->ns && node->ns->prefix)
    {
        *prefix = (const char*)node->ns->prefix;
        *prefix_len = strlen((const char*)node->ns->prefix);
        *total_len = *name_len + *prefix_len +1;
    }
    else
    {
        *prefix = NULL;
        *prefix_len = 0;
        *total_len = *name_len;
    }
    assert(*name != NULL);
}

void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len)
{
    *name = (const char*)attr->name;
    size_t name_b_len;
    size_t name_u_len;
    size_t prefix_b_len;
    size_t prefix_u_len;
    str_b_u_len(*name, NULL, &name_b_len, &name_u_len);

    if (attr->ns && attr->ns->prefix)
    {
        *prefix = (const char*)attr->ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = name_u_len + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = name_u_len;
    }
    assert(*name != NULL);
}

void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len)
{
    size_t prefix_b_len;
    size_t prefix_u_len;

    if (ns->prefix)
    {
        *prefix = (const char*)ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = /* xmlns */ 5  + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = /* xmlns */ 5;
    }
}

void element_strlen_name_prefix(xmlNode *element, const char **name, const char **prefix, size_t *total_u_len)
{
    *name = (const char*)element->name;
    if (!*name)
    {
        *name = "";
        *prefix = "";
        *total_u_len = 0;
        return;
    }
    size_t name_b_len;
    size_t name_u_len;
    size_t prefix_b_len;
    size_t prefix_u_len;
    str_b_u_len(*name, NULL, &name_b_len, &name_u_len);

    if (element->ns && element->ns->prefix)
    {
        *prefix = (const char*)element->ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = name_u_len + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = name_u_len;
    }
    assert(*name != NULL);
}

/**
    enter_compound_level:

    When parsing a value and the parser has entered a compound value,
    the level for the child quotes/entities are now handled as
    tokens inside the compound.

    LEVEL_ELEMENT_VALUE -> LEVEL_ELEMENT_VALUE_COMPOUND
    LEVEL_ATTR_VALUE -> LEVEL_ATTR_VALUE_COMPOUND
*/
Level enter_compound_level(Level l)
{
    assert(l != 0);
    return (Level)(((int)l)+1);
}

XMQColor level_to_quote_color(Level level)
{
    switch (level)
    {
    case LEVEL_XMQ: return COLOR_quote;
    case LEVEL_ELEMENT_VALUE: return COLOR_element_value_quote;
    case LEVEL_ELEMENT_VALUE_COMPOUND: return COLOR_element_value_compound_quote;
    case LEVEL_ATTR_VALUE: return COLOR_attr_value_quote;
    case LEVEL_ATTR_VALUE_COMPOUND: return COLOR_attr_value_compound_quote;
    }
    assert(false);
    return COLOR_none;
}

XMQColor level_to_entity_color(Level level)
{
    switch (level)
    {
    case LEVEL_XMQ: return COLOR_entity;
    case LEVEL_ELEMENT_VALUE: return COLOR_element_value_entity;
    case LEVEL_ELEMENT_VALUE_COMPOUND: return COLOR_element_value_compound_entity;
    case LEVEL_ATTR_VALUE: return COLOR_attr_value_entity;
    case LEVEL_ATTR_VALUE_COMPOUND: return COLOR_attr_value_compound_entity;
    }
    assert(false);
    return COLOR_none;
}

/**
  Scan the attribute names and find the max unicode character width.
*/
size_t find_attr_key_max_u_width(xmlAttr *a)
{
    size_t max = 0;
    while (a)
    {
        const char *name;
        const char *prefix;
        size_t total_u_len;
        attr_strlen_name_prefix(a, &name, &prefix, &total_u_len);

        if (total_u_len > max) max = total_u_len;
        a = xml_next_attribute(a);
    }
    return max;
}

/**
  Scan nodes until there is a node which is not suitable for using the = sign.
  I.e. it has multiple children or no children. This node unsuitable node is stored in
  restart_find_at_node or NULL if all nodes were suitable.
*/
size_t find_element_key_max_width(xmlNodePtr element, xmlNodePtr *restart_find_at_node)
{
    size_t max = 0;
    xmlNodePtr i = element;
    while (i)
    {
        if (!is_key_value_node(i) || xml_first_attribute(i))
        {
            if (i == element) *restart_find_at_node = xml_next_sibling(i);
            else *restart_find_at_node = i;
            return max;
        }
        const char *name;
        const char *prefix;
        size_t total_u_len;
        element_strlen_name_prefix(i, &name, &prefix, &total_u_len);

        if (total_u_len > max) max = total_u_len;
        i = xml_next_sibling(i);
    }
    *restart_find_at_node = NULL;
    return max;
}

/**
  Scan the namespace links and find the max unicode character width.
*/
size_t find_namespace_max_u_width(size_t max, xmlNs *ns)
{
    while (ns)
    {
        const char *prefix;
        size_t total_u_len;
        namespace_strlen_prefix(ns, &prefix, &total_u_len);

        // Print only new/overriding namespaces.
        if (total_u_len > max) max = total_u_len;
        ns = ns->next;
    }

    return max;
}

#endif // XMQ_INTERNALS_MODULE
