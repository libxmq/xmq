/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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
#include"parts/xmq_internals.h"
#include"parts/yaep.h"
#include"xmq-cli-join.h"
#define XMQ_PARSER_NO_MAIN
#include"xmq-cli-parser.h"

#include<assert.h>
#include<ctype.h>
#include<memory.h>
#include<string.h>
#include<strings.h>
#include<stdio.h>
#include<time.h>
#include<unistd.h>
#include<sys/time.h>
#include<sys/types.h>
#ifndef PLATFORM_WINAPI
#include<sys/wait.h>
#endif
#include<sys/stat.h>
#include<fcntl.h>

#ifdef PLATFORM_WINAPI
#include<windows.h>
#include<conio.h>
//char *strndup(const char *s, size_t l);
#else
#include<signal.h>
#include<sys/ioctl.h>
#include<termios.h>
#endif


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
    XMQ_CLI_CMD_TO_YAML,
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
    XMQ_CLI_CMD_TOKENIZE,
    XMQ_CLI_CMD_DELETE,
    XMQ_CLI_CMD_DELETE_ENTITY,
    XMQ_CLI_CMD_REPLACE,
    XMQ_CLI_CMD_REPLACE_ENTITY,
    XMQ_CLI_CMD_SUBSTITUTE_ENTITY,
    XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES,
    XMQ_CLI_CMD_EXPAND_ENTITIES,
    XMQ_CLI_CMD_TRANSFORM,
    XMQ_CLI_CMD_VALIDATE,
    XMQ_CLI_CMD_SELECT,
    XMQ_CLI_CMD_FOR_EACH,
    XMQ_CLI_CMD_ADD,
    XMQ_CLI_CMD_ADD_ROOT,
    XMQ_CLI_CMD_HEU,
    XMQ_CLI_CMD_STATISTICS,
    XMQ_CLI_CMD_QUOTE_C,
    XMQ_CLI_CMD_UNQUOTE_C,
    XMQ_CLI_CMD_CHECK_LIBRARY,
    XMQ_CLI_CMD_REFRESH_LIBRARY,
    XMQ_CLI_CMD_LIST_LIBRARY,
    XMQ_CLI_CMD_DESCRIBE_LIBRARY
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
    int entity_with; // replace-entity --with-file (1) or --with-text-file (2): the content argument is a file.
    const char *content; // Content to replace something.
    const char *add_xmq; // XMQ content to add.
    XMQDoc *xslt_doq; // The xmq document loaded to generate the xslt.
    XMQXslt *xslt; // The xslt transform to be used (the native xslt 1.0).
    HashMap *xslt_params; // Parameters to the xslt transform.

    const char *xsd_name; // Name of xsd file.
    XMQDoc *xsd_doq; // The xmq document loaded to generate the xsd.
    XMQXsd *xsd; // The xsd to validate agains (the native xsd 1.0).
    const char *save_file; // Save output to this file name.
    const char *ixml_source; // IXML grammar source to be used.
    const char *ixml_engine; // If NULL use the internal, otherwise invoke this external program like this:
                             // engine source.ixml input.txt > /tmp/tmpfile
                             // then load the Document from tmpfile and continue as before.
    const char *ixml_filename; // Where the ixml grammar was read from.
    XMQDoc *ixml_grammar; // The prepared ixml grammar to be reused.
    bool ixml_fail_silent; // Generate empty document ixml parse fails. Use with --lines
    bool ixml_complete; // --complete print what could follow the input, for completion.
    const char *ixml_trace_file; // --traceixml=file writes a trace of the ixml parse to this file.
    bool ixml_trace_terminal; // --traceixml=terminal shows the trace in the terminal, saved in trace_grammar.xmq.
    bool ixml_trace_browser; // --traceixml=br shows the trace in the browser, saved in trace_grammar.xmq and .html.
    bool ixml_all_parses; // Print all possible parses when parse is ambiguous.
    bool ixml_try_to_recover; // Try to recover when parsing with an ixml grammar.
    bool build_xml_of_ixml; // Generate xml directly from ixml.
    // xmq --ixml=grammar.ixml --xml-of-ixml # This will print the grammar as xmq.
    // xmq --ixml=ixml.ixml grammar.ixml # This will print the same grammar as xmq,
    // but uses the ixml early parser instead of the hand coded parser.

    // When using ixml:data/bar or xsl:render/nginx-html
    // it usually asks if you want to download the file.
    bool auto_yes;
    bool log_xmq; // Output verbose,debug,trace as human readable lines instead of xmq.
    XmlDom     *node_doc;
    XMQNode    *node_content; // Tree content to replace something.
    XMQDoc *node_doq; // Owns node_doc (replace-entity --with-file).
    char *content_buffer; // Owns content when it was read from a file (replace-entity --with-text-file).
    const char *in_format_name; // A dynamically named format based on an ixml+xslt combo.
    // Such an format can be: xmq -f core:csv-comma input.csv to-json
    // or                     xmq --format=core:csv-comma input.csv to-json
    // or                     xmq -f measurement:metar input.metar transform view:metar
    XMQContentType in_format;
    XMQContentType out_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    const char *render_theme_spec; // Specify the theme from the cli --theme=... or XMQ_THEME=
    int  flags;
    bool use_color; // Uses color or not for terminal/html/tex
    bool bg_dark_mode; // Terminal has dark background. Specify with XMQ_BG=light or XMQ_BG=dark or --bg=light --bg=dark
    bool bg_forced; // The dark/light background mode was explicitly specified with XMQ_BG or --bg=.
    bool truecolor; // Terminal supports truecolor/24bit ansi.
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
    const char *library_name; // describe-library ixml:data/csv
    bool print_version;
    bool print_license;
    bool debug;
    bool verbose;
    bool trace;
    int  add_indent;
    bool final_newline;
    bool omit_decl; // If true, then do not print <?xml ..?>
    bool compact;
    bool prefer_double_quotes; // If true, then prefer double quotes.
    bool escape_newlines;
    bool escape_non_7bit;
    bool escape_tabs;
    const char *implicit_root;

    // If true then chip away one line at a time from the content.
    bool lines;
    // The content to be parsed one line at a time.
    const char *input_content_start;
    // The byte after the last input content byte.
    const char *input_content_stop;
    // Current line start.
    const char *input_current_line_start;
    // Current line stop, points to byte after #a (newline).
    const char *input_current_line_stop;

    // When set, force a re download of ixml/xslq from libxmq.org
    bool force_download;

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
    bool bg_forced;
    bool truecolor;
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
    size_t num_nodes_with_sqs;
    size_t num_nodes_with_dqs;
    size_t num_nodes_with_any_qs;
    size_t num_nodes_with_no_qs;
    size_t num_nodes_with_both_qs;
} Stats;

typedef enum {
    DOWNLOAD_EXISTS,
    DOWNLOAD_OK,
    DOWNLOAD_ERROR
} DownloadStatus;

// How a library file is fetched from libxmq.org into the download dir.
typedef enum {
    LIBRARY_IF_MISSING, // Download only if it is not in the library (the default).
    LIBRARY_FORCE       // --force-download: always download.
} LibraryMode;

// FUNCTION DECLARATIONS /////////////////////////////////////////////////////////////

void accumulate_attribute(Stats *stats, XMQAttr *a);
void accumulate_attribute_content(Stats *stats, XMQNode *a);
void accumulate_children(Stats *stats, XMQNode *node);
void accumulate_statistics(Stats *stats, XMQNode *node);
void add_key_value(XmlDom *doc, XMQNode *root, const char *key, size_t value);
XMQCliCommand *allocate_cli_command(XMQCliEnvironment *env);
void append_text_children(MemBuffer *buf, XMQNode *n);
void append_text_node(MemBuffer *buf, XMQNode *node);
void browse(XMQCliCommand *c);
bool check_file_exists(const char *file);
bool check_for_ixml_fail(XmlDom* doc);
bool cmd_add(XMQCliCommand *command);
bool cmd_add_root(XMQCliCommand *command);
bool cmd_delete(XMQCliCommand *command);
bool cmd_for_each(XMQCliCommand *command);
XMQCliCmd cmd_from(const char *s);
XMQCliCmdGroup cmd_group(XMQCliCmd cmd);
bool cmd_help(XMQCliCommand *c);
static bool cmd_library(bool refresh);
static bool cmd_list_library(void);
static bool cmd_describe_library(const char *name);
void load_using_internal_ixml_engine(XMQCliCommand *command, const char *from);
void load_using_external_ixml_engine(XMQCliCommand *command, const char *engine, const char *from);
void save_ixml_trace(XMQCliCommand *command);
bool cmd_load(XMQCliCommand *command, bool *no_more_data);
const char *cmd_name(XMQCliCmd cmd);
bool cmd_output(XMQCliCommand *command);
bool cmd_quote_unquote(XMQCliCommand *command);
bool cmd_replace(XMQCliCommand *command);
bool cmd_select(XMQCliCommand *command);
bool cmd_statistics(XMQCliCommand *command);
bool cmd_substitute(XMQCliCommand *command);
bool cmd_to(XMQCliCommand *command);
bool cmd_heu(XMQCliCommand *command);
bool cmd_tokenize(XMQCliCommand *command);
bool cmd_transform(XMQCliCommand *command);
bool cmd_unload(XMQCliCommand *command);
bool cmd_validate(XMQCliCommand *command);
void console_write(const char *start, const char *stop);
const char *content_type_to_string(XMQContentType ct);
size_t count_non_ansi_chars(const char *start, const char *stop);
void count_statistics(Stats *stats, XmlDom *doc);
void debug_(const char* fmt, ...);
void delete_all_entities(XMQDoc *doq, XMQNode *node, const char *entity);
void delete_entities(XMQDoc *doq, const char *entity);
bool delete_entity(XMQCliCommand *command);
bool delete_xpath(XMQCliCommand *command);
void disable_stdout_raw_input_mode();
bool dot_found(const char *arg);
DownloadStatus download(LibraryMode mode, const char *suffix, const char *file, const char *local_file, bool accept_fail);
const char *download_dir();
const char *share_dir();
void enableAnsiColorsWindowsConsole();
void enableRawStdinTerminal();
void enable_stdout_raw_input_mode();
void error_(const char* fmt, ...);
void find_next_line(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width);
void find_next_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height);
void find_prev_line(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width);
void find_prev_page(const char **line_offset, const char **in_line_offset, const  char *start, const char *stop, int width, int height);
void free_cli_command(XMQCliCommand *cmd);
int get_char();
char *grab_content(XMQNode *node, const char *name);
char *grab_name(const char *s, const char **name_start);
bool handle_global_option(const char *arg, XMQCliCommand *command);
bool handle_option(const char *arg, bool is_option, XMQCliCommand *command);
bool has_debug(int argc, const char **argv);
bool has_log_xmq(int argc, const char **argv);
bool has_trace(int argc, const char **argv);
bool has_verbose(int argc, const char **argv);
bool invoke_shell(XMQNode *n, const char *cmd);
void invoke_cmd(const char *cmd);
const char *is_ixml_cmd(const char *arg);
const char *is_xsl_cmd(const char *arg);
char *load_file_into_buffer(const char *file);
bool load_xslt(XMQCliCommand *command, const char *arg);
char *make_shell_safe_name(char *name, char *name_start);
int mkpath(char* file_path, mode_t mode);
void open_browser(const char *file);
void page(const char *start, const char *stop);
bool perform_command(XMQCliCommand *c, bool *no_more_data);
void prepare_command(XMQCliCommand *c, XMQCliCommand *load_command);
void prepare_ixml_command(XMQCliCommand *c, const char *arg);
void prepare_xsl_command(XMQCliCommand *c, const char *arg);
static void xsl_library_file(XMQCliCommand *command, const char *file, char *local_file, size_t size);
void add_xsl_command(XMQCliCommand *command, const char *local_file);
void print_command_help(XMQCliCmd c);
void print_license_and_exit();
void print_version_and_exit();
void put_char(int c);
bool query_xterm_bgcolor();
KEY read_key(int *c);
const char *render_format_to_string(XMQRenderFormat rt);
int replace_xpath_content(XMQDoc *doq, const char *xpath, const char *content);
void replace_entities(XmlDom *doc, const char *entity, const char *content, XMQNode *node_content);
XMQNode *replace_entity(XmlDom *doc, XMQNode *node, const char *entity, const char *content, XMQNode *node_content);
void restoreStdinTerminal();
bool shell_safe(char *i);
const char *skip_ansi_backwards(const char *i, const char *start);
void substitute_entity(XmlDom *doc, XMQNode* node, const char *entity, bool only_chars);
bool detect_truecolor();
void lookup_bg(bool *use_color, bool *bg_dark_mode, bool *bg_forced);
XMQColorMode compute_color_mode(bool use_color, bool bg_dark_mode, bool bg_forced, XMQRenderFormat render_to);
const char *lookup_theme_spec();
const char *tokenize_type_to_string(XMQCliTokenizeType type);
void trace_(const char* fmt, ...);
void verbose_(const char* fmt, ...);
bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *command);

/////////////////////////////////////////////////////////////////////////////////////

const char *error_to_print_on_exit = NULL;

XMQLineConfig xmq_log_line_config__;

bool log_xmq__ = false;
// Without --we-should-stop-using-yaml every to-yaml output starts with a reminder comment.
bool we_should_stop_using_yaml__ = false;

const char *default_theme_spec_ = NULL;

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
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list ap;
            va_start(ap, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, ap);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(ap);
        }
    }
}

bool debug_enabled__ = false;

void debug_(const char* fmt, ...)
{
    if (debug_enabled__)
    {
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list args;
            va_start(args, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(args);
        }
    }
}

bool trace_enabled__ = false;

void trace_(const char* fmt, ...)
{
    if (trace_enabled__)
    {
        if (!xmq_log_filter_ || !strncmp(fmt, xmq_log_filter_, strlen(xmq_log_filter_)))
        {
            va_list args;
            va_start(args, fmt);
            char *line = xmqLineVPrintf(&xmq_log_line_config_, fmt, args);
            fprintf(stderr, "%s\n", line);
            free(line);
            va_end(args);
        }
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
    if (!strcmp(s, "check-library")) return XMQ_CLI_CMD_CHECK_LIBRARY;
    if (!strcmp(s, "refresh-library")) return XMQ_CLI_CMD_REFRESH_LIBRARY;
    if (!strcmp(s, "list-library")) return XMQ_CLI_CMD_LIST_LIBRARY;
    if (!strcmp(s, "describe-library")) return XMQ_CLI_CMD_DESCRIBE_LIBRARY;
    if (!strcmp(s, "load")) return XMQ_CLI_CMD_LOAD;
    if (!strcmp(s, "no-output")) return XMQ_CLI_CMD_NO_OUTPUT;
    if (!strcmp(s, "pa")) return XMQ_CLI_CMD_PAGER;
    if (!strcmp(s, "page")) return XMQ_CLI_CMD_PAGER;
    if (!strcmp(s, "print")) return XMQ_CLI_CMD_PRINT;
    if (!strcmp(s, "render-html")) return XMQ_CLI_CMD_RENDER_HTML;
    if (!strcmp(s, "render-terminal")) return XMQ_CLI_CMD_RENDER_TERMINAL;
    if (!strcmp(s, "render-tex")) return XMQ_CLI_CMD_RENDER_TEX;
    if (!strcmp(s, "replace")) return XMQ_CLI_CMD_REPLACE;
    if (!strcmp(s, "replace-entity")) return XMQ_CLI_CMD_REPLACE_ENTITY;
    if (!strcmp(s, "save-to")) return XMQ_CLI_CMD_SAVE_TO;
    if (!strcmp(s, "select")) return XMQ_CLI_CMD_SELECT;
    if (!strcmp(s, "statistics")) return XMQ_CLI_CMD_STATISTICS;
    if (!strcmp(s, "substitute-char-entities")) return XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES;
    if (!strcmp(s, "expand-entities")) return XMQ_CLI_CMD_EXPAND_ENTITIES;
    if (!strcmp(s, "substitute-entity")) return XMQ_CLI_CMD_SUBSTITUTE_ENTITY;
    if (!strcmp(s, "to-html")) return XMQ_CLI_CMD_TO_HTML;
    if (!strcmp(s, "to-htmq")) return XMQ_CLI_CMD_TO_HTMQ;
    if (!strcmp(s, "to-json")) return XMQ_CLI_CMD_TO_JSON;
    if (!strcmp(s, "to-yaml")) return XMQ_CLI_CMD_TO_YAML;
    if (!strcmp(s, "to-text")) return XMQ_CLI_CMD_TO_TEXT;
    if (!strcmp(s, "to-xml")) return XMQ_CLI_CMD_TO_XML;
    if (!strcmp(s, "to-xmq")) return XMQ_CLI_CMD_TO_XMQ;
    if (!strcmp(s, "to-clines")) return XMQ_CLI_CMD_TO_CLINES;
    if (!strcmp(s, "tokenize")) return XMQ_CLI_CMD_TOKENIZE;
    if (!strcmp(s, "transform")) return XMQ_CLI_CMD_TRANSFORM;
    if (!strcmp(s, "validate")) return XMQ_CLI_CMD_VALIDATE;
    if (!strcmp(s, "quote-c")) return XMQ_CLI_CMD_QUOTE_C;
    if (!strcmp(s, "unquote-c")) return XMQ_CLI_CMD_UNQUOTE_C;
    if (!strcmp(s, "heu")) return XMQ_CLI_CMD_HEU;

    return XMQ_CLI_CMD_NONE;
}

const char *cmd_name(XMQCliCmd cmd)
{
    switch (cmd)
    {
    case XMQ_CLI_CMD_NONE: return "noop";
    case XMQ_CLI_CMD_HELP: return "help";
    case XMQ_CLI_CMD_CHECK_LIBRARY: return "check-library";
    case XMQ_CLI_CMD_REFRESH_LIBRARY: return "refresh-library";
    case XMQ_CLI_CMD_LIST_LIBRARY: return "list-library";
    case XMQ_CLI_CMD_DESCRIBE_LIBRARY: return "describe-library";
    case XMQ_CLI_CMD_LOAD: return "load";
    case XMQ_CLI_CMD_TO_XMQ: return "to-xmq";
    case XMQ_CLI_CMD_TO_XML: return "to-xml";
    case XMQ_CLI_CMD_TO_HTMQ: return "to-htmq";
    case XMQ_CLI_CMD_TO_HTML: return "to-html";
    case XMQ_CLI_CMD_TO_JSON: return "to-json";
    case XMQ_CLI_CMD_TO_YAML: return "to-yaml";
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
    case XMQ_CLI_CMD_TOKENIZE: return "tokenize";
    case XMQ_CLI_CMD_DELETE: return "delete";
    case XMQ_CLI_CMD_DELETE_ENTITY: return "delete-entity";
    case XMQ_CLI_CMD_REPLACE: return "replace";
    case XMQ_CLI_CMD_REPLACE_ENTITY: return "replace-entity";
    case XMQ_CLI_CMD_SUBSTITUTE_ENTITY: return "substitute-entity";
    case XMQ_CLI_CMD_SUBSTITUTE_CHAR_ENTITIES: return "substitute-char-entities";
    case XMQ_CLI_CMD_EXPAND_ENTITIES: return "expand-entities";
    case XMQ_CLI_CMD_TRANSFORM: return "transform";
    case XMQ_CLI_CMD_VALIDATE: return "validate";
    case XMQ_CLI_CMD_SELECT: return "select";
    case XMQ_CLI_CMD_FOR_EACH: return "for-each";
    case XMQ_CLI_CMD_ADD: return "add";
    case XMQ_CLI_CMD_ADD_ROOT: return "add-root";
    case XMQ_CLI_CMD_HEU: return "heu";
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
    case XMQ_CLI_CMD_TO_YAML:
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
    case XMQ_CLI_CMD_EXPAND_ENTITIES:
        return XMQ_CLI_CMD_GROUP_SUBSTITUTE;

    case XMQ_CLI_CMD_TRANSFORM:
        return XMQ_CLI_CMD_GROUP_TRANSFORM;

    case XMQ_CLI_CMD_QUOTE_C:
    case XMQ_CLI_CMD_UNQUOTE_C:
        return XMQ_CLI_CMD_GROUP_QUOTE;

    case XMQ_CLI_CMD_VALIDATE:
        return XMQ_CLI_CMD_GROUP_VALIDATE;

    case XMQ_CLI_CMD_NONE:
    case XMQ_CLI_CMD_HEU:
    case XMQ_CLI_CMD_STATISTICS:
        return XMQ_CLI_CMD_GROUP_NONE;

    case XMQ_CLI_CMD_HELP:
        return XMQ_CLI_CMD_GROUP_HELP;

    case XMQ_CLI_CMD_CHECK_LIBRARY:
    case XMQ_CLI_CMD_REFRESH_LIBRARY:
    case XMQ_CLI_CMD_LIST_LIBRARY:
    case XMQ_CLI_CMD_DESCRIBE_LIBRARY:
        return XMQ_CLI_CMD_GROUP_NONE;
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
    case XMQ_CONTENT_YAML: return "yaml";
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
    c->bg_forced = env->bg_forced;
    c->truecolor = env->truecolor;
    c->env = env;
    c->cmd = XMQ_CLI_CMD_TO_XMQ;
    c->in_format = XMQ_CONTENT_DETECT;
    c->out_format = XMQ_CONTENT_XMQ;
    if (c->use_color)
    {
        c->render_to = XMQ_RENDER_TERMINAL;
        c->render_theme_spec = default_theme_spec_;
    }
    else
    {
        c->render_to = XMQ_RENDER_PLAIN;
    }
    c->add_indent = 4;
    c->compact = false;
    c->escape_tabs = false;
    c->final_newline = true;
    return c;
}

void free_cli_command(XMQCliCommand *cmd)
{
    if (cmd->xslt_params)
    {
        hashmap_free_and_values(cmd->xslt_params, free);
        cmd->xslt_params = NULL;
    }
    if (cmd->xslt)
    {
        xmqFreeXslt(cmd->xslt);
        xmqFreeDoc(cmd->xslt_doq);
        cmd->xslt = NULL;
    }
    if (cmd->xsd)
    {
        xmqFreeXsd(cmd->xsd);
        cmd->xsd = NULL;
    }
    xmqFreeDoc(cmd->node_doq);
    cmd->node_doq = NULL;
    free(cmd->content_buffer);
    cmd->content_buffer = NULL;
    // Input that cmd_unload did not get to free, eg after a parse error.
    if (cmd->input_content_start)
    {
        free((char*)cmd->input_content_start);
        cmd->input_content_start = NULL;
    }
    if (cmd->ixml_grammar)
    {
        xmqFreeDoc(cmd->ixml_grammar);
        cmd->ixml_grammar = NULL;
    }
    free((char*)cmd->ixml_filename);
    cmd->ixml_filename = NULL;
    free((char*)cmd->ixml_source);
    cmd->ixml_source = NULL;

    free(cmd);
}

// The messages of the xsd compilation and validation, printed like the libxml2 error
// callbacks did (a 4096 byte buffer, puts of the message).

static void cli_xsd_put(const char *message)
{
    char buf[4096];
    snprintf(buf, 4096, "%s", message);
    puts(buf);
}

/** The schema cannot be parsed: print the first error and exit. */
static void cli_xsd_parse_message(void *ctx, const char *message, bool is_warning)
{
    if (is_warning)
    {
        fprintf(stderr, "xmq: %s parse warning\n", (const char*)ctx);
        cli_xsd_put(message);
        return;
    }
    fprintf(stderr, "xmq: %s parse error\n", (const char*)ctx);
    cli_xsd_put(message);
    exit(1);
}

/** The document is not valid: print the first error and exit. */
static void cli_xsd_validate_message(void *ctx, const char *message, bool is_warning)
{
    if (is_warning)
    {
        printf("xmq: validation warning\n");
        cli_xsd_put(message);
        return;
    }
    printf("xmq: Document cannot be validated against %s\n", (const char*)ctx);
    cli_xsd_put(message);
    exit(1);
}

/** --silent: exit at the first error without a message. */
static void cli_xsd_silent_message(void *ctx, const char *message, bool is_warning)
{
    (void)ctx;
    (void)message;
    if (!is_warning) exit(1);
}

static bool load_entity_file(XMQCliCommand *command, const char *file)
{
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
    bool ignore_no_more_data = false;
    bool ok = cmd_load(&cmd, &ignore_no_more_data);
    // The loaded input buffer is not needed after the parse.
    free((char*)cmd.input_content_start);
    cmd.input_content_start = NULL;
    if (!ok)
    {
        xmqFreeDoc(cmd.env->doc);
        return false;
    }
    // The command keeps the loaded document, it is freed in free_cli_command.
    command->node_doq = cmd.env->doc;
    XmlDom *doc = xmq_get_dom(command->node_doq);
    command->node_doc = doc;
    command->node_content = xml_node_first_child(xml_dom_document(doc));
    return true;
}

static bool load_entity_text_file(XMQCliCommand *command, const char *file)
{
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
    command->content_buffer = buf;
    command->content = buf;
    return true;
}

bool handle_option(const char *arg, bool is_option, XMQCliCommand *command)
{
    if (!arg) return false;

    XMQCliCmdGroup group = cmd_group(command->cmd);

    if (command->cmd == XMQ_CLI_CMD_SAVE_TO && !is_option)
    {
        if (command->save_file == NULL)
        {
            command->save_file = arg;
            return true;
        }
    }

    if (command->cmd == XMQ_CLI_CMD_DESCRIBE_LIBRARY && !is_option && command->library_name == NULL)
    {
        command->library_name = arg;
        return true;
    }

    if (command->cmd == XMQ_CLI_CMD_HELP && !is_option)
    {
        command->help_command = arg;
        return true;
    }

    if (group == XMQ_CLI_CMD_GROUP_TO ||
        group == XMQ_CLI_CMD_GROUP_RENDER)
    {
        if (!strcmp(arg, "-n") ||
            !strcmp(arg, "--no-final-nl"))
        {
            command->final_newline = false;
            return true;
        }
        if (!strcmp(arg, "-c") ||
            !strcmp(arg, "--compact"))
        {
            command->add_indent = 0;
            command->compact = true;
            return true;
        }
        if (!strcmp(arg, "-p") ||
            !strcmp(arg, "--pretty"))
        {
            if (command->cmd == XMQ_CLI_CMD_TO_JSON ||
                command->cmd == XMQ_CLI_CMD_TO_YAML)
            {
                command->add_indent = 2;
            }
            else
            {
                command->add_indent = 4;
            }
            command->compact = false;
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
        command->cmd == XMQ_CLI_CMD_TO_YAML ||
        group == XMQ_CLI_CMD_GROUP_RENDER)
    {
        if (!strcmp(arg, "--escape-newlines"))
        {
            command->escape_newlines = true;
            return true;
        }
        if (!strcmp(arg, "--prefer-double-quotes"))
        {
            command->prefer_double_quotes = true;
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
        if (!strncmp(arg, "--id=", 5))
        {
            command->use_id = arg+5;
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
            command->render_theme_spec = arg+8;
            return true;
        }
        if (!strncmp(arg, "--bg=", 5))
        {
            const char *bg = arg+5;
            if (!strcmp(bg, "light"))
            {
                command->use_color = true;
                command->bg_dark_mode = false;
                command->bg_forced = true;
            }
            else if (!strcmp(bg, "dark"))
            {
                command->use_color = true;
                command->bg_dark_mode = true;
                command->bg_forced = true;
            }
            else if (!strcmp(bg, "mono"))
            {
                command->use_color = false;
                command->bg_dark_mode = false;
            }
            else
            {
                printf("xmq: unknown bg setting \"%s\"\n", bg);
                exit(1);
            }
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

    if (group == XMQ_CLI_CMD_GROUP_XPATH && !is_option)
    {
        if (command->xpath == NULL)
        {
            command->xpath = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_ADD_XMQ && !is_option)
    {
        if (command->add_xmq == NULL)
        {
            command->add_xmq = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_FOR_EACH && !is_option)
    {
        if (command->xpath == NULL)
        {
            command->xpath = arg;
            return true;
        }
    }

    if (group == XMQ_CLI_CMD_GROUP_SUBSTITUTE)
    {
        if (!is_option && cmd_from(arg) == XMQ_CLI_CMD_NONE && command->entity == NULL)
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
        // replace-entity --with-file NAME FILE parses the file into the entity content,
        // replace-entity --with-text-file NAME FILE uses the file as text.
        if (is_option && !strcmp(arg, "--with-file"))
        {
            command->entity_with = 1;
            return true;
        }
        if (is_option && !strcmp(arg, "--with-text-file"))
        {
            command->entity_with = 2;
            return true;
        }
        // The old forms --with-file=FILE and --with-text-file=FILE.
        if (!strncmp(arg, "--with-file=", 12)) return load_entity_file(command, arg+12);
        if (!strncmp(arg, "--with-text-file=", 17)) return load_entity_text_file(command, arg+17);

        if (!is_option && command->entity == NULL)
        {
            command->entity = arg;
            return true;
        }

        if (!is_option && group == XMQ_CLI_CMD_GROUP_ENTITY && command->content == NULL && command->node_content == NULL)
        {
            if (command->entity_with == 1) return load_entity_file(command, arg);
            if (command->entity_with == 2) return load_entity_text_file(command, arg);
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
        if (!is_option && command->xslt == NULL)
        {
            // transform xsl:data/table-to-html uses the stylesheet from the library.
            char library_file[1024];
            if (is_xsl_cmd(arg))
            {
                xsl_library_file(command, is_xsl_cmd(arg), library_file, sizeof(library_file));
                arg = library_file;
            }
            bool ok = load_xslt(command, arg);
            if (!ok) return false;
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
        if (!is_option && command->in == NULL)
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
        if (!is_option && command->xsd == NULL)
        {
            XMQReturnDoc rd = xmqNewDoc();
            assert(rd.status == XMQ_OK);
            XMQDoc *doq = rd.doc;
            bool ok = xmqParseFileWithType(doq, arg, NULL, XMQ_CONTENT_DETECT, XMQ_FLAG_TRIM_NONE);

            if (!ok)
            {
                const char *error = xmqDocError(doq);
                fprintf(stderr, "%s\n", error ? error : "xmq: parse error");
                xmqFreeDoc(doq);
                return false;
            }

            command->xsd_name = arg;
            command->xsd_doq = doq;

            verbose_("xmq=", "loaded xsd %zu bytes from %s", xmqGetOriginalSize(doq), arg);

            XMQXsdSettings settings;
            memset(&settings, 0, sizeof(settings));
            settings.message_callback = cli_xsd_parse_message;
            settings.message_user_data = (void*)arg;
            XMQReturnXsd rx = xmqCompileXsd(doq, &settings);
            if (rx.status == XMQ_ERROR_EXCLUDED_FROM_BUILD)
            {
                fprintf(stderr, "%s", xmqDocError(doq));
                exit(1);
            }
            command->xsd = rx.xsd;
            return true;
        }
    }

    return false;
}


int mkpath(char *file_path, mode_t mode)
{
    assert(file_path && *file_path);

    for (char* p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/'))
    {
        *p = '\0';
#ifdef PLATFORM_WINAPI
        int rc = mkdir(file_path);
#else
        int rc = mkdir(file_path, mode);
#endif
        if (rc == -1)
        {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

char download_dir_[256] = {0};

const char *download_dir()
{
    if (download_dir_[0]) return download_dir_;

#ifdef PLATFORM_WINAPI
    const char *localappdata = getenv("LOCALAPPDATA");
    snprintf(download_dir_, 256, "%s/xmq", localappdata);
#else
    const char *home = getenv("HOME");
    snprintf(download_dir_, 256, "%s/.local/share/xmq", home);
#endif

    return download_dir_;
}

char share_dir_[600] = {0};

/** Where make install puts the library: share/xmq next to the bin directory of this xmq
    binary, eg /usr/local/share/xmq for /usr/local/bin/xmq. */
const char *share_dir()
{
    if (share_dir_[0]) return share_dir_;

#ifdef PLATFORM_WINAPI
    const char *programfiles = getenv("PROGRAMFILES");
    snprintf(share_dir_, sizeof(share_dir_), "%s/xmq", programfiles);
#else
    snprintf(share_dir_, sizeof(share_dir_), "/usr/local/share/xmq");
#ifdef __linux__
    char exe[512];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe)-1);
    if (n > 0)
    {
        exe[n] = 0;
        char *slash = strrchr(exe, '/');
        if (slash)
        {
            *slash = 0; // The bin directory.
            snprintf(share_dir_, sizeof(share_dir_), "%s/../share/xmq", exe);
        }
    }
#endif
#endif

    return share_dir_;
}

/** Find a library file, eg xmq-utils/render-trace-to-terminal with the suffix .xslq. In order:
    $XMQ_LIBRARY (eg the library of the xmq source), the download dir ~/.local/share/xmq/library
    (files downloaded on demand or with xmq refresh-library), and the library that make install
    puts in share/xmq/library. $XMQ_INSTALLED_LIBRARY replaces the installed library (eg for an
    install elsewhere, or none at all in the tests). Returns false if it is in none of them, then
    the file is downloaded. */
static bool find_library_file(const char *file, const char *suffix, char *out, size_t size)
{
    const char *env = getenv("XMQ_LIBRARY");
    const char *installed = getenv("XMQ_INSTALLED_LIBRARY");
    const char *dirs[] = { env, download_dir(), share_dir(), "/usr/local/share/xmq", "/usr/share/xmq" };
    for (int i = 0; i < 5; i++)
    {
        if (i == 0)
        {
            if (!env || !*env) continue;
            snprintf(out, size, "%s/%s%s", env, file, suffix);
        }
        else if (i >= 2 && installed)
        {
            if (i > 2 || !*installed) continue;
            snprintf(out, size, "%s/%s%s", installed, file, suffix);
        }
        else
        {
            snprintf(out, size, "%s/library/%s%s", dirs[i], file, suffix);
        }
        FILE *f = fopen(out, "rb");
        if (f)
        {
            fclose(f);
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
    }
    assert(false);
    return "?";
}

#ifndef PLATFORM_WINAPI

static void sig_alarm_handler(int signo)
{
    fprintf(stderr,
            "xmq: no response from xterm/xterm-256color terminal whether background is dark/light.\n"
            "To skip this failed test please set environment variable XMQ_BG=mono|dark|light.\n");
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
        ssize_t n = read(STDOUT_FILENO, &c, sizeof(char));
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

static bool contains(const char *s, const char *sub)
{
    return s && strstr(s, sub);
}

static bool equals(const char *s, const char *v)
{
    return s && strcmp(s, v) == 0;
}

bool detect_truecolor()
{
    const char *override = getenv("XMQ_TERM");

    if (override)
    {
        if (equals(override, "truecolor"))
        {
            verbose_("xmq=", "force truecolor terminal using XMQ_TERM=truecolor");
            return true;
        }
        verbose_("xmq=", "force 256color terminal using XMQ_TERM=%s", override);
        return false;
    }

    const char *colorterm = getenv("COLORTERM");

    // Strong indicators
    if (contains(colorterm, "truecolor") || contains(colorterm, "24bit"))
    {
        verbose_("xmq=", "force truecolor terminal using COLORTERM=%s", colorterm);
        return true;
    }

    const char *term = getenv("TERM");
    const char *term_prog = getenv("TERM_PROGRAM");
    const char *wt = getenv("WT_SESSION");
    const char *konsole = getenv("KONSOLE_PROFILE_NAME");

    // Windows Terminal
    if (wt) {
        verbose_("xmq=", "force truecolor windows terminal WT_SESSION non-empty");
        return true;
    }

    // iTerm2
    if (equals(term_prog, "iTerm.app"))
    {
        verbose_("xmq=", "force truecolor using iTerm.app");
        return true;
    }

    // Apple Terminal does NOT support 24 bit color.
    if (equals(term_prog, "Apple_Terminal"))
    {
        verbose_("xmq=", "force 256color using Apple_Terminal");
        return false;
    }

    // Known truecolor terminals
    if (contains(term, "xterm-truecolor"))
    {
        verbose_("xmq=", "force truecolor TERM contains xterm-truecolor");
        return true;
    }
    if (contains(term, "tmux-truecolor"))
    {
        verbose_("xmq=", "force truecolor TERM contains tmux-truecolor");
        return true;
    }
    if (contains(term, "screen-truecolor"))
    {
        verbose_("xmq=", "force truecolor TERM contains screen-truecolor");
        return true;
    }

    // Konsole
    if (konsole)
    {
        verbose_("xmq=", "force truecolor konsole KONSOLE_PROFILE_NAME non-empty");
        return true;
    }

    // Many modern terminals set xterm-256color but still support truecolor
    if (contains(term, "xterm") ||
        contains(term, "screen") ||
        contains(term, "tmux") ||
        contains(term, "rxvt") ||
        contains(term, "alacritty") ||
        contains(term, "kitty"))
    {
        verbose_("xmq=", "force truecolor TERM=%s", term);
        return true;
    }

    verbose_("xmq=", "fall back on 256color terminal");
    return false;
}

XMQColorMode compute_color_mode(bool use_color, bool bg_dark_mode, bool bg_forced, XMQRenderFormat render_to)
{
    if (bg_forced) return bg_dark_mode ? XMQ_BG_DARK : XMQ_BG_LIGHT;
    if (!use_color) return XMQ_MONO;
    // Unforced background: for terminal/tex output use the background detected by
    // the cli (bg_dark_mode), for html output let the browser decide.
    if (render_to == XMQ_RENDER_HTML) return XMQ_BG_AUTO;
    return bg_dark_mode ? XMQ_BG_DARK : XMQ_BG_LIGHT;
}

void lookup_bg(bool *use_color, bool *bg_dark_mode, bool *bg_forced)
{
    const char *term = getenv("TERM");
    if (!term) term = "NULL";
    verbose_("xmq=", "detected terminal %s", term);

    *bg_forced = false;

    char *bg = getenv("XMQ_BG");
    if (bg != NULL)
    {
        if (!strcmp(bg, "mono"))
        {
            *use_color = false;
            *bg_dark_mode = false;
            verbose_("xmq=", "XMQ_BG set to mono");
            return;
        }
        *use_color = true;
        if (!strcmp(bg, "light"))
        {
            *bg_dark_mode = false;
            *bg_forced = true;
            verbose_("xmq=", "XMQ_BG set to light");
            return;
        }
        if (!strcmp(bg, "dark"))
        {
            *bg_dark_mode = true;
            *bg_forced = true;
            verbose_("xmq=", "XMQ_BG set to dark");
            return;
        }
        *use_color = false;
        *bg_dark_mode = false;
        verbose_("xmq=", "XMQ_BG content is bad, using mono");
        return;
    }

    if (!strcmp(term, "linux"))
    {
        // The Linux vt console is by default black. So dark-mode.
        *use_color = true;
        *bg_dark_mode = true;
        verbose_("xmq=", "assuming vt console has dark bg");
        return;
    }

#ifdef PLATFORM_WINAPI
    *use_color = true;
    *bg_dark_mode = true;
    verbose_("xmq=", "assuming console has dark background");
    return;
#else

    if (!strcmp(term, "xterm-256color") ||
        !strcmp(term, "xterm"))
    {
        if (!isatty(1))
        {
            *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "cannot query xterm assuming console has dark background");
            return;
        }

        bool is_dark = true;
        is_dark = query_xterm_bgcolor();

        if (is_dark)
        {
            *use_color = true;
            *bg_dark_mode = true;
            verbose_("xmq=", "terminal responds with dark background");
            return;
        }
        *use_color = true;
        *bg_dark_mode = false;
        verbose_("xmq=", "terminal responds with light background");
        return;
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
            return;
	}
	*use_color = true;
        *bg_dark_mode = false;
        verbose_("xmq=", "COLORFGBG means light \"%s\"", colorfgbg);
        return;
    }

    *use_color = true;
    *bg_dark_mode = true;
    verbose_("xmq=", "unknown terminal \"%s\" defaults to colors and dark background", term);
    return;
}


const char *lookup_theme_spec()
{
    return getenv("XMQ_THEME");
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
    if (!strcmp(arg, "-z") || !strcmp(arg, "--no-input"))
    {
        command->no_input = true;
        return true;
    }
    if (!strcmp(arg, "-y") || !strcmp(arg, "--yes"))
    {
        command->auto_yes = true;
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
    if (!strncmp(arg, "--trace=", 8))
    {
        command->trace = true;
        xmqLogFilter(arg+8);
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
    if (!strncmp(arg, "--debug=", 8))
    {
        command->debug = true;
        xmqLogFilter(arg+8);
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
    if (!strncmp(arg, "--verbose=", 9))
    {
        command->verbose = true;
        xmqLogFilter(arg+10);
        verbose_enabled__ = true;
        return true;
    }
    if (!strcmp(arg, "--progress"))
    {
        xmqSetProgress(true);
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
    if (!strcmp(arg, "--features"))
    {
        // The optional parts compiled in, eg "xmq-printing json yaml xslt xsd ixml html".
        const char *f = xmqFeatures();
        while (*f == ' ') f++;
        printf("%s\n", f);
        exit(0);
    }
    if (!strcmp(arg, "--check-bg-dark-light"))
    {
        bool color, dark, forced;
        lookup_bg(&color, &dark, &forced);
        (void)forced;
        if (error_to_print_on_exit) fprintf(stderr, "%s", error_to_print_on_exit);
        if (!color) exit(0); // Mono
        if (dark) exit(1); // Dark background
        exit(2); // Light background
    }
    if (!strcmp(arg, "--check-truecolor"))
    {
        bool tc = detect_truecolor();
        if (tc) exit(0); // True color
        exit(1); // Not true color.
    }
    if (!strcmp(arg, "--xmq") || !strcmp(arg, "--htmq"))
    {
        // htmq is xmq (the html flavour of it), it is read by the xmq parser.
        command->in_format=XMQ_CONTENT_XMQ;
        return true;
    }
    if (!strcmp(arg, "--json"))
    {
        command->in_format=XMQ_CONTENT_JSON;
        return true;
    }
    if (!strcmp(arg, "--yaml"))
    {
        command->in_format=XMQ_CONTENT_YAML;
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
    if (!strcmp(arg, "--ixml-fail-silent"))
    {
        command->ixml_fail_silent = true;
        return true;
    }
    if (!strncmp(arg, "--traceixml=", 12))
    {
        // A file ending in .xmq .xml or .json, terminal to show the trace in the terminal
        // or br to show it in the browser.
        const char *value = arg+12;
        size_t len = strlen(value);
        bool is_file = (len > 4 && (!strcmp(value+len-4, ".xmq") || !strcmp(value+len-4, ".xml"))) ||
                       (len > 5 && !strcmp(value+len-5, ".json"));
        if (!is_file && strcmp(value, "terminal") && strcmp(value, "br"))
        {
            fprintf(stderr, "xmq: --traceixml= takes a file ending in .xmq .xml or .json, terminal or br\n");
            exit(1);
        }
        command->ixml_trace_file = value;
        command->ixml_trace_terminal = !strcmp(value, "terminal");
        command->ixml_trace_browser = !strcmp(value, "br");
        return true;
    }
    if (!strcmp(arg, "--complete"))
    {
        command->ixml_complete = true;
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
    if (!strcmp(arg, "--we-should-stop-using-yaml"))
    {
        we_should_stop_using_yaml__ = true;
        return true;
    }
    if (!strcmp(arg, "--no-merge") || !strcmp(arg, "--nomerge"))
    {
        command->flags |= XMQ_FLAG_NOMERGE;
        return true;
    }
    if (!strcmp(arg, "--force-download"))
    {
        command->force_download = true;
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
    if (!strncmp(arg, "--ixml-engine=", 14))
    {
        const char *engine = arg+14;

        if (command->ixml_engine != NULL)
        {
            printf("You have already specified an ixml engine!\n");
            exit(1);
        }

        command->ixml_engine = strdup(engine);
        return true;
    }

    if (!strncmp(arg, "--ixml=", 7))
    {
        const char *file = arg+7;

        if (command->ixml_source != NULL)
        {
            printf("You have already specified an ixml grammar!\n");
            exit(1);
        }

        // g.ixml is an ixml grammar, the others are grammars in xml form, see load_using_internal_ixml_engine.
        // (Not yaml, the yaml mapping of xmq does not keep the attributes.)
        size_t len = strlen(file);
        bool ok = (len > 5 && !strcmp(file+len-5, ".ixml")) ||
            (len > 4 && (!strcmp(file+len-4, ".xml") || !strcmp(file+len-4, ".xmq"))) ||
            (len > 5 && !strcmp(file+len-5, ".json"));
        if (!ok)
        {
            fprintf(stderr, "For ixml you can specify: g.ixml g.xml g.xmq g.json\n");
            exit(1);
        }
        verbose_("xmq=", "reading ixml grammar %s", file);
        command->ixml_filename = strdup(file);
        command->ixml_source = load_file_into_buffer(file);
        if (command->ixml_source == NULL) exit(1);

        return true;
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
           "  --force-download Always download the library files used (ixml:name xsl:name) from libxmq.org.\n"
           "  --help     Display this help and exit.\n"
           "  --license  Print license.\n"
           "  --lines    Assume each input line is a separate document.\n"
           "  --no-merge When loading xmq do not merge text quotes and character entities.\n"
           "  --we-should-stop-using-yaml\n"
           "             Do not start the to-yaml output with the comment # we should stop using yaml\n"
           "  --root=<name>\n"
           "             Create a root node <name> unless the file starts with a node with this <name> already.\n"
           "  --trim=none|heuristic|exact\n"
           "             The default setting when reading xml/html content is to trim whitespace using a heuristic.\n"
           "             For xmq/htmq/json the default settings is none since whitespace is explicit in xmq/htmq/json.\n"
           "             Not yet implemented: exact will trim exactly to the significant whitespace according to xml/html rules.\n"
           "  --progress Print parse progress on stderr when parsing with an ixml grammar.\n"
           "  --verbose  Output extra information on stderr.\n"
           "  --version  Output version information and exit.\n"
           "  --features Print the optional parts compiled in: xmq-printing json yaml xslt xsd ixml html.\n"
           "  --xmq|--htmq|--xml|--html|--ixml|--json|--yaml|--clines\n"
           "             The input format is auto detected for xmq/xml/json but you can force the input format here.\n"
           "  --ixml=grammar.ixml Parse the content using the supplied grammar file.\n"
           "             The grammar can also be in xml form, ie the grammar parsed with the ixml grammar:\n"
           "             --ixml=grammar.xml or grammar.xmq or grammar.json\n"
           "  --complete Print what could follow the input parsed with the ixml grammar, one per line:\n"
           "             string|char|charset|end <tab> rule <tab> text, eg string verb gives\n"
           "  --traceixml=trace.xmq Write a trace of the ixml parse (rules, Earley items, tree) to the file (.xmq .xml or .json),\n"
           "             render it with xsl:xmq-utils/render-trace-to-html, -to-tex or -to-terminal.\n"
           "  --traceixml=terminal Show the trace in the terminal, it is saved in trace_grammar.xmq.\n"
           "  --traceixml=br Show the trace in the browser, it is saved in trace_grammar.xmq and trace_grammar.html.\n"
           "  -z         Do not read from stdin nor from a file. Start with an empty dom.\n"
           "  -i \"a=2\" Do not read from a file, use the next argument as the content to parse.\n"
           "\n"
           "To get help on the commands below: xmq help <command>\n\n"
           "COMMANDS\n"
           "  add\n"
           "  add-root\n"
           "  browser pager\n"
           "  check-library refresh-library list-library describe-library\n"
           "  delete delete-entity\n"
           "  for-each\n"
           "  help\n"
           "  heu\n"
           "  no-output\n"
           "  render-html render-terminal render-tex\n"
           "  replace replace-entity\n"
           "  quote-c unquote-c\n"
           "  select\n"
           "  statistics\n"
           "  expand-entities substitute-char-entities substitute-entity\n"
           "  to-html to-htmq to-json to-lines to-text to-xmq to-yaml\n"
           "  tokenize\n"
           "  transform\n"
           "  validate\n\n"
           "EXAMPLES\n"
           "  xmq pom.xml page\n"
           "  xmq index.html delete //script delete //style browse\n"
           "  xmq template.htmq replace-entity DATE 2024-02-08 to-html > index.html\n"
           "  xmq data.json select /_/work transform format.xslq to-xml > data.xml\n"
           "  xmq data.html select \"//tr[@class='print_list_table_content']\" \\\n"
           "                delete //@class \\\n"
           "                add-root Course \\\n"
           "                transform --stringparam=title=Welcome cols.xslq to-text\n"
           "  xmq work.xml validate --silent work.xsd no-output\n"
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
"  Copyright (c) 2019-2026 Fredrik Öhrström <oehrstroem@gmail.com>\n"
"\n"
"  YAEP (Yet Another Earley Parser)\n"
"  Copyright(c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>\n"
"  Copyright(c) 2024-2026 Fredrik Öhrström <oehrstroem@gmail.com>\n"
"\n"
"  LibXML2\n"
"  Copyright (C) 1998-2012 Daniel Veillard. All Rights Reserved.\n"
"  Copyright (C) The Libxml2 Contributors.\n"
"\n"
"  LibXSLT\n"
"  Copyright (C) 2001-2002 Daniel Veillard. All Rights Reserved.\n"
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
        xmqSetColorMode(output_settings, compute_color_mode(command->use_color, command->bg_dark_mode, command->bg_forced, XMQ_RENDER_TERMINAL));
        xmqSetRenderTheme(output_settings, command->render_theme_spec);
        xmqSetupDefaultColors(output_settings);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_TERMINAL);
        break;
    case XMQ_CLI_TOKENIZE_HTML:
        xmqSetRenderFormat(output_settings, XMQ_RENDER_HTML);
        xmqSetColorMode(output_settings, compute_color_mode(command->use_color, command->bg_dark_mode, command->bg_forced, XMQ_RENDER_HTML));
        xmqSetRenderTheme(output_settings, command->render_theme_spec);
        xmqSetupDefaultColors(output_settings);
        xmqSetupParseCallbacksColorizeTokens(callbacks, XMQ_RENDER_HTML);
        break;
    case XMQ_CLI_TOKENIZE_TEX:
        xmqSetRenderFormat(output_settings, XMQ_RENDER_TEX);
        xmqSetColorMode(output_settings, compute_color_mode(command->use_color, command->bg_dark_mode, command->bg_forced, XMQ_RENDER_TEX));
        xmqSetRenderTheme(output_settings, command->render_theme_spec);
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
        // Name the source in error messages like cmd_load does.
        xmqSetStateSourceName(state, "-i argument");
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

void load_using_internal_ixml_engine(XMQCliCommand *command, const char *from)
{
    if (command->ixml_grammar == NULL)
    {
        XMQReturnDoc rd = xmqNewDoc();
        assert(rd.status == XMQ_OK);
        command->ixml_grammar = rd.doc;
        xmqSetDocSourceName(command->ixml_grammar, command->ixml_filename);

        // --ixml=grammar.xml (or .xmq .json) is a grammar in xml form, read it with the
        // normal readers and turn it into ixml text.
        const char *fn = command->ixml_filename;
        size_t fn_len = fn ? strlen(fn) : 0;
        if (fn && !(fn_len >= 5 && !strcmp(fn + fn_len - 5, ".ixml")))
        {
            XMQReturnDoc gd = xmqNewDoc();
            assert(gd.status == XMQ_OK);
            xmqSetDocSourceName(gd.doc, fn);
            if (!xmqParseBufferWithType(gd.doc, command->ixml_source, NULL, NULL, XMQ_CONTENT_DETECT, 0))
            {
                fprintf(stderr, "%s\n", xmqDocError(gd.doc));
                exit(1);
            }
            char *error = NULL;
            char *text = xmqIXMLFromDoc(gd.doc, &error);
            xmqFreeDoc(gd.doc);
            if (!text)
            {
                fprintf(stderr, "xmq: %s: %s\n", fn, error);
                exit(1);
            }
            verbose_("xmq=", "ixml grammar from %s:\n%s", fn, text);
            free((void*)command->ixml_source);
            command->ixml_source = text;
        }

        bool ok = xmqParseBufferWithType(command->ixml_grammar, command->ixml_source, NULL, NULL, XMQ_CONTENT_IXML, 0);
        verbose_("xmq=", "parse ixml grammar %zu bytes from %s", strlen(command->ixml_source), command->ixml_filename);

        if (!ok)
        {
            fprintf(stderr, "%s\n", xmqDocError(command->ixml_grammar));
            exit(1);
        }
    }

    int flags = 0;
    if (command->ixml_all_parses) flags |= XMQ_FLAG_IXML_ALL_PARSES;
    if (command->ixml_try_to_recover) flags |= XMQ_FLAG_IXML_TRY_TO_RECOVER;
    if (command->ixml_fail_silent) flags |= XMQ_FLAG_IXML_FAIL_SILENT;
    if (command->ixml_trace_file) flags |= XMQ_FLAG_IXML_TRACE;
    if (command->ixml_complete) flags |= XMQ_FLAG_IXML_COMPLETE | XMQ_FLAG_IXML_FAIL_SILENT;

    bool ok = xmqParseBufferWithIXML(command->env->doc,
                                     command->input_current_line_start,
                                     command->input_current_line_stop,
                                     command->ixml_grammar,
                                     flags);

    if (command->ixml_trace_file) save_ixml_trace(command);

    if (command->ixml_complete)
    {
        // Print what could follow the input and stop, exit 1 if the input could
        // not be parsed up to its end.
        char *completions = xmqTakeIXMLCompletions(command->env->doc);
        if (!completions) exit(1);
        fputs(completions, stdout);
        free(completions);
        exit(0);
    }

    if (!ok)
    {
        verbose_("xmq=", "cmd-load-ixml parse failed", from);
    }
    else
    {
        verbose_("xmq=", "cmd-load-ixml %zu bytes from %s", xmqGetOriginalSize(command->env->doc), from);
    }
}

/** The path of this xmq binary, to run it again, or "xmq" (from PATH) if it is not known. */
static const char *self_exe()
{
    static char path[4096];
#ifdef __linux__
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (n > 0)
    {
        path[n] = 0;
        return path;
    }
#endif
    return "xmq";
}

/** Run xmq with the arguments, the output goes to the terminal. Returns true if it exited with 0. */
static bool run_xmq(const char **args, const char *out_file)
{
#ifndef PLATFORM_WINAPI
    pid_t pid = fork();
    if (pid == 0)
    {
        if (out_file)
        {
            // Write stdout to the file, but do not follow a symbolic link.
            int fd = open(out_file, O_CREAT | O_TRUNC | O_NOFOLLOW | O_WRONLY, 0666);
            if (fd < 0 || dup2(fd, 1) < 0)
            {
                fprintf(stderr, "xmq: could not write %s\n", out_file);
                _exit(127);
            }
            close(fd);
        }
        execvp(args[0], (char**)args);
        fprintf(stderr, "xmq: could not run %s\n", args[0]);
        _exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    (void)args;
    (void)out_file;
    fprintf(stderr, "xmq: running xmq again is not yet implemented on Windows.\n");
    return false;
#endif
}

/** Write the trace of the ixml parse to the --traceixml file, as xml or json if the file
    ends with .xml or .json, else as xmq. With --traceixml=terminal the trace is saved in
    trace_grammar.xmq (after the grammar file name) and shown with
    xmq trace_grammar.xmq transform xsl:xmq-utils/render-trace-to-terminal to-text
    With --traceixml=br it is rendered with render-trace-to-html into trace_grammar.html,
    which is opened in the browser. */
void save_ixml_trace(XMQCliCommand *command)
{
    XMQDoc *trace = xmqTakeIXMLTrace(command->env->doc);
    if (!trace) return;
    const char *file = command->ixml_trace_file;
    char terminal_file[1024];
    char html_file[1024];
    if (command->ixml_trace_terminal || command->ixml_trace_browser)
    {
        // trace_formula.xmq for the grammar dir/formula.ixml
        const char *g = command->ixml_filename ? command->ixml_filename : "grammar";
        const char *base = strrchr(g, '/');
        base = base ? base+1 : g;
        const char *dot = strrchr(base, '.');
        int base_len = dot ? (int)(dot-base) : (int)strlen(base);
        snprintf(terminal_file, sizeof(terminal_file), "trace_%.*s.xmq", base_len, base);
        snprintf(html_file, sizeof(html_file), "trace_%.*s.html", base_len, base);
        file = terminal_file;
    }
    size_t len = strlen(file);
    XMQOutputSettings *os = xmqNewOutputSettings();
    xmqSetOutputFormat(os, len > 4 && !strcmp(file+len-4, ".xml") ? XMQ_CONTENT_XML :
                           len > 5 && !strcmp(file+len-5, ".json") ? XMQ_CONTENT_JSON : XMQ_CONTENT_XMQ);
    char *start = NULL, *stop = NULL;
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(trace, os);
    xmqFreeOutputSettings(os);
    xmqFreeDoc(trace);
    FILE *f = fopen(file, "wb");
    if (!f || fwrite(start, 1, stop-start, f) != (size_t)(stop-start))
    {
        fprintf(stderr, "xmq: could not write the ixml trace to %s\n", file);
        if (f) fclose(f);
        free(start);
        exit(1);
    }
    fclose(f);
    free(start);
    verbose_("xmq=", "wrote the ixml trace to %s", file);

    if (command->ixml_trace_terminal)
    {
        const char *args[] = { self_exe(), file, "transform", "xsl:xmq-utils/render-trace-to-terminal", "to-text", NULL };
        if (!run_xmq(args, NULL))
        {
            fprintf(stderr, "xmq: could not show the ixml trace %s in the terminal\n", file);
        }
    }

    if (command->ixml_trace_browser)
    {
        const char *args[] = { self_exe(), file, "transform", "xsl:xmq-utils/render-trace-to-html", "to-html", NULL };
        if (!run_xmq(args, html_file))
        {
            fprintf(stderr, "xmq: could not render the ixml trace %s to %s\n", file, html_file);
            return;
        }
        verbose_("xmq=", "wrote the ixml trace as html to %s", html_file);
        open_browser(html_file);
    }
}

void load_using_external_ixml_engine(XMQCliCommand *command, const char *engine, const char *from)
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t tmp = tv.tv_sec;
    strftime(datetime, 20, "%Y-%m-%d_%H:%M:%S", localtime(&tmp));

    // Replace with mkstemp
    char tmp_in[256];
    size_t tmpu = tv.tv_usec;
    snprintf(tmp_in, 256, "/tmp/ixml_parse_input_%s.%zu_%d", datetime, tmpu, getpid());
    char tmp_out[256];
    snprintf(tmp_out, 256, "/tmp/ixml_parse_output_%s.%zu_%d", datetime, tmpu, getpid());

    FILE *f = fopen(tmp_in, "wb");
    if (f)
    {
        size_t len = command->input_current_line_stop - command->input_current_line_start;
        size_t wrote = fwrite(command->input_current_line_start, 1, len, f);
        fclose(f);
        if (wrote != len)
        {
            fprintf(stderr, "xmq: Failed to write all bytes to %s wrote %zu but expected %zu\n", tmp_in, wrote, len);
        }
    }

    char buf[1024];
    // The error messages of the engine are kept, they are the info of the failure if the
    // engine does not print a document.
    char tmp_err[300];
    snprintf(tmp_err, sizeof(tmp_err), "%s.err", tmp_out);
    snprintf(buf, 1024, "%s %s %s > %s 2> %s", engine, command->ixml_filename, tmp_in, tmp_out, tmp_err);

    verbose_("xmq=", "using: %s", buf);

    invoke_cmd(buf);

    // An ixml engine prints the result as xml, anything else is the error message of the engine.
    bool ok = xmqParseFileWithType(command->env->doc,
                                   tmp_out,
                                   NULL,
                                   XMQ_CONTENT_XML,
                                   XMQ_FLAG_TRIM_NONE);

    if (!ok)
    {
        verbose_("xmq=", "cmd-load-external-ixml-tool parse failed: %s", xmqDocError(command->env->doc));
        // The engine did not print a document, eg coffeepot prints the error
        // Unknown error: D02: w as text. Like a failed parse with the internal engine, the
        // result is a failure document, with the output of the engine as info.
        char *msg = NULL;
        size_t msg_len = 0;
        FILE *o = fopen(tmp_err, "rb");
        if (o)
        {
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), o)) > 0)
            {
                char *m = (char*)realloc(msg, msg_len+n+1);
                if (!m) break;
                msg = m;
                memcpy(msg+msg_len, chunk, n);
                msg_len += n;
                msg[msg_len] = 0;
            }
            fclose(o);
        }
        while (msg_len > 0 && (msg[msg_len-1] == '\n' || msg[msg_len-1] == '\r' || msg[msg_len-1] == ' '))
        {
            msg[--msg_len] = 0;
        }
        char *quoted = xmqCompactQuote(msg_len > 0 ? msg : "the ixml engine gave no result");
        const char *head = "ixml(xmlns:ixml = http://invisiblexml.org/NS ixml:state = failed) { info = ";
        size_t len = strlen(head) + strlen(quoted) + 3;
        char *failure = (char*)malloc(len);
        snprintf(failure, len, "%s%s }", head, quoted);
        xmqFreeDoc(command->env->doc);
        XMQReturnDoc rd = xmqNewDoc();
        command->env->doc = rd.doc;
        xmqParseBuffer(command->env->doc, failure, failure+strlen(failure), NULL, 0);
        free(failure);
        free(quoted);
        free(msg);
    }
    else
    {
        verbose_("xmq=", "cmd-load-external-ixml-tool %zu bytes from %s", xmqGetOriginalSize(command->env->doc), from);
    }

    unlink(tmp_in);
    if (ok)
    {
        // The engine succeeded, show any messages (eg warnings) it printed.
        FILE *e = fopen(tmp_err, "rb");
        if (e)
        {
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), e)) > 0) fwrite(chunk, 1, n, stderr);
            fclose(e);
        }
    }
    unlink(tmp_out);
    unlink(tmp_err);
}

bool cmd_load(XMQCliCommand *command, bool *no_more_data)
{
    if (!command) return false;

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    command->env->doc = rd.doc;

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

    const char *from = "stdin";

    if (command->in_is_content)
    {
        from = "-i argument";
        command->input_current_line_start = command->in;
        command->input_current_line_stop = command->in+strlen(command->in);
        xmqSetOriginalSize(command->env->doc, command->input_current_line_stop-command->input_current_line_start);
        verbose_("xmq=", "cmd-load from -i argument");
    }
    else
    {
        if (command->in) from = command->in;
        if (command->input_content_start == NULL)
        {
            XMQReturnDoc rd = xmqNewDoc();
            assert(rd.status == XMQ_OK);
            XMQDoc *tmp = rd.doc;
            size_t len = 0;
            verbose_("xmq=", "cmd-load from %s", from);
            bool ok = load_file(tmp, command->in, &len, &command->input_content_start);
            command->input_content_stop = command->input_content_start+len;

            if (!ok)
            {
                const char *err = tmp->error_;
                fprintf(stderr, "%s\n", err ? err : "xmq: could not load input");
                exit(1);
            }
            xmqFreeDoc(tmp);
            if (command->lines)
            {
                command->input_current_line_start = command->input_content_start;
                command->input_current_line_stop = find_eol_or_stop(command->input_current_line_start,
                                                                    command->input_content_stop);
            }
            else
            {
                command->input_current_line_start = command->input_content_start;
                command->input_current_line_stop = command->input_content_stop;
            }
            xmqSetOriginalSize(command->env->doc, command->input_current_line_stop-command->input_current_line_start);
        }
        else
        {
            command->input_current_line_start = command->input_current_line_stop+1;
            command->input_current_line_stop = find_eol_or_stop(command->input_current_line_start,
                                                                command->input_content_stop);
            if (command->input_current_line_start >= command->input_content_stop)
            {
                *no_more_data = true;
                return false;
            }
            xmqSetOriginalSize(command->env->doc, command->input_current_line_stop-command->input_current_line_start);
        }
    }

    command->env->load = command;

    // Make the input location (a file name, or "stdin" etc.) visible in
    // any parse error messages.
    xmqSetDocSourceName(command->env->doc, from);

    if (command->ixml_source != NULL)
    {
        const char *engine = command->ixml_engine;
        if (!engine) engine = getenv("XMQ_IXML_ENGINE");

        if (engine)
        {
            load_using_external_ixml_engine(command, engine, from);
        }
        else
        {
            load_using_internal_ixml_engine(command, from);
        }
    }
    else
    {
        // A file with a .yml or .yaml extension is YAML.
        if (command->in_format == XMQ_CONTENT_DETECT && command->in)
        {
            size_t in_len = strlen(command->in);
            if ((in_len > 4 && !strncasecmp(command->in+in_len-4, ".yml", 4)) ||
                (in_len > 5 && !strncasecmp(command->in+in_len-5, ".yaml", 5)))
            {
                command->in_format = XMQ_CONTENT_YAML;
            }
        }

        bool ok = xmqParseBufferWithType(command->env->doc,
                                         command->input_current_line_start,
                                         command->input_current_line_stop,
                                         command->implicit_root,
                                         command->in_format,
                                         command->flags);

        if (!ok)
        {
            const char *error = xmqDocError(command->env->doc);
            if (error) {
                fprintf(stderr, "%s\n", error);
            }
            xmqFreeDoc(command->env->doc);
            command->env->doc = NULL;
            return false;
        }
        verbose_("xmq=", "cmd-load %zu bytes from %s", xmqGetOriginalSize(command->env->doc), from);
    }

    return true;
}

bool cmd_unload(XMQCliCommand *command)
{
    if (command && command->env && command->env->doc)
    {
        verbose_("xmq=", "cmd-unload document");
        xmqFreeDoc(command->env->doc);
        command->env->doc = NULL;

        if (command->input_current_line_start >= command->input_content_stop)
        {
            // Handled all input.
            if (command->ixml_grammar)
            {
                xmqFreeDoc(command->ixml_grammar);
                command->ixml_grammar = NULL;
            }

            if (command->input_content_start) free((char*)command->input_content_start);
            command->input_content_start = NULL;
            command->input_content_stop = NULL;
            command->input_current_line_start = NULL;
            command->input_current_line_stop = NULL;

            free((char*)command->ixml_filename);
            command->ixml_filename = NULL;
            free((char*)command->ixml_source);
            command->ixml_source = NULL;
            return false;
        }
        return true;
    }
    return false;
}

// Derive a reasonable xmq element name from an input file name:
// take the basename, drop the last extension and replace all
// characters that are not allowed in an element name with underline.
static const char *heu_root_name(const char *file)
{
    static char name[512];

    if (file == NULL) return NULL;
    const char *base = file;
    for (const char *p = file; *p; ++p)
    {
        if (*p == '/' || *p == '\\') base = p+1;
    }
    if (base[0] == 0 || !strcmp(base, "-")) return NULL;

    // Drop the last extension, "foo.bar.yml" -> "foo.bar"
    const char *dot = NULL;
    for (const char *p = base + 1; *p; ++p)
    {
        if (*p == '.') dot = p;
    }
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if (len == 0) return NULL;

    size_t i = 0;
    for (size_t k = 0; k < len && i < 511; ++k)
    {
        char c = base[k];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
        {
            name[i++] = c;
        }
        else
        {
            name[i++] = '_';
        }
    }
    name[i] = 0;

    // First char must be a letter (or underscore) to be a valid element name.
    if (!((name[0] >= 'a' && name[0] <= 'z') ||
          (name[0] >= 'A' && name[0] <= 'Z') ||
          name[0] == '_'))
    {
        memmove(name+1, name, i+1);
        name[0] = '_';
    }
    return name;
}

bool cmd_heu(XMQCliCommand *command)
{
    if (command->env->doc == NULL)
    {
        fprintf(stderr, "xmq: no document loaded, cannot run heu\n");
        return false;
    }

    const char *root_name = NULL;
    if (command->implicit_root != NULL && command->implicit_root[0] != 0)
    {
        // Implicit root given with --root=<name>.
        root_name = command->implicit_root;
    }
    else
    {
        // No root option given, use the input file name cleaned up.
        root_name = heu_root_name(command->env->load->in);
    }

    verbose_("xmq=", "cmd-heu root_name=%s", root_name ? root_name : "(none)");

    bool ok = xmqInferMissingElementNames(command->env->doc, root_name);
    if (!ok)
    {
        fprintf(stderr, "xmq: heu failed\n");
        return false;
    }
    return true;
}

bool cmd_to(XMQCliCommand *command)
{
    XMQOutputSettings *settings = xmqNewOutputSettings();
    xmqSetCompact(settings, command->compact);
    xmqSetPreferDoubleQuotes(settings, command->prefer_double_quotes);
    xmqSetEscapeNewlines(settings, command->escape_newlines);
    xmqSetEscapeNon7bit(settings, command->escape_non_7bit);
    xmqSetEscapeTabs(settings, command->escape_tabs);
    xmqSetAddIndent(settings, command->add_indent);
    xmqSetFinalNewline(settings, command->final_newline);
    // A YAML input file that does not end with a final newline is
    // written out without one as well, so documents round-trip byte-exact.
    if (command->env->load &&
        command->env->load->in_format == XMQ_CONTENT_YAML &&
        command->env->load->input_content_start != NULL &&
        command->env->load->input_content_stop > command->env->load->input_content_start &&
        command->env->load->input_content_stop[-1] != '\n')
    {
        xmqSetFinalNewline(settings, false);
    }
    xmqSetColorMode(settings, compute_color_mode(command->use_color, command->bg_dark_mode, command->bg_forced, command->render_to));
    xmqSetOutputFormat(settings, command->out_format);
    xmqSetRenderFormat(settings, command->render_to);
    xmqSetRenderRaw(settings, command->render_raw);
    xmqSetRenderOnlyStyle(settings, command->only_style);
    xmqRenderHtmlSettings(settings, command->use_id, command->use_class);
    xmqSetRenderTheme(settings, command->render_theme_spec);
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
    if (xmqDocErrno(command->env->doc) == XMQ_ERROR_EXCLUDED_FROM_BUILD)
    {
        // The output format was excluded when xmq was built, eg XMQ_NO_JSON.
        fprintf(stderr, "%s", xmqDocError(command->env->doc));
        exit(1);
    }
    const char *note = "# we should stop using yaml\n";
    if (command->out_format == XMQ_CONTENT_YAML && !we_should_stop_using_yaml__ && command->env->out_start &&
        strncmp(command->env->out_start, note, strlen(note)))
    {
        // Put the reminder before the yaml. Reading yaml keeps its comments, so a yaml that
        // already starts with the reminder (eg yaml to yaml) does not get it twice.
        size_t nl = strlen(note);
        size_t len = command->env->out_stop - command->env->out_start;
        char *buf = (char*)malloc(nl + len + 1);
        memcpy(buf, note, nl);
        memcpy(buf + nl, command->env->out_start, len);
        buf[nl + len] = 0;
        free(command->env->out_start);
        command->env->out_start = buf;
        command->env->out_stop = buf + nl + len;
    }
    return true;
}

bool check_for_ixml_fail(XmlDom *doc)
{
    if (doc == NULL) return true;
    XMQNode *root = xml_dom_root_element(doc);
    if (!root) return true;
    XMQAttr *a = xml_node_get_attr(root, "state", "http://invisiblexml.org/NS");
    const char *state = a ? xml_attr_simple_value(a) : NULL;
    if (state != NULL && !strcmp(state,"failed"))
    {
        return false;
    }
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
        if (command->env->doc != NULL) return check_for_ixml_fail(xmq_get_dom(command->env->doc));
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
    verbose_("xmq=", "transforming");
    if (command->xslt_params)
    {
        HashMapIterator *i = hashmap_iterate(command->xslt_params);
        const char *key;
        void *val;
        while (hashmap_next_key_value(i, &key, &val))
        {
            // The values are xpath expressions, --stringparam values are quoted already.
            xmqSetXsltParam(command->xslt, key, (const char*)val, true);
            verbose_("xmq=", "param %s %s", key, val);
        }
        hashmap_free_iterator(i);
    }

    verbose_("xmq=", "applying stylesheet");
    XMQReturnDoc rd = xmqTransform(command->xslt, command->env->doc);
    XmlDom *result = NULL;
    if (rd.status == XMQ_OK)
    {
        // Take over the native dom of the result.
        result = xmq_get_dom(rd.doc);
        xmq_set_dom(rd.doc, NULL);
        xmqFreeDoc(rd.doc);
    }
    if (!result) result = xml_dom_new();
    XmlDom *old = xmq_get_dom(command->env->doc);
    xmq_set_dom(command->env->doc, result);
    if (old) xml_dom_free(old);
    // Replace the xmq marker elements, eg xmq:entity(name=nbsp), that the
    // transform may have generated (see xmq_replace_transform_markers).
    xmq_replace_transform_markers(command->env->doc);

    return true;
}

/** Return the concatenated text content of all top level nodes in the document.
    Never returns NULL, an empty document gives an empty string. Free with free(). */
static char *doc_text_content(XmlDom *doc)
{
    size_t len = 0;
    char *out = (char*)malloc(1);
    out[0] = 0;
    if (!doc) return out;

    for (XMQNode *i = xml_node_first_child(xml_dom_document(doc)); i; i = xml_node_next(i))
    {
        if (xml_node_kind(i) == XML_DOM_DTD) continue;
        char *c = xml_node_string_value(i);
        size_t n = strlen(c);
        out = (char*)realloc(out, len+n+1);
        memcpy(out+len, c, n+1);
        len += n;
        free(c);
    }
    return out;
}

bool cmd_quote_unquote(XMQCliCommand *command)
{
    XmlDom *doc = xmq_get_dom(command->env->doc);
    verbose_("(xmq) transforming\n");

    if (command->cmd == XMQ_CLI_CMD_QUOTE_C)
    {
        char *value = doc_text_content(doc);
        XMQReturnString rs = xmq_quote_as_c(value, value+strlen(value), true);
        free(value);
        if (rs.status != XMQ_OK) return false;
        char *quoted_value = rs.string;

        if (command->add_nl)
        {
            size_t len = strlen(quoted_value);
            char *q = (char*)malloc(len+2);
            strcpy(q, quoted_value);
            strcat(q, "\n");
            free(quoted_value);
            quoted_value = q;
        }

        XmlDom *new_doc = xml_dom_new();
        XMQNode *new_node = xml_node_new_text(new_doc, quoted_value);
        free(quoted_value);

        xml_add_root_child(new_doc, new_node);

        xmq_set_dom(command->env->doc, new_doc);
        xml_dom_free(doc);

        return true;
    }

    if (command->cmd == XMQ_CLI_CMD_UNQUOTE_C)
    {
        char *value = doc_text_content(doc);
        XMQReturnString rs = xmq_unquote_as_c(value, value+strlen(value), true);
        free(value);
        if (rs.status != XMQ_OK) return false;
        char *unquoted_value = rs.string;

        XmlDom *new_doc = xml_dom_new();
        XMQNode *new_node = xml_node_new_text(new_doc, unquoted_value);
        free(unquoted_value);

        xml_add_root_child(new_doc, new_node);

        xmq_set_dom(command->env->doc, new_doc);
        xml_dom_free(doc);

        return true;
    }

    return false;
}

bool cmd_validate(XMQCliCommand *command)
{
    verbose_("xmq=", "validating");

    if (!command->xsd)
    {
        fprintf(stderr, "xmq: Schema xsd not valid, cannot be used to validate document.\n");
        exit(1);
    }
    xmqSetXsdMessageCallback(command->xsd,
                             command->silent ? cli_xsd_silent_message : cli_xsd_validate_message,
                             (void*)command->xsd_name);

    XMQStatus status = xmqValidate(command->xsd, command->env->doc);
    if (status == XMQ_ERROR_XSD_COMPILE)
    {
        fprintf(stderr, "xmq: Schema xsd not valid, cannot be used to validate document.\n");
        exit(1);
    }
    if (status != XMQ_OK)
    {
        printf("xmq: Document failed validation agains %s\n", command->in);
        return false;
    }

    xmqFreeXsd(command->xsd);
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

/** Evaluate an xpath on the document like a fresh libxml2 xpath context: no context node. */
static XmlXPathResult *cli_xpath(XMQDoc *doq, const char *xpath)
{
    XmlDom *dom = xmq_get_dom(doq);
    XmlXPathSettings settings;
    xml_xpath_settings_default(&settings);
    settings.no_context_node = true;
    XmlXPathError error;
    XmlXPathResult *r = xml_xpath_eval(xpath, xml_dom_document(dom), &settings, &error);
    if (!r)
    {
        // Like the libxml2 xpath error report.
        fprintf(stderr, "XPath error : %s\n%s\n%*s^\n", error.message, xpath, (int)error.offset, "");
    }
    return r;
}

static void cli_copy_to_top(XmlDom *new_doc, XMQNode *n)
{
    if (xml_node_kind(n) == XML_DOM_DOCUMENT)
    {
        for (XMQNode *c = xml_node_first_child(n); c; c = xml_node_next(c))
        {
            xml_add_root_child(new_doc, xml_node_copy(new_doc, c, true));
        }
        return;
    }
    xml_add_root_child(new_doc, xml_node_copy(new_doc, n, true));
}

bool cmd_select(XMQCliCommand *command)
{
    XmlDom *doc = xmq_get_dom(command->env->doc);
    verbose_("xmq=", "selecting");

    XmlXPathResult *objects = cli_xpath(command->env->doc, command->xpath);

    if (objects == NULL)
    {
        fprintf(stderr, "xmq: no nodes matched %s\n", command->xpath);
        return false;
    }

    XmlDom *new_doc = xml_dom_new();

    if (xml_xpath_result_type(objects) == XML_XPATH_NUMBER)
    {
        double count = xml_xpath_result_number(objects);
        char buf[64];
        snprintf(buf, 64, "%.0f", count);
        xml_add_root_child(new_doc, xml_node_new_text(new_doc, buf));
    }
    else if (xml_xpath_result_type(objects) == XML_XPATH_BOOLEAN)
    {
        char buf[64];
        snprintf(buf, 64, "%d", xml_xpath_result_boolean(objects) ? 1 : 0);
        xml_add_root_child(new_doc, xml_node_new_text(new_doc, buf));
    }
    else if (xml_xpath_result_type(objects) == XML_XPATH_NODESET)
    {
        int size = (int)xml_xpath_result_size(objects);
        bool whole_document = false;

        // Copy and unlink in reverse order which means copy unlink deeper nodes first.
        for(int i = size-1; i >= 0; i--)
        {
            XMQNode *n = xml_xpath_result_node(objects, i);

            if (is_attribute_node(n))
            {
                // we found an attribute node, eg href="...", rewrite this into
                // <href>...</href> which prints nicely as href=... in xmq.
                XMQNode *new_node = xml_node_new_element(new_doc, xml_node_name(n), NULL);
                for (XMQNode *c = xml_node_first_child(n); c; c = xml_node_next(c))
                {
                    xml_node_append_child(new_node, xml_node_copy(new_doc, c, true));
                }
                xml_add_root_child(new_doc, new_node);
                xml_node_free(n);
            }
            else if (xml_node_kind(n) == XML_DOM_NAMESPACE)
            {
                // Namespace nodes belong to the xpath result.
            }
            else if (xml_node_kind(n) == XML_DOM_DOCUMENT)
            {
                cli_copy_to_top(new_doc, n);
                whole_document = true;
            }
            else
            {
                cli_copy_to_top(new_doc, n);
                xml_node_free(n);
            }
        }
        (void)whole_document;
    }
    else
    {
        error("Unknown xpath select result type %d\n", xml_xpath_result_type(objects));
    }

    xml_xpath_result_free(objects);

    xmq_set_dom(command->env->doc, new_doc);
    xml_dom_free(doc);

    return true;
}

bool cmd_for_each(XMQCliCommand *command)
{
    verbose_("xmq=", "for each");

    XmlXPathResult *objects = cli_xpath(command->env->doc, command->xpath);

    if (objects == NULL)
    {
        fprintf(stderr, "xmq: no nodes matched %s\n", command->xpath);
        return false;
    }

    int size = (int)xml_xpath_result_size(objects);

    for(int i = 0; i < size; ++i)
    {
        XMQNode *n = xml_xpath_result_node(objects, i);

        if (n)
        {
            while (n && !is_element_node(n))
            {
                // We found an attribute move to parent.
                n = xml_node_parent(n);
            }
            if (n) invoke_shell(n, command->content);
        }
    }

    xml_xpath_result_free(objects);

    return true;
}

bool cmd_add(XMQCliCommand *command)
{
    XmlDom* doc = xmq_get_dom(command->env->doc);
    verbose_("xmq=", "adding xmq >%s<", command->add_xmq);

    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doq = rd.doc;
    const char *start = command->add_xmq;
    const char *stop = start+strlen(start);

    bool ok = xmqParseBuffer(doq, start, stop, NULL, 0);
    if (!ok)
    {
        const char *error = xmqDocError(doq);
        fprintf(stderr, "%s\n", error ? error : "xmq: parse error");
        xmqFreeDoc(doq);
        return false;
    }

    XmlDom* append = xmq_get_dom(doq);
    XMQNode* i = xml_node_first_child(xml_dom_document(append));
    while (i)
    {
        XMQNode* next = xml_node_next(i);
        // Move the top level node to the end of the document.
        xml_node_add_child(xml_dom_document(doc), i);
        xml_node_reconcile_ns(i);
        i = next;
    }

    xmqFreeDoc(doq);

    return true;
}

bool cmd_add_root(XMQCliCommand *command)
{
    XmlDom* doc = xmq_get_dom(command->env->doc);
    verbose_("xmq=", "adding root");

    XmlDom *new_doc = xml_dom_new();

    XMQNode *root = xml_node_new_element(new_doc, command->xpath, NULL);
    xml_dom_set_root_element(new_doc, root);

    for (XMQNode *i = xml_node_first_child(xml_dom_document(doc)); i; i = xml_node_next(i))
    {
        if (xml_node_kind(i) == XML_DOM_DTD) continue;
        XMQNode *new_node = xml_node_copy(new_doc, i, true);
        xml_node_add_child(root, new_node);
    }

    xmq_set_dom(command->env->doc, new_doc);
    xml_dom_free(doc);

    return true;
}

void accumulate_children(Stats *stats, XMQNode *node)
{
    XMQAttr *a = xml_first_attribute(node);
    while (a)
    {
        XMQAttr *next = xml_attr_next(a);
        accumulate_attribute(stats, a);
        a = next;
    }

    XMQNode *i = xml_first_child(node);
    while (i)
    {
        XMQNode *next = xml_next_sibling(i);
        accumulate_statistics(stats, i);
        i = next;
    }
}

void accumulate_attribute(Stats *stats, XMQAttr *a)
{
    stats->num_attributes++;
    stats->size_attribute_names += strlen(xml_attr_name(a));

    XMQNode *i = xml_node_first_child(xml_attr_node(a));
    while (i)
    {
        XMQNode *next = xml_next_sibling(i);
        accumulate_attribute_content(stats, i);
        i = next;
    }

}

void accumulate_statistics(Stats *stats, XMQNode *node)
{
    if (xml_node_kind(node) == XML_DOM_ELEMENT)
    {
        stats->num_elements++;
        stats->size_element_names += strlen(xml_node_name(node));
        accumulate_children(stats, node);
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_TEXT)
    {
        stats->num_text_nodes++;
        stats->size_text_nodes += strlen(xml_node_content(node));
        bool has_quote = false;
        bool has_sq = false;
        bool has_dq = false;
        if (strchr(xml_node_content(node), '"'))
        {
            stats->num_nodes_with_dqs++;
            has_quote = true;
            has_dq = true;
        }
        if (strchr(xml_node_content(node), '\''))
        {
            stats->num_nodes_with_sqs++;
            has_quote = true;
            has_sq = true;
        }
        if (has_quote)
        {
            stats->num_nodes_with_any_qs++;
        }
        else
        {
            stats->num_nodes_with_no_qs++;
        }
        if (has_dq && has_sq)
        {
            stats->num_nodes_with_both_qs++;
        }
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_COMMENT)
    {
        stats->num_comments++;
        stats->size_comments += strlen(xml_node_content(node));
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_CDATA)
    {
        stats->num_cdata_nodes++;
        stats->size_cdata_nodes += strlen(xml_node_content(node));
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_ENTITY_REF)
    {
        stats->num_entities++;
        stats->size_entities += strlen(xml_node_name(node));
        return;
    }
    else if (xml_node_kind(node) == XML_DOM_DTD)
    {
        stats->has_doctype++;
        stats->size_doctype += strlen(xml_node_name(node));
        return;
    }
}

void accumulate_attribute_content(Stats *stats, XMQNode *node)
{
    if (xml_node_kind(node) == XML_DOM_TEXT)
    {
        stats->size_attribute_content += strlen(xml_node_content(node));
        return;
    }
}

void count_statistics(Stats *stats, XmlDom* doc)
{
    XMQNode* i = xml_node_first_child(xml_dom_document(doc));

    while (i)
    {
        XMQNode *next = xml_next_sibling(i);
        accumulate_statistics(stats, i);
        i = next;
    }
}

void add_key_value(XmlDom *doc, XMQNode *root, const char *key, size_t value)
{
    char buf[1024];
    snprintf(buf, 1024, "%zu", value);
    XMQNode *element = xml_node_new_element(doc, key, NULL);
    xml_node_append_child(root, element);
    xml_node_append_child(element, xml_node_new_text(doc, buf));
}

bool cmd_statistics(XMQCliCommand *command)
{
    XmlDom* doc = xmq_get_dom(command->env->doc);
    verbose_("xmq=", "calculating statistics");
    Stats stats;
    memset(&stats, 0, sizeof(stats));

    count_statistics(&stats, doc);

    XmlDom *new_doc = xml_dom_new();
    XMQNode *root = xml_node_new_element(new_doc, "statistics", NULL);
    xml_dom_set_root_element(new_doc, root);

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
    if (stats.num_nodes_with_sqs) add_key_value(new_doc, root, "num_text_nodes_with_single_quotes", stats.num_nodes_with_sqs);
    if (stats.num_nodes_with_dqs) add_key_value(new_doc, root, "num_text_nodes_with_double_quotes", stats.num_nodes_with_dqs);
    if (stats.num_nodes_with_no_qs) add_key_value(new_doc, root, "num_text_nodes_with_no_quotes", stats.num_nodes_with_no_qs);
    if (stats.num_nodes_with_any_qs) add_key_value(new_doc, root, "num_text_nodes_with_any_quotes", stats.num_nodes_with_any_qs);
    if (stats.num_nodes_with_both_qs) add_key_value(new_doc, root, "num_text_nodes_with_both_quotes", stats.num_nodes_with_both_qs);

    size_t sum_meta = stats.size_element_names+stats.size_attribute_names+stats.size_attribute_content+stats.size_doctype;
    size_t sum_text = stats.size_text_nodes;

    add_key_value(new_doc, root, "sum_meta", sum_meta);
    add_key_value(new_doc, root, "sum_text", sum_text);

    xmq_set_dom(command->env->doc, new_doc);
    xml_dom_free(doc);

    return true;
}

void delete_all_entities(XMQDoc *doq, XMQNode *node, const char *entity)
{
    if (xml_node_kind(node) == XML_DOM_ENTITY_DECL ||
        xml_node_kind(node) == XML_DOM_ENTITY_REF)
    {
        if (!strcmp(entity, (char*)xml_node_name(node)))
        {
            xml_node_free(node);
        }
        return;
    }

    XMQNode *i = xml_node_first_child(node);
    while (i)
    {
        XMQNode *next = xml_node_next(i); // i might be freed in trim.
        delete_all_entities(doq, i, entity);
        i = next;
    }
}

void delete_entities(XMQDoc *doq, const char *entity)
{
    XmlDom* doc = xmq_get_dom(doq);
    XMQNode* i = xml_node_first_child(xml_dom_document(doc));
    if (!doq || !i) return;

    while (i)
    {
        XMQNode *next = xml_node_next(i); // i might be freed in delete_all_entities
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
    XmlXPathResult *objects = cli_xpath(command->env->doc, command->xpath);

    if (objects == NULL)
    {
        verbose_("xmq=", "no nodes deleted");
        return true;
    }

    int size = (int)xml_xpath_result_size(objects);

    // Unlink in reverse order which means unlink deeper nodes first.
    for(int i = size-1; i >= 0; i--)
    {
        XMQNode *n = xml_xpath_result_node(objects, i);
        XmlNodeKind k = xml_node_kind(n);
        // The document node cannot be deleted, namespace nodes belong to the result.
        if (k == XML_DOM_DOCUMENT || k == XML_DOM_NAMESPACE) continue;
        xml_node_free(n);
    }

    xml_xpath_result_free(objects);

    return true;
}

int replace_xpath_content(XMQDoc *doq, const char *xpath, const char *content)
{
    if (doq == NULL || xpath == NULL || content == NULL) {
        return -1;
    }

    XmlXPathResult *r = cli_xpath(doq, xpath);
    if (r == NULL) return -1;
    int ret = -1;
    if (xml_xpath_result_type(r) == XML_XPATH_NODESET && xml_xpath_result_size(r) > 0)
    {
        /* Replace content of first matched node */
        XMQNode *node = xml_xpath_result_node(r, 0);
        if (xml_node_kind(node) != XML_DOM_DOCUMENT && xml_node_kind(node) != XML_DOM_NAMESPACE)
        {
            xml_node_set_content(node, content);
            ret = 0;  /* Success */
        }
    }
    xml_xpath_result_free(r);
    return ret;
}

/** Merge the text node b into the text node a (like libxml2 xmlTextMerge), returns a. */
static XMQNode *cli_text_merge(XMQNode *a, XMQNode *b)
{
    const char *t = xml_node_content(b);
    if (t) xml_node_append_content(a, t, strlen(t));
    xml_node_free(b);
    return a;
}

XMQNode *replace_entity(XmlDom *doc, XMQNode *node, const char *entity, const char *content, XMQNode* node_content)
{
    if (!node) return NULL;

    XMQNode *next = xml_node_next(node);

    if (xml_node_kind(node) == XML_DOM_ENTITY_REF)
    {
        if (!strcmp(xml_node_name(node), entity))
        {
            if (content)
            {
                XMQNode *new_node = xml_node_new_text(doc, content);
                xml_node_replace(node, new_node);
                xml_node_free(node);
                node = new_node;
                next = xml_node_next(node);
            }
            else if (node_content)
            {
                XMQNode *new_node = xml_node_copy(doc, node_content, true);
                xml_node_replace(node, new_node);
                xml_node_free(node);
                node = new_node;
                xml_node_reconcile_ns(node);
                next = xml_node_next(node);
            }
            else
            {
                // No content, eg --with-file of an empty file, the entity is replaced with nothing.
                XMQNode *new_node = xml_node_new_text(doc, "");
                xml_node_replace(node, new_node);
                xml_node_free(node);
                node = new_node;
                next = xml_node_next(node);
            }
        }
        if (xml_node_prev(node) != NULL && is_content_node(xml_node_prev(node)) && is_content_node(node))
        {
            node = cli_text_merge(xml_node_prev(node), node);
            next = xml_node_next(node);

            if (xml_node_next(node) != NULL && is_content_node(xml_node_next(node)))
            {
                node = cli_text_merge(node, xml_node_next(node));
                next = xml_node_next(node);
            }
        }
        return next;
    }

    XMQAttr *a = xml_first_attribute(node);
    while (a)
    {
        if (xml_node_first_child(xml_attr_node(a)) != NULL)
        {
            XMQNode *i = xml_node_first_child(xml_attr_node(a));
            while (i)
            {
                i = replace_entity(doc, i, entity, content, node_content);
            }
        }
        a = xml_next_attribute(a);
    }

    XMQNode *i = xml_node_first_child(node);
    while (i)
    {
        i = replace_entity(doc, i, entity, content, node_content);
    }

    return next;
}

void replace_entities(XmlDom *doc, const char *entity, const char *content, XMQNode* node_content)
{
    XMQNode* i = xml_node_first_child(xml_dom_document(doc));
    if (!doc || !i) return;

    while (i)
    {
        replace_entity(doc, i, entity, content, node_content);
        i = xml_node_next(i);
    }
}

bool cmd_replace(XMQCliCommand *command)
{
    XmlDom* doc = xmq_get_dom(command->env->doc);

    if (command->xpath)
    {
        verbose_("xmq=", "replacing xpath %s", command->xpath);
        const char *ep = strchr(command->xpath, '=');
        if (ep == NULL)
        {
            fprintf(stderr, "xmq: replace expects xpath=value, got: %s\n", command->xpath);
            return false;
        }
        char *xpath = strndup(command->xpath, ep-command->xpath);
        char *value = strdup(ep+1);
        replace_xpath_content(command->env->doc, xpath, value);
        free(xpath);
        free(value);
    }

    if (command->entity)
    {
        verbose_("xmq=", "replacing entity %s", command->entity);

        replace_entities(doc, command->entity, command->content, command->node_content);
    }

    return true;
}

void substitute_entity(XmlDom *doc, XMQNode *node, const char *entity, bool only_chars)
{
    if (!node) return;
    if (xml_node_kind(node) == XML_DOM_ENTITY_REF)
    {
        if (entity == NULL || !strcmp(xml_node_name(node), entity))
        {
            if (xml_node_name(node)[0] == '#')
            {
                char buf[5] = {};
                char *out = buf;
                int uc = decode_entity_ref(xml_node_name(node));
                UTF8Char utf8;
                size_t n = encode_utf8(uc, &utf8);
                for (size_t j = 0; j < n; ++j)
                {
                    *out++ = utf8.bytes[j];
                }
                XMQNode *new_node = xml_node_new_text(doc, buf);
                xml_node_replace(node, new_node);
                xml_node_free(node);
            }
        }
        return;
    }

    XMQNode *i = xml_node_first_child(node);
    while (i)
    {
        XMQNode *next = xml_node_next(i);
        substitute_entity(doc, i, entity, only_chars);
        i = next;
    }
}

bool cmd_substitute(XMQCliCommand *command)
{
    XmlDom* doc = xmq_get_dom(command->env->doc);
    if (doc == NULL) return true;
    XMQNode* root = xml_dom_document(doc);

    if (command->cmd == XMQ_CLI_CMD_SUBSTITUTE_ENTITY)
    {
        verbose_("xmq=", "substituting entity %s", command->entity);

        substitute_entity(doc, root, command->entity, true);
        XmlParseSettings settings;
        xml_parse_settings_default(&settings);
        XmlParseError error;
        if (!xml_expand_entities(doc, NULL, command->entity, xmqGetOriginalSize(command->env->doc), &settings, &error))
        {
            fprintf(stderr, "xmq: %s\n", error.short_message ? error.short_message : "entity expansion failed");
            xml_parse_error_free(&error);
            return false;
        }
        xml_parse_error_free(&error);
    }
    if (command->cmd == XMQ_CLI_CMD_EXPAND_ENTITIES)
    {
        verbose_("xmq=", "expanding all entities");
        if (xmqExpandEntities(command->env->doc) != XMQ_OK)
        {
            fputs(xmqDocError(command->env->doc), stderr);
            return false;
        }
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
    case XMQ_CLI_CMD_TO_YAML:
        c->out_format = XMQ_CONTENT_YAML;
        c->add_indent   = 2; // Default yaml output indentation.
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
        c->render_theme_spec = default_theme_spec_;
        return;
    case XMQ_CLI_CMD_RENDER_HTML:
        c->out_format = XMQ_CONTENT_XMQ;
        c->render_to = XMQ_RENDER_HTML;
        c->render_theme_spec = default_theme_spec_;
        return;
    case XMQ_CLI_CMD_RENDER_TEX:
        c->out_format = XMQ_CONTENT_XMQ;
        c->render_to = XMQ_RENDER_TEX;
        c->render_theme_spec = default_theme_spec_;
        return;
    case XMQ_CLI_CMD_TOKENIZE:
        c->in = load_command->in;
        // With -i the input is the content to tokenize, not a file name.
        c->in_is_content = load_command->in_is_content;
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
    case XMQ_CLI_CMD_EXPAND_ENTITIES:
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
    case XMQ_CLI_CMD_HEU:
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
    case XMQ_CLI_CMD_CHECK_LIBRARY:
    case XMQ_CLI_CMD_REFRESH_LIBRARY:
    case XMQ_CLI_CMD_LIST_LIBRARY:
    case XMQ_CLI_CMD_DESCRIBE_LIBRARY:
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
    ssize_t n = read(STDOUT_FILENO, &c, sizeof(char));
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

    if (isatty(1))
    {
        WriteConsole(out, start, stop-start, &wrote, NULL);
    }
    else
    {
        WriteFile(out, start, stop-start, &wrote, NULL);
    }
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
    case XMQ_CLI_CMD_CHECK_LIBRARY:
        printf(
            "Usage: xmq check-library\n"
            "List the library files (ixml grammars and xsl transforms) that refresh-library would\n"
            "download: new on https://libxmq.org/library or changed there more recently. The files\n"
            "are compared with the library xmq uses (see refresh-library) using the sha256 and\n"
            "modification time listed in https://libxmq.org/library/index.xmq\n");
        break;
    case XMQ_CLI_CMD_REFRESH_LIBRARY:
        printf(
            "Usage: xmq refresh-library\n"
            "Download the library files (ixml grammars and xsl transforms) that are new on\n"
            "https://libxmq.org/library or changed there more recently, into ~/.local/share/xmq/library.\n"
            "Files there are used before the library installed with xmq (share/xmq/library), so\n"
            "after make install the library can be refreshed without reinstalling.\n");
        break;
    case XMQ_CLI_CMD_LIST_LIBRARY:
        printf(
            "Usage: xmq list-library\n"
            "List the ixml grammars and xsl transforms in the library on https://libxmq.org/library\n"
            "(or the installed library index, if libxmq.org cannot be reached) with their versions:\n"
            "  ixml:data/csv   v2abcd v1abc\n"
            "A version is a major number and a minor letter, v2c is 2.3.0. ixml:data/csv is the\n"
            "latest version, ixml:data/csv_v2 the latest 2.x and ixml:data/csv_v2c exactly 2c.\n"
            "A published version only changes in its comments, for example:\n"
            "  xmq ixml:data/csv data.csv xsl:work/report_v2 to-text\n"
            "uses the latest csv grammar and the latest version 2 of the report.\n");
        break;
    case XMQ_CLI_CMD_DESCRIBE_LIBRARY:
        printf(
            "Usage: xmq describe-library <ixml:name|xsl:name>\n"
            "Print the description (the first comment) of a library grammar or transform and its\n"
            "versions, for example: xmq describe-library ixml:data/csv\n");
        break;
    case XMQ_CLI_CMD_HELP:
        printf(
            "Usage: xmq help <command>\n"
            "Print more detailed help about the command.\n");
        break;
    case XMQ_CLI_CMD_HEU:
        printf(
            "Usage: xmq <input.yml|input.json> heu [to-xmq to-yaml ...]\n"
            "Guess using a heuristic, names of unnamed elements (elements named _) by using the name of the parent:\n"
            "an unnamed child of an element named for example 'branches' is renamed 'branch'\n"
            "(steps -> step, branches -> branch, houses -> house, cities -> city)\n"
           "and an unnamed root is named with the --root=<name>\n"
            "option if given, or after the input file name if not.\n");
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
    size_t wrote = write(fd, c->env->out_start + c->env->out_skip, len);
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
    char *buf = (char*)malloc(len+1);
    *out_name_start = name_start;
    memcpy(buf, name_start, len);
    buf[len]  = 0;
    return buf;
}

void append_text_node(MemBuffer *buf, XMQNode *node)
{
    if (is_content_node(node))
    {
        membuffer_append(buf, (char*)xml_node_content(node));
    }
    else if (is_entity_node(node))
    {
        membuffer_append(buf, "TODO");
    }
    else if (is_element_node(node))
    {
        append_text_children(buf, xml_node_first_child(node));
    }
}

void append_text_children(MemBuffer *buf, XMQNode *n)
{
    XMQNode *i = n;

    while (i)
    {
        append_text_node(buf, i);
        i = xml_next_sibling(i);
    }
}

char *grab_content(XMQNode *n, const char *name)
{
    MemBuffer *buf = new_membuffer();

    if (!strcmp(name, ".."))
    {
        membuffer_append(buf, (char*)xml_node_name(n));
    }
    else
    if (!strcmp(name, "."))
    {
        append_text_node(buf, n);
    }
    else
    {
        XMQNode *i = xml_node_first_child(n);
        while (i)
        {
            if (!strcmp((char*)xml_node_name(i), name))
            {
                XMQNode *j = xml_node_first_child(i);
                while (j)
                {
                    if (is_text_node(j))
                    {
                        membuffer_append(buf, (char*)xml_node_content(j));
                    }
                    j = xml_node_next(j);
                }
            }
            i = xml_node_next(i);
        }
    }
    membuffer_append_null(buf);
    return free_membuffer_but_return_trimmed_content(buf);
}

#ifndef PLATFORM_WINAPI
// Posix says that this variable just exists.
// (On some systems this is also declared in unistd.h)
extern char **environ;
#endif

bool invoke_shell(XMQNode *node, const char *shell_command)
{
    bool ok = true;
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
                char *content = grab_content(node, name);
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
        execve("/bin/sh", argv, env);
        perror("Execve failed:");
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
                verbose_("shell=", "/bin/sh -c %s exited with non-zero return code: %d\n", "/bin/sh", rc);
                ok = false;
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
#else
    fprintf(stderr, "Not implemented invoke_shell on windows.\n");
    exit(1);
#endif
    return ok;
}

void invoke_cmd(const char *shell_cmd)
{
#ifndef PLATFORM_WINAPI
    pid_t pid = fork();
    int status;
    char *cmd = strdup(shell_cmd);
    char **argv = malloc(sizeof(char*)*4);
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = (char*)cmd;
    argv[3] = 0;

    if (pid == 0)
    {
        // I am the child!
        close(0); // Close stdin
        execve("/bin/sh", argv, environ);

        perror("Execvp failed:");
        // Use _exit() to avoid running parent's atexit handlers and destructors
        // which can deadlock in a forked child.
        _exit(127);
    }
    else
    {
        if (pid == -1) {
            error("shell=", "could not fork!");
        }
        debug_("shell=", "waiting for child %d to complete.", pid);
        // Wait for the child to finish!
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            debug("shell=", "%s: return code %d", cmd, rc);
            if (rc == 127) {
                warning("shell=", "invoking %s failed!", cmd);
            }
            else if (rc != 0) {
                warning("shell=", "%s exited with non-zero return code: %d", cmd, rc);
            }
        }
        free(cmd);
        free(argv);
    }
#else
    fprintf(stderr, "Invoke cmd not implemented on windows.\n");
    exit(1);
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

bool perform_command(XMQCliCommand *c, bool *no_more_data)
{
    *no_more_data = false;
    if (c->cmd == XMQ_CLI_CMD_NONE) return true;

    switch (c->cmd) {
    case XMQ_CLI_CMD_NONE:
        return true;
    case XMQ_CLI_CMD_NO_OUTPUT:
        return true;
    case XMQ_CLI_CMD_LOAD:
        return cmd_load(c, no_more_data);
    case XMQ_CLI_CMD_TO_XMQ:
    case XMQ_CLI_CMD_TO_XML:
    case XMQ_CLI_CMD_TO_HTMQ:
    case XMQ_CLI_CMD_TO_HTML:
    case XMQ_CLI_CMD_TO_JSON:
    case XMQ_CLI_CMD_TO_YAML:
    case XMQ_CLI_CMD_TO_TEXT:
    case XMQ_CLI_CMD_TO_CLINES:
    case XMQ_CLI_CMD_RENDER_TERMINAL:
    case XMQ_CLI_CMD_RENDER_HTML:
    case XMQ_CLI_CMD_RENDER_TEX:
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
    case XMQ_CLI_CMD_EXPAND_ENTITIES:
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
    case XMQ_CLI_CMD_HEU:
        return cmd_heu(c);
    case XMQ_CLI_CMD_STATISTICS:
        return cmd_statistics(c);
    case XMQ_CLI_CMD_QUOTE_C:
    case XMQ_CLI_CMD_UNQUOTE_C:
        return cmd_quote_unquote(c);
    case XMQ_CLI_CMD_HELP:
        return cmd_help(c);
    case XMQ_CLI_CMD_CHECK_LIBRARY:
    case XMQ_CLI_CMD_REFRESH_LIBRARY:
    case XMQ_CLI_CMD_LIST_LIBRARY:
    case XMQ_CLI_CMD_DESCRIBE_LIBRARY:
        // Handled in main before any input is read.
        return true;
    }
    assert(false);
    return false;
}

const char *is_ixml_cmd(const char *arg)
{
    // Does arrg look like:  ixml:data/csv
    char *colon = strrchr(arg, ':');
    if (colon && !strncmp("ixml:", arg, colon-arg))
    {
        return arg+5;
    }
    return NULL;
}

const char*is_xsl_cmd(const char *arg)
{
    // Does arg look like:  xsl:data/table-to-html
    char *colon = strrchr(arg, ':');
    if (colon && !strncmp("xsl:", arg, colon-arg))
    {
        return arg+4;
    }
    return NULL;
}

bool dot_found(const char *file)
{
    char *dot = strrchr(file, '.');
    if (dot) return true;
    return false;
}

/** How library files are fetched for the command: --force-download (also given before the
    first command), else only when they are not in the library. */
static LibraryMode library_mode(XMQCliCommand *command)
{
    bool force = command->force_download ||
        (command->env && command->env->load && command->env->load->force_download);
    return force ? LIBRARY_FORCE : LIBRARY_IF_MISSING;
}

/** The url of the library, https://libxmq.org/library/ or $XMQ_LIBRARY_URL (eg for tests). */
static const char *library_url()
{
    const char *env = getenv("XMQ_LIBRARY_URL");
    return (env && *env) ? env : "https://libxmq.org/library/";
}

/** Fetch url into local_file with curl, the directories are created. With keep_time the
    file gets the modification time of the server (curl -R). The command is stored in cmd
    for error messages. Windows runs it with cmd.exe, which only knows double quotes (curl
    is included in Windows 10 and later). Returns true if the file was fetched. */
static bool curl_fetch(const char *url, const char *local_file, bool keep_time, char *cmd, size_t cmd_size)
{
#ifdef PLATFORM_WINAPI
    const char *q = "\"";
#else
    const char *q = "'";
#endif
    snprintf(cmd, cmd_size, "curl -s --fail%s --create-dirs -o %s%s%s %s%s%s",
             keep_time ? " -R" : "", q, local_file, q, q, url, q);
    return system(cmd) == 0;
}

/** Move the downloaded tmp file to dest, replacing dest. On Windows rename fails when dest
    exists, MoveFileEx replaces it. Returns true on success. */
static bool replace_file(const char *tmp, const char *dest)
{
#ifdef PLATFORM_WINAPI
    return MoveFileExA(tmp, dest, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(tmp, dest) == 0;
#endif
}

/** Fetch library_url()/file+suffix into local_file, unless it is cached and the mode is not
    LIBRARY_FORCE. The file gets the modification time of the server (curl -R), which
    check-library compares with the time in the index. */
DownloadStatus download(LibraryMode mode, const char *suffix, const char *file, const char *local_file, bool accept_fail)
{
    FILE *f = fopen(local_file, "rb");
    if (f)
    {
        fclose(f);
        if (mode == LIBRARY_IF_MISSING) return DOWNLOAD_EXISTS;
    }

    // curl https://libxmq.org/library/data/tsv.ixml
    char url[1024];
    snprintf(url, sizeof(url), "%s%s%s", library_url(), file, suffix);
    char cmd[2048];

    fprintf(stderr, "fetching %s\n", url);
    if (!curl_fetch(url, local_file, true, cmd, sizeof(cmd)))
    {
        if (accept_fail) return DOWNLOAD_ERROR;

        fprintf(stderr, "xmq: failed to fetch %s using this command: %s\n", suffix, cmd);
        exit(1);
    }
    fprintf(stderr, "downloaded and cached %s\n", local_file);
    return DOWNLOAD_OK;
}

// The library index and xmq check-library / refresh-library ///////////////////////////

/* SHA-256 (FIPS 180-4), to compare library files with the sha256 in the library index. */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t used; } Sha256;

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

#define SHA256_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(Sha256 *s, const unsigned char *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = SHA256_ROR(w[i-15], 7) ^ SHA256_ROR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = SHA256_ROR(w[i-2], 17) ^ SHA256_ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++)
    {
        uint32_t t1 = h + (SHA256_ROR(e, 6) ^ SHA256_ROR(e, 11) ^ SHA256_ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha256_k[i] + w[i];
        uint32_t t2 = (SHA256_ROR(a, 2) ^ SHA256_ROR(a, 13) ^ SHA256_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha256_init(Sha256 *s)
{
    static const uint32_t h0[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    memcpy(s->h, h0, sizeof(h0));
    s->len = 0;
    s->used = 0;
}

static void sha256_update(Sha256 *s, const unsigned char *data, size_t n)
{
    s->len += n;
    while (n > 0)
    {
        size_t take = 64 - s->used;
        if (take > n) take = n;
        memcpy(s->buf + s->used, data, take);
        s->used += take;
        data += take;
        n -= take;
        if (s->used == 64) { sha256_block(s, s->buf); s->used = 0; }
    }
}

/** The sha256 as 64 lower case hex digits in out (65 bytes). */
static void sha256_final(Sha256 *s, char *out)
{
    uint64_t bits = s->len * 8;
    unsigned char pad = 0x80;
    sha256_update(s, &pad, 1);
    unsigned char zero = 0;
    while (s->used != 56) sha256_update(s, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - i*8));
    sha256_update(s, len, 8);
    for (int i = 0; i < 8; i++) snprintf(out + i*8, 9, "%08x", s->h[i]);
}

/** The sha256 of the file in out (65 bytes). */
static bool sha256_file(const char *file, char *out)
{
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    Sha256 s;
    sha256_init(&s);
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha256_update(&s, buf, n);
    fclose(f);
    sha256_final(&s, out);
    return true;
}

/** 2026-10-20T10:08:55Z as seconds since 1970, or 0. */
static time_t parse_utc_time(const char *s)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return 0;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef PLATFORM_WINAPI
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}

static void format_utc_time(time_t t, char *out, size_t size)
{
    struct tm tm;
#ifdef PLATFORM_WINAPI
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    strftime(out, size, "%Y-%m-%d %H:%M:%S", &tm);
}

static const char *index_attr(XMQNode *e, const char *name)
{
    XMQAttr *a = xml_node_get_attr(e, name, NULL);
    return a ? xml_attr_simple_value(a) : NULL;
}

/** Download the library file path into the download dir, keep it only if its sha256 is sha. */
static bool refresh_library_file(const char *path, const char *sha)
{
    char dest[1024], tmp[1100], url[1100];
    snprintf(dest, sizeof(dest), "%s/library/%s", download_dir(), path);
    snprintf(tmp, sizeof(tmp), "%s.download", dest);
    snprintf(url, sizeof(url), "%s%s", library_url(), path);
    char cmd[4096];
    if (!curl_fetch(url, tmp, true, cmd, sizeof(cmd)))
    {
        fprintf(stderr, "xmq: could not fetch %s\n", url);
        remove(tmp);
        return false;
    }
    char got[65];
    if (!sha256_file(tmp, got) || strcmp(got, sha))
    {
        fprintf(stderr, "xmq: the sha256 of %s is not the one in the library index, not used\n", url);
        remove(tmp);
        return false;
    }
    if (!replace_file(tmp, dest))
    {
        fprintf(stderr, "xmq: could not write %s\n", dest);
        remove(tmp);
        return false;
    }
    return true;
}

/** xmq check-library and xmq refresh-library. The library index, library_url()/index.xmq, lists
    every file of the library with its sha256 and modification time (made by
    scripts/build_library_index.sh). Each is compared with the file xmq uses (find_library_file):
    a file that is missing, or that differs and is older than the one on libxmq.org, is listed
    and with refresh downloaded into the download dir, where it is used before the installed
    library. A file that differs but is newer locally is listed and not touched. */
/** Fetch library_url()/index.xmq into the download dir. A failed fetch keeps the previous one. */
static bool fetch_library_index(char *index_file, size_t size)
{
    char url[1024], tmp[1100];
    snprintf(index_file, size, "%s/library/index.xmq", download_dir());
    snprintf(tmp, sizeof(tmp), "%s.download", index_file);
    snprintf(url, sizeof(url), "%sindex.xmq", library_url());
    char cmd[3000];
    if (!curl_fetch(url, tmp, false, cmd, sizeof(cmd)) || !replace_file(tmp, index_file))
    {
        remove(tmp);
        return false;
    }
    return true;
}

static bool cmd_library(bool refresh)
{
    char index_file[1024], url[1024];
    snprintf(url, sizeof(url), "%sindex.xmq", library_url());
    if (!fetch_library_index(index_file, sizeof(index_file)))
    {
        fprintf(stderr, "xmq: could not fetch the library index %s\n", url);
        return false;
    }
    XMQReturnDoc rd = xmqNewDoc();
    XMQDoc *doc = rd.doc;
    if (!xmqParseFile(doc, index_file, NULL, 0))
    {
        fprintf(stderr, "xmq: could not read the library index %s\n", url);
        xmqFreeDoc(doc);
        return false;
    }

    int num_refresh = 0, num_newer = 0, num_same = 0, num_failed = 0;
    XMQNode *root = xml_dom_root_element(xmq_get_dom(doc));
    for (XMQNode *e = root ? xml_node_first_child(root) : NULL; e; e = xml_node_next(e))
    {
        if (xml_node_kind(e) != XML_DOM_ELEMENT || strcmp(xml_node_name(e), "file")) continue;
        const char *path = index_attr(e, "path");
        const char *sha = index_attr(e, "sha256");
        const char *mtime = index_attr(e, "mtime");
        // The path is used in file names and a curl command line.
        if (!path || !sha || !mtime || strlen(sha) != 64 || path[0] == '/' || strstr(path, "..") ||
            strchr(path, '\'') || strchr(path, '\\'))
        {
            fprintf(stderr, "xmq: skipping a bad entry in the library index\n");
            continue;
        }
        time_t server_time = parse_utc_time(mtime);
        char server_str[32];
        format_utc_time(server_time, server_str, sizeof(server_str));

        char local[1024];
        char local_sha[65];
        if (find_library_file(path, "", local, sizeof(local)))
        {
            if (sha256_file(local, local_sha) && !strcmp(local_sha, sha))
            {
                num_same++;
                continue;
            }
            struct stat st;
            time_t local_time = stat(local, &st) == 0 ? st.st_mtime : 0;
            char local_str[32];
            format_utc_time(local_time, local_str, sizeof(local_str));
            if (local_time >= server_time)
            {
                // Changed locally, eg by hand, not replaced with the older one.
                printf("newer   %s  (%s %s is newer than libxmq.org %s, not refreshed)\n", path, local, local_str, server_str);
                num_newer++;
                continue;
            }
            printf("updated %s  (libxmq.org %s, %s %s)\n", path, server_str, local, local_str);
        }
        else
        {
            printf("new     %s  (libxmq.org %s)\n", path, server_str);
        }
        num_refresh++;
        if (refresh)
        {
            fflush(stdout); // Before any error from curl or the sha256 check on stderr.
            if (refresh_library_file(path, sha)) printf("        refreshed %s/library/%s\n", download_dir(), path);
            else num_failed++;
        }
    }
    xmqFreeDoc(doc);

    if (num_refresh == 0 && num_newer == 0)
    {
        printf("The library is up to date (%d %s).\n", num_same, num_same == 1 ? "file" : "files");
    }
    else if (refresh)
    {
        int n = num_refresh - num_failed;
        printf("%d %s refreshed, %d newer locally, %d up to date.\n", n, n == 1 ? "file" : "files", num_newer, num_same);
    }
    else
    {
        printf("%d %s to refresh (xmq refresh-library), %d newer locally, %d up to date.\n",
               num_refresh, num_refresh == 1 ? "file" : "files", num_newer, num_same);
    }
    return num_failed == 0;
}

/** The library index for list-library and describe-library: fetched from library_url(), else
    the last fetched or the installed one. Prints when it was created. NULL if there is none. */
static XMQDoc *load_library_index()
{
    char index_file[1024], url[1024];
    snprintf(url, sizeof(url), "%sindex.xmq", library_url());
    const char *from = url;
    if (!fetch_library_index(index_file, sizeof(index_file)))
    {
        if (!find_library_file("index.xmq", "", index_file, sizeof(index_file)))
        {
            fprintf(stderr, "xmq: could not fetch the library index %s and there is no local one\n", url);
            return NULL;
        }
        fprintf(stderr, "xmq: could not fetch the library index %s, using %s\n", url, index_file);
        from = index_file;
    }
    XMQDoc *doc = xmqNewDoc().doc;
    XMQNode *root = NULL;
    if (xmqParseFile(doc, index_file, NULL, 0)) root = xml_dom_root_element(xmq_get_dom(doc));
    if (!root || strcmp(xml_node_name(root), "library"))
    {
        fprintf(stderr, "xmq: could not read the library index %s\n", index_file);
        xmqFreeDoc(doc);
        return NULL;
    }
    const char *created = index_attr(root, "created");
    char created_str[32] = "unknown";
    if (created && parse_utc_time(created)) format_utc_time(parse_utc_time(created), created_str, sizeof(created_str));
    printf("Library index %s created %s UTC\n", from, created_str);
    return doc;
}

/** The library name of an index path: ixml:data/csv for data/csv.ixml, xsl:data/x for
    data/x.xslq, else false. */
static bool library_name_of(const char *path, char *out, size_t size)
{
    size_t n = strlen(path);
    if (n > 5 && !strcmp(path+n-5, ".ixml")) snprintf(out, size, "ixml:%.*s", (int)(n-5), path);
    else if (n > 5 && !strcmp(path+n-5, ".xslq")) snprintf(out, size, "xsl:%.*s", (int)(n-5), path);
    else return false;
    return true;
}

/** For ixml:data/csv_v2c the length of ixml:data/csv, the major 2 and the letter c (0 for
    ixml:data/csv_v2). False if the name is not a versioned copy. */
static bool split_versioned_name(const char *name, size_t *base_len, int *major, char *letter)
{
    const char *v = strrchr(name, '_');
    if (!v || v[1] != 'v' || !isdigit((unsigned char)v[2])) return false;
    const char *p = v+2;
    int m = 0;
    while (isdigit((unsigned char)*p)) m = m*10 + (*p++ - '0');
    char l = 0;
    if (*p >= 'a' && *p <= 'z') l = *p++;
    if (*p) return false;
    *base_len = v - name;
    *major = m;
    *letter = l;
    return true;
}

typedef struct { int major; char letter; } LibraryVersion;

static int compare_library_versions(const void *a, const void *b)
{
    const LibraryVersion *x = a, *y = b;
    if (x->major != y->major) return y->major - x->major; // Latest major first.
    return x->letter - y->letter;
}

/** The versions of the library name base, eg "v2abcd v1abc", from the copies in the index. */
static void library_versions(XMQNode *root, const char *base, char *out, size_t size)
{
    LibraryVersion versions[256];
    int num = 0;
    for (XMQNode *e = xml_node_first_child(root); e && num < 256; e = xml_node_next(e))
    {
        if (xml_node_kind(e) != XML_DOM_ELEMENT || strcmp(xml_node_name(e), "file")) continue;
        const char *path = index_attr(e, "path");
        char name[1024];
        size_t base_len;
        int major;
        char letter;
        if (!path || !library_name_of(path, name, sizeof(name))) continue;
        if (!split_versioned_name(name, &base_len, &major, &letter) || !letter) continue;
        if (strlen(base) != base_len || strncmp(name, base, base_len)) continue;
        versions[num].major = major;
        versions[num].letter = letter;
        num++;
    }
    qsort(versions, num, sizeof(LibraryVersion), compare_library_versions);
    size_t len = 0;
    out[0] = 0;
    for (int i = 0; i < num && len + 16 < size; i++)
    {
        if (i == 0 || versions[i].major != versions[i-1].major)
        {
            len += snprintf(out+len, size-len, "%sv%d", i ? " " : "", versions[i].major);
        }
        out[len++] = versions[i].letter;
        out[len] = 0;
    }
}

static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}

/** xmq list-library: every grammar and transform with its versions, sorted by name:
    ixml:data/csv v2abcd v1abc */
static bool cmd_list_library()
{
    XMQDoc *doc = load_library_index();
    if (!doc) return false;
    XMQNode *root = xml_dom_root_element(xmq_get_dom(doc));

    size_t num = 0, capacity = 64;
    char **names = (char**)malloc(capacity * sizeof(char*));
    int width = 0;
    for (XMQNode *e = xml_node_first_child(root); e; e = xml_node_next(e))
    {
        if (xml_node_kind(e) != XML_DOM_ELEMENT || strcmp(xml_node_name(e), "file")) continue;
        const char *path = index_attr(e, "path");
        char name[1024];
        size_t base_len;
        int major;
        char letter;
        if (!path || !library_name_of(path, name, sizeof(name))) continue;
        // The copies data/csv_v2c.ixml and data/csv_v2.ixml are listed as versions of data/csv.ixml.
        if (split_versioned_name(name, &base_len, &major, &letter)) continue;
        if (num == capacity)
        {
            capacity *= 2;
            names = (char**)realloc(names, capacity * sizeof(char*));
        }
        names[num++] = strdup(name);
        if ((int)strlen(name) > width) width = strlen(name);
    }
    if (num > 0) qsort(names, num, sizeof(char*), compare_strings);
    for (size_t i = 0; i < num; i++)
    {
        char versions[1024];
        library_versions(root, names[i], versions, sizeof(versions));
        printf("%-*s  %s\n", width, names[i], versions[0] ? versions : "(no version)");
        free(names[i]);
    }
    free(names);
    xmqFreeDoc(doc);
    return true;
}

/** xmq describe-library ixml:data/csv: the description (the first comment) and the versions. */
static bool cmd_describe_library(const char *name)
{
    if (!name)
    {
        fprintf(stderr, "Usage: xmq describe-library <ixml:name|xsl:name>\n");
        return false;
    }
    XMQDoc *doc = load_library_index();
    if (!doc) return false;
    XMQNode *root = xml_dom_root_element(xmq_get_dom(doc));

    XMQNode *found = NULL;
    char found_name[1024];
    for (XMQNode *e = xml_node_first_child(root); e && !found; e = xml_node_next(e))
    {
        if (xml_node_kind(e) != XML_DOM_ELEMENT || strcmp(xml_node_name(e), "file")) continue;
        const char *path = index_attr(e, "path");
        if (!path || !library_name_of(path, found_name, sizeof(found_name))) continue;
        // Without ixml: or xsl: either.
        const char *short_name = strchr(found_name, ':') + 1;
        if (!strcmp(found_name, name) || !strcmp(short_name, name)) found = e;
    }
    if (!found)
    {
        fprintf(stderr, "xmq: %s is not in the library, see xmq list-library\n", name);
        xmqFreeDoc(doc);
        return false;
    }
    const char *version = index_attr(found, "version");
    const char *description = index_attr(found, "description");
    printf("\n%s%s%s\n", found_name, version ? " v" : "", version ? version : "");
    if (description) printf("\n%s\n", description);

    char base[1024], versions[1024];
    size_t base_len;
    int major;
    char letter;
    snprintf(base, sizeof(base), "%s", found_name);
    if (split_versioned_name(found_name, &base_len, &major, &letter)) base[base_len] = 0;
    library_versions(root, base, versions, sizeof(versions));
    if (versions[0]) printf("\nVersions of %s: %s\n", base, versions);
    xmqFreeDoc(doc);
    return true;
}

bool load_xslt(XMQCliCommand *command, const char *arg)
{
    XMQReturnDoc rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *doq = rd.doc;

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
        fprintf(stderr, "%s\n", error ? error : "xmq: parse error");
        xmqFreeDoc(doq);
        return false;
    }

    verbose_("xmq=", "loaded xslt %s", arg);

    XmlDom *xslt_dom = xmq_get_dom(doq);
    if (xmqGetOriginalContentType(doq) == XMQ_CONTENT_XMQ ||
        xmqGetOriginalContentType(doq) == XMQ_CONTENT_HTMQ)
    {
        // We want to be able to use char entities in the xslq by default.
        // So we replace any explicit &#10; with actual newlines etc.
        XMQNode *root = xml_node_first_child(xml_dom_document(xslt_dom));
        // Replace all char entities! I.e. &#10; is replaced with a newline.
        if (root) substitute_entity(xslt_dom, root, NULL, true);
    }
    command->xslt_doq = doq;
    // The messages are printed on stderr, like libxslt does.
    XMQReturnXslt rx = xmqCompileXslt(doq, NULL);
    if (rx.status == XMQ_ERROR_EXCLUDED_FROM_BUILD)
    {
        fprintf(stderr, "%s", xmqDocError(doq));
        exit(1);
    }
    command->xslt = rx.xslt;
    return true;
}

void add_xsl_command(XMQCliCommand *command, const char *local_file)
{
    verbose_("xmq=", "reading xsl file %s", local_file);
    command->cmd = XMQ_CLI_CMD_TRANSFORM;
    bool ok = load_xslt(command, local_file);
    if (!ok || command->xslt == NULL)
    {
        printf("xmq: could not load %s\n", local_file);
        exit(1);
    }
}

/** The local file of a library stylesheet, eg xsl:data/table-to-html: from the installed
    library, else downloaded into the download dir. */
static void xsl_library_file(XMQCliCommand *command, const char *file, char *local_file, size_t size)
{
    if (dot_found(file))
    {
        printf("xmq: file after xsl: must not contain dots (%s)\n", file);
        exit(1);
    }
    // The installed library is used, unless the newest version is asked for.
    LibraryMode mode = library_mode(command);
    if (mode != LIBRARY_IF_MISSING || !find_library_file(file, ".xslq", local_file, size))
    {
        snprintf(local_file, size, "%s/library/%s.xslq", download_dir(), file);
        download(mode, ".xslq", file, local_file, false);
    }
}

void prepare_xsl_command(XMQCliCommand *command, const char *arg)
{
    // An xsl:data/table-to-html argument is interpreted as
    // transform ~/.local/share/xmq/transforms/data/table-to-html.xslq
    const char *file = is_xsl_cmd(arg);
    assert(file);
    char local_file[1024];
    xsl_library_file(command, file, local_file, sizeof(local_file));

    verbose_("xmq=", "reading xslq file %s", local_file);
    command->cmd = XMQ_CLI_CMD_TRANSFORM;
    bool ok = load_xslt(command, local_file);
    if (!ok || command->xslt == NULL)
    {
        printf("xmq: could not load %s\n", local_file);
        exit(1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// The command line parsed with the grammar library/xmq-utils/xmqcli.ixml
//
// argv is joined into a single line (xmq-cli-join.h) and parsed by the generated
// parser (xmq-cli-parser.h, generated with xmqj generate-parser --c). The grammar
// decides what every argument is: a global option, a grammar, the input file or
// content, a command, a command option or a command argument. The tree is then
// walked and each option and argument is given to the same handlers as before
// (handle_global_option, handle_option) as --name or --name=value or the argument.

// The tree is walked with the xmq.h api only. Strings given to the handlers
// are copies, since the handlers keep them for the whole run. The copies are
// kept in cli_kept_ and freed at the end of main by cli_free_kept.

static char **cli_kept_ = NULL;
static size_t cli_num_kept_ = 0;
static size_t cli_kept_cap_ = 0;

static const char *cli_keep(char *s)
{
    if (cli_num_kept_ == cli_kept_cap_)
    {
        cli_kept_cap_ = cli_kept_cap_ ? cli_kept_cap_ * 2 : 32;
        cli_kept_ = (char**)realloc(cli_kept_, cli_kept_cap_ * sizeof(char*));
    }
    cli_kept_[cli_num_kept_++] = s;
    return s;
}

static void cli_free_kept(void)
{
    for (size_t i = 0; i < cli_num_kept_; ++i) free(cli_kept_[i]);
    free(cli_kept_);
    cli_kept_ = NULL;
    cli_num_kept_ = cli_kept_cap_ = 0;
}

// The text of an element, "" if empty.
static const char *cli_text(XMQDoc *doq, XMQNode *n)
{
    const char *t = xmqGetStringRel(doq, ".", n);
    return cli_keep(strdup(t ? t : ""));
}

// The value of an attribute, "" if empty and NULL if not present.
static const char *cli_attr(XMQDoc *doq, XMQNode *n, const char *name)
{
    char xpath[64];
    snprintf(xpath, sizeof(xpath), "@%s", name);
    if (xmqForeachRel(doq, xpath, NULL, NULL, n) == 0) return NULL;
    const char *v = xmqGetStringRel(doq, xpath, n);
    return cli_keep(strdup(v ? v : ""));
}

typedef struct CliNodes { XMQNode **nodes; size_t n, cap; } CliNodes;

static XMQProceed cli_collect(XMQDoc *doq, XMQNode *node, void *user_data)
{
    CliNodes *l = (CliNodes*)user_data;
    if (l->n == l->cap)
    {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->nodes = (XMQNode**)realloc(l->nodes, l->cap * sizeof(XMQNode*));
    }
    l->nodes[l->n++] = node;
    return XMQ_CONTINUE;
}

// The child elements of n in document order.
static CliNodes cli_children(XMQDoc *doq, XMQNode *n)
{
    CliNodes l = { NULL, 0, 0 };
    xmqForeachRel(doq, "*", cli_collect, &l, n);
    return l;
}

// Rebuild --name or --name=value from an option(name value) element.
static const char *cli_option_string(XMQDoc *doq, XMQNode *opt)
{
    const char *name = cli_attr(doq, opt, "name");
    const char *value = cli_attr(doq, opt, "value");
    if (!name) name = "";
    size_t len = strlen(name) + (value ? strlen(value) : 0) + 4;
    char *buf = (char*)malloc(len);
    if (value) snprintf(buf, len, "--%s=%s", name, value);
    else snprintf(buf, len, "--%s", name);
    return cli_keep(buf);
}

static void cli_print_parse_error(const char *line, xmqcli_Error *err)
{
    fprintf(stderr, "xmq: bad command line, %s\n", err->message);
    // Show the joined line and point at the error, unless it contains newlines.
    if (!strchr(line, '\n') && strlen(line) < 200)
    {
        fprintf(stderr, "%s\n", line);
        for (size_t i = 1; i < err->col; i++) fputc(' ', stderr);
        fprintf(stderr, "^\n");
    }
    fprintf(stderr, "Try 'xmq --help' for more information\n");
}

static bool cli_load_ixml_library(XMQCliCommand *load_command, const char *arg, bool *add_xslq, char *add_xslq_file)
{
    // An ixml:data/csv argument is interpreted as --ixml=~/.local/share/xmq/library/data/csv.ixml
    const char *file = is_ixml_cmd(arg);
    assert(file);
    if (dot_found(file))
    {
        printf("xmq: name after ixml: must not contain dots (%s)\n", arg);
        return false;
    }

    char local_ixml_file[256];
    LibraryMode mode = library_mode(load_command);
    if (mode == LIBRARY_IF_MISSING && find_library_file(file, ".ixml", local_ixml_file, sizeof(local_ixml_file)))
    {
        // From the library, with its companion xslq if there is one.
        char installed_xslq[256];
        if (find_library_file(file, ".xslq", installed_xslq, sizeof(installed_xslq)))
        {
            strcpy(add_xslq_file, installed_xslq);
            *add_xslq = true;
        }
        verbose_("xmq=", "reading library ixml file %s", local_ixml_file);
        load_command->ixml_filename = strdup(local_ixml_file);
        load_command->ixml_source = load_file_into_buffer(local_ixml_file);
        if (load_command->ixml_source == NULL) exit(1);
        return true;
    }
    snprintf(local_ixml_file, 256, "%s/library/%s.ixml", download_dir(), file);
    DownloadStatus status = download(mode, ".ixml", file, local_ixml_file, false);

    // The download was DOWNLOAD_OK or DOWNLOAD_EXISTS. Otherwise the download command would have given up.
    // See if there is a companion xslq file with this ixml file.
    char local_xslq_file[256];
    snprintf(local_xslq_file, 256, "%s/library/%s.xslq", download_dir(), file);

    if (status == DOWNLOAD_EXISTS)
    {
        FILE *f = fopen(local_xslq_file, "rb");
        if (f)
        {
            // The ixml already existed, if the xslq exists use it.
            fclose(f);
            strcpy(add_xslq_file, local_xslq_file);
            *add_xslq = true;
        }
        // If the xslq did not exist, do not try to dowload it.
        // The assumption is that you either cached both ixml+xslq or just ixml.
    }
    else
    {
        // The ixml was downloaded, lets try to download an xslq.
        status = download(mode, ".xslq", file, local_xslq_file, true);
        if (status == DOWNLOAD_OK || status == DOWNLOAD_EXISTS)
        {
            strcpy(add_xslq_file, local_xslq_file);
            *add_xslq = true;
        }
    }

    verbose_("xmq=", "reading ixml file %s", local_ixml_file);
    load_command->ixml_filename = strdup(local_ixml_file);
    load_command->ixml_source = load_file_into_buffer(local_ixml_file);
    if (load_command->ixml_source == NULL) exit(1);
    return true;
}

static void cli_add_default_output(XMQCliCommand *load_command, XMQCliCommand *command,
                                   XMQCliCommand *to, XMQCliCommand *output, bool do_print)
{
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

}

bool xmq_parse_cmd_line(int argc, const char **argv, XMQCliCommand *load_command)
{
    bool add_xslq = false;
    char add_xslq_file[256];
    // Remember if we have found a to/render/tokenize command.
    XMQCliCommand *to = NULL;
    // Remember if we have found a print/save/pager command.
    XMQCliCommand *output = NULL;

    char *line = xmq_cli_join_argv(argc, argv);
    verbose_("xmq=", "command line: %s", line);

    XMQReturnDoc rd = xmqNewDoc();
    XMQDoc *doq = rd.doc;
    xmqcli_Error err;
    if (!xmqcli_parse(doq, line, line + strlen(line), &err))
    {
        cli_print_parse_error(line, &err);
        free(line);
        xmqFreeDoc(doq);
        return false;
    }
    free(line);

    CliNodes top = cli_children(doq, xmqGetRootNode(doq));
    CliNodes parts = { NULL, 0, 0 };
    size_t k = 0;

    // Global options, the grammar and the source come before the first command.
    for (; k < top.n; k++)
    {
        XMQNode *n = top.nodes[k];
        const char *what = xmqGetName(n);
        if (!strcmp(what, "command")) break;
        if (!strcmp(what, "option"))
        {
            const char *arg = cli_option_string(doq, n);
            if (handle_global_option(arg, load_command)) continue;
            printf("xmq: unrecognized global option: '%s'\nTry 'xmq --help' for more information\n", arg);
            goto fail;
        }
        if (!strcmp(what, "grammar"))
        {
            const char *g = cli_text(doq, n);
            if (load_command->ixml_source)
            {
                printf("You have already specified an ixml grammar!\n");
                goto fail;
            }
            if (is_ixml_cmd(g))
            {
                if (!cli_load_ixml_library(load_command, g, &add_xslq, add_xslq_file)) goto fail;
            }
            else
            {
                verbose_("xmq=", "reading ixml file %s", g);
                load_command->ixml_filename = strdup(g);
                load_command->ixml_source = load_file_into_buffer(g);
                if (load_command->ixml_source == NULL) exit(1);
            }
            continue;
        }
        if (!strcmp(what, "input"))
        {
            load_command->in_is_content = true;
            load_command->in = cli_text(doq, n);
            continue;
        }
        if (!strcmp(what, "file"))
        {
            load_command->in = cli_text(doq, n);
            continue;
        }
    }

    XMQCliCommand *command = load_command;
    bool do_print = true;

    if (add_xslq)
    {
        command = allocate_cli_command(command->env);
        load_command->next = command;
        add_xsl_command(command, add_xslq_file);
        load_command = command;
    }

    for (; k < top.n; k++)
    {
        XMQNode *n = top.nodes[k];
        const char *cname = cli_attr(doq, n, "name");
        parts = cli_children(doq, n);
        XMQCliCommand *prev = command;
        command = allocate_cli_command(command->env);
        prev->next = command;
        command->cmd = cmd_from(cname);

        if (cname && !strcmp(cname, "xsl"))
        {
            // xsl:name, the name is the arg.
            const char *x = "";
            for (size_t j = 0; j < parts.n; j++)
            {
                if (!strcmp(xmqGetName(parts.nodes[j]), "arg")) { x = cli_text(doq, parts.nodes[j]); break; }
            }
            size_t len = strlen(x) + 5;
            char *buf = (char*)malloc(len);
            snprintf(buf, len, "xsl:%s", x);
            prepare_xsl_command(command, buf);
            free(buf);
        }
        if (command->cmd == XMQ_CLI_CMD_NONE)
        {
            fprintf(stderr, "xmq: no such command \"%s\"\n", cname ? cname : "");
            exit(1);
        }
        prepare_command(command, load_command);
        verbose_("xmq=", "found command %s", cmd_name(command->cmd));

        if (command->cmd == XMQ_CLI_CMD_TO_JSON)
        {
            // By default print json in compact form. Can be changed with -p or --pretty.
            command->add_indent = 0;
            command->compact = true;
        }

        // The command options and arguments, in order.
        for (size_t j = 0; j < parts.n; j++)
        {
            XMQNode *c = parts.nodes[j];
            bool is_option = !strcmp(xmqGetName(c), "option");
            if (!is_option && cname && !strcmp(cname, "xsl")) continue; // Already used.
            const char *arg = is_option ? cli_option_string(doq, c) : cli_text(doq, c);
            if (!handle_option(arg, is_option, command))
            {
                if (is_option)
                    printf("xmq: option \"%s\" not available for command \"%s\"\n", arg, cmd_name(command->cmd));
                else
                    printf("xmq: bad argument \"%s\" for command \"%s\"\n", arg, cmd_name(command->cmd));
                exit(1);
            }
            verbose_("xmq=", "found argument %s", arg);
        }

        // Check if we should remember this command as a to/render/tokenize command?
        if (cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_TO ||
            cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_RENDER ||
            cmd_group(command->cmd) == XMQ_CLI_CMD_GROUP_TOKENIZE)
        {
            if (to)
            {
                fprintf(stderr, "xmq: you can only use one to/render/tokenize command.\n");
                goto fail;
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
                goto fail;
            }
            output = command;
        }
        free(parts.nodes);
        parts.nodes = NULL;
    }
    free(top.nodes);
    xmqFreeDoc(doq);

    cli_add_default_output(load_command, command, to, output, do_print);
    return true;

fail:
    free(parts.nodes);
    free(top.nodes);
    xmqFreeDoc(doq);
    return false;
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


/** Free the command chain and the strings kept for the command line handlers. */
static void free_all_cli_commands(XMQCliCommand *load_command)
{
    XMQCliCommand *c = load_command;
    while (c)
    {
        XMQCliCommand *tmp = c;
        c = c->next;
        free_cli_command(tmp);
    }
    cli_free_kept();
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

    // Check if can find the best background setting for this terminal. mono/dark/light
    lookup_bg(&env.use_color, &env.bg_dark_mode, &env.bg_forced);

    // Check if terminal supports truecolor.
    env.truecolor = detect_truecolor();

    // See if we can find the theme spec to use.
    default_theme_spec_ = lookup_theme_spec();

    // But override coloring if not tty.
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

    // The command line is parsed with the grammar library/xmq-utils/xmqcli.ixml
    bool ok = xmq_parse_cmd_line(argc, argv, load_command);
    if (!ok) {
        verbose_("xmq=", "parse cmd line failed");
        free_all_cli_commands(load_command);
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
        int help_rc = cmd_help(load_command->next);
        free_all_cli_commands(load_command);
        return help_rc;
    }
    if (load_command->next && (load_command->next->cmd == XMQ_CLI_CMD_CHECK_LIBRARY ||
                               load_command->next->cmd == XMQ_CLI_CMD_REFRESH_LIBRARY))
    {
        // These need no input.
        int rc = cmd_library(load_command->next->cmd == XMQ_CLI_CMD_REFRESH_LIBRARY) ? 0 : 1;
        free_all_cli_commands(load_command);
        return rc;
    }
    if (load_command->next && (load_command->next->cmd == XMQ_CLI_CMD_LIST_LIBRARY ||
                               load_command->next->cmd == XMQ_CLI_CMD_DESCRIBE_LIBRARY))
    {
        int rc = (load_command->next->cmd == XMQ_CLI_CMD_LIST_LIBRARY ? cmd_list_library() :
                  cmd_describe_library(load_command->next->library_name)) ? 0 : 1;
        free_all_cli_commands(load_command);
        return rc;
    }

    bool more_content = true;
    bool no_more_data = false;

    while (more_content)
    {
        // The load command will either load the whole file at once
        // or chip away at it for each line.
        XMQCliCommand *c = load_command;

        // Execute commands.
        while (c)
        {
            debug_("xmq=", "performing %s", cmd_name(c->cmd));
            bool ok = perform_command(c, &no_more_data);
            if (!ok)
            {
                // cmd_load returns false and sets no_more_data when we are out of lines.
                // This is not an error. Only set rc to 1 if a real failure.
                if (!no_more_data) rc = 1;
                break;
            }
            c = c->next;
        }
        // Free document.
        more_content = cmd_unload(load_command);
    }


    free_all_cli_commands(load_command);

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
