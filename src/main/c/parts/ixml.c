
#ifndef BUILDING_XMQ

#include"always.h"
#include"hashmap.h"
#include"ixml.h"
#include"membuffer.h"
#include"parts/xmq_internals.h"
#include"stack.h"
#include"text.h"
#include"xml.h"
#include<yaep/yaep.h>

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef IXML_MODULE

#define INLINE __attribute__((always_inline))

#define DEBUG_IXML_GRAMMAR

#ifdef DEBUG_IXML_GRAMMAR
#define IXML_STEP(name,state) { \
    if (true) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+10); \
        fprintf(stderr, "DBG PARSE " #name " \"%s...\n", tmp);      \
        free(tmp); \
    } \
}
#define EAT(name, num) { \
    if (true) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+num);       \
        fprintf(stderr, "DBG EAT %s \"%s\"\n", #name, tmp);       \
        free(tmp); \
    } \
    increment(0, num, &state->i, &state->line, &state->col); \
}
#else
#define IXML_STEP(name,state) {}
#define EAT(name, num) increment(0, num, &state->i, &state->line, &state->col);
#endif

inline bool is_ixml_eob(XMQParseState *state) INLINE;
bool is_ixml_alt_start(char c);
bool is_ixml_alt_end(char c);
bool is_ixml_character_start(char c);
inline bool is_ixml_comment_start(XMQParseState *state) INLINE;
bool is_ixml_hex_start(char c);
bool is_ixml_mark(char c);
bool is_ixml_name_follower(char c);
bool is_ixml_name_start(char c);
bool is_ixml_prolog_start(XMQParseState *state);
bool is_ixml_string_start(char c);
bool is_ixml_quote_start(char c);
bool is_ixml_rule_start(XMQParseState *state);
bool is_ixml_rule_end(char c);
bool is_ixml_terminal_start(XMQParseState *state);
inline bool is_ixml_whitespace_char(char c) INLINE;
inline bool is_ixml_whitespace_start(XMQParseState *state) INLINE;

void parse_ixml(XMQParseState *state);
void parse_ixml_alt(XMQParseState *state);
void parse_ixml_alts(XMQParseState *state);
void parse_ixml_comment(XMQParseState *state);
void parse_ixml_name(XMQParseState *state, const char **content_start, const char **content_stop);
void parse_ixml_naming(XMQParseState *state);
void parse_ixml_prolog(XMQParseState *state);
void parse_ixml_rule(XMQParseState *state);
void parse_ixml_string(XMQParseState *state, const char **content_start, const char **content_stop);
void parse_ixml_whitespace(XMQParseState *state);

int peek_ixml_code(XMQParseState *state);
int peek_ixml_character(XMQParseState *state);

void add_yaep_grammar_rule(char mark, const char *name_start, const char *name_stop);

//////////////////////////////////////////////////////////////////////////////////////

bool is_ixml_eob(XMQParseState *state)
{
    return state->i >= state->buffer_stop;
}

bool is_ixml_alt_start(char c)
{
    return
        c == '+' || // Insertion +"hej" or +#a
        c == '(' || // Group ( "svej" | "hojt" )

        is_ixml_mark(c) || // @^-
        is_ixml_name_start(c);
}

bool is_ixml_alt_end(char c)
{
    return
        c == ';' || // rule : "a", "b" ; "c", "d" .
        c == '|';   // rule : "a", "b" | "c", "d" .
}

bool is_ixml_character_start(char c)
{
    return c == '"' || c == '\'' || c == '#';
}

bool is_ixml_comment_start(XMQParseState *state)
{
    return *(state->i) == '{';
}

bool is_ixml_hex_start(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9');
}

bool is_ixml_mark(char c)
{
    return
        c == '@' || // Add as attribute.
        c == '^' || // Add as element (default but can be used to override attribute).
        c == '-';   // Do not generate node.
}

bool is_ixml_name_follower(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
        c == '-' || c == '.' || (c >= '0' && c <= '9') ;
}

bool is_ixml_name_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ixml_prolog_start(XMQParseState *state)
{
    // Detect "ixml ", "ixml\n", "ixml{}"
    return !strncmp(state->i, "ixml", 4) && is_ixml_whitespace_char(*(state->i+4));
}

bool is_ixml_string_start(char c)
{
    // Detect "howdy "" there" or 'howdy '' there'
    return c == '"' || c== '\'';
}

bool is_ixml_quote_start(char c)
{
    return c == '"' || c == '\'';
}

bool is_ixml_rule_start(XMQParseState *state)
{
//  rule: (mark, s)?, name,
    char c = *(state->i);

    if (is_ixml_mark(c)) return true;
    if (is_ixml_name_start(c)) return true;

    return false;
}

bool is_ixml_rule_end(char c)
{
    return c == '.'; // rule : "a", "b" ; "c", "d" .
}

bool is_ixml_terminal_start(XMQParseState *state)
{

    return false;
}

bool is_ixml_whitespace_char(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '{';
}

bool is_ixml_whitespace_start(XMQParseState *state)
{
    return is_ixml_whitespace_char(*(state->i));
}

/////////////////////////////////////////////////////////////////////////////

void parse_ixml(XMQParseState *state)
{
    // ixml: s, prolog?, rule++RS, s.
    parse_ixml_whitespace(state);

    if (is_ixml_prolog_start(state))
    {
        parse_ixml_prolog(state);
        parse_ixml_whitespace(state);
    }

    if (!is_ixml_rule_start(state))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected rule here";
        longjmp(state->error_handler, 1);
    }

    while (is_ixml_rule_start(state))
    {
        parse_ixml_rule(state);
    }

    parse_ixml_whitespace(state);
}

void parse_ixml_alt(XMQParseState *state)
{
    IXML_STEP(alt, state);

    if (is_ixml_name_start(*(state->i)))
    {
        const char *name_start;
        const char *name_stop;
        parse_ixml_name(state, &name_start, &name_stop);
    }
}

void parse_ixml_alts(XMQParseState *state)
{
    IXML_STEP(alts, state);

    // alts: alt++(-[";|"], s).
    for (;;)
    {
        if (is_ixml_eob(state) ||
            is_ixml_alt_end(*(state->i)) ||
            is_ixml_rule_end(*(state->i))) break;

        if (!is_ixml_alt_start(*(state->i)))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
            state->error_info = "expected name here";
            longjmp(state->error_handler, 1);
        }
        parse_ixml_alt(state);

        char c = *(state->i);
        if (is_ixml_alt_end(c) || is_ixml_rule_end(c)) break; // We found ';' or '|' or '.'

        parse_ixml_whitespace(state);
    }
}

void parse_ixml_comment(XMQParseState *state)
{
    IXML_STEP(comment, state);
    assert (*(state->i) == '{');

    EAT(comment_start, 1);

    for (;;)
    {
        char c = *(state->i);
        if (is_ixml_eob(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "comment is not closed";
            longjmp(state->error_handler, 1);
        }
        if (c == '{')
        {
            parse_ixml_comment(state);
        }
        if (c == '}') break;
        EAT(comment_inside, 1);
    }
    EAT(comment_stop, 1);
}

void parse_ixml_name(XMQParseState *state, const char **name_start, const char **name_stop)
{
    IXML_STEP(name,state);

    assert(is_ixml_name_start(*(state->i)));
    *name_start = state->i;
    EAT(name_start, 1);

    while (is_ixml_name_follower(*(state->i)))
    {
        EAT(name_inside, 1);
    }
    *name_stop = state->i;

    fprintf(stderr, "name >%.*s<\n", (int)(*name_stop-*name_start), *name_start);
}

void parse_ixml_prolog(XMQParseState *state)
{
    IXML_STEP(prolog, state);
    // version: -"ixml", RS, -"version", RS, string, s, -'.' .
    // Example: ixml version "1.2.3-gurka" .

    assert(is_ixml_prolog_start(state));
    EAT(prolog_ixml, 4);

    parse_ixml_whitespace(state);

    if (strncmp(state->i, "version", 7))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        longjmp(state->error_handler, 1);
    }

    EAT(prolog_version, 7);

    if (!is_ixml_whitespace_start(state))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        longjmp(state->error_handler, 1);
    }

    parse_ixml_whitespace(state);

    if (!is_ixml_string_start(*(state->i)))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        longjmp(state->error_handler, 1);
    }

    const char *content_start;
    const char *content_stop;
    parse_ixml_string(state, &content_start, &content_stop);

    parse_ixml_whitespace(state);

    char c = *(state->i);
    if (c != '.')
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = ": ixml version must end with a dot";
        longjmp(state->error_handler, 1);
    }
    EAT(prolog_stop, 1);
}

void parse_ixml_string(XMQParseState *state, const char **content_start, const char **content_stop)
{
    IXML_STEP(string, state);

    const char *start = state->i;
    const char *stop = state->buffer_stop;

    MemBuffer *buf = new_membuffer();

    const char *i = start;

    EAT(string_start, 1);

    while (i < stop)
    {
        char c = *i;
        if (c == '"')
        {
            EAT(string_inside_quote, 1);
            break;
        }
        membuffer_append_char(buf, c);
        EAT(string_inside, 1);
    }
    // Add a zero termination to the string which is not used except for
    // guaranteeing that there is at least one allocated byte for empty strings.
    membuffer_append_null(buf);

    // Calculate the real length which might be less than the original
    // since escapes have disappeared. Add 1 to have at least something to allocate.
    size_t len = membuffer_used(buf);
    char *quote = free_membuffer_but_return_trimmed_content(buf);
    *content_start = quote;
    *content_stop = quote+len-1; // Drop the zero byte.
}

void parse_ixml_whitespace(XMQParseState *state)
{
    if (is_ixml_eob(state) || !is_ixml_whitespace_start(state)) return;

    IXML_STEP(whitespace, state);

    while (state->i < state->buffer_stop && is_ixml_whitespace_start(state))
    {
        if (is_ixml_comment_start(state))
        {
            parse_ixml_comment(state);
        }
        else
        {
            EAT(ws, 1);
        }
    }
}

int peek_ixml_code(XMQParseState *state)
{
    char a = *(state->i);
    char b = *(state->i+1);
    bool capital = (a >= 'A' && a <= 'Z');
    if (!capital) return 0;
    bool letter = (b >= 'a' && b <= '<');
    if (!letter) return 1;
    return 2;
}


bool xmq_tokenize_buffer_ixml(XMQParseState *state, const char *start, const char *stop)
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
        parse_ixml(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "unexpected end";
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

void add_yaep_grammar_rule(char mark, const char *name_start, const char *name_stop)
{
}


void parse_ixml_rule(XMQParseState *state)
{
    IXML_STEP(rule, state);

    // rule: naming, -["=:"], s, -alts, -".".

    char c = *(state->i);
    char mark = 0;
    const char *name_start = NULL;
    const char *name_stop = NULL;

    if (is_ixml_mark(c))
    {
        mark = c;
        EAT(rule_mark, 1);
        parse_ixml_whitespace(state);
    }

    c = *(state->i);
    if (!is_ixml_name_start(c))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected name here";
        longjmp(state->error_handler, 1);
    }

    parse_ixml_name(state, &name_start, &name_stop);

    parse_ixml_whitespace(state);

    c = *(state->i);
    if (c != '=' && c != ':')
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected equal or colon here";
        longjmp(state->error_handler, 1);
    }
    EAT(rule_equal, 1);

    parse_ixml_whitespace(state);

    parse_ixml_alts(state);

    c = *(state->i);
    if (c != '.')
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected dot here";
        longjmp(state->error_handler, 1);
    }
    EAT(rule_stop, 1);

    add_yaep_grammar_rule(mark, name_start, name_stop);
}


#else

// Empty function when XMQ_NO_IXML is defined.
bool xmq_parse_ixml_grammar(struct grammar *g,
                            struct yaep_tree_node **root,
                            int *ambiguous,
                            XMQDoc *doq,
                            const char *start,
                            const char *stop)
{
    return false;
}

#endif // IXML_MODULE
