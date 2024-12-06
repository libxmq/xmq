/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef XMQ_INTERNALS_H
#define XMQ_INTERNALS_H

#include<assert.h>
#include<ctype.h>
#include<errno.h>
#include<math.h>
#include<setjmp.h>
#include<stdarg.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include"xmq.h"
#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/HTMLparser.h>
#include<libxml/HTMLtree.h>
#include<libxml/xmlreader.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>

#ifndef BUILDING_XMQ
#include"colors.h"
#endif

// STACK /////////////////////

struct Stack;
typedef struct Stack Stack;

struct Vector;
typedef struct Vector Vector;

struct HashMap;
typedef struct HashMap HashMap;

struct HashMapIterator;
typedef struct HashMapIterator HashMapIterator;

struct MemBuffer;
typedef struct MemBuffer MemBuffer;

struct YaepParseRun;
typedef struct YaepParseRun YaepParseRun;

struct YaepGrammar;
typedef struct YaepGrammar YaepGrammar;

struct XMQParseState;
typedef struct XMQParseState XMQParseState;

// DECLARATIONS /////////////////////////////////////////////////

#define LIST_OF_XMQ_TOKENS  \
    X(whitespace)           \
    X(equals)               \
    X(brace_left)           \
    X(brace_right)          \
    X(apar_left)            \
    X(apar_right)           \
    X(cpar_left)            \
    X(cpar_right)           \
    X(quote)                \
    X(entity)               \
    X(comment)              \
    X(comment_continuation) \
    X(element_ns)           \
    X(element_name)         \
    X(element_key)          \
    X(element_value_text)   \
    X(element_value_quote)  \
    X(element_value_entity) \
    X(element_value_compound_quote)  \
    X(element_value_compound_entity) \
    X(attr_ns)             \
    X(attr_key)            \
    X(attr_value_text)     \
    X(attr_value_quote)    \
    X(attr_value_entity)   \
    X(attr_value_compound_quote)    \
    X(attr_value_compound_entity)   \
    X(ns_declaration) \
    X(ns_colon)  \

#define MAGIC_COOKIE 7287528

struct XMQNode
{
    xmlNodePtr node;
};

struct XMQDoc
{
    union {
        xmlDocPtr xml;
        htmlDocPtr html;
    } docptr_; // Is both xmlDocPtr and htmlDocPtr.
    const char *source_name_; // File name or url from which the documented was fetched.
    int errno_; // A parse error is assigned a number.
    const char *error_; // And the error is explained here.
    XMQNode root_; // The root node.
    XMQContentType original_content_type_; // Remember if this document was created from xmq/xml etc.
    size_t original_size_; // Remember the original source size of the document it was loaded from.

    // For now these references are here. I am not quite sure where to put them in the future.
    // The IXML grammar can be parsed, but not actually turned into a yaep grammar until
    // we have seen the content to be parsed, since we want to shrink the charset membership tests to a minimum.
    // I.e. if the content only contains a:s and b:s (eg 'abbabbabaaab') then the charset test
    // ['a'-'z'] shrinks down to ['a';'b']
    YaepParseRun *yaep_parse_run_; // The currently executing parse variables.
    YaepGrammar *yaep_grammar_; // The yaep grammar to be used by the run.
    XMQParseState *yaep_parse_state_; // The parse state used to parse the ixml grammar.
};

#ifdef __cplusplus
enum Level : short {
#else
enum Level {
#endif
    LEVEL_XMQ = 0,
    LEVEL_ELEMENT_VALUE = 1,
    LEVEL_ELEMENT_VALUE_COMPOUND = 2,
    LEVEL_ATTR_VALUE = 3,
    LEVEL_ATTR_VALUE_COMPOUND = 4
};
typedef enum Level Level;

/**
    XMQOutputSettings:
    @add_indent: Default is 4. Indentation starts at 0 which means no spaces prepended.
    @compact: Print on a single line limiting whitespace to a minimum.
    @escape_newlines: Replace newlines with &#10; this is implied if compact is set.
    @escape_non_7bit: Replace all chars above 126 with char entities, ie &#10;
    @escape_tabs: Replace tabs with &#9;
    @output_format: Print xmq/xml/html/json
    @render_to: Render to terminal, html, tex.
    @render_raw: If true do not write surrounding html and css colors, likewise for tex.
    @only_style: Print only style sheet header.
    @write_content: Write content to buffer.
    @buffer_content: Supplied as buffer above.
    @write_error: Write error to buffer.
    @buffer_error: Supplied as buffer above.
    @colorings: Map from namespace (default is the empty string) to  prefixes/postfixes to colorize the output for ANSI/HTML/TEX.
*/
struct XMQOutputSettings
{
    int  add_indent;
    bool compact;
    bool omit_decl;
    bool use_color;
    bool bg_dark_mode;
    bool escape_newlines;
    bool escape_non_7bit;
    bool escape_tabs;

    XMQContentType output_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    const char *render_theme;

    XMQWriter content;
    XMQWriter error;

    // If printing to memory:
    MemBuffer *output_buffer;
    char **output_buffer_start;
    char **output_buffer_stop;
    size_t *output_skip;

    const char *indentation_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_tab; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_cr; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_nl; // If NULL use "\n" can be replaced with any other string.
    const char *prefix_line; // If non-NULL print this as the leader before each line.
    const char *postfix_line; // If non-NULL print this as the ending after each line.

    const char *use_id; // If non-NULL inser this id in the pre tag.
    const char *use_class; // If non-NULL insert this class in the pre tag.

    XMQTheme *theme; // The theme used to print.
    void *free_me;
    void *free_and_me;
};
typedef struct XMQOutputSettings XMQOutputSettings;

enum IXMLTermType
{
    IXML_TERMINAL, IXML_NON_TERMINAL, IXML_CHARSET
};
typedef enum IXMLTermType IXMLTermType;

struct IXMLCharsetPart;
typedef struct IXMLCharsetPart IXMLCharsetPart;
struct IXMLCharsetPart
{
    IXMLCharsetPart *next;
    // A charset range [ 'a' - 'z' ]
    // A single char is [ 'a' - 'a' ] used for each element in a string set definition, eg 'abc'
    int from, to;
    // A charset category [ Lc ] for lower case character or [Zs] for whitespace,
    // stored as two characters and the null terminatr.
    char category[3];
};

struct IXMLCharset
{
    // Negate the check, exclude the specified characters.
    bool exclude;
    // Charset contents.
    IXMLCharsetPart *first;
    IXMLCharsetPart *last;
};
typedef struct IXMLCharset IXMLCharset;

struct IXMLTerminal
{
    char *name;
    int code;
};
typedef struct IXMLTerminal IXMLTerminal;

struct IXMLNonTerminal
{
    char *name;
    char *alias;
    IXMLCharset *charset; // If the rule embodies a charset.
};
typedef struct IXMLNonTerminal IXMLNonTerminal;

struct IXMLTerm // A term is an element in the rhs vector in the rule.
{
    IXMLTermType type;
    char mark; // Exclude a term in a rule's rhs: rr: -A, B;  then only the contents of A will be printed.
    IXMLTerminal *t;
    IXMLNonTerminal *nt;
};
typedef struct IXMLTerm IXMLTerm;

struct IXMLRule
{
    IXMLNonTerminal *rule_name;
    char mark;
    Vector *rhs_terms;
};
typedef struct IXMLRule IXMLRule;

struct XMQParseState
{
    char *source_name; // Only used for generating any error messages.
    void *out;
    const char *buffer_start; // Points to first byte in buffer.
    const char *buffer_stop;   // Points to byte >after< last byte in buffer.
    const char *i; // Current parsing position.
    size_t line; // Current line.
    size_t col; // Current col.
    XMQParseError error_nr; // A standard parse error enum that maps to text.
    const char *error_info; // Additional info printed with the error nr.
    char *generated_error_msg; // Additional error information.
    MemBuffer *generating_error_msg;
    jmp_buf error_handler;

    bool simulated; // When true, this is generated from JSON parser to simulate an xmq element name.
    XMQParseCallbacks *parse;
    XMQDoc *doq;
    const char *implicit_root; // Assume that this is the first element name
    Stack *element_stack; // Top is last created node
    void *element_last; // Last added sibling to stack top node.
    bool parsing_doctype; // True when parsing a doctype.
    void *add_pre_node_before; // Used when retrofitting pre-root comments and doctype found in json.
    bool root_found; // Used to decide if _// should be printed before or after root.
    void *add_post_node_after; // Used when retrofitting post-root comments found in json.
    bool doctype_found; // True after a doctype has been parsed.
    bool parsing_pi; // True when parsing a processing instruction, pi.
    bool merge_text; // Merge text nodes and character entities.
    bool no_trim_quotes; // No trimming if quotes, used when reading json strings.
    bool ixml_all_parses; // If IXML parse is ambiguous then print all parses.
    bool ixml_try_to_recover; // If IXML parse fails, try to recover.
    const char *pi_name; // Name of the pi node just started.
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie; // Used to check that the state has been properly initialized.

    char *element_namespace; // The element namespace is found before the element name. Remember the namespace name here.
    char *attribute_namespace; // The attribute namespace is found before the attribute key. Remember the namespace name here.
    bool declaring_xmlns; // Set to true when the xmlns declaration is found, the next attr value will be a href
    void *declaring_xmlns_namespace; // The namespace to be updated with attribute value, eg. xmlns=uri or xmlns:prefix=uri

    void *default_namespace; // If xmlns=http... has been set, then a pointer to the namespace object is stored here.

    // These are used for better error reporting.
    const char *last_body_start;
    size_t last_body_start_line;
    size_t last_body_start_col;
    const char *last_attr_start;
    size_t last_attr_start_line;
    size_t last_attr_start_col;
    const char *last_quote_start;
    size_t last_quote_start_line;
    size_t last_quote_start_col;
    const char *last_compound_start;
    size_t last_compound_start_line;
    size_t last_compound_start_col;
    const char *last_equals_start;
    size_t last_equals_start_line;
    size_t last_equals_start_col;
    const char *last_suspicios_quote_end;
    size_t last_suspicios_quote_end_line;
    size_t last_suspicios_quote_end_col;

    ///////// The following variables are used when parsing ixml. //////////////////////////////////
    // Collect all unique terminals in this map from the name. #78 and 'x' is the same terminal with code 120.
    // The name is a #hex version of unicode codepoint.
    HashMap *ixml_terminals_map;
    // References to non-terminals, ie rules (includes mark -^@).
    Vector *ixml_non_terminals;
    // Store all rules here. The rule has a rule_name which is a non-terminal ref to itself.
    Vector *ixml_rules;
    // Rule stack is pushed by groups (..), ?, *, **, + and ++.
    Stack  *ixml_rule_stack;
    // Generated rule counter. Starts at 0 and increments after each generated
    // anonymous rule, ie (..), ? etc. The rules will have the names /0 /1 /2 etc.
    int num_generated_rules;
    // The ixml_rule is the current rule that we are building.
    IXMLRule *ixml_rule;
    // The ixml_nonterminal is the current rule reference.
    IXMLNonTerminal *ixml_nonterminal;
    // The ixml_tmp_terminals is the current collection of terminals, ie characters.
    // A single ixml #78 or 'x' or "x" adds a single terminal to this vector.
    // The string 'xy' adds two terminals to this vector, etc.
    // These terminals are then added to the rule's rhs.
    Vector *ixml_tmp_terminals;
    // The ixml_tmp_terms is the current collection of terms (terminal or non-terminals),
    // These terms are then added to the rule's rhs.
//    Vector *ixml_rhs_tmp_terms;
    // These are the marks @-^ for the terminals.
    Vector *ixml_rhs_tmp_marks;
    // The most recently parsed mark.
    char ixml_mark;
    // The most recently parsed encoded value #41
    int ixml_encoded;
    // Any charset we are currently building.
    IXMLCharset *ixml_charset;
    // Parsing ranges of charsets.
    int ixml_charset_from;
    int ixml_charset_to;
    // When parsing the grammar, collect all charset categories that we use.
    HashMap *ixml_found_categories;

    // These are used when travergins the IXML parse tree and generating the yaep grammar build calls.
    char **yaep_tmp_rhs_;
    char *yaep_tmp_marks_;
    int *yaep_tmp_transl_;
    HashMapIterator *yaep_i_;
    size_t yaep_j_;

    // When debugging parsing of ixml, track indentation of depth here.
    int depth;
    // Generate xml as well.
    bool build_xml_of_ixml;
};

/**
   XMQPrintState:
   @current_indent: The current_indent stores how far we have printed on the current line.
                    0 means nothing has been printed yet.
   @line_indent:  The line_indent stores the current indentation level from which print
                  starts for each new line. The line_indent is 0 when we start printing the xmq.
   @last_char: the last_char remembers the most recent printed character. or 0 if no char has been printed yet.
   @color_pre: The active color prefix.
   @prev_color_pre: The previous color prefix, used for restoring utf8 text after coloring unicode whitespace.
   @restart_line: after nl_and_indent print this to restart coloring of line.
   @ns: the last namespace reference.
   @output_settings: the output settings.
   @doc: The xmq document that is being printed.
*/
struct XMQPrintState
{
    size_t current_indent;
    size_t line_indent;
    int last_char;
    const char *replay_active_color_pre;
    const char *restart_line;
    const char *last_namespace;
    Stack *pre_nodes; // Used to remember leading comments/doctype when printing json.
    size_t pre_post_num_comments_total; // Number of comments outside of the root element.
    size_t pre_post_num_comments_used; // Active number of comment outside of the root element.
    Stack *post_nodes; // Used to remember ending comments when printing json.
    XMQOutputSettings *output_settings;
    XMQDoc *doq;
};
typedef struct XMQPrintState XMQPrintState;

/**
    XMQQuoteSettings:
    @force:           Always add single quotes. More quotes if necessary.
    @compact:         Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    @value_after_key: If enties are introduced by the quoting, then use compound ( ) around the content.

    @indentation_space: Use this as the indentation character. If NULL default to " ".
    @explicit_space: Use this as the explicit space/indentation character. If NULL default to " ".
    @newline:     Use this as the newline character. If NULL default to "\n".
    @prefix_line: Prepend each line with this. If NULL default to empty string.
    @postfix_line Suffix each line whith this. If NULL default to empty string.
    @prefix_entity: Prepend each entity with this. If NULL default to empty string.
    @postfix_entity: Suffix each entity whith this. If NULL default to empty string.
    @prefix_doublep: Prepend each ( ) with this. If NULL default to empty string.
    @postfix_doublep: Suffix each ( ) whith this. If NULL default to empty string.
*/
struct XMQQuoteSettings
{
    bool force; // Always add single quotes. More quotes if necessary.
    bool compact; // Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    bool value_after_key; // If enties are introduced by the quoting, then use compound (( )) around the content.

    const char *indentation_space;  // Use this as the indentation character. If NULL default to " ".
    const char *explicit_space;  // Use this as the explicit space character inside quotes. If NULL default to " ".
    const char *explicit_nl;      // Use this as the newline character. If NULL default to "\n".
    const char *explicit_tab;      // Use this as the tab character. If NULL default to "\t".
    const char *explicit_cr;      // Use this as the cr character. If NULL default to "\r".
    const char *prefix_line;  // Prepend each line with this. If NULL default to empty string.
    const char *postfix_line; // Append each line whith this, before any newline.
    const char *prefix_entity;  // Prepend each entity with this. If NULL default to empty string.
    const char *postfix_entity; // Append each entity whith this. If NULL default to empty string.
    const char *prefix_doublep;  // Prepend each ( ) with this. If NULL default to empty string.
    const char *postfix_doublep; // Append each ( ) whith this. If NULL default to empty string.
};
typedef struct XMQQuoteSettings XMQQuoteSettings;

void generate_state_error_message(XMQParseState *state, XMQParseError error_nr, const char *start, const char *stop);

// Text functions ////////////////

bool is_all_xml_whitespace(const char *start);
bool is_lowercase_hex(char c);
bool is_unicode_whitespace(const char *start, const char *stop);
size_t count_whitespace(const char *i, const char *stop);

// XMQ parser utility functions //////////////////////////////////

bool is_xml_whitespace(char c); // 0x9 0xa 0xd 0x20
bool is_xmq_token_whitespace(char c); // 0xa 0xd 0x20
bool is_xmq_attribute_key_start(char c);
bool is_xmq_comment_start(char c, char cc);
bool is_xmq_compound_start(char c);
bool is_xmq_doctype_start(const char *start, const char *stop);
bool is_xmq_pi_start(const char *start, const char *stop);
bool is_xmq_entity_start(char c);
bool is_xmq_quote_start(char c);
bool is_xmq_text_value(const char *i, const char *stop);
bool is_xmq_text_value_char(const char *i, const char *stop);
bool unsafe_value_start(char c, char cc);
bool is_safe_value_char(const char *i, const char *stop);

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk);
size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound);
size_t count_necessary_slashes(const char *start, const char *stop);


// Common parser functions ///////////////////////////////////////

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col);

Level enter_compound_level(Level l);
XMQColor level_to_quote_color(Level l);
XMQColor level_to_entity_color(Level l);
void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len);
void element_strlen_name_prefix(xmlNode *attr, const char **name, const char **prefix, size_t *total_u_len);
void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len);
size_t find_attr_key_max_u_width(xmlAttr *a);
size_t find_namespace_max_u_width(size_t max, xmlNs *ns);
size_t find_element_key_max_width(xmlNodePtr node, xmlNodePtr *restart_find_at_node);
bool is_hex(char c);
unsigned char hex_value(char c);
const char *find_word_ignore_case(const char *start, const char *stop, const char *word);

XMQParseState *xmqAllocateParseState(XMQParseCallbacks *callbacks);
void xmqFreeParseState(XMQParseState *state);
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);
bool xmqTokenizeFile(XMQParseState *state, const char *file);

void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_html_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_tex_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);

// XMQ tokenizer functions ///////////////////////////////////////////////////////////

void eat_xml_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_token_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_entity(XMQParseState *state);
void eat_xmq_comment_to_eol(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_comment_to_close(XMQParseState *state, const char **content_start, const char **content_stop, size_t n, bool *found_asterisk);
void eat_xmq_text_value(XMQParseState *state);
bool peek_xmq_next_is_equal(XMQParseState *state);
size_t count_xmq_quotes(const char *i, const char *stop);
void eat_xmq_quote(XMQParseState *state, const char **start, const char **stop);
char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop);
char *escape_xml_comment(const char *comment);
char *unescape_xml_comment(const char *comment);
void xmq_fixup_html_before_writeout(XMQDoc *doq);
void xmq_fixup_comments_after_readin(XMQDoc *doq);

char *xmq_comment(int indent,
                 const char *start,
                 const char *stop,
                 XMQQuoteSettings *settings);
char *xmq_un_comment(size_t indent, char space, const char *start, const char *stop);
char *xmq_un_quote(size_t indent, char space, const char *start, const char *stop, bool remove_qs);

// XMQ syntax parser functions ///////////////////////////////////////////////////////////

void parse_xmq(XMQParseState *state);
void parse_xmq_attribute(XMQParseState *state);
void parse_xmq_attributes(XMQParseState *state);
void parse_xmq_comment(XMQParseState *state, char cc);
void parse_xmq_compound(XMQParseState *state, Level level);
void parse_xmq_compound_children(XMQParseState *state, Level level);
void parse_xmq_element_internal(XMQParseState *state, bool doctype, bool pi);
void parse_xmq_element(XMQParseState *state);
void parse_xmq_doctype(XMQParseState *state);
void parse_xmq_pi(XMQParseState *state);
void parse_xmq_entity(XMQParseState *state, Level level);
void parse_xmq_quote(XMQParseState *state, Level level);
void parse_xmq_text_any(XMQParseState *state);
void parse_xmq_text_name(XMQParseState *state);
void parse_xmq_text_value(XMQParseState *state, Level level);
void parse_xmq_value(XMQParseState *state, Level level);
void parse_xmq_whitespace(XMQParseState *state);

// XML/HTML dom functions ///////////////////////////////////////////////////////////////

xmlDtdPtr parse_doctype_raw(XMQDoc *doq, const char *start, const char *stop);
char *xml_collapse_text(xmlNode *node);
xmlNode *xml_first_child(xmlNode *node);
xmlNode *xml_last_child(xmlNode *node);
xmlNode *xml_prev_sibling(xmlNode *node);
xmlNode *xml_next_sibling(xmlNode *node);
xmlAttr *xml_first_attribute(xmlNode *node);
xmlNs *xml_first_namespace_def(xmlNode *node);
xmlNs *xml_next_namespace_def(xmlNs *ns);
xmlAttr *xml_next_attribute(xmlAttr *attr);
const char*xml_element_name(xmlNode *node);
const char*xml_element_content(xmlNode *node);
const char *xml_element_ns_prefix(const xmlNode *node);
const char*xml_attr_key(xmlAttr *attr);
const char*xml_namespace_href(xmlNs *ns);
bool is_entity_node(const xmlNode *node);
bool is_content_node(const xmlNode *node);
bool is_doctype_node(const xmlNode *node);
bool is_comment_node(const xmlNode *node);
bool is_pi_node(const xmlNode *node);
bool is_leaf_node(xmlNode *node);
bool is_key_value_node(xmlNode *node);
void trim_node(xmlNode *node, int flags);
void trim_text_node(xmlNode *node, int flags);

// Output buffer functions ////////////////////////////////////////////////////////

const char *build_error_message(const char *fmt, ...);

void node_strlen_name_prefix(xmlNode *node, const char **name, size_t *name_len, const char **prefix, size_t *prefix_len, size_t *total_len);

bool need_separation_before_attribute_key(XMQPrintState *ps);
bool need_separation_before_element_name(XMQPrintState *ps);
bool need_separation_before_quote(XMQPrintState *ps);
bool need_separation_before_comment(XMQPrintState *ps);

void check_space_before_attribute(XMQPrintState *ps);
void check_space_before_comment(XMQPrintState *ps);
void check_space_before_entity_node(XMQPrintState *ps);
void check_space_before_key(XMQPrintState *ps);
void check_space_before_opening_brace(XMQPrintState *ps);
void check_space_before_closing_brace(XMQPrintState *ps);
void check_space_before_quote(XMQPrintState *ps, Level level);

bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop);

// Printing the DOM as xmq/htmq ///////////////////////////////////////////////////

size_t print_char_entity(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);
void print_color_post(XMQPrintState *ps, XMQColor c);
void print_color_pre(XMQPrintState *ps, XMQColor c);
void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix);
void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);
void print_quoted_spaces(XMQPrintState *ps, XMQColor c, int n);
void print_quotes(XMQPrintState *ps, size_t num, XMQColor c);
void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n);
void print_white_spaces(XMQPrintState *ps, int n);
void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level);

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align);
void print_node(XMQPrintState *ps, xmlNode *node, size_t align);
void print_entity_node(XMQPrintState *ps, xmlNode *node);
void print_content_node(XMQPrintState *ps, xmlNode *node);
void print_comment_node(XMQPrintState *ps, xmlNode *node);
void print_doctype(XMQPrintState *ps, xmlNode *node);
void print_key_node(XMQPrintState *ps, xmlNode *node, size_t align);
void print_leaf_node(XMQPrintState *ps, xmlNode *node);
void print_element_with_children(XMQPrintState *ps, xmlNode *node, size_t align);
size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node);
void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align);
void print_attributes(XMQPrintState *ps, xmlNodePtr node);
void print_value(XMQPrintState *ps, xmlNode *node, Level level);
void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level);
const char *needs_escape(XMQRenderFormat f, const char *start, const char *stop);
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop);
size_t print_utf8(XMQPrintState *ps, XMQColor c, size_t num_pairs, ...);
size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop);
void print_quote(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);

struct YaepGrammar;
typedef struct YaepGrammar YaepGrammar;
struct YaepParseRun;
typedef struct YaepParseRun YaepParseRun;
struct yaep_tree_node;

bool xmq_parse_buffer_ixml(XMQDoc *ixml_grammar, const char *start, const char *stop, int flags);

typedef void (*XMQContentCallback)(XMQParseState *state,
                                   size_t start_line,
                                   size_t start_col,
                                   const char *start,
                                   const char *stop,
                                   const char *suffix);

struct XMQParseCallbacks
{
#define X(TYPE) \
    XMQContentCallback handle_##TYPE;
LIST_OF_XMQ_TOKENS
#undef X

    void (*init)(XMQParseState*);
    void (*done)(XMQParseState*);

    int magic_cookie;
};

struct XMQPrintCallbacks
{
    void (*writeIndent)(int n);
    void (*writeElementName)(char *start, char *stop);
    void (*writeElementContent)(char *start, char *stop);
};

#define DO_CALLBACK(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); }

#define DO_CALLBACK_SIM(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) { state->simulated=true; state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); state->simulated=false; } }

bool debug_enabled();

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks);
void xmq_set_yaep_grammar(XMQDoc *doc, YaepGrammar *g);
YaepGrammar *xmq_get_yaep_grammar(XMQDoc *doc);
YaepParseRun *xmq_get_yaep_parse_run(XMQDoc *doc);
XMQParseState *xmq_get_yaep_parse_state(XMQDoc *doc);

void set_node_namespace(XMQParseState *state, xmlNodePtr node, const char *node_name);

// Multicolor terminals like gnome-term etc.

#define NOCOLOR      "\033[0m"

#define XMQ_INTERNALS_MODULE

#endif // XMQ_INTERNALS_H
