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

#include"xmq.h"
#include"parts/hashmap.h"
#include"parts/membuffer.h"
#include"parts/text.h"
#include"parts/ixml.h"
#include"parts/xml.h"
#include"parts/yaep.h"

#include<assert.h>
#include<ctype.h>
#include<memory.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#ifndef PLATFORM_WINAPI
#include<sys/wait.h>
#endif
#include<sys/stat.h>
#include<fcntl.h>

#ifdef PLATFORM_WINAPI
#include<windows.h>
#include<conio.h>
char *strndup(const char *s, size_t l);
#else
#include<signal.h>
#include<sys/ioctl.h>
#include<termios.h>
#endif

#define LIBXML_STATIC
#define LIBXSLT_STATIC
#define XMLSEC_STATIC

#include<libxml/tree.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>
#include<libxml/xmlschemas.h>
#include<libxml/parserInternals.h>
#include<libxslt/documents.h>
#include<libxslt/xslt.h>
#include<libxslt/xsltInternals.h>
#include<libxslt/transform.h>
#include<libxslt/xsltutils.h>

typedef enum
{
    XMQ_CLI_TOKENIZE_NONE = 0,
    XMQ_CLI_TOKENIZE_DEBUG_TOKENS = 1,
    XMQ_CLI_TOKENIZE_DEBUG_CONTENT = 2,
    XMQ_CLI_TOKENIZE_TERMINAL = 3,
    XMQ_CLI_TOKENIZE_HTML = 4,
    XMQ_CLI_TOKENIZE_TEX = 5,
    XMQ_CLI_TOKENIZE_LOCATION = 6
} XMQCliTokenizeType;

typedef enum
{
    XMQ_CLI_CMD_NONE,
    XMQ_CLI_CMD_HELP,
    XMQ_CLI_CMD_LOAD,
    XMQ_CLI_CMD_TO_XMQ,
    XMQ_CLI_CMD_TO_XML,
    XMQ_CLI_CMD_TO_HTMQ,
    XMQ_CLI_CMD_TO_HTML,
    XMQ_CLI_CMD_TO_JSON,
    XMQ_CLI_CMD_TO_TEXT,
    XMQ_CLI_CMD_TO_CLINES,
    XMQ_CLI_CMD_NO_OUTPUT,
    XMQ_CLI_CMD_PRINT,
    XMQ_CLI_CMD_SAVE_TO,
    XMQ_CLI_CMD_PAGER,
    XMQ_CLI_CMD_BROWSER,
    XMQ_CLI_CMD_RENDER_TERMINAL,
    XMQ_CLI_CMD_RENDER_HTML,
    XMQ_CLI_CMD_RENDER_TEX,
    XMQ_CLI_CMD_RENDER_RAW,
    XMQ_CLI_CMD_TOKENIZE,
    XMQ_CLI_CMD_DELETE,
    XMQ_CLI_CMD_DELETE_ENTITY,
    XMQ_CLI_CMD_REPLACE,
    XMQ_CLI_CMD_REPLACE_ENTITY,
    XMQ_CLI_CMD_SUBSTITUTE_ENTITY,
    XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES,
    XMQ_CLI_CMD_TRANSFORM,
    XMQ_CLI_CMD_VALIDATE,
    XMQ_CLI_CMD_SELECT,
    XMQ_CLI_CMD_FOR_EACH,
    XMQ_CLI_CMD_ADD,
    XMQ_CLI_CMD_ADD_ROOT,
    XMQ_CLI_CMD_STATISTICS,
    XMQ_CLI_CMD_QUOTE_C,
    XMQ_CLI_CMD_UNQUOTE_C
} XMQCliCmd;

typedef enum {
    XMQ_CLI_CMD_GROUP_NONE,
    XMQ_CLI_CMD_GROUP_LOAD,
    XMQ_CLI_CMD_GROUP_HELP,
    XMQ_CLI_CMD_GROUP_TO,
    XMQ_CLI_CMD_GROUP_RENDER,
    XMQ_CLI_CMD_GROUP_TOKENIZE,
    XMQ_CLI_CMD_GROUP_XPATH,
    XMQ_CLI_CMD_GROUP_ADD_XMQ,
    XMQ_CLI_CMD_GROUP_FOR_EACH,
    XMQ_CLI_CMD_GROUP_ENTITY,
    XMQ_CLI_CMD_GROUP_SUBSTITUTE,
    XMQ_CLI_CMD_GROUP_TRANSFORM,
    XMQ_CLI_CMD_GROUP_VALIDATE,
    XMQ_CLI_CMD_GROUP_OUTPUT,
    XMQ_CLI_CMD_GROUP_QUOTE,
} XMQCliCmdGroup;

typedef enum {
    XMQ_RENDER_MONO = 0,
    XMQ_RENDER_COLOR_DARKBG = 1,
    XMQ_RENDER_COLOR_LIGHTBG= 2
} XMQRenderStyle;

typedef struct XMQCliEnvironment XMQCliEnvironment;
struct XMQCliEnvironment;

typedef struct XMQCliCommand XMQCliCommand;
struct XMQCliCommand
{
    XMQCliEnvironment *env;
    XMQCliCmd cmd;
    bool silent;
    const char *in;
    bool in_is_content; // If set to true, then in is the actual content to be parsed.
    bool no_input; // If set to true, then no initial file is read nor any stdin is read.
    const char *out;
    const char *alias;
    XMQDoc *alias_doq; // The xmq document loaded to generate the xsd.
    const char *xpath;
    const char *entity;
    const char *content; // Content to replace something.
    const char *add_xmq; // XMQ content to add.
    XMQDoc *xslt_doq; // The xmq document loaded to generate the xslt.
    xsltStylesheetPtr xslt; // The xslt transform to be used.
    HashMap *xslt_params; // Parameters to the xslt transform.

    const char *xsd_name; // Name of xsd file.
    XMQDoc *xsd_doq; // The xmq document loaded to generate the xsd.
    xmlSchemaPtr xsd; // The xsd to validate agains.
    const char *save_file; // Save output to this file name.
    xmlDocPtr   ixml_doc;  // A DOM containing the ixml grammar.
    const char *ixml_ixml; // IXML grammar source to be used.
    const char *ixml_filename; // Where the ixml grammar was read from.
    bool ixml_all_parses; // Print all possible parses when parse is ambiguous.
    bool ixml_try_to_recover; // Try to recover when parsing with an ixml grammar.
    bool build_xml_of_ixml; // Generate xml directly from ixml.
    // xmq --ixml=grammar.ixml --xml-of-ixml # This will print the grammar as xmq.
    // xmq --ixml=ixml.ixml grammar.ixml # This will print the same grammar as xmq,
    // but uses the ixml early parser instead of the hand coded parser.
    bool log_xmq; // Output verbose,debug,trace as human readable lines instead of xmq.
    xmlDocPtr   node_doc;
    xmlNodePtr  node_content; // Tree content to replace something.
    XMQContentType in_format;
    XMQContentType out_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    const char *render_theme; // A named coloring.
    int  flags;
    bool use_color; // Uses color or not for terminal/html/tex
    bool bg_dark_mode; // Terminal has dark background.
    const char *use_id; // When rendering html mark the pre tag with this id.
    const char *use_class; // When rendering html mark the pre tag with this class.
    bool add_nl; // Add newline at end of quote/unquote printing.

    // Overrides of output settings.
    const char *indentation_space;
    const char *explicit_space;
    const char *explicit_tab;
    const char *explicit_cr;
    const char *explicit_nl;
    bool has_overrides;

    bool print_help;
    const char *help_command;
    bool print_version;
    bool print_license;
    bool debug;
    bool verbose;
    bool trace;
    int  add_indent;
    bool omit_decl; // If true, then do not print <?xml ..?>
    bool compact;
    bool escape_newlines;
    bool escape_non_7bit;
    bool escape_tabs;
    const char *implicit_root;
    bool lines;

    // Command tok
    XMQCliTokenizeType tok_type; // Do not pretty print, just debug/colorize tokens.
    XMQCliCommand *next; // Point to next command to be executed.
};

typedef struct XMQCliEnvironment XMQCliEnvironment;
struct XMQCliEnvironment
{
    XMQDoc *doc;
    XMQCliCommand *load;
    bool use_color;
    bool bg_dark_mode;
    const char *use_id;
    char *out_start; // Points to generated output: xml/xmq/htmq/html/json/text
    char *out_stop; // Points to byte after output, or NULL which means start is NULL terminated.
    size_t out_skip; // Skip some leading part of the generated output. Used to skip the <?xml ..?>
};

typedef enum {
    CHARACTER,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    BACKSPACE,
    ENTER
} KEY;

typedef struct
{
    size_t num_elements;
    size_t size_element_names;
    size_t num_text_nodes;
    size_t size_text_nodes;
    size_t num_entities;
    size_t size_entities;
    size_t num_char_entities;
    size_t size_char_entities;
    size_t num_tabs;
    size_t num_non_7bit_chars;
    size_t num_attributes;
    size_t size_attribute_names;
    size_t size_attribute_content;
    size_t num_comments;
    size_t size_comments;
    size_t num_pi_nodes;
    size_t size_pi_nodes;
    size_t has_doctype;
    size_t size_doctype;
    size_t num_cdata_nodes;
    size_t size_cdata_nodes;
} Stats;

// FUNCTION DECLARATIONS /////////////////////////////////////////////////////////////

void accumulate_attribute(Stats *stats, xmlAttr *a);
void accumulate_attribute_content(Stats *stats, xmlNode *a);
void accumulate_children(Stats *stats, xmlNode *node);
void accumulate_statistics(Stats *stats, xmlNode *node);
void append_text_node(MemBuffer *buf, xmlNode *node);
void append_text_children(MemBuffer *buf, xmlNode *n);
void count_statistics(Stats *stats, xmlDoc *doc);

char *grab_name(const char *s, const char **name_start);
char *make_shell_safe_name(char *name, char *name_start);
bool shell_safe(char *i);
char *grab_content(xmlNode *node, const char *name);
void add_key_value(xmlDoc *doc, xmlNode *root, const char *key, size_t value);
XMQCliCommand *allocate_cli_command(XMQCliEnvironment *env);
void free_cli_command(XMQCliCommand *cmd);
bool cmd_delete(XMQCliCommand *command);
bool cmd_help(XMQCliCommand *c);
bool cmd_select(XMQCliCommand *command);
bool cmd_for_each(XMQCliCommand *command);
bool cmd_add(XMQCliCommand *command);
bool cmd_add_root(XMQCliCommand *command);
bool cmd_statistics(XMQCliCommand *command);
bool cmd_replace(XMQCliCommand *command);
bool cmd_substitute(XMQCliCommand *command);
XMQCliCmd cmd_from(const char *s);
XMQCliCmdGroup cmd_group(XMQCliCmd cmd);
bool cmd_load(XMQCliCommand *command);
const char *cmd_name(XMQCliCmd cmd);
bool cmd_to(XMQCliCommand *command);
bool cmd_output(XMQCliCommand *command);
bool cmd_transform(XMQCliCommand *command);
bool cmd_validate(XMQCliCommand *command);
void cmd_unload(XMQCliCommand *command);
bool cmd_quote_unquote(XMQCliCommand *command);
const char *content_type_to_string(XMQContentType ct);
const char *tokenize_type_to_string(XMQCliTokenizeType type);
void debug_(const char* fmt, ...);
void trace_(const char* fmt, ...);
void delete_all_entities(XMQDoc *doq, xmlNode *node, const char *entity);
void delete_entities(XMQDoc *doq, const char *entity);
bool delete_entity(XMQCliCommand *command);
bool delete_xpath(XMQCliCommand *command);
void disable_stdout_raw_input_mode();
void enable_stdout_raw_input_mode();
void enableAnsiColorsWindowsConsole();
void enableRawStdinTerminal();
const char *skip_ansi_backwards(const char *i, const char *start);
size_t count_non_ansi_chars(const char *start, const char *stop);
void find_next_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height);
void find_prev_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height);
void find_next_line(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width);
void find_prev_line(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width);
bool has_log_xmq(int argc, const char **argv);
bool has_trace(int argc, const char **argv);
bool has_debug(int argc, const char **argv);
bool has_verbose(int argc, const char **argv);
bool handle_global_option(const char *arg, XMQCliCommand *command);
bool handle_option(const char *arg, const char *arg_next, XMQCliCommand *command);
void invoke_shell(xmlNode *n, const char *cmd);
void page(const char *start, const char *stop);
void browse(XMQCliCommand *c);
void open_browser(const char *file);
bool perform_command(XMQCliCommand *c);
void prepare_command(XMQCliCommand *c, XMQCliCommand *load_command);
void print_version_and_exit();
void print_license_and_exit();
int get_char();
void put_char(int c);
void console_write(const char *start, const char *stop);
void print_command_help(XMQCliCmd c);
KEY read_key(int *c);
const char *render_format_to_string(XMQRenderFormat rt);
xmlNodePtr replace_entity(xmlDoc *doc, xmlNodePtr node, const char *entity, const char *content, xmlNodePtr node_content);
void replace_entities(xmlDoc *doc, const char *entity, const char *content, xmlNodePtr node_content);
void substitute_entity(xmlDoc *doc, xmlNodePtr node, const char *entity, bool only_chars);
bool query_xterm_bgcolor();
XMQRenderStyle terminal_render_theme(bool *use_color, bool *bg_dark_mode);
void restoreStdinTerminal();
bool cmd_tokenize(XMQCliCommand *command);
void error_(const char* fmt, ...);
void verbose_(const char* fmt, ...);
bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *command);
xmlDocPtr xmqDocDefaultLoaderFunc(const xmlChar * URI, xmlDictPtr dict, int options,
                                  void *ctxt /* ATTRIBUTE_UNUSED */,
                                  xsltLoadType type /* ATTRIBUTE_UNUSED*/);

char *load_file_into_buffer(const char *file);
bool check_file_exists(const char *file);

// TODO REMOVE...
void xmq_set_yaep_grammar(XMQDoc *doc, YaepGrammar *g);
YaepGrammar *xmq_get_yaep_grammar(XMQDoc *doc);

/////////////////////////////////////////////////////////////////////////////////////

const char *error_to_print_on_exit = NULL;

XMQLineConfig xmq_log_line_config__;

bool log_xmq__ = false;

void error_(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, ap);
    fprintf(stderr, "%s\n", line);
    free(line);
    va_end(ap);
}

bool verbose_enabled__ = false;

void verbose_(const char* fmt, ...)
{
    if (verbose_enabled__)
    {
        va_list ap;
        va_start(ap, fmt);
        char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, ap);
        fprintf(stderr, "%s\n", line);
        free(line);
        va_end(ap);
    }
}

bool debug_enabled__ = false;

void debug_(const char* fmt, ...)
{
    if (debug_enabled__)
    {
        va_list args;
        va_start(args, fmt);
        char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
        fprintf(stderr, "%s\n", line);
        free(line);
        va_end(args);
    }
}

bool trace_enabled__ = false;

void trace_(const char* fmt, ...)
{
    if (trace_enabled__)
    {
        va_list args;
        va_start(args, fmt);
        char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
        fprintf(stderr, "%s\n", line);
        free(line);
        va_end(args);
    }
}

bool logxmq_enabled__ = false;

XMQCliCmd cmd_from(const char *s)
{
    if (!s) return XMQ_CLI_CMD_NONE;
    if (!strcmp(s, "add")) return XMQ_CLI_CMD_ADD;
    if (!strcmp(s, "add-root")) return XMQ_CLI_CMD_ADD_ROOT;
    if (!strcmp(s, "br")) return XMQ_CLI_CMD_BROWSER;
    if (!strcmp(s, "browse")) return XMQ_CLI_CMD_BROWSER;
    if (!strcmp(s, "delete")) return XMQ_CLI_CMD_DELETE;
    if (!strcmp(s, "delete-entity")) return XMQ_CLI_CMD_DELETE_ENTITY;
    if (!strcmp(s, "for-each")) return XMQ_CLI_CMD_FOR_EACH;
    if (!strcmp(s, "help")) return XMQ_CLI_CMD_HELP;
    if (!strcmp(s, "load")) return XMQ_CLI_CMD_LOAD;
    if (!strcmp(s, "no-output")) return XMQ_CLI_CMD_NO_OUTPUT;
    if (!strcmp(s, "pa")) return XMQ_CLI_CMD_PAGER;
    if (!strcmp(s, "page")) return XMQ_CLI_CMD_PAGER;
    if (!strcmp(s, "print")) return XMQ_CLI_CMD_PRINT;
    if (!strcmp(s, "render-html")) return XMQ_CLI_CMD_RENDER_HTML;
    if (!strcmp(s, "render-raw")) return XMQ_CLI_CMD_RENDER_RAW;
    if (!strcmp(s, "render-terminal")) return XMQ_CLI_CMD_RENDER_TERMINAL;
    if (!strcmp(s, "render-tex")) return XMQ_CLI_CMD_RENDER_TEX;
    if (!strcmp(s, "replace")) return XMQ_CLI_CMD_REPLACE;
    if (!strcmp(s, "replace-entity")) return XMQ_CLI_CMD_REPLACE_ENTITY;
    if (!strcmp(s, "save-to")) return XMQ_CLI_CMD_SAVE_TO;
    if (!strcmp(s, "select")) return XMQ_CLI_CMD_SELECT;
    if (!strcmp(s, "statistics")) return XMQ_CLI_CMD_STATISTICS;
    if (!strcmp(s, "substitute-char-entities")) return XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES;
    if (!strcmp(s, "substitute-entity")) return XMQ_CLI_CMD_SUBSTITUTE_ENTITY;
    if (!strcmp(s, "to-html")) return XMQ_CLI_CMD_TO_HTML;
    if (!strcmp(s, "to-htmq")) return XMQ_CLI_CMD_TO_HTMQ;
    if (!strcmp(s, "to-json")) return XMQ_CLI_CMD_TO_JSON;
    if (!strcmp(s, "to-text")) return XMQ_CLI_CMD_TO_TEXT;
    if (!strcmp(s, "to-xml")) return XMQ_CLI_CMD_TO_XML;
    if (!strcmp(s, "to-xmq")) return XMQ_CLI_CMD_TO_XMQ;
    if (!strcmp(s, "to-clines")) return XMQ_CLI_CMD_TO_CLINES;
    if (!strcmp(s, "tokenize")) return XMQ_CLI_CMD_TOKENIZE;
    if (!strcmp(s, "transform")) return XMQ_CLI_CMD_TRANSFORM;
    if (!strcmp(s, "validate")) return XMQ_CLI_CMD_VALIDATE;
    if (!strcmp(s, "quote-c")) return XMQ_CLI_CMD_QUOTE_C;
    if (!strcmp(s, "unquote-c")) return XMQ_CLI_CMD_UNQUOTE_C;

    return XMQ_CLI_CMD_NONE;
}

const char *cmd_name(XMQCliCmd cmd)
{
    switch (cmd)
    {
    case XMQ_CLI_CMD_NONE: return "noop";
    case XMQ_CLI_CMD_HELP: return "help";
    case XMQ_CLI_CMD_LOAD: return "load";
    case XMQ_CLI_CMD_TO_XMQ: return "to-xmq";
    case XMQ_CLI_CMD_TO_XML: return "to-xml";
    case XMQ_CLI_CMD_TO_HTMQ: return "to-htmq";
    case XMQ_CLI_CMD_TO_HTML: return "to-html";
    case XMQ_CLI_CMD_TO_JSON: return "to-json";
    case XMQ_CLI_CMD_TO_TEXT: return "to-text";
    case XMQ_CLI_CMD_TO_CLINES: return "to-clines";
    case XMQ_CLI_CMD_NO_OUTPUT: return "no-output";
    case XMQ_CLI_CMD_PRINT: return "print";
    case XMQ_CLI_CMD_SAVE_TO: return "save-to";
    case XMQ_CLI_CMD_PAGER: return "pager";
    case XMQ_CLI_CMD_BROWSER: return "browser";
    case XMQ_CLI_CMD_RENDER_TERMINAL: return "render-terminal";
    case XMQ_CLI_CMD_RENDER_HTML: return "render-html";
    case XMQ_CLI_CMD_RENDER_TEX: return "render-tex";
    case XMQ_CLI_CMD_RENDER_RAW: return "render-raw";
    case XMQ_CLI_CMD_TOKENIZE: return "tokenize";
    case XMQ_CLI_CMD_DELETE: return "delete";
    case XMQ_CLI_CMD_DELETE_ENTITY: return "delete-entity";
    case XMQ_CLI_CMD_REPLACE: return "replace";
    case XMQ_CLI_CMD_REPLACE_ENTITY: return "replace-entity";
    case XMQ_CLI_CMD_SUBSTITUTE_ENTITY: return "substitute-entity";
    case XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES: return "substitute-char-entities";
    case XMQ_CLI_CMD_TRANSFORM: return "transform";
    case XMQ_CLI_CMD_VALIDATE: return "validate";
    case XMQ_CLI_CMD_SELECT: return "select";
    case XMQ_CLI_CMD_FOR_EACH: return "for-each";
    case XMQ_CLI_CMD_ADD: return "add";
    case XMQ_CLI_CMD_ADD_ROOT: return "add-root";
    case XMQ_CLI_CMD_STATISTICS: return "statistics";
    case XMQ_CLI_CMD_QUOTE_C: return "quote-c";
    case XMQ_CLI_CMD_UNQUOTE_C: return "unquote-c";
    }
    return "?";
}

XMQCliCmdGroup cmd_group(XMQCliCmd cmd)
{
    switch (cmd)
    {
    case XMQ_CLI_CMD_LOAD:
        return XMQ_CLI_CMD_GROUP_LOAD;

    case XMQ_CLI_CMD_TO_XMQ:
    case XMQ_CLI_CMD_TO_XML:
    case XMQ_CLI_CMD_TO_HTMQ:
    case XMQ_CLI_CMD_TO_HTML:
    case XMQ_CLI_CMD_TO_JSON:
    case XMQ_CLI_CMD_TO_TEXT:
    case XMQ_CLI_CMD_TO_CLINES:
        return XMQ_CLI_CMD_GROUP_TO;

    case XMQ_CLI_CMD_NO_OUTPUT:
    case XMQ_CLI_CMD_PRINT:
    case XMQ_CLI_CMD_SAVE_TO:
    case XMQ_CLI_CMD_PAGER:
    case XMQ_CLI_CMD_BROWSER:
        return XMQ_CLI_CMD_GROUP_OUTPUT;

    case XMQ_CLI_CMD_RENDER_TERMINAL:
    case XMQ_CLI_CMD_RENDER_HTML:
    case XMQ_CLI_CMD_RENDER_TEX:
    case XMQ_CLI_CMD_RENDER_RAW:
        return XMQ_CLI_CMD_GROUP_RENDER;

    case XMQ_CLI_CMD_TOKENIZE:
        return XMQ_CLI_CMD_GROUP_TOKENIZE;

    case XMQ_CLI_CMD_DELETE:
    case XMQ_CLI_CMD_REPLACE:
    case XMQ_CLI_CMD_SELECT:
    case XMQ_CLI_CMD_ADD_ROOT:
        return XMQ_CLI_CMD_GROUP_XPATH;

    case XMQ_CLI_CMD_ADD:
        return XMQ_CLI_CMD_GROUP_ADD_XMQ;

    case XMQ_CLI_CMD_FOR_EACH:
        return XMQ_CLI_CMD_GROUP_FOR_EACH;

    case XMQ_CLI_CMD_DELETE_ENTITY:
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return XMQ_CLI_CMD_GROUP_ENTITY;

    case XMQ_CLI_CMD_SUBSTITUTE_ENTITY:
    case XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES:
        return XMQ_CLI_CMD_GROUP_SUBSTITUTE;

    case XMQ_CLI_CMD_TRANSFORM:
        return XMQ_CLI_CMD_GROUP_TRANSFORM;

    case XMQ_CLI_CMD_QUOTE_C:
    case XMQ_CLI_CMD_UNQUOTE_C:
        return XMQ_CLI_CMD_GROUP_QUOTE;

    case XMQ_CLI_CMD_VALIDATE:
        return XMQ_CLI_CMD_GROUP_VALIDATE;

    case XMQ_CLI_CMD_NONE:
    case XMQ_CLI_CMD_STATISTICS:
        return XMQ_CLI_CMD_GROUP_NONE;
    case XMQ_CLI_CMD_HELP:
        return XMQ_CLI_CMD_GROUP_HELP;
    }
    return XMQ_CLI_CMD_GROUP_NONE;
}

const char *content_type_to_string(XMQContentType ct)
{
    switch(ct)
    {
    case XMQ_CONTENT_UNKNOWN: return "unknown";
    case XMQ_CONTENT_DETECT: return "detect";
    case XMQ_CONTENT_XMQ: return "xmq";
    case XMQ_CONTENT_HTMQ: return "htmq";
    case XMQ_CONTENT_XML: return "xml";
    case XMQ_CONTENT_HTML: return "html";
    case XMQ_CONTENT_JSON: return "json";
    case XMQ_CONTENT_IXML: return "ixml";
    case XMQ_CONTENT_TEXT: return "text";
    case XMQ_CONTENT_CLINES: return "clines";
    }
    assert(false);
    return "?";
}

const char *tokenize_type_to_string(XMQCliTokenizeType type)
{
    switch (type)
    {
    case XMQ_CLI_TOKENIZE_NONE: return "none";
    case XMQ_CLI_TOKENIZE_DEBUG_TOKENS: return "debugtokens";
    case XMQ_CLI_TOKENIZE_DEBUG_CONTENT: return "debugcontent";
    case XMQ_CLI_TOKENIZE_TERMINAL: return "terminal;";
    case XMQ_CLI_TOKENIZE_HTML: return "html";
    case XMQ_CLI_TOKENIZE_TEX: return "tex";
    case XMQ_CLI_TOKENIZE_LOCATION: return "location";
    }
    return "?";
}

XMQCliCommand *allocate_cli_command(XMQCliEnvironment *env)
{
    XMQCliCommand *c = (XMQCliCommand*)malloc(sizeof(XMQCliCommand));
    memset(c, 0, sizeof(*c));

    c->use_color = env->use_color;
    c->bg_dark_mode = env->bg_dark_mode;
    c->env = env;
    c->cmd = XMQ_CLI_CMD_TO_XMQ;
    c->in_format = XMQ_CONTENT_DETECT;
    c->out_format = XMQ_CONTENT_XMQ;
    if (c->use_color)
    {
        c->render_to = XMQ_RENDER_TERMINAL;
    }
    else
    {
        c->render_to = XMQ_RENDER_PLAIN;
    }
    c->add_indent = 4;
    c->compact = false;
    c->escape_tabs = false;

    return c;
}

void free_cli_command(XMQCliCommand *cmd)
{
    if (cmd->xslt_params)
    {
        hashmap_free_and_values(cmd->xslt_params, free);
        cmd->xslt_params = NULL;
    }
    free(cmd);
}

void abortParsing(void *ctx, const char *msg, ...);

void abortParsing(void *ctx, const char *fmt, ...)
{
    fprintf(stderr, "xmq: %s parse error\n", (const char*)ctx);

    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);

    puts(buf);
    exit(1);
}

void warnParsing(void *ctx, const char *msg, ...);

void warnParsing(void *ctx, const char *fmt, ...)
{
    fprintf(stderr, "xmq: %s parse warning\n", (const char*)ctx);

    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);

    puts(buf);
}

void abortValidating(void *ctx, const char *msg, ...);

void abortValidating(void *ctx, const char *fmt, ...)
{
    printf("xmq: Document cannot be validated against %s\n", (const char*)ctx);

    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);

    puts(buf);
    exit(1);
}

void warnValidation(void *ctx, const char *msg, ...);

void warnValidation(void *ctx, const char *fmt, ...)
{
    printf("xmq: validation warning\n");

    char buf[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);

    puts(buf);
}

void abortSilentValidating(void *ctx, const char *msg, ...);

void abortSilentValidating(void *ctx, const char *fmt, ...)
{
    exit(1);
}

void warnSilentValidation(void *ctx, const char *msg, ...);

void warnSilentValidation(void *ctx, const char *fmt, ...)
{
}

bool handle_option(const char *arg, const char *arg_next, XMQCliCommand *command)
{
    if (!arg) return false;

    XMQCliCmdGroup group = cmd_group(command->cmd);

    if (command->cmd == XMQ_CLI_CMD_SAVE_TO)
    {
        if (command->save_file == NULL)
        {
            command->save_file = arg;
            return true;
        }
    }

    if (command->cmd == XMQ_CLI_CMD_HELP)
    {
        command->help_command = arg;
        return true;
    }

    if (group == XMQ_CLI_CMD_GROUP_TO ||
        group == XMQ_CLI_CMD_GROUP_RENDER)
    {
        if (!strcmp(arg, "--compact"))
        {
            command->add_indent = 0;
            command->compact = true;
            return true;
        }
    }

    if (command->cmd == XMQ_CLI_CMD_TO_XML)
    {
        if (!strcmp(arg, "-o") ||
            !strcmp(arg, "--omit-decl"))
        {
            command->omit_decl = true;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_QUOTE)
    {
        if (!strcmp(arg, "--add-nl"))
        {
            command->add_nl = true;
            return true;
        }
    }

    if (command->cmd == XMQ_CLI_CMD_TO_XMQ ||
        command->cmd == XMQ_CLI_CMD_TO_HTMQ ||
        group == XMQ_CLI_CMD_GROUP_RENDER)
    {
        if (!strcmp(arg, "--escape-newlines"))
        {
            command->escape_newlines = true;
            return true;
        }
        if (!strcmp(arg, "--escape-non-7bit"))
        {
            command->escape_non_7bit = true;
            return true;
        }
        if (!strcmp(arg, "--escape-tabs"))
        {
            command->escape_tabs = true;
            return true;
        }
        if (!strncmp(arg, "--indent=", 9))
        {
            for (const char *i = arg+9; *i; i++)
            {
                if (!isdigit(*i))
                {
                    printf("xmq: indent must be a positive integer\n");
                    exit(1);
                }
            }
            command->add_indent = atoi(arg+9);
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_RENDER)
    {
        if (!strncmp(arg, "--class=", 8))
        {
            command->use_class = arg+8;
            return true;
        }
        if (!strcmp(arg, "--color"))
        {
            command->use_color = true;
            if (command->render_to == XMQ_RENDER_PLAIN)
            {
                command->render_to = XMQ_RENDER_TERMINAL;
            }
            return true;
        }
        if (!strncmp(arg, "--id=", 5))
        {
            command->use_id = arg+5;
            return true;
        }
        if (!strcmp(arg, "--mono"))
        {
            command->use_color = false;
            return true;
        }
        if (!strcmp(arg, "--nostyle"))
        {
            command->render_raw = true;
            return true;
        }
        if (!strcmp(arg, "--onlystyle"))
        {
            command->only_style = true;
            return true;
        }
        if (!strncmp(arg, "--theme=", 8))
        {
            command->render_theme = arg+8;
            return true;
        }
        if (!strncmp(arg, "--use-cr=", 9))
        {
            command->explicit_cr = arg+9;
            command->has_overrides = true;
            return true;
        }
        if (!strncmp(arg, "--use-es=", 9))
        {
            command->explicit_space = arg+9;
            command->has_overrides = true;
            return true;
        }
        if (!strncmp(arg, "--use-et=", 9))
        {
            command->explicit_tab = arg+9;
            command->has_overrides = true;
            return true;
        }
        if (!strncmp(arg, "--use-is=", 9))
        {
            command->indentation_space = arg+9;
            command->has_overrides = true;
            return true;
        }
        if (!strncmp(arg, "--use-nl=", 9))
        {
            command->explicit_nl = arg+9;
            command->has_overrides = true;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_TOKENIZE)
    {
        if (!strncmp(arg, "--type=", 7))
        {
            if (!strcmp(arg, "--type=debugtokens"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_DEBUG_TOKENS;
                return true;
            }
            else if (!strcmp(arg, "--type=debugcontent"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_DEBUG_CONTENT;
                return true;
            }
            else if (!strcmp(arg, "--type=terminal"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_TERMINAL;
                command->use_color = true;
                return true;
            }
            else if (!strcmp(arg, "--type=html"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_HTML;
                return true;
            }
            else if (!strcmp(arg, "--type=tex"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_TEX;
                return true;
            }
            else if (!strcmp(arg, "--type=location"))
            {
                command->tok_type = XMQ_CLI_TOKENIZE_LOCATION;
                return true;
            }
            else
            {
                fprintf(stderr, "No such tokenize %s\n", arg);
                exit(1);
            }
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_XPATH)
    {
        if (command->xpath == NULL)
        {
            command->xpath = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_ADD_XMQ)
    {
        if (command->add_xmq == NULL)
        {
            command->add_xmq = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_FOR_EACH)
    {
        if (command->xpath == NULL)
        {
            command->xpath = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_SUBSTITUTE)
    {
        if (cmd_from(arg) == XMQ_CLI_CMD_NONE && command->entity == NULL)
        {
            command->entity = arg;
            return true;
        }
        return false;
    }

    if (group == XMQ_CLI_CMD_GROUP_FOR_EACH)
    {
        if (!strncmp(arg, "--shell=", 8))
        {
            command->content = arg+8;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_ENTITY)
    {
        if (!strncmp(arg, "--with-file=", 12))
        {
            const char *file = arg+12;
            XMQCliEnvironment env;
            memset(&env, 0, sizeof(env));
            XMQCliCommand cmd;
            memset(&cmd, 0, sizeof(XMQCliCommand));
            cmd.env = &env;
            cmd.in = file;
            cmd.in_format = XMQ_CONTENT_DETECT;
            cmd.flags = XMQ_FLAG_TRIM_NONE;
            size_t len = strlen(file);
            if (len > 4 && !(strncmp(file+len-4, ".xml", 4)))
            {
                cmd.in_format = XMQ_CONTENT_XML;
            }
            if (len > 5 && !(strncmp(file+len-5, ".html", 5)))
            {
                cmd.in_format = XMQ_CONTENT_HTML;
            }
            bool ok = cmd_load(&cmd);
            if (!ok) return false;
            xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(cmd.env->doc);
            command->node_doc = doc;
            command->node_content = doc->children;
            return true;
        }

        if (!strncmp(arg, "--with-text-file=", 17))
        {
            const char *file = arg+17;
            verbose_("xmq=", "reading text file %s", file);
            FILE *f = fopen(file, "rb");
            if (!f)
            {
                error_("xmq.err=", "%s: No such file or directory", file);
                return false;
            }
            fseek(f, 0L, SEEK_END);
            size_t sz = ftell(f);
            fseek(f, 0L, SEEK_SET);
            char *buf = (char*)malloc(sz+1);
            size_t n = fread(buf, 1, sz, f);
            buf[sz] = 0;
            if (n != sz) printf("ARRRRRGGGG\n");
            fclose(f);
            command->content = buf;
            return true;
        }

        if (command->entity == NULL)
        {
            command->entity = arg;
            return true;
        }

        if (group == XMQ_CLI_CMD_GROUP_ENTITY && command->content == NULL && command->node_content == NULL)
        {
            command->content = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_TRANSFORM)
    {
        if (!strncmp(arg, "--stringparam=", 14) ||
            !strncmp(arg, "--param=", 8))
        {
            size_t offset = (arg[2] == 's')?14:8;

            if (!command->xslt_params)
            {
                command->xslt_params = hashmap_create(64);
            }

            // Start of key in key=value
            const char *s = arg+offset;
            // Find start of value
            const char *p = strchr(s, '=');
            if (p == NULL)
            {
                fprintf(stderr, "Invalid: %s\nUsage: --stringparam=key=value\n", arg);
                return false;
            }
            char *key = strndup(s, p-s);
            char *val = NULL;
            if (offset == 8)
            {
                val = strdup(p+1);
            }
            else
            {
                char *sq = strchr(p+1, '\'');
                char *dq = strchr(p+1, '"');
                if (sq && dq)
                {
                    fprintf(stderr, "Invalid: %s\nA string param cannot contain both single and double quotes.\n", arg);
                    return false;
                }
                char q = sq?'"':'\'';
                size_t len = strlen(p+1);
                val = malloc(1+2+len);
                val[0] = q;
                val[1+len] = q;
                val[2+len] = 0;
                memcpy(val+1, p+1, len);
            }
            hashmap_put(command->xslt_params, key, val);
            free(key);
            return true;
        }
        if (command->xslt == NULL)
        {
            XMQDoc *doq = xmqNewDoc();

            // We load the xslt transform with TRIM_NONE! Why?
            // XSLT transform are terribly picky with whitespace and the xslt compiler will also trim whitespace
            // so to make a normal xslt transform work here we do not use the whitespace trimming heuristic.
            // There might be other problems here when loading normal old xslt transforms, we will get to them when we
            // find them.
            //
            // When loading an xslq transform the default is TRIM_NONE anyway since it is xmq.
            bool ok = xmqParseFileWithType(doq, arg, NULL, XMQ_CONTENT_DETECT, XMQ_FLAG_TRIM_NONE);

            if (!ok)
            {
                const char *error = xmqDocError(doq);
                fprintf(stderr, error, command->in);
                xmqFreeDoc(doq);
                return false;
            }

            verbose_("xmq=", "loaded xslt %s", arg);

            xmlDocPtr xslt = (xmlDocPtr)xmqGetImplementationDoc(doq);
            if (xmqGetOriginalContentType(doq) == XMQ_CONTENT_XMQ ||
                xmqGetOriginalContentType(doq) == XMQ_CONTENT_HTMQ)
            {
                // We want to be able to use char entities in the xslq by default.
                // So we replace any explicit &#10; with actual newlines etc.
                xmlNodePtr root = xmlDocGetRootElement(xslt);
                while (root->prev) root = root->prev;
                // Replace all char entities! I.e. &#10; is replaced with a newline.
                substitute_entity(xslt, root, NULL, true);
            }
            command->xslt_doq = doq;
            command->xslt = xsltParseStylesheetDoc(xslt);
            if (!command->xslt) return false;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_LOAD)
    {
        if (!strncmp(arg, "--alias=", 8))
        {
            command->alias = arg+8;
            return true;
        }
        if (command->in == NULL)
        {
            command->in = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_VALIDATE)
    {
        if (!strncmp(arg, "--silent", 8))
        {
            command->silent = true;
            return true;
        }
        if (command->xsd == NULL)
        {
            XMQDoc *doq = xmqNewDoc();
            bool ok = xmqParseFileWithType(doq, arg, NULL, XMQ_CONTENT_DETECT, XMQ_FLAG_TRIM_NONE);

            if (!ok)
            {
                const char *error = xmqDocError(doq);
                fprintf(stderr, error, command->in);
                xmqFreeDoc(doq);
                return false;
            }

            command->xsd_name = arg;
            command->xsd_doq = doq;

            verbose_("xmq=", "loaded xsd %zu bytes from %s", xmqGetOriginalSize(doq), arg);

            xmlDocPtr xsd = (xmlDocPtr)xmqGetImplementationDoc(doq);

            xmlSchemaParserCtxtPtr ctxt = xmlSchemaNewDocParserCtxt(xsd);
            xmlSchemaSetParserErrors(ctxt,
                                     abortParsing,
                                     warnParsing,
                                     (void*)arg);

            command->xsd = xmlSchemaParse(ctxt);

            xmlSchemaFreeParserCtxt(ctxt);
            if (!command->xsd) return true;

            /*fprintf(stderr, "-------------------------\n");
            xmlSchemaDump(stdout, command->xsd);
            fprintf(stderr, "-------------------------\n");*/
            return true;
        }
    }

    return false;
}

#ifndef PLATFORM_WINAPI
struct termios orig_stdout_termios;
#endif

void disable_stdout_raw_input_mode()
{
#ifndef PLATFORM_WINAPI
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &orig_stdout_termios);
#endif
}

void enable_stdout_raw_input_mode()
{
#ifndef PLATFORM_WINAPI
    tcgetattr(STDOUT_FILENO, &orig_stdout_termios);
    atexit(disable_stdout_raw_input_mode);

    struct termios raw;
    tcgetattr(STDOUT_FILENO, &raw);
    raw.c_lflag &= ~ECHO;
    raw.c_lflag &= ~ICANON;
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw);
#endif
}

const char *render_format_to_string(XMQRenderFormat rf)
{
    switch(rf)
    {
    case XMQ_RENDER_PLAIN: return "plain";
    case XMQ_RENDER_TERMINAL: return "terminal";
    case XMQ_RENDER_HTML: return "html";
    case XMQ_RENDER_HTMQ: return "htmq";
    case XMQ_RENDER_TEX: return "tex";
    case XMQ_RENDER_RAW: return "raw";
    }
    assert(false);
    return "?";
}

#ifndef PLATFORM_WINAPI

static void sig_alarm_handler(int signo)
{
    fprintf(stderr,
            "xmq: no response from xterm/xterm-256color terminal whether background is dark/light.\n"
            "To skip this failed test please set environment variable XMQ_THEME=mono|darkbg|lightbg.\n");
    exit(1);
}

struct sigaction old_alarm_;
struct sigaction new_alarm_;

bool query_xterm_bgcolor()
{
    bool is_dark = true;

    // If we use xmq to output to a tty which is xterm-256color),
    // then we can query the tty for its background color.
    // This is done sending an ansi sequence to stdout and then
    // read the tty respons. Normally the stdin is connected to the tty
    // and we can read from the stdin filedescriptor.

    // However if we use xmq in a pipe, then stdin is not the tty, its the pipe.
    // So how do we get the response? We can actually read from stdout!
    // The tty behaves like a file, it supports reads and writes, it
    // seems like the same underlying file is in both the stdin and stdout slot.
    // Thank you Johan Walles https://github.com/walles/ for this insight!

    // To make things simpler here, we always read from stdout.
    // Now lets put the tty in no-echo raw input mode, using the stdout filedescriptor.
    enable_stdout_raw_input_mode();

    int r = 0;
    int g = 0;
    int b = 0;

    new_alarm_.sa_handler = sig_alarm_handler;
    sigemptyset(&new_alarm_.sa_mask);
    new_alarm_.sa_flags = 0;

    sigaction(SIGALRM, &new_alarm_, &old_alarm_);

    // We wait at most 2 seconds for the response before giving up.
    alarm(1);

    // Send the ansi query sequence.
    printf("\x1b]11;?\x07");
    fflush(stdout);

    // Expected response:
    // \033]11;rgb:ffff/ffff/dddd\07
    char buf[64];
    memset(buf, 0, sizeof(buf));
    int i = 0;
    for (;;)
    {
        char c = 0;
        // Reading from stdout!
        size_t n = read(STDOUT_FILENO, &c, sizeof(char));
        if (n == -1)
        {
            fprintf(stderr, "xmq: error reading from stdout\n");
            break;
        }
        if (c == 0x07) break;
        buf[i++] = c;
        if (i >= 24) break;
    }
    buf[i] = 0;
    if (buf[0] == 0x1b && i >= 23 && 3 == sscanf(buf+1, "]11;rgb:%04x/%04x/%04x", &r, &g, &b))
    {
        double brightness = (0.2126*r + 0.7152*g + 0.0722*b)/256.0;
        if (brightness > 153) {
            is_dark = false;
        }
    }
    else
    {
        verbose_("xmq=", "bad response from terminal, assuming dark background.");
        is_dark = true;
    }

    alarm(0);
    sigaction(SIGALRM, &old_alarm_, NULL);

    disable_stdout_raw_input_mode();

    return is_dark;
}

#endif

XMQRenderStyle terminal_render_theme(bool *use_color, bool *bg_dark_mode)
{
    const char *term = getenv("TERM");
    if (!term) term = "NULL";
    verbose_("xmq=", "detected terminal %s", term);

    char *xmq_mode = getenv("XMQ_THEME");
    if (xmq_mode != NULL)
    {
        if (!strcmp(xmq_mode, "mono"))
        {
            *use_color = false;
            *bg_dark_mode = false;
            verbose_("xmq=", "XMQ_THEME set to mono");
            return XMQ_RENDER_MONO;
        }
        if (!strcmp(xmq_mode, "lightbg"))
        {
            *use_color = true;
            *bg_dark_mode = false;
            verbose_("xmq=", "XMQ_THEME set to lightbg");
            return XMQ_RENDER_COLOR_LIGHTBG;
        }
        if (!strcmp(xmq_mode, "darkbg"))
        {
            *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "XMQ_THEME set to darkbg");
            return XMQ_RENDER_COLOR_DARKBG;
        }
        *use_color = false;
        *bg_dark_mode = false;
        verbose_("xmq=", "XMQ_THEME content is bad, using mono");
        return XMQ_RENDER_MONO;
    }

    if (!strcmp(term, "linux"))
    {
        // The Linux vt console is by default black. So dark-mode.
        *use_color = true;
        *bg_dark_mode = true;
        verbose_("xmq=", "assuming vt console has dark bg");
        return XMQ_RENDER_COLOR_DARKBG;
    }

#ifdef PLATFORM_WINAPI
    *use_color = true;
    *bg_dark_mode = true;
    verbose_("xmq=", "assuming console has dark background");
    return XMQ_RENDER_COLOR_DARKBG;
#else

    if (!strcmp(term, "xterm-256color") ||
        !strcmp(term, "xterm"))
    {
        if (!isatty(1))
        {
            *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "cannot query xterm assuming console has dark background");
            return XMQ_RENDER_COLOR_DARKBG;
        }

        bool is_dark = true;
        is_dark = query_xterm_bgcolor();

        if (is_dark)
        {
            *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "terminal responds with dark background");
            return XMQ_RENDER_COLOR_DARKBG;
        }
        *use_color = true;
        *bg_dark_mode = false;
        verbose_("xmq=", "terminal responds with light background");
        return XMQ_RENDER_COLOR_LIGHTBG;
    }

#endif

    char *colorfgbg = getenv("COLORFGBG");
    if (colorfgbg != NULL)
    {
        // Black text on white background: 0;default;15
        // Or 0;15
        // White text on black background: 15;default;0
        // Or 15;0 or 7;0 etc
        const char *p = strrchr(colorfgbg, ';');
        // If the string ends with ;0 then it is dark mode.
	if (p == NULL || (*(p+2) == 0 && *(p+1) == '0'))
	{
	    *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "COLORFGBG means dark \"%s\"", colorfgbg);
            return XMQ_RENDER_COLOR_DARKBG;
	}
	*use_color = true;
        *bg_dark_mode = false;
        verbose_("xmq=", "COLORFGBG means light \"%s\"", colorfgbg);
        return XMQ_RENDER_COLOR_LIGHTBG;
    }

    *use_color = true;
    *bg_dark_mode = true;
    verbose_("xmq=", "unknown terminal \"%s\" defaults to colors and dark background", term);
    return XMQ_RENDER_COLOR_DARKBG;
}

bool handle_global_option(const char *arg, XMQCliCommand *command)
{
    debug_("xmq=", "option %s", arg);
    if (!strcmp(arg, "--help") ||
        !strcmp(arg, "-h"))
    {
        command->print_help = true;
        return true;
    }
    if (!strcmp(arg, "-z"))
    {
        command->no_input = true;
        return true;
    }
    if (!strcmp(arg, "-i"))
    {
        command->in_is_content = true;
        return true;
    }
    if (!strcmp(arg, "--xml-of-ixml"))
    {
        command->build_xml_of_ixml = true;
        return true;
    }
    if (!strcmp(arg, "--trace"))
    {
        command->trace = true;
        trace_enabled__ = true;
        debug_enabled__ = true;
        verbose_enabled__ = true;
        return true;
    }
    if (!strcmp(arg, "--debug"))
    {
        command->debug = true;
        debug_enabled__ = true;
        verbose_enabled__ = true;
        return true;
    }
    if (!strcmp(arg, "--verbose"))
    {
        command->verbose = true;
        verbose_enabled__ = true;
        return true;
    }
    if (!strcmp(arg, "--license"))
    {
        command->print_license = true;
        return true;
    }
    if (!strcmp(arg, "--version"))
    {
        command->print_version = true;
        return true;
    }
    if (!strcmp(arg, "--render-theme"))
    {
        bool c, dark;
        int rc = terminal_render_theme(&c, &dark);
        if (error_to_print_on_exit) fprintf(stderr, "%s", error_to_print_on_exit);
        exit(rc);
    }
    if (!strcmp(arg, "--xmq"))
    {
        command->in_format=XMQ_CONTENT_XMQ;
        return true;
    }
    if (!strcmp(arg, "--json"))
    {
        command->in_format=XMQ_CONTENT_JSON;
        return true;
    }
    if (!strcmp(arg, "--clines"))
    {
        command->in_format=XMQ_CONTENT_CLINES;
        return true;
    }
    if (!strcmp(arg, "--xml"))
    {
        command->in_format=XMQ_CONTENT_XML;
        return true;
    }
    if (!strcmp(arg, "--ixml"))
    {
        command->in_format=XMQ_CONTENT_IXML;
        return true;
    }
    if (!strcmp(arg, "--ixml-all-parses"))
    {
        command->ixml_all_parses=true;
        return true;
    }
    if (!strcmp(arg, "--ixml-try-to-recover"))
    {
        command->ixml_try_to_recover=true;
        return true;
    }
    if (!strcmp(arg, "--log-xmq") || !strcmp(arg, "-lx"))
    {
        log_xmq__ = true;
        return true;
    }
    if (!strcmp(arg, "--html"))
    {
        command->in_format=XMQ_CONTENT_HTML;
        return true;
    }
    if (!strcmp(arg, "--text"))
    {
        command->in_format=XMQ_CONTENT_TEXT;
        return true;
    }
    if (!strcmp(arg, "--clines"))
    {
        command->in_format=XMQ_CONTENT_CLINES;
        return true;
    }
    if (!strncmp(arg, "--root=", 7))
    {
        command->implicit_root = arg+7;
        return true;
    }
    if (!strcmp(arg, "--lines"))
    {
        command->lines = true;
        return true;
    }
    if (!strcmp(arg, "--no-merge"))
    {
        command->flags |= XMQ_FLAG_NOMERGE;
        return true;
    }
    if (!strncmp(arg, "--trim=", 7))
    {
        if (!strcmp(arg+7, "none"))
        {
            command->flags |= XMQ_FLAG_TRIM_NONE;
        }
        else if (!strcmp(arg+7, "heuristic"))
        {
            command->flags |= XMQ_FLAG_TRIM_HEURISTIC;
        }
        else if (!strcmp(arg+7, "exact"))
        {
            command->flags |= XMQ_FLAG_TRIM_EXACT;
            fprintf(stderr, "Warning --trim=exact is not yet implemented! Care to help?\n");
            exit(1);
        }
        else
        {
            printf("No such trim rule \"%s\"!\n", arg+7);
            exit(1);
        }
        return true;
    }
    if (!strncmp(arg, "--ixml=", 7))
    {
        const char *file = arg+7;

        if (command->ixml_doc != NULL || command->ixml_ixml != NULL)
        {
            printf("You have already specified an ixml grammar!\n");
            exit(1);
        }

        size_t len = strlen(file);
        if (len < 6 ||
            (strcmp(file+len-5, ".ixml") &&
             strcmp(file+len-4, ".xml") &&
             strcmp(file+len-4, ".xmq")))
        {
            printf("For ixml you can specify: g.ixml g.xml g.xmq\n");
            exit(1);
        }
        if (!strcmp(file+len-5, ".ixml"))
        {
            verbose_("xmq=", "reading ixml file %s", file);
            command->ixml_filename = strdup(file);
            command->ixml_ixml = load_file_into_buffer(file);
            if (command->ixml_ixml == NULL) exit(1);

            return true;
        }

        return false;
    }

    return false;
}

bool cmd_help(XMQCliCommand *cmd)
{
    if (cmd)
    {
        if (cmd->help_command)
        {
            XMQCliCmd c = cmd_from(cmd->help_command);
            if (c == XMQ_CLI_CMD_NONE)
            {
                printf("xmq: Unknown command %s\n", cmd->help_command);
                return false;
            }
            print_command_help(c);
            return true;
        }
    }
    printf("Usage: xmq [options] <file> ( <command> [options] )*\n"
           "\n"
           "  --debug    Output debug information on stderr.\n"
           "  --help     Display this help and exit.\n"
           "  --license  Print license.\n"
           "  --lines    Assume each input line is a separate document.\n"
           "  --nomerge  When loading xmq do not merge text quotes and character entities.\n"
           "  --root=<name>\n"
           "             Create a root node <name> unless the file starts with a node with this <name> already.\n"
           "  --trim=none|heuristic|exact\n"
           "             The default setting when reading xml/html content is to trim whitespace using a heuristic.\n"
           "             For xmq/htmq/json the default settings is none since whitespace is explicit in xmq/htmq/json.\n"
           "             Not yet implemented: exact will trim exactly to the significant whitespace according to xml/html rules.\n"
           "  --verbose  Output extra information on stderr.\n"
           "  --version  Output version information and exit.\n"
           "  --xmq|--htmq|--xml|--html|--ixml|--json|--clines\n"
           "             The input format is auto detected for xmq/xml/json but you can force the input format here.\n"
           "  --ixml=grammar.ixml Parse the content using the supplied grammar file.\n"
           "  -z         Do not read from stdin nor from a file. Start with an empty dom.\n"
           "  -i \"a=2\" Do not read from a file, use the next argument as the content to parse.\n"
           "\n"
           "To get help on the commands below: xmq help <command>\n\n"
           "COMMANDS\n"
           "  add\n"
           "  add-root\n"
           "  browser pager\n"
           "  delete delete-entity\n"
           "  for-each\n"
           "  help\n"
           "  no-output\n"
           "  render-html render-terminal render-tex\n"
           "  replace replace-entity\n"
           "  quote-c unquote-c\n"
           "  select\n"
           "  statistics\n"
           "  substitite-char-entities substitute-entity\n"
           "  to-html to-htmq to-json to-lines to-text to-xml to-xmq\n"
           "  tokenize\n"
           "  transform\n"
           "  validate\n\n"
           "EXAMPLES\n"
           "  xmq pom.xml page"
           "  xmq index.html delete //script delete //style browse\n"
           "  xmq template.htmq replace-entity DATE 2024-02-08 to-html > index.html\n"
           "  xmq data.json select /_/work transform format.xslq to-xml > data.xml\n"
           "  xmq data.html select \"//tr[@class='print_list_table_content']\" \\\n"
           "                delete //@class \\\n"
           "                add-root Course \\\n"
           "                transform --stringparam=title=Welcome cols.xslq to-text\n"
           "  xmq work.xml validate --silent work.xsd noout\n"
        );

    exit(0);
}

void print_version_and_exit()
{
    printf("xmq: %s\n", xmqVersion());
    exit(0);
}

void print_license_and_exit()
{
    puts(
"  LibXMQ\n"
"  Copyright (c) 2019-2024 Fredrik Öhrström <oehrstroem@gmail.com>\n"
"\n"
"  YAEP (Yet Another Earley Parser)\n"
"  Copyright(c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>\n"
"  Copyright(c) 2024 Fredrik Öhrström <oehrstroem@gmail.com>\n"
"\n"
"  Permission is hereby granted, free of charge, to any person obtaining a copy\n"
"  of this software and associated documentation files (the \"Software\"), to deal\n"
"  in the Software without restriction, including without limitation the rights\n"
"  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
"  copies of the Software, and to permit persons to whom the Software is\n"
"  furnished to do so, subject to the following conditions:\n"
"\n"
"  The above copyright notice and this permission notice shall be included in all\n"
"  copies or substantial portions of the Software.\n"
"\n"
"  THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
"  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
"  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
"  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
"  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
"  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
"  SOFTWARE.");

    exit(0);
}

bool cmd_tokenize(XMQCliCommand *command)
{
    verbose_("xmq=", "cmd-tokenize %s", tokenize_type_to_string(command->tok_type));
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    xmqSetupPrintMemory(output_settings, &command->env->out_start, &command->env->out_stop);
    xmqSetupPrintSkip(output_settings, &command->env->out_skip);

    XMQParseCallbacks *callbacks = xmqNewParseCallbacks();

    switch (command->tok_type) {
    case XMQ_CLI_TOKENIZE_NONE:
    case XMQ_CLI_TOKENIZE_TERMINAL:
        xmqSetRenderFormat(output_settings, XMQ_RENDER_TERMINAL);
        xmqSetUseColor(output_settings, command->use_color);
        xmqSetBackgroundMode(output_settings, command->bg_dark_mode);
        xmqSetRenderTheme(output_settings, command->render_theme);
        xmqSetupDefaultColors(output_settings);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_TERMINAL);
        break;
    case XMQ_CLI_TOKENIZE_HTML:
        xmqSetRenderFormat(output_settings, XMQ_RENDER_HTML);
        xmqSetUseColor(output_settings, command->use_color);
        xmqSetBackgroundMode(output_settings, command->bg_dark_mode);
        xmqSetRenderTheme(output_settings, command->render_theme);
        xmqSetupDefaultColors(output_settings);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_HTML);
        break;
    case XMQ_CLI_TOKENIZE_TEX:
        xmqSetRenderFormat(output_settings, XMQ_RENDER_TEX);
        xmqSetUseColor(output_settings, command->use_color);
        xmqSetBackgroundMode(output_settings, command->bg_dark_mode);
        xmqSetRenderTheme(output_settings, command->render_theme);
        xmqSetupDefaultColors(output_settings);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_TEX);
        break;
    case XMQ_CLI_TOKENIZE_DEBUG_TOKENS:
        xmqSetupParseCallbacksDebugTokens(callbacks);
        break;
    case XMQ_CLI_TOKENIZE_DEBUG_CONTENT:
        xmqSetupParseCallbacksDebugContent(callbacks);
        break;
    default:
        fprintf(stderr, "Internal error.\n");
    }

    XMQParseState *state = xmqNewParseState(callbacks, output_settings);
    if (command->in_is_content)
    {
        xmqTokenizeBuffer(state, command->in, NULL);
    }
    else
    {
        xmqTokenizeFile(state, command->in);
    }

    int err = 0;
    if (xmqStateErrno(state))
    {
        fprintf(stderr, "%s\n", xmqStateErrorMsg(state));
        err = xmqStateErrno(state);
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(callbacks);
    xmqFreeOutputSettings(output_settings);

    return err == 0;
}

bool cmd_load(XMQCliCommand *command)
{
    if (!command) return false;

    command->env->doc = xmqNewDoc();

    if (command->no_input)
    {
        command->env->load = command;
        command->in = "z";
        return true;
    }

    if (command->in &&
        command->in[0] == '-' &&
        command->in[1] == 0)
    {
        command->in = NULL;
    }

    verbose_("xmq=", "cmd-load %s", command->in);

    command->env->load = command;

    if (command->ixml_ixml != NULL)
    {
        XMQDoc *ixml_grammar = xmqNewDoc();
        xmqSetDocSourceName(ixml_grammar, command->ixml_filename);

        bool ok = xmqParseBufferWithType(ixml_grammar, command->ixml_ixml, NULL, NULL, XMQ_CONTENT_IXML, 0);

        if (!ok)
        {
            fprintf(stderr, "%s\n", xmqDocError(ixml_grammar));
            exit(1);
        }

        int flags = 0;
        if (command->ixml_all_parses) flags |= XMQ_FLAG_IXML_ALL_PARSES;
        if (command->ixml_try_to_recover) flags |= XMQ_FLAG_IXML_TRY_TO_RECOVER;

        if (command->in)
        {
            if (command->in_is_content)
            {
                ok = xmqParseBufferWithIXML(command->env->doc, command->in, NULL, ixml_grammar, flags);
                if (!ok)
                {
                    printf("xmq: could not parse: %s\n", command->in);
                    exit(1);
                }
            }
            else
            {
                ok = check_file_exists(command->in);
                if (!ok)
                {
                    printf("xmq: could not read file %s\n", command->in);
                    exit(1);
                }
                ok = xmqParseFileWithIXML(command->env->doc, command->in, ixml_grammar, flags);
                if (!ok)
                {
                    exit(1);
                }
            }
        }

        xmqFreeDoc(ixml_grammar);

        const char *from = "stdin";
        if (command->in) from = command->in;
        free((char*)command->ixml_filename);
        command->ixml_filename = NULL;
        free((char*)command->ixml_ixml);
        command->ixml_ixml = NULL;
        verbose_("xmq=", "cmd-load-ixml %zu bytes from %s", xmqGetOriginalSize(command->env->doc), from);
    }
    else
    {
        bool ok = false;
        if (command->in_is_content)
        {
            ok = xmqParseBufferWithType(command->env->doc,
                                        command->in,
                                        NULL,
                                        command->implicit_root,
                                        command->in_format,
                                        command->flags);
        }
        else
        {
            ok = xmqParseFileWithType(command->env->doc,
                                      command->in,
                                      command->implicit_root,
                                      command->in_format,
                                      command->flags);
        }

        if (!ok)
        {
            const char *error = xmqDocError(command->env->doc);
            if (error) {
                fprintf(stderr, error, command->in);
            }
            xmqFreeDoc(command->env->doc);
            command->env->doc = NULL;
            return false;
        }
        const char *from = "stdin";
        if (command->in) from = command->in;
        verbose_("xmq=", "cmd-load %zu bytes from %s", xmqGetOriginalSize(command->env->doc), from);
    }

    return true;
}

void cmd_unload(XMQCliCommand *command)
{
    if (command && command->env && command->env->doc)
    {
        verbose_("xmq=", "cmd-unload document");
        xmqFreeDoc(command->env->doc);
        command->env->doc = NULL;
    }
}

bool cmd_to(XMQCliCommand *command)
{
    XMQOutputSettings *settings = xmqNewOutputSettings();
    xmqSetCompact(settings, command->compact);
    xmqSetEscapeNewlines(settings, command->escape_newlines);
    xmqSetEscapeNon7bit(settings, command->escape_non_7bit);
    xmqSetEscapeTabs(settings, command->escape_tabs);
    xmqSetAddIndent(settings, command->add_indent);
    xmqSetUseColor(settings, command->use_color);
    xmqSetBackgroundMode(settings, command->bg_dark_mode);
    xmqSetOutputFormat(settings, command->out_format);
    xmqSetRenderFormat(settings, command->render_to);
    xmqSetRenderRaw(settings, command->render_raw);
    xmqSetRenderOnlyStyle(settings, command->only_style);
    xmqRenderHtmlSettings(settings, command->use_id, command->use_class);
    xmqSetRenderTheme(settings, command->render_theme);
    xmqSetOmitDecl(settings, command->omit_decl);
    xmqSetupDefaultColors(settings);

    if (command->has_overrides)
    {
        xmqOverrideSettings(settings,
                            command->indentation_space,
                            command->explicit_space,
                            command->explicit_tab,
                            command->explicit_cr,
                            command->explicit_nl);
    }

    verbose_("xmq=", "cmd-to %s render %s",
             content_type_to_string(command->out_format),
             render_format_to_string(command->render_to));

    xmqSetupPrintMemory(settings, &command->env->out_start, &command->env->out_stop);
    xmqSetupPrintSkip(settings, &command->env->out_skip);
    xmqPrint(command->env->doc, settings);

    xmqFreeOutputSettings(settings);
    return true;
}

bool cmd_output(XMQCliCommand *command)
{
    if (command->cmd == XMQ_CLI_CMD_STATISTICS ||
        command->cmd == XMQ_CLI_CMD_FOR_EACH ||
        command->cmd == XMQ_CLI_CMD_NO_OUTPUT)
    {
        return true;
    }
    if (!command->env->out_start)
    {
        fprintf(stderr, "xmq: no output found, please add a to/render/tokenize command\n");
        return false;
    }
    if (command->cmd == XMQ_CLI_CMD_PRINT)
    {
        verbose_("xmq=", "cmd-print output");
        console_write(command->env->out_start + command->env->out_skip, command->env->out_stop);
        free(command->env->out_start);
        return true;
    }
    if (command->cmd == XMQ_CLI_CMD_PAGER)
    {
        verbose_("xmq=", "cmd-pager output");
        page(command->env->out_start + command->env->out_skip, command->env->out_stop);
        free(command->env->out_start);
        return true;
    }
    if (command->cmd == XMQ_CLI_CMD_BROWSER)
    {
        verbose_("xmq=", "cmd-browser output");
        browse(command);
        free(command->env->out_start);
        return true;
    }
    if (command->cmd == XMQ_CLI_CMD_SAVE_TO)
    {
        if (!command->save_file)
        {
            fprintf(stderr, "xmq: save command missing file name\n");
            return false;
        }
        verbose_("xmq=", "cmd-save output to %s", command->save_file);
        size_t len = command->env->out_stop -  command->env->out_start - command->env->out_skip;
        FILE *f = fopen(command->save_file, "wb");
        if (!f)
        {
            fprintf(stderr, "xmq: %s: Cannot open file for writing\n", command->save_file);
            return false;
        }
        size_t wrote = fwrite(command->env->out_start + command->env->out_skip, 1, len, f);
        if (wrote != len)
        {
            fprintf(stderr, "xmq: Failed to write all bytes to %s wrote %zu but expected %zu\n",
                    command->save_file, wrote, len);
        }
        free(command->env->out_start);
        fclose(f);

        return true;
    }

    return false;
}

bool cmd_transform(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "transforming");

    xsltSetLoaderFunc(xmqDocDefaultLoaderFunc);

    const char **params = NULL;
    HashMapIterator *i;
    size_t j = 0;

    if (command->xslt_params)
    {
        size_t n = hashmap_size(command->xslt_params);
        params = (const char**)calloc(2*n+1, sizeof(char*));

        i = hashmap_iterate(command->xslt_params);
        const char *key;
        void *val;
        while (hashmap_next_key_value(i, &key, &val))
        {
            params[j++] = key;
            params[j++] = (const char*)val;
            verbose_("xmq=", "param %s %s", key, val);
        }
        params[j++] = NULL;
    }

    verbose_("xmq=", "applying stylesheet");
    xmlDocPtr new_doc = xsltApplyStylesheet(command->xslt, doc, params);

    if (params)
    {
        hashmap_free_iterator(i);
        free(params);
    }
    xmlFreeDoc(doc);
    xmqSetImplementationDoc(command->env->doc, new_doc);

    xsltFreeStylesheet(command->xslt);
    // The xslt free has freed the internal implementation doc as well.
    // Mark it as null to prevent double free.
    xmqSetImplementationDoc(command->xslt_doq, NULL);
    xmqFreeDoc(command->xslt_doq);
    command->xslt = NULL;

    return true;
}

bool cmd_quote_unquote(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("(xmq) transforming\n");

    if (command->cmd == XMQ_CLI_CMD_QUOTE_C)
    {
        char *value = (char*)xmlNodeListGetString(doc, doc->children, 1);
        char *quoted_value = xmq_quote_as_c(value, value+strlen(value), true);
        xmlFree(value);

        if (command->add_nl)
        {
            size_t len = strlen(quoted_value);
            char *q = malloc(len+2);
            strcpy(q, quoted_value);
            strcat(q, "\n");
            free(quoted_value);
            quoted_value = q;
        }

        xmlDocPtr new_doc = xmlNewDoc((xmlChar*)"1.0");
        xmlNodePtr new_node = xmlNewDocText(new_doc, (xmlChar*)quoted_value);
        free(quoted_value);

        xml_add_root_child(new_doc, new_node);

        xmlFreeDoc(doc);
        xmqSetImplementationDoc(command->env->doc, new_doc);

        return true;
    }

    if (command->cmd == XMQ_CLI_CMD_UNQUOTE_C)
    {
        char *value = (char*)xmlNodeListGetString(doc, doc->children, 1);
        char *unquoted_value = xmq_unquote_as_c(value, value+strlen(value), true);
        xmlFree(value);

        xmlDocPtr new_doc = xmlNewDoc((xmlChar*)"1.0");
        xmlNodePtr new_node = xmlNewDocText(new_doc, (xmlChar*)unquoted_value);
        free(unquoted_value);

        xml_add_root_child(new_doc, new_node);

        xmlFreeDoc(doc);
        xmqSetImplementationDoc(command->env->doc, new_doc);

        return true;
    }

    return false;
}

bool cmd_validate(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "validating");

    xmlSchemaValidCtxtPtr validation_ctxt = xmlSchemaNewValidCtxt(command->xsd);

    if (!command->silent)
    {
        xmlSchemaSetValidErrors(validation_ctxt,
                                abortValidating,
                                warnValidation,
                                (void*)command->xsd_name);
    }
    else
    {
        xmlSchemaSetValidErrors(validation_ctxt,
                                abortSilentValidating,
                                warnSilentValidation,
                                (void*)command->xsd_name);
    }

    if (!xmlSchemaIsValid(validation_ctxt))
    {
        fprintf(stderr, "xmq: Schema xsd not valid, cannot be used to validate document.\n");
        exit(1);
    }

    bool ok = xmlSchemaValidateDoc(validation_ctxt, doc);

    if (ok != 0)
    {
        printf("xmq: Document failed validation agains %s\n", command->in);
        return false;
    }

    xmlSchemaFreeValidCtxt(validation_ctxt);
    xmlSchemaFree(command->xsd);
    command->xsd = NULL;
    xmqFreeDoc(command->xsd_doq);
    command->xsd_doq = NULL;

    return true;
}

bool cmd_delete(XMQCliCommand *command)
{
    if (command->xpath)
    {
        verbose_("xmq=", "cmd-delete xpath %s", command->xpath);
        return delete_xpath(command);
    }

    if (command->entity)
    {
        verbose_("xmq=", "cmd-delete entity %s", command->entity);
        return delete_entity(command);
    }

    return false;
}

bool cmd_select(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "selecting");

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    assert(ctx);

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)command->xpath, ctx);

    if (objects == NULL)
    {
        fprintf(stderr, "xmq: no nodes matched %s\n", command->xpath);
        xmlXPathFreeContext(ctx);
        return false;
    }

    xmlDocPtr new_doc = xmlNewDoc((xmlChar*)"1.0");

    xmlNodeSetPtr nodes = objects->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;

    // Copy and unlink in reverse order which means copy unlink deeper nodes first.
    for(int i = size-1; i >= 0; i--)
    {
        assert(nodes->nodeTab[i]);
        xmlNode *n = nodes->nodeTab[i];

        while (n && !is_element_node(n) && !is_content_node(n))
        {
            // We found an attribute move to parent.
            n = n->parent;
        }
        xmlNodePtr new_node = xmlCopyNode(n, 1);
        xml_add_root_child(new_doc, new_node);

        xmlUnlinkNode(n);
        xmlFreeNode(n);
    }

    xmlXPathFreeObject(objects);
    xmlXPathFreeContext(ctx);

    xmlFreeDoc(doc);
    xmqSetImplementationDoc(command->env->doc, new_doc);

    return true;
}

bool cmd_for_each(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "for each");

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    assert(ctx);

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)command->xpath, ctx);

    if (objects == NULL)
    {
        fprintf(stderr, "xmq: no nodes matched %s\n", command->xpath);
        xmlXPathFreeContext(ctx);
        return false;
    }

    xmlNodeSetPtr nodes = objects->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;

    for(int i = 0; i < size; ++i)
    {
        assert(nodes->nodeTab[i]);
        xmlNode *n = nodes->nodeTab[i];

        if (n)
        {
            while (n && !is_element_node(n))
            {
                // We found an attribute move to parent.
                n = n->parent;
            }
            invoke_shell(n, command->content);
        }
    }

    xmlXPathFreeObject(objects);
    xmlXPathFreeContext(ctx);

    return true;
}

bool cmd_add(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "adding xmq >%s<", command->add_xmq);

    XMQDoc *doq = xmqNewDoc();
    const char *start = command->add_xmq;
    const char *stop = start+strlen(start);

    bool ok = xmqParseBuffer(doq, start, stop, NULL, 0);
    if (!ok)
    {
        const char *error = xmqDocError(doq);
        fprintf(stderr, error, command->in);
        xmqFreeDoc(doq);
        return false;
    }

    xmlDocPtr append = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlNodePtr i = append->children;
    while (i)
    {
        xmlNodePtr next = i->next;
        if (doc->last)
        {
            xmlAddSibling(doc->last, i);
        }
        else
        {
            xmlDocSetRootElement(doc, i);
        }
        i = next;
    }

    xmqFreeDoc(doq);

    return true;
}

bool cmd_add_root(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "adding root");

    xmlDocPtr new_doc = xmlNewDoc((xmlChar*)"1.0");

    xmlNodePtr root = xmlNewDocNode(new_doc, NULL, (const xmlChar*)command->xpath, NULL);
    xmlDocSetRootElement(new_doc, root);

    xmlNodePtr i = doc->children;

    while (i)
    {
        xmlNodePtr new_node = xmlCopyNode(i, 1);
        xmlAddChild(root, new_node);
        i = i->next;
    }

    xmlFreeDoc(doc);
    xmqSetImplementationDoc(command->env->doc, new_doc);

    return true;
}

void accumulate_children(Stats *stats, xmlNode *node)
{
    xmlAttr *a = xml_first_attribute(node);
    while (a)
    {
        xmlAttr *next = a->next;
        accumulate_attribute(stats, a);
        a = next;
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i);
        accumulate_statistics(stats, i);
        i = next;
    }
}

void accumulate_attribute(Stats *stats, xmlAttr *a)
{
    stats->num_attributes++;
    stats->size_attribute_names += strlen((const char*)a->name);

    xmlNode *i = a->children;
    while (i)
    {
        xmlNode *next = xml_next_sibling(i);
        accumulate_attribute_content(stats, i);
        i = next;
    }

}

void accumulate_statistics(Stats *stats, xmlNode *node)
{
    if (node->type == XML_ELEMENT_NODE)
    {
        stats->num_elements++;
        stats->size_element_names += strlen((const char*)node->name);
        accumulate_children(stats, node);
        return;
    }
    else if (node->type == XML_TEXT_NODE)
    {
        stats->num_text_nodes++;
        stats->size_text_nodes += strlen((const char*)node->content);
        return;
    }
    else if (node->type == XML_COMMENT_NODE)
    {
        stats->num_comments++;
        stats->size_comments += strlen((const char*)node->content);
        return;
    }
    else if (node->type == XML_CDATA_SECTION_NODE)
    {
        stats->num_cdata_nodes++;
        stats->size_cdata_nodes += strlen((const char*)node->content);
        return;
    }
    else if (node->type == XML_ENTITY_REF_NODE)
    {
        stats->num_entities++;
        stats->size_entities += strlen((const char*)node->name);
        return;
    }
    else if (node->type == XML_DTD_NODE)
    {
        stats->has_doctype++;
        stats->size_doctype += strlen((const char*)node->name);
        return;
    }
}

void accumulate_attribute_content(Stats *stats, xmlNode *node)
{
    if (node->type == XML_TEXT_NODE)
    {
        stats->size_attribute_content += strlen((const char*)node->content);
        return;
    }
}

void count_statistics(Stats *stats, xmlDocPtr doc)
{
    xmlNodePtr i = doc->children;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i);
        accumulate_statistics(stats, i);
        i = next;
    }
}

void add_key_value(xmlDoc *doc, xmlNode *root, const char *key, size_t value)
{
    char buf[1024];
    snprintf(buf, 1024, "%zu", value);
    xmlNodePtr element = xmlNewDocNode(doc, NULL, (xmlChar*)key, NULL);
    xmlAddChild(root, element);
    xmlNodePtr text = xmlNewDocText(doc, (xmlChar*)buf);
    xmlAddChild(element, text);
}

bool cmd_statistics(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    verbose_("xmq=", "calculating statistics");
    Stats stats;
    memset(&stats, 0, sizeof(stats));

    count_statistics(&stats, doc);

    xmlDocPtr new_doc = xmlNewDoc((xmlChar*)"1.0");
    xmlNodePtr root = xmlNewDocNode(new_doc, NULL, (xmlChar*)"statistics", NULL);
    xmlDocSetRootElement(new_doc, root);

    size_t size_source = xmqGetOriginalSize(command->env->doc);
    add_key_value(new_doc, root, "size_source", size_source);
    if (stats.num_elements) add_key_value(new_doc, root, "num_elements", stats.num_elements);
    if (stats.size_element_names) add_key_value(new_doc, root, "size_element_names", stats.size_element_names);
    if (stats.num_text_nodes) add_key_value(new_doc, root, "num_text_nodes", stats.num_text_nodes);
    if (stats.size_text_nodes) add_key_value(new_doc, root, "size_text_nodes", stats.size_text_nodes);
    if (stats.num_attributes) add_key_value(new_doc, root, "num_attributes", stats.num_attributes);
    if (stats.size_attribute_names) add_key_value(new_doc, root, "size_attribute_names", stats.size_attribute_names);
    if (stats.size_attribute_content) add_key_value(new_doc, root, "size_attribute_content", stats.size_attribute_content);
    if (stats.num_comments) add_key_value(new_doc, root, "num_comments", stats.num_comments);
    if (stats.size_comments) add_key_value(new_doc, root, "size_comments", stats.size_comments);
    if (stats.size_doctype) add_key_value(new_doc, root, "size_doctype", stats.size_doctype);
    if (stats.num_cdata_nodes) add_key_value(new_doc, root, "num_cdata_nodes", stats.num_cdata_nodes);
    if (stats.size_cdata_nodes) add_key_value(new_doc, root, "size_cdata_nodes", stats.size_cdata_nodes);

    size_t sum_meta = stats.size_element_names+stats.size_attribute_names+stats.size_attribute_content+stats.size_doctype;
    size_t sum_text = stats.size_text_nodes;

    add_key_value(new_doc, root, "sum_meta", sum_meta);
    add_key_value(new_doc, root, "sum_text", sum_text);

    xmlFreeDoc(doc);
    xmqSetImplementationDoc(command->env->doc, new_doc);

    return true;
}

void delete_all_entities(XMQDoc *doq, xmlNode *node, const char *entity)
{
    if (node->type == XML_ENTITY_NODE ||
        node->type == XML_ENTITY_REF_NODE)
    {
        if (!strcmp(entity, (char*)node->name))
        {
            xmlUnlinkNode(node);
            xmlFreeNode(node);
        }
        return;
    }

    xmlNode *i = node->children;
    while (i)
    {
        xmlNode *next = i->next; // i might be freed in trim.
        delete_all_entities(doq, i, entity);
        i = next;
    }
}

void delete_entities(XMQDoc *doq, const char *entity)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlNodePtr i = doc->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = i->next; // i might be freed in delete_all_entities
        delete_all_entities(doq, i, entity);
        i = next;
    }
}

bool delete_entity(XMQCliCommand *command)
{
    verbose_("xmq=", "deleting entity %s", command->entity);

    delete_entities(command->env->doc, command->entity);

    return true;
}

bool delete_xpath(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    assert(ctx);

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)command->xpath, ctx);

    if (objects == NULL)
    {
        verbose_("xmq=", "no nodes deleted");
        xmlXPathFreeContext(ctx);
        return true;
    }

    xmlNodeSetPtr nodes = objects->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;

    // Unlink in reverse order which means unlink deeper nodes first.
    for(int i = size-1; i >= 0; i--)
    {
        assert(nodes->nodeTab[i]);

        xmlUnlinkNode(nodes->nodeTab[i]);
        xmlFreeNode(nodes->nodeTab[i]);
    }

    xmlXPathFreeObject(objects);
    xmlXPathFreeContext(ctx);

    return true;
}

xmlNode *replace_entity(xmlDoc *doc, xmlNode *node, const char *entity, const char *content, xmlNodePtr node_content)
{
    if (!node) return NULL;

    xmlNode *next = node->next;

    if (node->type == XML_ENTITY_REF_NODE)
    {
        if (!strcmp((const char *)node->name, entity))
        {
            if (content)
            {
                xmlNodePtr new_node = xmlNewDocText(doc, (const xmlChar*)content);
                xmlReplaceNode(node, new_node);
                xmlFreeNode(node);
                node = new_node;
                next = node->next;
            }
            else if (node_content)
            {
                xmlNodePtr new_node = xmlCopyNode(node_content, 1);
                xmlReplaceNode(node, new_node);
                xmlFreeNode(node);
                node = new_node;
                next = node->next;
            }
            else
            {
                assert(false);
            }
        }
        if (node->prev != NULL && is_content_node(node->prev) && is_content_node(node))
        {
            node = xmlTextMerge(node->prev, node);
            next = node->next;

            if (node->next != NULL && is_content_node(node->next))
            {
                node = xmlTextMerge(node, node->next);
                next = node->next;
            }
        }
        return next;
    }

    xmlAttr *a = xml_first_attribute(node);
    while (a)
    {
        if (a->children != NULL)
        {
            xmlNode *i = a->children;
            while (i)
            {
                i = replace_entity(doc, i, entity, content, node_content);
            }
        }
        a = xml_next_attribute(a);
    }

    xmlNode *i = node->children;
    while (i)
    {
        i = replace_entity(doc, i, entity, content, node_content);
    }

    return next;
}

void replace_entities(xmlDoc *doc, const char *entity, const char *content, xmlNodePtr node_content)
{
    xmlNodePtr i = doc->children;
    if (!doc || !i) return;

    while (i)
    {
        replace_entity(doc, i, entity, content, node_content);
        i = i->next;
    }
}

bool cmd_replace(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    xmlNodePtr root = xmlDocGetRootElement(doc);

    while (root->prev) root = root->prev;
    assert(root != NULL);

    if (command->xpath)
    {
        verbose_("xmq=", "replacing xpath %s", command->xpath);
    }

    if (command->entity)
    {
        verbose_("xmq=", "replacing entity %s", command->entity);

        replace_entities(doc, command->entity, command->content, command->node_content);
    }

    return true;
}

void substitute_entity(xmlDoc *doc, xmlNode *node, const char *entity, bool only_chars)
{
    if (!node) return;
    if (node->type == XML_ENTITY_REF_NODE)
    {
        if (entity == NULL || !strcmp((const char *)node->name, entity))
        {
            if (node->name[0] == '#')
            {
                char buf[5] = {};
                char *out = buf;
                int uc = decode_entity_ref((const char *)node->name);
                UTF8Char utf8;
                size_t n = encode_utf8(uc, &utf8);
                for (size_t j = 0; j < n; ++j)
                {
                    *out++ = utf8.bytes[j];
                }
                xmlNodePtr new_node = xmlNewDocText(doc, (const xmlChar*)buf);
                xmlReplaceNode(node, new_node);
                xmlFreeNode(node);
            }
            else if (!only_chars)
            {
                if (node->children)
                {
                    if (is_content_node(node))
                    {
                        xmlNodePtr new_node = xmlNewDocText(doc, node->content);
                        xmlReplaceNode(node, new_node);
                        xmlFreeNode(node);
                    }
                }
            }
        }
        return;
    }

    xmlNode *i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        substitute_entity(doc, i, entity, only_chars);
        i = next;
    }
}

bool cmd_substitute(XMQCliCommand *command)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(command->env->doc);
    xmlNodePtr root = xmlDocGetRootElement(doc);

    while (root->prev) root = root->prev;
    assert(root != NULL);

    if (command->cmd == XMQ_CLI_CMD_SUBSTITUTE_ENTITY)
    {
        verbose_("xmq=", "substituting entity %s", command->entity);

        substitute_entity(doc, root, command->entity, false);
    }
    if (command->cmd == XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES)
    {
        verbose_("xmq=", "substituting all char entities");

        substitute_entity(doc, root, NULL, true);
    }

    return true;
}

void prepare_command(XMQCliCommand *c, XMQCliCommand *load_command)
{
    switch (c->cmd) {
    case XMQ_CLI_CMD_TO_XMQ:
        c->out_format = XMQ_CONTENT_XMQ;
        return;
    case XMQ_CLI_CMD_TO_XML:
        c->out_format = XMQ_CONTENT_XML;
        return;
    case XMQ_CLI_CMD_TO_HTMQ:
        c->out_format = XMQ_CONTENT_HTMQ;
        return;
    case XMQ_CLI_CMD_TO_HTML:
        c->out_format = XMQ_CONTENT_HTML;
        return;
    case XMQ_CLI_CMD_TO_JSON:
        c->out_format = XMQ_CONTENT_JSON;
        return;
    case XMQ_CLI_CMD_TO_TEXT:
        c->out_format = XMQ_CONTENT_TEXT;
        return;
    case XMQ_CLI_CMD_TO_CLINES:
        c->out_format = XMQ_CONTENT_CLINES;
        return;
    case XMQ_CLI_CMD_PAGER:
        c->out_format = XMQ_CONTENT_UNKNOWN;
        c->render_to = XMQ_RENDER_TERMINAL;
        return;
    case XMQ_CLI_CMD_BROWSER:
        c->out_format = XMQ_CONTENT_UNKNOWN;
        c->render_to = XMQ_RENDER_HTML;
        return;
    case XMQ_CLI_CMD_PRINT:
        c->out_format = XMQ_CONTENT_UNKNOWN;
        c->render_to = XMQ_RENDER_TERMINAL;
        return;
    case XMQ_CLI_CMD_SAVE_TO:
        c->out_format = XMQ_CONTENT_UNKNOWN;
        c->render_to = XMQ_RENDER_PLAIN;
        return;
    case XMQ_CLI_CMD_RENDER_TERMINAL:
        c->out_format = XMQ_CONTENT_XMQ;
        c->render_to = XMQ_RENDER_TERMINAL;
        return;
    case XMQ_CLI_CMD_RENDER_HTML:
        c->out_format = XMQ_CONTENT_XMQ;
        c->render_to = XMQ_RENDER_HTML;
        return;
    case XMQ_CLI_CMD_RENDER_TEX:
        c->out_format = XMQ_CONTENT_XMQ;
        c->render_to = XMQ_RENDER_TEX;
        return;
    case XMQ_CLI_CMD_RENDER_RAW:
        c->out_format = XMQ_CONTENT_UNKNOWN;
        c->render_to = XMQ_RENDER_RAW;
        return;
    case XMQ_CLI_CMD_TOKENIZE:
        c->in = load_command->in;
        // Overwrite load command, do not load before tokenize.
        load_command->cmd = XMQ_CLI_CMD_NONE;
        return;
    case XMQ_CLI_CMD_DELETE:
        return;
    case XMQ_CLI_CMD_DELETE_ENTITY:
        return;
    case XMQ_CLI_CMD_REPLACE:
        return;
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return;
    case XMQ_CLI_CMD_SUBSTITUTE_ENTITY:
        return;
    case XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES:
        return;
    case XMQ_CLI_CMD_TRANSFORM:
        return;
    case XMQ_CLI_CMD_VALIDATE:
        return;
    case XMQ_CLI_CMD_SELECT:
        return;
    case XMQ_CLI_CMD_FOR_EACH:
        return;
    case XMQ_CLI_CMD_ADD_ROOT:
        return;
    case XMQ_CLI_CMD_ADD:
        return;
    case XMQ_CLI_CMD_STATISTICS:
        return;
    case XMQ_CLI_CMD_QUOTE_C:
    case XMQ_CLI_CMD_UNQUOTE_C:
        return;
    case XMQ_CLI_CMD_NONE:
        return;
    case XMQ_CLI_CMD_LOAD:
        return;
    case XMQ_CLI_CMD_NO_OUTPUT:
        return;
    case XMQ_CLI_CMD_HELP:
        c->print_help = true;
        return;
    }
}

size_t count_non_ansi_chars(const char *start, const char *stop)
{
    const char *i = start;
    size_t n = 0;

    while (i < stop)
    {
        if (*i == 27)
        {
            // Skip ansi escape sequence.
            while (i < stop)
            {
                char c = *i++;
                if (c == 'm') continue;
            }
        }
        i++;
        n++;
    }

    return n;
}

const char *skip_ansi_backwards(const char *i, const char *start)
{
    // An ansi escape looks like: \033[38m
    if (*i != 'm') return i;
    // Check that we have space to look for ansi. Minimum ansi: \033[0m which is 4 bytes.
    if (i-4 < start) return i;
    // Must be a digit before m.
    if (!isdigit(*(i-1))) return i;

    const char *org_i = i;
    while (i > start)
    {
        char c = *i;
        // Stop at [
        if (c == '[') break;
        // Abort if not digits nor ;
        if (!isdigit(c) && c != ';') return org_i;
        i--;
    }
    if (i <= start) return org_i;
    i--;
    if (i < start) return org_i;
    if (*i != '\033') return org_i;
    return i-1;
}

void print_until_newline(const char *info, const char *i, const char *stop);

void print_until_newline(const char *info, const char *i, const char *stop)
{
    const char *start = i;
    while (i < stop && *i != '\n') i++;
    size_t n = i-start;
    fprintf(stderr, "%s >%.*s<\n", info, (int)n, start);
}

const char *skip_line_width_backwards(const char *lo, const char *ilo, const char *start, const char *stop, int width);

const char *skip_line_width_backwards(const char *lo, const char *ilo, const char *start, const char *stop, int width)
{
    assert(width > 0);
    const char *i = ilo;
    while (i > start)
    {
        i = skip_ansi_backwards(i, start);
        width--;
        i--;
        if (*i == '\n') return i+1; // This is an error....
        if (i == lo) return i; // This is an error....
        if (width == 0) return i; // Good, we skipped backwards in a wrapped line.
    }
    return start;
}

const char *find_prev_line_start(const char *lo, const char *ilo, const char *start, const char *stop);

const char *find_prev_line_start(const char *lo, const char *ilo, const char *start, const char *stop)
{
    if (ilo > lo) return lo;
    // *lo is the first character in the current line.
    // *(lo-1) is the newline ending the previous line.
    // *(lo-2) is the last character on the previous line.
    // Start searching from the last character on the previous line.
    const char *i = lo-2;
    while (i > start)
    {
        if (*i == '\n')
        {
            return i+1;
        }
        i--;
    }
    return start;
}

void find_prev_line(const char **line_offset,
                    const char **in_line_offset,
                    const  char *start,
                    const char *stop,
                    int width)
{
    /* - Lo and ilo might point to the same line starting point.
         This happens for non-wrapped lines.
       - Lo and ilo might point to the same line starting point for
         wrapped long lines if we have not scrolled down into the line.
       - Ilo might point into the wrapped line lo after we have scrolled
         down a line or more into a wrapped long line. */
    const char *lo = *line_offset;
    const char *ilo = *in_line_offset;

    const char *prev_lo = find_prev_line_start(lo, ilo, start, stop);
    // Prev_lo is now lo or the line before.

    // How far is it from the prev line start to the ilo?
    int diff = count_non_ansi_chars(prev_lo, ilo);

    if (diff > width)
    {
        // We have a wrapped line move backwards the width within the line.
        *line_offset = prev_lo;
        *in_line_offset = skip_line_width_backwards(lo, ilo, start, stop, width);
        return;
    }

    *line_offset = prev_lo;
    *in_line_offset = prev_lo;
}

void find_next_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height)
{
    for (int i=0; i<height; ++i)
    {
        find_next_line(line_offset, in_line_offset, start, stop, width);
    }
}

void find_prev_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height)
{
    for (int i=0; i<height; ++i)
    {
        find_prev_line(line_offset, in_line_offset, start, stop, width);
    }
}

void find_next_line(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width)
{
    const char *lo = *line_offset;
    const char *ilo = *in_line_offset;

    // Lo and ilo might point to the same line starting point. This happens for non-wrapped lines.
    // Lo and ilo might point to the same line starting point for wrapped long lines if we have not scrolled
    // down into the line.
    // Ilo might point to the start of the wrapped line lo when scrolled down a line or more into a wrapped long line.

    // Lets find the next ilo if we move into the next wrapped line or lo+ilo if we exit the wrapped line.
    const char *i = ilo;
    int col = 0;
    while (i < stop)
    {
        if (*i == 27)
        {
            // Skip ansi escape sequence.
            while (i < stop)
            {
                char c = *i;
                i++;
                if (c == 'm') break;
            }
        }

        if (*i == '\n')
        {
            // We found a newline. Nice, we have a new lo and ilo after the newline.
            lo = ilo = i+1;
            break;
        }

        col++;
        if (col >= width)
        {
            // We reached a new wrapped line. Stop here.
            ilo = i;
            break;
        }
        i++;
    }

    // We reached end of buffer before we found a new line. Do nothing.
    if (i >= stop) return;

    *line_offset = lo;
    *in_line_offset = ilo;
}

int get_char()
{
#ifndef PLATFORM_WINAPI
    unsigned char c;
    size_t n = read(STDOUT_FILENO, &c, sizeof(char));
    if (n == -1) return -1;
    return c;
#else
    return _getch();
#endif
}

void put_char(int c)
{
#ifndef PLATFORM_WINAPI
    putchar(c);
#else
    _putch(c);
#endif
}

void console_write(const char *start, const char *stop)
{
#ifndef PLATFORM_WINAPI
    printf("%.*s", (int)(stop-start), start);
#else
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD wrote = 0;
	WriteConsole(out, start, stop-start, &wrote, NULL);
#endif
}

void open_browser(const char *file)
{
#ifdef PLATFORM_WINAPI
    ShellExecute(NULL, "open", file, NULL, NULL, SW_SHOWNORMAL);
#else
    char buf[2049];
    memset(buf, 0, 2049);
    snprintf(buf, 2048, "open %s", file);
    int rc = system(buf);
    if (rc != 0)
    {
        fprintf(stderr, "xmq: command failed rc=%d:\n%s\n", rc, buf);
    }
#endif
}

void print_command_help(XMQCliCmd c)
{
    switch (c)
    {
    case XMQ_CLI_CMD_NONE:
        break;
    case XMQ_CLI_CMD_LOAD:
        printf(
            "Usage: xmq <input> load <file>\n"
            "       xmq <input> load --alias=WORK <file>\n"
            "Load a second document and store it under the alias. If no alias is given, then the first document is replaced.\n");
        break;
    case XMQ_CLI_CMD_ADD:
        printf(
            "Usage: xmq <input> add <xmq>\n"
            "Append the xmq to the DOM.\n");
        break;
    case XMQ_CLI_CMD_ADD_ROOT:
        printf(
            "Usage: xmq <input> add-root <root-element-name>\n"
            "Place a new root node below all existing nodes. Useful after a select.\n");
        break;
    case XMQ_CLI_CMD_HELP:
        printf(
            "Usage: xmq help <command>\n"
            "Print more detailed help about the command.\n");
        break;
    case XMQ_CLI_CMD_DELETE:
        printf(
            "Usage: xmq <input> delete <xpath>\n"
            "Delete all nodes matching xpath.\n");
        break;
    case XMQ_CLI_CMD_VALIDATE:
        printf(
            "Usage: xmq <input> validate <xsd>\n"
            "Validate document against the supplied xsd. Exit with error if validation fails.\n");
        break;
    case XMQ_CLI_CMD_NO_OUTPUT:
        printf(
            "Usage: xmq <input> no-output\n"
            "Do not print any output, can be used after for example validate.\n");
        break;
    default:
        printf("Help not written yet.\n");
    }
}

KEY read_key(int *c)
{
    int k = get_char();
    if (k == 10 || k == 13) return ENTER;
    else if (k == 0 || k == 224)
    {
        k = get_char();
        if (k == 72) return ARROW_UP;
        if (k == 75) return ARROW_LEFT;
        if (k == 77) return ARROW_RIGHT;
        if (k == 80) return ARROW_DOWN;
        if (k == 73) return PAGE_UP;
        if (k == 81) return PAGE_DOWN;
    }
    else
    if (k == 27)
    {
        k = get_char();
        if (k == 91)
        {
            k = get_char();
            if (k == 65) return ARROW_UP;
            else if (k == 66) return ARROW_DOWN;
            else if (k == 53)
            {
                k = get_char();
                if (k == 126) return PAGE_UP;
            }
            else if (k == 54)
            {
                k = get_char();
                if (k == 126) return PAGE_DOWN;
            }
        }
    }

    *c = k;
    return CHARACTER;
}


void browse(XMQCliCommand *c)
{
    const char *file;

    if (c->env->load->in)
    {
        const char *in = c->env->load->in;
        file = in+strlen(in);
        // Find basename, eg data.xml from /alfa/beta/data.xml
        // or data.xml from C:\alfa\beta\data.xml
        while (file > in && *(file-1) != '/' && *(file-1) != '\\') file--;
    }
    else
    {
        file = "stdin";
    }

    char tmpfile[1024];
    memset(tmpfile, 0, 1024);
    snprintf(tmpfile, 1023, "xmq_browsing_%s.html", file);

    printf("Created file %s\n", tmpfile);

#ifdef PLATFORM_WINAPI
    int fd = open(tmpfile, O_CREAT | O_TRUNC | O_RDWR, 0666);
#else
    // Open the temporary file and truncate it, but do not follow a symbolic link
    // since such a link could trick you into overwriting something else.
    int fd = open(tmpfile, O_CREAT | O_TRUNC | O_NOFOLLOW | O_RDWR, 0666);
#endif
    size_t len = c->env->out_stop - c->env->out_start - c->env->out_skip;
    size_t wrote = write(fd, c->env->out_start, len);
    close(fd);

    if (wrote == len)
    {
        open_browser(tmpfile);
    }
    else
    {
        fprintf(stderr, "xmq: Failed to write content to %s\n", tmpfile);
    }
}

bool shell_safe(char *i)
{
    char c = *i;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}

char *make_shell_safe_name(char *name, char *name_start)
{
    char *s = strdup(name);
    for (char *i = s, *j = name_start; *i; ++i, ++j)
    {
        if (!shell_safe(i))
        {
            *i = *j = '_';
        }
    }
    return s;
}

char *grab_name(const char *s, const char **out_name_start)
{
    const char *i = s;
    const char *name_start = NULL;
    if (*i != '$') return NULL;
    i++;
    if (*i != '{') return NULL;
    i++;
    name_start = i;
    while (*i && *i != '}') i++;
    if (!*i) return NULL;
    const char *name_stop = i;
    assert(*i == '}');
    i++;
    size_t len = name_stop-name_start;
    char *buf = malloc(len+1);
    *out_name_start = name_start;
    memcpy(buf, name_start, len);
    buf[len]  = 0;
    return buf;
}

void append_text_node(MemBuffer *buf, xmlNode *node)
{
    if (is_content_node(node))
    {
        membuffer_append(buf, (char*)node->content);
    }
    else if (is_entity_node(node))
    {
        membuffer_append(buf, "TODO");
    }
    else if (is_element_node(node))
    {
        append_text_children(buf, node->children);
    }
}

void append_text_children(MemBuffer *buf, xmlNode *n)
{
    xmlNode *i = n;

    while (i)
    {
        append_text_node(buf, i);
        i = xml_next_sibling(i);
    }
}

char *grab_content(xmlNode *n, const char *name)
{
    MemBuffer *buf = new_membuffer();

    if (!strcmp(name, ".."))
    {
        membuffer_append(buf, (char*)n->name);
    }
    else
    if (!strcmp(name, "."))
    {
        append_text_node(buf, n);
    }
    else
    {
        xmlNode *i = n->children;
        while (i)
        {
            if (!strcmp((char*)i->name, name))
            {
                xmlNode *j = i->children;
                while (j)
                {
                    if (is_text_node(j))
                    {
                        membuffer_append(buf, (char*)j->content);
                    }
                    j = j->next;
                }
            }
            i = i->next;
        }
    }
    membuffer_append_null(buf);
    return free_membuffer_but_return_trimmed_content(buf);
}

// Posix says that this variable just exists.
// (On some systems this is also declared in unistd.h)
extern char **environ;

void invoke_shell(xmlNode *n, const char *shell_command)
{
#ifndef PLATFORM_WINAPI
    char *cmd = strdup(shell_command);
    char **argv = malloc(sizeof(char*)*4);
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = (char*)cmd;
    argv[3] = 0;

    MemBuffer *envbuf = new_membuffer();

    size_t num_envs;
    for (num_envs = 0; environ[num_envs]; num_envs++)
    {
        const char *e = environ[num_envs];
        membuffer_append_pointer(envbuf, (void*)e);
    }

    const char *i;
    for (i = cmd; *i; ++i)
    {
        if (*i == '$')
        {
            const char *name_start;
            char *name = grab_name(i, &name_start);
            if (name)
            {
                char *safe_name = make_shell_safe_name(name, (char*)name_start);
                char *content = grab_content(n, name);
                if (content)
                {
                    MemBuffer *kv = new_membuffer();
                    membuffer_append(kv, safe_name);
                    membuffer_append(kv, "=");
                    membuffer_append(kv, content);
                    membuffer_append_null(kv);
                    char *e = free_membuffer_but_return_trimmed_content(kv);
                    membuffer_append_pointer(envbuf, e);
                }
                free(content);
                free(safe_name);
                free(name);
            }
        }
    }
    membuffer_append_pointer(envbuf, 0);
    char **env = (void*)free_membuffer_but_return_trimmed_content(envbuf);

    pid_t pid = fork();
    int status;
    if (pid == 0)
    {
        // I am the child!
        close(0); // Close stdin
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        execve("/bin/sh", argv, env);
#else
        execve("/bin/sh", argv, env);
#endif
        perror("Execvp failed:");
        //error_("(shell) invoking %s failed!\n", program.c_str());
    } else {
        if (pid == -1) {
            perror("(shell) could not fork!\n");
        }
        debug_("shell=", "waiting for child %d to complete.", pid);
        // Wait for the child to finish!
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            debug_("shell=", "%s: return code %d\n", "/bin/sh", rc);
            if (rc != 0) {
                verbose_("shell=", "%s exited with non-zero return code: %d\n", "/bin/sh", rc);
            }
        }
        free(argv);
        size_t n = 0;
        for (char **i = env; *i; ++i, n++)
        {
            if (n >= num_envs)
            {
                free(*i);
            }
        }
        free(env);
    }
    free(cmd);
#endif
}

void page(const char *start, const char *stop)
{
    int width = 0; // Max columns ie the width
    int height = 0; // Max height ie the number of rows
    int col = 0; // Current column starts with 0
    int row = 0; // Current row starts with 0
    const char *line_offset = start; // Points to a start of line in the buffer.
    const char *in_line_offset = start; // Points to line_offset or to start of next wrapped line.

    if (!isatty(1))
    {
        fprintf(stderr, "xmq: The page command can only be used with a terminal as output.\n");
        exit(1);
    }
#ifndef PLATFORM_WINAPI
    struct winsize max;
    ioctl(STDOUT_FILENO, TIOCGWINSZ , &max);
    height = max.ws_row - 1;
    width = max.ws_col;

#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    width = csbi.srWindow.Right - csbi.srWindow.Left + 1 - 1;
    height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1 - 1;

#endif

    enableRawStdinTerminal();

    for (;;)
    {
        MemBuffer *buffer = new_membuffer();
        row = col = 0;
        const char *i = line_offset;
        for (; i < stop; ++i)
        {
            if (*i == 27)
            {
                // Ansi escape sequence.
                while (i < stop)
                {
                    char c = *i;
                    membuffer_append_char(buffer, c);
                    if (c == 'm') break;
                    i++;
                }
                continue;
            }
            // Only start counting rows and cols when we have passed the in_line_offset.
            // Why? Because we want to print the line from the start to have the ansi colors
            // correctly setup. Then we will print a bit more att the end which causes
            // the display to scroll up and hide the line(s) before in_line_offset.
            if (i >= in_line_offset)
            {
                if (*i == '\n')
                {
                    row++;
                    col = 0;
                    if (row >= height) break;
                }
                else
                {
                    col++;
                    if (col >= width)
                    {
                        row++;
                        col = 0;
                        if (row >= height) {
                            membuffer_append_char(buffer, *i);
                            break;
                        }
                    }
                }
            }
            membuffer_append_char(buffer, *i);
        }
        console_write(buffer->buffer_, buffer->buffer_+buffer->used_);
        printf("\033[0m\n:");
        fflush(stdout);
        free_membuffer_and_free_content(buffer);
        int c;
        KEY key = read_key(&c);

        if ((key == CHARACTER && c == ' ') ||
            (key == PAGE_DOWN))
        {
            find_next_page(&line_offset, &in_line_offset, start, stop, width, height);
        }
        if (key == PAGE_UP)
        {
            find_prev_page(&line_offset, &in_line_offset, start, stop, width, height);
        }
        else if (key == ENTER ||
                 key == ARROW_DOWN)
        {
            find_next_line(&line_offset, &in_line_offset, start, stop, width);
        }
        else if (key == ARROW_UP)
        {
            find_prev_line(&line_offset, &in_line_offset, start, stop, width);
        }
        else if (key == CHARACTER && c == '<')
        {
            line_offset = in_line_offset = start;
        }
        else if (key == CHARACTER && c == '>')
        {
            line_offset = in_line_offset = stop-100;
        }
        else if (key == CHARACTER &&
                 (c == 27 || c == 'q' || c == 'Q'))
        {
            printf("\033[2K\r");
            break;
        }
        printf("\033[2J\033[H");
    }

    restoreStdinTerminal();
    printf("\033[0m");
}

bool perform_command(XMQCliCommand *c)
{
    if (c->cmd == XMQ_CLI_CMD_NONE) return true;

    switch (c->cmd) {
    case XMQ_CLI_CMD_NONE:
        return true;
    case XMQ_CLI_CMD_NO_OUTPUT:
        return true;
    case XMQ_CLI_CMD_LOAD:
        return cmd_load(c);
    case XMQ_CLI_CMD_TO_XMQ:
    case XMQ_CLI_CMD_TO_XML:
    case XMQ_CLI_CMD_TO_HTMQ:
    case XMQ_CLI_CMD_TO_HTML:
    case XMQ_CLI_CMD_TO_JSON:
    case XMQ_CLI_CMD_TO_TEXT:
    case XMQ_CLI_CMD_TO_CLINES:
    case XMQ_CLI_CMD_RENDER_TERMINAL:
    case XMQ_CLI_CMD_RENDER_HTML:
    case XMQ_CLI_CMD_RENDER_TEX:
    case XMQ_CLI_CMD_RENDER_RAW:
        return cmd_to(c);
    case XMQ_CLI_CMD_PRINT:
    case XMQ_CLI_CMD_SAVE_TO:
    case XMQ_CLI_CMD_PAGER:
    case XMQ_CLI_CMD_BROWSER:
        return cmd_output(c);
    case XMQ_CLI_CMD_TOKENIZE:
        return cmd_tokenize(c);
    case XMQ_CLI_CMD_DELETE:
    case XMQ_CLI_CMD_DELETE_ENTITY:
        return cmd_delete(c);
    case XMQ_CLI_CMD_REPLACE:
    case XMQ_CLI_CMD_REPLACE_ENTITY:
        return cmd_replace(c);
    case XMQ_CLI_CMD_SUBSTITUTE_ENTITY:
    case XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES:
        return cmd_substitute(c);
    case XMQ_CLI_CMD_TRANSFORM:
        return cmd_transform(c);
    case XMQ_CLI_CMD_VALIDATE:
        return cmd_validate(c);
    case XMQ_CLI_CMD_SELECT:
        return cmd_select(c);
    case XMQ_CLI_CMD_FOR_EACH:
        return cmd_for_each(c);
    case XMQ_CLI_CMD_ADD:
        return cmd_add(c);
    case XMQ_CLI_CMD_ADD_ROOT:
        return cmd_add_root(c);
    case XMQ_CLI_CMD_STATISTICS:
        return cmd_statistics(c);
    case XMQ_CLI_CMD_QUOTE_C:
    case XMQ_CLI_CMD_UNQUOTE_C:
        return cmd_quote_unquote(c);
    case XMQ_CLI_CMD_HELP:
        return cmd_help(c);
    }
    assert(false);
    return false;
}

bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *load_command)
{
    int i = 1;

    // Remember if we have found a to/render/tokenize command.
    XMQCliCommand *to = NULL;
    // Remember if we have found a print/save/pager command.
    XMQCliCommand *output = NULL;

    // Handle all global options....
    for (i = 1; argv[i]; ++i)
    {
        const char *arg = argv[i];

        // Stop handling options when arg does not start with - or is exactly "-"
        if (arg[0] != '-' || !strcmp(arg, "-")) break;

        bool ok = handle_global_option(arg, load_command);
        if (ok) continue;

        printf("xmq: unrecognized global option: '%s'\nTry 'xmq --help' for more information\n", arg);
        return false;
    }

    if (argv[i])
    {
        // Check if not a command, then assume a file.
        // You cannot load files that are named like the commands.
        if (cmd_from(argv[i]) == XMQ_CLI_CMD_NONE)
        {
            // Next argument is the file name or - for reading from stdin.
            load_command->in = argv[i];
            i++;
        }
    }

    XMQCliCommand *command = load_command;
    bool do_print = true;

    if (argv[i])
    {
        command = allocate_cli_command(command->env);
        load_command->next = command;
        command->cmd = cmd_from(argv[i]);
        if (command->cmd == XMQ_CLI_CMD_NONE)
        {
            fprintf(stderr, "xmq: no such command \"%s\"\n", argv[i-1]);
            exit(1);
        }

        i++;
        prepare_command(command, load_command);

        while (command->cmd != XMQ_CLI_CMD_NONE)
        {
            verbose_("xmq=", "found command %s", cmd_name(command->cmd));

            // Now handle any command options and args.
            for (; argv[i]; ++i)
            {
                const char *arg = argv[i];
                bool ok = handle_option(arg, argv[i+1], command);

                if (!ok)
                {
                    if (cmd_from(arg) != XMQ_CLI_CMD_NONE) break; // Start next command.
                    // Else this is bad....
                    printf("xmq: option \"%s\" not available for command \"%s\"\n", arg, cmd_name(command->cmd));
                    exit(1);
                }
                verbose_("(xmq) found argument %s\n", arg);
            }

            // Check if we should remember this command as a to/render/tokenize command?
            if (cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_TO ||
                cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_RENDER ||
                cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_TOKENIZE)
            {
                if (to)
                {
                    fprintf(stderr, "xmq: you can only use one to/render/tokenize command.\n");
                    return false;
                }
                to = command;
                do_print = true;
            }

            if (cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_FOR_EACH ||
                command->cmd == XMQ_CLI_CMD_NO_OUTPUT)
            {
                // These commands by default do not need to print the output.
                do_print = false;
            }

            // Check if we should remember this command as an print/save/pager command?
            if (cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_OUTPUT)
            {
                if (output)
                {
                    fprintf(stderr, "xmq: you can only use one print/save/pager/browse/noout command.\n");
                    return false;
                }
                output = command;
            }

            // No more args to parse
            if (argv[i] == NULL) break;

            XMQCliCommand *prev = command;
            command = allocate_cli_command(command->env);
            prev->next = command;
            command->cmd = cmd_from(argv[i]);
            prepare_command(command, load_command);
            i++;
        }
    }

    if (!to && do_print)
    {
        XMQCliCommand *to = allocate_cli_command(command->env);
        if (output && output->cmd == XMQ_CLI_CMD_BROWSER)
        {
            to->cmd = XMQ_CLI_CMD_RENDER_HTML;
        }
        else
        {
            to->cmd = XMQ_CLI_CMD_TO_XMQ;
        }
        prepare_command(to, load_command);

        if (!output)
        {
            command->next = to;
            command = to;
            verbose_("xmq=", "added to-xmq command");
        }
        else
        {
            verbose_("xmq=", "inserted to-xmq command before output");
            // We have a print but no to-xmq, lets insert
            // the to-xmq before the print.
            XMQCliCommand *prev = load_command;
            while (prev && prev->next != output)
            {
                prev = prev->next;
            }
            if (prev && prev->next == output)
            {
                prev->next = to;
                to->next = output;
            }
            else
            {
                command->next = to;
            }
        }
    }

    if (!output && do_print)
    {
        command->next = allocate_cli_command(command->env);
        command->next->cmd = XMQ_CLI_CMD_PRINT;
        prepare_command(command->next, load_command);
        verbose_("xmq=", "added print command");
    }

    return true;
}

#ifdef PLATFORM_WINAPI
void enableAnsiColorsWindowsConsole()
{
    debug_("xmq=", "enable ansi colors terminal");
    debug_("xmq=", "GetStdHandle");

    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) return; // Fail

    debug_("xmq=", "GetConsoleMode");
    DWORD mode;
    if (!GetConsoleMode(hStdOut, &mode)) return; // Fail

    DWORD enabled = (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? true : false;

    debug_("xmq=", "enabled %x", enabled);

    if (enabled) return; // Already enabled colors.

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    debug_("xmq=", "SetConsoleMode %x", mode);

    SetConsoleMode(hStdOut, mode);

    debug_("xmq=", "SetConsoleOutputCP");
    SetConsoleOutputCP(CP_UTF8);

    debug_("xmq=", "Ansi done");
}

void enableRawStdinTerminal()
{
    debug_("xmq=", "enable raw stdin terminal");

    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) return; // Fail

    DWORD mode;
    if (!GetConsoleMode(handle, &mode)) return; // Fail

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    mode &= ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT);

    debug_("xmq=", "SetConsoleMode %x", mode);

    SetConsoleMode(handle, mode);

    debug_("xmq=", "raw stdin done");
}

void restoreStdinTerminal()
{
}

#else
static struct termios old_terminal, new_terminal;

/**
  enableRawStdinTerminal:

  Disable echo and enter raw mode for key presses.
*/
void enableRawStdinTerminal()
{
    tcgetattr(STDOUT_FILENO, &old_terminal);
    new_terminal = old_terminal;

    new_terminal.c_lflag &= ~(ICANON);
    new_terminal.c_lflag &= ~(ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &new_terminal);
}

/**
  restoreStdinTerminal:

  Restore stdin to cooked mode with echo.
*/
void restoreStdinTerminal()
{
    tcsetattr(STDOUT_FILENO, TCSANOW, &old_terminal);
}

#endif

bool has_verbose(int argc, const char **argv)
{

    for (const char **i = argv; *i; ++i)
    {
        if (!strcmp(*i, "--verbose")) return true;
        if (!strcmp(*i, "--debug")) return true;
    }
    return false;
}

bool has_debug(int argc, const char **argv)
{
    for (const char **i = argv; *i; ++i)
    {
        if (!strcmp(*i, "--debug")) return true;
    }
    return false;
}

bool has_trace(int argc, const char **argv)
{
    for (const char **i = argv; *i; ++i)
    {
        if (!strcmp(*i, "--trace")) return true;
    }
    return false;
}

bool has_log_xmq(int argc, const char **argv)
{
    for (const char **i = argv; *i; ++i)
    {
        if (!strcmp(*i, "--log-xmq") || !strcmp(*i, "-lx")) return true;
    }
    return false;
}

xmlDocPtr
xmqDocDefaultLoaderFunc(const xmlChar * URI,
                        xmlDictPtr dict,
                        int options,
                        void *ctxt /* ATTRIBUTE_UNUSED */,
                        xsltLoadType type /*ATTRIBUTE_UNUSED */)
{
    XMQDoc *doq = xmqNewDoc();

    verbose_("xmq=", "xsl-document-load %s", URI);

    bool ok = xmqParseFileWithType(doq,
                                   (const char*)URI,
                                   NULL,
                                   XMQ_CONTENT_DETECT,
                                   XMQ_FLAG_TRIM_NONE);
    if (!ok)
    {
        const char *error = xmqDocError(doq);
        if (error) {
            fprintf(stderr, error, URI);
        }
        xmqFreeDoc(doq);
        return NULL;
    }

    verbose_("xmq=", "xsl-document-load %zu bytes from %s", xmqGetOriginalSize(doq), URI);

    return xmqGetImplementationDoc(doq);

    /*
    xmlParserCtxtPtr pctxt;
    xmlParserInputPtr inputStream;
    xmlDocPtr doc;

    pctxt = xmlNewParserCtxt();
    if (pctxt == NULL)
        return(NULL);
    if ((dict != NULL) && (pctxt->dict != NULL)) {
        xmlDictFree(pctxt->dict);
        pctxt->dict = NULL;
    }
    if (dict != NULL) {
        pctxt->dict = dict;
        xmlDictReference(pctxt->dict);
#ifdef WITH_XSLT_DEBUG
        xsltGenericDebug(xsltGenericDebugContext,
                     "Reusing dictionary for document\n");
#endif
    }
    xmlCtxtUseOptions(pctxt, options);
    inputStream = xmlLoadExternalEntity((const char *) URI, NULL, pctxt);
    if (inputStream == NULL) {
        xmlFreeParserCtxt(pctxt);
        return(NULL);
    }
    inputPush(pctxt, inputStream);

    xmlParseDocument(pctxt);

    if (pctxt->wellFormed) {
        doc = pctxt->myDoc;
    }
    else {
        doc = NULL;
        xmlFreeDoc(pctxt->myDoc);
        pctxt->myDoc = NULL;
    }
    xmlFreeParserCtxt(pctxt);

    return(doc);
    */
}

int main(int argc, const char **argv)
{
    int rc = 0;
    if (argc < 2 && isatty(0)) cmd_help(NULL);

    verbose_enabled__ = has_verbose(argc, argv);
    debug_enabled__ = has_debug(argc, argv);
    trace_enabled__ = has_trace(argc, argv);
    log_xmq__ = has_log_xmq(argc, argv);
    xmqSetTrace(trace_enabled__);
    xmqSetDebug(debug_enabled__ || trace_enabled__);
    xmqSetVerbose(verbose_enabled__ || debug_enabled__ || trace_enabled__);
    xmqSetLogHumanReadable(!log_xmq__);

    XMQCliEnvironment env;
    memset(&env, 0, sizeof(env));

    // See if we can find from the env variables or the terminal if it is dark mode or not.
    terminal_render_theme(&env.use_color, &env.bg_dark_mode);

    if (isatty(1))
    {
#ifdef PLATFORM_WINAPI
        enableAnsiColorsWindowsConsole();
#endif
    }
    else
    {
        env.use_color = false;
        verbose_("xmq=", "using mono since output is not a tty");
    }
    XMQCliCommand *load_command = allocate_cli_command(&env);
    load_command->cmd = XMQ_CLI_CMD_LOAD;
    prepare_command(load_command, load_command);

    bool ok = xmq_parse_cmd_line(argc, argv, load_command);
    if (!ok) {
        verbose_("xmq=", "parse cmd line failed");
        return 1;
    }

    trace_enabled__ = load_command->trace;
    debug_enabled__ = load_command->debug || load_command->trace;
    verbose_enabled__ = load_command->verbose || load_command->debug || load_command->trace;
    xmqSetTrace(trace_enabled__);
    xmqSetDebug(debug_enabled__);
    xmqSetVerbose(verbose_enabled__);
    xmqSetLogHumanReadable(!log_xmq__);

    if (load_command->print_version) print_version_and_exit();
    if (load_command->print_license) print_license_and_exit();
    if (load_command->print_help) cmd_help(NULL);
    if (load_command->next && load_command->next->cmd == XMQ_CLI_CMD_HELP)
    {
        return cmd_help(load_command->next);
    }

    XMQCliCommand *c = load_command;

    // Execute commands.
    while (c)
    {
        debug_("xmq=", "performing %s", cmd_name(c->cmd));
        bool ok = perform_command(c);
        if (!ok)
        {
            rc = 1;
            break;
        }
        c = c->next;
    }

    // Free document.
    cmd_unload(load_command);

    // Free commands.
    c = load_command;
    while (c)
    {
        XMQCliCommand *tmp = c;
        c = c->next;
        free_cli_command(tmp);
    }

    if (error_to_print_on_exit) fprintf(stderr, "%s", error_to_print_on_exit);

    return rc;
}

char *load_file_into_buffer(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f)
    {
        fprintf(stderr, "xmq: %s: No such file or directory\n", file);
        return NULL;
    }
    fseek(f, 0L, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0L, SEEK_SET);
    char *buf = (char*)malloc(sz+1);
    size_t n = fread(buf, 1, sz, f);
    buf[sz] = 0;
    if (n != sz) printf("ARRRRRGGGG\n");
    fclose(f);
    return buf;
}

bool check_file_exists(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}
