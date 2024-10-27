/* libxmq - Copyright (C) 2024 Fredrik Öhrström (spdx: MIT)

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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef BUILDING_XMQ

#include"always.h"
#include"hashmap.h"
#include"ixml.h"
#include"membuffer.h"
#include"parts/xmq_internals.h"
#include"stack.h"
#include"text.h"
#include"vector.h"
#include"xml.h"
#include"yaep.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef IXML_MODULE

#define DEBUG_IXML_GRAMMAR

#ifdef DEBUG_IXML_GRAMMAR
#define IXML_STEP(name,state) {                 \
    if (xmq_trace_enabled_) {                                \
        char *tmp = xmq_quote_as_c(state->i, state->i+10); \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("dbg " #name " >%s...\n", tmp);       \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("{\n");      \
        state->depth++; \
        free(tmp); \
    } \
}
#define IXML_DONE(name,state) {                 \
    if (xmq_trace_enabled_) {                                \
        char *tmp = xmq_quote_as_c(state->i, state->i+10); \
        state->depth--; \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("}\n"); \
        free(tmp); \
    } \
}

#define EAT(name, num) { \
    if (xmq_trace_enabled_) { \
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
bool is_ixml_alt_start(XMQParseState *state);
bool is_ixml_alt_end(char c);
bool is_ixml_charset_start(XMQParseState *state);
int  is_ixml_code_start(XMQParseState *state);
bool is_ixml_comment_start(XMQParseState *state);
bool is_ixml_encoded_start(XMQParseState *state);
bool is_ixml_factor_start(XMQParseState *state);
bool is_ixml_group_start(XMQParseState *state);
bool is_ixml_group_end(XMQParseState *state);
bool is_ixml_hex_char(char c);
bool is_ixml_hex_start(XMQParseState *state);
bool is_ixml_insertion_start(XMQParseState *state);
bool is_ixml_literal_start(XMQParseState *state);
bool is_ixml_mark_char(char c);
bool is_ixml_name_follower(char c);
bool is_ixml_name_start(char c);
bool is_ixml_naming_char(char c);
bool is_ixml_naming_start(XMQParseState *state);
bool is_ixml_nonterminal_start(XMQParseState *state);
bool is_ixml_prolog_start(XMQParseState *state);
bool is_ixml_range_start(XMQParseState *state);
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
void parse_ixml_alias(XMQParseState *state, const char **alias_start, const char **alias_stop);
void parse_ixml_alt(XMQParseState *state);
void parse_ixml_alts(XMQParseState *state);
void parse_ixml_charset(XMQParseState *state);
void parse_ixml_comment(XMQParseState *state);
void parse_ixml_encoded(XMQParseState *state);
void parse_ixml_factor(XMQParseState *state);
void parse_ixml_group(XMQParseState *state);
void parse_ixml_hex(XMQParseState *state, int *value);
void parse_ixml_insertion(XMQParseState *state);
void parse_ixml_literal(XMQParseState *state);
void parse_ixml_name(XMQParseState *state, const char **content_start, const char **content_stop);
void parse_ixml_naming(XMQParseState *state,
                       char *mark,
                       char **name,
                       char **alias);
void parse_ixml_nonterminal(XMQParseState *state);
void parse_ixml_prolog(XMQParseState *state);
void parse_ixml_range(XMQParseState *state);
void parse_ixml_quoted(XMQParseState *state);
void parse_ixml_rule(XMQParseState *state);
void parse_ixml_string(XMQParseState *state, char **content);
void parse_ixml_term(XMQParseState *state);
void parse_ixml_terminal(XMQParseState *state);
void parse_ixml_whitespace(XMQParseState *state);

void skip_comment(const char **i);
void skip_encoded(const char **i);
void skip_mark(const char **i);
void skip_string(const char **i);
void skip_tmark(const char **i);
void skip_whitespace(const char **i);

void allocate_yaep_tmp_terminals(XMQParseState *state);
void free_yaep_tmp_terminals(XMQParseState *state);
bool has_ixml_tmp_terminals(XMQParseState *state);
void add_yaep_grammar_rule(char mark, const char *name_start, const char *name_stop);
void add_yaep_terminal(XMQParseState *state, IXMLTerminal *terminal);
void add_yaep_terminal_to_rule(XMQParseState *state, IXMLTerminal *terminal, IXMLRule *rule);
void add_yaep_tmp_terminal(XMQParseState *state, char *name, int code);
// Store all state->ixml_tmp_terminals on the rule rhs.
void add_yaep_tmp_terminals_to_rule(XMQParseState *state, IXMLRule *rule);

void do_ixml(XMQParseState *state);
void do_ixml_comment(XMQParseState *state, const char *start, const char *stop);
void do_ixml_rule(XMQParseState *state, const char *name_start, const char *name_stop);
void do_ixml_alt(XMQParseState *state);
void do_ixml_nonterminal(XMQParseState *state, const char *name_start, char *name_stop);
void do_ixml_option(XMQParseState *state);

IXMLRule *new_ixml_rule();
void free_ixml_rule(IXMLRule *r);
IXMLTerminal *new_ixml_terminal();
void free_ixml_terminal(IXMLTerminal *t);
IXMLNonTerminal *new_ixml_nonterminal();
void free_ixml_nonterminal(IXMLNonTerminal *t);

//////////////////////////////////////////////////////////////////////////////////////

bool is_ixml_eob(XMQParseState *state)
{
    return state->i >= state->buffer_stop || *(state->i) == 0;
}

bool is_ixml_alias_start(XMQParseState *state)
{
    return *(state->i) == '>';
}

bool is_ixml_alt_start(XMQParseState *state)
{
    char c = *(state->i);
    return
        c == '+' || // Insertion +"hej" or +#a
        c == '#' || // encoded literal
        c == '(' || // Group ( "svej" | "hojt" )
        c == '"' || // "string"
        c == '\'' || // 'string'
        c == '[' || // Charset
        c == '~' || // Negative charset
        is_ixml_mark_char(c) || // @^-
        is_ixml_name_start(c);
}

bool is_ixml_alt_end(char c)
{
    return
        c == ';' || // rule : "a", "b" ; "c", "d" .
        c == '|'; // rule : "a", "b" | "c", "d" .
}

bool is_ixml_charset_start(XMQParseState *state)
{
    const char *i = state->i;

    skip_tmark(&i);

    if (*i == '~') i++;
    while (is_ixml_whitespace_char(*i)) i++;

    if (*i == '[') return true;

    return false;
}

int is_ixml_code_start(XMQParseState *state)
{
    char a = *(state->i);
    char b = *(state->i+1);
    bool capital = (a >= 'A' && a <= 'Z');
    if (!capital) return 0;
    bool letter = (b >= 'a' && b <= 'z');
    if (!letter) return 1;
    return 2;
}

bool is_ixml_comment_start(XMQParseState *state)
{
    return *(state->i) == '{';
}

bool is_ixml_encoded_start(XMQParseState *state)
{
    // -encoded: (tmark, s)?, -"#", hex, s.

    const char *i = state->i;
    skip_tmark(&i);

    char c = *i;
    if (c == '#') return true;
    return false;
}

bool is_ixml_factor_start(XMQParseState *state)
{
    return
        is_ixml_terminal_start(state) ||
        is_ixml_nonterminal_start(state) ||
        is_ixml_insertion_start(state) ||
        is_ixml_group_start(state);
}

bool is_ixml_group_start(XMQParseState *state)
{
    return *(state->i) == '(';
}

bool is_ixml_group_end(XMQParseState *state)
{
    return *(state->i) == ')';
}

bool is_ixml_insertion_start(XMQParseState *state)
{
    return *(state->i) == '+';
}

bool is_ixml_hex_char(char c)
{
    return
        (c >= 'A' && c <= 'F') ||
        (c >= 'a' && c <= 'f') ||
        (c >= '0' && c <= '9');
}

bool is_ixml_hex_start(XMQParseState *state)
{
    return is_ixml_hex_char(*(state->i));
}

bool is_ixml_literal_start(XMQParseState *state)
{
    const char *i = state->i;
    skip_tmark(&i);

    char c = *i;
    return c == '"' || c == '\'' || c == '#';
}

bool is_ixml_mark_char(char c)
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
    return is_ixml_name_start(c) || is_ixml_mark_char(c);
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

bool is_ixml_range_start(XMQParseState *state)
{
    // -range: from, s, -"-", s, to.
    // @from: character.
    // @to: character.
    // -character: -'"', dchar, -'"';
    //             -"'", schar, -"'";
    //             "#", hex.

    const char *j = state->i;
    if (is_ixml_string_start(state))
    {
        skip_string(&j);
        skip_whitespace(&j);
        if (*j == '-') return true;
        return false;
    }
    else if (is_ixml_encoded_start(state))
    {
        skip_encoded(&j);
        skip_whitespace(&j);
        if (*j == '-') return true;
        return false;
    }

    return false;
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
    skip_tmark(&i);
    char c = *i;
    return c == '"' || c  == '\'';
}

bool is_ixml_rule_start(XMQParseState *state)
{
//  rule: (mark, s)?, name,
    if (is_ixml_naming_start(state)) return true;

    return false;
}

bool is_ixml_rule_end(char c)
{
    return c == '.'; // rule : "a", "b" ; "c", "d" .
}

bool is_ixml_terminal_start(XMQParseState *state)
{
    return
        is_ixml_encoded_start(state) ||
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
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '{' || c == '}';
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

void parse_ixml_alias(XMQParseState *state, const char **alias_start, const char **alias_stop)
{
    IXML_STEP(alias, state);

    ASSERT(is_ixml_alias_start(state));
    EAT(alias_start, 1);

    parse_ixml_whitespace(state);

    parse_ixml_name(state, alias_start, alias_stop);

    parse_ixml_whitespace(state);

    IXML_DONE(alias, state);
}

void parse_ixml_alt(XMQParseState *state)
{
    // alt: term**(-",", s).
    IXML_STEP(alt, state);

    for (;;)
    {
        if (!is_ixml_alt_start(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
            state->error_info = "expected term here";
            longjmp(state->error_handler, 1);
        }
        parse_ixml_term(state);

        parse_ixml_whitespace(state);

        char c = *(state->i);
        if (is_ixml_alt_end(c) ||
            is_ixml_group_end(state) ||
            is_ixml_rule_end(c)) break;

        if (c != ',')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected , or . here";
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

        if (!is_ixml_alt_start(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
            state->error_info = "expected alt here";
            longjmp(state->error_handler, 1);
        }
        parse_ixml_alt(state);

        parse_ixml_whitespace(state);

        char c = *(state->i);
        if (is_ixml_rule_end(c) || is_ixml_group_end(state)) break;
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

        else if (is_ixml_range_start(state))
        {
            parse_ixml_range(state);
        }
        else if (is_ixml_encoded_start(state))
        {
            parse_ixml_encoded(state);
        }
        else if (is_ixml_code_start(state))
        {
            int num = is_ixml_code_start(state);
            EAT(unicode_class, num);
        }
        else if (is_ixml_string_start(state))
        {
            char *content = NULL;
            parse_ixml_string(state, &content);
            free(content);
        }

        char c = *(state->i);
        if (c == ']') break;
        if (c != ';' && c != '|')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected ; or |";
            longjmp(state->error_handler, 1);
        }

        EAT(next_charset_part, 1);
        parse_ixml_whitespace(state);
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
        if (is_ixml_eob(state))
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "comment is not closed";
            longjmp(state->error_handler, 1);
        }
        if (*(state->i) == '{')
        {
            parse_ixml_comment(state);
        }
        if (*(state->i) == '}') break;
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

    int value;
    parse_ixml_hex(state, &value);
    parse_ixml_whitespace(state);

    char buffer[16];
    snprintf(buffer, 15, "#%x", value);

    add_yaep_tmp_terminal(state, strdup(buffer), value);

    IXML_DONE(encoded, state);
}

void parse_ixml_factor(XMQParseState *state)
{
    IXML_STEP(factor, state);
    ASSERT(is_ixml_factor_start(state));

    if (is_ixml_terminal_start(state))
    {
        allocate_yaep_tmp_terminals(state);

        parse_ixml_terminal(state);

        if (has_ixml_tmp_terminals(state)) // Test needed while developing parser.
        {
            add_yaep_tmp_terminals_to_rule(state, state->ixml_rule);
        }

        free_yaep_tmp_terminals(state);
    }
    else if (is_ixml_nonterminal_start(state))
    {
        state->ixml_nonterminal = new_ixml_nonterminal();

        parse_ixml_nonterminal(state);

        vector_push_back(state->ixml_non_terminals, state->ixml_nonterminal);
        vector_push_back(state->ixml_rule->rhs, state->ixml_nonterminal);
    }
    else  if (is_ixml_insertion_start(state))
    {
        parse_ixml_insertion(state);
    }
    else if (is_ixml_group_start(state))
    {
        parse_ixml_group(state);
    }

    parse_ixml_whitespace(state);

    IXML_DONE(factor, state);
}

void parse_ixml_group(XMQParseState *state)
{
    IXML_STEP(group, state);
    ASSERT(is_ixml_group_start(state));

    EAT(left_par, 1);

    if (is_ixml_alt_start(state))
    {
        parse_ixml_alts(state);
    }
    else
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected alts in group";
        longjmp(state->error_handler, 1);
    }

    if (*(state->i) != ')')
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected ) to close group";
        longjmp(state->error_handler, 1);
    }
    EAT(right_par, 1);

    parse_ixml_whitespace(state);

    IXML_DONE(factor, state);
}

void parse_ixml_hex(XMQParseState *state, int *value)
{
    IXML_STEP(hex,state);

    const char *start = state->i;
    while (is_ixml_hex_start(state))
    {
        EAT(hex_inside, 1);
    }
    const char *stop = state->i;
    char *hex = strndup(start, stop-start);
    *value = (int)strtol(hex, NULL, 16);
    free(hex);

    IXML_DONE(hex, state);
}

void parse_ixml_insertion(XMQParseState *state)
{
    IXML_STEP(insertion,state);

    ASSERT(is_ixml_insertion_start(state));

    EAT(insertion_plus, 1);

    parse_ixml_whitespace(state);

    if (is_ixml_string_start(state))
    {
        char *content = NULL;
        parse_ixml_string(state, &content);
        free(content);
    }
    else if (is_ixml_encoded_start(state))
    {
        parse_ixml_encoded(state);
    }
    else
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected string or encoded character after insertion +";
        longjmp(state->error_handler, 1);
    }

    IXML_DONE(insertion, state);
}


void parse_ixml_literal(XMQParseState *state)
{
    IXML_STEP(literal,state);

    ASSERT(is_ixml_literal_start(state));

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

    IXML_DONE(name, state);
}

void parse_ixml_naming(XMQParseState *state,
                       char *mark,
                       char **name,
                       char **alias)
{
    IXML_STEP(naming,state);

    ASSERT(is_ixml_naming_start(state));

    if (is_ixml_mark_char(*(state->i)))
    {
        *mark = (*(state->i));
        EAT(naming_mark, 1);
    }
    else
    {
        *mark = 0;
    }
    parse_ixml_whitespace(state);

    if (!is_ixml_name_start(*(state->i)))
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected a name";
        longjmp(state->error_handler, 1);
    }
    const char *name_start, *name_stop;
    parse_ixml_name(state, &name_start, &name_stop);
    *name = strndup(name_start, (name_stop-name_start));

    parse_ixml_whitespace(state);

    if (is_ixml_alias_start(state))
    {
        const char *alias_start, *alias_stop;
        parse_ixml_alias(state, &alias_start, &alias_stop);
        *alias = strndup(alias_start, alias_stop-alias_start);
    }
    else
    {
        *alias = NULL;
    }

    IXML_DONE(naming, state);
}

void parse_ixml_nonterminal(XMQParseState *state)
{
    IXML_STEP(nonterminal, state);
    ASSERT(is_ixml_naming_start(state));

    IXMLNonTerminal *nt = state->ixml_nonterminal;
    parse_ixml_naming(state, &nt->mark, &nt->name, &nt->alias);

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

    char *content;
    parse_ixml_string(state, &content);
    free(content);

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


void parse_ixml_range(XMQParseState *state)
{
    IXML_STEP(range, state);
    ASSERT(is_ixml_range_start(state));

    if (is_ixml_string_start(state))
    {
        char *content;
        parse_ixml_string(state, &content);
        free(content);
    }
    else
    {
        parse_ixml_encoded(state);
    }
    parse_ixml_whitespace(state);

    // This is guaranteed by the is range test in the assert.
    ASSERT(*(state->i) == '-');
    EAT(range_minus, 1);

    parse_ixml_whitespace(state);

    if (is_ixml_string_start(state))
    {
        char *content;
        parse_ixml_string(state, &content);
        free(content);
    }
    else if (is_ixml_encoded_start(state))
    {
        parse_ixml_encoded(state);
    }
    else
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected range ending with string or hex char";
        longjmp(state->error_handler, 1);
    }
    parse_ixml_whitespace(state);

    IXML_DONE(range, state);
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

    char *content = NULL;
    parse_ixml_string(state, &content);

    for (const char *i = content; *i; ++i)
    {
        add_yaep_tmp_terminal(state, strndup(i, 1), *i);
    }
    free(content);

    parse_ixml_whitespace(state);

    IXML_DONE(quoted, state);
}

void parse_ixml_rule(XMQParseState *state)
{
    // rule: naming, -["=:"], s, -alts, -".".
    IXML_STEP(rule, state);
    ASSERT(is_ixml_naming_start(state));

    IXMLRule *rule = new_ixml_rule();
    vector_push_back(state->ixml_rules, rule);
//    push_stack(state->ixml_rule_stack, rule);
    state->ixml_rule = rule;

    parse_ixml_naming(state,
                      &rule->rule_name->mark,
                      &rule->rule_name->name,
                      &rule->rule_name->alias);

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

    state->ixml_rule = NULL;

    IXML_DONE(rule, state);
}

void parse_ixml_string(XMQParseState *state, char **content)
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
    // Add a zero termination to the string.
    membuffer_append_null(buf);

    char *quote = free_membuffer_but_return_trimmed_content(buf);
    *content = quote;

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
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "expected factor";
        longjmp(state->error_handler, 1);
    }

    char c = *(state->i);
    if (c == '?')
    {
        EAT(option, 1);
        parse_ixml_whitespace(state);
    }
    else if (c == '*' || c == '+')
    {
        if (c == '*')
        {
            EAT(star, 1);
        }
        else
        {
            EAT(plus, 1);
        }

        char cc = *(state->i);
        if (c == cc)
        {
            if (c == '*')
            {
                EAT(star, 1);
            }
            else
            {
                EAT(plus, 1);
            }
            parse_ixml_whitespace(state);

            // sep
            parse_ixml_factor(state);
        }
        parse_ixml_whitespace(state);
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

static __thread XMQParseState *yaep_state_ = NULL;
static __thread char **yaep_tmp_rhs_ = NULL;
static __thread int *yaep_tmp_transl_ = NULL;
static __thread HashMapIterator *yaep_i_ = NULL;
static __thread size_t yaep_j_ = 0;

const char *ixml_to_yaep_read_terminal(int *code);

const char *ixml_to_yaep_read_terminal(int *code)
{
    const char *key;
    void *val;
    bool ok = hashmap_next_key_value(yaep_i_, &key, &val);

    if (ok)
    {
        IXMLTerminal *t = (IXMLTerminal*)val;
        *code = t->code;
        return t->name;
    }

    return NULL;
}

const char *ixml_to_yaep_read_rule(const char ***rhs,
                                   const char **abs_node,
                                   int *cost,
                                   int **transl);

const char *ixml_to_yaep_read_rule(const char ***rhs,
                                   const char **abs_node,
                                   int *cost,
                                   int **transl)
{
    if (yaep_j_ >= yaep_state_->ixml_rules->size) return NULL;
    IXMLRule *rule = (IXMLRule*)vector_element_at(yaep_state_->ixml_rules, yaep_j_);
    *abs_node = rule->rule_name->name;
    if (rule->rule_name->alias) *abs_node = rule->rule_name->alias;
    size_t num_rhs = rule->rhs->size;
    if (yaep_tmp_rhs_) free(yaep_tmp_rhs_);
    yaep_tmp_rhs_ = (char**)calloc(num_rhs+1, sizeof(char*));
    if (yaep_tmp_transl_) free(yaep_tmp_transl_);
    yaep_tmp_transl_ = (int*)calloc(num_rhs+1, sizeof(char*));
    for (size_t i = 0; i < num_rhs; ++i)
    {
        yaep_tmp_transl_[i] = (int)i;
        IXMLTermType *tt = (IXMLTermType*)vector_element_at(rule->rhs, i);
        if (*tt == IXML_TERMINAL)
        {
            IXMLTerminal *t = (IXMLTerminal*)tt;
            yaep_tmp_rhs_[i] = t->name;
        }
        else if (*tt == IXML_NON_TERMINAL)
        {
            IXMLNonTerminal *nt = (IXMLNonTerminal*)tt;
            yaep_tmp_rhs_[i] = nt->name;
        }
        else
        {
            fprintf(stderr, "Internal error %d as term type does not exist!\n", *tt);
            assert(false);
        }
    }
    yaep_tmp_transl_[num_rhs] = -1;
    yaep_tmp_rhs_[num_rhs] = NULL;
    *rhs = (const char **)yaep_tmp_rhs_;
    *transl = yaep_tmp_transl_;
    *cost = 0;
    yaep_j_++;
    return rule->rule_name->name;
}

bool xmq_parse_buffer_ixml(XMQParseState *state, const char *start, const char *stop, struct grammar *g)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    state->ixml_rules = vector_create();
    state->ixml_terminals_map = hashmap_create(16);
    state->ixml_non_terminals = vector_create();
    state->ixml_rule_stack = new_stack();

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = XMQ_ERROR_NONE;

    if (state->parse && state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        parse_ixml(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "failed to parse whole buffer";
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        XMQParseError error_nr = state->error_nr;
        generate_state_error_message(state, error_nr, start, stop);
        return false;
    }

    if (state->parse && state->parse->done) state->parse->done(state);

    // Now build vectors suitable for yaep.
    yaep_i_ = hashmap_iterate(state->ixml_terminals_map);
    yaep_state_ = state;

    HashMapIterator *i = hashmap_iterate(state->ixml_terminals_map);

    const char *name;
    void *val;
    while (hashmap_next_key_value(i, &name, &val))
    {
        IXMLTerminal *t = (IXMLTerminal*)val;
        printf("TERMINAL %s %d\n", t->name, t->code);
    }

    hashmap_free_iterator(i);

    for (size_t i = 0; i < state->ixml_rules->size; ++i)
    {
        IXMLRule *r = (IXMLRule*)vector_element_at(state->ixml_rules, i);
        printf("RULE %c%s \n", r->rule_name->mark, r->rule_name->name);
        for (size_t j = 0; j < r->rhs->size; ++j)
        {
            IXMLTermType *tt = (IXMLTermType*)vector_element_at(r->rhs, j);
            if (*tt == IXML_TERMINAL)
            {
                IXMLTerminal *t = (IXMLTerminal*)tt;
                printf("   T %s %d\n", t->name, t->code);
            }
            else
            {
                IXMLNonTerminal *nt = (IXMLNonTerminal*)tt;
                printf("  nt %c%s \n", nt->mark, nt->name);
            }
        }
    }

    int rc = yaep_read_grammar(g, 0, ixml_to_yaep_read_terminal, ixml_to_yaep_read_rule);

    hashmap_free_iterator(yaep_i_);

    if (rc != 0)
    {
        state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
        state->error_info = "internal error, yaep did not accept generated yaep grammar";
        printf("xmq: could not parse input using ixml/yaep grammar: %s\n", yaep_error_message(g));
        longjmp(state->error_handler, 1);
    }

    if (yaep_tmp_rhs_) free(yaep_tmp_rhs_);
    if (yaep_tmp_transl_) free(yaep_tmp_transl_);

    vector_free_and_values(state->ixml_rules, (FreeFuncPtr)free_ixml_rule);
    state->ixml_rules = NULL;

    hashmap_free_and_values(state->ixml_terminals_map, (FreeFuncPtr)free_ixml_terminal);
    state->ixml_terminals_map = NULL;

    vector_free_and_values(state->ixml_non_terminals, (FreeFuncPtr)free_ixml_nonterminal);
    state->ixml_non_terminals = NULL;

    free_stack(state->ixml_rule_stack);
    state->ixml_rule_stack = NULL;

    return true;
}

void skip_comment(const char **i)
{
    const char *j = *i;
    ASSERT(*j == '{');
    for (;;)
    {
        j++;
        if (*j == '{') skip_comment(&j);
        if (*j == '}') break;
        if (*j == 0) return;
    }
    j++;
    *i = j;
}

void skip_encoded(const char **i)
{
    const char *j = *i;
    if (*j != '#') return;
    j++;
    while (is_ixml_hex_char(*j)) j++;
    *i = j;
}

void skip_mark(const char **i)
{
    const char *j = *i;
    char c = *j;
    if (c == '^' || c == '-' || c == '@') // mark
    {
        j++;
        while (is_ixml_whitespace_char(*j)) j++;
    }
    *i = j;
}

void skip_string(const char **i)
{
    const char *j = *i;
    if (*j == '"') // mark
    {
        j++;
        while (*j != '"')
        {
            if (*j == 0) return; // Ouch string not closed.
            if (*j == '"' && *(j+1) == '"') j++;
            j++;
        }
        // Move past the last "
        j++;
    }
    else
    if (*j == '\'') // mark
    {
        j++;
        while (*j != '\'')
        {
            if (*j == 0) return; // Ouch string not closed.
            if (*j == '\'' && *(j+1) == '\'') j++;
            j++;
        }
        j++;
    }
    *i = j;
}

void skip_tmark(const char **i)
{
    const char *j = *i;
    char c = *j;
    if (c == '^' || c == '-') // tmark
    {
        j++;
        while (is_ixml_whitespace_char(*j)) j++;
    }
    *i = j;
}

void skip_whitespace(const char **i)
{
    const char *j = *i;
    while (is_ixml_whitespace_char(*j))
    {
        if (*j == '{') skip_comment(&j);
        else j++;
    }
    *i = j;
}

void add_yaep_grammar_rule(char mark, const char *name_start, const char *name_stop)
{
}

void add_yaep_terminal(XMQParseState *state, IXMLTerminal *terminal)
{
    hashmap_put(state->ixml_terminals_map, terminal->name, terminal);
}

void add_yaep_terminal_to_rule(XMQParseState *state, IXMLTerminal *terminal, IXMLRule *rule)
{
    vector_push_back(rule->rhs, terminal);
}

void add_yaep_tmp_terminal(XMQParseState *state, char *name, int code)
{
    IXMLTerminal *t = new_ixml_terminal();
    t->name = name;
    t->code = code;
    vector_push_back(state->ixml_tmp_terminals, t);
}

void add_yaep_tmp_terminals_to_rule(XMQParseState *state, IXMLRule *rule)
{
    for (size_t i = 0; i < state->ixml_tmp_terminals->size; ++i)
    {
        IXMLTerminal *te = state->ixml_tmp_terminals->elements[i];
        IXMLTerminal *t = (IXMLTerminal*)hashmap_get(state->ixml_terminals_map, te->name);
        if (t == NULL)
        {
            add_yaep_terminal(state, te);
        }
        add_yaep_terminal_to_rule(state, te, rule);
        state->ixml_tmp_terminals->elements[i] = NULL;
    }
}

void allocate_yaep_tmp_terminals(XMQParseState *state)
{
    assert(state->ixml_tmp_terminals == NULL);
    state->ixml_tmp_terminals = vector_create();
}

void free_yaep_tmp_terminals(XMQParseState *state)
{
    // Only free the vector, the content is copied to another array.
    vector_free(state->ixml_tmp_terminals);
    state->ixml_tmp_terminals = NULL;
}

bool has_ixml_tmp_terminals(XMQParseState *state)
{
    assert(state->ixml_tmp_terminals);

    return state->ixml_tmp_terminals->size > 0;
}

IXMLRule *new_ixml_rule()
{
    IXMLRule *r = (IXMLRule*)calloc(1, sizeof(IXMLRule));
    r->rule_name = new_ixml_nonterminal();
    r->rhs = vector_create();
    return r;
}

void free_ixml_rule(IXMLRule *r)
{
    if (r->rule_name) free_ixml_nonterminal(r->rule_name);
    r->rule_name = NULL;
    vector_free(r->rhs);
    r->rhs = NULL;
    free(r);
}

IXMLTerminal *new_ixml_terminal()
{
    IXMLTerminal *t = (IXMLTerminal*)calloc(1, sizeof(IXMLTerminal));
    t->type = IXML_TERMINAL;
    return t;
}

void free_ixml_terminal(IXMLTerminal *t)
{
    if (t->name) free(t->name);
    free(t);
}

IXMLNonTerminal *new_ixml_nonterminal()
{
    IXMLNonTerminal *t = (IXMLNonTerminal*)calloc(1, sizeof(IXMLNonTerminal));
    t->type = IXML_NON_TERMINAL;
    return t;
}

void free_ixml_nonterminal(IXMLNonTerminal *nt)
{
    if (nt->name) free(nt->name);
    nt->name = NULL;
    free(nt);
}

#else

// Empty function when XMQ_NO_IXML is defined.
bool xmq_parse_ixml_grammar(struct grammar *g,
                            struct yaep_tree_node **root,
                            int *ambiguous,
                            XMQDoc *doq,
                            const char *start,
                            const char *stop,
                            bool build_xml_of_ixml)
{
    return false;
}

#endif // IXML_MODULE
