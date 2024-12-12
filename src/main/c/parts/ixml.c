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
#include"xmq.h"
#include"yaep.h"

#include<assert.h>
#include<string.h>
#include<stdbool.h>

#endif

#ifdef IXML_MODULE

#define DEBUG_IXML_GRAMMAR

#ifdef DEBUG_IXML_GRAMMAR
#define IXML_STEP(name,state) {                 \
    if (false && xmq_trace_enabled_) {                                \
        char *tmp = xmq_quote_as_c(state->i, state->i+10, false);           \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("dbg " #name " >%s...\n", tmp);       \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("{\n");      \
        state->depth++; \
        free(tmp); \
    } \
}
#define IXML_DONE(name,state) {                 \
    if (false && xmq_trace_enabled_) {                                \
        char *tmp = xmq_quote_as_c(state->i, state->i+10, false);           \
        state->depth--; \
        for (int i=0; i<state->depth; ++i) trace("    "); \
        trace("}\n"); \
        free(tmp); \
    } \
}

#define EAT(name, num) { \
    if (false && xmq_trace_enabled_) { \
        char *tmp = xmq_quote_as_c(state->i, state->i+num, false);        \
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

void add_insertion_rule(XMQParseState *state, const char *content);
void add_single_char_rule(XMQParseState *state, IXMLNonTerminal *nt, int uc, char mark, char tmark);

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
void parse_ixml_encoded(XMQParseState *state, bool add_terminal);
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
void parse_ixml_term_nonterminal(XMQParseState *state);
void parse_ixml_prolog(XMQParseState *state);
void parse_ixml_range(XMQParseState *state);
void parse_ixml_quoted(XMQParseState *state);
void parse_ixml_rule(XMQParseState *state);
void parse_ixml_string(XMQParseState *state, int **content);
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
bool has_more_than_one_ixml_tmp_terminals(XMQParseState *state);
void add_yaep_term_to_rule(XMQParseState *state, char mark, IXMLTerminal *t, IXMLNonTerminal *nt);
void add_yaep_terminal(XMQParseState *state, IXMLTerminal *terminal);
IXMLNonTerminal *lookup_yaep_nonterminal_already(XMQParseState *state, const char *name);
void add_yaep_nonterminal(XMQParseState *state, IXMLNonTerminal *nonterminal);
void add_yaep_tmp_term_terminal(XMQParseState *state, char *name, int code);
// Store all state->ixml_tmp_terminals on the rule rhs.
void add_yaep_tmp_terminals_to_rule(XMQParseState *state, IXMLRule *rule, char mark);

void do_ixml(XMQParseState *state);
void do_ixml_comment(XMQParseState *state, const char *start, const char *stop);
void do_ixml_rule(XMQParseState *state, const char *name_start, const char *name_stop);
void do_ixml_alt(XMQParseState *state);
void do_ixml_nonterminal(XMQParseState *state, const char *name_start, char *name_stop);
void do_ixml_option(XMQParseState *state);

IXMLRule *new_ixml_rule();
void free_ixml_rule(IXMLRule *r);
IXMLTerminal *new_ixml_terminal();
IXMLCharset *new_ixml_charset(bool exclude);
void new_ixml_charset_part(IXMLCharset *cs, int from, int to, const char *category);
void free_ixml_charset(IXMLCharset *cs);
void free_ixml_terminal(IXMLTerminal *t);
IXMLNonTerminal *new_ixml_nonterminal();
IXMLNonTerminal *copy_ixml_nonterminal(IXMLNonTerminal *nt);
void free_ixml_nonterminal(IXMLNonTerminal *t);
void free_ixml_term(IXMLTerm *t);

char *generate_rule_name(XMQParseState *state);
char *generate_charset_rule_name(IXMLCharset *cs, char mark);
void make_last_term_optional(XMQParseState *state);
void make_last_term_repeat(XMQParseState *state, char factor_mark);
void make_last_term_repeat_zero(XMQParseState *state, char factor_mark);
void make_last_term_repeat_infix(XMQParseState *state, char factor_mark, char infix_mark);
void make_last_term_repeat_zero_infix(XMQParseState *state, char factor_mark, char infix_mark);

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
    // Most unicode category classes (codes) are two characters Nd, Zs, Lu etc.
    // Some aggregated classes are a single uppercase L,N
    // A single silly is LC.
    bool letter = ((b >= 'a' && b <= 'z') || b == 'C');
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
    xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)"ixml", NULL);
    state->element_last = root;
    xmlDocSetRootElement(state->doq->docptr_.xml, root);
    state->doq->root_.node = root;
    stack_push(state->element_stack, state->element_last);

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

        if (c == ',')
        {
            if (c != ',')
            {
                state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
                state->error_info = "expected , or . here";
                longjmp(state->error_handler, 1);
            }
            EAT(comma, 1);
        }

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
        parse_ixml_whitespace(state);

        if (is_ixml_eob(state) ||
            is_ixml_rule_end(*(state->i)) ||
            is_ixml_group_end(state)) break;

        char c = *(state->i);
        if (c != '|')
        {
            if (!is_ixml_alt_start(state))
            {
                state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR; // TODO
                state->error_info = "expected alt here";
                longjmp(state->error_handler, 1);
            }
            parse_ixml_alt(state);

            parse_ixml_whitespace(state);
        }

        c = *(state->i);
        if (is_ixml_rule_end(c) || is_ixml_group_end(state)) break;
        if (c != '|' && c != ';')
        {
            state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
            state->error_info = "expected ; or | here";
            longjmp(state->error_handler, 1);
        }
        EAT(choice, 1);

        // The yaep grammar performs | by having mutiple rule entries
        // with the same name. We reached an alternative version of this
        // rule, create a new rule with the same name.
        parse_ixml_whitespace(state);
        IXMLNonTerminal *name = state->ixml_rule->rule_name;
        char mark = state->ixml_rule->mark;
        state->ixml_rule = new_ixml_rule();
        state->ixml_rule->rule_name->name = strdup(name->name);
        state->ixml_rule->mark = mark;
        vector_push_back(state->ixml_rules, state->ixml_rule);
    }

    IXML_DONE(alts, state);
}

void parse_ixml_charset(XMQParseState *state)
{
    IXML_STEP(charset, state);
    ASSERT(is_ixml_charset_start(state));

    char tmark = 0;
    if (is_ixml_tmark_char(*(state->i)))
    {
        tmark = *(state->i);
        EAT(tmark, 1);
        parse_ixml_whitespace(state);
    }

    char category[3];
    category[0] = 0;
    bool exclude = false;
    if (*(state->i) == '~')
    {
        exclude = true;
        EAT(negate, 1);
        parse_ixml_whitespace(state);
    }

    ASSERT(*(state->i) == '[');

    EAT(left_bracket, 1);
    parse_ixml_whitespace(state);

    state->ixml_charset = new_ixml_charset(exclude);

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
            new_ixml_charset_part(state->ixml_charset,
                                  state->ixml_charset_from,
                                  state->ixml_charset_to,
                                  "");
        }
        else if (is_ixml_encoded_start(state))
        {
            parse_ixml_encoded(state, false);
            new_ixml_charset_part(state->ixml_charset,
                                  state->ixml_encoded,
                                  state->ixml_encoded,
                                  "");
        }
        else if (is_ixml_code_start(state))
        {
            // Category can be Lc or just L.
            int n = is_ixml_code_start(state);
            category[0] = *state->i;
            category[1] = 0;
            if (n > 1) category[1] = *(state->i+1);
            category[2] = 0;
            EAT(unicode_category, n);
            const char **check = unicode_lookup_category_parts(category);
            if (!check)
            {
                state->error_nr = XMQ_ERROR_IXML_SYNTAX_ERROR;
                state->error_info = "unknown unicode category";
                longjmp(state->error_handler, 1);
            }
            parse_ixml_whitespace(state);
            new_ixml_charset_part(state->ixml_charset,
                                  0,
                                  0,
                                  category);
        }
        else if (is_ixml_string_start(state))
        {
            int *content = NULL;
            parse_ixml_string(state, &content);
            int *i = content;
            while (*i)
            {
                new_ixml_charset_part(state->ixml_charset,
                                      *i,
                                      *i,
                                      "");
                i++;
            }
            free(content);
            parse_ixml_whitespace(state);
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

    char *cs_name = generate_charset_rule_name(state->ixml_charset, tmark);
    IXMLNonTerminal *nt = lookup_yaep_nonterminal_already(state, cs_name);
    if (!nt)
    {
        nt = new_ixml_nonterminal();
        nt->name = cs_name;
        nt->charset = state->ixml_charset;
        add_yaep_nonterminal(state, nt);
    }
    else
    {
        free_ixml_charset(state->ixml_charset);
        state->ixml_charset = NULL;
        free(cs_name);
    }
    add_yaep_term_to_rule(state, tmark, NULL, nt);
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

void parse_ixml_encoded(XMQParseState *state, bool add_terminal)
{
    IXML_STEP(encoded, state);
    ASSERT(is_ixml_encoded_start(state));

    int tmark = 0;
    if (is_ixml_tmark_start(state))
    {
        tmark = *(state->i);
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
    state->ixml_mark = tmark;
    state->ixml_encoded = value;

    parse_ixml_whitespace(state);

    if (add_terminal)
    {
        char buffer[16];
        snprintf(buffer, 15, "#%x", value);

        add_yaep_tmp_term_terminal(state, strdup(buffer), value);
    }

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

        if (has_ixml_tmp_terminals(state))
        {
            if (has_more_than_one_ixml_tmp_terminals(state))
            {
                // Create a group.
                IXMLNonTerminal *nt = new_ixml_nonterminal();
                nt->name = generate_rule_name(state);
                add_yaep_nonterminal(state, nt);
                add_yaep_term_to_rule(state, '-', NULL, nt);
                stack_push(state->ixml_rule_stack, state->ixml_rule);
                state->ixml_rule = new_ixml_rule();
                state->ixml_rule->mark = '-';
                vector_push_back(state->ixml_rules, state->ixml_rule);

                free_ixml_nonterminal(state->ixml_rule->rule_name);
                state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

                // Add the terminals to the inner group/rule.
                add_yaep_tmp_terminals_to_rule(state, state->ixml_rule, state->ixml_mark);

                // Pop back out.
                state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
            }
            else
            {
                add_yaep_tmp_terminals_to_rule(state, state->ixml_rule, state->ixml_mark);
            }
        }

        free_yaep_tmp_terminals(state);
    }
    else if (is_ixml_nonterminal_start(state))
    {
        state->ixml_nonterminal = new_ixml_nonterminal();

        parse_ixml_term_nonterminal(state);

        add_yaep_nonterminal(state, state->ixml_nonterminal);
        add_yaep_term_to_rule(state, state->ixml_mark, NULL, state->ixml_nonterminal);
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

    parse_ixml_whitespace(state);

    if (is_ixml_alt_start(state))
    {
        IXMLNonTerminal *nt = new_ixml_nonterminal();
        nt->name = generate_rule_name(state);

        add_yaep_nonterminal(state, nt);
        add_yaep_term_to_rule(state, '-', NULL, nt);

        stack_push(state->ixml_rule_stack, state->ixml_rule);
        state->ixml_rule = new_ixml_rule();
        state->ixml_rule->mark = '-';
        vector_push_back(state->ixml_rules, state->ixml_rule);

        free_ixml_nonterminal(state->ixml_rule->rule_name);
        state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

        parse_ixml_alts(state);

        state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
    }
    else if (*(state->i) != ')')
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
        int *content = NULL;
        parse_ixml_string(state, &content);
        size_t len;
        for (len = 0; content[len]; ++len);
        // If all require 4 bytes of utf8, then this is the max length.
        char *buf = (char*)malloc(len*4 + 1);
        size_t offset = 0;
        for (size_t i = 0; i < len; ++i)
        {
            UTF8Char c;
            size_t l = encode_utf8(content[i], &c);
            strncpy(buf+offset, c.bytes, l);
            offset += l;
        }
        buf[offset] = 0;
        add_insertion_rule(state, buf);
        free(buf);
        free(content);
    }
    else if (is_ixml_encoded_start(state))
    {
        parse_ixml_encoded(state, true);
        UTF8Char c;
        encode_utf8(state->ixml_encoded, &c);
        add_insertion_rule(state, c.bytes);
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
        parse_ixml_encoded(state, true);
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

void parse_ixml_term_nonterminal(XMQParseState *state)
{
    IXML_STEP(nonterminal, state);
    ASSERT(is_ixml_naming_start(state));

    IXMLNonTerminal *nt = state->ixml_nonterminal;
    parse_ixml_naming(state, &state->ixml_mark, &nt->name, &nt->alias);

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

    int *content;
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
        int *content;
        parse_ixml_string(state, &content);
        state->ixml_charset_from = content[0];
        free(content);
    }
    else
    {
        parse_ixml_encoded(state, false);
        state->ixml_charset_from = state->ixml_encoded;
    }
    parse_ixml_whitespace(state);

    // This is guaranteed by the is range test in the assert.
    ASSERT(*(state->i) == '-');
    EAT(range_minus, 1);

    parse_ixml_whitespace(state);

    if (is_ixml_string_start(state))
    {
        int *content;
        parse_ixml_string(state, &content);
        state->ixml_charset_to = content[0];
        free(content);
    }
    else if (is_ixml_encoded_start(state))
    {
        parse_ixml_encoded(state, false);
        state->ixml_charset_to = state->ixml_encoded;
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

    char tmark = 0;
    if (is_ixml_tmark_start(state))
    {
        tmark = *(state->i);
        EAT(quoted_tmark, 1);
        parse_ixml_whitespace(state);
    }

    state->ixml_mark = tmark;

    int *content = NULL;
    parse_ixml_string(state, &content);

    for (int *i = content; *i; ++i)
    {
        char buf[16];
        snprintf(buf, 15, "#%x", *i);
        add_yaep_tmp_term_terminal(state, strdup(buf), *i);
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

    stack_push(state->ixml_rule_stack, state->ixml_rule);

    state->ixml_rule = new_ixml_rule();
    vector_push_back(state->ixml_rules, state->ixml_rule);

    parse_ixml_naming(state,
                      &state->ixml_rule->mark,
                      &state->ixml_rule->rule_name->name,
                      &state->ixml_rule->rule_name->alias);

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

    state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);

    IXML_DONE(rule, state);
}

void parse_ixml_string(XMQParseState *state, int **content)
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

        int uc = 0;
        size_t len = 0;
        bool ok = decode_utf8(state->i, state->buffer_stop, &uc, &len);
        if (!ok)
        {
            fprintf(stderr, "xmq: broken utf8\n");
            exit(1);
        }
        membuffer_append_int(buf, uc);
        EAT(string_inside, len);
    }
    // Add a zero termination to the integer array.
    membuffer_append_int(buf, 0);

    char *quote = free_membuffer_but_return_trimmed_content(buf);
    *content = (int*)quote;

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

    char factor_mark = state->ixml_mark;
    char c = *(state->i);
    if (c == '?')
    {
        EAT(option, 1);
        parse_ixml_whitespace(state);

        make_last_term_optional(state);
    }
    else if (c == '*' || c == '+')
    {
        // Remember the mark for the member.
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
            // We have value++infix
            if (c == '*')
            {
                EAT(star, 1);
            }
            else
            {
                EAT(plus, 1);
            }
            parse_ixml_whitespace(state);

            // infix separator
            parse_ixml_factor(state);
            char infix_mark = state->ixml_mark;

            // We have value++infix value**infix
            if (c == '+') make_last_term_repeat_infix(state, factor_mark, infix_mark);
            else  make_last_term_repeat_zero_infix(state, factor_mark, infix_mark);

        }
        else
        {
            // We have value+ or value*
            if (c == '+') make_last_term_repeat(state, factor_mark);
            else  make_last_term_repeat_zero(state, factor_mark);
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

const char *ixml_to_yaep_read_terminal(YaepParseRun *pr,
                                       YaepGrammar *g,
                                       int *code);

const char *ixml_to_yaep_read_terminal(YaepParseRun *pr,
                                       YaepGrammar *g,
                                       int *code)
{
    XMQParseState *state = (XMQParseState*)pr->user_data;
    const char *key;
    void *val;
    bool ok = hashmap_next_key_value(state->yaep_i_, &key, &val);

    if (ok)
    {
        IXMLTerminal *t = (IXMLTerminal*)val;
        *code = t->code;
        return t->name;
    }

    return NULL;
}

const char *ixml_to_yaep_read_rule(YaepParseRun *pr,
                                   YaepGrammar *g,
                                   const char ***rhs,
                                   const char **abs_node,
                                   int *cost,
                                   int **transl,
                                   char *mark,
                                   char **marks);

const char *ixml_to_yaep_read_rule(YaepParseRun *pr,
                                   YaepGrammar *g,
                                   const char ***rhs,
                                   const char **abs_node,
                                   int *cost,
                                   int **transl,
                                   char *mark,
                                   char **marks)
{
    XMQParseState *state = (XMQParseState*)pr->user_data;
    if (state->yaep_j_ >= state->ixml_rules->size) return NULL;
    IXMLRule *rule = (IXMLRule*)vector_element_at(state->ixml_rules, state->yaep_j_);

    // Check that the rule has not been removed when expainding charsets.
    if (true) //FIXMErule->mark != 'X')
    {
        // This is a valid rule.
        size_t num_rhs = rule->rhs_terms->size;

        // Add rhs rules as translations.
        if (state->yaep_tmp_rhs_) free(state->yaep_tmp_rhs_);
        state->yaep_tmp_rhs_ = (char**)calloc(num_rhs+1, sizeof(char*));
        if (state->yaep_tmp_marks_) free(state->yaep_tmp_marks_);
        state->yaep_tmp_marks_ = (char*)calloc(num_rhs+1, sizeof(char));
        memset(state->yaep_tmp_marks_, 0, num_rhs+1);

        for (size_t i = 0; i < num_rhs; ++i)
        {
            IXMLTerm *term = (IXMLTerm*)vector_element_at(rule->rhs_terms, i);
            state->yaep_tmp_marks_[i] = term->mark;
            if (term->type == IXML_TERMINAL)
            {
                state->yaep_tmp_rhs_[i] = term->t->name;
            }
            else if (term->type == IXML_NON_TERMINAL)
            {
                state->yaep_tmp_rhs_[i] = term->nt->name;
            }
            else
            {
                fprintf(stderr, "Internal error %d as term type does not exist!\n", term->type);
                assert(false);
            }
        }

        *abs_node = rule->rule_name->name;
        if (rule->rule_name->alias) *abs_node = rule->rule_name->alias;

        if (state->yaep_tmp_transl_) free(state->yaep_tmp_transl_);
        state->yaep_tmp_transl_ = (int*)calloc(num_rhs+1, sizeof(char*));
        for (size_t i = 0; i < num_rhs; ++i)
        {
            state->yaep_tmp_transl_[i] = (int)i;
        }
        state->yaep_tmp_transl_[num_rhs] = -1;

        state->yaep_tmp_rhs_[num_rhs] = NULL;
        *rhs = (const char **)state->yaep_tmp_rhs_;
        *marks = state->yaep_tmp_marks_;
        *transl = state->yaep_tmp_transl_;
        *cost = 1;
        *mark = rule->mark;
    }
    state->yaep_j_++;
    return rule->rule_name->name;
}

bool ixml_build_yaep_grammar(YaepParseRun *pr,
                             YaepGrammar *g,
                             XMQParseState *state,
                             const char *grammar_start,
                             const char *grammar_stop,
                             const char *content_start,
                             const char *content_stop)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    pr->user_data = state;
    pr->grammar = g;
    state->ixml_rules = vector_create();
    state->ixml_terminals_map = hashmap_create(256);
    state->ixml_non_terminals = vector_create();
    state->ixml_rule_stack = stack_create();

    state->buffer_start = grammar_start;
    state->buffer_stop = grammar_stop;
    state->i = grammar_start;
    state->line = 1;
    state->col = 1;
    state->error_nr = XMQ_ERROR_NONE;

    if (state->parse && state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        // Parse the IXML grammar.
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
        generate_state_error_message(state, error_nr, grammar_start, grammar_stop);
        return false;
    }

    if (state->parse && state->parse->done) state->parse->done(state);

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

void add_yaep_term_to_rule(XMQParseState *state, char mark, IXMLTerminal *t, IXMLNonTerminal *nt)
{
    IXMLTerm *term = (IXMLTerm*)calloc(1, sizeof(IXMLTerm));
    assert( (t || nt) && !(t && nt) );
    term->type = t ? IXML_TERMINAL :  IXML_NON_TERMINAL;
    term->mark = mark;
    term->t = t;
    term->nt = nt;
    vector_push_back(state->ixml_rule->rhs_terms, term);
}

void add_yaep_terminal(XMQParseState *state, IXMLTerminal *terminal)
{
    hashmap_put(state->ixml_terminals_map, terminal->name, terminal);
}

IXMLNonTerminal *lookup_yaep_nonterminal_already(XMQParseState *state, const char *name)
{
    // FIXME. Replace this with a set or map for faster lookup if needed.
    for (size_t i = 0; i < state->ixml_non_terminals->size; ++i)
    {
        IXMLNonTerminal *nt = (IXMLNonTerminal*)vector_element_at(state->ixml_non_terminals, i);
        if (!strcmp(name, nt->name)) return nt;
    }
    return NULL;
}

void add_yaep_nonterminal(XMQParseState *state, IXMLNonTerminal *nt)
{
    vector_push_back(state->ixml_non_terminals, nt);
}

void add_yaep_tmp_term_terminal(XMQParseState *state, char *name, int code)
{
    IXMLTerminal *t = new_ixml_terminal();
    t->name = name;
    t->code = code;
    vector_push_back(state->ixml_tmp_terminals, t);
}

void add_single_char_rule(XMQParseState *state, IXMLNonTerminal *nt, int uc, char mark, char tmark)
{
    IXMLRule *rule = new_ixml_rule();
    rule->rule_name->name = strdup(nt->name);
    rule->mark = mark;
    vector_push_back(state->ixml_rules, rule);
    char buffer[16];
    snprintf(buffer, 15, "#%x", uc);
    allocate_yaep_tmp_terminals(state);
    add_yaep_tmp_term_terminal(state, strdup(buffer), uc);
    state->ixml_rule = rule; // FIXME this is a bit ugly.
    add_yaep_tmp_terminals_to_rule(state, rule, tmark);
    free_yaep_tmp_terminals(state);
}

void add_insertion_rule(XMQParseState *state, const char *content)
{
    IXMLRule *rule = new_ixml_rule();
    // Generate a name like |+.......
    char *name = (char*)malloc(strlen(content)+3);
    name[0] = '|';
    name[1] = '+';
    strcpy(name+2, content);
    rule->rule_name->name = name;
    rule->mark = ' ';
    vector_push_back(state->ixml_rules, rule);

    IXMLNonTerminal *nt = copy_ixml_nonterminal(rule->rule_name);
    add_yaep_nonterminal(state, nt);
    add_yaep_term_to_rule(state, 0, NULL, nt);
}

void scan_content_fixup_charsets(XMQParseState *state, const char *start, const char *stop)
{
    const char *i = start;
    // We allocate a byte for every possible unicode. Yes, a bit excessive, 1 megabyte. Can be fixed though.
    // 17*65536 = 0x11000 = 1 MB
    char *used = (char*)malloc(0x110000);
    memset(used, 0, 0x110000);

    while (i < stop)
    {
        int uc = 0;
        size_t len = 0;
        bool ok = decode_utf8(i, stop, &uc, &len);
        if (!ok)
        {
            fprintf(stderr, "xmq: broken utf8\n");
            exit(1);
        }
        i += len;

        used[uc] = 1;
        char buf[16];
        snprintf(buf,16, "#%x", uc);
        IXMLTerminal *t = (IXMLTerminal*)hashmap_get(state->ixml_terminals_map, buf);
        if (t == NULL)
        {
            // Add terminal not in the grammar, but found inside the input content to be parsed.
            // This gives us better error messages from yaep. Instead of "No such token" we
            // get syntax error at line:col.
            t = new_ixml_terminal();
            t->name = strdup(buf);
            t->code = uc;
            add_yaep_terminal(state, t);
        }
    }

    for (size_t i = 0; i < state->ixml_non_terminals->size; ++i)
    {
        IXMLNonTerminal *nt = (IXMLNonTerminal*)vector_element_at(state->ixml_non_terminals, i);
        if (nt->charset)
        {
            bool include = nt->name[0] != '~';
            char tmark = 0;
            if ((nt->name[0] == '[' && nt->name[1] == '-') ||
                (nt->name[1] == '[' && nt->name[2] == '-'))
            {
                tmark = '-';
            }

            if (include)
            {
                // Including...
                for (IXMLCharsetPart *p = nt->charset->first; p; p = p->next)
                {
                    if (!p->category[0])
                    {
                        // Not a category, test a range.
                        // All characters within the range that have been used
                        // should be tested for.
                        for (int c = p->from; c <= p->to; ++c)
                        {
                            if (c >= 0 && c <= 0x10ffff && used[c])
                            {
                                add_single_char_rule(state, nt, c, '-', tmark);
                            }
                        }
                    }
                    else
                    {
                        const char **cat_part = unicode_lookup_category_parts(p->category);
                        for (; *cat_part; ++cat_part)
                        {
                            int *cat = NULL;
                            size_t cat_len = 0;
                            bool ok = unicode_get_category_part(*cat_part, &cat, &cat_len);
                            if (!ok)
                            {
                                fprintf(stderr, "Invalid unicode category: %s\n", *cat_part);
                                exit(1);
                            }
                            // All characters in the category that have been used
                            // should be tested for.
                            for (size_t i = 0; i < cat_len; ++i)
                            {
                                int c = cat[i];
                                if (c >= 0 && c <= 0x10ffff && used[c])
                                {
                                    add_single_char_rule(state, nt, c, '-', tmark);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                // Excluding....
                // Scan all actually used characters in the input content.
                for (int c = 0; c <= 0x10ffff; ++c)
                {
                    if (!used[c]) continue;
                    bool should_add = true;
                    for (IXMLCharsetPart *p = nt->charset->first; p; p = p->next)
                    {
                        if (p->category[0])
                        {
                            const char **cat_part = unicode_lookup_category_parts(p->category);
                            for (; *cat_part; ++cat_part)
                            {
                                int *cat = NULL;
                                size_t cat_len = 0;
                                bool ok = unicode_get_category_part(*cat_part, &cat, &cat_len);
                                if (!ok)
                                {
                                    fprintf(stderr, "Invalid unicode category: %s\n", *cat_part);
                                    exit(1);
                                }
                                // Is the used character in the category set?
                                if (category_has_code(c, cat, cat_len))
                                {
                                    // Yes it is.
                                    // Do not add the used character! We do not want to match against
                                    // it since it is excluded.
                                    should_add = false;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            // Is the used character in the range?
                            if (c >= p->from && c <= p->to)
                            {
                                // Yes it is.
                                // Do not add the used character! We do not want to match against
                                // it since it is excluded.
                                should_add= false;
                                break;
                            }
                        }
                    }
                    if (should_add)
                    {
                        add_single_char_rule(state, nt, c, '-', tmark);
                    }
                }
            }
        }
    }

    free(used);
}

void add_yaep_tmp_terminals_to_rule(XMQParseState *state, IXMLRule *rule, char mark)
{
    for (size_t i = 0; i < state->ixml_tmp_terminals->size; ++i)
    {
        IXMLTerminal *te = (IXMLTerminal*)state->ixml_tmp_terminals->elements[i];
        IXMLTerminal *t = (IXMLTerminal*)hashmap_get(state->ixml_terminals_map, te->name);
        if (t == NULL)
        {
            // This terminal was not already in the map. Add it.
            add_yaep_terminal(state, te);
            t = te;
        }
        else
        {
            // This terminal was already in the map. Free the terminal and add a
            // pointer the already stored termina to the rule.
            free_ixml_terminal(te);
        }
        add_yaep_term_to_rule(state, mark, t, NULL);
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

bool has_more_than_one_ixml_tmp_terminals(XMQParseState *state)
{
    assert(state->ixml_tmp_terminals);

    return state->ixml_tmp_terminals->size > 1;
}

IXMLRule *new_ixml_rule()
{
    IXMLRule *r = (IXMLRule*)calloc(1, sizeof(IXMLRule));
    r->rule_name = new_ixml_nonterminal();
    r->rhs_terms = vector_create();
    return r;
}

void free_ixml_rule(IXMLRule *r)
{
    if (r->rule_name) free_ixml_nonterminal(r->rule_name);
    r->rule_name = NULL;
    vector_free_and_values(r->rhs_terms, (FreeFuncPtr)free_ixml_term);
    r->rhs_terms = NULL;
    free(r);
}

IXMLTerminal *new_ixml_terminal()
{
    IXMLTerminal *t = (IXMLTerminal*)calloc(1, sizeof(IXMLTerminal));
    return t;
}

void free_ixml_terminal(IXMLTerminal *t)
{
    if (t->name) free(t->name);
    t->name = NULL;
    free(t);
}

IXMLNonTerminal *new_ixml_nonterminal()
{
    IXMLNonTerminal *nt = (IXMLNonTerminal*)calloc(1, sizeof(IXMLNonTerminal));
    return nt;
}

IXMLNonTerminal *copy_ixml_nonterminal(IXMLNonTerminal *nt)
{
    IXMLNonTerminal *t = (IXMLNonTerminal*)calloc(1, sizeof(IXMLNonTerminal));
    t->name = strdup(nt->name);
    return t;
}

void free_ixml_nonterminal(IXMLNonTerminal *nt)
{
    if (nt->name) free(nt->name);
    nt->name = NULL;
    if (nt->charset) free_ixml_charset(nt->charset);
    nt->charset = NULL;
    free(nt);
}

void free_ixml_term(IXMLTerm *t)
{
    t->t = NULL;
    t->nt = NULL;
    free(t);
}

IXMLCharset *new_ixml_charset(bool exclude)
{
    IXMLCharset *cs = (IXMLCharset*)calloc(1, sizeof(IXMLCharset));
    cs->exclude = exclude;
    return cs;
}

void new_ixml_charset_part(IXMLCharset *cs, int from, int to, const char *category)
{
    IXMLCharsetPart *csp = (IXMLCharsetPart*)calloc(1, sizeof(IXMLCharsetPart));
    csp->from = from;
    csp->to = to;
    strncpy(csp->category, category, 2);
    if (cs->last == NULL)
    {
        cs->first = cs->last = csp;
    }
    else
    {
        cs->last->next = csp;
        cs->last = csp;
    }
}

void free_ixml_charset(IXMLCharset *cs)
{
    IXMLCharsetPart *i = cs->first;
    while (i)
    {
        IXMLCharsetPart *next = i->next;
        i->next = NULL;
        free(i);
        i = next;
    }
    cs->first = NULL;
    cs->last = NULL;
    free(cs);
}

char *generate_rule_name(XMQParseState *state)
{
    char buf[16];
    snprintf(buf, 15, "|%d", state->num_generated_rules);
    state->num_generated_rules++;
    return strdup(buf);
}

char *generate_charset_rule_name(IXMLCharset *cs, char mark)
{
    char *buf = (char*)malloc(1024);
    buf[0] = 0;

    if (cs->exclude) strcat(buf, "~");
    strcat(buf, "[");
    char m[2];
    m[0] = mark;
    m[1] = 0;
    if (mark != 0 && mark != ' ') strcat(buf, m);

    IXMLCharsetPart *i = cs->first;
    bool first = true;

    while (i)
    {
        if (!first) { strcat(buf, ";"); } else { first = false; }
        if (i->category[0]) strcat(buf, i->category);
        else
        {
            if (i->from != i->to)
            {
                char s[16];
                snprintf(s, 16, "#%x-#%x", i->from, i->to);
                strcat(buf, s);
            }
            else
            {
                char s[16];
                snprintf(s, 16, "#%x", i->from);
                strcat(buf, s);
            }
        }
        i = i->next;
    }

    strcat(buf, "]");
    return buf;
}

void make_last_term_optional(XMQParseState *state)
{
    // Compose an anonymous rule.
    // 'a'? is replaced with the nonterminal /17 (for example)
    // Then /17 can be either 'a' or the empty string.
    // /17 : 'a'.
    // /17 : .

    // Pop the last term.
    IXMLTerm *term = (IXMLTerm*)vector_pop_back(state->ixml_rule->rhs_terms);

    IXMLNonTerminal *nt = new_ixml_nonterminal();
    nt->name = generate_rule_name(state);

    add_yaep_nonterminal(state, nt);
    add_yaep_term_to_rule(state, '-', NULL, nt);

    stack_push(state->ixml_rule_stack, state->ixml_rule);

    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // Single term in this rule alternative.
    add_yaep_term_to_rule(state, state->ixml_mark, term->t, term->nt);
    free(term);
    term = NULL;

    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';

    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // No terms in this rule alternative!

    state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
}

void make_last_term_repeat(XMQParseState *state, char factor_mark)
{
    // Compose an anonymous rule.
    // 'a'+ is replaced with the nonterminal /17 (for example)
    // Then /17 can be either 'a' or the /17, 'a'
    // /17 = "a".
    // /17 = /17, "a".

    // Pop the last term.
    IXMLTerm *term = (IXMLTerm*)vector_pop_back(state->ixml_rule->rhs_terms);

    IXMLNonTerminal *nt = new_ixml_nonterminal();
    nt->name = generate_rule_name(state);

    add_yaep_nonterminal(state, nt);
    add_yaep_term_to_rule(state, '-', NULL, nt);

    stack_push(state->ixml_rule_stack, state->ixml_rule);

    // Build first alternative rule: /17 = 'a'.
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // 'a'
    add_yaep_term_to_rule(state, factor_mark, term->t, term->nt);

    // Build second alternative rule: /17 = /17, 'a'.
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // /17, 'a'
    add_yaep_term_to_rule(state, '-', NULL, nt);
    add_yaep_term_to_rule(state, factor_mark, term->t, term->nt);

    free(term);
    term = NULL;

    state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
}

void make_last_term_repeat_infix(XMQParseState *state, char factor_mark, char infix_mark)
{
    // Compose an anonymous rule.
    // 'a'++'b' is replaced with the nonterminal /17 (for example)
    // Then /17 can be either 'a' or the /17, 'a'
    // /17 = "a".
    // /17 = /17, "b", "a".

    // Pop the last two terms.
    IXMLTerm *infix = (IXMLTerm*)vector_pop_back(state->ixml_rule->rhs_terms);
    IXMLTerm *term = (IXMLTerm*)vector_pop_back(state->ixml_rule->rhs_terms);

    IXMLNonTerminal *nt = new_ixml_nonterminal();
    nt->name = generate_rule_name(state);

    add_yaep_nonterminal(state, nt);
    add_yaep_term_to_rule(state, '-', NULL, nt);

    stack_push(state->ixml_rule_stack, state->ixml_rule);

    // Build first alternative rule: /17 = 'a'.
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // 'a'
    add_yaep_term_to_rule(state, factor_mark, term->t, term->nt);

    // Build second alternative rule: /17 = /17, 'b', 'a'.
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // /17, 'b', 'a'
    add_yaep_term_to_rule(state, '-', NULL, nt);
    add_yaep_term_to_rule(state, infix_mark, infix->t, infix->nt);
    add_yaep_term_to_rule(state, factor_mark, term->t, term->nt);

    free(term);
    free(infix);
    term = NULL;
    infix = NULL;

    state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
}

void make_last_term_repeat_zero(XMQParseState *state, char factor_mark)
{
    // Compose an anonymous rule.
    // 'a'+ is replaced with the nonterminal /17 (for example)
    // Then /17 can be either 'a' or the /17, 'a'
    // /17 = .
    // /17 = /17 | "a".

    // Pop the last term.
    IXMLTerm *term = (IXMLTerm*)vector_pop_back(state->ixml_rule->rhs_terms);

    IXMLNonTerminal *nt = new_ixml_nonterminal();
    nt->name = generate_rule_name(state);

    add_yaep_nonterminal(state, nt);
    add_yaep_term_to_rule(state, '-', NULL, nt);

    stack_push(state->ixml_rule_stack, state->ixml_rule);

    // Build first alternative rule: /17 = .
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // Build second alternative rule: /17 = /17, 'a'.
    state->ixml_rule = new_ixml_rule();
    state->ixml_rule->mark = '-';
    vector_push_back(state->ixml_rules, state->ixml_rule);

    free_ixml_nonterminal(state->ixml_rule->rule_name);
    state->ixml_rule->rule_name = copy_ixml_nonterminal(nt);

    // /17, 'a'
    add_yaep_term_to_rule(state, '-', NULL, nt);
    add_yaep_term_to_rule(state, factor_mark, term->t, term->nt);

    free(term);
    term = NULL;

    state->ixml_rule = (IXMLRule*)stack_pop(state->ixml_rule_stack);
}

void make_last_term_repeat_zero_infix(XMQParseState *state, char factor_mark, char infix_mark)
{
    make_last_term_repeat_infix(state, factor_mark, infix_mark);
    state->ixml_mark = '-';
    make_last_term_optional(state);
}

void ixml_print_grammar(XMQParseState *state)
{
    if (false && xmq_trace_enabled_)
    {
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
            printf("RULE %c%s\n", r->mark?r->mark:' ', r->rule_name->name);

            for (size_t j = 0; j < r->rhs_terms->size; ++j)
            {
                IXMLTerm *term = (IXMLTerm*)vector_element_at(r->rhs_terms, j);
                if (term->type == IXML_TERMINAL)
                {
                    printf("   T %c%s %d\n", term->mark?term->mark:' ', term->t->name, term->t->code);
                }
                else
                {
                    printf("   R %c%s \n", term->mark?term->mark:' ', term->nt->name);
                }
            }
        }
    }
}

#else


#endif // IXML_MODULE
