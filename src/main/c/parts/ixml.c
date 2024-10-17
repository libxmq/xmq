
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
#define IXML_STEP(name,state) {                 \
    if (true) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+10); \
        for (int i=0; i<state->depth; ++i) fprintf(stderr, "    "); \
        fprintf(stderr, "dbg " #name " >%s...\n", tmp);      \
        for (int i=0; i<state->depth; ++i) fprintf(stderr, "    "); \
        fprintf(stderr, "{\n");      \
        state->depth++; \
        free(tmp); \
    } \
}
#define IXML_DONE(name,state) {                 \
    if (true) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+10); \
        state->depth--; \
        for (int i=0; i<state->depth; ++i) fprintf(stderr, "    "); \
        fprintf(stderr, "}\n"); \
        free(tmp); \
    } \
}

#define EAT(name, num) { \
    if (true) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+num);       \
        for (int i=0; i<state->depth; ++i) fprintf(stderr, "    "); \
        fprintf(stderr, "eat %s %s\n", #name, tmp);       \
        free(tmp); \
    } \
    increment(0, num, &state->i, &state->line, &state->col); \
}
#define ASSERT(x) assert(x)
#else
#define IXML_STEP(name,state) {}
#define EAT(name, num) increment(0, num, &state->i, &state->line, &state->col);
#define ASSERT(x) {}
#endif

bool is_ixml_eob(XMQParseState *state);
bool is_ixml_alias_start(XMQParseState *state);
bool is_ixml_alt_start(char c);
bool is_ixml_alt_end(char c);
bool is_ixml_charset_start(XMQParseState *state);
bool is_ixml_comment_start(XMQParseState *state);
bool is_ixml_encoded_start(XMQParseState *state);
bool is_ixml_factor_start(XMQParseState *state);
bool is_ixml_hex_start(XMQParseState *state);
bool is_ixml_insertion_start(XMQParseState *state);
bool is_ixml_literal_start(XMQParseState *state);
bool is_ixml_mark(char c);
bool is_ixml_name_follower(char c);
bool is_ixml_name_start(char c);
bool is_ixml_naming_char(char c);
bool is_ixml_naming_start(XMQParseState *state);
bool is_ixml_nonterminal_start(XMQParseState *state);
bool is_ixml_prolog_start(XMQParseState *state);
bool is_ixml_string_char(char c);
bool is_ixml_string_start(XMQParseState *state);
bool is_ixml_term_start(XMQParseState *state);
bool is_ixml_tmark_char(char c);
bool is_ixml_tmark_start(XMQParseState *state);
bool is_ixml_quote_start(char c);
bool is_ixml_quoted_start(XMQParseState *state);
bool is_ixml_rule_start(XMQParseState *state);
bool is_ixml_rule_end(char c);
bool is_ixml_terminal_start(XMQParseState *state);
bool is_ixml_whitespace_char(char c);
bool is_ixml_whitespace_start(XMQParseState *state);

void parse_ixml(XMQParseState *state);
void parse_ixml_alias(XMQParseState *state);
void parse_ixml_alt(XMQParseState *state);
void parse_ixml_alts(XMQParseState *state);
void parse_ixml_charset(XMQParseState *state);
void parse_ixml_comment(XMQParseState *state);
void parse_ixml_encoded(XMQParseState *state);
void parse_ixml_factor(XMQParseState *state);
void parse_ixml_hex(XMQParseState *state);
void parse_ixml_literal(XMQParseState *state);
void parse_ixml_name(XMQParseState *state, const char **content_start, const char **content_stop);
void parse_ixml_naming(XMQParseState *state);
void parse_ixml_nonterminal(XMQParseState *state);
void parse_ixml_prolog(XMQParseState *state);
void parse_ixml_quoted(XMQParseState *state);
void parse_ixml_rule(XMQParseState *state);
void parse_ixml_string(XMQParseState *state, const char **content_start, const char **content_stop);
void parse_ixml_term(XMQParseState *state);
void parse_ixml_terminal(XMQParseState *state);
void parse_ixml_whitespace(XMQParseState *state);

int peek_ixml_code(XMQParseState *state);
int peek_ixml_character(XMQParseState *state);

void add_yaep_grammar_rule(char mark, const char *name_start, const char *name_stop);

//////////////////////////////////////////////////////////////////////////////////////

bool is_ixml_eob(XMQParseState *state)
{
    return state->i >= state->buffer_stop;
}

bool is_ixml_alias_start(XMQParseState *state)
{
    return *(state->i) == '>';
}

bool is_ixml_alt_start(char c)
{
    return
        c == '+' || // Insertion +"hej" or +#a
        c == '#' || // encoded literal
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

bool is_ixml_charset_start(XMQParseState *state)
{
    const char *i = state->i;

    if (is_ixml_tmark_char(*i)) i++;
    while (is_ixml_whitespace_char(*i)) i++;
    if (*i == '~') i++;
    while (is_ixml_whitespace_char(*i)) i++;

    if (*i == '[') return true;

    return false;
}

bool is_ixml_comment_start(XMQParseState *state)
{
    return *(state->i) == '{';
}

bool is_ixml_encoded_start(XMQParseState *state)
{
    // -encoded: (tmark, s)?, -"#", hex, s.

    const char *i = state->i;
    char c = *i;
    if (c == '#') return true;
    if (c != '^' && c != '-' && !is_ixml_whitespace_char(*i)) return false;
    while (is_ixml_whitespace_char(*i))
    {
        i++;
    }
    return *i == '#';
}

bool is_ixml_factor_start(XMQParseState *state)
{
    return
        is_ixml_terminal_start(state) ||
        is_ixml_nonterminal_start(state);
}

bool is_ixml_insertion_start(XMQParseState *state)
{
    return *(state->i) == '+';
}

bool is_ixml_hex_start(XMQParseState *state)
{
    char c = *(state->i);
    return
        (c >= 'A' && c <= 'F') ||
        (c >= 'a' && c <= 'f') ||
        (c >= '0' && c <= '9');
}

bool is_ixml_literal_start(XMQParseState *state)
{
    return is_ixml_quoted_start(state) || is_ixml_encoded_start(state);
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
        c == '-' || (c >= '0' && c <= '9') ;
}

bool is_ixml_name_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ixml_naming_char(char c)
{
    return is_ixml_name_start(c) || is_ixml_mark(c);
}

bool is_ixml_naming_start(XMQParseState *state)
{
    return is_ixml_naming_char(*(state->i));
}

bool is_ixml_nonterminal_start(XMQParseState *state)
{
    return
        is_ixml_naming_start(state);
}

bool is_ixml_prolog_start(XMQParseState *state)
{
    // Detect "ixml ", "ixml\n", "ixml{}"
    return !strncmp(state->i, "ixml", 4) && is_ixml_whitespace_char(*(state->i+4));
}

bool is_ixml_string_char(char c)
{
    // Detect "howdy "" there" or 'howdy '' there'
    return c == '"' || c== '\'';
}

bool is_ixml_string_start(XMQParseState *state)
{
    return is_ixml_string_char(*(state->i));
}

bool is_ixml_term_start(XMQParseState *state)
{
    return is_ixml_factor_start(state);
}

bool is_ixml_quote_start(char c)
{
    return c == '"' || c == '\'';
}

bool is_ixml_quoted_start(XMQParseState *state)
{
    //  -quoted: (tmark, s)?, string, s.

    const char *i = state->i;
    char c = *i;
    if (c == '"' || c == '\'') return true;
    if (c != '^' && c != '-' && !is_ixml_whitespace_char(*i)) return false;
    while (is_ixml_whitespace_char(*i))
    {
        i++;
    }
    return *i == '"' || *i  == '\'';
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
    return
        is_ixml_literal_start(state) ||
        is_ixml_charset_start(state);
}

bool is_ixml_tmark_char(char c)
{
    return
        c == '^' || // Add as element (default but can be used to override attribute).
        c == '-';   // Do not generate node.
}

bool is_ixml_tmark_start(XMQParseState *state)
{
    return is_ixml_tmark_char(*(state->i));
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

void parse_ixml_alias(XMQParseState *state)
{
    IXML_STEP(alias, state);

    ASSERT(is_ixml_alias_start(state));
    EAT(alias_start, 1);

    parse_ixml_whitespace(state);

    const char *name_start;
    const char *name_stop;
    parse_ixml_name(state, &name_start, &name_stop);

    parse_ixml_whitespace(state);

    IXML_DONE(alias, state);
}

void parse_ixml_alt(XMQParseState *state)
{
    // alt: term**(-",", s).
    IXML_STEP(alt, state);

    for (;;)
    {
        if (!is_ixml_alt_start(*(state->i)))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
            state->error_info = "expected term here";
            longjmp(state->error_handler, 1);
        }
        parse_ixml_term(state);

        parse_ixml_whitespace(state);

        char c = *(state->i);
        if (is_ixml_eob(state) ||
            is_ixml_alt_end(c) ||
            is_ixml_rule_end(c)) break;

        if (c != ',')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected , or |; or . here";
            longjmp(state->error_handler, 1);
        }
        EAT(comma, 1);

        parse_ixml_whitespace(state);
    }


    IXML_DONE(alt, state);
}

void parse_ixml_alts(XMQParseState *state)
{
    IXML_STEP(alts, state);

    // alts: alt++(-[";|"], s).
    for (;;)
    {
        if (is_ixml_eob(state) ||
            is_ixml_rule_end(*(state->i))) break;

        if (!is_ixml_alt_start(*(state->i)))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
            state->error_info = "expected name here";
            longjmp(state->error_handler, 1);
        }
        parse_ixml_alt(state);

        parse_ixml_whitespace(state);

        char c = *(state->i);
        if (is_ixml_rule_end(c)) break;
        if (c != '|' && c != ';')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected ; or | here";
            longjmp(state->error_handler, 1);
        }
        EAT(choice, 1);

        parse_ixml_whitespace(state);
    }

    IXML_DONE(alts, state);
}

void parse_ixml_charset(XMQParseState *state)
{
    IXML_STEP(charset, state);
    ASSERT(is_ixml_charset_start(state));

    if (is_ixml_tmark_char(*(state->i)))
    {
        EAT(tmark, 1);
        parse_ixml_whitespace(state);
    }

    if (*(state->i) == '~')
    {
        EAT(negate, 1);
        parse_ixml_whitespace(state);
    }

    ASSERT(*(state->i) == '[');

    EAT(left_bracket, 1);
    parse_ixml_whitespace(state);

    for (;;)
    {
        if (is_ixml_eob(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "charset is not closed";
            longjmp(state->error_handler, 1);
        }

        if (is_ixml_string_start(state))
        {
            const char *start, *stop;
            parse_ixml_string(state, &start, &stop);
        }

        char c = *(state->i);
        if (c == ']') break;
        if (c != ';' && c != '|')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected ; or |";
            longjmp(state->error_handler, 1);
        }
    }

    EAT(right_bracket, 1);
    IXML_DONE(charset, state);
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

    IXML_DONE(comment, state);
}

void parse_ixml_encoded(XMQParseState *state)
{
    IXML_STEP(encoded, state);
    ASSERT(is_ixml_encoded_start(state));

    if (is_ixml_tmark_start(state))
    {
        EAT(encoded_tmark, 1);
    }

    parse_ixml_whitespace(state);

    char c = *(state->i);
    ASSERT(c == '#');
    EAT(hash, 1);

    if (!is_ixml_hex_start(state))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected hex after #";
        longjmp(state->error_handler, 1);
    }
    parse_ixml_hex(state);
    parse_ixml_whitespace(state);

    IXML_DONE(encoded, state);
}

void parse_ixml_factor(XMQParseState *state)
{
    IXML_STEP(factor, state);
    ASSERT(is_ixml_factor_start(state));

    if (is_ixml_terminal_start(state))
    {
        parse_ixml_terminal(state);
    }
    else if (is_ixml_nonterminal_start(state))
    {
        parse_ixml_nonterminal(state);
    }
    else  if (is_ixml_insertion_start(state))
    {
        // parse_ixml_insertion(state);
    }
    else
    {
        // parse group.
    }

    parse_ixml_whitespace(state);

    IXML_DONE(factor, state);
}

void parse_ixml_hex(XMQParseState *state)
{
    IXML_STEP(hex,state);

    while (is_ixml_hex_start(state))
    {
        EAT(hex_inside, 1);
    }

    IXML_DONE(hex, state);
}

void parse_ixml_literal(XMQParseState *state)
{
    IXML_STEP(literal,state);

    ASSERT(is_ixml_quoted_start(state) || is_ixml_encoded_start(state));

    if (is_ixml_quoted_start(state))
    {
        parse_ixml_quoted(state);
    }
    else
    {
        parse_ixml_encoded(state);
    }

    IXML_DONE(literal, state);
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
    IXML_DONE(name, state);
}

void parse_ixml_naming(XMQParseState *state)
{
    IXML_STEP(naming,state);

    ASSERT(is_ixml_naming_start(state));

    if (is_ixml_mark(*(state->i)))
    {
        EAT(naming_mark, 1);
    }

    parse_ixml_whitespace(state);

    const char *name_start;
    const char *name_stop;
    parse_ixml_name(state, &name_start, &name_stop);

    parse_ixml_whitespace(state);

    if (is_ixml_alias_start(state))
    {
        parse_ixml_alias(state);
    }

    IXML_DONE(naming, state);
}

void parse_ixml_nonterminal(XMQParseState *state)
{
    IXML_STEP(nonterminal, state);
    ASSERT(is_ixml_naming_start(state));

    parse_ixml_naming(state);

    IXML_DONE(nonterminal, state);
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
        state->error_info = "expected \"version\" ";
        longjmp(state->error_handler, 1);
    }

    EAT(prolog_version, 7);

    if (!is_ixml_whitespace_start(state))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected whitespace";
        longjmp(state->error_handler, 1);
    }

    parse_ixml_whitespace(state);

    if (!is_ixml_string_start(state))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected string";
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

    IXML_DONE(prolog, state);
}

void parse_ixml_quoted(XMQParseState *state)
{
    IXML_STEP(quoted, state);
    // -quoted: (tmark, s)?, string, s.

    ASSERT(is_ixml_quoted_start(state));

    if (is_ixml_tmark_start(state))
    {
        EAT(quoted_tmark, 1);
        parse_ixml_whitespace(state);
    }

    const char *start, *stop;
    parse_ixml_string(state, &start, &stop);

    parse_ixml_whitespace(state);

    IXML_DONE(quoted, state);
}

void parse_ixml_rule(XMQParseState *state)
{
    // rule: naming, -["=:"], s, -alts, -".".
    IXML_STEP(rule, state);
    ASSERT(is_ixml_naming_start(state));

    parse_ixml_naming(state);

    parse_ixml_whitespace(state);

    char c = *(state->i);
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

    parse_ixml_whitespace(state);

//    add_yaep_grammar_rule(0, name_start, name_stop);

    IXML_DONE(rule, state);
}

void parse_ixml_string(XMQParseState *state, const char **content_start, const char **content_stop)
{
    IXML_STEP(string, state);

    MemBuffer *buf = new_membuffer();

    ASSERT(is_ixml_string_start(state));

    char q = *(state->i);
    EAT(string_start, 1);

    for (;;)
    {
        if (is_ixml_eob(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "string not terminated";
            longjmp(state->error_handler, 1);
        }

        if (*(state->i) == q)
        {
            if (*(state->i+1) == q)
            {
                // A double '' or "" means a single ' or " inside the string.
                EAT(string_quote, 1);
            }
            else
            {
                EAT(string_stop, 1);
                break;
            }
        }
        membuffer_append_char(buf, *(state->i));
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

    IXML_DONE(string, state);
}

void parse_ixml_term(XMQParseState *state)
{
    IXML_STEP(term, state);
    ASSERT(is_ixml_factor_start(state));

    if (is_ixml_factor_start(state))
    {
        parse_ixml_factor(state);
    }
    else
    {
        // parse group.
    }

    parse_ixml_whitespace(state);

    IXML_DONE(term, state);
}

void parse_ixml_terminal(XMQParseState *state)
{
    IXML_STEP(terminal, state);
    ASSERT(is_ixml_literal_start(state) || is_ixml_charset_start(state));

    if (is_ixml_literal_start(state))
    {
        parse_ixml_literal(state);
    }
    else
    {
        parse_ixml_charset(state);
    }

    IXML_DONE(terminal, state);
}

void parse_ixml_whitespace(XMQParseState *state)
{
    if (is_ixml_eob(state) || !is_ixml_whitespace_start(state)) return;

    IXML_STEP(ws, state);

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

    IXML_DONE(ws, state);
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
